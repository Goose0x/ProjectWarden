#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "KodCombatAttributeSet.generated.h"

#define KOD_ATTR_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS()
class KODUNITS_API UKodCombatAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UKodCombatAttributeSet();

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Combat", ReplicatedUsing = OnRep_Health)
	FGameplayAttributeData Health;
	KOD_ATTR_ACCESSORS(UKodCombatAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Combat", ReplicatedUsing = OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth;
	KOD_ATTR_ACCESSORS(UKodCombatAttributeSet, MaxHealth)

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UFUNCTION()
	virtual void OnRep_Health(const FGameplayAttributeData& OldHealth);

	UFUNCTION()
	virtual void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth);
};
