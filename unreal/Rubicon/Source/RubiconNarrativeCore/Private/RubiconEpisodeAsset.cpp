#include "RubiconEpisodeAsset.h"

#include "RubiconConvert.h"
#include "NarrativeCore/RubiconContent.h"
#include "NarrativeCore/RubiconJson.h"
#include "NarrativeCore/RubiconRacingThoughts.h"

using RubiconConvert::ToFString;
using RubiconConvert::ToUtf8;

bool URubiconLocaleAsset::RebuildFromJson(TArray<FString>& OutErrors)
{
	RubiconCore::FJson Dom;
	std::string Error;
	if (!RubiconCore::ParseJson(ToUtf8(LocaleJson), Dom, &Error))
	{
		OutErrors.Add(TEXT("locale: ") + ToFString(Error));
		return false;
	}
	RubiconCore::FLocale L;
	if (!RubiconCore::BuildLocale(Dom, L, &Error))
	{
		OutErrors.Add(ToFString(Error));
		return false;
	}
	Strings.Reset();
	for (const auto& KV : L) Strings.Add(ToFString(KV.first), ToFString(KV.second));
	return true;
}

FString URubiconLocaleAsset::Lookup(const FString& Key) const
{
	const FString* V = Strings.Find(Key);
	return V ? *V : Key;
}

bool URubiconEpisodeAsset::CompileCore(RubiconCore::FManifest& OutManifest, RubiconCore::FLocale& OutLocale, TArray<FString>& OutErrors,
	TArray<FString>* OutWarnings) const
{
	if (!Locale)
	{
		OutErrors.Add(TEXT("episode has no locale asset"));
		return false;
	}
	std::vector<std::string> Errors, Warnings;
	const bool bOk = RubiconCore::LoadEpisode(ToUtf8(ManifestJson), ToUtf8(Locale->LocaleJson), OutManifest, OutLocale, Errors, &Warnings);
	for (const std::string& E : Errors) OutErrors.Add(ToFString(E));
	if (OutWarnings)
	{
		for (const std::string& W : Warnings) OutWarnings->Add(ToFString(W));
	}
	return bOk;
}

bool URubiconEpisodeAsset::RebuildViews(TArray<FString>& OutErrors)
{
	RubiconCore::FManifest M;
	RubiconCore::FLocale L;
	TArray<FString> Warnings;
	if (!CompileCore(M, L, OutErrors, &Warnings)) return false;

	EpisodeId = ToFString(M.Id);
	Title = ToFString(M.Title);
	Edition = ToFString(M.Edition);
	SchemaVersion = ToFString(M.SchemaVersion);
	ValidationWarnings = Warnings;

	Timeline.Reset();
	for (const RubiconCore::FTimelineStateDef& D : M.Timeline)
	{
		FRubiconStateInfo& S = Timeline.AddDefaulted_GetRef();
		S.State = RubiconConvert::ToUE(D.Id);
		S.Kind = ToFString(RubiconCore::ToString(D.Kind));
		S.Label = ToFString(D.Label);
		S.bAllowEmbodiment = D.bAllowEmbodiment;
		S.NominalDurationMs = RubiconCore::NominalStateDurationMs(D);
		if (D.Saying)
		{
			for (const std::string& C : D.Saying->Sources) S.SayingCitations.Add(ToFString(C));
			S.DialogueKey = ToFString(D.Saying->DialogueKey);
		}
	}

	GazeTargets.Reset();
	for (const RubiconCore::FGazeTargetDef& G : M.GazeTargets)
	{
		FRubiconGazeTargetInfo& T = GazeTargets.AddDefaulted_GetRef();
		T.Id = FName(*ToFString(G.Id));
		T.Label = ToFString(G.Label);
		T.Stress = RubiconConvert::ToUE(G.Stress);
		T.Evidence = RubiconConvert::ToUE(G.Evidence);
		T.Tag = RubiconConvert::GazeTagFor(ToFString(G.Id));
	}

	Sources.Reset();
	for (const RubiconCore::FSourceRef& Src : M.Sources)
	{
		FRubiconSourceInfo& S = Sources.AddDefaulted_GetRef();
		S.Id = ToFString(Src.Id);
		S.Citation = ToFString(Src.Citation);
		S.Evidence = RubiconConvert::ToUE(Src.Evidence);
		S.KjvText = Src.KjvText ? ToFString(*Src.KjvText) : FString();
		for (const std::string& N : Src.ManuscriptNotes) S.ManuscriptNotes.Add(ToFString(N));
	}

	Fragments.Reset();
	for (int I = 0; I < static_cast<int>(M.Fragments.size()); ++I) Fragments.Add(RubiconConvert::FragmentInfo(M, L, I));
	return true;
}

bool URubiconEpisodeAsset::FindGazeTarget(FName Id, FRubiconGazeTargetInfo& OutInfo) const
{
	for (const FRubiconGazeTargetInfo& G : GazeTargets)
	{
		if (G.Id == Id)
		{
			OutInfo = G;
			return true;
		}
	}
	return false;
}
