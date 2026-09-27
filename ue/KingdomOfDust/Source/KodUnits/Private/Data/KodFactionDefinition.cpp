#include "Data/KodFactionDefinition.h"

#if WITH_EDITOR
void UKodFactionDefinition::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (DefinitionId.IsNone())
	{
		DefinitionId = GetFName();
	}
}
#endif
