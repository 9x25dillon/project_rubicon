#include "RubiconFloatEnv.h"
#include "NarrativeCore/RubiconContent.h"

#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <set>

namespace RubiconCore
{
	namespace ContentDetail
	{
		// JavaScript value semantics over the DOM. A null pointer is `undefined`.

		const FJson* Get(const FJson* P, const std::string& K) { return (P && P->IsObject()) ? P->Find(K) : nullptr; }
		bool IsObj(const FJson* P) { return P && P->IsObject(); }
		bool IsArr(const FJson* P) { return P && P->IsArray(); }
		bool IsStr(const FJson* P) { return P && P->IsString() && !P->StringValue.empty(); }
		bool IsNum(const FJson* P) { return P && P->IsNumber() && std::isfinite(P->NumberValue); }
		bool In01(const FJson* P) { return IsNum(P) && P->NumberValue >= 0 && P->NumberValue <= 1; }
		bool IsInteger(const FJson* P) { return IsNum(P) && std::floor(P->NumberValue) == P->NumberValue; }

		double ToNum(const FJson* P)
		{
			if (!P) return std::nan("");
			switch (P->Type)
			{
			case FJson::EType::Null: return 0;
			case FJson::EType::Bool: return P->BoolValue ? 1 : 0;
			case FJson::EType::Number: return P->NumberValue;
			case FJson::EType::String:
			{
				const std::string& S = P->StringValue;
				size_t B = S.find_first_not_of(" \t\n\r");
				if (B == std::string::npos) return 0;
				size_t E = S.find_last_not_of(" \t\n\r");
				const std::string T = S.substr(B, E - B + 1);
				char* End = nullptr;
				const double V = std::strtod(T.c_str(), &End);
				return (End && *End == 0) ? V : std::nan("");
			}
			default: return std::nan("");
			}
		}

		bool Truthy(const FJson* P)
		{
			if (!P) return false;
			switch (P->Type)
			{
			case FJson::EType::Null: return false;
			case FJson::EType::Bool: return P->BoolValue;
			case FJson::EType::Number: return P->NumberValue != 0 && !std::isnan(P->NumberValue);
			case FJson::EType::String: return !P->StringValue.empty();
			default: return true;
			}
		}

		bool IsString(const FJson* P, const char* S) { return P && P->IsString() && P->StringValue == S; }

		/** a === b for DOM values; objects and arrays compare by identity. */
		bool StrictEq(const FJson* A, const FJson* B)
		{
			if (!A || !B) return A == B;
			if (A->Type != B->Type) return false;
			switch (A->Type)
			{
			case FJson::EType::Null: return true;
			case FJson::EType::Bool: return A->BoolValue == B->BoolValue;
			case FJson::EType::Number: return A->NumberValue == B->NumberValue;
			case FJson::EType::String: return A->StringValue == B->StringValue;
			default: return A == B;
			}
		}

		/** SameValueZero key for Set membership. */
		std::string SetKey(const FJson* P)
		{
			if (!P) return "u";
			switch (P->Type)
			{
			case FJson::EType::Null: return "z";
			case FJson::EType::Bool: return P->BoolValue ? "b1" : "b0";
			case FJson::EType::Number: return "n" + JsNumberToString(P->NumberValue);
			case FJson::EType::String: return "s" + P->StringValue;
			default: return "o" + std::to_string(reinterpret_cast<uintptr_t>(P));
			}
		}

		/** `.length` (UTF-16 units for strings); empty optional is undefined. */
		std::optional<double> Length(const FJson* P)
		{
			if (!P) return std::nullopt;
			if (P->IsArray()) return static_cast<double>(P->Items.size());
			if (P->IsString())
			{
				double N = 0;
				for (unsigned char C : P->StringValue) { if ((C & 0xc0) != 0x80) N += (C >= 0xf0) ? 2 : 1; }
				return N;
			}
			return std::nullopt;
		}

		const std::vector<FJson>& Items(const FJson* P)
		{
			static const std::vector<FJson> Empty;
			return IsArr(P) ? P->Items : Empty;
		}

		bool LocaleHas(const FLocale* L, const FJson* KeyP)
		{
			if (!L) return false;
			const auto It = L->find(JsString(KeyP));
			return It != L->end() && !It->second.empty();
		}

		template <typename E>
		bool EnumOk(const FJson* P)
		{
			E Dummy;
			return P && P->IsString() && Parse(P->StringValue, Dummy);
		}

		static const char* const TimelineIds[TimelineLength] = {
			"threshold", "elevation", "forgiveness", "paradise", "mother", "abandonment", "thirst", "completion",
			"surrender", "release", "spear", "contemplation" };
		static const char* const SayingIds[] = { "forgiveness", "paradise", "mother", "abandonment", "thirst", "completion", "surrender" };
		static const char* const StressOrder[] = { "relief", "low", "medium", "high", "extreme" };
		static const char* const VantageIds[] = { "centurion", "beloved_disciple", "mary", "women_afar" };
		static const char* const GospelIds[] = { "mark", "matthew", "luke", "john" };

		bool InTable(const FJson* P, const char* const* Table, size_t N)
		{
			if (!P || !P->IsString()) return false;
			for (size_t I = 0; I < N; ++I) if (P->StringValue == Table[I]) return true;
			return false;
		}

		/** PHASES_BY_KIND[kind].includes(phase) */
		bool PhaseInKind(const FJson* Kind, const FJson* Phase)
		{
			EStateKind K;
			EPhase Ph;
			if (!(Kind && Kind->IsString() && Parse(Kind->StringValue, K) && Phase && Phase->IsString() && Parse(Phase->StringValue, Ph))) return false;
			switch (K)
			{
			case EStateKind::Gate: return Ph == EPhase::Advisory;
			case EStateKind::Elevation: return Ph == EPhase::Raising || Ph == EPhase::Settled;
			case EStateKind::Saying: return Ph == EPhase::Approach || Ph == EPhase::Utterance || Ph == EPhase::Resonance;
			case EStateKind::Timed: return Ph == EPhase::Running;
			case EStateKind::Terminal: return Ph == EPhase::Open;
			}
			return false;
		}
	}

	FValidationResult ValidateManifest(const FJson& Raw, const FLocale* Locale)
	{
		using namespace ContentDetail;
		FValidationResult R;
		auto Err = [&R](const std::string& M) { R.Errors.push_back(M); };
		auto S = [](const FJson* P) { return JsString(P); };

		if (!Raw.IsObject())
		{
			R.bOk = false;
			R.Errors.push_back("manifest: not an object");
			return R;
		}
		const FJson* M = &Raw;

		for (const char* K : { "schemaVersion", "id", "title", "edition", "harmonizationNote" })
		{
			if (!IsStr(Get(M, K))) Err(std::string("manifest.") + K + ": required string");
		}
		for (const char* K : { "cast", "sources", "gazeTargets", "fragments", "cues", "timeline", "witnessVantages" })
		{
			if (!IsArr(Get(M, K))) Err(std::string("manifest.") + K + ": required array");
		}
		if (!R.Errors.empty()) { R.bOk = false; return R; }

		auto EvidenceOk = [](const FJson* P) { return EnumOk<EEvidence>(P); };
		auto StressOk = [](const FJson* P) { return EnumOk<EStress>(P); };

		/* sources */
		std::set<std::string> SourceIds;
		for (const FJson& Src : Items(Get(M, "sources")))
		{
			const FJson* Id = Get(&Src, "id");
			if (!IsStr(Id)) { Err("source: missing id"); continue; }
			const std::string Ctx = "source " + Id->StringValue;
			if (SourceIds.count(Id->StringValue)) Err(Ctx + ": duplicate id");
			SourceIds.insert(Id->StringValue);
			if (!IsStr(Get(&Src, "citation"))) Err(Ctx + ": missing citation");
			if (!EvidenceOk(Get(&Src, "evidence"))) Err(Ctx + ": invalid evidence \"" + S(Get(&Src, "evidence")) + "\"");
			const FJson* Mn = Get(&Src, "manuscriptNotes");
			const bool bHasNotes = Truthy(Mn) && Length(Mn).value_or(0) != 0;
			if (IsString(Get(&Src, "evidence"), "textual-variant") && !bHasNotes) Err(Ctx + ": textual-variant requires manuscriptNotes");
			const FJson* Kjv = Get(&Src, "kjv");
			if (Truthy(Kjv) && !IsStr(Get(Kjv, "text"))) Err(Ctx + ": kjv.text empty");
			const FJson* St = Get(&Src, "sourceType");
			ESourceType SourceType;
			if (!(St && St->IsString() && Parse(St->StringValue, SourceType))) Err(Ctx + ": invalid sourceType \"" + S(St) + "\"");
			else if (IsNonScriptural(SourceType) && !IsString(Get(&Src, "evidence"), "historical-inference"))
			{
				Err(Ctx + ": " + St->StringValue + " sources must be historical-inference");
			}
			else if (SourceType == ESourceType::Tradition && !IsString(Get(&Src, "evidence"), "artistic-interpretation"))
			{
				Err(Ctx + ": tradition sources must be artistic-interpretation");
			}
			const FJson* Ex = Get(&Src, "excerpt");
			if (Ex && !(IsObj(Ex) && IsStr(Get(Ex, "text")) && EnumOk<ERendering>(Get(Ex, "rendering")))) Err(Ctx + ": excerpt requires text and a valid rendering");
			if (!Truthy(Kjv) && !Truthy(Ex) && !IsStr(Get(&Src, "note"))) Err(Ctx + ": needs kjv text, an excerpt, or a note");
		}
		auto RefOk = [&](const std::string& Ctx, const FJson* Ids)
		{
			if (!IsArr(Ids)) { Err(Ctx + ": sourceIds must be an array"); return; }
			for (const FJson& Id : Ids->Items)
			{
				if (!(Id.IsString() && SourceIds.count(Id.StringValue))) Err(Ctx + ": unknown source \"" + JsString(&Id) + "\"");
			}
		};

		const FJson* Setting = Get(M, "setting");
		RefOk("setting", Get(Setting, "sourceIds"));
		{
			const auto& Notes = Items(Get(Setting, "reconstructionNotes"));
			for (size_t I = 0; I < Notes.size(); ++I)
			{
				const std::string Ctx = "setting.reconstructionNotes[" + std::to_string(I) + "]";
				if (!EvidenceOk(Get(&Notes[I], "evidence"))) Err(Ctx + ": invalid evidence");
				RefOk(Ctx, Get(&Notes[I], "sourceIds"));
			}
		}

		/* cast */
		std::set<std::string> CastIds;
		for (const FJson& C : Items(Get(M, "cast")))
		{
			const std::string Ctx = "cast " + S(Get(&C, "id"));
			const std::string Key = SetKey(Get(&C, "id"));
			if (CastIds.count(Key)) Err(Ctx + ": duplicate");
			CastIds.insert(Key);
			if (!EvidenceOk(Get(&C, "evidence"))) Err(Ctx + ": invalid evidence");
			RefOk(Ctx, Get(&C, "sources"));
		}

		/* gaze targets */
		std::set<std::string> TargetIds;
		for (const FJson& G : Items(Get(M, "gazeTargets")))
		{
			const std::string Ctx = "gazeTarget " + S(Get(&G, "id"));
			const std::string Key = SetKey(Get(&G, "id"));
			if (TargetIds.count(Key)) Err(Ctx + ": duplicate");
			TargetIds.insert(Key);
			if (!StressOk(Get(&G, "stress"))) Err(Ctx + ": invalid stress \"" + S(Get(&G, "stress")) + "\"");
			if (!EvidenceOk(Get(&G, "evidence"))) Err(Ctx + ": invalid evidence");
			RefOk(Ctx, Get(&G, "sources"));
			for (const FJson& A : Items(Get(&G, "actors")))
			{
				if (!CastIds.count(SetKey(&A))) Err(Ctx + ": unknown actor \"" + JsString(&A) + "\"");
			}
		}
		auto HasTarget = [&TargetIds](const FJson* P) { return TargetIds.count(SetKey(P)) > 0; };

		/* witness vantages */
		{
			std::set<std::string> Vantages;
			for (const FJson& V : Items(Get(M, "witnessVantages"))) Vantages.insert(SetKey(Get(&V, "id")));
			for (const char* V : VantageIds)
			{
				if (!Vantages.count(std::string("s") + V)) Err(std::string("witnessVantages: missing \"") + V + "\"");
			}
			for (const FJson& V : Items(Get(M, "witnessVantages")))
			{
				const std::string Ctx = "witnessVantage " + S(Get(&V, "id"));
				if (!CastIds.count(SetKey(Get(&V, "castId")))) Err(Ctx + ": unknown cast \"" + S(Get(&V, "castId")) + "\"");
				RefOk(Ctx, Get(&V, "sourceIds"));
			}
		}

		/* racing thoughts model */
		const FJson* Rt = Get(M, "racingThoughts");
		if (!IsObj(Rt)) Err("racingThoughts: required");
		else
		{
			const FJson* Sw = Get(Rt, "stressWeights");
			for (const char* Lvl : StressOrder)
			{
				if (!IsNum(Get(Sw, Lvl))) Err(std::string("racingThoughts.stressWeights.") + Lvl + ": required");
			}
			if (!(ToNum(Get(Sw, "relief")) < 0)) Err("racingThoughts.stressWeights.relief must be negative");
			for (int I = 2; I < 5; ++I)
			{
				if (!(ToNum(Get(Sw, StressOrder[I])) > ToNum(Get(Sw, StressOrder[I - 1]))))
				{
					Err(std::string("racingThoughts.stressWeights must increase: ") + StressOrder[I - 1] + " < " + StressOrder[I]);
				}
			}
			if (!StressOk(Get(Rt, "noTargetStress"))) Err("racingThoughts.noTargetStress invalid");
			for (const char* K : { "witnessEchoScale", "comfortGain" })
			{
				if (!In01(Get(Rt, K))) Err(std::string("racingThoughts.") + K + ": [0,1]");
			}
			if (!(ToNum(Get(Rt, "comfortGain")) <= 0.1)) Err("racingThoughts.comfortGain must be <= 0.1 (Comfort Mode is near silence)");
			if (!(ToNum(Get(Rt, "witnessEchoScale")) <= 0.5)) Err("racingThoughts.witnessEchoScale must be <= 0.5 (Witness must reduce immediately)");
			for (const char* K : { "dwellTauMs", "witnessEchoTauMs", "comfortReleaseTauMs", "maxVoices", "recentWindow" })
			{
				const FJson* V = Get(Rt, K);
				if (!(IsNum(V) && V->NumberValue > 0)) Err(std::string("racingThoughts.") + K + ": positive number");
			}
			if (!(IsInteger(Get(Rt, "maxVoices")) && IsInteger(Get(Rt, "recentWindow")))) Err("racingThoughts.maxVoices and recentWindow must be integers");
			const FJson* Fr = Get(Rt, "fragmentRate");
			const FJson* FrMin = Get(Fr, "minPerSec");
			const FJson* FrMax = Get(Fr, "maxPerSec");
			const FJson* FrGamma = Get(Fr, "gamma");
			if (!(IsObj(Fr) && IsNum(FrMin) && FrMin->NumberValue >= 0 && IsNum(FrMax) && FrMax->NumberValue >= FrMin->NumberValue && IsNum(FrGamma) && FrGamma->NumberValue > 0))
			{
				Err("racingThoughts.fragmentRate: requires 0 <= minPerSec <= maxPerSec and gamma > 0");
			}
			const FJson* Xd = Get(Rt, "externalDuck");
			const FJson* XdStart = Get(Xd, "startAt");
			const FJson* XdDb = Get(Xd, "maxDb");
			if (!(IsObj(Xd) && IsNum(XdStart) && XdStart->NumberValue >= 0 && XdStart->NumberValue < 1 && IsNum(XdDb) && XdDb->NumberValue <= 0))
			{
				Err("racingThoughts.externalDuck: requires 0 <= startAt < 1 and maxDb <= 0");
			}
			const FJson* Aff = Get(Rt, "affinityMultiplier");
			if (!(IsNum(Aff) && Aff->NumberValue >= 1)) Err("racingThoughts.affinityMultiplier: must be >= 1");
			const FJson* Tiers = Get(Rt, "dwellTiers");
			if (!(IsArr(Tiers) && !Tiers->Items.empty())) Err("racingThoughts.dwellTiers: required non-empty array");
			else
			{
				std::set<std::string> TierIds;
				for (size_t I = 0; I < Tiers->Items.size(); ++I)
				{
					const FJson* T = &Tiers->Items[I];
					const std::string Ix = std::to_string(I);
					const FJson* Tid = Get(T, "id");
					if (!IsStr(Tid)) Err("racingThoughts.dwellTiers[" + Ix + "]: id required");
					else if (TierIds.count(SetKey(Tid))) Err("racingThoughts.dwellTiers: duplicate id \"" + Tid->StringValue + "\"");
					TierIds.insert(SetKey(Tid));
					const FJson* Md = Get(T, "minDwellMs");
					if (!IsNum(Md)) Err("racingThoughts.dwellTiers[" + Ix + "].minDwellMs: number required");
					else if (I == 0 && Md->NumberValue != 0) Err("racingThoughts.dwellTiers[0].minDwellMs must be 0");
					else if (I > 0 && !(Md->NumberValue > ToNum(Get(&Tiers->Items[I - 1], "minDwellMs")))) Err("racingThoughts.dwellTiers[" + Ix + "].minDwellMs must increase");
				}
			}
			if (!IsString(Get(Rt, "evidence"), "artistic-interpretation")) Err("racingThoughts.evidence must be artistic-interpretation");
			if (!IsStr(Get(Rt, "disclosure"))) Err("racingThoughts.disclosure: required");
		}

		/* exertion */
		const FJson* Ex = Get(M, "exertion");
		if (!IsObj(Ex)) Err("exertion: required");
		else
		{
			for (const char* K : { "angularDeadzoneDegPerSec", "linearDeadzoneCmPerSec", "recoveryPerSec", "swoonRecoveryPerSec", "minSwoonMs", "sayingCost" })
			{
				const FJson* V = Get(Ex, K);
				if (!(IsNum(V) && V->NumberValue >= 0)) Err(std::string("exertion.") + K + ": number >= 0");
			}
			for (const char* K : { "angularScaleDegPerSec", "linearScaleCmPerSec", "risePerSec" })
			{
				const FJson* V = Get(Ex, K);
				if (!(IsNum(V) && V->NumberValue > 0)) Err(std::string("exertion.") + K + ": positive number");
			}
			const FJson* Vd = Get(Ex, "voiceDeadzone");
			if (!(IsNum(Vd) && Vd->NumberValue >= 0 && Vd->NumberValue < 1)) Err("exertion.voiceDeadzone: [0,1)");
			const FJson* Wa = Get(Ex, "wakeAt");
			if (!(IsNum(Wa) && Wa->NumberValue >= 0 && Wa->NumberValue < 1)) Err("exertion.wakeAt: [0,1)");
			if (!IsString(Get(Ex, "evidence"), "artistic-interpretation")) Err("exertion.evidence must be artistic-interpretation");
			if (!IsStr(Get(Ex, "disclosure"))) Err("exertion.disclosure: required");
		}

		/* timeline */
		const auto& Timeline = Items(Get(M, "timeline"));
		const auto& GazeTargets = Items(Get(M, "gazeTargets"));
		const auto& Sources = Items(Get(M, "sources"));
		if (Timeline.size() != TimelineLength)
		{
			Err("timeline: expected " + std::to_string(TimelineLength) + " states, got " + std::to_string(Timeline.size()));
		}
		for (size_t I = 0; I < Timeline.size(); ++I)
		{
			const FJson* St = &Timeline[I];
			const FJson* Id = Get(St, "id");
			const std::string Expected = I < TimelineLength ? TimelineIds[I] : "undefined";
			const bool bOrderOk = I < TimelineLength ? IsString(Id, TimelineIds[I]) : Id == nullptr; // undefined === undefined
			if (!bOrderOk)
			{
				Err("timeline[" + std::to_string(I) + "]: expected \"" + Expected + "\", got \"" + S(Id) + "\" (canonical order is immutable)");
			}
			const std::string Ctx = "state " + S(Id);
			const FJson* Kind = Get(St, "kind");
			if (!EnumOk<EStateKind>(Kind)) Err(Ctx + ": invalid kind \"" + S(Kind) + "\"");
			if (!EvidenceOk(Get(St, "evidence"))) Err(Ctx + ": invalid evidence");
			RefOk(Ctx, Get(St, "sourceIds"));

			const FJson* Rs = Get(St, "rt");
			for (const char* K : { "floor", "ceiling", "utteranceDuck", "postUtteranceFloorBoost", "entryImpulse" })
			{
				if (!In01(Get(Rs, K))) Err(Ctx + ".rt." + K + ": [0,1]");
			}
			if (Get(Rs, "ceilingEnd") && !In01(Get(Rs, "ceilingEnd"))) Err(Ctx + ".rt.ceilingEnd: [0,1]");
			if (Truthy(Rs) && ToNum(Get(Rs, "floor")) > ToNum(Get(Rs, "ceiling"))) Err(Ctx + ".rt: floor > ceiling");
			const FJson* Up = Get(Rs, "tauUpMs");
			const FJson* Down = Get(Rs, "tauDownMs");
			if (!(IsNum(Up) && Up->NumberValue > 0 && IsNum(Down) && Down->NumberValue > 0)) Err(Ctx + ".rt: taus must be positive");
			if (Truthy(Rs) && ToNum(Down) < ToNum(Up)) R.Warnings.push_back(Ctx + ".rt: tauDown < tauUp (looking away will feel fast)");

			const FJson* Ov = Get(St, "stressOverrides");
			if (IsObj(Ov))
			{
				for (const auto& F : Ov->Fields)
				{
					const FJson KeyJ = FJson::MakeString(F.first);
					if (!HasTarget(&KeyJ)) Err(Ctx + ".stressOverrides: unknown target \"" + F.first + "\"");
					if (!StressOk(&F.second)) Err(Ctx + ".stressOverrides." + F.first + ": invalid stress");
				}
			}

			const FJson* Tm = Get(St, "timing");
			auto TmN = [Tm](const char* K) { return ToNum(Get(Tm, K)); };
			const std::string KindS = (Kind && Kind->IsString()) ? Kind->StringValue : "";
			if (KindS == "elevation")
			{
				if (!(TmN("raisingMs") > 0 && TmN("settledMs") > 0)) Err(Ctx + ": elevation requires raisingMs, settledMs");
			}
			else if (KindS == "saying")
			{
				if (!(TmN("approachMinMs") >= 0 && TmN("approachAutoMs") > TmN("approachMinMs") && TmN("utteranceMs") > 0 && TmN("resonanceMs") > 0))
				{
					Err(Ctx + ": saying requires 0 <= approachMinMs < approachAutoMs, utteranceMs > 0, resonanceMs > 0");
				}
				const FJson* Sy = Get(St, "saying");
				if (!Truthy(Sy))
				{
					Err(Ctx + ": saying payload missing");
				}
				else
				{
					const FJson* SyId = Get(Sy, "id");
					if (!StrictEq(SyId, Id)) Err(Ctx + ": saying.id must equal state id");
					if (!InTable(SyId, SayingIds, 7)) Err(Ctx + ": unknown saying id");
					const auto SrcLen = Length(Get(Sy, "sources"));
					if (!(SrcLen && *SrcLen != 0)) Err(Ctx + ": saying needs at least one citation");
					RefOk(Ctx + ".saying", Get(Sy, "sourceIds"));
					if (SrcLen != Length(Get(Sy, "sourceIds"))) Err(Ctx + ": saying.sources and sourceIds length mismatch");
					const FJson* Gt = Get(Sy, "gazeTarget");
					if (!HasTarget(Gt)) Err(Ctx + ": unknown gazeTarget \"" + S(Gt) + "\"");
					const FJson* Sl = Get(Sy, "stressLevel");
					if (!StressOk(Sl)) Err(Ctx + ": invalid saying.stressLevel");
					const FJson* Effective = Get(Ov, JsString(Gt));
					if (!Effective || Effective->IsNull())
					{
						Effective = nullptr;
						for (const FJson& G : GazeTargets) if (StrictEq(Get(&G, "id"), Gt)) { Effective = Get(&G, "stress"); break; }
					}
					if (!StrictEq(Effective, Sl))
					{
						Err(Ctx + ": saying.stressLevel \"" + S(Sl) + "\" disagrees with effective target stress \"" + S(Effective) + "\"");
					}
					const FJson* SyEv = Get(Sy, "evidence");
					if (!EvidenceOk(SyEv)) Err(Ctx + ": invalid saying.evidence");
					if (!EvidenceOk(Get(Get(Sy, "pronunciation"), "evidence"))) Err(Ctx + ": invalid pronunciation.evidence");
					bool bVariantSrc = false;
					for (const FJson& SId : Items(Get(Sy, "sourceIds")))
					{
						for (const FJson& Src : Sources)
						{
							if (StrictEq(Get(&Src, "id"), &SId))
							{
								if (IsString(Get(&Src, "evidence"), "textual-variant")) bVariantSrc = true;
								break;
							}
						}
						if (bVariantSrc) break;
					}
					if (bVariantSrc && !IsString(SyEv, "textual-variant")) Err(Ctx + ": cites a textual-variant source but saying.evidence is \"" + S(SyEv) + "\"");
					if (IsString(SyEv, "textual-variant") && !IsStr(Get(Sy, "sourceNote"))) Err(Ctx + ": textual-variant saying requires sourceNote");
					if (Locale && !LocaleHas(Locale, Get(Sy, "dialogueKey"))) Err(Ctx + ": locale missing \"" + S(Get(Sy, "dialogueKey")) + "\"");
				}
			}
			else if (KindS == "timed")
			{
				if (!(TmN("runningMs") > 0)) Err(Ctx + ": timed requires runningMs");
			}
			if (KindS != "saying" && Truthy(Get(St, "saying"))) Err(Ctx + ": only saying states carry a saying");
			if (const FJson* Fat = Get(St, "fatigue"))
			{
				if (!(IsNum(Fat) && Fat->NumberValue > 0)) Err(Ctx + ".fatigue: positive number");
			}
			if (IsString(Get(St, "forcedMode"), "witness") && Truthy(Get(St, "allowEmbodiment"))) Err(Ctx + ": forcedMode witness contradicts allowEmbodiment");
		}

		/* post-death embodiment lock: John 19:33-34 spear must be witnessed, never embodied */
		const size_t SpearIdx = static_cast<size_t>(EStateId::Spear);
		for (size_t I = 0; I < Timeline.size(); ++I)
		{
			if (I >= SpearIdx && Truthy(Get(&Timeline[I], "allowEmbodiment"))) Err("state " + S(Get(&Timeline[I], "id")) + ": embodiment must be locked from spear onward");
		}
		for (const FJson& St : Timeline)
		{
			if (IsString(Get(&St, "id"), "spear"))
			{
				if (!IsString(Get(&St, "forcedMode"), "witness")) Err("state spear: forcedMode must be witness");
				break;
			}
		}
		for (const FJson& St : Timeline)
		{
			if (IsString(Get(&St, "id"), "forgiveness"))
			{
				const FJson* Fg = Get(&St, "saying");
				if (Truthy(Fg) && !IsString(Get(Fg, "evidence"), "textual-variant")) Err("forgiveness: Luke 23:34a must be marked textual-variant");
				break;
			}
		}

		/* fragments */
		std::set<std::string> FragIds;
		for (const FJson& F : Items(Get(M, "fragments")))
		{
			const std::string Ctx = "fragment " + S(Get(&F, "id"));
			const std::string Key = SetKey(Get(&F, "id"));
			if (FragIds.count(Key)) Err(Ctx + ": duplicate");
			FragIds.insert(Key);
			if (!IsString(Get(&F, "evidence"), "artistic-interpretation")) Err(Ctx + ": internal monologue must be artistic-interpretation");
			if (IsString(Get(&F, "kind"), "verbal") && !IsStr(Get(&F, "textKey"))) Err(Ctx + ": verbal requires textKey");
			if (IsString(Get(&F, "kind"), "sonic") && !IsStr(Get(&F, "cueId"))) Err(Ctx + ": sonic requires cueId");
			if (Locale && Truthy(Get(&F, "textKey")) && !LocaleHas(Locale, Get(&F, "textKey"))) Err(Ctx + ": locale missing \"" + S(Get(&F, "textKey")) + "\"");
			if (!In01(Get(&F, "minIntensity"))) Err(Ctx + ": minIntensity [0,1]");
			if (!(ToNum(Get(&F, "durationMs")) > 0 && ToNum(Get(&F, "weight")) > 0)) Err(Ctx + ": durationMs and weight must be positive");
			RefOk(Ctx, Get(&F, "anchors"));
			if (!IsString(Get(&F, "states"), "*"))
			{
				for (const FJson& St : Items(Get(&F, "states")))
				{
					if (!InTable(&St, TimelineIds, TimelineLength)) Err(Ctx + ": unknown state \"" + JsString(&St) + "\"");
				}
			}
			for (const FJson& T : Items(Get(&F, "targetAffinity")))
			{
				if (!HasTarget(&T)) Err(Ctx + ": unknown affinity target \"" + JsString(&T) + "\"");
			}
			if (const FJson* Gz = Get(&F, "gaze"))
			{
				if (!HasTarget(Get(Gz, "target"))) Err(Ctx + ": gaze.target unknown \"" + S(Get(Gz, "target")) + "\"");
				bool bTierKnown = false;
				for (const FJson& T : Items(Get(Rt, "dwellTiers"))) if (StrictEq(Get(&T, "id"), Get(Gz, "tier"))) bTierKnown = true;
				if (!bTierKnown) Err(Ctx + ": gaze.tier unknown \"" + S(Get(Gz, "tier")) + "\"");
			}
		}

		/* dialogue cues */
		std::set<std::string> CueIds;
		for (const FJson& C : Items(Get(M, "cues")))
		{
			const FJson* Id = Get(&C, "id");
			const std::string Ctx = "cue " + S(Id);
			if (CueIds.count(SetKey(Id))) Err(Ctx + ": duplicate");
			CueIds.insert(SetKey(Id));
			const FJson* Kind = Get(&C, "kind");
			if (!EnumOk<ECueKind>(Kind)) Err(Ctx + ": invalid kind \"" + S(Kind) + "\"");
			const FJson* Speaker = Get(&C, "speaker");
			if (!IsString(Speaker, "narrator") && !CastIds.count(SetKey(Speaker))) Err(Ctx + ": unknown speaker \"" + S(Speaker) + "\"");
			if ((IsString(Kind, "narration") || IsString(Kind, "telling")) && !IsString(Speaker, "narrator")) Err(Ctx + ": " + Kind->StringValue + " is spoken by the narrator");
			if (IsString(Kind, "telling"))
			{
				if (!InTable(Get(&C, "telling"), GospelIds, 4)) Err(Ctx + ": telling must name a Gospel (mark, matthew, luke, john)");
				if (!InTable(Get(&C, "vantage"), VantageIds, 4)) Err(Ctx + ": telling needs a vantage");
				if (!IsString(Get(Get(&C, "trigger"), "type"), "telling")) Err(Ctx + ": telling cues use a telling trigger");
			}
			const FJson* Ev = Get(&C, "evidence");
			if (IsString(Kind, "spoken") && !IsString(Ev, "gospel-text") && !IsString(Ev, "textual-variant"))
			{
				Err(Ctx + ": spoken cues quote scripture (evidence gospel-text or textual-variant)");
			}
			if (!EvidenceOk(Ev)) Err(Ctx + ": invalid evidence");
			const FJson* Tk = Get(&C, "textKey");
			if (!IsStr(Tk)) Err(Ctx + ": textKey required");
			else if (Locale && !LocaleHas(Locale, Tk)) Err(Ctx + ": locale missing \"" + Tk->StringValue + "\"");
			const FJson* Sids = Get(&C, "sourceIds");
			if (!(IsArr(Sids) && !Sids->Items.empty())) Err(Ctx + ": at least one source");
			RefOk(Ctx, Sids);
			const FJson* Origin = Get(&C, "origin");
			if (Origin && !HasTarget(Origin)) Err(Ctx + ": unknown origin \"" + S(Origin) + "\"");
			const FJson* Dur = Get(&C, "durationMs");
			if (!(IsNum(Dur) && Dur->NumberValue > 0)) Err(Ctx + ": durationMs must be positive");
			if (!IsString(Get(&C, "states"), "*"))
			{
				for (const FJson& St : Items(Get(&C, "states")))
				{
					if (!InTable(&St, TimelineIds, TimelineLength)) Err(Ctx + ": unknown state \"" + JsString(&St) + "\"");
				}
			}
			const FJson* Tr = Get(&C, "trigger");
			const FJson* TrType = Get(Tr, "type");
			const FJson* SayingDef = nullptr;
			if (IsString(TrType, "gaze"))
			{
				if (!HasTarget(Get(Tr, "target"))) Err(Ctx + ": unknown trigger target \"" + S(Get(Tr, "target")) + "\"");
				const FJson* Md = Get(Tr, "minDwellMs");
				if (!(IsNum(Md) && Md->NumberValue >= 0)) Err(Ctx + ": trigger.minDwellMs must be >= 0");
				if (!EnumOk<ECueMode>(Get(Tr, "mode"))) Err(Ctx + ": invalid trigger.mode \"" + S(Get(Tr, "mode")) + "\"");
			}
			else if (IsString(TrType, "telling"))
			{
				const FJson* TrState = Get(Tr, "state");
				const FJson* Def = nullptr;
				for (const FJson& St : Timeline) if (StrictEq(Get(&St, "id"), TrState)) { Def = &St; break; }
				if (!Def) Err(Ctx + ": unknown trigger state \"" + S(TrState) + "\"");
				else if (!Truthy(Get(Def, "allowEmbodiment")) || IsString(Get(Def, "kind"), "gate")) Err(Ctx + ": tellings belong to embodied states");
				if (!IsString(Kind, "telling")) Err(Ctx + ": telling trigger requires kind telling");
			}
			else if (IsString(TrType, "phase"))
			{
				const FJson* TrState = Get(Tr, "state");
				const FJson* Def = nullptr;
				for (const FJson& St : Timeline) if (StrictEq(Get(&St, "id"), TrState)) { Def = &St; break; }
				if (!Def) Err(Ctx + ": unknown trigger state \"" + S(TrState) + "\"");
				else if (!PhaseInKind(Get(Def, "kind"), Get(Tr, "phase")))
				{
					Err(Ctx + ": phase \"" + S(Get(Tr, "phase")) + "\" does not occur in state \"" + S(TrState) + "\"");
				}
				const FJson* At = Get(Tr, "atMs");
				if (!(IsNum(At) && At->NumberValue >= 0)) Err(Ctx + ": trigger.atMs must be >= 0");
				SayingDef = Def;
			}
			else
			{
				Err(Ctx + ": invalid trigger type \"" + S(TrType) + "\"");
			}
			const FJson* Before = Get(&C, "beforeSaying");
			if (Before && Before->IsBool() && Before->BoolValue)
			{
				if (!SayingDef || !IsString(Get(SayingDef, "kind"), "saying") || !IsString(Get(Tr, "phase"), "approach"))
				{
					Err(Ctx + ": beforeSaying requires a phase trigger in a saying's approach");
				}
				else
				{
					const double End = ToNum(Get(Tr, "atMs")) + ToNum(Dur);
					const FJson* MinJ = Get(Get(SayingDef, "timing"), "approachMinMs");
					const bool bHasMin = MinJ && !MinJ->IsNull();
					const double Min = bHasMin ? ToNum(MinJ) : 0;
					if (End > Min) Err(Ctx + ": beforeSaying cue must end by approachMinMs (" + JsNumberToString(End) + "ms > " + (bHasMin ? JsString(MinJ) : std::string("0")) + "ms)");
				}
			}
		}

		/* every RT-active state needs eligible fragments, or the monologue would be silent where it should not be */
		for (const FJson& St : Timeline)
		{
			if (ToNum(Get(Get(&St, "rt"), "ceiling")) > 0 && Truthy(Get(&St, "allowEmbodiment")))
			{
				int N = 0;
				for (const FJson& F : Items(Get(M, "fragments")))
				{
					const FJson* States = Get(&F, "states");
					if (IsString(States, "*")) { ++N; continue; }
					for (const FJson& X : Items(States)) if (StrictEq(&X, Get(&St, "id"))) { ++N; break; }
				}
				if (N < 3) R.Warnings.push_back("state " + S(Get(&St, "id")) + ": only " + std::to_string(N) + " eligible fragments (repetition risk)");
			}
		}

		if (Locale)
		{
			const FJson* Adv = Get(M, "advisory");
			for (const FJson* K : { Get(Adv, "titleKey"), Get(Adv, "bodyKey") })
			{
				if (!Truthy(K) || !LocaleHas(Locale, K)) Err("advisory: locale missing \"" + S(K) + "\"");
			}
		}

		R.bOk = R.Errors.empty();
		return R;
	}

	bool BuildLocale(const FJson& Raw, FLocale& Out, std::string* OutError)
	{
		Out.clear();
		if (!Raw.IsObject())
		{
			if (OutError) *OutError = "locale: not an object";
			return false;
		}
		for (const auto& F : Raw.Fields) Out[F.first] = F.second.IsString() ? F.second.StringValue : JsString(&F.second);
		return true;
	}

	namespace ContentDetail
	{
		class FBuilder
		{
		public:
			std::vector<std::string>& Errors;
			explicit FBuilder(std::vector<std::string>& InErrors) : Errors(InErrors) {}

			std::string Str(const FJson* P, const std::string& Ctx)
			{
				if (P && P->IsString()) return P->StringValue;
				Errors.push_back(Ctx + ": expected string");
				return {};
			}
			std::optional<std::string> OptStr(const FJson* P) { if (P && P->IsString()) return P->StringValue; return std::nullopt; }
			double Num(const FJson* P, const std::string& Ctx)
			{
				if (P && P->IsNumber()) return P->NumberValue;
				Errors.push_back(Ctx + ": expected number");
				return 0;
			}
			std::optional<double> OptNum(const FJson* P) { if (P && P->IsNumber()) return P->NumberValue; return std::nullopt; }
			std::vector<std::string> StrList(const FJson* P)
			{
				std::vector<std::string> Out;
				for (const FJson& E : Items(P)) if (E.IsString()) Out.push_back(E.StringValue);
				return Out;
			}
			template <typename E>
			E Enum(const FJson* P, const std::string& Ctx, E Fallback)
			{
				E V = Fallback;
				if (!(P && P->IsString() && Parse(P->StringValue, V))) Errors.push_back(Ctx + ": invalid value \"" + JsString(P) + "\"");
				return V;
			}
		};
	}

	bool BuildManifest(const FJson& Raw, FManifest& Out, std::vector<std::string>& OutErrors)
	{
		using namespace ContentDetail;
		const size_t ErrorsBefore = OutErrors.size();
		FBuilder B(OutErrors);
		Out = FManifest();
		const FJson* M = &Raw;

		Out.SchemaVersion = B.Str(Get(M, "schemaVersion"), "schemaVersion");
		Out.Id = B.Str(Get(M, "id"), "id");
		Out.Title = B.Str(Get(M, "title"), "title");
		Out.Edition = B.Str(Get(M, "edition"), "edition");
		Out.HarmonizationNote = B.Str(Get(M, "harmonizationNote"), "harmonizationNote");

		for (const FJson& C : Items(Get(M, "cast")))
		{
			FCastMember Cm;
			Cm.Id = B.Str(Get(&C, "id"), "cast.id");
			Cm.Name = B.Str(Get(&C, "name"), "cast " + Cm.Id + ".name");
			Cm.Sources = B.StrList(Get(&C, "sources"));
			Cm.Evidence = B.Enum(Get(&C, "evidence"), "cast " + Cm.Id + ".evidence", EEvidence::GospelText);
			Cm.Note = B.OptStr(Get(&C, "note"));
			Out.Cast.push_back(std::move(Cm));
		}

		for (const FJson& S : Items(Get(M, "sources")))
		{
			FSourceRef Sr;
			Sr.Id = B.Str(Get(&S, "id"), "source.id");
			Sr.Citation = B.Str(Get(&S, "citation"), "source " + Sr.Id + ".citation");
			Sr.Evidence = B.Enum(Get(&S, "evidence"), "source " + Sr.Id + ".evidence", EEvidence::GospelText);
			Sr.SourceType = B.Enum(Get(&S, "sourceType"), "source " + Sr.Id + ".sourceType", ESourceType::Gospel);
			Sr.KjvText = B.OptStr(Get(Get(&S, "kjv"), "text"));
			if (const FJson* Ex = Get(&S, "excerpt"))
			{
				FExcerpt E;
				E.Text = B.Str(Get(Ex, "text"), "source " + Sr.Id + ".excerpt.text");
				E.Rendering = B.Enum(Get(Ex, "rendering"), "source " + Sr.Id + ".excerpt.rendering", ERendering::Description);
				E.Translator = B.OptStr(Get(Ex, "translator"));
				Sr.Excerpt = E;
			}
			Sr.Debate = B.OptStr(Get(&S, "debate"));
			Sr.Greek = B.OptStr(Get(&S, "greek"));
			Sr.Note = B.OptStr(Get(&S, "note"));
			Sr.bHasManuscriptNotes = IsArr(Get(&S, "manuscriptNotes"));
			Sr.ManuscriptNotes = B.StrList(Get(&S, "manuscriptNotes"));
			Out.Sources.push_back(std::move(Sr));
		}

		for (const FJson& G : Items(Get(M, "gazeTargets")))
		{
			FGazeTargetDef Gt;
			Gt.Id = B.Str(Get(&G, "id"), "gazeTarget.id");
			Gt.Label = B.Str(Get(&G, "label"), "gazeTarget " + Gt.Id + ".label");
			Gt.Stress = B.Enum(Get(&G, "stress"), "gazeTarget " + Gt.Id + ".stress", EStress::Low);
			Gt.Actors = B.StrList(Get(&G, "actors"));
			Gt.Sources = B.StrList(Get(&G, "sources"));
			Gt.Evidence = B.Enum(Get(&G, "evidence"), "gazeTarget " + Gt.Id + ".evidence", EEvidence::GospelText);
			Gt.Note = B.OptStr(Get(&G, "note"));
			Out.GazeTargets.push_back(std::move(Gt));
		}

		for (const FJson& V : Items(Get(M, "witnessVantages")))
		{
			FWitnessVantageDef Wv;
			Wv.Id = B.Enum(Get(&V, "id"), "witnessVantage.id", EVantage::Centurion);
			Wv.CastId = B.Str(Get(&V, "castId"), "witnessVantage.castId");
			Wv.SourceIds = B.StrList(Get(&V, "sourceIds"));
			Wv.Evidence = B.Enum(Get(&V, "evidence"), "witnessVantage.evidence", EEvidence::GospelText);
			Out.WitnessVantages.push_back(std::move(Wv));
		}

		const FJson* Rt = Get(M, "racingThoughts");
		{
			FRacingThoughtsModel& Rm = Out.RacingThoughts;
			for (int I = 0; I < StressLevelCount; ++I)
			{
				Rm.StressWeights[static_cast<size_t>(I)] = B.Num(Get(Get(Rt, "stressWeights"), StressOrder[I]), std::string("racingThoughts.stressWeights.") + StressOrder[I]);
			}
			Rm.NoTargetStress = B.Enum(Get(Rt, "noTargetStress"), "racingThoughts.noTargetStress", EStress::Low);
			Rm.DwellTauMs = B.Num(Get(Rt, "dwellTauMs"), "racingThoughts.dwellTauMs");
			Rm.WitnessEchoScale = B.Num(Get(Rt, "witnessEchoScale"), "racingThoughts.witnessEchoScale");
			Rm.WitnessEchoTauMs = B.Num(Get(Rt, "witnessEchoTauMs"), "racingThoughts.witnessEchoTauMs");
			Rm.ComfortGain = B.Num(Get(Rt, "comfortGain"), "racingThoughts.comfortGain");
			Rm.ComfortReleaseTauMs = B.Num(Get(Rt, "comfortReleaseTauMs"), "racingThoughts.comfortReleaseTauMs");
			Rm.AffinityMultiplier = B.Num(Get(Rt, "affinityMultiplier"), "racingThoughts.affinityMultiplier");
			Rm.RateMinPerSec = B.Num(Get(Get(Rt, "fragmentRate"), "minPerSec"), "racingThoughts.fragmentRate.minPerSec");
			Rm.RateMaxPerSec = B.Num(Get(Get(Rt, "fragmentRate"), "maxPerSec"), "racingThoughts.fragmentRate.maxPerSec");
			Rm.RateGamma = B.Num(Get(Get(Rt, "fragmentRate"), "gamma"), "racingThoughts.fragmentRate.gamma");
			Rm.MaxVoices = static_cast<int>(B.Num(Get(Rt, "maxVoices"), "racingThoughts.maxVoices"));
			Rm.RecentWindow = static_cast<int>(B.Num(Get(Rt, "recentWindow"), "racingThoughts.recentWindow"));
			Rm.DuckStartAt = B.Num(Get(Get(Rt, "externalDuck"), "startAt"), "racingThoughts.externalDuck.startAt");
			Rm.DuckMaxDb = B.Num(Get(Get(Rt, "externalDuck"), "maxDb"), "racingThoughts.externalDuck.maxDb");
			for (const FJson& T : Items(Get(Rt, "dwellTiers")))
			{
				FDwellTier Tier;
				Tier.Id = B.Str(Get(&T, "id"), "racingThoughts.dwellTiers.id");
				Tier.MinDwellMs = B.Num(Get(&T, "minDwellMs"), "racingThoughts.dwellTiers.minDwellMs");
				Rm.DwellTiers.push_back(std::move(Tier));
			}
			if (Rm.DwellTiers.empty()) OutErrors.push_back("racingThoughts.dwellTiers: required non-empty array");
			Rm.Evidence = B.Enum(Get(Rt, "evidence"), "racingThoughts.evidence", EEvidence::ArtisticInterpretation);
			Rm.Disclosure = B.Str(Get(Rt, "disclosure"), "racingThoughts.disclosure");
		}
		{
			const FJson* Ex = Get(M, "exertion");
			FExertionModel& X = Out.Exertion;
			X.AngularDeadzoneDegPerSec = B.Num(Get(Ex, "angularDeadzoneDegPerSec"), "exertion.angularDeadzoneDegPerSec");
			X.AngularScaleDegPerSec = B.Num(Get(Ex, "angularScaleDegPerSec"), "exertion.angularScaleDegPerSec");
			X.LinearDeadzoneCmPerSec = B.Num(Get(Ex, "linearDeadzoneCmPerSec"), "exertion.linearDeadzoneCmPerSec");
			X.LinearScaleCmPerSec = B.Num(Get(Ex, "linearScaleCmPerSec"), "exertion.linearScaleCmPerSec");
			X.VoiceDeadzone = B.Num(Get(Ex, "voiceDeadzone"), "exertion.voiceDeadzone");
			X.RisePerSec = B.Num(Get(Ex, "risePerSec"), "exertion.risePerSec");
			X.RecoveryPerSec = B.Num(Get(Ex, "recoveryPerSec"), "exertion.recoveryPerSec");
			X.SwoonRecoveryPerSec = B.Num(Get(Ex, "swoonRecoveryPerSec"), "exertion.swoonRecoveryPerSec");
			X.WakeAt = B.Num(Get(Ex, "wakeAt"), "exertion.wakeAt");
			X.MinSwoonMs = B.Num(Get(Ex, "minSwoonMs"), "exertion.minSwoonMs");
			X.SayingCost = B.Num(Get(Ex, "sayingCost"), "exertion.sayingCost");
			X.Evidence = B.Enum(Get(Ex, "evidence"), "exertion.evidence", EEvidence::ArtisticInterpretation);
			X.Disclosure = B.Str(Get(Ex, "disclosure"), "exertion.disclosure");
		}

		auto ResolveSources = [&](const std::vector<std::string>& Ids, const std::string& Ctx)
		{
			std::vector<int> Out2;
			for (const std::string& Id : Ids)
			{
				const int Idx = Out.FindSource(Id);
				if (Idx < 0) OutErrors.push_back(Ctx + ": unknown source \"" + Id + "\"");
				Out2.push_back(Idx);
			}
			return Out2;
		};

		for (const FJson& F : Items(Get(M, "fragments")))
		{
			FFragment Fr;
			Fr.Id = B.Str(Get(&F, "id"), "fragment.id");
			const std::string Ctx = "fragment " + Fr.Id;
			Fr.Kind = B.Enum(Get(&F, "kind"), Ctx + ".kind", EFragmentKind::Verbal);
			Fr.Category = B.Enum(Get(&F, "category"), Ctx + ".category", EFragmentCategory::Dread);
			Fr.TextKey = B.OptStr(Get(&F, "textKey"));
			Fr.CueId = B.OptStr(Get(&F, "cueId"));
			Fr.Evidence = B.Enum(Get(&F, "evidence"), Ctx + ".evidence", EEvidence::ArtisticInterpretation);
			Fr.Anchors = B.StrList(Get(&F, "anchors"));
			Fr.AnchorIndices = ResolveSources(Fr.Anchors, Ctx);
			Fr.MinIntensity = B.Num(Get(&F, "minIntensity"), Ctx + ".minIntensity");
			Fr.DurationMs = B.Num(Get(&F, "durationMs"), Ctx + ".durationMs");
			Fr.Weight = B.Num(Get(&F, "weight"), Ctx + ".weight");
			const FJson* States = Get(&F, "states");
			if (IsString(States, "*")) Fr.bAllStates = true;
			else
			{
				for (const std::string& S : B.StrList(States))
				{
					EStateId Sid;
					if (Parse(S, Sid)) Fr.States[static_cast<size_t>(Sid)] = true;
					else OutErrors.push_back(Ctx + ": unknown state \"" + S + "\"");
				}
			}
			for (const std::string& T : B.StrList(Get(&F, "targetAffinity")))
			{
				const int Idx = Out.FindGazeTarget(T);
				if (Idx < 0) OutErrors.push_back(Ctx + ": unknown affinity target \"" + T + "\"");
				else Fr.TargetAffinity.push_back(Idx);
			}
			if (const FJson* Gz = Get(&F, "gaze"))
			{
				Fr.bHasGaze = true;
				Fr.GazeTarget = Out.FindGazeTarget(B.Str(Get(Gz, "target"), Ctx + ".gaze.target"));
				if (Fr.GazeTarget < 0) OutErrors.push_back(Ctx + ": gaze.target unknown");
				const std::string Tier = B.Str(Get(Gz, "tier"), Ctx + ".gaze.tier");
				Fr.GazeTier = -1;
				for (size_t I = 0; I < Out.RacingThoughts.DwellTiers.size(); ++I)
				{
					if (Out.RacingThoughts.DwellTiers[I].Id == Tier) Fr.GazeTier = static_cast<int>(I);
				}
				if (Fr.GazeTier < 0) OutErrors.push_back(Ctx + ": gaze.tier unknown \"" + Tier + "\"");
			}
			Out.Fragments.push_back(std::move(Fr));
		}

		for (const FJson& C : Items(Get(M, "cues")))
		{
			FCue Cu;
			Cu.Id = B.Str(Get(&C, "id"), "cue.id");
			const std::string Ctx = "cue " + Cu.Id;
			Cu.Kind = B.Enum(Get(&C, "kind"), Ctx + ".kind", ECueKind::Spoken);
			Cu.Speaker = B.Str(Get(&C, "speaker"), Ctx + ".speaker");
			Cu.TextKey = B.Str(Get(&C, "textKey"), Ctx + ".textKey");
			Cu.CueId = B.OptStr(Get(&C, "cueId"));
			if (const FJson* O = Get(&C, "origin"))
			{
				Cu.Origin = Out.FindGazeTarget(B.Str(O, Ctx + ".origin"));
				if (Cu.Origin < 0) OutErrors.push_back(Ctx + ": unknown origin");
			}
			Cu.SourceIds = B.StrList(Get(&C, "sourceIds"));
			Cu.SourceIndices = ResolveSources(Cu.SourceIds, Ctx);
			Cu.Evidence = B.Enum(Get(&C, "evidence"), Ctx + ".evidence", EEvidence::GospelText);
			const FJson* Tr = Get(&C, "trigger");
			if (IsString(Get(Tr, "type"), "gaze"))
			{
				Cu.Trigger.Type = ECueTriggerType::Gaze;
				Cu.Trigger.Target = Out.FindGazeTarget(B.Str(Get(Tr, "target"), Ctx + ".trigger.target"));
				if (Cu.Trigger.Target < 0) OutErrors.push_back(Ctx + ": unknown trigger target");
				Cu.Trigger.MinDwellMs = B.Num(Get(Tr, "minDwellMs"), Ctx + ".trigger.minDwellMs");
				Cu.Trigger.Mode = B.Enum(Get(Tr, "mode"), Ctx + ".trigger.mode", ECueMode::Any);
			}
			else if (IsString(Get(Tr, "type"), "telling"))
			{
				Cu.Trigger.Type = ECueTriggerType::Telling;
				Cu.Trigger.State = B.Enum(Get(Tr, "state"), Ctx + ".trigger.state", EStateId::Elevation);
			}
			else
			{
				Cu.Trigger.Type = ECueTriggerType::Phase;
				Cu.Trigger.State = B.Enum(Get(Tr, "state"), Ctx + ".trigger.state", EStateId::Threshold);
				Cu.Trigger.Phase = B.Enum(Get(Tr, "phase"), Ctx + ".trigger.phase", EPhase::Advisory);
				Cu.Trigger.AtMs = B.Num(Get(Tr, "atMs"), Ctx + ".trigger.atMs");
			}
			const FJson* States = Get(&C, "states");
			if (IsString(States, "*")) Cu.bAllStates = true;
			else
			{
				for (const std::string& St : B.StrList(States))
				{
					EStateId Sid;
					if (Parse(St, Sid)) Cu.States[static_cast<size_t>(Sid)] = true;
					else OutErrors.push_back(Ctx + ": unknown state \"" + St + "\"");
				}
			}
			Cu.DurationMs = B.Num(Get(&C, "durationMs"), Ctx + ".durationMs");
			const FJson* Before = Get(&C, "beforeSaying");
			Cu.bBeforeSaying = Before && Before->IsBool() && Before->BoolValue;
			Cu.Note = B.OptStr(Get(&C, "note"));
			if (const FJson* Tg = Get(&C, "telling")) Cu.Telling = B.Enum(Tg, Ctx + ".telling", EGospel::Mark);
			if (const FJson* Va = Get(&C, "vantage")) Cu.Vantage = B.Enum(Va, Ctx + ".vantage", EVantage::BelovedDisciple);
			Out.Cues.push_back(std::move(Cu));
		}

		const auto& Timeline = Items(Get(M, "timeline"));
		if (Timeline.size() != TimelineLength) OutErrors.push_back("timeline: expected " + std::to_string(TimelineLength) + " states");
		for (size_t I = 0; I < Timeline.size(); ++I)
		{
			const FJson* St = &Timeline[I];
			FTimelineStateDef D;
			D.Id = B.Enum(Get(St, "id"), "timeline[" + std::to_string(I) + "].id", EStateId::Threshold);
			if (static_cast<size_t>(D.Id) != I) OutErrors.push_back("timeline[" + std::to_string(I) + "]: out of canonical order");
			const std::string Ctx = std::string("state ") + ToString(D.Id);
			D.Kind = B.Enum(Get(St, "kind"), Ctx + ".kind", EStateKind::Gate);
			D.Label = B.Str(Get(St, "label"), Ctx + ".label");
			D.SourceIds = B.StrList(Get(St, "sourceIds"));
			D.SourceIndices = ResolveSources(D.SourceIds, Ctx);
			D.Evidence = B.Enum(Get(St, "evidence"), Ctx + ".evidence", EEvidence::GospelText);
			const FJson* Tm = Get(St, "timing");
			D.Timing.RaisingMs = B.OptNum(Get(Tm, "raisingMs"));
			D.Timing.SettledMs = B.OptNum(Get(Tm, "settledMs"));
			D.Timing.ApproachMinMs = B.OptNum(Get(Tm, "approachMinMs"));
			D.Timing.ApproachAutoMs = B.OptNum(Get(Tm, "approachAutoMs"));
			D.Timing.UtteranceMs = B.OptNum(Get(Tm, "utteranceMs"));
			D.Timing.ResonanceMs = B.OptNum(Get(Tm, "resonanceMs"));
			D.Timing.RunningMs = B.OptNum(Get(Tm, "runningMs"));
			const FJson* Rs = Get(St, "rt");
			D.Rt.Floor = B.Num(Get(Rs, "floor"), Ctx + ".rt.floor");
			D.Rt.Ceiling = B.Num(Get(Rs, "ceiling"), Ctx + ".rt.ceiling");
			D.Rt.CeilingEnd = B.OptNum(Get(Rs, "ceilingEnd"));
			D.Rt.TauUpMs = B.Num(Get(Rs, "tauUpMs"), Ctx + ".rt.tauUpMs");
			D.Rt.TauDownMs = B.Num(Get(Rs, "tauDownMs"), Ctx + ".rt.tauDownMs");
			D.Rt.UtteranceDuck = B.Num(Get(Rs, "utteranceDuck"), Ctx + ".rt.utteranceDuck");
			D.Rt.PostUtteranceFloorBoost = B.Num(Get(Rs, "postUtteranceFloorBoost"), Ctx + ".rt.postUtteranceFloorBoost");
			D.Rt.EntryImpulse = B.Num(Get(Rs, "entryImpulse"), Ctx + ".rt.entryImpulse");
			D.StressOverride.assign(Out.GazeTargets.size(), static_cast<int8_t>(-1));
			if (const FJson* Ov = Get(St, "stressOverrides"))
			{
				for (const auto& F : Ov->Fields)
				{
					const int Idx = Out.FindGazeTarget(F.first);
					const EStress Lvl = B.Enum(&F.second, Ctx + ".stressOverrides." + F.first, EStress::Low);
					if (Idx < 0) OutErrors.push_back(Ctx + ".stressOverrides: unknown target \"" + F.first + "\"");
					else D.StressOverride[static_cast<size_t>(Idx)] = static_cast<int8_t>(Lvl);
				}
			}
			const FJson* Ae = Get(St, "allowEmbodiment");
			D.bAllowEmbodiment = Ae && Ae->IsBool() && Ae->BoolValue;
			if (const FJson* Fm = Get(St, "forcedMode")) D.ForcedMode = B.Enum(Fm, Ctx + ".forcedMode", EMode::Witness);
			if (const FJson* Fv = Get(St, "forcedVantage")) D.ForcedVantage = B.Enum(Fv, Ctx + ".forcedVantage", EVantage::BelovedDisciple);
			if (const FJson* At = Get(St, "atmosphere"))
			{
				FAtmosphereKey A;
				A.SunIlluminanceLux = B.Num(Get(At, "sunIlluminanceLux"), Ctx + ".atmosphere.sunIlluminanceLux");
				A.SkyCctK = B.Num(Get(At, "skyCctK"), Ctx + ".atmosphere.skyCctK");
				A.AerosolOpticalDepth = B.Num(Get(At, "aerosolOpticalDepth"), Ctx + ".atmosphere.aerosolOpticalDepth");
				A.Evidence = B.Enum(Get(At, "evidence"), Ctx + ".atmosphere.evidence", EEvidence::ArtisticInterpretation);
				A.Note = B.OptStr(Get(At, "note"));
				D.Atmosphere = A;
			}
			if (const FJson* Sy = Get(St, "saying"))
			{
				FSaying Sg;
				Sg.Id = B.Enum(Get(Sy, "id"), Ctx + ".saying.id", EStateId::Forgiveness);
				Sg.Theme = B.Str(Get(Sy, "theme"), Ctx + ".saying.theme");
				Sg.Sources = B.StrList(Get(Sy, "sources"));
				Sg.SourceIds = B.StrList(Get(Sy, "sourceIds"));
				Sg.SourceIndices = ResolveSources(Sg.SourceIds, Ctx + ".saying");
				Sg.Evidence = B.Enum(Get(Sy, "evidence"), Ctx + ".saying.evidence", EEvidence::GospelText);
				Sg.DialogueKey = B.Str(Get(Sy, "dialogueKey"), Ctx + ".saying.dialogueKey");
				Sg.GazeTarget = B.Str(Get(Sy, "gazeTarget"), Ctx + ".saying.gazeTarget");
				Sg.GazeTargetIndex = Out.FindGazeTarget(Sg.GazeTarget);
				if (Sg.GazeTargetIndex < 0) OutErrors.push_back(Ctx + ".saying: unknown gazeTarget \"" + Sg.GazeTarget + "\"");
				Sg.StressLevel = B.Enum(Get(Sy, "stressLevel"), Ctx + ".saying.stressLevel", EStress::Low);
				Sg.HistoricalCue = B.Str(Get(Sy, "historicalCue"), Ctx + ".saying.historicalCue");
				Sg.TheologicalCue = B.Str(Get(Sy, "theologicalCue"), Ctx + ".saying.theologicalCue");
				Sg.SourceNote = B.OptStr(Get(Sy, "sourceNote"));
				Sg.PsychologicalNote = B.Str(Get(Sy, "psychologicalNote"), Ctx + ".saying.psychologicalNote");
				const FJson* Pr = Get(Sy, "pronunciation");
				Sg.Pronunciation.Greek = B.OptStr(Get(Pr, "greek"));
				Sg.Pronunciation.GreekTranslit = B.OptStr(Get(Pr, "greekTranslit"));
				Sg.Pronunciation.AramaicReconstruction = B.OptStr(Get(Pr, "aramaicReconstruction"));
				Sg.Pronunciation.AramaicTranslit = B.OptStr(Get(Pr, "aramaicTranslit"));
				Sg.Pronunciation.Evidence = B.Enum(Get(Pr, "evidence"), Ctx + ".saying.pronunciation.evidence", EEvidence::HistoricalInference);
				Sg.Pronunciation.Note = B.OptStr(Get(Pr, "note"));
				Sg.Attestation = B.Str(Get(Sy, "attestation"), Ctx + ".saying.attestation");
				D.Saying = std::move(Sg);
			}
			D.Notes = B.StrList(Get(St, "notes"));
			if (const FJson* Fat = Get(St, "fatigue")) D.Fatigue = B.Num(Fat, Ctx + ".fatigue");
			Out.Timeline.push_back(std::move(D));
		}

		const FJson* Adv = Get(M, "advisory");
		Out.AdvisoryTitleKey = B.Str(Get(Adv, "titleKey"), "advisory.titleKey");
		Out.AdvisoryBodyKey = B.Str(Get(Adv, "bodyKey"), "advisory.bodyKey");

		return OutErrors.size() == ErrorsBefore;
	}

	bool LoadEpisode(const std::string& ManifestJson, const std::string& LocaleJson, FManifest& OutManifest,
		FLocale& OutLocale, std::vector<std::string>& OutErrors, std::vector<std::string>* OutWarnings)
	{
		FJson LocaleDom, ManifestDom;
		std::string ParseError;
		if (!ParseJson(LocaleJson, LocaleDom, &ParseError)) { OutErrors.push_back("locale: " + ParseError); return false; }
		if (!BuildLocale(LocaleDom, OutLocale, &ParseError)) { OutErrors.push_back(ParseError); return false; }
		if (!ParseJson(ManifestJson, ManifestDom, &ParseError)) { OutErrors.push_back("manifest: " + ParseError); return false; }
		FValidationResult V = ValidateManifest(ManifestDom, &OutLocale);
		if (OutWarnings) OutWarnings->insert(OutWarnings->end(), V.Warnings.begin(), V.Warnings.end());
		if (!V.bOk)
		{
			OutErrors.insert(OutErrors.end(), V.Errors.begin(), V.Errors.end());
			return false;
		}
		return BuildManifest(ManifestDom, OutManifest, OutErrors);
	}
}
