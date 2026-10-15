#include "RubiconEpisodeValidator.h"

#include "RubiconConvert.h"
#include "RubiconEpisodeAsset.h"
#include "NarrativeCore/RubiconContent.h"
#include "NarrativeCore/RubiconJson.h"

#define LOCTEXT_NAMESPACE "RubiconEpisodeValidator"

bool URubiconEpisodeValidator::CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject, FDataValidationContext& InContext) const
{
	return InObject && (InObject->IsA<URubiconEpisodeAsset>() || InObject->IsA<URubiconLocaleAsset>());
}

EDataValidationResult URubiconEpisodeValidator::ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& Context)
{
	if (const URubiconLocaleAsset* LocaleAsset = Cast<URubiconLocaleAsset>(InAsset))
	{
		RubiconCore::FJson Dom;
		std::string Error;
		if (!RubiconCore::ParseJson(RubiconConvert::ToUtf8(LocaleAsset->LocaleJson), Dom, &Error) || !Dom.IsObject())
		{
			AssetFails(InAsset, FText::Format(LOCTEXT("LocaleJson", "Locale JSON is invalid: {0}"), FText::FromString(RubiconConvert::ToFString(Error))));
			return GetValidationResult();
		}
		AssetPasses(InAsset);
		return GetValidationResult();
	}

	const URubiconEpisodeAsset* Episode = Cast<URubiconEpisodeAsset>(InAsset);
	if (!Episode)
	{
		AssetPasses(InAsset);
		return GetValidationResult();
	}
	if (!Episode->Locale)
	{
		AssetFails(InAsset, LOCTEXT("NoLocale", "Episode has no locale asset; re-run Rubicon.ImportEpisode."));
		return GetValidationResult();
	}

	RubiconCore::FJson ManifestDom, LocaleDom;
	std::string Error;
	if (!RubiconCore::ParseJson(RubiconConvert::ToUtf8(Episode->ManifestJson), ManifestDom, &Error))
	{
		AssetFails(InAsset, FText::Format(LOCTEXT("ManifestJson", "Manifest JSON is invalid: {0}"), FText::FromString(RubiconConvert::ToFString(Error))));
		return GetValidationResult();
	}
	RubiconCore::FLocale Locale;
	if (!RubiconCore::ParseJson(RubiconConvert::ToUtf8(Episode->Locale->LocaleJson), LocaleDom, &Error) || !RubiconCore::BuildLocale(LocaleDom, Locale, &Error))
	{
		AssetFails(InAsset, FText::Format(LOCTEXT("EpisodeLocale", "Locale JSON is invalid: {0}"), FText::FromString(RubiconConvert::ToFString(Error))));
		return GetValidationResult();
	}

	const RubiconCore::FValidationResult R = RubiconCore::ValidateManifest(ManifestDom, &Locale);
	for (const std::string& W : R.Warnings) AssetWarning(InAsset, FText::FromString(RubiconConvert::ToFString(W)));
	for (const std::string& E : R.Errors) AssetFails(InAsset, FText::FromString(RubiconConvert::ToFString(E)));
	if (R.bOk)
	{
		// Also prove the runtime can compile what the validator accepted.
		RubiconCore::FManifest M;
		std::vector<std::string> BuildErrors;
		if (!RubiconCore::BuildManifest(ManifestDom, M, BuildErrors))
		{
			for (const std::string& E : BuildErrors) AssetFails(InAsset, FText::FromString(RubiconConvert::ToFString(E)));
		}
		else
		{
			AssetPasses(InAsset);
		}
	}
	return GetValidationResult();
}

#undef LOCTEXT_NAMESPACE
