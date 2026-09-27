#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "KodGameInstance.generated.h"

/**
 * Primary game instance for Kingdom of Dust.
 * Owns long-lived match/session state beyond a single world.
 */
UCLASS(Blueprintable, Config=Game)
class KODCORE_API UKodGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	UKodGameInstance();

	virtual void Init() override;
	virtual void Shutdown() override;
};
