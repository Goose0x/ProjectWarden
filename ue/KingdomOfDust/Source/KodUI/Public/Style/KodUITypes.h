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
 * HUD accent theme ids. Ironstock metal shell only. No cyan glass.
 * USA_TanGreenGold and RSF_MetalStoneOrangeArch are the Versus ids.
 * USA paint is HOLD pending v4 QA. RSF paint is color-language v3.
 * CN_JadeStoneGoldRed and RU_SovietColdBlueIce stay stamped v2 HUD previews.
 * USA_Ironstock aliases USA_TanGreenGold. RSF_RustOrange aliases RSF_MetalStoneOrangeArch.
 * RU_RustIndustrial and CN_ImperialGreenGold are not ids.
 */
UENUM(BlueprintType)
enum class EKodFactionAccentTheme : uint8
{
	USA_TanGreenGold UMETA(DisplayName = "USA Tan Green Gold"),
	RSF_MetalStoneOrangeArch UMETA(DisplayName = "RSF Metal Stone Orange"),
	CN_JadeStoneGoldRed UMETA(DisplayName = "CN Jade Stone Gold Red"),
	RU_SovietColdBlueIce UMETA(DisplayName = "RU Soviet Cold Blue Ice"),
};

/**
 * One faction color language. Roles stay distinct so USA does not collapse to a single olive.
 * Metal and Frame are the shell. Field is the second hue. Proud is icon and readout.
 * Mark is an extra hue (CN red). Other languages set Mark equal to Proud.
 * Arch and Mandate are material motifs, not extra widgets.
 */
USTRUCT(BlueprintType)
struct FKodHudAccentColors
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Kod|UI|Style")
	FLinearColor Metal = FLinearColor(0.52f, 0.42f, 0.28f, 1.f);

	UPROPERTY(BlueprintReadOnly, Category = "Kod|UI|Style")
	FLinearColor Frame = FLinearColor(0.80f, 0.68f, 0.46f, 1.f);

	UPROPERTY(BlueprintReadOnly, Category = "Kod|UI|Style")
	FLinearColor Field = FLinearColor(0.42f, 0.48f, 0.28f, 1.f);

	UPROPERTY(BlueprintReadOnly, Category = "Kod|UI|Style")
	FLinearColor Proud = FLinearColor(0.95f, 0.78f, 0.30f, 1.f);

	UPROPERTY(BlueprintReadOnly, Category = "Kod|UI|Style")
	FLinearColor Mark = FLinearColor(0.95f, 0.78f, 0.30f, 1.f);
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
