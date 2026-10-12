#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "KodCommandTypes.h"
#include "Sim/KodSimSubsystem.h"
#include "KodCommandSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FKodCommandEnqueuedSignature, const FKodCommand&, Command);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FKodCommandProcessedSignature, const FKodCommand&, Command);

/**
 * Central order pipeline. All gameplay orders (UI, AI, cheats) enqueue here —
 * even offline — so MP can later replicate the same stream.
 */
UCLASS()
class KODNET_API UKodCommandSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return true; }
	virtual bool IsTickableInEditor() const override { return false; }

	/** Push a command onto the pending queue. Assigns CommandId. */
	UFUNCTION(BlueprintCallable, Category = "Kod|Command")
	int32 Enqueue(FKodCommand Command);

	/** Process up to MaxCount pending commands immediately (also called from Tick). */
	UFUNCTION(BlueprintCallable, Category = "Kod|Command")
	int32 ProcessPending(int32 MaxCount = 32);

	UFUNCTION(BlueprintPure, Category = "Kod|Command")
	int32 GetPendingCount() const { return Pending.Num(); }

	UPROPERTY(BlueprintAssignable, Category = "Kod|Command")
	FKodCommandEnqueuedSignature OnCommandEnqueued;

	UPROPERTY(BlueprintAssignable, Category = "Kod|Command")
	FKodCommandProcessedSignature OnCommandProcessed;

	/** When true, Tick drains the queue automatically. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Command")
	bool bAutoProcess = true;

protected:
	void ExecuteCommand(const FKodCommand& Command);
	void ExecuteMove(const FKodCommand& Command);
	void ExecuteAttack(const FKodCommand& Command);
	void ExecuteStop(const FKodCommand& Command);
	void ExecuteBuild(const FKodCommand& Command);
	void ExecuteCastPower(const FKodCommand& Command);
	void ExecuteGather(const FKodCommand& Command);
	void ExecuteTrain(const FKodCommand& Command);
	void ExecuteCancelTrain(const FKodCommand& Command);
	void ExecuteSetRally(const FKodCommand& Command);
	void ExecutePlaceBuild(const FKodCommand& Command);
	void ExecuteResumeBuild(const FKodCommand& Command);
	void ExecuteCancelBuild(const FKodCommand& Command);
	void ExecuteUseAbility(const FKodCommand& Command);
	void ExecuteAttackMove(const FKodCommand& Command);
	void ExecuteHold(const FKodCommand& Command);
	void ExecutePatrol(const FKodCommand& Command);
	void ExecuteFollow(const FKodCommand& Command);
	void ExecuteGuard(const FKodCommand& Command);
	void IssueGroundOrders(const FKodCommand& Command, EKodSimOrderType Kind);

	UPROPERTY()
	TArray<FKodCommand> Pending;

	int32 NextCommandId = 1;
};
