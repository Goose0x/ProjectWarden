#pragma once

#include "CoreMinimal.h"
#include "Sim/KodEntityId.h"
#include "KodCommandTypes.generated.h"

/** High-level order kinds flowing through UKodCommandSubsystem. */
UENUM(BlueprintType)
enum class EKodCommandType : uint8
{
	None        UMETA(DisplayName = "None"),
	Move        UMETA(DisplayName = "Move"),
	Attack      UMETA(DisplayName = "Attack"),
	Stop        UMETA(DisplayName = "Stop"),
	Build       UMETA(DisplayName = "Build / Train"),
	CastPower   UMETA(DisplayName = "Cast Power"),
	Gather      UMETA(DisplayName = "Gather"),
	SetRally    UMETA(DisplayName = "Set Rally")
};

/**
 * Single ordered command. Offline M1 executes locally;
 * later replication / server auth can serialize the same payload.
 */
USTRUCT(BlueprintType)
struct KODNET_API FKodCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Command")
	EKodCommandType Type = EKodCommandType::None;

	/** Issuing player (PlayerState / Controller id). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Command")
	int32 IssuerPlayerId = INDEX_NONE;

	/** Units / buildings that should receive the order. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Command")
	TArray<FKodEntityId> SourceEntities;

	/** Attack / interact target entity (optional). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Command")
	FKodEntityId TargetEntity;

	/** World location for Move / CastPower / Build placement. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Command")
	FVector TargetLocation = FVector::ZeroVector;

	/** Soft path or primary asset name for Build / CastPower payloads. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Command")
	FName PayloadName;

	/** Queue behind existing orders when true (shift-queue). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Command")
	bool bQueued = false;

	/** Monotonic id assigned by subsystem on enqueue. */
	UPROPERTY(BlueprintReadOnly, Category = "Kod|Command")
	int32 CommandId = 0;
};
