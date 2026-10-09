#pragma once

#include "CoreMinimal.h"
#include "Sim/KodEntityId.h"
#include "KodResourceTypes.generated.h"

/**
 * Slice 0 economy. Match authority lives on UKodSimSubsystem.
 * Amounts are int32. The idle hash appends banks, nodes, drop-offs, and gather state as integers.
 *
 * Jadeite is the internal money id. The HUD labels it Credits.
 * Luminene is the gas pulled from fissure vents (the old Oil name).
 *
 *   gather worker  — sim gather loop, then Deposit of the carried ints
 *   train / build  — CanAfford + Spend of an FKodResourceCost
 *   debug          — HarvestNode(id, amount, team) still deposits immediately
 * Resource nodes and drop-offs stay out of the combat registry.
 */

namespace KodEconomyDefaults
{
	inline constexpr int32 StartingJadeite = 50;
	inline constexpr int32 StartingLuminene = 0;
	inline constexpr int32 JadeiteNodeAmount = 1500;
	inline constexpr int32 LumineneNodeAmount = 2250;
	inline constexpr int32 JadeiteHarvestPerTrip = 5;
	inline constexpr int32 LumineneHarvestPerTrip = 4;
	/** 1.5 s at SimHz 16. Both trips use this unless the DataAsset overrides it. */
	inline constexpr int32 JadeiteHarvestTicks = 24;
	inline constexpr int32 LumineneHarvestTicks = 24;
	/** Depleted vent keeps yielding this much per trip. Jadeite clusters do not. */
	inline constexpr int32 LumineneTricklePerTrip = 1;
	inline constexpr int32 JadeiteGatherStandUU = 150;
	inline constexpr int32 LumineneGatherStandUU = 200;
	inline constexpr int32 DropOffStandUU = 360;
	inline constexpr int32 WorkerSpawnCount = 3;
	/** ~3 s at 16 Hz with no pose change before a gather leg retargets, then aborts. */
	inline constexpr int32 GatherStallTicks = 48;
}

/** sRGB hex colours for presentation. Not sim state. */
namespace KodResourceColors
{
	/** #00A86B deep jade. */
	inline FLinearColor Jadeite()
	{
		return FLinearColor(FColor(0x00, 0xA8, 0x6B));
	}

	/** #9EE60B neon yellow-green vent core. */
	inline FLinearColor LumineneCore()
	{
		return FLinearColor(FColor(0x9E, 0xE6, 0x0B));
	}

	/** #C6FF1A vent highlight. */
	inline FLinearColor LumineneHighlight()
	{
		return FLinearColor(FColor(0xC6, 0xFF, 0x1A));
	}
}

UENUM(BlueprintType)
enum class EKodResourceType : uint8
{
	Jadeite UMETA(DisplayName = "Jadeite"),
	Luminene UMETA(DisplayName = "Luminene")
};

UENUM(BlueprintType)
enum class EKodGatherPhase : uint8
{
	None,
	ToNode,
	Harvest,
	ToDropOff
};

/** Purchasing cost and the per-team bank use the same two integers. */
USTRUCT(BlueprintType)
struct KODCORE_API FKodResourceCost
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Economy")
	int32 Jadeite = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Economy")
	int32 Luminene = 0;
};

/** Authored numbers copied onto the sim node at register. */
USTRUCT(BlueprintType)
struct KODCORE_API FKodResourceNodeSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Economy")
	EKodResourceType Type = EKodResourceType::Jadeite;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Economy")
	int32 Amount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Economy")
	int32 HarvestPerTrip = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Economy")
	int32 HarvestTicks = KodEconomyDefaults::JadeiteHarvestTicks;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Economy")
	int32 TricklePerTrip = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Economy")
	int32 GatherStandUU = KodEconomyDefaults::JadeiteGatherStandUU;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Economy")
	FName DefinitionId;
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

	/** What one worker trip will take while Remaining > 0. Debug harvest can pass its own amount. */
	UPROPERTY(BlueprintReadOnly, Category = "Kod|Economy")
	int32 HarvestPerTrip = 0;

	/** Sim ticks to stand and mine one trip. Integer. Hashed only via the worker's countdown. */
	UPROPERTY(BlueprintReadOnly, Category = "Kod|Economy")
	int32 HarvestTicks = KodEconomyDefaults::JadeiteHarvestTicks;

	/** Yield per trip after Remaining hits 0. Zero means the node is destroyed. */
	UPROPERTY(BlueprintReadOnly, Category = "Kod|Economy")
	int32 TricklePerTrip = 0;

	/** Stop this many uu from the node origin so the worker stands outside the mesh. */
	UPROPERTY(BlueprintReadOnly, Category = "Kod|Economy")
	int32 GatherStandUU = KodEconomyDefaults::JadeiteGatherStandUU;

	/** NodeDepleted is logged once. Not part of the idle hash (Remaining already is). */
	UPROPERTY(BlueprintReadOnly, Category = "Kod|Economy")
	bool bDepletionLogged = false;
};

/** Result of one withdrawal. Not hashed on its own — the node and the worker cargo are. */
USTRUCT(BlueprintType)
struct KODCORE_API FKodNodeTakeResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Economy")
	int32 Amount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Economy")
	EKodResourceType Type = EKodResourceType::Jadeite;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Economy")
	bool bFound = false;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Economy")
	bool bFromTrickle = false;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Economy")
	bool bDepleted = false;
};

/** Command center / PROXY_CC. Not a combat entity. */
USTRUCT(BlueprintType)
struct KODCORE_API FKodDropOffState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Economy")
	FKodEntityId Id;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Economy")
	int32 TeamId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Economy")
	FVector Position = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Economy")
	int32 StandUU = KodEconomyDefaults::DropOffStandUU;
};

FORCEINLINE const TCHAR* KodResourceTypeName(EKodResourceType Type)
{
	return Type == EKodResourceType::Luminene ? TEXT("Luminene") : TEXT("Jadeite");
}
