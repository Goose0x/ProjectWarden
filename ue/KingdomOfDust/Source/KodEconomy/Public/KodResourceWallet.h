#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "KodResourceWallet.generated.h"

/**
 * Player resource store (Dust Crystal primary for M1).
 * Attach to AKodPlayerState or own via PlayerState.
 */
UCLASS(ClassGroup = (Kod), meta = (BlueprintSpawnableComponent))
class KODECONOMY_API UKodResourceWallet : public UActorComponent
{
	GENERATED_BODY()

public:
	UKodResourceWallet();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_DustCrystal, Category = "Kod|Economy")
	int32 DustCrystal = 500;

	UFUNCTION(BlueprintCallable, Category = "Kod|Economy")
	bool CanAfford(int32 DustCost) const;

	UFUNCTION(BlueprintCallable, Category = "Kod|Economy")
	bool TrySpend(int32 DustCost);

	UFUNCTION(BlueprintCallable, Category = "Kod|Economy")
	void AddDust(int32 Amount);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UFUNCTION()
	void OnRep_DustCrystal();
};
