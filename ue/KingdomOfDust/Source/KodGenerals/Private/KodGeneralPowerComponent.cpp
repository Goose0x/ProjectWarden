#include "KodGeneralPowerComponent.h"
#include "KodGeneralPowerSet.h"
#include "AbilitySystemComponent.h"
#include "GameplayAbilitySpec.h"

UKodGeneralPowerComponent::UKodGeneralPowerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UKodGeneralPowerComponent::GrantPowers(UAbilitySystemComponent* ASC)
{
	if (!ASC)
	{
		return;
	}
	UKodGeneralPowerSet* Set = PowerSet.LoadSynchronous();
	if (!Set)
	{
		return;
	}
	for (const FKodPowerEntry& Entry : Set->Powers)
	{
		if (Entry.AbilityClass)
		{
			FGameplayAbilitySpec Spec(Entry.AbilityClass, 1, INDEX_NONE, GetOwner());
			ASC->GiveAbility(Spec);
		}
	}
}

bool UKodGeneralPowerComponent::TryActivatePowerByTag(UAbilitySystemComponent* ASC, FGameplayTag PowerTag)
{
	if (!ASC || !PowerTag.IsValid())
	{
		return false;
	}
	// M1: activate by matching granted ability AssetTags — refine with tag query later.
	return ASC->TryActivateAbilitiesByTag(FGameplayTagContainer(PowerTag));
}
