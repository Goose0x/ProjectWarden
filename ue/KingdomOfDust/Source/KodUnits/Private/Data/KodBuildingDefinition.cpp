#include "Data/KodBuildingDefinition.h"

#if WITH_EDITOR
void UKodBuildingDefinition::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (DefinitionId.IsNone())
	{
		DefinitionId = GetFName();
	}

	const FName Prop = PropertyChangedEvent.GetPropertyName();
	if (Prop == GET_MEMBER_NAME_CHECKED(UKodBuildingDefinition, BuildTimeSeconds))
	{
		SyncBuildTimingFromSeconds();
	}
	else if (Prop == GET_MEMBER_NAME_CHECKED(UKodBuildingDefinition, BuildTicks))
	{
		SyncBuildTimingFromTicks();
	}
}
#endif
