#include "Data/KodResourceNodeDefinition.h"

#if WITH_EDITOR
void UKodResourceNodeDefinition::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (DefinitionId.IsNone())
	{
		DefinitionId = GetFName();
	}
}
#endif
