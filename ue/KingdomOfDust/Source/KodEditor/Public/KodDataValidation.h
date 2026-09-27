#pragma once

#include "CoreMinimal.h"
#include "EditorValidatorBase.h"
#include "KodDataValidation.generated.h"

/** Editor validator stub for Kod PrimaryDataAssets (units, buildings, powers). */
UCLASS()
class KODEDITOR_API UKodDataValidation : public UEditorValidatorBase
{
	GENERATED_BODY()

public:
	virtual bool CanValidateAsset_Implementation(UObject* InAsset) const override;
	virtual EDataValidationResult ValidateLoadedAsset_Implementation(
		UObject* InAsset, TArray<FText>& ValidationErrors) override;
};
