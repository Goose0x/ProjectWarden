#include "Slice0/KodDataCatalog.h"
#include "Engine/DataAsset.h"

void UKodDataCatalog::RegisterAsset(FName DefinitionId, UPrimaryDataAsset* Asset)
{
	if (DefinitionId.IsNone() || !Asset)
	{
		return;
	}
	Assets.Add(DefinitionId, Asset);
}

UPrimaryDataAsset* UKodDataCatalog::FindAsset(FName DefinitionId) const
{
	if (const TObjectPtr<UPrimaryDataAsset>* Found = Assets.Find(DefinitionId))
	{
		return Found->Get();
	}
	return nullptr;
}
