// Automation tests: C++ narrative core vs. the TypeScript reference.
//
//   Session Frontend > Automation > Rubicon.NarrativeCore.*
//   or: UnrealEditor-Cmd Rubicon.uproject -ExecCmds="Automation RunTests Rubicon.NarrativeCore; Quit" -unattended -nullrhi
//
// Golden files are produced by `npm run golden` in the repository root (not committed). The repository root is
// found as <ProjectDir>/../.. (unreal/Rubicon -> repo); override with -RubiconRepoRoot=<path>.

#include "Misc/AutomationTest.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RubiconConvert.h"
#include "NarrativeCore/RubiconContent.h"
#include "NarrativeCore/RubiconFixedStep.h"
#include "NarrativeCore/RubiconMath.h"
#include "NarrativeCore/RubiconRacingThoughts.h"
#include "NarrativeCore/RubiconReplay.h"
#include "NarrativeCore/RubiconSession.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace RubiconTests
{
	FString RepoRoot()
	{
		FString Root;
		if (FParse::Value(FCommandLine::Get(), TEXT("RubiconRepoRoot="), Root)) return Root;
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT(".."), TEXT("..")));
	}

	bool ReadUtf8(const FString& Path, std::string& Out)
	{
		TArray<uint8> Bytes;
		if (!FFileHelper::LoadFileToArray(Bytes, *Path)) return false;
		Out.assign(reinterpret_cast<const char*>(Bytes.GetData()), static_cast<size_t>(Bytes.Num()));
		return true;
	}

	bool ReadJson(FAutomationTestBase& Test, const FString& Path, RubiconCore::FJson& Out)
	{
		std::string Text, Error;
		if (!ReadUtf8(Path, Text))
		{
			Test.AddError(FString::Printf(TEXT("cannot read %s (run `npm run golden` in the repository root)"), *Path));
			return false;
		}
		if (!RubiconCore::ParseJson(Text, Out, &Error))
		{
			Test.AddError(FString::Printf(TEXT("%s: %s"), *Path, *RubiconConvert::ToFString(Error)));
			return false;
		}
		return true;
	}

	bool LoadContent(FAutomationTestBase& Test, RubiconCore::FManifest& M, RubiconCore::FLocale& L)
	{
		const FString Content = FPaths::Combine(RepoRoot(), TEXT("content"));
		std::string ManifestText, LocaleText;
		if (!ReadUtf8(FPaths::Combine(Content, TEXT("passion.episode.json")), ManifestText) || !ReadUtf8(FPaths::Combine(Content, TEXT("locale.en.json")), LocaleText))
		{
			Test.AddError(FString::Printf(TEXT("cannot read content from %s (set -RubiconRepoRoot=)"), *Content));
			return false;
		}
		std::vector<std::string> Errors;
		if (!RubiconCore::LoadEpisode(ManifestText, LocaleText, M, L, Errors))
		{
			for (const std::string& E : Errors) Test.AddError(RubiconConvert::ToFString(E));
			return false;
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRubiconGoldenReplayTest, "Rubicon.NarrativeCore.GoldenReplays",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FRubiconGoldenReplayTest::RunTest(const FString& Parameters)
{
	using namespace RubiconTests;
	RubiconCore::FManifest M;
	RubiconCore::FLocale L;
	if (!LoadContent(*this, M, L)) return false;

	std::string MathFailure;
	TestTrue(TEXT("floating-point environment is bit-exact (no FMA contraction, no fast-math)"), RubiconCore::SelfTestDeterministicMath(&MathFailure));

	const FString Golden = FPaths::Combine(RepoRoot(), TEXT("golden"));
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *FPaths::Combine(Golden, TEXT("replay-*.json")), true, false);
	Files.Sort();
	if (Files.Num() == 0)
	{
		AddError(FString::Printf(TEXT("no replay-*.json in %s (run `npm run golden`)"), *Golden));
		return false;
	}

	for (const FString& File : Files)
	{
		RubiconCore::FJson Doc;
		if (!ReadJson(*this, FPaths::Combine(Golden, File), Doc)) continue;
		const RubiconCore::FGoldenReport R = RubiconCore::RunGolden(M, Doc, 1e-6);
		const FString Name = File + TEXT(" ") + RubiconConvert::ToFString(R.Name);
		// Acceptance: identical timeline.enter and saying.trigger, Racing Thoughts output within 1e-6.
		TestTrue(Name + TEXT(": timeline.enter and saying.trigger identical"), R.bTimelineEventsMatch);
		TestTrue(Name + TEXT(": Racing Thoughts trace within 1e-6"), R.bTraceWithinTolerance);
		// Stronger: every event identical and the final state bit-identical.
		TestTrue(Name + TEXT(": every event identical"), R.bAllEventsMatch);
		TestTrue(Name + TEXT(": state and event hashes identical"), R.bHashesMatch);
		for (const std::string& F : R.Failures) AddError(Name + TEXT(": ") + RubiconConvert::ToFString(F));
		AddInfo(FString::Printf(TEXT("%s: %d ticks, %d events, max trace error %g"), *Name, static_cast<int32>(R.Ticks), static_cast<int32>(R.Events),
			FMath::Max3(R.MaxIntensityError, R.MaxOutputError, R.MaxGainError)));
	}

	RubiconCore::FJson Vectors;
	if (ReadJson(*this, FPaths::Combine(Golden, TEXT("math-vectors.json")), Vectors))
	{
		const RubiconCore::FMathParityReport Mr = RubiconCore::RunMathVectors(Vectors);
		TestEqual(TEXT("exp vectors bit-exact"), static_cast<int32>(Mr.ExpMismatches), 0);
		TestEqual(TEXT("pow vectors bit-exact"), static_cast<int32>(Mr.PowMismatches), 0);
		if (!Mr.FirstMismatch.empty()) AddError(RubiconConvert::ToFString(Mr.FirstMismatch));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRubiconValidatorParityTest, "Rubicon.NarrativeCore.ValidatorParity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FRubiconValidatorParityTest::RunTest(const FString& Parameters)
{
	using namespace RubiconTests;
	RubiconCore::FJson Cases;
	if (!ReadJson(*this, FPaths::Combine(RepoRoot(), TEXT("golden"), TEXT("validator-cases.json")), Cases)) return false;
	const RubiconCore::FValidatorParityReport R = RubiconCore::RunValidatorCases(Cases);
	TestEqual(TEXT("validator cases identical to src/validate.ts"), static_cast<int32>(R.Passed), static_cast<int32>(R.Cases));
	for (const std::string& F : R.Failures) AddError(RubiconConvert::ToFString(F));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRubiconSafeguardTest, "Rubicon.NarrativeCore.Safeguards",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FRubiconSafeguardTest::RunTest(const FString& Parameters)
{
	using namespace RubiconTests;
	using namespace RubiconCore;
	FManifest M;
	FLocale L;
	if (!LoadContent(*this, M, L)) return false;
	const int Crowd = M.FindGazeTarget("crowd");
	const int Horizon = M.FindGazeTarget("horizon");

	auto RunMs = [&M](FSessionState& S, int Ms, int Gaze, std::vector<FCommand> Commands = {})
	{
		for (int T = 0; T < Ms; T += TickMs)
		{
			FTickInput In;
			In.Gaze = Gaze;
			if (T == 0) In.Commands = Commands;
			S = Step(M, S, In).State;
		}
	};
	auto RunUntil = [&](FSessionState& S, EStateId Id, int Gaze)
	{
		for (int Guard = 0; StateDef(M, S).Id != Id && Guard < 100000; ++Guard) RunMs(S, TickMs, Gaze);
	};
	auto Out = [&M](const FSessionState& S) { return OutputIntensity(M, StateDef(M, S), S); };

	// Gazing at the crowd raises output; looking away lowers it gradually.
	FSessionState A = CreateSession(M, 3);
	RunMs(A, TickMs, -1, { FCommand::Make(ECommandType::AcknowledgeAdvisory) });
	RunUntil(A, EStateId::Forgiveness, Horizon);
	FSessionState B = A;
	RunMs(A, 20000, Crowd);
	RunMs(B, 20000, Horizon);
	TestTrue(TEXT("crowd gaze raises Racing Thoughts output versus the horizon"), Out(A) > Out(B) + 0.2);
	const double Peak = Out(A);
	RunMs(A, 1000, Horizon);
	TestTrue(TEXT("looking away: still above 85% of peak after 1 s"), Out(A) > Peak * 0.85);
	RunMs(A, 9000, Horizon);
	TestTrue(TEXT("looking away: clearly lower after 10 s"), Out(A) < Peak - 0.15);

	// Comfort and Witness reduce output on the same tick, from every state.
	for (int Si = 1; Si < TimelineLength; ++Si)
	{
		FSessionState S = CreateSession(M, 5);
		RunMs(S, TickMs, Crowd, { FCommand::Make(ECommandType::AcknowledgeAdvisory) });
		RunUntil(S, static_cast<EStateId>(Si), Crowd);
		RunMs(S, 3000, Crowd);
		FSessionState W = S;
		FCommand Comfort = FCommand::Make(ECommandType::SetComfort);
		Comfort.bOn = true;
		RunMs(S, TickMs, Crowd, { Comfort });
		TestTrue(FString::Printf(TEXT("Comfort same tick in %s"), *RubiconConvert::ToFString(ToString(static_cast<EStateId>(Si)))), Out(S) <= M.RacingThoughts.ComfortGain + 1e-12);
		const double Before = Out(W);
		const bool bAlreadyWitness = W.Mode == EMode::Witness;
		FCommand Witness = FCommand::Make(ECommandType::SetMode);
		Witness.Mode = EMode::Witness;
		RunMs(W, TickMs, Crowd, { Witness });
		const double Bound = bAlreadyWitness ? Before : Before * M.RacingThoughts.WitnessEchoScale;
		TestTrue(FString::Printf(TEXT("Witness same tick in %s"), *RubiconConvert::ToFString(ToString(static_cast<EStateId>(Si)))), Out(W) <= Bound + 1e-9);
	}

	// Dialogue: Luke's exchange is heard before the Paradise saying, even when it is triggered at the earliest moment.
	{
		const int Road = M.FindGazeTarget("road");
		FSessionState S = CreateSession(M, 4);
		std::vector<std::string> Order;
		auto Collect = [&](const FStepResult& R)
		{
			for (const FEvent& E : R.Events)
			{
				if (E.Type == EEventType::CueStart && M.Cues[static_cast<size_t>(E.Cue)].Id.rfind("paradise_", 0) == 0) Order.push_back(M.Cues[static_cast<size_t>(E.Cue)].Id);
				if (E.Type == EEventType::SayingTrigger && E.Saying == EStateId::Paradise) Order.push_back("SAYING");
			}
		};
		FTickInput Ack;
		Ack.Commands.push_back(FCommand::Make(ECommandType::AcknowledgeAdvisory));
		FStepResult R = Step(M, S, Ack);
		S = R.State;
		for (int Guard = 0; Guard < 200000 && !(StateDef(M, S).Id == EStateId::Paradise && S.Timeline.Phase != EPhase::Approach); ++Guard)
		{
			FTickInput In;
			In.Gaze = Road;
			if (StateDef(M, S).Id == EStateId::Paradise) In.Commands.push_back(FCommand::Make(ECommandType::TriggerSaying));
			R = Step(M, S, In);
			Collect(R);
			S = R.State;
		}
		TestTrue(TEXT("paradise exchange (rail, rebuke, request) precedes the saying"),
			Order == std::vector<std::string>{ "paradise_rail", "paradise_rebuke", "paradise_request", "SAYING" });
	}

	// Frame chunking never changes the simulation (fixed-step accumulator).
	std::vector<FTickInput> In(2000);
	In[0].Commands.push_back(FCommand::Make(ECommandType::AcknowledgeAdvisory));
	for (size_t I = 1; I < In.size(); ++I) In[I].Gaze = (I / 200) % 2 ? Crowd : Horizon;
	const FReplayResult Ref = RunReplay(M, 11, In);
	for (const double FrameMs : { 8.333333333333334, 11.1111, 13.9, 45.0 })
	{
		FFixedStepClock Clock;
		FSessionState S = CreateSession(M, 11);
		size_t Next = 0;
		while (Next < In.size())
		{
			const int Steps = Clock.Advance(FrameMs, TickMs);
			for (int K = 0; K < Steps && Next < In.size(); ++K) S = Step(M, S, In[Next++]).State;
		}
		TestEqual(FString::Printf(TEXT("frame %.3f ms: identical final state"), FrameMs), RubiconConvert::ToFString(StateHash(M, S)), RubiconConvert::ToFString(Ref.StateHashHex));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
