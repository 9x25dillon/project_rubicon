// Editor validator for episode and locale assets: the src/validate.ts rules, ported message for message
// (RubiconCore::ValidateManifest), run on save and from Data Validation.

#pragma once

#include "CoreMinimal.h"
#include "EditorValidatorBase.h"
#include "RubiconEpisodeValidator.generated.h"

UCLASS()
class RUBICONEDITOR_API URubiconEpisodeValidator : public UEditorValidatorBase
{
	GENERATED_BODY()

protected:
	virtual bool CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject, FDataValidationContext& InContext) const override;
	virtual EDataValidationResult ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& Context) override;
};
