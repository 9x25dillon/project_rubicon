// Cross-language parity runner: the C++ narrative core against the TypeScript reference.
//   rubicon_parity <golden-dir> <content-dir>
// Exit code 0 only if every golden replay, validator case and math vector matches.

#include "NarrativeCore/RubiconContent.h"
#include "NarrativeCore/RubiconReplay.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace RubiconCore;

static bool ReadFile(const fs::path& P, std::string& Out)
{
	std::ifstream In(P, std::ios::binary);
	if (!In) return false;
	std::ostringstream Ss;
	Ss << In.rdbuf();
	Out = Ss.str();
	return true;
}

static bool LoadJson(const fs::path& P, FJson& Out)
{
	std::string Text, Err;
	if (!ReadFile(P, Text)) { std::fprintf(stderr, "cannot read %s\n", P.string().c_str()); return false; }
	if (!ParseJson(Text, Out, &Err)) { std::fprintf(stderr, "%s: %s\n", P.string().c_str(), Err.c_str()); return false; }
	return true;
}

int main(int Argc, char** Argv)
{
	const fs::path GoldenDir = Argc > 1 ? Argv[1] : "golden";
	const fs::path ContentDir = Argc > 2 ? Argv[2] : "content";

	std::string ManifestText, LocaleText;
	if (!ReadFile(ContentDir / "passion.episode.json", ManifestText) || !ReadFile(ContentDir / "locale.en.json", LocaleText))
	{
		std::fprintf(stderr, "cannot read content from %s\n", ContentDir.string().c_str());
		return 2;
	}
	FManifest M;
	FLocale L;
	std::vector<std::string> Errors, Warnings;
	if (!LoadEpisode(ManifestText, LocaleText, M, L, Errors, &Warnings))
	{
		for (const auto& E : Errors) std::fprintf(stderr, "error %s\n", E.c_str());
		return 2;
	}
	for (const auto& W : Warnings) std::printf("warn  %s\n", W.c_str());
	std::printf("manifest %s %s: %zu states, %zu gaze targets, %zu fragments, %zu sources\n", M.Id.c_str(), M.SchemaVersion.c_str(),
		M.Timeline.size(), M.GazeTargets.size(), M.Fragments.size(), M.Sources.size());

	int Failed = 0;
	std::vector<fs::path> Replays;
	if (fs::exists(GoldenDir))
		for (const auto& E : fs::directory_iterator(GoldenDir))
			if (E.path().filename().string().rfind("replay-", 0) == 0 && E.path().extension() == ".json") Replays.push_back(E.path());
	std::sort(Replays.begin(), Replays.end());
	if (Replays.empty()) { std::fprintf(stderr, "no golden/replay-*.json found: run npm run golden\n"); return 2; }

	for (const auto& P : Replays)
	{
		FJson G;
		if (!LoadJson(P, G)) { ++Failed; continue; }
		const FGoldenReport R = RunGolden(M, G, 1e-6);
		std::printf("%s %-22s %-18s ticks=%zu events=%zu timeline=%s events=%s trace=%s(max %.3g) hashes=%s\n", R.bPassed ? "PASS" : "FAIL",
			P.filename().string().c_str(), R.Name.c_str(), R.Ticks, R.Events, R.bTimelineEventsMatch ? "ok" : "MISMATCH", R.bAllEventsMatch ? "ok" : "MISMATCH",
			R.bTraceWithinTolerance ? "ok" : "MISMATCH", std::max(R.MaxIntensityError, std::max(R.MaxOutputError, R.MaxGainError)), R.bHashesMatch ? "ok" : "MISMATCH");
		for (const auto& F : R.Failures) std::printf("     %s\n", F.c_str());
		if (!R.bPassed) ++Failed;
	}

	FJson Cases;
	if (LoadJson(GoldenDir / "validator-cases.json", Cases))
	{
		const FValidatorParityReport V = RunValidatorCases(Cases);
		std::printf("%s validator-cases.json   %zu/%zu cases identical\n", V.Passed == V.Cases ? "PASS" : "FAIL", V.Passed, V.Cases);
		for (const auto& F : V.Failures) std::printf("     %s\n", F.c_str());
		if (V.Passed != V.Cases) ++Failed;
	}
	else ++Failed;

	FJson Vectors;
	if (LoadJson(GoldenDir / "math-vectors.json", Vectors))
	{
		const FMathParityReport Mr = RunMathVectors(Vectors);
		const bool bOk = Mr.ExpMismatches == 0 && Mr.PowMismatches == 0 && Mr.ExpVectors > 0 && Mr.PowVectors > 0;
		std::printf("%s math-vectors.json      exp %zu/%zu  pow %zu/%zu bit-exact\n", bOk ? "PASS" : "FAIL", Mr.ExpVectors - Mr.ExpMismatches, Mr.ExpVectors,
			Mr.PowVectors - Mr.PowMismatches, Mr.PowVectors);
		if (!Mr.FirstMismatch.empty()) std::printf("     %s\n", Mr.FirstMismatch.c_str());
		if (!bOk) ++Failed;
	}
	else ++Failed;

	std::printf(Failed ? "\nPARITY FAILED (%d)\n" : "\nPARITY OK\n", Failed);
	return Failed ? 1 : 0;
}
