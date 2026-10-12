#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

/** Playtest #1 population ceiling. Current supply and SupplyCap cannot exceed this. */
inline constexpr int32 KodSupplyHardCap = 200;

#include "KodResourceWallet.generated.h"

/**
 * Retired as match authority. Cash and Oil mirror the sim bank for team 0
 * (Jadeite and Luminene). The sim supply cap comes from registered structures.
 * SetSupplyCount only clamps these mirrored fields.
 */
UCLASS(ClassGroup = (Kod), meta = (BlueprintSpawnableComponent))
class KODECONOMY_API UKodResourceWallet : public UActorComponent
{
	GENERATED_BODY()

public:
	UKodResourceWallet();

	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Cash, Category = "Kod|Economy")
	int32 Cash = 500;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Oil, Category = "Kod|Economy")
	int32 Oil = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Supply, Category = "Kod|Economy")
	int32 Supply = 0;

	/** Matches KodSupplyHardCap. ClampSupply enforces the ceiling. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Supply, Category = "Kod|Economy")
	int32 SupplyCap = 200;

	UFUNCTION(BlueprintCallable, Category = "Kod|Economy")
	bool CanAfford(int32 CashCost) const;

	/** Spends Cash only. Oil is not spent here. */
	UFUNCTION(BlueprintCallable, Category = "Kod|Economy")
	bool TrySpend(int32 CashCost);

	UFUNCTION(BlueprintCallable, Category = "Kod|Economy")
	void AddCash(int32 Amount);

	/** Stores Oil. Does not start a gather loop. */
	UFUNCTION(BlueprintCallable, Category = "Kod|Economy")
	void AddOil(int32 Amount);

	/** Clamps current supply to SupplyCap, and SupplyCap to KodSupplyHardCap. */
	UFUNCTION(BlueprintCallable, Category = "Kod|Economy")
	void SetSupplyCount(int32 Current, int32 Cap);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UFUNCTION()
	void OnRep_Cash();

	UFUNCTION()
	void OnRep_Oil();

	UFUNCTION()
	void OnRep_Supply();

	void ClampSupply();
	void MirrorFromSim();
};
