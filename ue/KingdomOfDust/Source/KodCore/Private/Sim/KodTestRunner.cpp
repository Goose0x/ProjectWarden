#include "Sim/KodSimSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Sim/KodBuildTicks.h"
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
			if (!Candidate || (Candidate->WorldType != EWorldType::PIE && Candidate->WorldType != EWorldType::Game))
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

	void KodTest(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogTemp, Warning, TEXT("kod.Test <marines|stim|bunker> [runs] [seed]"));
			return;
		}
		UKodSimSubsystem* Sim = FindPlaySim(World);
		if (!Sim)
		{
			UE_LOG(LogTemp, Warning, TEXT("kod.Test: no sim (start PIE)"));
			return;
		}
		const int32 Runs = Args.Num() > 1 ? FMath::Max(1, FCString::Atoi(*Args[1])) : 1;
		const int32 Seed = Args.Num() > 2 ? FCString::Atoi(*Args[2]) : 1;
		Sim->RunScriptedTest(FName(*Args[0]), Runs, Seed);
	}

	void KodTestSummary(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		UKodSimSubsystem* Sim = FindPlaySim(World);
		if (!Sim)
		{
			UE_LOG(LogTemp, Warning, TEXT("kod.TestSummary: no sim (start PIE)"));
			return;
		}
		Sim->LogTestSummary();
	}

	FAutoConsoleCommandWithWorldAndArgs GKodTest(
		TEXT("kod.Test"),
		TEXT("Seeded headless matchup. Usage: kod.Test marines|stim|bunker [runs] [seed]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&KodTest));

	FAutoConsoleCommandWithWorldAndArgs GKodTestSummary(
		TEXT("kod.TestSummary"),
		TEXT("Print the scripted tests run in this session."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&KodTestSummary));
}

void UKodSimSubsystem::LogTestSummary() const
{
	UE_LOG(LogTemp, Log, TEXT("KodTest Summary Count=%d"), TestResults.Num());
	for (const FKodTestResult& Result : TestResults)
	{
		UE_LOG(LogTemp, Log, TEXT("KodTest Summary %s seed=%d run=%d winner=%d time=%d survivors=%d/%d dealt=%d/%d taken=%d/%d dps=%d/%d hash=%s"),
			*Result.Scenario,
			Result.Seed,
			Result.Run,
			Result.Winner,
			Result.TimeSeconds,
			Result.Survivors0,
			Result.Survivors1,
			Result.Dealt0,
			Result.Dealt1,
			Result.Taken0,
			Result.Taken1,
			Result.Dps0,
			Result.Dps1,
			*Result.Hash);
	}
}

bool UKodSimSubsystem::RunScriptedTest(FName Scenario, int32 Runs, int32 Seed)
{
	const FString Name = Scenario.ToString().ToLower();
	const bool bMarines = Name == TEXT("marines");
	const bool bStim = Name == TEXT("stim");
	const bool bBunker = Name == TEXT("bunker");
	if (!bMarines && !bStim && !bBunker)
	{
		UE_LOG(LogTemp, Warning, TEXT("KodTest Unknown Scenario=%s"), *Scenario.ToString());
		return false;
	}

	const int32 Count = FMath::Clamp(Runs, 1, 50);
	const int32 UseSeed = Seed == 0 ? 1 : Seed;

	TestStash.Blob = ExportSimBlob();
	TestStash.Entities = Entities;
	TestStash.PendingSpawns = PendingSpawns;
	TestStash.PendingBuildingSpawns = PendingBuildingSpawns;
	TestStash.PrevPositions = PrevPositions;
	TestStash.PrevYaws = PrevYaws;
	TestStash.NextId = NextId;
	TestStash.TimeAccumulator = TimeAccumulator;
	TestStash.bSimPaused = bSimPaused;
	TestStash.bMenuMode = bMenuMode;
	TestStash.bHasPlayable = bHasPlayable;
	TestStash.PlayMinX = PlayMinX;
	TestStash.PlayMinY = PlayMinY;
	TestStash.PlayMaxX = PlayMaxX;
	TestStash.PlayMaxY = PlayMaxY;
	TestStashPaths = StoredPaths;
	TestStashNextPathId = NextPathId;
	bTestIsolation = true;

	auto Wipe = [this]()
	{
		States.Reset();
		Entities.Reset();
		Structures.Reset();
		Banks.Reset();
		Supply.Reset();
		ResourceNodes.Reset();
		DropOffs.Reset();
		PendingSpawns.Reset();
		PendingBuildingSpawns.Reset();
		PrevPositions.Reset();
		PrevYaws.Reset();
		TeamVision.Reset();
		SeenStructures.Reset();
		SeenUnits.Reset();
		Samples0.Reset();
		Samples1.Reset();
		OlderSamples0.Reset();
		OlderSamples1.Reset();
		StoredPaths.Reset();
		AcquireGrid.Reset();
		MeleeAttackersByTarget.Reset();
		MatchOutcome = EKodMatchOutcome::None;
		bAiEnabled = false;
		bAiSeeded = false;
		bExposedRule = false;
		bTeam0Exposed = false;
		bTeam1Exposed = false;
		bTeam0ExposedWarned = false;
		bTeam1ExposedWarned = false;
		SimTickIndex = 0;
		TimeAccumulator = 0.f;
		NextId = 1;
		DamageDealt0 = 0;
		DamageDealt1 = 0;
		DamageTaken0 = 0;
		DamageTaken1 = 0;
		bMatchCheated = false;
		CheatUses.Reset();
		LastCheatLine.Reset();
		bCheatInvincible = false;
		bCheatInstantBuild = false;
		bCheatNoSupply = false;
		bCheatReveal = false;
		bCheatNoCooldown = false;
		bCompetitive = false;
		bGeneralPowers = false;
		bPowerStartGranted = false;
		PowerPoints0 = 0;
		PowerPoints1 = 0;
		ArtilleryCd0 = 0;
		ArtilleryCd1 = 0;
		ReconCd0 = 0;
		ReconCd1 = 0;
		RepairCd0 = 0;
		RepairCd1 = 0;
		ScheduledPowers.Reset();
		AiOpener = 0;
		bAiOpenerChosen = false;
		bScoutSawEnemy = false;
		bAiRetreating = false;
		Totals0 = FKodTeamTotals();
		Totals1 = FKodTeamTotals();
	};

	auto Living = [this](int32 TeamId) -> int32
	{
		int32 Alive = 0;
		TArray<int32> Keys;
		States.GetKeys(Keys);
		Keys.Sort();
		for (int32 Key : Keys)
		{
			const FKodSimEntityState* State = States.Find(Key);
			if (State && State->TeamId == TeamId && State->Health > 0.f)
			{
				++Alive;
			}
		}
		return Alive;
	};

	auto SpawnMarine = [this](int32 TeamId, const FVector& Pos) -> FKodEntityId
	{
		FKodEntityId Id;
		Id.Value = NextId++;
		FKodSimEntityState State;
		State.Id = Id;
		State.DefinitionId = FName(KodWardenPaths::Id_Ranger);
		State.TeamId = TeamId;
		State.Position = Pos;
		State.bMobile = true;
		State.bBiological = true;
		State.bInfantry = true;
		State.CargoSize = 1;
		State.MaxHealth = 120.f;
		State.Health = 120.f;
		State.ArmorPoints = 0;
		State.MoveSpeed = 450.f;
		State.SightRadiusUU = 1600;
		State.WeaponDamage = 12.f;
		State.WeaponRange = 900.f;
		State.WeaponCooldownTicks = 13;
		State.WeaponCooldownSeconds = 13.f * KodBuildTicks::SimDt;
		State.WeaponBackswingTicks = 2;
		State.AcceptanceRadius = 50.f;
		State.BodyRadiusUU = 34;
		if (const FKodUnitBalanceRow* Row = UnitTable ? UnitTable->FindRow<FKodUnitBalanceRow>(State.DefinitionId, TEXT("KodTest"), false) : nullptr)
		{
			State.MaxHealth = static_cast<float>(FMath::Max(1, Row->MaxHealth));
			State.Health = State.MaxHealth;
			State.ArmorPoints = Row->Armor;
			State.MoveSpeed = static_cast<float>(Row->MoveSpeed);
			State.SightRadiusUU = Row->Sight;
		}
		if (const FKodWeaponBalanceRow* Weapon = WeaponTable ? WeaponTable->FindRow<FKodWeaponBalanceRow>(FName(KodWardenPaths::Id_RangerRifle), TEXT("KodTest"), false) : nullptr)
		{
			State.WeaponDamage = static_cast<float>(Weapon->Damage);
			State.WeaponRange = static_cast<float>(Weapon->Range);
			State.WeaponCooldownTicks = Weapon->CooldownTicks;
			State.WeaponCooldownSeconds = static_cast<float>(Weapon->CooldownTicks) * KodBuildTicks::SimDt;
		}
		QuantizePose(State);
		States.Add(Id.Value, State);
		PrevPositions.Add(Id.Value, State.Position);
		PrevYaws.Add(Id.Value, State.YawDegrees);
		return Id;
	};

	const FString Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("KodTests"));
	IFileManager::Get().MakeDirectory(*Dir, true);

	for (int32 Run = 0; Run < Count; ++Run)
	{
		Wipe();
		SetPlayableWorldBox(FBox(FVector(-4000.f, -4000.f, -200.f), FVector(4000.f, 4000.f, 800.f)));
		const int32 Shift = FMath::Abs(UseSeed) % 5;
		TArray<FKodEntityId> Team0;
		TArray<FKodEntityId> Team1;
		if (bBunker)
		{
			FKodEntityId BunkerId;
			BunkerId.Value = NextId++;
			FKodStructureState Structure;
			Structure.Id = BunkerId;
			Structure.TeamId = 0;
			Structure.DefinitionId = FName(KodWardenPaths::Id_Bunker);
			Structure.Position = FVector::ZeroVector;
			Structure.bComplete = true;
			Structure.CargoCap = 4;
			Structure.MaxHealth = 400.f;
			Structure.FootprintX = 3;
			Structure.FootprintY = 3;
			if (const FKodStructureBalanceRow* Row = StructureTable ? StructureTable->FindRow<FKodStructureBalanceRow>(Structure.DefinitionId, TEXT("KodTest"), false) : nullptr)
			{
				Structure.MaxHealth = static_cast<float>(FMath::Max(1, Row->MaxHealth));
				Structure.CargoCap = Row->CargoCap;
			}
			Structures.Add(BunkerId.Value, Structure);
			FKodSimEntityState Shell;
			Shell.Id = BunkerId;
			Shell.DefinitionId = Structure.DefinitionId;
			Shell.TeamId = 0;
			Shell.Position = Structure.Position;
			Shell.bMobile = false;
			Shell.MaxHealth = Structure.MaxHealth;
			Shell.Health = Structure.MaxHealth;
			QuantizePose(Shell);
			States.Add(BunkerId.Value, Shell);
			for (int32 Index = 0; Index < 4; ++Index)
			{
				FKodEntityId Marine = SpawnMarine(0, FVector(80.f, static_cast<float>(Index * 40), 0.f));
				LoadUnitIntoBunker(Marine, BunkerId);
				Team0.Add(Marine);
			}
			for (int32 Index = 0; Index < 10; ++Index)
			{
				const FVector Pos(500.f + static_cast<float>(Shift), static_cast<float>(Index * 80 - 360), 0.f);
				Team1.Add(SpawnMarine(1, Pos));
			}
			for (const FKodEntityId& Attacker : Team1)
			{
				IssueAttackForced(Attacker, BunkerId, true);
			}
		}
		else
		{
			for (int32 Index = 0; Index < 10; ++Index)
			{
				const float Y = static_cast<float>(Index * 80 - 360);
				Team0.Add(SpawnMarine(0, FVector(0.f, Y, 0.f)));
				Team1.Add(SpawnMarine(1, FVector(400.f + static_cast<float>(Shift), Y, 0.f)));
			}
			for (int32 Index = 0; Index < Team0.Num() && Index < Team1.Num(); ++Index)
			{
				IssueAttackForced(Team0[Index], Team1[Index], true);
				IssueAttackForced(Team1[Index], Team0[Index], true);
				if (bStim)
				{
					UseAbility(Team0[Index], FName(KodWardenPaths::Id_CombatStim));
				}
			}
		}

		int32 Guard = 0;
		while (Guard < 16 * 120 && Living(0) > 0 && Living(1) > 0)
		{
			StepSim(KodBuildTicks::SimDt);
			++Guard;
		}

		FKodTestResult Result;
		Result.Scenario = Name;
		Result.Seed = UseSeed;
		Result.Run = Run;
		Result.Survivors0 = Living(0);
		Result.Survivors1 = Living(1);
		if (Result.Survivors0 > 0 && Result.Survivors1 == 0)
		{
			Result.Winner = 0;
		}
		else if (Result.Survivors1 > 0 && Result.Survivors0 == 0)
		{
			Result.Winner = 1;
		}
		else
		{
			Result.Winner = -1;
		}
		const int32 Ticks = FMath::Max(1, static_cast<int32>(SimTickIndex));
		Result.TimeSeconds = Ticks / 16;
		Result.Dealt0 = DamageDealt0;
		Result.Dealt1 = DamageDealt1;
		Result.Taken0 = DamageTaken0;
		Result.Taken1 = DamageTaken1;
		Result.Dps0 = DamageDealt0 * 16 / Ticks;
		Result.Dps1 = DamageDealt1 * 16 / Ticks;
		Result.Hash = ComputeIdleHash();
		TestResults.Add(Result);

		const FString Stem = FString::Printf(TEXT("%s_%d_%d"), *Name, UseSeed, Run);
		const FString CsvPath = FPaths::Combine(Dir, Stem + TEXT(".csv"));
		const FString JsonPath = FPaths::Combine(Dir, Stem + TEXT(".json"));
		const FString Header = TEXT("scenario,seed,run,winner,time,survivors0,survivors1,dealt0,dealt1,taken0,taken1,dps0,dps1,hash\n");
		const FString Row = FString::Printf(TEXT("%s,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%s\n"),
			*Result.Scenario,
			Result.Seed,
			Result.Run,
			Result.Winner,
			Result.TimeSeconds,
			Result.Survivors0,
			Result.Survivors1,
			Result.Dealt0,
			Result.Dealt1,
			Result.Taken0,
			Result.Taken1,
			Result.Dps0,
			Result.Dps1,
			*Result.Hash);
		FFileHelper::SaveStringToFile(Header + Row, *CsvPath);
		const FString Json = FString::Printf(
			TEXT("{\"scenario\":\"%s\",\"seed\":%d,\"run\":%d,\"winner\":%d,\"time\":%d,\"survivors\":[%d,%d],\"dealt\":[%d,%d],\"taken\":[%d,%d],\"dps\":[%d,%d],\"hash\":\"%s\"}\n"),
			*Result.Scenario,
			Result.Seed,
			Result.Run,
			Result.Winner,
			Result.TimeSeconds,
			Result.Survivors0,
			Result.Survivors1,
			Result.Dealt0,
			Result.Dealt1,
			Result.Taken0,
			Result.Taken1,
			Result.Dps0,
			Result.Dps1,
			*Result.Hash);
		FFileHelper::SaveStringToFile(Json, *JsonPath);
		UE_LOG(LogTemp, Log, TEXT("KodTest Scenario=%s Run=%d Seed=%d Winner=%d Time=%d Hash=%s"),
			*Result.Scenario,
			Result.Run,
			Result.Seed,
			Result.Winner,
			Result.TimeSeconds,
			*Result.Hash);
	}

	Wipe();
	Entities = TestStash.Entities;
	PendingSpawns = TestStash.PendingSpawns;
	PendingBuildingSpawns = TestStash.PendingBuildingSpawns;
	PrevPositions = TestStash.PrevPositions;
	PrevYaws = TestStash.PrevYaws;
	NextId = TestStash.NextId;
	TimeAccumulator = TestStash.TimeAccumulator;
	bSimPaused = TestStash.bSimPaused;
	bMenuMode = TestStash.bMenuMode;
	bHasPlayable = TestStash.bHasPlayable;
	PlayMinX = TestStash.PlayMinX;
	PlayMinY = TestStash.PlayMinY;
	PlayMaxX = TestStash.PlayMaxX;
	PlayMaxY = TestStash.PlayMaxY;
	StoredPaths = TestStashPaths;
	NextPathId = TestStashNextPathId;
	ImportSimBlob(TestStash.Blob);
	NextId = FMath::Max(NextId, TestStash.NextId);
	bTestIsolation = false;
	return true;
}
