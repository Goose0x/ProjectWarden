#include "Game/KodSkirmishSetup.h"

#include "Kismet/GameplayStatics.h"

namespace
{
	FString ModeName(EKodMatchMode Mode)
	{
		switch (Mode)
		{
		case EKodMatchMode::Campaign:
			return TEXT("campaign");
		case EKodMatchMode::OfflineSkirmish:
			return TEXT("skirmish");
		case EKodMatchMode::OnlineRanked:
			return TEXT("ranked");
		case EKodMatchMode::Replay:
			return TEXT("replay");
		case EKodMatchMode::Custom:
			return TEXT("custom");
		default:
			return TEXT("skirmish");
		}
	}

	EKodMatchMode ModeFromText(const FString& Text)
	{
		if (Text.Equals(TEXT("campaign"), ESearchCase::IgnoreCase))
		{
			return EKodMatchMode::Campaign;
		}
		if (Text.Equals(TEXT("ranked"), ESearchCase::IgnoreCase) || Text.Equals(TEXT("online"), ESearchCase::IgnoreCase))
		{
			return EKodMatchMode::OnlineRanked;
		}
		if (Text.Equals(TEXT("replay"), ESearchCase::IgnoreCase))
		{
			return EKodMatchMode::Replay;
		}
		if (Text.Equals(TEXT("custom"), ESearchCase::IgnoreCase))
		{
			return EKodMatchMode::Custom;
		}
		return EKodMatchMode::OfflineSkirmish;
	}

	bool DifficultyFromText(const FString& Text, EKodLobbyDifficulty& OutDifficulty)
	{
		if (Text.Equals(TEXT("Easy"), ESearchCase::IgnoreCase))
		{
			OutDifficulty = EKodLobbyDifficulty::Easy;
			return true;
		}
		if (Text.Equals(TEXT("Normal"), ESearchCase::IgnoreCase) || Text.Equals(TEXT("Medium"), ESearchCase::IgnoreCase))
		{
			OutDifficulty = EKodLobbyDifficulty::Normal;
			return true;
		}
		if (Text.Equals(TEXT("Hard"), ESearchCase::IgnoreCase))
		{
			OutDifficulty = EKodLobbyDifficulty::Hard;
			return true;
		}
		return false;
	}
}

FString FKodSkirmishSetup::FactionName(EKodRosterFaction Faction)
{
	switch (Faction)
	{
	case EKodRosterFaction::Scinder:
		return TEXT("Scinder");
	case EKodRosterFaction::Dynasty:
		return TEXT("Dynasty");
	case EKodRosterFaction::Random:
		return TEXT("Random");
	default:
		return TEXT("Vanguard");
	}
}

FString FKodSkirmishSetup::DifficultyName(EKodLobbyDifficulty LobbyDifficulty)
{
	switch (LobbyDifficulty)
	{
	case EKodLobbyDifficulty::Easy:
		return TEXT("Easy");
	case EKodLobbyDifficulty::Hard:
		return TEXT("Hard");
	default:
		return TEXT("Normal");
	}
}

bool FKodSkirmishSetup::IsSelectableFaction(EKodRosterFaction Faction)
{
	return Faction == EKodRosterFaction::Vanguard
		|| Faction == EKodRosterFaction::Scinder
		|| Faction == EKodRosterFaction::Random;
}

bool FKodSkirmishSetup::TryParseFaction(const FString& Text, EKodRosterFaction& OutFaction)
{
	if (Text.Equals(TEXT("Vanguard"), ESearchCase::IgnoreCase) || Text.Equals(TEXT("USA"), ESearchCase::IgnoreCase))
	{
		OutFaction = EKodRosterFaction::Vanguard;
		return true;
	}
	if (Text.Equals(TEXT("Scinder"), ESearchCase::IgnoreCase))
	{
		OutFaction = EKodRosterFaction::Scinder;
		return true;
	}
	if (Text.Equals(TEXT("Random"), ESearchCase::IgnoreCase))
	{
		OutFaction = EKodRosterFaction::Random;
		return true;
	}
	if (Text.Equals(TEXT("Dynasty"), ESearchCase::IgnoreCase))
	{
		OutFaction = EKodRosterFaction::Dynasty;
		return false;
	}
	return false;
}

int32 FKodSkirmishSetup::ToAiDifficulty(EKodLobbyDifficulty LobbyDifficulty)
{
	switch (LobbyDifficulty)
	{
	case EKodLobbyDifficulty::Easy:
		return 1;
	case EKodLobbyDifficulty::Hard:
		return 3;
	default:
		return 2;
	}
}

bool FKodSkirmishSetup::MatchAllowsCheats(EKodMatchMode MatchMode)
{
	return MatchMode == EKodMatchMode::Campaign || MatchMode == EKodMatchMode::OfflineSkirmish;
}

void FKodSkirmishSetup::GetMaps(TArray<FKodLobbyMap>& OutMaps)
{
	OutMaps.Reset();
	FKodLobbyMap Faultline;
	Faultline.Id = TEXT("Faultline");
	Faultline.PackagePath = TEXT("/Game/Maps/Sandbox/L_FaultlineTraining");
	Faultline.Label = TEXT("Faultline Training");
	Faultline.bPlayable = true;
	OutMaps.Add(Faultline);

	FKodLobbyMap Sandbox;
	Sandbox.Id = TEXT("Slice0");
	Sandbox.PackagePath = TEXT("/Game/Maps/Sandbox/L_Slice0");
	Sandbox.Label = TEXT("Sandbox");
	Sandbox.bPlayable = true;
	OutMaps.Add(Sandbox);

	FKodLobbyMap CoreLoop;
	CoreLoop.Id = TEXT("CoreLoop");
	CoreLoop.PackagePath = TEXT("/Game/Maps/Sandbox/L_CoreLoop_Test");
	CoreLoop.Label = TEXT("Core Loop");
	CoreLoop.bPlayable = false;
	OutMaps.Add(CoreLoop);
}

bool FKodSkirmishSetup::FindMap(FKodLobbyMap& OutMap) const
{
	TArray<FKodLobbyMap> Maps;
	GetMaps(Maps);
	for (const FKodLobbyMap& Map : Maps)
	{
		if (Map.Id == MapId)
		{
			OutMap = Map;
			return true;
		}
	}
	return false;
}

int32 FKodSkirmishSetup::ResolveRosterIndex(EKodRosterFaction Faction, int32 Salt) const
{
	if (Faction == EKodRosterFaction::Scinder)
	{
		return 1;
	}
	if (Faction == EKodRosterFaction::Random)
	{
		FRandomStream Stream(Salt == 0 ? 1 : Salt);
		return Stream.RandRange(0, 1);
	}
	return 0;
}

int32 FKodSkirmishSetup::ResolvePlayerFactionIndex() const
{
	return ResolveRosterIndex(PlayerFaction, Seed);
}

FString FKodSkirmishSetup::ToOptions() const
{
	return FString::Printf(
		TEXT("?Match=%s?Map=%s?PFaction=%s?AFaction=%s?Difficulty=%s?PColor=%d?AColor=%d?PStart=%d?AStart=%d?Ai=%d?Seed=%d"),
		*ModeName(Mode),
		*MapId.ToString(),
		*FactionName(PlayerFaction),
		*FactionName(AiFaction),
		*DifficultyName(Difficulty),
		PlayerColor,
		AiColor,
		PlayerStart,
		AiStart,
		bEnableAi ? 1 : 0,
		Seed);
}

bool FKodSkirmishSetup::TryParse(const FString& Options, FKodSkirmishSetup& Out)
{
	const FString Match = UGameplayStatics::ParseOption(Options, TEXT("Match"));
	const FString Map = UGameplayStatics::ParseOption(Options, TEXT("Map"));
	if (Match.IsEmpty() && Map.IsEmpty())
	{
		return false;
	}
	FKodSkirmishSetup Parsed;
	if (!Match.IsEmpty())
	{
		Parsed.Mode = ModeFromText(Match);
	}
	if (!Map.IsEmpty())
	{
		Parsed.MapId = FName(*Map);
	}
	EKodRosterFaction Faction = EKodRosterFaction::Vanguard;
	const FString PlayerText = UGameplayStatics::ParseOption(Options, TEXT("PFaction"));
	if (!PlayerText.IsEmpty() && TryParseFaction(PlayerText, Faction) && IsSelectableFaction(Faction))
	{
		Parsed.PlayerFaction = Faction;
	}
	const FString AiText = UGameplayStatics::ParseOption(Options, TEXT("AFaction"));
	if (!AiText.IsEmpty() && TryParseFaction(AiText, Faction) && IsSelectableFaction(Faction))
	{
		Parsed.AiFaction = Faction;
	}
	EKodLobbyDifficulty ParsedDifficulty = EKodLobbyDifficulty::Normal;
	const FString DifficultyText = UGameplayStatics::ParseOption(Options, TEXT("Difficulty"));
	if (!DifficultyText.IsEmpty() && DifficultyFromText(DifficultyText, ParsedDifficulty))
	{
		Parsed.Difficulty = ParsedDifficulty;
	}
	const FString PlayerColorText = UGameplayStatics::ParseOption(Options, TEXT("PColor"));
	if (!PlayerColorText.IsEmpty())
	{
		Parsed.PlayerColor = FMath::Clamp(FCString::Atoi(*PlayerColorText), 0, 5);
	}
	const FString AiColorText = UGameplayStatics::ParseOption(Options, TEXT("AColor"));
	if (!AiColorText.IsEmpty())
	{
		Parsed.AiColor = FMath::Clamp(FCString::Atoi(*AiColorText), 0, 5);
	}
	const FString PlayerStartText = UGameplayStatics::ParseOption(Options, TEXT("PStart"));
	if (!PlayerStartText.IsEmpty())
	{
		Parsed.PlayerStart = FMath::Clamp(FCString::Atoi(*PlayerStartText), 0, 1);
	}
	const FString AiStartText = UGameplayStatics::ParseOption(Options, TEXT("AStart"));
	if (!AiStartText.IsEmpty())
	{
		Parsed.AiStart = FMath::Clamp(FCString::Atoi(*AiStartText), 0, 1);
	}
	if (Parsed.AiStart == Parsed.PlayerStart)
	{
		Parsed.AiStart = Parsed.PlayerStart == 0 ? 1 : 0;
	}
	const FString AiFlag = UGameplayStatics::ParseOption(Options, TEXT("Ai"));
	if (!AiFlag.IsEmpty())
	{
		Parsed.bEnableAi = AiFlag != TEXT("0");
	}
	const FString SeedText = UGameplayStatics::ParseOption(Options, TEXT("Seed"));
	if (!SeedText.IsEmpty())
	{
		Parsed.Seed = FCString::Atoi(*SeedText);
	}
	Out = Parsed;
	return true;
}
