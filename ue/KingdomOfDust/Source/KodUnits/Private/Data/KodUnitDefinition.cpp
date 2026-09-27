#include "Data/KodUnitDefinition.h"

#if WITH_EDITOR
void UKodUnitDefinition::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (DefinitionId.IsNone())
	{
		DefinitionId = GetFName();
	}

	const FName Prop = PropertyChangedEvent.GetPropertyName();
	if (Prop == GET_MEMBER_NAME_CHECKED(UKodUnitDefinition, BuildTimeSeconds))
	{
		SyncBuildTimingFromSeconds();
	}
	else if (Prop == GET_MEMBER_NAME_CHECKED(UKodUnitDefinition, BuildTicks))
	{
		SyncBuildTimingFromTicks();
	}
}
#endif
