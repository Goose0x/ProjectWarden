#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Sim/KodEntityId.h"
#include "Sim/KodResourceTypes.h"
#include "KodResourceNode.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UStaticMesh;
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

	/**
	 * Jadeite: three engine cones, about 135 cm tall.
	 * Oil: one engine cylinder, about 250 cm wide and low.
	 */
	void ApplyPlaceholder(EKodResourceType Type);

	void SetEntityId(FKodEntityId Id) { EntityId = Id; }

	UFUNCTION(BlueprintPure, Category = "Kod|Economy")
	FKodEntityId GetEntityId() const { return EntityId; }

	/**
	 * Actor origin. SpawnActor writes the arc point here.
	 * NodeMesh is a child so a half-height lift does not replace that world location.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Kod|Economy")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Kod|Economy")
	TObjectPtr<UStaticMeshComponent> NodeMesh;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Economy")
	EKodResourceType ResourceType = EKodResourceType::Jadeite;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Economy")
	FName DefinitionId;

protected:
	void ClearClusterMeshes();
	UStaticMeshComponent* AddClusterMesh(UStaticMesh* Shape, const FVector& RelativeLocation, const FRotator& RelativeRotation, const FVector& Scale);
	void ConfigurePlaceholderCollision(UStaticMeshComponent* Mesh) const;
	/** Returns the parameter that took the tint. None when nothing was assigned. */
	FName ApplyTint(UStaticMeshComponent* Mesh, const FLinearColor& Tint, bool bEmissive);

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Sim")
	FKodEntityId EntityId;
};
