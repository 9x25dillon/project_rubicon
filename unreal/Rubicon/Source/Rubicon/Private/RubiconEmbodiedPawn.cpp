#include "RubiconEmbodiedPawn.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "IXRTrackingSystem.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "RubiconDebugPanelComponent.h"
#include "RubiconGazeResolverComponent.h"
#include "RubiconGolgothaGreybox.h"
#include "RubiconNarrativeSubsystem.h"
#include "RubiconSettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogRubiconPawn, Log, All);

namespace RubiconPawnDetail
{
	ARubiconGolgothaGreybox* FindGreybox(UWorld* World)
	{
		for (TActorIterator<ARubiconGolgothaGreybox> It(World); It; ++It) return *It;
		return nullptr;
	}

	/** Live setting from the core (it can change mid-session), falling back to the project default. */
	bool IsSeated(const UWorld* World)
	{
		const URubiconNarrativeSubsystem* N = World ? World->GetSubsystem<URubiconNarrativeSubsystem>() : nullptr;
		const RubiconCore::FSessionState* S = N ? N->GetCoreState() : nullptr;
		return S ? S->Settings.bSeated : URubiconSettings::Get()->DefaultPlayerSettings.bSeated;
	}

	UInputAction* MakeAction(UObject* Outer, const TCHAR* Name, EInputActionValueType Type = EInputActionValueType::Boolean)
	{
		UInputAction* A = NewObject<UInputAction>(Outer, Name);
		A->ValueType = Type;
		return A;
	}
}

ARubiconEmbodiedPawn::ARubiconEmbodiedPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	TrackingOrigin = CreateDefaultSubobject<USceneComponent>(TEXT("TrackingOrigin"));
	TrackingOrigin->SetupAttachment(Root);
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(TrackingOrigin);
	Camera->bLockToHmd = true;
	Camera->bUsePawnControlRotation = false;

	Gaze = CreateDefaultSubobject<URubiconGazeResolverComponent>(TEXT("Gaze"));

	// Body-locked (not head-locked) panel low and to the left, angled toward the eye.
	DebugPanel = CreateDefaultSubobject<URubiconDebugPanelComponent>(TEXT("DebugPanel"));
	DebugPanel->SetupAttachment(Root);
	DebugPanel->SetRelativeLocation(FVector(85, -55, -40));
	DebugPanel->SetRelativeRotation(FRotator(25, 150, 0));
}

FVector ARubiconEmbodiedPawn::ClampCorrection(const FVector& HeadLocal, const FVector& CenterLocal, double Radius)
{
	const FVector Delta = HeadLocal - CenterLocal;
	return Delta - Delta.GetClampedToMaxSize(FMath::Max(0.0, Radius));
}

void ARubiconEmbodiedPawn::BeginPlay()
{
	Super::BeginPlay();
	bHmd = GEngine && GEngine->XRSystem.IsValid() && GEngine->XRSystem->IsHeadTrackingAllowed();
	if (!bHmd)
	{
		// Desktop debugging only: mouse look through the controller. In a headset only the HMD orients the camera.
		Camera->bUsePawnControlRotation = true;
	}
	PlaceForView(ERubiconViewMode::Embodiment, ERubiconWitnessVantage::BelovedDisciple);
	AddMappingContext();
}

void ARubiconEmbodiedPawn::EndPlay(const EEndPlayReason::Type Reason)
{
	Super::EndPlay(Reason);
}

FVector2D ARubiconEmbodiedPawn::HeadSpeed(const FQuat& PrevRot, const FVector& PrevLoc, const FQuat& Rot, const FVector& Loc, double DeltaSeconds)
{
	if (DeltaSeconds <= 0.0) return FVector2D::ZeroVector;
	const double Deg = FMath::RadiansToDegrees(PrevRot.AngularDistance(Rot));
	return FVector2D(Deg / DeltaSeconds, FVector::Dist(PrevLoc, Loc) / DeltaSeconds);
}

FVector ARubiconEmbodiedPawn::AnchorWorld(ERubiconViewMode View, ERubiconWitnessVantage Vantage, FRotator& OutYaw) const
{
	const ARubiconGolgothaGreybox* G = RubiconPawnDetail::FindGreybox(GetWorld());
	if (!G)
	{
		OutYaw = GetActorRotation();
		return GetActorLocation();
	}
	if (View == ERubiconViewMode::Embodiment)
	{
		const FTransform T = G->GetEmbodimentEyeTransform();
		OutYaw = T.Rotator();
		return T.GetLocation();
	}
	const FTransform Floor = G->GetWitnessFloorTransform(Vantage);
	const bool bSeated = RubiconPawnDetail::IsSeated(GetWorld());
	OutYaw = Floor.Rotator();
	return Floor.GetLocation() + FVector(0, 0, bSeated ? WitnessEyeHeightSeated : WitnessEyeHeightStanding);
}

void ARubiconEmbodiedPawn::PlaceForView(ERubiconViewMode View, ERubiconWitnessVantage Vantage)
{
	FRotator Yaw;
	const FVector Anchor = AnchorWorld(View, Vantage, Yaw);
	SetActorLocation(Anchor);
	const bool bAlign = View != ERubiconViewMode::Embodiment && URubiconSettings::Get()->bAlignYawOnWitness;
	if (bAlign)
	{
		// Opt-in witness snap, applied only while the view is fully black. Initial yaw comes from the spawn transform.
		SetActorRotation(FRotator(0, Yaw.Yaw, 0));
	}
	PlacedView = View;
	PlacedVantage = Vantage;
	bNeedsCalibration = true; // the current head position becomes the new anchor
}

void ARubiconEmbodiedPawn::UpdateViewPlacement(const FRubiconFrameOutputs& F, float DeltaSeconds)
{
	APlayerCameraManager* Cam = nullptr;
	if (APlayerController* Pc = Cast<APlayerController>(GetController())) Cam = Pc->PlayerCameraManager;

	const bool bDiffers = F.View != PlacedView || (F.View != ERubiconViewMode::Embodiment && F.ViewVantage != PlacedVantage);
	if (MoveTimer < 0.f)
	{
		if (!bDiffers) return;
		// Losing or regaining consciousness fades slowly; Witness keeps its short fade.
		const bool bSwoon = F.View == ERubiconViewMode::Telling || PlacedView == ERubiconViewMode::Telling;
		MoveFade = bSwoon ? SwoonFadeSeconds : WitnessFadeSeconds;
		MoveTimer = 0.f;
		if (Cam) Cam->StartCameraFade(0.f, 1.f, MoveFade, FLinearColor::Black, false, true);
		return;
	}
	// Moves happen only while the view is fully black, to wherever the core says the camera belongs now.
	MoveTimer += DeltaSeconds;
	if (MoveTimer < MoveFade) return;
	PlaceForView(F.View, F.ViewVantage);
	MoveTimer = -1.f;
	if (Cam) Cam->StartCameraFade(1.f, 0.f, MoveFade, FLinearColor::Black, false, false);
}

void ARubiconEmbodiedPawn::UpdateHeadMotion(URubiconNarrativeSubsystem* N, float DeltaSeconds)
{
	if (!N) return;
	if (!bHmd)
	{
		// Desktop mouse look is not a body; it never builds exertion.
		N->ClearExertionInputs();
		return;
	}
	// Tracking-space pose: independent of the clamp offset, the vantage and the elevation tilt, so only the
	// player's own movement counts.
	const FQuat Rot = Camera->GetRelativeRotation().Quaternion();
	const FVector Loc = Camera->GetRelativeLocation();
	if (bHasPrevHead && DeltaSeconds > 0.f)
	{
		const FVector2D Speed = HeadSpeed(PrevHeadRot, PrevHeadLoc, Rot, Loc, DeltaSeconds);
		const double Alpha = 1.0 - FMath::Exp(-static_cast<double>(DeltaSeconds) / FMath::Max(0.01, static_cast<double>(HeadMotionTau)));
		HeadMotion += (Speed - HeadMotion) * Alpha;
	}
	PrevHeadRot = Rot;
	PrevHeadLoc = Loc;
	bHasPrevHead = true;
	N->SetHeadMotion(static_cast<float>(HeadMotion.X), static_cast<float>(HeadMotion.Y));
}

void ARubiconEmbodiedPawn::Recenter()
{
	bNeedsCalibration = true;
}

void ARubiconEmbodiedPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	URubiconNarrativeSubsystem* N = GetWorld()->GetSubsystem<URubiconNarrativeSubsystem>();
	const FRubiconFrameOutputs* F = N ? &N->GetFrameOutputsRef() : nullptr;

	if (F && F->bValid) UpdateViewPlacement(*F, DeltaSeconds);
	UpdateHeadMotion(N, DeltaSeconds);

	if (!bHmd) return;
	const FVector HeadLocal = Camera->GetRelativeLocation(); // HMD position in tracking space (bLockToHmd)
	if (bNeedsCalibration)
	{
		HeadCenter = HeadLocal;
		bNeedsCalibration = false;
	}
	const bool bEmbodied = PlacedView == ERubiconViewMode::Embodiment;
	const bool bSeated = RubiconPawnDetail::IsSeated(GetWorld());
	const double Radius = bEmbodied ? (bSeated ? SeatedHeadClampRadius : HeadClampRadius) : WitnessRadius;
	// Position only: the tracking origin's rotation is never touched.
	TrackingOrigin->SetRelativeLocation(-HeadCenter - ClampCorrection(HeadLocal, HeadCenter, Radius));
}

void ARubiconEmbodiedPawn::BuildInput()
{
	using namespace RubiconPawnDetail;
	if (Mapping) return;
	Mapping = NewObject<UInputMappingContext>(this, TEXT("RubiconMapping"));
	SelectAction = MakeAction(this, TEXT("Select"));
	WitnessAction = MakeAction(this, TEXT("Witness"));
	ComfortAction = MakeAction(this, TEXT("Comfort"));
	PauseAction = MakeAction(this, TEXT("Pause"));
	ExitAction = MakeAction(this, TEXT("Exit"));
	LayerAction = MakeAction(this, TEXT("Layer"));
	RecenterAction = MakeAction(this, TEXT("Recenter"));
	DebugAction = MakeAction(this, TEXT("Debug"));
	LookAction = MakeAction(this, TEXT("Look"), EInputActionValueType::Axis2D);

	auto Map = [this](UInputAction* A, std::initializer_list<FKey> Keys)
	{
		for (const FKey& K : Keys) Mapping->MapKey(A, K);
	};
	// OpenXR controller keys by name: unknown names on other runtimes simply never fire.
	Map(SelectAction, { FKey(TEXT("OculusTouch_Right_Trigger_Click")), EKeys::SpaceBar, EKeys::Enter, EKeys::Gamepad_FaceButton_Bottom });
	Map(WitnessAction, { FKey(TEXT("OculusTouch_Right_A_Click")), EKeys::W, EKeys::Gamepad_FaceButton_Top });
	Map(ComfortAction, { FKey(TEXT("OculusTouch_Right_B_Click")), EKeys::C, EKeys::Gamepad_FaceButton_Left });
	Map(PauseAction, { FKey(TEXT("OculusTouch_Left_X_Click")), EKeys::P, EKeys::Gamepad_Special_Right });
	Map(ExitAction, { FKey(TEXT("OculusTouch_Left_Menu_Click")), EKeys::Escape, EKeys::Gamepad_Special_Left });
	Map(LayerAction, { FKey(TEXT("OculusTouch_Left_Y_Click")), EKeys::L });
	Map(RecenterAction, { FKey(TEXT("OculusTouch_Left_Thumbstick_Click")), EKeys::R });
	Map(DebugAction, { FKey(TEXT("OculusTouch_Right_Thumbstick_Click")), EKeys::F1 });
	Map(LookAction, { EKeys::Mouse2D });
}

void ARubiconEmbodiedPawn::AddMappingContext()
{
	BuildInput();
	const APlayerController* Pc = Cast<APlayerController>(GetController());
	if (!Pc) return;
	if (UEnhancedInputLocalPlayerSubsystem* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Pc->GetLocalPlayer()))
	{
		if (!Sub->HasMappingContext(Mapping)) Sub->AddMappingContext(Mapping, 0);
	}
}

void ARubiconEmbodiedPawn::PawnClientRestart()
{
	Super::PawnClientRestart();
	AddMappingContext();
}

void ARubiconEmbodiedPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	BuildInput();
	UEnhancedInputComponent* Eic = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Eic)
	{
		UE_LOG(LogRubiconPawn, Error, TEXT("Enhanced Input component missing (Config/DefaultEngine.ini DefaultInputComponentClass)"));
		return;
	}
	Eic->BindAction(SelectAction, ETriggerEvent::Started, this, &ARubiconEmbodiedPawn::OnSelect);
	Eic->BindAction(WitnessAction, ETriggerEvent::Started, this, &ARubiconEmbodiedPawn::OnWitness);
	Eic->BindAction(ComfortAction, ETriggerEvent::Started, this, &ARubiconEmbodiedPawn::OnComfort);
	Eic->BindAction(PauseAction, ETriggerEvent::Started, this, &ARubiconEmbodiedPawn::OnPause);
	Eic->BindAction(ExitAction, ETriggerEvent::Started, this, &ARubiconEmbodiedPawn::OnExit);
	Eic->BindAction(LayerAction, ETriggerEvent::Started, this, &ARubiconEmbodiedPawn::OnLayer);
	Eic->BindAction(RecenterAction, ETriggerEvent::Started, this, &ARubiconEmbodiedPawn::OnRecenter);
	Eic->BindAction(DebugAction, ETriggerEvent::Started, this, &ARubiconEmbodiedPawn::OnDebug);
	Eic->BindAction(LookAction, ETriggerEvent::Triggered, this, &ARubiconEmbodiedPawn::OnLook);
}

void ARubiconEmbodiedPawn::OnSelect()
{
	URubiconNarrativeSubsystem* N = GetWorld()->GetSubsystem<URubiconNarrativeSubsystem>();
	if (!N) return;
	// One button: consent at the threshold, otherwise speak the saying when the core allows it.
	if (N->GetFrameOutputsRef().State == ERubiconTimelineState::Threshold) N->AcknowledgeAdvisory();
	else N->TriggerSaying(ERubiconTriggerVia::Select);
}

void ARubiconEmbodiedPawn::OnWitness() { if (URubiconNarrativeSubsystem* N = GetWorld()->GetSubsystem<URubiconNarrativeSubsystem>()) N->ToggleWitness(); }
void ARubiconEmbodiedPawn::OnComfort() { if (URubiconNarrativeSubsystem* N = GetWorld()->GetSubsystem<URubiconNarrativeSubsystem>()) N->ToggleComfort(); }
void ARubiconEmbodiedPawn::OnPause() { if (URubiconNarrativeSubsystem* N = GetWorld()->GetSubsystem<URubiconNarrativeSubsystem>()) N->TogglePause(); }
void ARubiconEmbodiedPawn::OnExit() { if (URubiconNarrativeSubsystem* N = GetWorld()->GetSubsystem<URubiconNarrativeSubsystem>()) N->Exit(); }
void ARubiconEmbodiedPawn::OnLayer() { if (URubiconNarrativeSubsystem* N = GetWorld()->GetSubsystem<URubiconNarrativeSubsystem>()) N->ToggleLayer(); }
void ARubiconEmbodiedPawn::OnRecenter() { Recenter(); }
void ARubiconEmbodiedPawn::OnDebug() { URubiconDebugPanelComponent::ToggleDebugVisible(); }

void ARubiconEmbodiedPawn::OnLook(const FInputActionValue& Value)
{
	if (bHmd) return;
	const FVector2D V = Value.Get<FVector2D>();
	AddControllerYawInput(V.X);
	AddControllerPitchInput(-V.Y);
}
