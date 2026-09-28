#pragma once

#include "CoreMinimal.h"
#include "Style/KodUITypes.generated.h"

/**
 * Material language is a stamp lock, not a skin swap.
 * Ironstock is the in-game HUD. Cyan glass is front-end only.
 */
UENUM(BlueprintType)
enum class EKodUIMaterialLanguage : uint8
{
	Ironstock UMETA(DisplayName = "Ironstock"),
	CyanGlass UMETA(DisplayName = "Cyan Glass"),
};

UENUM(BlueprintType)
enum class EKodUIScreen : uint8
{
	Hud UMETA(DisplayName = "In-game HUD"),
	MainMenu UMETA(DisplayName = "Main Menu"),
	LobbyVersus UMETA(DisplayName = "Lobby Versus"),
};

/**
 * Versus faction row. USA and RSF only.
 * RU, CN, and RANDOM are not values here — they are not hidden slots.
 */
UENUM(BlueprintType)
enum class EKodVersusFaction : uint8
{
	USA UMETA(DisplayName = "USA"),
	RSF UMETA(DisplayName = "RSF"),
};

/** Versus sub-nav. Stamp language: TRAINING · 1V1 · TEAMS · TOURNAMENTS. */
UENUM(BlueprintType)
enum class EKodVersusMode : uint8
{
	Training UMETA(DisplayName = "TRAINING"),
	OneVOne UMETA(DisplayName = "1V1"),
	Teams UMETA(DisplayName = "TEAMS"),
	Tournaments UMETA(DisplayName = "TOURNAMENTS"),
};

/**
 * Lobby chat rail. Stamp v2.6 has a single legal side.
 * There is no left-rail enumerator.
 */
UENUM(BlueprintType)
enum class EKodLobbyChatRail : uint8
{
	Right UMETA(DisplayName = "Right"),
};

UENUM(BlueprintType)
enum class EKodLobbyChatTab : uint8
{
	Lobby UMETA(DisplayName = "LOBBY"),
	Party UMETA(DisplayName = "PARTY"),
};

/** Lobby tile chrome. Cyan glass only. Not a HUD material. */
UENUM(BlueprintType)
enum class EKodFactionTileState : uint8
{
	Idle UMETA(DisplayName = "Idle"),
	Hover UMETA(DisplayName = "Hover"),
	Selected UMETA(DisplayName = "Selected"),
	Disabled UMETA(DisplayName = "Disabled"),
};

/**
 * HUD accent theme ids on the locked v4 shell. Ironstock metal only.
 * USA_Ironstock and RSF_RustOrange are id stubs. Their re-skin paint is on HOLD.
 * RU_RustIndustrial and CN_ImperialGreenGold are not enumerators. Those sheets are CONDITIONAL/FAIL.
 * There is no cyan-glass enumerator.
 */
UENUM(BlueprintType)
enum class EKodFactionAccentTheme : uint8
{
	USA_Ironstock UMETA(DisplayName = "USA Ironstock"),
	RSF_RustOrange UMETA(DisplayName = "RSF Rust Orange"),
};

/** One Versus chat line. Speaker is cyan; body is white. */
USTRUCT(BlueprintType)
struct FKodChatLine
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|UI|Lobby")
	FText Speaker;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|UI|Lobby")
	FText Body;
};

/** Bottom-bar columns, left to right, matching layout stamp v4. */
UENUM(BlueprintType)
enum class EKodHudColumn : uint8
{
	Minimap,
	Selection,
	Portrait,
	Command,
	Gutter,
};

UENUM(BlueprintType)
enum class EKodMainMenuAction : uint8
{
	PlayVersus,
	Training,
	Campaign,
	Coop,
	Armory,
	Options,
	Exit,
	Friends,
	Help,
	Menu,
};

/** Fictional ladder preview row. Not a live ranked service. */
USTRUCT(BlueprintType)
struct FKodLadderRow
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|UI|Lobby")
	int32 Rank = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|UI|Lobby")
	FText Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|UI|Lobby")
	int32 Mmr = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|UI|Lobby")
	int32 Wins = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|UI|Lobby")
	int32 Losses = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|UI|Lobby")
	bool bLocalPlayer = false;
};
