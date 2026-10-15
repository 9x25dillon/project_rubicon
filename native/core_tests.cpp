// Acceptance tests for the C++ narrative core, mirroring tests/*.test.ts on the shipped C++ code.
//   rubicon_core_tests <content-dir>

#include "NarrativeCore/RubiconContent.h"
#include "NarrativeCore/RubiconFrame.h"
#include "NarrativeCore/RubiconMath.h"
#include "NarrativeCore/RubiconRacingThoughts.h"
#include "NarrativeCore/RubiconReplay.h"
#include "NarrativeCore/RubiconSession.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

using namespace RubiconCore;

static FManifest M;
static FLocale L;
static int Failures = 0, Passed = 0;

#define EXPECT(cond, ...) do { if (!(cond)) { std::printf("    expect failed: %s  ", #cond); std::printf(__VA_ARGS__); std::printf("\n"); ok = false; } } while (0)

static void Test(const char* Name, const std::function<bool()>& Body)
{
	const bool bOk = Body();
	std::printf("%s %s\n", bOk ? "ok  " : "FAIL", Name);
	(bOk ? Passed : Failures)++;
}

struct FDriver
{
	FSessionState S;
	std::vector<FEvent> Events;
	explicit FDriver(uint32_t Seed = 1, FSettingsPatch P = {}) : S(CreateSession(M, Seed, P)) {}

	FDriver& Run(int Ms, int Gaze = -1, std::vector<FCommand> Commands = {})
	{
		const int Ticks = std::max(1, Ms / TickMs);
		for (int I = 0; I < Ticks; ++I)
		{
			FTickInput In;
			In.Gaze = Gaze;
			if (I == 0) In.Commands = Commands;
			FStepResult R = Step(M, S, In);
			S = std::move(R.State);
			Events.insert(Events.end(), R.Events.begin(), R.Events.end());
		}
		return *this;
	}
	FDriver& Cmd(FCommand C) { return Run(TickMs, S.GazeTarget, { std::move(C) }); }
	FDriver& Until(EStateId Id, int Gaze = -1)
	{
		int Guard = 0;
		while (StateDef(M, S).Id != Id && Guard++ < 200000) Run(TickMs, Gaze);
		return *this;
	}
	double Out() const { return OutputIntensity(M, StateDef(M, S), S); }
	int FragmentsSince(double T0) const
	{
		int N = 0;
		for (const FEvent& E : Events) if (E.Type == EEventType::RtFragment && E.T > T0) ++N;
		return N;
	}
};

static FCommand Ack() { return FCommand::Make(ECommandType::AcknowledgeAdvisory); }
static FCommand Comfort(bool bOn) { FCommand C = FCommand::Make(ECommandType::SetComfort); C.bOn = bOn; return C; }
static FCommand Mode(EMode Md) { FCommand C = FCommand::Make(ECommandType::SetMode); C.Mode = Md; return C; }

static std::string ReadFile(const std::string& P)
{
	std::ifstream In(P, std::ios::binary);
	std::ostringstream Ss;
	Ss << In.rdbuf();
	return Ss.str();
}

int main(int Argc, char** Argv)
{
	const std::string Dir = Argc > 1 ? Argv[1] : "content";
	std::vector<std::string> Errors;
	if (!LoadEpisode(ReadFile(Dir + "/passion.episode.json"), ReadFile(Dir + "/locale.en.json"), M, L, Errors))
	{
		for (const auto& E : Errors) std::printf("error %s\n", E.c_str());
		return 2;
	}
	const int Crowd = M.FindGazeTarget("crowd");
	const int Horizon = M.FindGazeTarget("horizon");

	Test("deterministic math self-test passes in this build", []
	{
		bool ok = true;
		std::string F;
		EXPECT(SelfTestDeterministicMath(&F), "%s", F.c_str());
		return ok;
	});

	Test("gazing at the crowd raises intensity and fragment frequency versus the horizon", [&]
	{
		bool ok = true;
		FDriver A(3), B(3);
		A.Cmd(Ack()).Until(EStateId::Forgiveness, Horizon);
		B.Cmd(Ack()).Until(EStateId::Forgiveness, Horizon);
		const double Ta = A.S.T, Tb = B.S.T;
		A.Run(20000, Crowd);
		B.Run(20000, Horizon);
		EXPECT(A.Out() > B.Out() + 0.2, "crowd %f vs horizon %f", A.Out(), B.Out());
		EXPECT(A.FragmentsSince(Ta) > B.FragmentsSince(Tb), "%d vs %d", A.FragmentsSince(Ta), B.FragmentsSince(Tb));
		EXPECT(SelectFrame(M, A.S, &L).ExternalDuckDb < -3, "duck %f", SelectFrame(M, A.S, &L).ExternalDuckDb);
		return ok;
	});

	Test("looking away reduces intensity only gradually", [&]
	{
		bool ok = true;
		FDriver D(3);
		D.Cmd(Ack()).Until(EStateId::Forgiveness, Horizon).Run(20000, Crowd);
		const double Peak = D.Out();
		D.Run(1000, Horizon);
		EXPECT(D.Out() > Peak * 0.85, "after 1s %f vs peak %f", D.Out(), Peak);
		D.Run(9000, Horizon);
		EXPECT(D.Out() < Peak - 0.15, "after 10s %f vs peak %f", D.Out(), Peak);
		EXPECT(D.Out() > M.Timeline[static_cast<size_t>(EStateId::Forgiveness)].Rt.Floor * 0.9, "below floor");
		return ok;
	});

	Test("Comfort reduces output to near silence on the same tick, from every state", [&]
	{
		bool ok = true;
		for (int Si = 1; Si < TimelineLength; ++Si)
		{
			FDriver D(5);
			D.Cmd(Ack()).Until(static_cast<EStateId>(Si), Crowd).Run(3000, Crowd);
			D.Cmd(Comfort(true));
			EXPECT(D.Out() <= M.RacingThoughts.ComfortGain + 1e-12, "state %s output %f", ToString(static_cast<EStateId>(Si)), D.Out());
		}
		return ok;
	});

	Test("Witness reduces output on the same tick, from every state, then decays to silence", [&]
	{
		bool ok = true;
		for (int Si = 1; Si < TimelineLength; ++Si)
		{
			FDriver D(6);
			D.Cmd(Ack()).Until(static_cast<EStateId>(Si), Crowd).Run(3000, Crowd);
			const double Before = D.Out();
			const bool bAlreadyWitness = D.S.Mode == EMode::Witness; // forced from Release onward
			D.Cmd(Mode(EMode::Witness));
			const double Bound = bAlreadyWitness ? Before : Before * M.RacingThoughts.WitnessEchoScale;
			EXPECT(D.Out() <= Bound + 1e-9, "state %s %f -> %f", ToString(static_cast<EStateId>(Si)), Before, D.Out());
			EXPECT(D.S.Mode == EMode::Witness, "mode");
			if (StateDef(M, D.S).Id <= EStateId::Surrender)
			{
				D.Run(10000, Crowd);
				EXPECT(D.Out() < 0.01, "state %s echo after 10s %f", ToString(static_cast<EStateId>(Si)), D.Out());
			}
		}
		return ok;
	});

	Test("the timeline cannot be changed or soft-locked by any input (I1, I2, I4)", [&]
	{
		bool ok = true;
		const int Limit = static_cast<int>(MaxAutoRunMs(M)) + 2000;
		for (uint32_t Seed = 1; Seed <= 12; ++Seed)
		{
			FDriver D(Seed);
			uint32_t R = Seed * 7919u;
			auto Rand = [&R] { const FRngDraw Dr = RngNext(R); R = Dr.State; return Dr.Value; };
			D.Cmd(Ack());
			for (int T = 0; T < Limit; T += TickMs)
			{
				std::vector<FCommand> Cs;
				if (Rand() < 0.01)
				{
					const double P = Rand();
					if (P < 0.2) Cs.push_back(Mode(Rand() < 0.5 ? EMode::Witness : EMode::Embodiment));
					else if (P < 0.4) Cs.push_back(Comfort(Rand() < 0.5));
					else if (P < 0.6) { FCommand C = FCommand::Make(ECommandType::SetLayer); C.Layer = Rand() < 0.5 ? ELayer::Theological : ELayer::Historical; Cs.push_back(C); }
					else if (P < 0.8) { FCommand C = FCommand::Make(ECommandType::TriggerSaying); C.Via = ETriggerVia::Speech; Cs.push_back(C); }
					else Cs.push_back(Ack());
				}
				const int Gaze = static_cast<int>(Rand() * (M.GazeTargets.size() + 1)) - 1;
				D.Run(TickMs, Gaze, Cs);
			}
			EXPECT(StateDef(M, D.S).Id == EStateId::Contemplation, "seed %u stuck in %s", Seed, ToString(StateDef(M, D.S).Id));
			int Expect = 1;
			for (const FEvent& E : D.Events)
			{
				if (E.Type != EEventType::TimelineEnter) continue;
				EXPECT(static_cast<int>(E.State) == Expect, "seed %u entered %s out of order", Seed, ToString(E.State));
				++Expect;
			}
			EXPECT(D.S.Mode == EMode::Witness, "seed %u: witness from release onward", Seed);
		}
		return ok;
	});

	Test("frame: Luke 23:34 is shown as a textual variant with manuscript notes, voices are disclosed", [&]
	{
		bool ok = true;
		FDriver D(3);
		D.Cmd(Ack()).Until(EStateId::Forgiveness, Crowd).Run(20000, Crowd);
		const FFrameOutputs F = SelectFrame(M, D.S, &L);
		EXPECT(F.Saying.has_value(), "saying");
		bool bVariant = false, bVoice = false;
		for (const FEvidenceItem& E : F.EvidenceOverlay)
		{
			if (E.Category == EEvidence::TextualVariant && E.Citation == "Luke 23:34" && E.ManuscriptNotes.size() >= 2) bVariant = true;
			if (E.Category == EEvidence::ArtisticInterpretation && E.Citation.rfind("internal voice", 0) == 0) bVoice = true;
		}
		EXPECT(bVariant, "variant item");
		EXPECT(bVoice || F.ActiveVoices.empty(), "voice disclosure");
		EXPECT(!F.Saying->Subtitle, "no subtitle before utterance");
		D.Run(30000, Crowd);
		const FFrameOutputs F2 = SelectFrame(M, D.S, &L);
		EXPECT(F2.Saying && F2.Saying->Subtitle && *F2.Saying->Subtitle == L.at("passion.forgiveness"), "subtitle during utterance/resonance");
		return ok;
	});

	Test("exit is accepted in every state and rejects everything after it (I3)", [&]
	{
		bool ok = true;
		for (int Si = 0; Si < TimelineLength; ++Si)
		{
			FDriver D(9);
			if (Si > 0) D.Cmd(Ack()).Until(static_cast<EStateId>(Si));
			D.Cmd(FCommand::Make(ECommandType::Exit));
			EXPECT(D.S.bExited && D.Out() == 0, "state %d", Si);
			const FFrameOutputs F = SelectFrame(M, D.S, &L);
			EXPECT(!F.bControlPause && !F.bControlWitness, "controls off after exit");
			const size_t N = D.Events.size();
			D.Cmd(Comfort(true));
			EXPECT(D.Events.size() == N + 1 && D.Events.back().Type == EEventType::CommandRejected, "rejected after exit");
		}
		return ok;
	});

	Test("replay state and event hashes are deterministic (I6)", [&]
	{
		bool ok = true;
		std::vector<FTickInput> In(5000);
		In[0].Commands.push_back(Ack());
		for (size_t I = 1; I < In.size(); ++I) In[I].Gaze = static_cast<int>((I / 300) % M.GazeTargets.size());
		const FReplayResult A = RunReplay(M, 77, In), B = RunReplay(M, 77, In), C = RunReplay(M, 78, In);
		EXPECT(A.StateHashHex == B.StateHashHex && A.EventHashHex == B.EventHashHex, "same seed");
		EXPECT(A.EventHashHex != C.EventHashHex, "different seed changes fragments");
		return ok;
	});

	std::printf("\n%d passed, %d failed\n", Passed, Failures);
	return Failures ? 1 : 0;
}
