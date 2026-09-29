#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "KodPlayerState.generated.h"

class UKodResourceWallet;

/**
 * Per-player match state. Holds economy wallet soft ownership for M1.
 */
UCLASS(Blueprintable)
class KODCORE_API AKodPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	AKodPlayerState();

	/** Team index (0-based). */
	UPROPERTY(BlueprintReadWrite, Replicated, Category = "Kod|Team")
	int32 TeamId = 0;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
