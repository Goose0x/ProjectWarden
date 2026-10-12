#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "KodAIMemorySaveGame.generated.h"

/** One opener's results across matches. Not part of the sim hash. */
USTRUCT()
struct KODCORE_API FKodOpenerRecord
{
	GENERATED_BODY()

	UPROPERTY()
	FName Opener;

	UPROPERTY()
	int32 Wins = 0;

	UPROPERTY()
	int32 Losses = 0;
};

/** Saved/SaveGames/KodAIMemory.sav. Weights the next match's build. */
UCLASS()
class KODCORE_API UKodAIMemorySaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<FKodOpenerRecord> Records;
};
