#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "KodAbilitySystemComponent.generated.h"

/** Thin ASC wrapper for Kod units / generals. */
UCLASS(ClassGroup = (Kod), meta = (BlueprintSpawnableComponent))
class KODUNITS_API UKodAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UKodAbilitySystemComponent();
};
