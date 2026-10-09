#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Sim/KodEntityId.h"
#include "Sim/KodResourceTypes.h"
#include "KodResourceNode.generated.h"

class UStaticMeshComponent;
class UKodResourceNodeDefinition;

/**
 * Presentation for a sim resource node. Does not register itself.
 * AKodSlice0GameMode registers after the smoke Marine and the hostile so those
 * combat ids stay 1 and 2. Not a character: no movement, no weapon, no auto-acquire.
 */
UCLASS(Blueprintable)
class KODUNITS_API AKodResourceNode : public AActor
{
	GENERATED_BODY()

public:
	AKodResourceNode();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Definition mesh when it loads, otherwise the engine-shape placeholder. */
	void ApplyDefinition(const UKodResourceNodeDefinition* Def);

	/** Cone (Jadeite) or squat cylinder (Oil) on BasicShapeMaterial. */
	void ApplyPlaceholder(EKodResourceType Type);

	void SetEntityId(FKodEntityId Id) { EntityId = Id; }

	UFUNCTION(BlueprintPure, Category = "Kod|Economy")
	FKodEntityId GetEntityId() const { return EntityId; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Kod|Economy")
	TObjectPtr<UStaticMeshComponent> NodeMesh;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Economy")
	EKodResourceType ResourceType = EKodResourceType::Jadeite;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Economy")
	FName DefinitionId;

protected:
	void SitMeshOnGround();
	void ApplyTint(const FLinearColor& Tint);

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Sim")
	FKodEntityId EntityId;
};
