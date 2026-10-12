#include "Sim/KodSimSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

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
		TEXT("SC2-style cheat. Usage: kod.Cheat invincible | add credits [n] | add luminene [n] | instant build | no supply cap | reveal map | no cooldown"),
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
		TEXT("Override a balance row integer. Usage: kod.Balance Unit Ranger MaxHealth 200"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&KodBalance));
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

bool UKodSimSubsystem::ApplyCheatLine(const FString& Line)
{
#if UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Log, TEXT("KodCheat Refused Shipping"));
	return false;
#else
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
	if (bCompetitive)
	{
		UE_LOG(LogTemp, Log, TEXT("KodCheat Refused Competitive Line=%s"), *Text);
		return false;
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

	bool bUsed = false;
	if (Head == TEXT("invincible"))
	{
		bCheatInvincible = !bCheatInvincible;
		bUsed = true;
		UE_LOG(LogTemp, Log, TEXT("KodCheat Invincible=%d"), bCheatInvincible ? 1 : 0);
	}
	else if (Head == TEXT("add") && Second == TEXT("credits"))
	{
		FKodResourceCost Gain;
		Gain.Jadeite = Amount(5000);
		Deposit(0, Gain);
		bUsed = true;
		UE_LOG(LogTemp, Log, TEXT("KodCheat Credits=%d"), Gain.Jadeite);
	}
	else if (Head == TEXT("add") && Second == TEXT("luminene"))
	{
		FKodResourceCost Gain;
		Gain.Luminene = Amount(5000);
		Deposit(0, Gain);
		bUsed = true;
		UE_LOG(LogTemp, Log, TEXT("KodCheat Luminene=%d"), Gain.Luminene);
	}
	else if (Head == TEXT("instant"))
	{
		bCheatInstantBuild = !bCheatInstantBuild;
		bUsed = true;
		UE_LOG(LogTemp, Log, TEXT("KodCheat InstantBuild=%d"), bCheatInstantBuild ? 1 : 0);
	}
	else if (Head == TEXT("no") && (Second == TEXT("supply") || Second == TEXT("cooldown")))
	{
		if (Second == TEXT("supply"))
		{
			bCheatNoSupply = !bCheatNoSupply;
			UE_LOG(LogTemp, Log, TEXT("KodCheat NoSupply=%d"), bCheatNoSupply ? 1 : 0);
		}
		else
		{
			bCheatNoCooldown = !bCheatNoCooldown;
			UE_LOG(LogTemp, Log, TEXT("KodCheat NoCooldown=%d"), bCheatNoCooldown ? 1 : 0);
		}
		bUsed = true;
	}
	else if (Head == TEXT("reveal"))
	{
		bCheatReveal = !bCheatReveal;
		bUsed = true;
		UE_LOG(LogTemp, Log, TEXT("KodCheat Reveal=%d"), bCheatReveal ? 1 : 0);
	}
	else if (Head == TEXT("nocooldown") || Head == TEXT("nocool"))
	{
		bCheatNoCooldown = !bCheatNoCooldown;
		bUsed = true;
		UE_LOG(LogTemp, Log, TEXT("KodCheat NoCooldown=%d"), bCheatNoCooldown ? 1 : 0);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("KodCheat Unknown Line=%s"), *Text);
		return false;
	}

	if (bUsed)
	{
		bMatchCheated = true;
	}
	return bUsed;
#endif
}
