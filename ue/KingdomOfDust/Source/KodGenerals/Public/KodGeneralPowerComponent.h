#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "KodGeneralPowerComponent.generated.h"

class UKodGeneralPowerSet;
class UAbilitySystemComponent;
class UGameplayAbility;

/** Presents / grants general powers from a power set onto an ASC. */
UCLASS(ClassGroup = (Kod), meta = (BlueprintSpawnableComponent))
class KODGENERALS_API UKodGeneralPowerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UKodGeneralPowerComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kod|Powers")
	TSoftObjectPtr<UKodGeneralPowerSet> PowerSet;

	UFUNCTION(BlueprintCallable, Category = "Kod|Powers")
	void GrantPowers(UAbilitySystemComponent* ASC);

	UFUNCTION(BlueprintCallable, Category = "Kod|Powers")
	bool TryActivatePowerByTag(UAbilitySystemComponent* ASC, FGameplayTag PowerTag);
};
