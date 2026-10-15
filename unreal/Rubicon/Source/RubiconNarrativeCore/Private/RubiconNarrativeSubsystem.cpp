#include "RubiconNarrativeSubsystem.h"

#include "RubiconConvert.h"
#include "RubiconEpisodeAsset.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "NarrativeCore/RubiconFrame.h"
#include "NarrativeCore/RubiconJson.h"
#include "NarrativeCore/RubiconReplay.h"
#include "NarrativeCore/RubiconSession.h"

DEFINE_LOG_CATEGORY(LogRubiconNarrative);

using RubiconConvert::ToFString;
using RubiconConvert::ToUE;

bool URubiconNarrativeSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void URubiconNarrativeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void URubiconNarrativeSubsystem::Deinitialize()
{
	bActive = false;
	PendingCommands.clear();
	Recorded.clear();
	Super::Deinitialize();
}

TStatId URubiconNarrativeSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URubiconNarrativeSubsystem, STATGROUP_Tickables);
}

bool URubiconNarrativeSubsystem::StartSession(URubiconEpisodeAsset* Episode, int32 Seed, const FRubiconSettings& Settings)
{
	bActive = false;
	if (!Episode)
	{
		UE_LOG(LogRubiconNarrative, Error, TEXT("StartSession: no episode asset (run Rubicon.ImportEpisode in the editor)"));
		return false;
	}
	TArray<FString> Errors, Warnings;
	RubiconCore::FManifest NewManifest;
	RubiconCore::FLocale NewLocale;
	if (!Episode->CompileCore(NewManifest, NewLocale, Errors, &Warnings))
	{
		for (const FString& E : Errors) UE_LOG(LogRubiconNarrative, Error, TEXT("manifest: %s"), *E);
		return false;
	}
	for (const FString& W : Warnings) UE_LOG(LogRubiconNarrative, Warning, TEXT("manifest: %s"), *W);

	Manifest = MoveTemp(NewManifest);
	Locale = MoveTemp(NewLocale);
	ActiveEpisode = Episode;
	InitialSettings = RubiconConvert::ToPatch(Settings);
	State = RubiconCore::CreateSession(Manifest, static_cast<uint32>(Seed), InitialSettings);
	Clock = RubiconCore::FFixedStepClock();
	TickCount = 0;
	PendingGaze = -1;
	PendingBreath.Reset();
	PendingCommands.clear();
	WarnedUnknownGaze.Reset();
	Recorded.clear();
	bActive = true;
	RefreshFrame();
	UE_LOG(LogRubiconNarrative, Log, TEXT("session started: %s (%s), seed %d"), *ToFString(Manifest.Title), *ToFString(Manifest.Edition), Seed);
	return true;
}

void URubiconNarrativeSubsystem::Tick(float DeltaTime)
{
	if (!bActive) return;
	const int Steps = Clock.Advance(static_cast<double>(DeltaTime) * 1000.0, RubiconCore::TickMs);
	for (int I = 0; I < Steps && bActive; ++I) StepOnce();
	if (Steps > 0) RefreshFrame();
}

void URubiconNarrativeSubsystem::StepOnce()
{
	RubiconCore::FTickInput Input;
	Input.Gaze = PendingGaze;
	if (PendingBreath.IsSet()) Input.Breath = PendingBreath.GetValue();
	Input.Commands.swap(PendingCommands);

	RubiconCore::FStepResult R = RubiconCore::Step(Manifest, State, Input, RubiconCore::TickMs);
	if (Recorded.size() < static_cast<size_t>(MaxRecordedTicks)) Recorded.push_back(Input);
	State = MoveTemp(R.State);
	++TickCount;
	Broadcast(R.Events);
}

void URubiconNarrativeSubsystem::Broadcast(const std::vector<RubiconCore::FEvent>& Events)
{
	using RubiconCore::EEventType;
	for (const RubiconCore::FEvent& E : Events)
	{
		switch (E.Type)
		{
		case EEventType::TimelineEnter: OnTimelineEnter.Broadcast(E.T, ToUE(E.State)); break;
		case EEventType::TimelinePhase: OnTimelinePhase.Broadcast(E.T, ToUE(E.State), ToUE(E.Phase)); break;
		case EEventType::SayingTrigger: OnSayingTrigger.Broadcast(E.T, ToUE(E.Saying), ToUE(E.Via)); break;
		case EEventType::ModeChange: OnModeChange.Broadcast(E.T, ToUE(E.Mode), ToUE(E.Vantage), E.bForced); break;
		case EEventType::LayerChange: OnLayerChange.Broadcast(E.T, ToUE(E.Layer)); break;
		case EEventType::ComfortChange: OnComfortChange.Broadcast(E.T, E.bOn); break;
		case EEventType::PauseChange: OnPauseChange.Broadcast(E.T, E.bPaused); break;
		case EEventType::Exit: OnExit.Broadcast(E.T); break;
		case EEventType::EvidenceChange:
			OnEvidenceChange.Broadcast(E.T, E.SourceIndex >= 0 ? ToFString(Manifest.Sources[static_cast<size_t>(E.SourceIndex)].Id) : FString());
			break;
		case EEventType::SettingsChange: OnSettingsChange.Broadcast(E.T, RubiconConvert::ToUE(E.Settings)); break;
		case EEventType::RtFragment: OnRacingThoughtsFragment.Broadcast(E.T, RubiconConvert::FragmentInfo(Manifest, Locale, E.Fragment), E.Voice); break;
		case EEventType::CommandRejected:
			UE_LOG(LogRubiconNarrative, Verbose, TEXT("command rejected: %s (%s)"), *ToFString(RubiconCore::ToString(E.Command)), *ToFString(E.Reason));
			OnCommandRejected.Broadcast(E.T, ToFString(RubiconCore::ToString(E.Command)), ToFString(E.Reason));
			break;
		case EEventType::CueStart: OnDialogueCue.Broadcast(E.T, RubiconConvert::CueInfo(Manifest, Locale, E.Cue)); break;
		}
	}
}

void URubiconNarrativeSubsystem::RefreshFrame()
{
	if (!bActive) return;
	const RubiconCore::FFrameOutputs F = RubiconCore::SelectFrame(Manifest, State, &Locale);
	RubiconConvert::ToUE(Manifest, F, TickCount, Frame);
}

void URubiconNarrativeSubsystem::SetGazeTarget(FName TargetId)
{
	if (!bActive) return;
	if (TargetId.IsNone()) { PendingGaze = -1; return; }
	const int Idx = Manifest.FindGazeTarget(RubiconConvert::ToUtf8(TargetId.ToString()));
	if (Idx < 0 && !WarnedUnknownGaze.Contains(TargetId))
	{
		WarnedUnknownGaze.Add(TargetId);
		UE_LOG(LogRubiconNarrative, Warning, TEXT("gaze target '%s' is not in the manifest; treating as no target"), *TargetId.ToString());
	}
	PendingGaze = Idx;
}

void URubiconNarrativeSubsystem::SetBreath(float Breath01) { PendingBreath = FMath::Clamp(static_cast<double>(Breath01), 0.0, 1.0); }
void URubiconNarrativeSubsystem::ClearBreath() { PendingBreath.Reset(); }

void URubiconNarrativeSubsystem::Enqueue(RubiconCore::FCommand Command)
{
	if (!bActive) return;
	PendingCommands.push_back(MoveTemp(Command));
}

void URubiconNarrativeSubsystem::AcknowledgeAdvisory() { Enqueue(RubiconCore::FCommand::Make(RubiconCore::ECommandType::AcknowledgeAdvisory)); }

void URubiconNarrativeSubsystem::TriggerSaying(ERubiconTriggerVia Via)
{
	RubiconCore::FCommand C = RubiconCore::FCommand::Make(RubiconCore::ECommandType::TriggerSaying);
	// Auto is the core's own clock; a player trigger is Select or Speech.
	C.Via = Via == ERubiconTriggerVia::Speech ? RubiconCore::ETriggerVia::Speech : RubiconCore::ETriggerVia::Select;
	Enqueue(MoveTemp(C));
}

void URubiconNarrativeSubsystem::SetMode(ERubiconExperienceMode Mode)
{
	RubiconCore::FCommand C = RubiconCore::FCommand::Make(RubiconCore::ECommandType::SetMode);
	C.Mode = RubiconConvert::ToCore(Mode);
	Enqueue(MoveTemp(C));
}

void URubiconNarrativeSubsystem::SetModeWithVantage(ERubiconExperienceMode Mode, ERubiconWitnessVantage Vantage)
{
	RubiconCore::FCommand C = RubiconCore::FCommand::Make(RubiconCore::ECommandType::SetMode);
	C.Mode = RubiconConvert::ToCore(Mode);
	C.Vantage = RubiconConvert::ToCore(Vantage);
	Enqueue(MoveTemp(C));
}

void URubiconNarrativeSubsystem::ToggleWitness()
{
	// Decided against the state the command will meet: pending SetMode commands are folded in.
	RubiconCore::EMode Effective = State.Mode;
	for (const RubiconCore::FCommand& C : PendingCommands) if (C.Type == RubiconCore::ECommandType::SetMode) Effective = C.Mode;
	SetMode(Effective == RubiconCore::EMode::Witness ? ERubiconExperienceMode::Embodiment : ERubiconExperienceMode::Witness);
}

void URubiconNarrativeSubsystem::SetLayer(ERubiconVisualLayer Layer)
{
	RubiconCore::FCommand C = RubiconCore::FCommand::Make(RubiconCore::ECommandType::SetLayer);
	C.Layer = RubiconConvert::ToCore(Layer);
	Enqueue(MoveTemp(C));
}

void URubiconNarrativeSubsystem::ToggleLayer()
{
	RubiconCore::ELayer Effective = State.Layer;
	for (const RubiconCore::FCommand& C : PendingCommands) if (C.Type == RubiconCore::ECommandType::SetLayer) Effective = C.Layer;
	SetLayer(Effective == RubiconCore::ELayer::Historical ? ERubiconVisualLayer::Theological : ERubiconVisualLayer::Historical);
}

void URubiconNarrativeSubsystem::SetComfort(bool bOn)
{
	RubiconCore::FCommand C = RubiconCore::FCommand::Make(RubiconCore::ECommandType::SetComfort);
	C.bOn = bOn;
	Enqueue(MoveTemp(C));
}

void URubiconNarrativeSubsystem::ToggleComfort()
{
	bool bEffective = State.bComfort;
	for (const RubiconCore::FCommand& C : PendingCommands) if (C.Type == RubiconCore::ECommandType::SetComfort) bEffective = C.bOn;
	SetComfort(!bEffective);
}

void URubiconNarrativeSubsystem::Pause() { Enqueue(RubiconCore::FCommand::Make(RubiconCore::ECommandType::Pause)); }
void URubiconNarrativeSubsystem::Resume() { Enqueue(RubiconCore::FCommand::Make(RubiconCore::ECommandType::Resume)); }

void URubiconNarrativeSubsystem::TogglePause()
{
	bool bEffective = State.bPaused;
	for (const RubiconCore::FCommand& C : PendingCommands)
	{
		if (C.Type == RubiconCore::ECommandType::Pause) bEffective = true;
		if (C.Type == RubiconCore::ECommandType::Resume) bEffective = false;
	}
	if (bEffective) Resume();
	else Pause();
}

void URubiconNarrativeSubsystem::Exit() { Enqueue(RubiconCore::FCommand::Make(RubiconCore::ECommandType::Exit)); }

void URubiconNarrativeSubsystem::OpenEvidence(const FString& SourceId)
{
	RubiconCore::FCommand C = RubiconCore::FCommand::Make(RubiconCore::ECommandType::OpenEvidence);
	C.SourceId = RubiconConvert::ToUtf8(SourceId);
	Enqueue(MoveTemp(C));
}

void URubiconNarrativeSubsystem::CloseEvidence() { Enqueue(RubiconCore::FCommand::Make(RubiconCore::ECommandType::CloseEvidence)); }

void URubiconNarrativeSubsystem::SetIntensityCeiling(float Ceiling01)
{
	RubiconCore::FCommand C = RubiconCore::FCommand::Make(RubiconCore::ECommandType::SetSettings);
	C.Settings.IntensityCeiling = static_cast<double>(Ceiling01);
	Enqueue(MoveTemp(C));
}

void URubiconNarrativeSubsystem::SetReducedEffects(bool bOn)
{
	RubiconCore::FCommand C = RubiconCore::FCommand::Make(RubiconCore::ECommandType::SetSettings);
	C.Settings.ReducedEffects = bOn;
	Enqueue(MoveTemp(C));
}

void URubiconNarrativeSubsystem::SetSeated(bool bSeated)
{
	RubiconCore::FCommand C = RubiconCore::FCommand::Make(RubiconCore::ECommandType::SetSettings);
	C.Settings.Seated = bSeated;
	Enqueue(MoveTemp(C));
}

FRubiconFragmentInfo URubiconNarrativeSubsystem::GetFragmentInfo(FName FragmentId) const
{
	if (!bActive) return FRubiconFragmentInfo();
	return RubiconConvert::FragmentInfo(Manifest, Locale, Manifest.FindFragment(RubiconConvert::ToUtf8(FragmentId.ToString())));
}

FString URubiconNarrativeSubsystem::GetLocaleString(const FString& Key) const
{
	const auto It = Locale.find(RubiconConvert::ToUtf8(Key));
	return It != Locale.end() ? ToFString(It->second) : Key;
}

void URubiconNarrativeSubsystem::GetAdvisory(FString& Title, FString& Body) const
{
	Title = bActive ? GetLocaleString(ToFString(Manifest.AdvisoryTitleKey)) : FString();
	Body = bActive ? GetLocaleString(ToFString(Manifest.AdvisoryBodyKey)) : FString();
}

bool URubiconNarrativeSubsystem::ExportReplay(const FString& FilePath) const
{
	if (!bActive) return false;
	using RubiconCore::FJson;
	const RubiconCore::FReplayResult R = RubiconCore::RunReplay(Manifest, State.Seed, Recorded, InitialSettings);
	FJson Doc = FJson::MakeObject();
	Doc.Set("manifestId", FJson::MakeString(Manifest.Id));
	Doc.Set("schemaVersion", FJson::MakeString(Manifest.SchemaVersion));
	Doc.Set("profile", FJson::MakeString("recorded"));
	Doc.Set("seed", FJson::MakeNumber(State.Seed));
	Doc.Set("tickMs", FJson::MakeNumber(RubiconCore::TickMs));
	FJson Settings = FJson::MakeObject();
	if (InitialSettings.IntensityCeiling) Settings.Set("intensityCeiling", FJson::MakeNumber(*InitialSettings.IntensityCeiling));
	if (InitialSettings.ReducedEffects) Settings.Set("reducedEffects", FJson::MakeBool(*InitialSettings.ReducedEffects));
	if (InitialSettings.Seated) Settings.Set("seated", FJson::MakeBool(*InitialSettings.Seated));
	if (InitialSettings.HapticsEnabled) Settings.Set("hapticsEnabled", FJson::MakeBool(*InitialSettings.HapticsEnabled));
	Doc.Set("settings", MoveTemp(Settings));
	FJson Inputs = FJson::MakeArray();
	for (const RubiconCore::FTickInput& In : Recorded) Inputs.Push(RubiconCore::TickInputToJson(Manifest, In));
	Doc.Set("inputs", MoveTemp(Inputs));
	FJson Expected = FJson::MakeObject();
	Expected.Set("stateHash", FJson::MakeString(R.StateHashHex));
	Expected.Set("eventHash", FJson::MakeString(R.EventHashHex));
	FJson Events = FJson::MakeArray();
	for (const RubiconCore::FEvent& E : R.Events) Events.Push(RubiconCore::EventToJson(Manifest, E));
	Expected.Set("events", MoveTemp(Events));
	Doc.Set("expected", MoveTemp(Expected));

	const bool bLive = RubiconCore::StateHash(Manifest, State) == R.StateHashHex;
	if (!bLive) UE_LOG(LogRubiconNarrative, Error, TEXT("ExportReplay: re-run diverged from the live session (determinism violation)"));
	const std::string Text = RubiconCore::Stringify(Doc);
	TArray<uint8> Bytes;
	Bytes.Append(reinterpret_cast<const uint8*>(Text.data()), static_cast<int32>(Text.size()));
	return FFileHelper::SaveArrayToFile(Bytes, *FilePath) && bLive;
}
