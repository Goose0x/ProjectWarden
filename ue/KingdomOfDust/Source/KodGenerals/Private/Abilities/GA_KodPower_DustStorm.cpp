#include "Abilities/GA_KodPower_DustStorm.h"
#include "Tags/KodGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"

UGA_KodPower_DustStorm::UGA_KodPower_DustStorm()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	FGameplayTagContainer DustTags;
	DustTags.AddTag(KodGameplayTags::Kod_Power_DustStorm);
	SetAssetTags(DustTags);
}

void UGA_KodPower_DustStorm::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// M1 stub: resolve target location from event data / player aim; apply GE / Niagara later.
	(void)TriggerEventData;
	(void)Radius;
	(void)DurationSeconds;

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UGA_KodPower_DustStorm::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
