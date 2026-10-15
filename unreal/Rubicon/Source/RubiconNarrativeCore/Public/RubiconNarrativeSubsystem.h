// Fixed-step host for the narrative core.
//
//   engine frame (variable dt) --> FFixedStepClock --> N x Step(manifest, state, input, 10 ms) --> events --> delegates
//                                                                                    \--> SelectFrame --> FRubiconFrameOutputs
//
// Inputs are latched between ticks: gaze and breath are levels (the latest value is used for every tick until it
// changes); commands are edges (queued, all delivered on the next tick, in call order). Every tick's input is
// recorded, so any play session can be exported as a golden-format replay and re-run bit for bit (I6).
//
// The subsystem never rotates or moves anything; it only publishes state. Rendering, audio and UI read
// FRubiconFrameOutputs and the per-event delegates.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RubiconNarrativeTypes.h"
#include "NarrativeCore/RubiconCoreTypes.h"
#include "NarrativeCore/RubiconFixedStep.h"

#include <vector>

#include "RubiconNarrativeSubsystem.generated.h"

class URubiconEpisodeAsset;

DECLARE_LOG_CATEGORY_EXTERN(LogRubiconNarrative, Log, All);

UCLASS()
class RUBICONNARRATIVECORE_API URubiconNarrativeSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/* USubsystem / FTickableGameObject */
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

	/** Compile the episode (validated) and start a fresh session. Returns false and logs every error otherwise. */
	UFUNCTION(BlueprintCallable, Category = "Rubicon|Session")
	bool StartSession(URubiconEpisodeAsset* Episode, int32 Seed, const FRubiconSettings& Settings);

	UFUNCTION(BlueprintPure, Category = "Rubicon|Session")
	bool IsSessionActive() const { return bActive; }

	UFUNCTION(BlueprintPure, Category = "Rubicon|Session")
	FRubiconFrameOutputs GetFrameOutputs() const { return Frame; }

	/** Native, copy-free access to the latest frame. */
	const FRubiconFrameOutputs& GetFrameOutputsRef() const { return Frame; }

	/* Level inputs */

	/** Gaze target id from the manifest (NAME_None = nothing). Unknown ids are logged once and treated as none. */
	UFUNCTION(BlueprintCallable, Category = "Rubicon|Input")
	void SetGazeTarget(FName TargetId);

	/** Optional breath signal [0,1]; affects audio focus only. */
	UFUNCTION(BlueprintCallable, Category = "Rubicon|Input")
	void SetBreath(float Breath01);

	UFUNCTION(BlueprintCallable, Category = "Rubicon|Input")
	void ClearBreath();

	/** Measured head motion (angular deg/s, linear cm/s), smoothed by the caller. Sustained motion builds exertion. */
	UFUNCTION(BlueprintCallable, Category = "Rubicon|Input")
	void SetHeadMotion(float DegPerSec, float CmPerSec);

	/** Voice activity [0,1] from the microphone (talking builds exertion; speaking the saying is never penalised by a swoon). */
	UFUNCTION(BlueprintCallable, Category = "Rubicon|Input")
	void SetVoiceActivity(float Voice01);

	/** Stop feeding motion and voice (e.g. desktop debugging without an HMD). */
	UFUNCTION(BlueprintCallable, Category = "Rubicon|Input")
	void ClearExertionInputs();

	/* Commands (delivered on the next 10 ms tick, in call order) */

	UFUNCTION(BlueprintCallable, Category = "Rubicon|Commands") void AcknowledgeAdvisory();
	UFUNCTION(BlueprintCallable, Category = "Rubicon|Commands") void TriggerSaying(ERubiconTriggerVia Via = ERubiconTriggerVia::Select);
	UFUNCTION(BlueprintCallable, Category = "Rubicon|Commands") void SetMode(ERubiconExperienceMode Mode);
	UFUNCTION(BlueprintCallable, Category = "Rubicon|Commands") void SetModeWithVantage(ERubiconExperienceMode Mode, ERubiconWitnessVantage Vantage);
	UFUNCTION(BlueprintCallable, Category = "Rubicon|Commands") void ToggleWitness();
	UFUNCTION(BlueprintCallable, Category = "Rubicon|Commands") void SetLayer(ERubiconVisualLayer Layer);
	UFUNCTION(BlueprintCallable, Category = "Rubicon|Commands") void ToggleLayer();
	UFUNCTION(BlueprintCallable, Category = "Rubicon|Commands") void SetComfort(bool bOn);
	UFUNCTION(BlueprintCallable, Category = "Rubicon|Commands") void ToggleComfort();
	UFUNCTION(BlueprintCallable, Category = "Rubicon|Commands") void Pause();
	UFUNCTION(BlueprintCallable, Category = "Rubicon|Commands") void Resume();
	UFUNCTION(BlueprintCallable, Category = "Rubicon|Commands") void TogglePause();
	UFUNCTION(BlueprintCallable, Category = "Rubicon|Commands") void Exit();
	UFUNCTION(BlueprintCallable, Category = "Rubicon|Commands") void OpenEvidence(const FString& SourceId);
	UFUNCTION(BlueprintCallable, Category = "Rubicon|Commands") void CloseEvidence();
	UFUNCTION(BlueprintCallable, Category = "Rubicon|Commands") void SetIntensityCeiling(float Ceiling01);
	UFUNCTION(BlueprintCallable, Category = "Rubicon|Commands") void SetReducedEffects(bool bOn);
	UFUNCTION(BlueprintCallable, Category = "Rubicon|Commands") void SetSeated(bool bSeated);
	/** Off: exertion never takes consciousness (and wakes him at once if he is out). */
	UFUNCTION(BlueprintCallable, Category = "Rubicon|Commands") void SetSwoonEnabled(bool bOn);

	/** Write the recorded session as golden-format JSON (seed, settings, per-tick inputs, hashes) for QA and parity. */
	UFUNCTION(BlueprintCallable, Category = "Rubicon|Replay")
	bool ExportReplay(const FString& FilePath) const;

	UFUNCTION(BlueprintPure, Category = "Rubicon|Session")
	FRubiconFragmentInfo GetFragmentInfo(FName FragmentId) const;

	/** Locale lookup (returns the key itself when missing). */
	UFUNCTION(BlueprintPure, Category = "Rubicon|Session")
	FString GetLocaleString(const FString& Key) const;

	/** The consent advisory shown at the Threshold (manifest.advisory). */
	UFUNCTION(BlueprintPure, Category = "Rubicon|Session")
	void GetAdvisory(FString& Title, FString& Body) const;

	/* Core access for engine-side readers (read-only). */
	const RubiconCore::FManifest* GetCoreManifest() const { return bActive ? &Manifest : nullptr; }
	const RubiconCore::FSessionState* GetCoreState() const { return bActive ? &State : nullptr; }

	/* One delegate per event type. */
	UPROPERTY(BlueprintAssignable, Category = "Rubicon|Events") FRubiconTimelineEnterSignature OnTimelineEnter;
	UPROPERTY(BlueprintAssignable, Category = "Rubicon|Events") FRubiconTimelinePhaseSignature OnTimelinePhase;
	UPROPERTY(BlueprintAssignable, Category = "Rubicon|Events") FRubiconSayingTriggerSignature OnSayingTrigger;
	UPROPERTY(BlueprintAssignable, Category = "Rubicon|Events") FRubiconModeChangeSignature OnModeChange;
	UPROPERTY(BlueprintAssignable, Category = "Rubicon|Events") FRubiconLayerChangeSignature OnLayerChange;
	UPROPERTY(BlueprintAssignable, Category = "Rubicon|Events") FRubiconComfortChangeSignature OnComfortChange;
	UPROPERTY(BlueprintAssignable, Category = "Rubicon|Events") FRubiconPauseChangeSignature OnPauseChange;
	UPROPERTY(BlueprintAssignable, Category = "Rubicon|Events") FRubiconExitSignature OnExit;
	UPROPERTY(BlueprintAssignable, Category = "Rubicon|Events") FRubiconEvidenceChangeSignature OnEvidenceChange;
	UPROPERTY(BlueprintAssignable, Category = "Rubicon|Events") FRubiconSettingsChangeSignature OnSettingsChange;
	UPROPERTY(BlueprintAssignable, Category = "Rubicon|Events") FRubiconFragmentSignature OnRacingThoughtsFragment;
	UPROPERTY(BlueprintAssignable, Category = "Rubicon|Events") FRubiconCommandRejectedSignature OnCommandRejected;
	/** A dialogue cue starts: play its VO or sound at Cue.Origin (narrator: head-locked), subtitle Cue.Text. */
	UPROPERTY(BlueprintAssignable, Category = "Rubicon|Events") FRubiconCueSignature OnDialogueCue;
	/** He loses consciousness (cause "exertion") or wakes ("recovered", "saying", "death", "setting"). Fade the view on it. */
	UPROPERTY(BlueprintAssignable, Category = "Rubicon|Events") FRubiconConsciousnessSignature OnConsciousnessChange;

	/** Maximum recorded ticks (about 2.8 hours at 100 Hz); recording stops beyond it, the session does not. */
	static constexpr int32 MaxRecordedTicks = 1000000;

private:
	void Enqueue(RubiconCore::FCommand Command);
	void StepOnce();
	void Broadcast(const std::vector<RubiconCore::FEvent>& Events);
	void RefreshFrame();

	bool bActive = false;
	RubiconCore::FManifest Manifest;
	RubiconCore::FLocale Locale;
	RubiconCore::FSessionState State;
	RubiconCore::FSettingsPatch InitialSettings;
	RubiconCore::FFixedStepClock Clock;
	int64 TickCount = 0;

	int PendingGaze = -1;
	TOptional<double> PendingBreath;
	TOptional<double> PendingHeadDeg;
	TOptional<double> PendingHeadCm;
	TOptional<double> PendingVoice;
	std::vector<RubiconCore::FCommand> PendingCommands;
	TSet<FName> WarnedUnknownGaze;

	std::vector<RubiconCore::FTickInput> Recorded;

	UPROPERTY(Transient)
	FRubiconFrameOutputs Frame;

	UPROPERTY(Transient)
	TObjectPtr<URubiconEpisodeAsset> ActiveEpisode;
};
