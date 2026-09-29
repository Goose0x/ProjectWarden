#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "KodCommanderAIController.generated.h"

/** Skirmish commander AI stub (M2+ BT/StateTree). */
UCLASS(Blueprintable)
class KODAI_API AKodCommanderAIController : public AAIController
{
	GENERATED_BODY()

public:
	AKodCommanderAIController();

	UFUNCTION(BlueprintCallable, Category = "Kod|AI")
	void RunSkirmishLogicStub();
};
