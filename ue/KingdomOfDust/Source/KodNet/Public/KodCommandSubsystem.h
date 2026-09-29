#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "KodCommandTypes.h"
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

	UPROPERTY()
	TArray<FKodCommand> Pending;

	int32 NextCommandId = 1;
};
