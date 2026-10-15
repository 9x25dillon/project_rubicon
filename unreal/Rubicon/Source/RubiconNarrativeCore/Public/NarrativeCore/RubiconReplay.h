// Replay and parity: golden/replay-*.json -> run -> compare. Port of src/replay.ts plus the checks the C++
// parity tests need. Used by the standalone harness (native/) and the UE automation test, so both run the
// same comparison logic.

#pragma once

#include "NarrativeCore/RubiconCoreTypes.h"
#include "NarrativeCore/RubiconJson.h"

#include <string>
#include <vector>

namespace RubiconCore
{
	bool ParseCommand(const FJson& J, FCommand& Out, std::string* OutError);
	/** Unknown gaze ids are an error (the engine must only report tagged manifest targets). */
	bool ParseTickInput(const FManifest& M, const FJson& J, FTickInput& Out, std::string* OutError);
	bool ParseSettingsPatch(const FJson& J, FSettingsPatch& Out);

	FJson CommandToJson(const FCommand& C);
	FJson TickInputToJson(const FManifest& M, const FTickInput& In);
	FJson SettingsToJson(const FSettings& S);
	FJson EventToJson(const FManifest& M, const FEvent& E);
	FJson StateToJson(const FManifest& M, const FSessionState& S);

	/** fnv1a(stableStringify(state)) and fnv1a(stableStringify(events)), identical to src/replay.ts. */
	std::string StateHash(const FManifest& M, const FSessionState& S);
	std::string EventHash(const FManifest& M, const std::vector<FEvent>& Events);

	struct FReplayResult
	{
		FSessionState State;
		std::vector<FEvent> Events;
		std::vector<double> TraceIntensity, TraceOutput, TraceGain; // sampled after every step
		std::string StateHashHex, EventHashHex;
	};

	FReplayResult RunReplay(const FManifest& M, uint32_t Seed, const std::vector<FTickInput>& Inputs, const FSettingsPatch& Settings = {});

	struct FGoldenReport
	{
		std::string Name;
		bool bPassed = false;
		bool bTimelineEventsMatch = false; // timeline.enter + saying.trigger, exact (acceptance)
		bool bAllEventsMatch = false;      // every event, exact
		bool bTraceWithinTolerance = false;
		bool bHashesMatch = false;
		double MaxIntensityError = 0, MaxOutputError = 0, MaxGainError = 0;
		size_t Ticks = 0, Events = 0;
		std::vector<std::string> Failures; // first few, human readable
	};

	/** Run one golden replay document. Tolerance applies to the Racing Thoughts trace. */
	FGoldenReport RunGolden(const FManifest& M, const FJson& Golden, double Tolerance = 1e-6);

	struct FValidatorParityReport
	{
		size_t Cases = 0;
		size_t Passed = 0;
		std::vector<std::string> Failures;
	};

	/** golden/validator-cases.json: each case must produce identical errors and warnings, in order. */
	FValidatorParityReport RunValidatorCases(const FJson& Cases);

	struct FMathParityReport
	{
		size_t ExpVectors = 0, ExpMismatches = 0;
		size_t PowVectors = 0, PowMismatches = 0;
		std::string FirstMismatch;
	};

	/** golden/math-vectors.json: Exp and Pow must equal V8's Math.exp / Math.pow bit for bit. */
	FMathParityReport RunMathVectors(const FJson& Vectors);
}
