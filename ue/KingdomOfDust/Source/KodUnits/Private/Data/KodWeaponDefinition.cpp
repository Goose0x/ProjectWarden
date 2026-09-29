#include "Data/KodWeaponDefinition.h"

#if WITH_EDITOR
void UKodWeaponDefinition::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (DefinitionId.IsNone())
	{
		DefinitionId = GetFName();
	}
}
#endif
