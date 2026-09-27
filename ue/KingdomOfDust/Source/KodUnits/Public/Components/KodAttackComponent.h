#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "KodAttackComponent.generated.h"

/**
 * Hitscan attack bridge into UKodSimSubsystem.
 * No actor bullets. Damage / range / cooldown come from weapon DataAsset via ConfigureEntity.
 */
UCLASS(ClassGroup = (Kod), meta = (BlueprintSpawnableComponent))
class KODUNITS_API UKodAttackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UKodAttackComponent();

	UFUNCTION(BlueprintCallable, Category = "Kod|Attack")
	bool RequestAttack(AActor* Target);

	UFUNCTION(BlueprintCallable, Category = "Kod|Attack")
	void ClearTarget();

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Attack")
	TWeakObjectPtr<AActor> CurrentTarget;
};
