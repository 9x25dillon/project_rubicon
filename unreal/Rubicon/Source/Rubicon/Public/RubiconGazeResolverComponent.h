// HMD-forward gaze resolution with a cone and hysteresis. Feeds URubiconNarrativeSubsystem::SetGazeTarget.
//
//   1. line trace along the camera forward (ECC_Visibility); a hit whose component or actor carries
//      Gaze.Target.<id> resolves to <id>
//   2. otherwise the closest Cone target whose bounding sphere lies within ConeHalfAngleDeg of the ray
//   3. otherwise by pitch: above the band -> SkyAbove target, below -> GroundBelow target, else -> Horizon target
//   4. hysteresis: a new target must persist HysteresisMs before it replaces the current one
//
// Gaze is the player's agency; this component only reports where the head points. It never moves the camera.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RubiconGazeResolverComponent.generated.h"

class ARubiconGazeTargetActor;
class UCameraComponent;

/** Debounce: a raw target replaces the current one only after HoldMs of continuous agreement. */
struct RUBICON_API FRubiconGazeHysteresis
{
	FName Current;
	FName Candidate;
	double CandidateMs = 0;

	FName Update(FName Raw, double DeltaMs, double HoldMs);
};

UCLASS(ClassGroup = (Rubicon), meta = (BlueprintSpawnableComponent))
class RUBICON_API URubiconGazeResolverComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URubiconGazeResolverComponent();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, Category = "Gaze") float MaxDistance = 300000.f;
	UPROPERTY(EditAnywhere, Category = "Gaze", meta = (ClampMin = "0", ClampMax = "15")) float ConeHalfAngleDeg = 4.f;
	UPROPERTY(EditAnywhere, Category = "Gaze") float HysteresisMs = 150.f;
	UPROPERTY(EditAnywhere, Category = "Gaze") float HorizonBandDeg = 6.f;
	UPROPERTY(EditAnywhere, Category = "Debug") bool bDrawDebug = false;

	UFUNCTION(BlueprintPure, Category = "Rubicon") FName GetResolvedTarget() const { return Hysteresis.Current; }
	UFUNCTION(BlueprintPure, Category = "Rubicon") FName GetRawTarget() const { return LastRaw; }

	/** Gaze.Target.<id> -> <id>, NAME_None if the tag is not a gaze tag. */
	static FName IdFromTag(const FName& Tag);
	/** Step 3 above. Pure; unit-tested. */
	static FName ResolveByPitch(double PitchDeg, double BandDeg, FName SkyId, FName HorizonId, FName GroundId);
	/** Angular distance (deg) from the ray to the sphere's silhouette; <= 0 means the ray passes through it. */
	static double ConeAngleToSphere(const FVector& Origin, const FVector& Direction, const FSphere& Sphere);

private:
	TWeakObjectPtr<UCameraComponent> Camera;
	TArray<TWeakObjectPtr<ARubiconGazeTargetActor>> Targets;
	FName SkyId, HorizonId, GroundId;
	double RefreshTimer = 0;
	FRubiconGazeHysteresis Hysteresis;
	FName LastRaw;

	void RefreshTargets();
	FName Resolve() const;
};
