// A gaze target in the world. Carries the actor tag Gaze.Target.<id> for its manifest gaze target id.
//
// Resolution modes:
//   Collision   resolved when the HMD-forward trace hits this actor's proxy (people, crosses, ground)
//   Cone        also resolved when the gaze ray passes within the cone half-angle of its bounds (small or distant
//               figures that a strict line trace would make fiddly to hold)
//   SkyAbove    resolved when nothing is hit and gaze pitch is above the horizon band
//   Horizon     resolved when gaze pitch is within the horizon band and no tagged target is hit or in the cone
//   GroundBelow collision target, and the fallback for downward gaze that hits only untagged ground (road, quarry)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RubiconGazeTargetActor.generated.h"

class UStaticMeshComponent;

UENUM(BlueprintType)
enum class ERubiconGazeResolution : uint8 { Collision, Cone, SkyAbove, Horizon, GroundBelow };

UENUM(BlueprintType)
enum class ERubiconProxyShape : uint8 { None, Box, Cylinder, Sphere, Figures, HillRing, Cross };

UCLASS()
class RUBICON_API ARubiconGazeTargetActor : public AActor
{
	GENERATED_BODY()

public:
	ARubiconGazeTargetActor();

	/** Manifest gaze target id (content/passion.episode.json gazeTargets[].id). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gaze")
	FName TargetId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gaze")
	ERubiconGazeResolution Resolution = ERubiconGazeResolution::Cone;

	/**
	 * Greybox stand-in. Figures: standing figures scattered inside ProxyExtent (crowds, groups).
	 * HillRing: FigureCount low hills on a circle of radius ProxyExtent.X, height ProxyExtent.Z (the horizon).
	 * Cross: an upright (half thickness ProxyExtent.X, half height ProxyExtent.Z) and a crossbeam (half length
	 * ProxyExtent.Y) at 1.4 x ProxyExtent.Z, slightly forward of the upright.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Greybox")
	ERubiconProxyShape ProxyShape = ERubiconProxyShape::Box;

	/** Half extents in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Greybox")
	FVector ProxyExtent = FVector(40, 40, 90);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Greybox", meta = (ClampMin = "1", ClampMax = "64"))
	int32 FigureCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Greybox")
	FLinearColor ProxyColor = FLinearColor(0.35f, 0.32f, 0.28f);

	static FName TagFor(FName Id);

	/** World-space bounding sphere used by the cone test. */
	FSphere GetGazeBounds() const;

	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Gaze")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Proxies;

	void RebuildProxies();
};
