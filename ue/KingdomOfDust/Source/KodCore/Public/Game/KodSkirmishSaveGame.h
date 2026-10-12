#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "KodSkirmishSaveGame.generated.h"

/** One skirmish slot. The blob is the deterministic sim state. */
UCLASS()
class KODCORE_API UKodSkirmishSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<uint8> Blob;

	UPROPERTY()
	FString StateHash;

	UPROPERTY()
	int32 Slot = 1;
};
