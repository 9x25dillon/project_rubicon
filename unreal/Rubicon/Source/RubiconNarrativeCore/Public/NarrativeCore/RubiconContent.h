// Content layer: manifest validation (port of src/validate.ts) and JSON -> FManifest compilation.
//
// ValidateManifest runs on the raw JSON DOM so it can report exactly what src/validate.ts reports, message
// for message (checked by native/ against golden/validator-cases.json). BuildManifest then compiles a valid
// DOM into the interned runtime manifest. LoadEpisode does both and is what every caller should use.

#pragma once

#include "NarrativeCore/RubiconCoreTypes.h"
#include "NarrativeCore/RubiconJson.h"

#include <string>
#include <vector>

namespace RubiconCore
{
	struct FValidationResult
	{
		bool bOk = true;
		std::vector<std::string> Errors;
		std::vector<std::string> Warnings;
	};

	/** Port of validateManifest(raw, locale). Pass nullptr for no locale. */
	FValidationResult ValidateManifest(const FJson& Raw, const FLocale* Locale);

	/** Locale JSON (flat object of strings) -> map. */
	bool BuildLocale(const FJson& Raw, FLocale& Out, std::string* OutError);

	/** Compile a validated manifest DOM. Returns false (with reasons) on anything it cannot represent. */
	bool BuildManifest(const FJson& Raw, FManifest& Out, std::vector<std::string>& OutErrors);

	/** Parse, validate and compile. Errors include JSON syntax errors and every validator error. */
	bool LoadEpisode(const std::string& ManifestJson, const std::string& LocaleJson, FManifest& OutManifest,
		FLocale& OutLocale, std::vector<std::string>& OutErrors, std::vector<std::string>* OutWarnings = nullptr);
}
