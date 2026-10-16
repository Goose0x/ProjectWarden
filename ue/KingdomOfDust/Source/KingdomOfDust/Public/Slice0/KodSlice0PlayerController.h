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
	virtual void IssueFollowToSelection(AActor* Target, bool bGuard) override;
	virtual bool ConsumeArmedOrderClick() override;
	virtual FName GetArmedOrderName() const override;
	virtual FName GetPlacementType() const override;
	virtual bool TryGetPlacementAim(FName& OutType, FVector& OutCenter) const override;
	virtual void IssueGatherToSelection_Implementation(AActor* Node, FVector NonGathererMovePoint) override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual bool ApplySelectedStructureRally(const FKodCursorCommand& Cmd) override;

	bool EnqueueTrainOnSelectedStructures();
	void CancelTrainOnSelectedStructures();
	void TryCenterControlGroup();
	virtual bool ConsumeHudCommandClick() override;
	virtual bool ConsumePlacementLeftClick() override;
	virtual bool ConsumePlacementRightClick() override;
	virtual FString GetBuildMenuHint() const override;
	virtual bool IsWorkerBuildMenuOpen() const override;
	virtual int32 GetBuildMenuTier() const override;
	virtual void HandleRightClickCommand() override;
	virtual int32 GetControlGroupCount(int32 GroupIndex) const override;
	virtual TArray<FKodEntityId> GetControlGroupIds(int32 GroupIndex) const override;
	virtual void AssignControlGroup(int32 GroupIndex) override;
	virtual void AddToControlGroup(int32 GroupIndex) override;
	virtual void IssueMinimapGroundOrder(FVector WorldLocation) override;

	bool IsActiveCommandSubgroup(FName DefinitionId) const;

	bool HasSelectedWorker() const;
	FKodEntityId FirstSelectedWorker() const;
	void UpdateBuildGhost();
	void CancelPlacement();
	bool CancelSelectedConstruction();
	bool TryResumeBuildUnderCursor();
	bool TryRepairUnderCursor();
	bool AllowTrain(FName UnitId);
	bool TryArmPower(FName PowerId);
	void TryBeginPlace(FName TypeId);
	void IssueTrain(FName UnitId, int32 StructureId);
	void IssueCancelSlot(int32 StructureId, int32 SlotIndex);
	void IssueAbility(FName AbilityId);
	void ToggleSelectedDepots();
	void CastSelectedHeal();
	void ToggleSelectedHeal();
	void LoadSelectedInfantry();
	bool TryLoadBunkerUnderCursor();

	void ArmOrder(FName OrderName);
	void IssueStopOnSelectedUnits();
	void IssueHoldOnSelectedUnits();
	bool ShiftQueued() const;

	TArray<float> LastGroupTapSeconds;
	FName ArmedOrder;
	int32 ArmedStructureId = 0;
	bool bBuildMenu = false;
	bool bAdvancedMenu = false;
	FName PlaceType;
	TWeakObjectPtr<AKodBuildGhost> BuildGhost;
};
