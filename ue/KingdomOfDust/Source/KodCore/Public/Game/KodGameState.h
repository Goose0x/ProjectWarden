#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "KodGameState.generated.h"

/**
 * Replicated match state (teams, clock, win conditions later).
 */
UCLASS(Blueprintable)
class KODCORE_API AKodGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AKodGameState();

	/** Match elapsed time in seconds (local for M1). */
	UPROPERTY(BlueprintReadOnly, Category = "Kod|Match")
	float MatchTimeSeconds = 0.f;

	virtual void Tick(float DeltaSeconds) override;
};
