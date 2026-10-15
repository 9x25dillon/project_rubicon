// The crucified body, greybox. Attached to the cross (not to the camera), posed once by analytic two-bone IK.
//
// It stays on the cross in every mode: in Embodiment the player's head sits at the body's eye point and looks
// down at it; in Witness the player sees it from outside. Only the hands collide with gaze traces; they carry
// Gaze.Target.own_hands (component tags, and the actor tag for tooling). Nothing here moves at runtime, and
// nothing is gory: proxies are plain primitives.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RubiconEmbodiedBody.generated.h"

class UStaticMeshComponent;

USTRUCT(BlueprintType)
struct RUBICON_API FRubiconTwoBoneSolution
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "IK") FVector Root = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "IK") FVector Joint = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "IK") FVector End = FVector::ZeroVector;
	/** False when the target was out of reach and the chain was clamped to full extension. */
	UPROPERTY(BlueprintReadOnly, Category = "IK") bool bReached = true;
};

UCLASS()
class RUBICON_API ARubiconEmbodiedBody : public AActor
{
	GENERATED_BODY()

public:
	ARubiconEmbodiedBody();

	/** Analytic two-bone IK (law of cosines). Joint bends toward Pole. Pure; unit-tested. */
	static FRubiconTwoBoneSolution SolveTwoBone(const FVector& Root, const FVector& Target, double UpperLength, double LowerLength, const FVector& Pole);

	/* Pose, in the cross frame (origin at the cross base, X forward, Z up), cm. Defaults: an adult about 170 cm. */
	UPROPERTY(EditAnywhere, Category = "Pose") FVector EyePoint = FVector(12, 0, 210);
	UPROPERTY(EditAnywhere, Category = "Pose") double ShoulderHalfWidth = 19;
	UPROPERTY(EditAnywhere, Category = "Pose") double ShoulderDrop = 24;   // eye to shoulder line
	UPROPERTY(EditAnywhere, Category = "Pose") double TorsoLength = 52;    // shoulder line to hip line
	UPROPERTY(EditAnywhere, Category = "Pose") double UpperArm = 30;
	UPROPERTY(EditAnywhere, Category = "Pose") double Forearm = 27;
	UPROPERTY(EditAnywhere, Category = "Pose") double Thigh = 44;
	UPROPERTY(EditAnywhere, Category = "Pose") double Shin = 42;
	/** Wrists on the crossbeam, slightly above the shoulders: the arms are drawn up, the body has sagged. */
	UPROPERTY(EditAnywhere, Category = "Pose") FVector LeftWristOnBeam = FVector(-4, -64, 194);
	UPROPERTY(EditAnywhere, Category = "Pose") FVector RightWristOnBeam = FVector(-4, 64, 194);
	/** Feet against the upright, knees bent forward. */
	UPROPERTY(EditAnywhere, Category = "Pose") FVector LeftFoot = FVector(-2, -6, 62);
	UPROPERTY(EditAnywhere, Category = "Pose") FVector RightFoot = FVector(-2, 6, 60);

	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Body") TObjectPtr<USceneComponent> Root;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Parts;

	void Rebuild();
	UStaticMeshComponent* AddSegment(const FVector& A, const FVector& B, double Radius, bool bGazeHands);
	UStaticMeshComponent* AddBlob(const FVector& Center, const FVector& HalfExtent, bool bGazeHands);
};
