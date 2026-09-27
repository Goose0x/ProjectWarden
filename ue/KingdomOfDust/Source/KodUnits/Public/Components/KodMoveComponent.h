#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "KodMoveComponent.generated.h"

/**
 * Order bridge into UKodSimSubsystem seek.
 *
 * LAW (Slice 0): NavMesh is FORBIDDEN as truth. Do NOT use AI MoveTo / PathFollowing /
 * CharacterMovement as match-state authority. Sim (16 Hz) owns pose; this component
 * only issues Move orders. Actors interpolate presentation from sim.
 */
UCLASS(ClassGroup = (Kod), meta = (BlueprintSpawnableComponent))
class KODUNITS_API UKodMoveComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UKodMoveComponent();

	/** Enqueue sim Move toward WorldLocation (seek + acceptance radius stop). */
	UFUNCTION(BlueprintCallable, Category = "Kod|Move")
	bool RequestMoveTo(FVector WorldLocation);

	UFUNCTION(BlueprintCallable, Category = "Kod|Move")
	void StopMovement();

	UFUNCTION(BlueprintPure, Category = "Kod|Move")
	bool IsMoveActive() const;

	/** Acceptance radius used when configuring sim entity (stop at Radius*0.5). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Move")
	float AcceptanceRadius = 50.f;
};
