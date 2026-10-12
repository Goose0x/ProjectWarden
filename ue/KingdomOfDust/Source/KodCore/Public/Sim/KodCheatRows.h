#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "KodCheatRows.generated.h"

/** Where this match allows cheats. Campaign and OfflineSkirmish only. Hashed. */
UENUM(BlueprintType)
enum class EKodMatchMode : uint8
{
	Campaign,
	OfflineSkirmish,
	OnlineRanked,
	Replay
};

/** What a chat phrase does. The phrase text lives in the cheat table. */
UENUM(BlueprintType)
enum class EKodCheatEffect : uint8
{
	None,
	Jade,
	Luminene,
	Invincible,
	InstantBuild,
	NoSupply,
	Reveal,
	NoCooldown,
	AllUpgrades,
	Victory,
	Defeat,
	DustToDust
};

/** One renameable phrase. Effect is the stable id. */
USTRUCT(BlueprintType)
struct KODCORE_API FKodCheatPhraseRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Cheat")
	FString Phrase;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Cheat")
	EKodCheatEffect Effect = EKodCheatEffect::None;
};

/** Result of typing a line into chat. */
enum class EKodCheatSubmit : uint8
{
	NotCheat,
	Refused,
	Applied
};
