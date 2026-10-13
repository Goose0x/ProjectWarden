#include "Sim/KodSimSubsystem.h"

#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/KodRTSCameraPawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Warden/KodWardenPaths.h"

namespace
{
	UKodSimSubsystem* FindPlaySim(UWorld* World)
	{
		if (World)
		{
			if (UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>())
			{
				return Sim;
			}
		}
		if (!GEngine)
		{
			return nullptr;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* Candidate = Context.World();
			if (!Candidate || Candidate->WorldType != EWorldType::PIE)
			{
				continue;
			}
			if (UKodSimSubsystem* Sim = Candidate->GetSubsystem<UKodSimSubsystem>())
			{
				return Sim;
			}
		}
		return nullptr;
	}

#if !UE_BUILD_SHIPPING
	void KodCheat(const TArray<FString>& Args, UWorld* World)
	{
		UKodSimSubsystem* Sim = FindPlaySim(World);
		if (!Sim)
		{
			UE_LOG(LogTemp, Warning, TEXT("kod.Cheat: no sim (start PIE)"));
			return;
		}
		FString Line;
		for (int32 Index = 0; Index < Args.Num(); ++Index)
		{
			if (Index > 0)
			{
				Line += TEXT(" ");
			}
			Line += Args[Index];
		}
		Sim->ApplyCheatLine(Line);
	}
#endif

	void KodCompetitive(const TArray<FString>& Args, UWorld* World)
	{
		UKodSimSubsystem* Sim = FindPlaySim(World);
		if (!Sim)
		{
			UE_LOG(LogTemp, Warning, TEXT("kod.Competitive: no sim (start PIE)"));
			return;
		}
		const int32 Enabled = (Args.Num() > 0 && FCString::Atoi(*Args[0]) == 0) ? 0 : 1;
		Sim->SetCompetitive(Enabled != 0);
	}

	void KodBalance(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 4)
		{
			UE_LOG(LogTemp, Warning, TEXT("kod.Balance <table> <row> <field> <value>"));
			return;
		}
		UKodSimSubsystem* Sim = FindPlaySim(World);
		if (!Sim)
		{
			UE_LOG(LogTemp, Warning, TEXT("kod.Balance: no sim (start PIE)"));
			return;
		}
		Sim->SetBalanceField(FName(*Args[0]), FName(*Args[1]), FName(*Args[2]), FCString::Atoi(*Args[3]));
	}

#if !UE_BUILD_SHIPPING
	FAutoConsoleCommandWithWorldAndArgs GKodCheat(
		TEXT("kod.Cheat"),
		TEXT("Dev alias for chat cheats. Refused outside campaign and offline skirmish."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&KodCheat));
#endif

	FAutoConsoleCommandWithWorldAndArgs GKodCompetitive(
		TEXT("kod.Competitive"),
		TEXT("1 disables cheats. Usage: kod.Competitive 0|1"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&KodCompetitive));

	void KodGeneralPowers(const TArray<FString>& Args, UWorld* World)
	{
		UKodSimSubsystem* Sim = FindPlaySim(World);
		if (!Sim)
		{
			UE_LOG(LogTemp, Warning, TEXT("kod.GeneralPowers: no sim (start PIE)"));
			return;
		}
		const int32 Enabled = (Args.Num() > 0 && FCString::Atoi(*Args[0]) == 0) ? 0 : 1;
		Sim->SetGeneralPowers(Enabled != 0);
	}

	void KodRamp(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 2)
		{
			UE_LOG(LogTemp, Warning, TEXT("kod.Ramp <cellX> <cellY>"));
			return;
		}
		UKodSimSubsystem* Sim = FindPlaySim(World);
		if (!Sim)
		{
			UE_LOG(LogTemp, Warning, TEXT("kod.Ramp: no sim (start PIE)"));
			return;
		}
		Sim->RegisterRampCell(FCString::Atoi(*Args[0]), FCString::Atoi(*Args[1]));
	}

	FAutoConsoleCommandWithWorldAndArgs GKodGeneralPowers(
		TEXT("kod.GeneralPowers"),
		TEXT("1 enables Vanguard powers. Competitive forces them off. Usage: kod.GeneralPowers 0|1"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&KodGeneralPowers));

	FAutoConsoleCommandWithWorldAndArgs GKodRamp(
		TEXT("kod.Ramp"),
		TEXT("Mark a build cell as a ramp. Usage: kod.Ramp <cellX> <cellY>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&KodRamp));

	FAutoConsoleCommandWithWorldAndArgs GKodBalance(
		TEXT("kod.Balance"),
		TEXT("Override a balance row integer. Usage: kod.Balance Unit Marine MaxHealth 200"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&KodBalance));

	void KodMatchMode(const TArray<FString>& Args, UWorld* World)
	{
		UKodSimSubsystem* Sim = FindPlaySim(World);
		if (!Sim)
		{
			UE_LOG(LogTemp, Warning, TEXT("kod.MatchMode: no sim (start PIE)"));
			return;
		}
		const FString Arg = Args.Num() > 0 ? Args[0].ToLower() : FString();
		EKodMatchMode Mode = Sim->GetMatchMode();
		if (Arg == TEXT("campaign"))
		{
			Mode = EKodMatchMode::Campaign;
		}
		else if (Arg == TEXT("skirmish") || Arg == TEXT("offline"))
		{
			Mode = EKodMatchMode::OfflineSkirmish;
		}
		else if (Arg == TEXT("ranked") || Arg == TEXT("online"))
		{
			Mode = EKodMatchMode::OnlineRanked;
		}
		else if (Arg == TEXT("replay"))
		{
			Mode = EKodMatchMode::Replay;
		}
		else if (Arg == TEXT("custom"))
		{
			Mode = EKodMatchMode::Custom;
		}
		else if (!Arg.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("kod.MatchMode campaign|skirmish|ranked|replay|custom"));
			return;
		}
		Sim->SetMatchMode(Mode);
	}

	FAutoConsoleCommandWithWorldAndArgs GKodMatchMode(
		TEXT("kod.MatchMode"),
		TEXT("Campaign, offline skirmish, ranked, replay, or custom. Usage: kod.MatchMode skirmish"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&KodMatchMode));
}

void UKodSimSubsystem::SetCompetitive(bool bEnabled)
{
	bCompetitive = bEnabled;
	if (bCompetitive)
	{
		bGeneralPowers = false;
	}
	UE_LOG(LogTemp, Log, TEXT("KodCompetitive %d"), bEnabled ? 1 : 0);
	UE_LOG(LogTemp, Log, TEXT("KodPowers Enabled=%d"), bGeneralPowers ? 1 : 0);
}

namespace
{
	const TCHAR* MatchModeLabel(EKodMatchMode Mode)
	{
		switch (Mode)
		{
		case EKodMatchMode::Campaign:
			return TEXT("Campaign");
		case EKodMatchMode::OfflineSkirmish:
			return TEXT("OfflineSkirmish");
		case EKodMatchMode::OnlineRanked:
			return TEXT("OnlineRanked");
		case EKodMatchMode::Replay:
			return TEXT("Replay");
		case EKodMatchMode::Custom:
			return TEXT("Custom");
		default:
			return TEXT("OfflineSkirmish");
		}
	}

	int64 EdgeCross(int32 AX, int32 AY, int32 BX, int32 BY, int32 PX, int32 PY)
	{
		return (static_cast<int64>(BX) - AX) * (static_cast<int64>(PY) - AY)
			- (static_cast<int64>(BY) - AY) * (static_cast<int64>(PX) - AX);
	}

	bool PointInTriangle(int32 AX, int32 AY, int32 BX, int32 BY, int32 CX, int32 CY, int32 PX, int32 PY)
	{
		const int64 AB = EdgeCross(AX, AY, BX, BY, PX, PY);
		const int64 BC = EdgeCross(BX, BY, CX, CY, PX, PY);
		const int64 CA = EdgeCross(CX, CY, AX, AY, PX, PY);
		const bool bNeg = AB < 0 || BC < 0 || CA < 0;
		const bool bPos = AB > 0 || BC > 0 || CA > 0;
		return !(bNeg && bPos);
	}

	bool IsCheatToggle(EKodCheatEffect Effect)
	{
		return Effect == EKodCheatEffect::Invincible
			|| Effect == EKodCheatEffect::InstantBuild
			|| Effect == EKodCheatEffect::NoSupply
			|| Effect == EKodCheatEffect::Reveal
			|| Effect == EKodCheatEffect::NoCooldown;
	}

	const TCHAR* CheatListName(EKodCheatEffect Effect)
	{
		switch (Effect)
		{
		case EKodCheatEffect::Jade:
			return TEXT("Show Me The Jade");
		case EKodCheatEffect::Luminene:
			return TEXT("Glow In The Dark");
		case EKodCheatEffect::Invincible:
			return TEXT("Invincible");
		case EKodCheatEffect::InstantBuild:
			return TEXT("Instant Build");
		case EKodCheatEffect::NoSupply:
			return TEXT("No Supply Cap");
		case EKodCheatEffect::Reveal:
			return TEXT("Reveal Map");
		case EKodCheatEffect::NoCooldown:
			return TEXT("No Cooldown");
		case EKodCheatEffect::AllUpgrades:
			return TEXT("All Upgrades");
		case EKodCheatEffect::Victory:
			return TEXT("Victory");
		case EKodCheatEffect::Defeat:
			return TEXT("Defeat");
		case EKodCheatEffect::DustToDust:
			return TEXT("Dust to Dust");
		case EKodCheatEffect::Credits:
			return TEXT("Credits");
		default:
			return TEXT("Cheat");
		}
	}
}

void UKodSimSubsystem::SetMatchMode(EKodMatchMode Mode)
{
	MatchMode = Mode;
	UE_LOG(LogTemp, Log, TEXT("KodMatch Mode=%s"), MatchModeLabel(MatchMode));
}

bool UKodSimSubsystem::AreCheatsAllowed() const
{
	if (bCompetitive)
	{
		return false;
	}
	return MatchMode == EKodMatchMode::Campaign || MatchMode == EKodMatchMode::OfflineSkirmish;
}

void UKodSimSubsystem::EnsureCheatPhrases()
{
	if (CheatTable)
	{
		return;
	}
	CheatTable = NewObject<UDataTable>(this, FName(TEXT("CheatPhrases")));
	CheatTable->RowStruct = FKodCheatPhraseRow::StaticStruct();
	auto Add = [this](const TCHAR* RowId, const TCHAR* Phrase, EKodCheatEffect Effect)
	{
		FKodCheatPhraseRow Row;
		Row.Phrase = Phrase;
		Row.Effect = Effect;
		CheatTable->AddRow(FName(RowId), Row);
	};
	Add(TEXT("Jade"), TEXT("show me the jade"), EKodCheatEffect::Jade);
	Add(TEXT("Luminene"), TEXT("glow in the dark"), EKodCheatEffect::Luminene);
	Add(TEXT("Invincible"), TEXT("it is a good day to dust"), EKodCheatEffect::Invincible);
	Add(TEXT("InstantBuild"), TEXT("rapid deployment"), EKodCheatEffect::InstantBuild);
	Add(TEXT("NoSupply"), TEXT("more boots"), EKodCheatEffect::NoSupply);
	Add(TEXT("Reveal"), TEXT("eyes in the sky"), EKodCheatEffect::Reveal);
	Add(TEXT("NoCooldown"), TEXT("no rest for the wicked"), EKodCheatEffect::NoCooldown);
	Add(TEXT("AllUpgrades"), TEXT("upgrade everything"), EKodCheatEffect::AllUpgrades);
	Add(TEXT("Victory"), TEXT("victory is mine"), EKodCheatEffect::Victory);
	Add(TEXT("Defeat"), TEXT("we are not worthy"), EKodCheatEffect::Defeat);
	Add(TEXT("DustToDust"), TEXT("dust to dust"), EKodCheatEffect::DustToDust);
}

bool UKodSimSubsystem::SetCheatPhrase(EKodCheatEffect Effect, const FString& Phrase)
{
	EnsureCheatPhrases();
	const FString Trimmed = Phrase.TrimStartAndEnd();
	if (!CheatTable || Trimmed.IsEmpty() || Effect == EKodCheatEffect::None)
	{
		return false;
	}
	TArray<FName> Names;
	CheatTable->GetRowMap().GetKeys(Names);
	Names.Sort([](const FName& A, const FName& B)
	{
		return A.ToString() < B.ToString();
	});
	bool bFound = false;
	for (const FName& Name : Names)
	{
		const FKodCheatPhraseRow* Row = CheatTable->FindRow<FKodCheatPhraseRow>(Name, TEXT("KodCheat"), false);
		if (!Row || Row->Effect != Effect)
		{
			continue;
		}
		const_cast<FKodCheatPhraseRow*>(Row)->Phrase = Trimmed;
		bFound = true;
	}
	return bFound;
}

bool UKodSimSubsystem::FindCheatPhrase(const FString& Line, EKodCheatEffect& OutEffect) const
{
	OutEffect = EKodCheatEffect::None;
	if (!CheatTable)
	{
		return false;
	}
	const FString Trimmed = Line.TrimStartAndEnd();
	if (Trimmed.IsEmpty())
	{
		return false;
	}
	TArray<FName> Names;
	CheatTable->GetRowMap().GetKeys(Names);
	Names.Sort([](const FName& A, const FName& B)
	{
		return A.ToString() < B.ToString();
	});
	for (const FName& Name : Names)
	{
		const FKodCheatPhraseRow* Row = CheatTable->FindRow<FKodCheatPhraseRow>(Name, TEXT("KodCheat"), false);
		if (!Row || !Row->Phrase.Equals(Trimmed, ESearchCase::IgnoreCase))
		{
			continue;
		}
		OutEffect = Row->Effect;
		return OutEffect != EKodCheatEffect::None;
	}
	return false;
}

void UKodSimSubsystem::NoteCheatUse(EKodCheatEffect Effect, bool bEnabled, const FString& Detail)
{
	const uint8 Key = static_cast<uint8>(Effect);
	int32 Index = INDEX_NONE;
	for (int32 UseIndex = 0; UseIndex < CheatUses.Num(); ++UseIndex)
	{
		if (CheatUses[UseIndex].Effect == Key)
		{
			Index = UseIndex;
			break;
		}
	}
	if (Index == INDEX_NONE)
	{
		FKodCheatUse Added;
		Added.Effect = Key;
		CheatUses.Add(Added);
		CheatUses.Sort([](const FKodCheatUse& A, const FKodCheatUse& B)
		{
			return A.Effect < B.Effect;
		});
		for (int32 UseIndex = 0; UseIndex < CheatUses.Num(); ++UseIndex)
		{
			if (CheatUses[UseIndex].Effect == Key)
			{
				Index = UseIndex;
				break;
			}
		}
	}
	if (Index == INDEX_NONE)
	{
		return;
	}
	FKodCheatUse& Use = CheatUses[Index];
	Use.Count += 1;
	Use.bEnabled = bEnabled ? 1 : 0;
	bMatchCheated = true;
	const TCHAR* Verb = (IsCheatToggle(Effect) && !bEnabled) ? TEXT("disabled") : TEXT("enabled");
	LastCheatLine = FString::Printf(TEXT("Cheat %s: %s"), Verb, *Detail);
	RaiseCue(FName(TEXT("cheat_enabled")), FVector::ZeroVector, false);
	UE_LOG(LogTemp, Log, TEXT("KodCheat Line=%s"), *LastCheatLine);
}

FString UKodSimSubsystem::GetCheatResultLine() const
{
	if (CheatUses.Num() == 0)
	{
		return FString();
	}
	FString Line = TEXT("Cheats: ");
	for (int32 Index = 0; Index < CheatUses.Num(); ++Index)
	{
		if (Index > 0)
		{
			Line += TEXT(", ");
		}
		const FKodCheatUse& Use = CheatUses[Index];
		const EKodCheatEffect Effect = static_cast<EKodCheatEffect>(Use.Effect);
		Line += CheatListName(Effect);
		if (IsCheatToggle(Effect))
		{
			Line += Use.bEnabled ? TEXT(" (enabled)") : TEXT(" (disabled)");
		}
		else if (Use.Count > 1)
		{
			Line += FString::Printf(TEXT(" x%d"), Use.Count);
		}
	}
	return Line;
}

void UKodSimSubsystem::GrantAllUpgrades(int32 TeamId)
{
	TArray<FName> Names;
	UpgradeSpecs.GetKeys(Names);
	Names.Sort([](const FName& A, const FName& B)
	{
		return A.ToString() < B.ToString();
	});
	FKodTeamTech& Tech = TeamTech.FindOrAdd(TeamId);
	for (const FName& Name : Names)
	{
		const FKodUpgradeSpec* Spec = UpgradeSpecs.Find(Name);
		if (!Spec)
		{
			continue;
		}
		if (Spec->WeaponBonus > Tech.InfantryWeapons)
		{
			Tech.InfantryWeapons = Spec->WeaponBonus;
		}
		if (Spec->ArmorBonus > Tech.InfantryArmor)
		{
			Tech.InfantryArmor = Spec->ArmorBonus;
		}
		if (Spec->SatelliteFeed > 0 && Tech.SatelliteFeed < 1)
		{
			Tech.SatelliteFeed = 1;
			const int32 EnergyMax = FMath::Max(1, Spec->EnergyMax);
			const int32 EnergyStart = FMath::Clamp(Spec->EnergyStart, 0, EnergyMax);
			TArray<int32> CcKeys;
			Structures.GetKeys(CcKeys);
			CcKeys.Sort();
			for (int32 CcKey : CcKeys)
			{
				const FKodStructureState* Cc = Structures.Find(CcKey);
				if (!Cc || Cc->TeamId != TeamId || Cc->DefinitionId != FName(KodWardenPaths::Id_CommandCenter))
				{
					continue;
				}
				if (FKodSimEntityState* Combat = States.Find(CcKey))
				{
					Combat->EnergyMax = EnergyMax;
					Combat->Energy = EnergyStart;
				}
			}
		}
		UE_LOG(LogTemp, Log, TEXT("KodCheat Upgrade=%s"), *Name.ToString());
	}
}

void UKodSimSubsystem::KillEnemyUnitsOnScreen()
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	AKodRTSCameraPawn* CameraPawn = PC ? Cast<AKodRTSCameraPawn>(PC->GetPawn()) : nullptr;
	FVector NearLeft;
	FVector NearRight;
	FVector FarRight;
	FVector FarLeft;
	if (!CameraPawn || !CameraPawn->GetGroundViewCorners(NearLeft, NearRight, FarRight, FarLeft))
	{
		UE_LOG(LogTemp, Log, TEXT("KodCheat Dust Miss"));
		return;
	}
	const int32 NLX = FMath::RoundToInt(NearLeft.X);
	const int32 NLY = FMath::RoundToInt(NearLeft.Y);
	const int32 NRX = FMath::RoundToInt(NearRight.X);
	const int32 NRY = FMath::RoundToInt(NearRight.Y);
	const int32 FRX = FMath::RoundToInt(FarRight.X);
	const int32 FRY = FMath::RoundToInt(FarRight.Y);
	const int32 FLX = FMath::RoundToInt(FarLeft.X);
	const int32 FLY = FMath::RoundToInt(FarLeft.Y);
	TArray<int32> Keys;
	States.GetKeys(Keys);
	Keys.Sort();
	for (int32 Key : Keys)
	{
		FKodSimEntityState* State = States.Find(Key);
		if (!State || !State->bMobile || State->TeamId == 0 || State->Health <= 0.f)
		{
			continue;
		}
		const int32 PX = FMath::RoundToInt(State->Position.X);
		const int32 PY = FMath::RoundToInt(State->Position.Y);
		const bool bOnScreen = PointInTriangle(NLX, NLY, NRX, NRY, FRX, FRY, PX, PY)
			|| PointInTriangle(NLX, NLY, FRX, FRY, FLX, FLY, PX, PY);
		if (!bOnScreen)
		{
			continue;
		}
		FinishEntityDeath(*State, 0);
	}
}

void UKodSimSubsystem::ApplyCheatEffect(EKodCheatEffect Effect)
{
	switch (Effect)
	{
	case EKodCheatEffect::Jade:
	{
		FKodResourceCost Gain;
		Gain.Jadeite = 10000;
		Gain.Luminene = 5000;
		Deposit(0, Gain);
		UE_LOG(LogTemp, Log, TEXT("KodCheat Jade=10000 Luminene=5000"));
		NoteCheatUse(Effect, true, TEXT("+10000 Credits, +5000 Luminene"));
		break;
	}
	case EKodCheatEffect::Luminene:
	{
		FKodResourceCost Gain;
		Gain.Luminene = 5000;
		Deposit(0, Gain);
		UE_LOG(LogTemp, Log, TEXT("KodCheat Luminene=5000"));
		NoteCheatUse(Effect, true, TEXT("+5000 Luminene"));
		break;
	}
	case EKodCheatEffect::Invincible:
		bCheatInvincible = !bCheatInvincible;
		UE_LOG(LogTemp, Log, TEXT("KodCheat Invincible=%d"), bCheatInvincible ? 1 : 0);
		NoteCheatUse(Effect, bCheatInvincible, TEXT("Invincible"));
		break;
	case EKodCheatEffect::InstantBuild:
		bCheatInstantBuild = !bCheatInstantBuild;
		UE_LOG(LogTemp, Log, TEXT("KodCheat InstantBuild=%d"), bCheatInstantBuild ? 1 : 0);
		NoteCheatUse(Effect, bCheatInstantBuild, TEXT("Instant Build"));
		break;
	case EKodCheatEffect::NoSupply:
		bCheatNoSupply = !bCheatNoSupply;
		UE_LOG(LogTemp, Log, TEXT("KodCheat NoSupply=%d"), bCheatNoSupply ? 1 : 0);
		NoteCheatUse(Effect, bCheatNoSupply, TEXT("No Supply Cap"));
		break;
	case EKodCheatEffect::Reveal:
		bCheatReveal = !bCheatReveal;
		UE_LOG(LogTemp, Log, TEXT("KodCheat Reveal=%d"), bCheatReveal ? 1 : 0);
		NoteCheatUse(Effect, bCheatReveal, TEXT("Reveal Map"));
		break;
	case EKodCheatEffect::NoCooldown:
		bCheatNoCooldown = !bCheatNoCooldown;
		UE_LOG(LogTemp, Log, TEXT("KodCheat NoCooldown=%d"), bCheatNoCooldown ? 1 : 0);
		NoteCheatUse(Effect, bCheatNoCooldown, TEXT("No Cooldown"));
		break;
	case EKodCheatEffect::AllUpgrades:
		GrantAllUpgrades(0);
		NoteCheatUse(Effect, true, TEXT("All Upgrades"));
		break;
	case EKodCheatEffect::Victory:
		MatchOutcome = EKodMatchOutcome::Victory;
		UE_LOG(LogTemp, Log, TEXT("KodAI Match Victory"));
		RaiseCue(FName(TEXT("victory")), FVector::ZeroVector, false);
		RememberAiMatch();
		NoteCheatUse(Effect, true, TEXT("Victory"));
		break;
	case EKodCheatEffect::Defeat:
		MatchOutcome = EKodMatchOutcome::Defeat;
		UE_LOG(LogTemp, Log, TEXT("KodAI Match Defeat"));
		RaiseCue(FName(TEXT("defeat")), FVector::ZeroVector, false);
		RememberAiMatch();
		NoteCheatUse(Effect, true, TEXT("Defeat"));
		break;
	case EKodCheatEffect::DustToDust:
		KillEnemyUnitsOnScreen();
		UE_LOG(LogTemp, Log, TEXT("KodCheat Dust"));
		NoteCheatUse(Effect, true, TEXT("Dust to Dust"));
		break;
	case EKodCheatEffect::Credits:
		NoteCheatUse(Effect, true, TEXT("Credits"));
		break;
	default:
		break;
	}
}

EKodCheatSubmit UKodSimSubsystem::SubmitCheatPhrase(const FString& Line)
{
	EnsureCheatPhrases();
	EKodCheatEffect Effect = EKodCheatEffect::None;
	if (!FindCheatPhrase(Line, Effect))
	{
		return EKodCheatSubmit::NotCheat;
	}
	if (!AreCheatsAllowed())
	{
		UE_LOG(LogTemp, Log, TEXT("KodCheat Refused Mode=%s"), MatchModeLabel(MatchMode));
		return EKodCheatSubmit::Refused;
	}
	ApplyCheatEffect(Effect);
	return EKodCheatSubmit::Applied;
}

bool UKodSimSubsystem::ApplyCheatLine(const FString& Line)
{
	FString Text = Line.TrimStartAndEnd();
	if (Text.StartsWith(TEXT("kod.Cheat"), ESearchCase::IgnoreCase))
	{
		Text = Text.Mid(9).TrimStartAndEnd();
	}
	if (Text.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("KodCheat Empty"));
		return false;
	}
	EnsureCheatPhrases();
	if (!AreCheatsAllowed())
	{
		UE_LOG(LogTemp, Log, TEXT("KodCheat Refused Mode=%s"), MatchModeLabel(MatchMode));
		return false;
	}
	EKodCheatEffect PhraseEffect = EKodCheatEffect::None;
	if (FindCheatPhrase(Text, PhraseEffect))
	{
		ApplyCheatEffect(PhraseEffect);
		return true;
	}

	TArray<FString> Parts;
	Text.ParseIntoArrayWS(Parts);
	if (Parts.Num() <= 0)
	{
		return false;
	}
	const FString Head = Parts[0].ToLower();
	const FString Second = Parts.Num() > 1 ? Parts[1].ToLower() : FString();
	auto Amount = [&Parts](int32 DefaultValue) -> int32
	{
		for (int32 Index = Parts.Num() - 1; Index >= 0; --Index)
		{
			if (Parts[Index].IsNumeric())
			{
				return FMath::Max(0, FCString::Atoi(*Parts[Index]));
			}
		}
		return DefaultValue;
	};

	if (Head == TEXT("invincible"))
	{
		bCheatInvincible = !bCheatInvincible;
		UE_LOG(LogTemp, Log, TEXT("KodCheat Invincible=%d"), bCheatInvincible ? 1 : 0);
		NoteCheatUse(EKodCheatEffect::Invincible, bCheatInvincible, TEXT("Invincible"));
		return true;
	}
	if (Head == TEXT("add") && Second == TEXT("credits"))
	{
		FKodResourceCost Gain;
		Gain.Jadeite = Amount(5000);
		Deposit(0, Gain);
		UE_LOG(LogTemp, Log, TEXT("KodCheat Credits=%d"), Gain.Jadeite);
		NoteCheatUse(EKodCheatEffect::Credits, true, FString::Printf(TEXT("+%d Credits"), Gain.Jadeite));
		return true;
	}
	if (Head == TEXT("add") && Second == TEXT("luminene"))
	{
		FKodResourceCost Gain;
		Gain.Luminene = Amount(5000);
		Deposit(0, Gain);
		UE_LOG(LogTemp, Log, TEXT("KodCheat Luminene=%d"), Gain.Luminene);
		NoteCheatUse(EKodCheatEffect::Luminene, true, FString::Printf(TEXT("+%d Luminene"), Gain.Luminene));
		return true;
	}
	if (Head == TEXT("instant"))
	{
		bCheatInstantBuild = !bCheatInstantBuild;
		UE_LOG(LogTemp, Log, TEXT("KodCheat InstantBuild=%d"), bCheatInstantBuild ? 1 : 0);
		NoteCheatUse(EKodCheatEffect::InstantBuild, bCheatInstantBuild, TEXT("Instant Build"));
		return true;
	}
	if (Head == TEXT("no") && Second == TEXT("supply"))
	{
		bCheatNoSupply = !bCheatNoSupply;
		UE_LOG(LogTemp, Log, TEXT("KodCheat NoSupply=%d"), bCheatNoSupply ? 1 : 0);
		NoteCheatUse(EKodCheatEffect::NoSupply, bCheatNoSupply, TEXT("No Supply Cap"));
		return true;
	}
	if (Head == TEXT("no") && Second == TEXT("cooldown"))
	{
		bCheatNoCooldown = !bCheatNoCooldown;
		UE_LOG(LogTemp, Log, TEXT("KodCheat NoCooldown=%d"), bCheatNoCooldown ? 1 : 0);
		NoteCheatUse(EKodCheatEffect::NoCooldown, bCheatNoCooldown, TEXT("No Cooldown"));
		return true;
	}
	if (Head == TEXT("reveal"))
	{
		bCheatReveal = !bCheatReveal;
		UE_LOG(LogTemp, Log, TEXT("KodCheat Reveal=%d"), bCheatReveal ? 1 : 0);
		NoteCheatUse(EKodCheatEffect::Reveal, bCheatReveal, TEXT("Reveal Map"));
		return true;
	}
	if (Head == TEXT("nocooldown") || Head == TEXT("nocool"))
	{
		bCheatNoCooldown = !bCheatNoCooldown;
		UE_LOG(LogTemp, Log, TEXT("KodCheat NoCooldown=%d"), bCheatNoCooldown ? 1 : 0);
		NoteCheatUse(EKodCheatEffect::NoCooldown, bCheatNoCooldown, TEXT("No Cooldown"));
		return true;
	}
	UE_LOG(LogTemp, Warning, TEXT("KodCheat Unknown Line=%s"), *Text);
	return false;
}
