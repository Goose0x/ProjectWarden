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
class USkeletalMesh;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Selectable mobile combatant. Definition soft-ref drives stats at BeginPlay.
 * Body mesh is presentation: definition static mesh when it loads, otherwise the Engine cube.
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
	/** Skeletal mesh wins when it loads; else static mesh on UnitMesh; else the Engine cube. */
	void ApplyBodyMesh(const UKodUnitDefinition* Def);
	void MountResolvedSkeletalMesh(USkeletalMesh* SkelMesh);
	void MountResolvedStaticMesh(UStaticMesh* StaticBody);
	void MountCubePlaceholder();

	/** QueryOnly Pawn on UnitMesh. Same contract as the constructor — does not touch the capsule. */
	void KeepSelectCollision();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Kod|GAS")
	TObjectPtr<UKodAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UKodCombatAttributeSet> CombatAttributes;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Kod|Components")
	TObjectPtr<UKodMoveComponent> MoveComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Kod|Components")
	TObjectPtr<UKodAttackComponent> AttackComponent;

	/**
	 * Static mesh child. Slice 0 swaps in UKodUnitDefinition::StaticMesh when that soft path loads.
	 * Constructor plants /Engine/BasicShapes/Cube at 0.8×0.8×1.7 when nothing resolves.
	 * Collision stays QueryOnly Pawn (Visibility ignored via the Pawn profile). Capsule is not retuned.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Kod|Mesh")
	TObjectPtr<UStaticMeshComponent> UnitMesh;
};
