// Golgotha greybox: quarry-edge site near a road outside the city wall, a low cross, tagged gaze actors,
// horizon and sky, plus the two runtime drivers that read FRubiconFrameOutputs (atmosphere, elevation).
//
// Frame (cm): origin at the cross base, +X is the direction the crucified faces (toward the road, the crowd and
// the city wall), +Y is his right, +Z up.
//
//   Root (cross base)
//    |- WorldRoot   everything in the world; the slow-tilt elevation rotates THIS about the cross base
//    |    |- environment meshes (quarry pit and face, rock knoll)
//    |    \- gaze target actors (attached at runtime or by Rubicon.CreateGolgothaMap)
//    \- CrossRoot   the cross, the titulus and the embodied body; never moved while embodied, so the player's
//                   camera is never moved or rotated by code. In Witness the cross itself leans and rises instead.
//
// Placement basis per target is recorded in Layout[].Basis (gospel text vs. tradition vs. greybox convenience).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RubiconGazeTargetActor.h"
#include "RubiconNarrativeTypes.h"
#include "RubiconGolgothaGreybox.generated.h"

class ARubiconEmbodiedBody;
class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class UPostProcessComponent;
class URubiconAtmosphereDriverComponent;
class URubiconElevationDirectorComponent;
class URubiconEpisodeAsset;
class USkyAtmosphereComponent;
class USkyLightComponent;
class UStaticMeshComponent;

USTRUCT(BlueprintType)
struct RUBICON_API FRubiconGazePlacement
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gaze") FName TargetId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gaze") FVector Location = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gaze") FRotator Rotation = FRotator::ZeroRotator;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gaze") ERubiconGazeResolution Resolution = ERubiconGazeResolution::Cone;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gaze") ERubiconProxyShape Shape = ERubiconProxyShape::Figures;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gaze") FVector Extent = FVector(40, 40, 85);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gaze") int32 FigureCount = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gaze") FLinearColor Color = FLinearColor(0.35f, 0.32f, 0.28f);
	/** Attach to the cross (moves with it in Witness) instead of the world. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gaze") bool bAttachToCross = false;
	/** Why it is here: the passage, tradition or greybox convenience behind the placement. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gaze") FString Basis;
};

UCLASS()
class RUBICON_API ARubiconGolgothaGreybox : public AActor
{
	GENERATED_BODY()

public:
	ARubiconGolgothaGreybox();

	/** Gaze targets placed in the world. own_hands is provided by the embodied body, not by this list. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	TArray<FRubiconGazePlacement> Layout;

	/** Eye point of the embodied player on the cross (cross frame). Must match ARubiconEmbodiedBody::EyePoint. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	FVector EyePoint = FVector(12, 0, 210);

	/** Witness vantages (cross frame, floor level). John 19:25-26 places Mary and the beloved disciple by the cross. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	TMap<ERubiconWitnessVantage, FVector> WitnessVantages;

	UFUNCTION(BlueprintPure, Category = "Rubicon")
	FTransform GetEmbodimentEyeTransform() const;

	/** Floor-level transform at the vantage; yaw faces the cross. */
	UFUNCTION(BlueprintPure, Category = "Rubicon")
	FTransform GetWitnessFloorTransform(ERubiconWitnessVantage Vantage) const;

	/** Spawn any missing gaze target actors and the embodied body, attached to WorldRoot / CrossRoot. */
	UFUNCTION(BlueprintCallable, Category = "Rubicon")
	int32 EnsureGazeTargets();

	/** Manifest gaze target ids with no tagged actor in this world (should be empty). */
	UFUNCTION(BlueprintCallable, Category = "Rubicon")
	TArray<FName> FindMissingGazeTargets(const URubiconEpisodeAsset* Episode) const;

	USceneComponent* GetWorldRoot() const { return WorldRoot; }
	USceneComponent* GetCrossRoot() const { return CrossRoot; }
	UDirectionalLightComponent* GetSun() const { return Sun; }
	USkyAtmosphereComponent* GetSkyAtmosphere() const { return SkyAtmosphere; }
	UExponentialHeightFogComponent* GetHeightFog() const { return HeightFog; }
	UPostProcessComponent* GetPostProcess() const { return PostProcess; }

	virtual void BeginPlay() override;

	static TArray<FRubiconGazePlacement> DefaultLayout();

protected:
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USceneComponent> WorldRoot;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USceneComponent> CrossRoot;

	UPROPERTY(VisibleAnywhere, Category = "Environment") TObjectPtr<UStaticMeshComponent> QuarryFloor;
	UPROPERTY(VisibleAnywhere, Category = "Environment") TObjectPtr<UStaticMeshComponent> QuarryFace;
	UPROPERTY(VisibleAnywhere, Category = "Environment") TObjectPtr<UStaticMeshComponent> Knoll;
	UPROPERTY(VisibleAnywhere, Category = "Cross") TObjectPtr<UStaticMeshComponent> Upright;
	UPROPERTY(VisibleAnywhere, Category = "Cross") TObjectPtr<UStaticMeshComponent> Crossbeam;

	UPROPERTY(VisibleAnywhere, Category = "Sky") TObjectPtr<UDirectionalLightComponent> Sun;
	UPROPERTY(VisibleAnywhere, Category = "Sky") TObjectPtr<USkyAtmosphereComponent> SkyAtmosphere;
	UPROPERTY(VisibleAnywhere, Category = "Sky") TObjectPtr<USkyLightComponent> SkyLight;
	UPROPERTY(VisibleAnywhere, Category = "Sky") TObjectPtr<UExponentialHeightFogComponent> HeightFog;
	UPROPERTY(VisibleAnywhere, Category = "Sky") TObjectPtr<UPostProcessComponent> PostProcess;

	UPROPERTY(VisibleAnywhere, Category = "Drivers") TObjectPtr<URubiconAtmosphereDriverComponent> AtmosphereDriver;
	UPROPERTY(VisibleAnywhere, Category = "Drivers") TObjectPtr<URubiconElevationDirectorComponent> ElevationDirector;
};
