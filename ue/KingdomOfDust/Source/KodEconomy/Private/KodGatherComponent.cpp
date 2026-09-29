#include "KodGatherComponent.h"

UKodGatherComponent::UKodGatherComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

int32 UKodGatherComponent::GatherOnce()
{
	return GatherAmount;
}
