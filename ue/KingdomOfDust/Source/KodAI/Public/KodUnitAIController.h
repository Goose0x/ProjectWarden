#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "KodUnitAIController.generated.h"

/**
 * Per-unit AI. Issues Move / Attack via UKodCommandSubsystem (never direct sim hacks).
 */
UCLASS(Blueprintable)
class KODAI_API AKodUnitAIController : public AAIController
{
	GENERATED_BODY()

public:
	AKodUnitAIController();

	virtual void OnPossess(APawn* InPawn) override;

	UFUNCTION(BlueprintCallable, Category = "Kod|AI")
	void IssueMoveCommand(FVector WorldLocation);

	UFUNCTION(BlueprintCallable, Category = "Kod|AI")
	void IssueAttackCommand(AActor* Target);
};
