#include "RubiconFloatEnv.h"
#include "NarrativeCore/RubiconReplay.h"
#include "NarrativeCore/RubiconContent.h"
#include "NarrativeCore/RubiconMath.h"
#include "NarrativeCore/RubiconRacingThoughts.h"
#include "NarrativeCore/RubiconSession.h"

#include <cmath>
#include <cstring>

namespace RubiconCore
{
	namespace ReplayDetail
	{
		FJson Str(const std::string& S) { return FJson::MakeString(S); }
		FJson Num(double V) { return FJson::MakeNumber(V); }
		FJson Bool(bool V) { return FJson::MakeBool(V); }

		template <typename E>
		bool ReadEnum(const FJson& Obj, const char* Key, E& Out)
		{
			const FJson* V = Obj.Find(Key);
			return V && V->IsString() && Parse(V->StringValue, Out);
		}
	}

	bool ParseSettingsPatch(const FJson& J, FSettingsPatch& Out)
	{
		Out = FSettingsPatch();
		if (!J.IsObject()) return false;
		if (const FJson* V = J.Find("intensityCeiling")) { if (!V->IsNumber()) return false; Out.IntensityCeiling = V->NumberValue; }
		if (const FJson* V = J.Find("reducedEffects")) { if (!V->IsBool()) return false; Out.ReducedEffects = V->BoolValue; }
		if (const FJson* V = J.Find("seated")) { if (!V->IsBool()) return false; Out.Seated = V->BoolValue; }
		if (const FJson* V = J.Find("hapticsEnabled")) { if (!V->IsBool()) return false; Out.HapticsEnabled = V->BoolValue; }
		return true;
	}

	bool ParseCommand(const FJson& J, FCommand& Out, std::string* OutError)
	{
		using namespace ReplayDetail;
		Out = FCommand();
		auto Fail = [OutError](const std::string& M) { if (OutError) *OutError = M; return false; };
		if (!J.IsObject() || !ReadEnum(J, "type", Out.Type)) return Fail("command: missing or unknown type");
		switch (Out.Type)
		{
		case ECommandType::TriggerSaying:
			if (!ReadEnum(J, "via", Out.Via) || Out.Via == ETriggerVia::Auto) return Fail("triggerSaying: via must be select or speech");
			return true;
		case ECommandType::SetMode:
		{
			if (!ReadEnum(J, "mode", Out.Mode)) return Fail("setMode: bad mode");
			if (J.Find("vantage"))
			{
				EVantage V;
				if (!ReadEnum(J, "vantage", V)) return Fail("setMode: bad vantage");
				Out.Vantage = V;
			}
			return true;
		}
		case ECommandType::SetLayer:
			if (!ReadEnum(J, "layer", Out.Layer)) return Fail("setLayer: bad layer");
			return true;
		case ECommandType::SetComfort:
		{
			const FJson* On = J.Find("on");
			if (!On || !On->IsBool()) return Fail("setComfort: bad on");
			Out.bOn = On->BoolValue;
			return true;
		}
		case ECommandType::OpenEvidence:
		{
			const FJson* Id = J.Find("sourceId");
			if (!Id || !Id->IsString()) return Fail("openEvidence: bad sourceId");
			Out.SourceId = Id->StringValue;
			return true;
		}
		case ECommandType::SetSettings:
		{
			const FJson* S = J.Find("settings");
			if (!S || !ParseSettingsPatch(*S, Out.Settings)) return Fail("setSettings: bad settings");
			return true;
		}
		default:
			return true;
		}
	}

	bool ParseTickInput(const FManifest& M, const FJson& J, FTickInput& Out, std::string* OutError)
	{
		Out = FTickInput();
		if (!J.IsObject()) { if (OutError) *OutError = "input: not an object"; return false; }
		const FJson* Gaze = J.Find("gaze");
		if (Gaze && Gaze->IsString())
		{
			Out.Gaze = M.FindGazeTarget(Gaze->StringValue);
			if (Out.Gaze < 0) { if (OutError) *OutError = "input: unknown gaze target " + Gaze->StringValue; return false; }
		}
		else if (Gaze && !Gaze->IsNull()) { if (OutError) *OutError = "input: bad gaze"; return false; }
		if (const FJson* B = J.Find("breath"))
		{
			if (!B->IsNumber()) { if (OutError) *OutError = "input: bad breath"; return false; }
			Out.Breath = B->NumberValue;
		}
		if (const FJson* Cs = J.Find("commands"))
		{
			if (!Cs->IsArray()) { if (OutError) *OutError = "input: commands must be an array"; return false; }
			for (const FJson& C : Cs->Items)
			{
				FCommand Cmd;
				if (!ParseCommand(C, Cmd, OutError)) return false;
				Out.Commands.push_back(std::move(Cmd));
			}
		}
		return true;
	}

	FJson SettingsToJson(const FSettings& S)
	{
		using namespace ReplayDetail;
		FJson J = FJson::MakeObject();
		J.Set("intensityCeiling", Num(S.IntensityCeiling));
		J.Set("reducedEffects", Bool(S.bReducedEffects));
		J.Set("seated", Bool(S.bSeated));
		J.Set("hapticsEnabled", Bool(S.bHapticsEnabled));
		J.Set("subtitles", Bool(true));
		return J;
	}

	FJson CommandToJson(const FCommand& C)
	{
		using namespace ReplayDetail;
		FJson J = FJson::MakeObject();
		J.Set("type", Str(ToString(C.Type)));
		switch (C.Type)
		{
		case ECommandType::TriggerSaying: J.Set("via", Str(ToString(C.Via))); break;
		case ECommandType::SetMode:
			J.Set("mode", Str(ToString(C.Mode)));
			if (C.Vantage) J.Set("vantage", Str(ToString(*C.Vantage)));
			break;
		case ECommandType::SetLayer: J.Set("layer", Str(ToString(C.Layer))); break;
		case ECommandType::SetComfort: J.Set("on", Bool(C.bOn)); break;
		case ECommandType::OpenEvidence: J.Set("sourceId", Str(C.SourceId)); break;
		case ECommandType::SetSettings:
		{
			FJson P = FJson::MakeObject();
			if (C.Settings.IntensityCeiling) P.Set("intensityCeiling", Num(*C.Settings.IntensityCeiling));
			if (C.Settings.ReducedEffects) P.Set("reducedEffects", Bool(*C.Settings.ReducedEffects));
			if (C.Settings.Seated) P.Set("seated", Bool(*C.Settings.Seated));
			if (C.Settings.HapticsEnabled) P.Set("hapticsEnabled", Bool(*C.Settings.HapticsEnabled));
			J.Set("settings", std::move(P));
			break;
		}
		default: break;
		}
		return J;
	}

	FJson TickInputToJson(const FManifest& M, const FTickInput& In)
	{
		using namespace ReplayDetail;
		FJson J = FJson::MakeObject();
		J.Set("gaze", In.Gaze >= 0 ? Str(M.GazeTargets[static_cast<size_t>(In.Gaze)].Id) : FJson::MakeNull());
		if (In.Breath) J.Set("breath", Num(*In.Breath));
		if (!In.Commands.empty())
		{
			FJson Cs = FJson::MakeArray();
			for (const FCommand& C : In.Commands) Cs.Push(CommandToJson(C));
			J.Set("commands", std::move(Cs));
		}
		return J;
	}

	FJson EventToJson(const FManifest& M, const FEvent& E)
	{
		using namespace ReplayDetail;
		FJson J = FJson::MakeObject();
		J.Set("t", Num(E.T));
		J.Set("type", Str(ToString(E.Type)));
		switch (E.Type)
		{
		case EEventType::TimelineEnter: J.Set("state", Str(ToString(E.State))); break;
		case EEventType::TimelinePhase: J.Set("state", Str(ToString(E.State))); J.Set("phase", Str(ToString(E.Phase))); break;
		case EEventType::SayingTrigger: J.Set("saying", Str(ToString(E.Saying))); J.Set("via", Str(ToString(E.Via))); break;
		case EEventType::ModeChange:
			J.Set("mode", Str(ToString(E.Mode)));
			J.Set("vantage", Str(ToString(E.Vantage)));
			J.Set("forced", Bool(E.bForced));
			break;
		case EEventType::LayerChange: J.Set("layer", Str(ToString(E.Layer))); break;
		case EEventType::ComfortChange: J.Set("on", Bool(E.bOn)); break;
		case EEventType::PauseChange: J.Set("paused", Bool(E.bPaused)); break;
		case EEventType::Exit: break;
		case EEventType::EvidenceChange:
			J.Set("sourceId", E.SourceIndex >= 0 ? Str(M.Sources[static_cast<size_t>(E.SourceIndex)].Id) : FJson::MakeNull());
			break;
		case EEventType::SettingsChange: J.Set("settings", SettingsToJson(E.Settings)); break;
		case EEventType::RtFragment:
			J.Set("fragmentId", Str(M.Fragments[static_cast<size_t>(E.Fragment)].Id));
			J.Set("kind", Str(ToString(E.FragmentKind)));
			J.Set("voice", Num(E.Voice));
			break;
		case EEventType::CommandRejected: J.Set("command", Str(ToString(E.Command))); J.Set("reason", Str(E.Reason)); break;
		}
		return J;
	}

	FJson StateToJson(const FManifest& M, const FSessionState& S)
	{
		using namespace ReplayDetail;
		FJson J = FJson::MakeObject();
		J.Set("t", Num(S.T));
		J.Set("seed", Num(S.Seed));
		J.Set("rng", Num(S.Rng));
		FJson Tl = FJson::MakeObject();
		Tl.Set("index", Num(S.Timeline.Index));
		Tl.Set("phase", Str(ToString(S.Timeline.Phase)));
		Tl.Set("phaseElapsedMs", Num(S.Timeline.PhaseElapsedMs));
		Tl.Set("stateElapsedMs", Num(S.Timeline.StateElapsedMs));
		Tl.Set("triggeredVia", S.Timeline.TriggeredVia ? Str(ToString(*S.Timeline.TriggeredVia)) : FJson::MakeNull());
		Tl.Set("attendedTargetMs", Num(S.Timeline.AttendedTargetMs));
		J.Set("timeline", std::move(Tl));
		J.Set("mode", Str(ToString(S.Mode)));
		J.Set("vantage", Str(ToString(S.Vantage)));
		J.Set("layer", Str(ToString(S.Layer)));
		J.Set("comfort", Bool(S.bComfort));
		J.Set("paused", Bool(S.bPaused));
		J.Set("exited", Bool(S.bExited));
		J.Set("evidenceOpen", S.EvidenceOpen >= 0 ? Str(M.Sources[static_cast<size_t>(S.EvidenceOpen)].Id) : FJson::MakeNull());
		J.Set("settings", SettingsToJson(S.Settings));
		FJson Gaze = FJson::MakeObject();
		Gaze.Set("target", S.GazeTarget >= 0 ? Str(M.GazeTargets[static_cast<size_t>(S.GazeTarget)].Id) : FJson::MakeNull());
		Gaze.Set("dwellMs", Num(S.GazeDwellMs));
		J.Set("gaze", std::move(Gaze));
		J.Set("breath", Num(S.Breath));
		FJson Rt = FJson::MakeObject();
		Rt.Set("intensity", Num(S.Rt.Intensity));
		Rt.Set("gain", Num(S.Rt.Gain));
		Rt.Set("floorBoost", Num(S.Rt.FloorBoost));
		Rt.Set("spawnAccumulator", Num(S.Rt.SpawnAccumulator));
		FJson Voices = FJson::MakeArray();
		for (const FVoice& V : S.Rt.Voices)
		{
			FJson Vj = FJson::MakeObject();
			Vj.Set("fragmentId", Str(M.Fragments[static_cast<size_t>(V.Fragment)].Id));
			Vj.Set("startedAt", Num(V.StartedAt));
			Vj.Set("endsAt", Num(V.EndsAt));
			Voices.Push(std::move(Vj));
		}
		Rt.Set("voices", std::move(Voices));
		FJson Recent = FJson::MakeArray();
		for (int F : S.Rt.Recent) Recent.Push(Str(M.Fragments[static_cast<size_t>(F)].Id));
		Rt.Set("recent", std::move(Recent));
		J.Set("rt", std::move(Rt));
		return J;
	}

	std::string StateHash(const FManifest& M, const FSessionState& S) { return Fnv1a(StableStringify(StateToJson(M, S))); }

	std::string EventHash(const FManifest& M, const std::vector<FEvent>& Events)
	{
		FJson A = FJson::MakeArray();
		for (const FEvent& E : Events) A.Push(EventToJson(M, E));
		return Fnv1a(StableStringify(A));
	}

	FReplayResult RunReplay(const FManifest& M, uint32_t Seed, const std::vector<FTickInput>& Inputs, const FSettingsPatch& Settings)
	{
		FReplayResult R;
		R.State = CreateSession(M, Seed, Settings);
		R.TraceIntensity.reserve(Inputs.size());
		R.TraceOutput.reserve(Inputs.size());
		R.TraceGain.reserve(Inputs.size());
		for (const FTickInput& In : Inputs)
		{
			FStepResult S = Step(M, R.State, In, TickMs);
			R.State = std::move(S.State);
			R.Events.insert(R.Events.end(), S.Events.begin(), S.Events.end());
			R.TraceIntensity.push_back(R.State.Rt.Intensity);
			R.TraceOutput.push_back(OutputIntensity(M, StateDef(M, R.State), R.State));
			R.TraceGain.push_back(R.State.Rt.Gain);
		}
		R.StateHashHex = StateHash(M, R.State);
		R.EventHashHex = EventHash(M, R.Events);
		return R;
	}

	FGoldenReport RunGolden(const FManifest& M, const FJson& Golden, double Tolerance)
	{
		FGoldenReport Rep;
		auto Fail = [&Rep](const std::string& Msg) { if (Rep.Failures.size() < 12) Rep.Failures.push_back(Msg); };
		const FJson* Seed = Golden.Find("seed");
		const FJson* Inputs = Golden.Find("inputs");
		const FJson* Expected = Golden.Find("expected");
		const FJson* Profile = Golden.Find("profile");
		Rep.Name = "seed " + JsString(Seed) + (Profile ? " (" + JsString(Profile) + ")" : "");
		if (!Seed || !Seed->IsNumber() || !Inputs || !Inputs->IsArray() || !Expected || !Expected->IsObject())
		{
			Fail("golden: missing seed, inputs or expected");
			return Rep;
		}
		if (const FJson* Mid = Golden.Find("manifestId"))
		{
			if (!Mid->IsString() || Mid->StringValue != M.Id) Fail("golden: manifestId " + JsString(Mid) + " != " + M.Id + " (regenerate with npm run golden)");
		}
		if (const FJson* Sv = Golden.Find("schemaVersion"))
		{
			if (!Sv->IsString() || Sv->StringValue != M.SchemaVersion) Fail("golden: schemaVersion " + JsString(Sv) + " != " + M.SchemaVersion);
		}
		if (const FJson* Tm = Golden.Find("tickMs"))
		{
			if (!Tm->IsNumber() || Tm->NumberValue != TickMs) Fail("golden: tickMs " + JsString(Tm) + " != " + std::to_string(TickMs));
		}

		FSettingsPatch Settings;
		if (const FJson* Sj = Golden.Find("settings")) ParseSettingsPatch(*Sj, Settings);

		std::vector<FTickInput> Ticks;
		Ticks.reserve(Inputs->Items.size());
		for (size_t I = 0; I < Inputs->Items.size(); ++I)
		{
			FTickInput In;
			std::string Err;
			if (!ParseTickInput(M, Inputs->Items[I], In, &Err)) { Fail("input " + std::to_string(I) + ": " + Err); return Rep; }
			Ticks.push_back(std::move(In));
		}
		Rep.Ticks = Ticks.size();

		const FReplayResult R = RunReplay(M, static_cast<uint32_t>(Seed->NumberValue), Ticks, Settings);
		Rep.Events = R.Events.size();

		/* events */
		const FJson* ExpEvents = Expected->Find("events");
		std::vector<std::string> Exp, Got, ExpTl, GotTl;
		if (ExpEvents && ExpEvents->IsArray())
		{
			for (const FJson& E : ExpEvents->Items)
			{
				Exp.push_back(StableStringify(E));
				const FJson* Ty = E.Find("type");
				if (Ty && Ty->IsString() && (Ty->StringValue == "timeline.enter" || Ty->StringValue == "saying.trigger")) ExpTl.push_back(Exp.back());
			}
		}
		for (const FEvent& E : R.Events)
		{
			Got.push_back(StableStringify(EventToJson(M, E)));
			if (E.Type == EEventType::TimelineEnter || E.Type == EEventType::SayingTrigger) GotTl.push_back(Got.back());
		}
		Rep.bTimelineEventsMatch = ExpTl == GotTl && !ExpTl.empty();
		if (!Rep.bTimelineEventsMatch) Fail("timeline.enter/saying.trigger: expected " + std::to_string(ExpTl.size()) + ", got " + std::to_string(GotTl.size()));
		Rep.bAllEventsMatch = Exp == Got;
		if (!Rep.bAllEventsMatch)
		{
			const size_t N = std::min(Exp.size(), Got.size());
			size_t I = 0;
			while (I < N && Exp[I] == Got[I]) ++I;
			Fail("events diverge at #" + std::to_string(I) + ": expected " + (I < Exp.size() ? Exp[I] : "<end>") + " got " + (I < Got.size() ? Got[I] : "<end>"));
		}

		/* trace */
		const FJson* Trace = Expected->Find("trace");
		Rep.bTraceWithinTolerance = Trace && Trace->IsObject();
		if (!Rep.bTraceWithinTolerance) Fail("golden: expected.trace missing (regenerate with npm run golden)");
		auto CheckTrace = [&](const char* Key, const std::vector<double>& Ours, double& MaxErr)
		{
			const FJson* A = Trace ? Trace->Find(Key) : nullptr;
			if (!A || !A->IsArray() || A->Items.size() != Ours.size())
			{
				Rep.bTraceWithinTolerance = false;
				Fail(std::string("trace.") + Key + ": length mismatch");
				return;
			}
			bool bReported = false;
			for (size_t I = 0; I < Ours.size(); ++I)
			{
				const double E = std::fabs(A->Items[I].NumberValue - Ours[I]);
				if (E > MaxErr) MaxErr = E;
				if (!(E <= Tolerance))
				{
					Rep.bTraceWithinTolerance = false;
					if (!bReported) { Fail(std::string("trace.") + Key + " tick " + std::to_string(I) + ": expected " + JsNumberToString(A->Items[I].NumberValue) + " got " + JsNumberToString(Ours[I])); bReported = true; }
				}
			}
		};
		CheckTrace("intensity", R.TraceIntensity, Rep.MaxIntensityError);
		CheckTrace("output", R.TraceOutput, Rep.MaxOutputError);
		CheckTrace("gain", R.TraceGain, Rep.MaxGainError);

		/* hashes: bit-level parity of the entire final state and event log */
		const FJson* Sh = Expected->Find("stateHash");
		const FJson* Eh = Expected->Find("eventHash");
		Rep.bHashesMatch = Sh && Eh && Sh->IsString() && Eh->IsString() && Sh->StringValue == R.StateHashHex && Eh->StringValue == R.EventHashHex;
		if (!Rep.bHashesMatch) Fail("hashes: expected state " + JsString(Sh) + " events " + JsString(Eh) + ", got state " + R.StateHashHex + " events " + R.EventHashHex);

		Rep.bPassed = Rep.bTimelineEventsMatch && Rep.bAllEventsMatch && Rep.bTraceWithinTolerance && Rep.bHashesMatch && Rep.Failures.empty();
		return Rep;
	}

	FValidatorParityReport RunValidatorCases(const FJson& Cases)
	{
		FValidatorParityReport Rep;
		for (const FJson& C : Cases.Items)
		{
			++Rep.Cases;
			const std::string Name = JsString(C.Find("name"));
			FLocale Locale;
			const FJson* Lj = C.Find("locale");
			const bool bHasLocale = Lj && Lj->IsObject();
			if (bHasLocale) BuildLocale(*Lj, Locale, nullptr);
			static const FJson Undefined;
			const FJson* Mj = C.Find("manifest");
			const FValidationResult V = ValidateManifest(Mj ? *Mj : Undefined, bHasLocale ? &Locale : nullptr);
			auto ToList = [](const FJson* A)
			{
				std::vector<std::string> Out;
				if (A && A->IsArray()) for (const FJson& E : A->Items) Out.push_back(E.StringValue);
				return Out;
			};
			const auto ExpErrors = ToList(C.Find("errors"));
			const auto ExpWarnings = ToList(C.Find("warnings"));
			if (V.Errors == ExpErrors && V.Warnings == ExpWarnings) { ++Rep.Passed; continue; }
			auto FirstDiff = [](const std::vector<std::string>& A, const std::vector<std::string>& B)
			{
				size_t I = 0;
				while (I < A.size() && I < B.size() && A[I] == B[I]) ++I;
				return "#" + std::to_string(I) + " expected [" + (I < A.size() ? A[I] : "<none>") + "] got [" + (I < B.size() ? B[I] : "<none>") + "]";
			};
			if (V.Errors != ExpErrors) Rep.Failures.push_back(Name + ": errors " + FirstDiff(ExpErrors, V.Errors));
			if (V.Warnings != ExpWarnings) Rep.Failures.push_back(Name + ": warnings " + FirstDiff(ExpWarnings, V.Warnings));
		}
		return Rep;
	}

	FMathParityReport RunMathVectors(const FJson& Vectors)
	{
		FMathParityReport Rep;
		auto Same = [](double A, double B) { return std::memcmp(&A, &B, sizeof(double)) == 0 || (std::isnan(A) && std::isnan(B)); };
		if (const FJson* E = Vectors.Find("exp"))
		{
			for (const FJson& V : E->Items)
			{
				if (V.Items.size() != 2) continue;
				++Rep.ExpVectors;
				const double Got = Exp(V.Items[0].NumberValue);
				if (!Same(Got, V.Items[1].NumberValue))
				{
					if (Rep.ExpMismatches++ == 0 && Rep.FirstMismatch.empty())
						Rep.FirstMismatch = "exp(" + JsNumberToString(V.Items[0].NumberValue) + "): expected " + JsNumberToString(V.Items[1].NumberValue) + " got " + JsNumberToString(Got);
				}
			}
		}
		if (const FJson* P = Vectors.Find("pow"))
		{
			for (const FJson& V : P->Items)
			{
				if (V.Items.size() != 3) continue;
				++Rep.PowVectors;
				const double Got = Pow(V.Items[0].NumberValue, V.Items[1].NumberValue);
				if (!Same(Got, V.Items[2].NumberValue))
				{
					if (Rep.PowMismatches++ == 0 && Rep.FirstMismatch.empty())
						Rep.FirstMismatch = "pow(" + JsNumberToString(V.Items[0].NumberValue) + ", " + JsNumberToString(V.Items[1].NumberValue) + "): expected " + JsNumberToString(V.Items[2].NumberValue) + " got " + JsNumberToString(Got);
				}
			}
		}
		return Rep;
	}
}
