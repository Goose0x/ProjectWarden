#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "Sim/KodEntityId.h"
#include "KodUnit.generated.h"

class UKodUnitDefinition;
class UKodAbilitySystemComponent;
class UKodCombatAttributeSet;
class UKodMoveComponent;
class UKodAttackComponent;

/**
 * Selectable mobile combatant. Definition soft-ref drives stats at BeginPlay.
 */
UCLASS(Blueprintable)
class KODUNITS_API AKodUnit : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AKodUnit();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kod|Data")
	TSoftObjectPtr<UKodUnitDefinition> Definition;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Sim")
	FKodEntityId EntityId;

	UPROPERTY(BlueprintReadWrite, Category = "Kod|Team")
	int32 TeamId = 0;

	UFUNCTION(BlueprintCallable, Category = "Kod|Data")
	UKodUnitDefinition* GetDefinition() const;

	UFUNCTION(BlueprintCallable, Category = "Kod|Data")
	void ApplyDefinition(UKodUnitDefinition* Def);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Kod|GAS")
	TObjectPtr<UKodAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UKodCombatAttributeSet> CombatAttributes;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Kod|Components")
	TObjectPtr<UKodMoveComponent> MoveComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Kod|Components")
	TObjectPtr<UKodAttackComponent> AttackComponent;
};
