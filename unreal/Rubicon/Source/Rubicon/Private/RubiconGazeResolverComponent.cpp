#include "RubiconGazeResolverComponent.h"

#include "Camera/CameraComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "RubiconGazeTargetActor.h"
#include "RubiconNarrativeSubsystem.h"

FName FRubiconGazeHysteresis::Update(FName Raw, double DeltaMs, double HoldMs)
{
	if (Raw == Current)
	{
		Candidate = Current;
		CandidateMs = 0;
		return Current;
	}
	if (Raw != Candidate)
	{
		Candidate = Raw;
		CandidateMs = 0;
	}
	CandidateMs += DeltaMs;
	if (CandidateMs >= HoldMs)
	{
		Current = Candidate;
		CandidateMs = 0;
	}
	return Current;
}

URubiconGazeResolverComponent::URubiconGazeResolverComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork; // after the HMD pose has been applied to the camera
}

void URubiconGazeResolverComponent::BeginPlay()
{
	Super::BeginPlay();
	Camera = GetOwner() ? GetOwner()->FindComponentByClass<UCameraComponent>() : nullptr;
	RefreshTargets();
}

FName URubiconGazeResolverComponent::IdFromTag(const FName& Tag)
{
	static const FString Prefix = TEXT("Gaze.Target.");
	const FString S = Tag.ToString();
	return S.StartsWith(Prefix) ? FName(*S.RightChop(Prefix.Len())) : NAME_None;
}

FName URubiconGazeResolverComponent::ResolveByPitch(double PitchDeg, double BandDeg, FName InSky, FName InHorizon, FName InGround)
{
	if (PitchDeg > BandDeg) return InSky;
	if (PitchDeg < -BandDeg) return InGround;
	return InHorizon;
}

double URubiconGazeResolverComponent::ConeAngleToSphere(const FVector& Origin, const FVector& Direction, const FSphere& Sphere)
{
	const FVector To = Sphere.Center - Origin;
	const double Dist = To.Size();
	if (Dist <= Sphere.W) return -1.0; // inside the sphere
	const double Angle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Direction.GetSafeNormal(), To / Dist), -1.0, 1.0)));
	const double Radius = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Sphere.W / Dist, 0.0, 1.0)));
	return Angle - Radius;
}

void URubiconGazeResolverComponent::RefreshTargets()
{
	Targets.Reset();
	SkyId = HorizonId = GroundId = NAME_None;
	if (!GetWorld()) return;
	for (TActorIterator<ARubiconGazeTargetActor> It(GetWorld()); It; ++It)
	{
		Targets.Add(*It);
		switch (It->Resolution)
		{
		case ERubiconGazeResolution::SkyAbove: SkyId = It->TargetId; break;
		case ERubiconGazeResolution::Horizon: HorizonId = It->TargetId; break;
		case ERubiconGazeResolution::GroundBelow: GroundId = It->TargetId; break;
		default: break;
		}
	}
}

FName URubiconGazeResolverComponent::Resolve() const
{
	const UCameraComponent* Cam = Camera.Get();
	if (!Cam) return NAME_None;
	const FVector Origin = Cam->GetComponentLocation();
	const FVector Dir = Cam->GetForwardVector();

	FCollisionQueryParams Params(SCENE_QUERY_STAT(RubiconGaze), false);
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin + Dir * MaxDistance, ECC_Visibility, Params))
	{
		if (const UPrimitiveComponent* C = Hit.GetComponent())
		{
			for (const FName& Tag : C->ComponentTags)
			{
				const FName Id = IdFromTag(Tag);
				if (!Id.IsNone()) return Id;
			}
		}
		if (const AActor* A = Hit.GetActor())
		{
			for (const FName& Tag : A->Tags)
			{
				const FName Id = IdFromTag(Tag);
				if (!Id.IsNone()) return Id;
			}
		}
	}

	FName Best;
	double BestAngle = ConeHalfAngleDeg;
	for (const TWeakObjectPtr<ARubiconGazeTargetActor>& W : Targets)
	{
		const ARubiconGazeTargetActor* T = W.Get();
		if (!T || T->Resolution != ERubiconGazeResolution::Cone) continue;
		const double A = ConeAngleToSphere(Origin, Dir, T->GetGazeBounds());
		if (A <= BestAngle)
		{
			BestAngle = A;
			Best = T->TargetId;
		}
	}
	if (!Best.IsNone()) return Best;

	const double PitchDeg = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Dir.Z, -1.0, 1.0)));
	return ResolveByPitch(PitchDeg, HorizonBandDeg, SkyId, HorizonId, GroundId);
}

void URubiconGazeResolverComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	RefreshTimer += DeltaTime;
	if (RefreshTimer > 2.0)
	{
		RefreshTimer = 0;
		RefreshTargets(); // gaze actors can be spawned after the pawn (greybox BeginPlay order)
	}

	LastRaw = Resolve();
	const FName Resolved = Hysteresis.Update(LastRaw, static_cast<double>(DeltaTime) * 1000.0, HysteresisMs);
	if (URubiconNarrativeSubsystem* N = GetWorld()->GetSubsystem<URubiconNarrativeSubsystem>())
	{
		N->SetGazeTarget(Resolved);
	}

	if (bDrawDebug && Camera.IsValid())
	{
		const FVector O = Camera->GetComponentLocation();
		DrawDebugLine(GetWorld(), O + Camera->GetForwardVector() * 30.0, O + Camera->GetForwardVector() * 5000.0, FColor::Red, false, -1.f, 0, 0.5f);
	}
}
