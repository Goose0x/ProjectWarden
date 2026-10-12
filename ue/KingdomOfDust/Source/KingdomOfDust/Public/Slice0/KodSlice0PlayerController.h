#pragma once

#include "CoreMinimal.h"
#include "Game/KodPlayerController.h"
#include "Sim/KodEntityId.h"
#include "KodSlice0PlayerController.generated.h"

class AKodBuildGhost;

/**
 * L_Slice0 PC: left select/box, right move/attack through UKodCommandSubsystem.
 * Attack requires a sim entity. Non-sim actors are not retargeted to the actor pivot;
 * the base RMB path Moves to the cursor ImpactPoint instead.
 *
 * Script class path: /Script/KingdomOfDust.KodSlice0PlayerController
 */
UCLASS(Blueprintable)
class KINGDOMOFDUST_API AKodSlice0PlayerController : public AKodPlayerController
{
	GENERATED_BODY()

public:
	AKodSlice0PlayerController();

protected:
	virtual void IssueMoveToSelection_Implementation(FVector WorldLocation) override;
	virtual void IssueAttackToSelection_Implementation(AActor* Target) override;
	virtual void IssueGatherToSelection_Implementation(AActor* Node, FVector NonGathererMovePoint) override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual bool ApplySelectedStructureRally(const FKodCursorCommand& Cmd) override;

	void EnqueueTrainOnSelectedStructures();
	void CancelTrainOnSelectedStructures();
	void TryCenterControlGroup();
	virtual bool ConsumePlacementLeftClick() override;
	virtual bool ConsumePlacementRightClick() override;
	virtual FString GetBuildMenuHint() const override;
	virtual void HandleRightClickCommand() override;

	bool HasSelectedWorker() const;
	FKodEntityId FirstSelectedWorker() const;
	void UpdateBuildGhost();
	void CancelPlacement();
	bool CancelSelectedConstruction();
	bool TryResumeBuildUnderCursor();

	TArray<float> LastGroupTapSeconds;
	bool bBuildMenu = false;
	FName PlaceType;
	TWeakObjectPtr<AKodBuildGhost> BuildGhost;
};
