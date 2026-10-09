#pragma once

#include "CoreMinimal.h"
#include "Sim/KodEntityId.h"
#include "KodResourceTypes.generated.h"

/**
 * Slice 0 economy step 1. Match authority lives on UKodSimSubsystem.
 * Amounts are int32. The idle hash appends banks and nodes as integers — no float amounts.
 *
 * Later steps call this same API:
 *   gather worker  — HarvestNode(id, HarvestPerTrip, team)
 *   train / build  — CanAfford + Spend of an FKodResourceCost
 * Resource nodes stay out of the combat registry, so they are not attackable and do not auto-acquire.
 */

namespace KodEconomyDefaults
{
	inline constexpr int32 StartingJadeite = 50;
	inline constexpr int32 StartingOil = 0;
	inline constexpr int32 JadeiteNodeAmount = 1500;
	inline constexpr int32 OilNodeAmount = 5000;
	inline constexpr int32 JadeiteHarvestPerTrip = 5;
	inline constexpr int32 OilHarvestPerTrip = 4;
}

UENUM(BlueprintType)
enum class EKodResourceType : uint8
{
	Jadeite UMETA(DisplayName = "Jadeite"),
	Oil UMETA(DisplayName = "Oil")
};

/** Purchasing cost and the per-team bank use the same two integers. */
USTRUCT(BlueprintType)
struct KODCORE_API FKodResourceCost
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Economy")
	int32 Jadeite = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Economy")
	int32 Oil = 0;
};

/** Finite gather source. Not a combat entity. Remaining is int32 and is hashed. */
USTRUCT(BlueprintType)
struct KODCORE_API FKodResourceNodeState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Economy")
	FKodEntityId Id;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Economy")
	EKodResourceType Type = EKodResourceType::Jadeite;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Economy")
	FName DefinitionId;

	/** Quantized once at register. Nodes do not integrate. */
	UPROPERTY(BlueprintReadOnly, Category = "Kod|Economy")
	FVector Position = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Economy")
	int32 Remaining = 0;

	/** What one worker trip will take. The debug harvest command passes its own amount. */
	UPROPERTY(BlueprintReadOnly, Category = "Kod|Economy")
	int32 HarvestPerTrip = 0;
};

FORCEINLINE const TCHAR* KodResourceTypeName(EKodResourceType Type)
{
	return Type == EKodResourceType::Oil ? TEXT("Oil") : TEXT("Jadeite");
}
