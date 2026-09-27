#include "KodDataValidation.h"
#include "Data/KodUnitDefinition.h"
#include "Data/KodBuildingDefinition.h"

bool UKodDataValidation::CanValidateAsset_Implementation(UObject* InAsset) const
{
	return InAsset && (InAsset->IsA<UKodUnitDefinition>() || InAsset->IsA<UKodBuildingDefinition>());
}

EDataValidationResult UKodDataValidation::ValidateLoadedAsset_Implementation(
	UObject* InAsset, TArray<FText>& ValidationErrors)
{
	if (UKodUnitDefinition* Unit = Cast<UKodUnitDefinition>(InAsset))
	{
		if (Unit->MaxHealth <= 0.f)
		{
			ValidationErrors.Add(NSLOCTEXT("KodEditor", "BadHealth", "MaxHealth must be > 0"));
			return EDataValidationResult::Invalid;
		}
	}
	return EDataValidationResult::Valid;
}
