#pragma once

#include "CoreMinimal.h"
#include "Sim/KodCheatRows.h"

/** Front-end roster. Dynasty is shown and not selectable. Random resolves to Vanguard or Scinder. */
enum class EKodRosterFaction : uint8
{
	Vanguard,
	Scinder,
	Dynasty,
	Random
};

/** Lobby labels. Normal is the sim's Medium step (2). */
enum class EKodLobbyDifficulty : uint8
{
	Easy,
	Normal,
	Hard
};

struct KODCORE_API FKodLobbyMap
{
	FName Id;
	FString PackagePath;
	FString Label;
	bool bPlayable = false;
};

/** How a match chooses Vanguard or Scinder. Index 0 is Vanguard, 1 is Scinder. */
struct KODCORE_API FKodMatchStart
{
	bool bPrompt = true;
	int32 FactionIndex = 0;
};

/**
 * Skirmish and campaign travel payload.
 * URL options are the only channel into the match. Not sim state until applied.
 */
struct KODCORE_API FKodSkirmishSetup
{
	EKodMatchMode Mode = EKodMatchMode::OfflineSkirmish;
	FName MapId = TEXT("Faultline");
	EKodRosterFaction PlayerFaction = EKodRosterFaction::Vanguard;
	EKodRosterFaction AiFaction = EKodRosterFaction::Scinder;
	EKodLobbyDifficulty Difficulty = EKodLobbyDifficulty::Normal;
	int32 PlayerColor = 0;
	int32 AiColor = 1;
	int32 PlayerStart = 0;
	int32 AiStart = 1;
	bool bEnableAi = true;
	int32 Seed = 1;

	FString ToOptions() const;
	static bool TryParse(const FString& Options, FKodSkirmishSetup& Out);
	static bool TryParseFaction(const FString& Text, EKodRosterFaction& OutFaction);
	static bool IsSelectableFaction(EKodRosterFaction Faction);
	static int32 ToAiDifficulty(EKodLobbyDifficulty LobbyDifficulty);
	static bool MatchAllowsCheats(EKodMatchMode MatchMode);
	/** False when the session is online, or the mode is not offline campaign or offline skirmish. */
	static bool CheatsAllowed(EKodMatchMode MatchMode, bool bOnlineSession);
	/** True for Match=ranked, Match=online, Online=1, or Session=online/ranked. */
	static bool OptionsMarkOnline(const FString& Options);
	static const TCHAR* CheatRefusalText();
	/** Vanguard is 0, Scinder is 1. Random uses Salt (player uses Seed, AI uses Seed + 17). */
	int32 ResolveRosterIndex(EKodRosterFaction Faction, int32 Salt) const;
	int32 ResolvePlayerFactionIndex() const;
	bool FindMap(FKodLobbyMap& OutMap) const;

	static void GetMaps(TArray<FKodLobbyMap>& OutMaps);
	static FString FactionName(EKodRosterFaction Faction);
	static FString DifficultyName(EKodLobbyDifficulty LobbyDifficulty);
	static FLinearColor TeamSwatch(int32 ColorIndex);

	/**
	 * Prompt is skipped for -kod.Autoplay, -kod.ShotAfter, -kod.ShotZoom,
	 * -kod.ShotSelectMove, -kod.ShotBuildings, -kod.ShotPan, -kod.ShotVoiceSpam,
	 * or a front-end PFaction option.
	 * Those runs use -kod.Faction=VG|SC|Vanguard|Scinder and default to Vanguard.
	 */
	static FKodMatchStart ResolveMatchStart(const TCHAR* CommandLine, const FString& Options);
};
