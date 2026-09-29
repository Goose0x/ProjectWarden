#include "Attributes/KodCombatAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"

UKodCombatAttributeSet::UKodCombatAttributeSet()
{
	// Editor / construction defaults only — runtime MUST ApplyDefinition from DataAsset.
	InitHealth(100.f);
	InitMaxHealth(100.f);
}

void UKodCombatAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(UKodCombatAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UKodCombatAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
}

void UKodCombatAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UKodCombatAttributeSet, Health, OldHealth);
}

void UKodCombatAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UKodCombatAttributeSet, MaxHealth, OldMaxHealth);
}
