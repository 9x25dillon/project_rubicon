#include "RubiconElevationDirectorComponent.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "RubiconGolgothaGreybox.h"
#include "RubiconNarrativeSubsystem.h"

FRubiconElevationPose FRubiconElevationPose::Lerp(const FRubiconElevationPose& A, const FRubiconElevationPose& B, double T)
{
	FRubiconElevationPose P;
	P.LeanBackDeg = FMath::Lerp(A.LeanBackDeg, B.LeanBackDeg, T);
	P.LowerCm = FMath::Lerp(A.LowerCm, B.LowerCm, T);
	return P;
}

FTransform FRubiconElevationPose::EmbodiedWorldTransform() const
{
	// Cross motion relative to the world: T_cross = Translate(0,0,-Lower) * Rotate(pitch +Lean) about the base.
	// The embodied view keeps the cross fixed, so the world takes the inverse: Rotate(-Lean) * Translate(0,0,+Lower).
	const FRotator R(-LeanBackDeg, 0, 0);
	return FTransform(R, R.RotateVector(FVector(0, 0, LowerCm)));
}

FTransform FRubiconElevationPose::WitnessCrossTransform() const
{
	return FTransform(FRotator(LeanBackDeg, 0, 0), FVector(0, 0, -LowerCm));
}

URubiconElevationDirectorComponent::URubiconElevationDirectorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

FRubiconElevationPose URubiconElevationDirectorComponent::TargetPose(const FRubiconFrameOutputs& Frame, const FRubiconElevationPose& Start)
{
	const FRubiconElevationPose Upright;
	if (!Frame.bValid || Frame.State == ERubiconTimelineState::Threshold) return Start;
	if (Frame.State == ERubiconTimelineState::Elevation && Frame.Phase == ERubiconPhase::Raising)
	{
		const double P = FMath::Clamp(Frame.PhaseProgress, 0.0, 1.0);
		return FRubiconElevationPose::Lerp(Start, Upright, P * P * (3.0 - 2.0 * P));
	}
	return Upright;
}

void URubiconElevationDirectorComponent::Fade(float From, float To, float Seconds, bool bHold)
{
	if (APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		Cam->StartCameraFade(From, To, Seconds, FLinearColor::Black, false, bHold);
	}
}

void URubiconElevationDirectorComponent::ApplyPose(const FRubiconElevationPose& Pose, bool bWitness)
{
	Applied = Pose;
	ARubiconGolgothaGreybox* G = Cast<ARubiconGolgothaGreybox>(GetOwner());
	if (!G) return;
	if (bWitness)
	{
		G->GetWorldRoot()->SetRelativeTransform(FTransform::Identity);
		G->GetCrossRoot()->SetRelativeTransform(Pose.WitnessCrossTransform());
	}
	else
	{
		G->GetCrossRoot()->SetRelativeTransform(FTransform::Identity);
		G->GetWorldRoot()->SetRelativeTransform(Pose.EmbodiedWorldTransform());
	}
}

void URubiconElevationDirectorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const URubiconNarrativeSubsystem* N = GetWorld() ? GetWorld()->GetSubsystem<URubiconNarrativeSubsystem>() : nullptr;
	if (!N) return;
	const FRubiconFrameOutputs& F = N->GetFrameOutputsRef();
	// Witness and a telling both put the player off the cross; the cross then rises instead of the world tilting.
	const bool bWitness = F.bValid && F.View != ERubiconViewMode::Embodiment;
	const FRubiconElevationPose Target = TargetPose(F, StartPose);
	const bool bMoving = Target.LeanBackDeg != 0.0 || Target.LowerCm != 0.0;
	const bool bRaising = F.bValid && F.State == ERubiconTimelineState::Elevation && F.Phase == ERubiconPhase::Raising;
	const bool bFadeSettle = F.ElevationTransition == ERubiconElevationTransition::FadeSettle;

	if (F.bValid && F.State == ERubiconTimelineState::Threshold) bSettledByCut = false;

	// Fade-settle: never animate. When the raising begins (or Comfort is switched on mid-raise), cut to upright.
	if (Cut == ECutState::None && bFadeSettle && bRaising && !bWitness && !bSettledByCut && bMoving)
	{
		Cut = ECutState::FadingOut;
		CutTimer = 0.f;
		Fade(0.f, 1.f, FadeOutSeconds, true);
	}
	if (Cut == ECutState::FadingOut)
	{
		CutTimer += DeltaTime;
		if (CutTimer >= FadeOutSeconds)
		{
			bSettledByCut = true;
			ApplyPose(FRubiconElevationPose(), bWitness); // fully black: cut
			Cut = ECutState::FadingIn;
			CutTimer = 0.f;
			Fade(1.f, 0.f, FadeInSeconds, false);
		}
		return; // hold the pre-cut pose while fading out: no motion during the fade
	}
	if (Cut == ECutState::FadingIn)
	{
		CutTimer += DeltaTime;
		if (CutTimer >= FadeInSeconds) Cut = ECutState::None;
	}

	ApplyPose(bSettledByCut ? FRubiconElevationPose() : (bFadeSettle && !bWitness && bRaising ? Applied : Target), bWitness);
}
