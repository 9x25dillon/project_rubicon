// Elevation: the cross is raised with the player on it. Two variants, chosen per frame by the core
// (FRubiconFrameOutputs.ElevationTransition):
//
//   SlowTilt (default)      Embodiment: the WORLD rotates about the cross base from the start pose to upright
//                           over the raising phase (smoothstep). The cross, the body and the player's camera
//                           are never moved or rotated by code. Witness: the world stays put and the cross
//                           itself leans and rises, so the raising is seen from outside.
//   FadeSettle (Comfort or  No motion at all: fade to black, cut to the upright pose, fade back in. Switching to
//   Reduced Effects)        Comfort mid-tilt cuts the same way.
//
// Pose: lean-back angle and lowering, both zero once settled. The start pose is deliberately modest (comfort).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RubiconNarrativeTypes.h"
#include "RubiconElevationDirectorComponent.generated.h"

USTRUCT(BlueprintType)
struct RUBICON_API FRubiconElevationPose
{
	GENERATED_BODY()
	/** Degrees the cross leans back from vertical. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Elevation") double LeanBackDeg = 0;
	/** How far below its final height the cross (and the player) sits, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Elevation") double LowerCm = 0;

	static FRubiconElevationPose Make(double InLeanBackDeg, double InLowerCm)
	{
		FRubiconElevationPose P;
		P.LeanBackDeg = InLeanBackDeg;
		P.LowerCm = InLowerCm;
		return P;
	}
	static FRubiconElevationPose Lerp(const FRubiconElevationPose& A, const FRubiconElevationPose& B, double T);
	/** World-root transform that shows this pose from the embodied view (inverse of the cross motion). */
	FTransform EmbodiedWorldTransform() const;
	/** Cross-root transform that shows this pose from outside (Witness). */
	FTransform WitnessCrossTransform() const;
};

UCLASS(ClassGroup = (Rubicon), meta = (BlueprintSpawnableComponent))
class RUBICON_API URubiconElevationDirectorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URubiconElevationDirectorComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Elevation")
	FRubiconElevationPose StartPose = FRubiconElevationPose::Make(14.0, 70.0);

	UPROPERTY(EditAnywhere, Category = "Elevation") float FadeOutSeconds = 0.4f;
	UPROPERTY(EditAnywhere, Category = "Elevation") float FadeInSeconds = 0.9f;

	/** Pose implied by the frame, before any fade-settle hold. Pure; unit-tested. */
	static FRubiconElevationPose TargetPose(const FRubiconFrameOutputs& Frame, const FRubiconElevationPose& Start);

	UFUNCTION(BlueprintPure, Category = "Rubicon")
	FRubiconElevationPose GetAppliedPose() const { return Applied; }

private:
	enum class ECutState : uint8 { None, FadingOut, FadingIn };
	ECutState Cut = ECutState::None;
	float CutTimer = 0.f;
	bool bSettledByCut = false;
	FRubiconElevationPose Applied = FRubiconElevationPose::Make(14.0, 70.0);

	void ApplyPose(const FRubiconElevationPose& Pose, bool bWitness);
	void Fade(float From, float To, float Seconds, bool bHold);
};
