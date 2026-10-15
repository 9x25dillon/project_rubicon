// The player: a camera rig and nothing more. The body lives on the cross (ARubiconEmbodiedBody).
//
//   Root
//    \- TrackingOrigin   offset each frame only to clamp head POSITION (never rotation)
//         \- Camera      bLockToHmd: orientation and position come from the headset alone
//
// Embodiment: head rotation is free; head position may drift at most HeadClampRadius from the calibrated
//   centre (the eye point on the cross); beyond that the tracking origin absorbs the excess.
// Witness: the rig is moved, under a fade, to a vantage on the floor near the cross; head position is free
//   within WitnessRadius. Yaw is never changed by code unless URubiconSettings::bAlignYawOnWitness.
// Seated and standing: calibration maps wherever the head is at start (or on Recenter) to the eye point, so
//   the same rig serves both; Seated also tightens the clamp. Recenter is position only.
//
// Input (Enhanced Input, created in code so the project needs no input assets):
//   Select / acknowledge   Right trigger, Space, Enter, Gamepad A
//   Witness                Right A, W, Gamepad Y          Comfort    Right B, C, Gamepad X
//   Pause                  Left X, P, Gamepad Start       Exit       Left Menu, Escape, Gamepad Back
//   Layer                  Left Y, L                      Recenter   Left stick click, R
//   Debug HUD              Right stick click, F1          (desktop without HMD: mouse look)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "InputActionValue.h"
#include "RubiconNarrativeTypes.h"
#include "RubiconEmbodiedPawn.generated.h"

class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class URubiconDebugPanelComponent;
class URubiconGazeResolverComponent;

UCLASS()
class RUBICON_API ARubiconEmbodiedPawn : public APawn
{
	GENERATED_BODY()

public:
	ARubiconEmbodiedPawn();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PawnClientRestart() override;

	UPROPERTY(EditAnywhere, Category = "Embodiment") float HeadClampRadius = 12.f;
	UPROPERTY(EditAnywhere, Category = "Embodiment") float SeatedHeadClampRadius = 8.f;
	UPROPERTY(EditAnywhere, Category = "Witness") float WitnessRadius = 150.f;
	UPROPERTY(EditAnywhere, Category = "Witness") float WitnessEyeHeightStanding = 165.f;
	UPROPERTY(EditAnywhere, Category = "Witness") float WitnessEyeHeightSeated = 125.f;
	UPROPERTY(EditAnywhere, Category = "Witness") float WitnessFadeSeconds = 0.3f;

	/** Re-anchor the head position (not orientation) to the current eye point. */
	UFUNCTION(BlueprintCallable, Category = "Rubicon")
	void Recenter();

	/** Pure: tracking-origin offset that keeps the eye within Radius of the anchor. Unit-tested. */
	static FVector ClampCorrection(const FVector& HeadLocal, const FVector& CenterLocal, double Radius);

	UCameraComponent* GetCamera() const { return Camera; }

protected:
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USceneComponent> TrackingOrigin;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UCameraComponent> Camera;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<URubiconGazeResolverComponent> Gaze;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<URubiconDebugPanelComponent> DebugPanel;

	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> Mapping;
	UPROPERTY(Transient) TObjectPtr<UInputAction> SelectAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> WitnessAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> ComfortAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> PauseAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> ExitAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> LayerAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> RecenterAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> DebugAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> LookAction;

private:
	bool bHmd = false;
	bool bNeedsCalibration = true;
	FVector HeadCenter = FVector::ZeroVector; // calibrated head position in tracking space
	ERubiconExperienceMode PlacedMode = ERubiconExperienceMode::Embodiment;
	ERubiconWitnessVantage PlacedVantage = ERubiconWitnessVantage::BelovedDisciple;
	float MoveTimer = -1.f; // >= 0 while a witness/embodiment move is fading

	void BuildInput();
	void AddMappingContext();
	void PlaceForMode(ERubiconExperienceMode Mode, ERubiconWitnessVantage Vantage);
	FVector AnchorWorld(ERubiconExperienceMode Mode, ERubiconWitnessVantage Vantage, FRotator& OutYaw) const;

	UFUNCTION() void HandleModeChange(double TimeMs, ERubiconExperienceMode Mode, ERubiconWitnessVantage Vantage, bool bForced);
	UFUNCTION() void HandleSettingsChange(double TimeMs, const FRubiconSettings& Settings);

	void OnSelect();
	void OnWitness();
	void OnComfort();
	void OnPause();
	void OnExit();
	void OnLayer();
	void OnRecenter();
	void OnDebug();
	void OnLook(const FInputActionValue& Value);
};
