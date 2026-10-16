#include "Sim/KodSimSubsystem.h"
#include "Warden/KodWardenPaths.h"
#include "Game/KodPlayerController.h"
#include "Game/KodPlayerState.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Hash/CityHash.h"
#include "UObject/UObjectArray.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Crc.h"

void UKodSimSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	NextId = 1;
	Entities.Reset();
	States.Reset();
	PrevPositions.Reset();
	PrevYaws.Reset();
	Banks.Reset();
	ResourceNodes.Reset();
	DropOffs.Reset();
	Supply.Reset();
	Structures.Reset();
	TrainSpecs.Reset();
	BuildSpecs.Reset();
	AbilitySpecs.Reset();
	UpgradeSpecs.Reset();
	TeamTech.Reset();
	TeamFacingYaw.Reset();
	UnitAbilities.Reset();
	PendingSpawns.Reset();
	PendingBuildingSpawns.Reset();
	DeferredCommands.Reset();
	DeferredSequence = 0;
	bSoakDeterminismDone = false;
	bSoakMemoryAnchored = false;
	SoakMemoryAnchor = 0;
	SoakMemoryAnchorElapsed = 0.f;
	SoakPulseAt = 0.f;
	bAutoplaySquadSent = false;
	AutoplayOrderMarks.Reset();
	bHasPlayable = false;
	TimeAccumulator = 0.f;
	SimTickIndex = 0;
	bAiEnabled = true;
	AiDifficulty = 1;
	bAiSeeded = false;
	bAiScoutSent = false;
	bHasEnemyStart = false;
	EnemyStartX = 0;
	EnemyStartY = 0;
	AiWavesLaunched = 0;
	AiNextWaveTick = 0;
	MatchOutcome = EKodMatchOutcome::None;
	bTeam0HadStructure = false;
	bTeam1HadStructure = false;
	AcquireGrid.Reset();
	MeleeAttackersByTarget.Reset();
	StoredPaths.Reset();
	TeamVision.Reset();
	SeenStructures.Reset();
	SeenUnits.Reset();
	bAiCheatVision = false;
	VisionOriginX = 0;
	VisionOriginY = 0;
	VisionWidth = 0;
	VisionHeight = 0;
	NextPathId = 1;
	Samples0.Reset();
	Samples1.Reset();
	OlderSamples0.Reset();
	OlderSamples1.Reset();
	Totals0 = FKodTeamTotals();
	Totals1 = FKodTeamTotals();
	ScoreTracker.Reset();
	bMatchCheated = false;
	bCompetitive = false;
	MatchMode = EKodMatchMode::OfflineSkirmish;
	bOnlineSession = false;
	CheatUses.Reset();
	LastCheatLine.Reset();
	bGeneralPowers = true;
	bPowerStartGranted = false;
	AttackAlerts.Reset();
	PlayerFaction = 0;
	bFaultlineTraining = false;
	bFaultlineWaveSent = false;
	bFaultlineWaveErrorLogged = false;
	FaultlineOutpostCount = 0;
	FaultlineWaveX = 0;
	FaultlineWaveY = 0;
	FaultlineWaveZ = 0;
	FaultlineHomeX = 0;
	FaultlineHomeY = 0;
	FaultlineHomeZ = 0;
	ResetFaultlineScript();
	PowerPoints0 = 0;
	PowerPoints1 = 0;
	ArtilleryCd0 = 0;
	ArtilleryCd1 = 0;
	ReconCd0 = 0;
	ReconCd1 = 0;
	RepairCd0 = 0;
	RepairCd1 = 0;
	ScheduledPowers.Reset();
	RampCells.Reset();
	HighGroundCells.Reset();
	DressBlockCells.Reset();
	VisionBlockCells.Reset();
	DestructFoots.Reset();
	LayoutSockets.Reset();
	bCheatInvincible = false;
	bCheatInstantBuild = false;
	bCheatNoSupply = false;
	bCheatReveal = false;
	bCheatNoCooldown = false;
	DamageDealt0 = 0;
	DamageDealt1 = 0;
	DamageTaken0 = 0;
	DamageTaken1 = 0;
	AiOpener = 0;
	bAiOpenerChosen = false;
	bScoutSawEnemy = false;
	bAiRetreating = false;
	bAiMemoryWritten = false;
	bTestIsolation = false;
	bLoadRespawn = false;
	LoadRespawnTicks = 0;
	OnJuiceText.Clear();
	CheatTable = nullptr;
	EnsureBalanceTables();
	EnsureCheatPhrases();
	SimHz = KodBuildTicks::SimHz;
	MaxCatchUpSteps = KodBuildTicks::MaxCatchUpSteps;
	SetTeamBank(0, StartingJadeite, StartingLuminene);
}

void UKodSimSubsystem::Deinitialize()
{
	OnUnitFired.Clear();
	OnUnitHit.Clear();
	OnUnitKilled.Clear();
	OnTrainCompleted.Clear();
	OnCue.Clear();
	OnJuiceText.Clear();
	Entities.Reset();
	States.Reset();
	Banks.Reset();
	ResourceNodes.Reset();
	DropOffs.Reset();
	Supply.Reset();
	Structures.Reset();
	TrainSpecs.Reset();
	BuildSpecs.Reset();
	AbilitySpecs.Reset();
	UpgradeSpecs.Reset();
	TeamTech.Reset();
	TeamFacingYaw.Reset();
	UnitAbilities.Reset();
	PendingSpawns.Reset();
	PendingBuildingSpawns.Reset();
	AcquireGrid.Reset();
	MeleeAttackersByTarget.Reset();
	StoredPaths.Reset();
	TeamVision.Reset();
	SeenStructures.Reset();
	SeenUnits.Reset();
	bAiCheatVision = false;
	VisionOriginX = 0;
	VisionOriginY = 0;
	VisionWidth = 0;
	VisionHeight = 0;
	bHasPlayable = false;
	PrevPositions.Reset();
	PrevYaws.Reset();
	AutoplayOrderMarks.Reset();
	bAiEnabled = true;
	AiDifficulty = 1;
	bAiSeeded = false;
	bAiScoutSent = false;
	bHasEnemyStart = false;
	EnemyStartX = 0;
	EnemyStartY = 0;
	AiWavesLaunched = 0;
	AiNextWaveTick = 0;
	MatchOutcome = EKodMatchOutcome::None;
	bTeam0HadStructure = false;
	bTeam1HadStructure = false;
	PlayerFaction = 0;
	bFaultlineTraining = false;
	bFaultlineWaveSent = false;
	bFaultlineWaveErrorLogged = false;
	FaultlineOutpostCount = 0;
	FaultlineWaveX = 0;
	FaultlineWaveY = 0;
	FaultlineWaveZ = 0;
	FaultlineHomeX = 0;
	FaultlineHomeY = 0;
	FaultlineHomeZ = 0;
	ResetFaultlineScript();
	Super::Deinitialize();
}

TStatId UKodSimSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UKodSimSubsystem, STATGROUP_Tickables);
}

void UKodSimSubsystem::Tick(float DeltaTime)
{
	if (!bSoakParsed)
	{
		bSoakParsed = true;
		int32 Seconds = 0;
		if (FParse::Value(FCommandLine::Get(), TEXT("-KodSoakSeconds="), Seconds)
			|| FParse::Value(FCommandLine::Get(), TEXT("KodSoakSeconds="), Seconds))
		{
			SoakSeconds = FMath::Max(0, Seconds);
		}
		int32 AutoplayValue = 0;
		const FString CommandLine = FCommandLine::Get();
		const bool bAutoplayFlag = (FParse::Value(FCommandLine::Get(), TEXT("kod.Autoplay="), AutoplayValue) && AutoplayValue != 0)
			|| CommandLine.Contains(TEXT("kod.Autoplay 1"))
			|| CommandLine.Contains(TEXT("kod.Autoplay=1"));
		if (bAutoplayFlag)
		{
			SetPlayerAutoplay(true);
		}
		else if (SoakSeconds > 0)
		{
			UE_LOG(LogTemp, Log, TEXT("KodSoak Autoplay off. Pass -kod.Autoplay=1. -ExecCmds is read here too when it contains kod.Autoplay 1."));
		}
		if (SoakSeconds > 0)
		{
			UE_LOG(LogTemp, Log, TEXT("KodSoak Start Seconds=%d Autoplay=%d"), SoakSeconds, bPlayerAutoplay ? 1 : 0);
		}
	}
	if (bLoadRespawn)
	{
		if (PendingSpawns.Num() == 0 && PendingBuildingSpawns.Num() == 0)
		{
			++LoadRespawnTicks;
			if (LoadRespawnTicks >= 3)
			{
				bLoadRespawn = false;
				LoadRespawnTicks = 0;
			}
		}
		else
		{
			LoadRespawnTicks = 0;
		}
	}
	if (bSimPaused)
	{
		SyncActorPresentation(1.f);
		return;
	}
	const float FixedDt = (SimHz > 0) ? (1.f / static_cast<float>(SimHz)) : KodBuildTicks::SimDt;
	TimeAccumulator += DeltaTime;

	int32 Steps = 0;
	while (TimeAccumulator >= FixedDt && Steps < MaxCatchUpSteps)
	{
		// Snapshot previous poses for presentation lerp
		PrevPositions.Reset();
		PrevYaws.Reset();
		for (const TPair<int32, FKodSimEntityState>& Pair : States)
		{
			PrevPositions.Add(Pair.Key, Pair.Value.Position);
			PrevYaws.Add(Pair.Key, Pair.Value.YawDegrees);
		}

		StepSim(FixedDt);
		TimeAccumulator -= FixedDt;
		++Steps;
		++SimTickIndex;
	}

	// Avoid spiral: drop excess accumulated time beyond one step
	if (TimeAccumulator > FixedDt)
	{
		TimeAccumulator = FixedDt;
	}

	const float Alpha = FixedDt > 0.f ? FMath::Clamp(TimeAccumulator / FixedDt, 0.f, 1.f) : 1.f;
	SyncActorPresentation(Alpha);
	PollSoak(DeltaTime);
}

void UKodSimSubsystem::AdvanceSimTicks(int32 Ticks)
{
	const int32 Count = FMath::Max(0, Ticks);
	const float FixedDt = (SimHz > 0) ? (1.f / static_cast<float>(SimHz)) : KodBuildTicks::SimDt;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		StepSim(FixedDt);
		++SimTickIndex;
	}
}

void UKodSimSubsystem::PollSoak(float DeltaTime)
{
	if (SoakSeconds <= 0)
	{
		return;
	}
	if (bPlayerAutoplay && !AreCheatsAllowed())
	{
		SetPlayerAutoplay(false);
	}
	if (!bSoakDeterminismDone)
	{
		bSoakDeterminismDone = true;
		if (!RunAutoplayDeterminismCheck(32))
		{
			RequestSoakExit(2);
			return;
		}
	}
	SoakElapsed += FMath::Max(0.f, DeltaTime);
	if (SoakHashTick != SimTickIndex)
	{
		SoakHashTick = SimTickIndex;
		SoakHash = ComputeIdleHash();
	}
	if (SoakElapsed - SoakPulseAt >= 60.f)
	{
		SoakPulseAt = SoakElapsed;
		LogMatchPulse(TEXT("Pulse"));
	}
	if (SoakElapsed - SoakMemoryMark >= 30.f)
	{
		SoakMemoryMark = SoakElapsed;
		const FPlatformMemoryStats Stats = FPlatformMemory::GetStats();
		UE_LOG(LogTemp, Log, TEXT("KodSoak Memory UsedPhysical=%llu Tick=%lld Hash=%s"),
			static_cast<unsigned long long>(Stats.UsedPhysical),
			static_cast<long long>(SimTickIndex),
			*SoakHash);
		if (SoakElapsed >= 120.f)
		{
			FKodSoakMemPoint Point;
			Point.TimeSeconds = FMath::FloorToInt(SoakElapsed);
			Point.Bytes = Stats.UsedPhysical;
			SoakMemPoints.Add(Point);
			while (SoakMemPoints.Num() > 8)
			{
				SoakMemPoints.RemoveAt(0);
			}
			TArray<int32> Times;
			TArray<uint64> Bytes;
			Times.Reserve(SoakMemPoints.Num());
			Bytes.Reserve(SoakMemPoints.Num());
			for (const FKodSoakMemPoint& Sample : SoakMemPoints)
			{
				Times.Add(Sample.TimeSeconds);
				Bytes.Add(Sample.Bytes);
			}
			if (SoakMemorySlopeOverBudget(Times.GetData(), Bytes.GetData(), Times.Num()))
			{
				UE_LOG(LogTemp, Error, TEXT("KodSoak MemoryBudget Samples=%d Span=%d Now=%llu"),
					Times.Num(),
					Times.Last() - Times[0],
					static_cast<unsigned long long>(Stats.UsedPhysical));
				LogMatchPulse(TEXT("Memory"));
				RequestSoakExit(2);
				return;
			}
		}
	}
	if (SoakElapsed >= static_cast<float>(SoakSeconds))
	{
		UE_LOG(LogTemp, Log, TEXT("KodSoak Done Seconds=%d Tick=%lld"), SoakSeconds, static_cast<long long>(SimTickIndex));
		LogMatchPulse(TEXT("Final"));
		RequestSoakExit(0);
	}
}

bool UKodSimSubsystem::FaultlineWaveDue(int32 Phase, int32 LivingOutposts, int64 Tick, int32 DelayTicks, bool bAutoplay)
{
	if (Phase != 0)
	{
		return false;
	}
	if (LivingOutposts <= 0)
	{
		return true;
	}
	return bAutoplay && DelayTicks > 0 && Tick >= static_cast<int64>(DelayTicks);
}

bool UKodSimSubsystem::FaultlineWaveShouldRetreat(int32 LiveHp, int32 StartHp, int32 PlayerCombat, int64 ElapsedTicks)
{
	constexpr int64 MaxFieldTicks = 90 * 16;
	const bool bBroken = StartHp > 0 && LiveHp < StartHp && LiveHp * 1000 <= StartHp * 300;
	const bool bWon = PlayerCombat <= 0;
	const bool bTimedOut = ElapsedTicks >= MaxFieldTicks;
	return bBroken || bWon || bTimedOut;
}

bool UKodSimSubsystem::FaultlineWaveRearmDue(int64 Tick, int64 EndedTick, int32 DelayTicks)
{
	const int64 Gap = static_cast<int64>(FMath::Max(DelayTicks, 16 * 20));
	return Tick >= EndedTick + Gap;
}

bool UKodSimSubsystem::FaultlineLaneEndRetarget(
	bool bReachedLaneEnd,
	bool bContact,
	float FromX,
	float FromY,
	int32 UnitId,
	float UnitX,
	float UnitY,
	int32 StructureId,
	float StructureX,
	float StructureY,
	int32& OutId,
	float& OutX,
	float& OutY)
{
	if (!bReachedLaneEnd || bContact)
	{
		return false;
	}
	const bool bUnit = UnitId > 0;
	const bool bStructure = StructureId > 0;
	if (!bUnit && !bStructure)
	{
		return false;
	}
	auto Dist = [FromX, FromY](float X, float Y)
	{
		const float DX = X - FromX;
		const float DY = Y - FromY;
		return DX * DX + DY * DY;
	};
	const bool bUseUnit = bUnit && (!bStructure || Dist(UnitX, UnitY) <= Dist(StructureX, StructureY));
	if (bUseUnit)
	{
		OutId = UnitId;
		OutX = UnitX;
		OutY = UnitY;
	}
	else
	{
		OutId = StructureId;
		OutX = StructureX;
		OutY = StructureY;
	}
	return true;
}

void UKodSimSubsystem::SetFaultlineLane(const TArray<FVector>& Points)
{
	FaultlineLanePoints.Reset();
	WaveLaneCursor = 0;
	FaultlineWaveFocusId = 0;
	AutoplayLaneCursor = -1;
	bool bPlaced = false;
	for (const FVector& Point : Points)
	{
		if (FMath::Abs(Point.X) > 1.f || FMath::Abs(Point.Y) > 1.f)
		{
			bPlaced = true;
			break;
		}
	}
	if (!bPlaced)
	{
		UE_LOG(LogTemp, Warning, TEXT("KodFaultline Lane rejected Points=%d; all zero, attacking outposts directly"), Points.Num());
		return;
	}
	FaultlineLanePoints.Reserve(Points.Num());
	for (const FVector& Point : Points)
	{
		if (FMath::Abs(Point.X) <= 1.f && FMath::Abs(Point.Y) <= 1.f)
		{
			continue;
		}
		FVector Kept = Point;
		QuantizeResourcePosition(Kept);
		FaultlineLanePoints.Add(Kept);
	}
	if (FaultlineLanePoints.Num() > 0)
	{
		const FVector& First = FaultlineLanePoints[0];
		const FVector& Last = FaultlineLanePoints.Last();
		UE_LOG(LogTemp, Log, TEXT("KodFaultline Lane Points=%d First=%.0f,%.0f Last=%.0f,%.0f"),
			FaultlineLanePoints.Num(),
			First.X,
			First.Y,
			Last.X,
			Last.Y);
	}
}

bool UKodSimSubsystem::NextLanePoint(int32 Direction, int32& Cursor, const FVector& From, FVector& OutPoint) const
{
	const int32 Count = FaultlineLanePoints.Num();
	if (Count <= 0)
	{
		return false;
	}
	// Count means the wave already left the lane for its attack. -2 means autoplay did.
	if (Direction >= 0 && Cursor >= Count)
	{
		return false;
	}
	if (Direction < 0 && Cursor == -2)
	{
		return false;
	}
	if (Cursor < 0)
	{
		Cursor = Direction < 0 ? Count - 1 : 0;
	}
	Cursor = FMath::Clamp(Cursor, 0, Count - 1);
	const bool bReached = FVector::DistSquared2D(From, FaultlineLanePoints[Cursor]) <= FMath::Square(350.f);
	if (bReached)
	{
		if (Direction < 0)
		{
			if (Cursor <= 0)
			{
				Cursor = -2;
				return false;
			}
			--Cursor;
		}
		else if (Cursor >= Count - 1)
		{
			Cursor = Count;
			return false;
		}
		else
		{
			++Cursor;
		}
	}
	if (!FaultlineLanePoints.IsValidIndex(Cursor))
	{
		return false;
	}
	OutPoint = FaultlineLanePoints[Cursor];
	return FMath::Abs(OutPoint.X) > 1.f || FMath::Abs(OutPoint.Y) > 1.f;
}

bool UKodSimSubsystem::SoakMemoryOverBudget(uint64 AnchorBytes, uint64 NowBytes, int32 ElapsedSeconds)
{
	if (ElapsedSeconds <= 0 || NowBytes <= AnchorBytes)
	{
		return false;
	}
	const uint64 Growth = NowBytes - AnchorBytes;
	const uint64 Allowed = (static_cast<uint64>(ElapsedSeconds) * 50ull * 1024ull * 1024ull) / 300ull;
	return Growth > Allowed;
}

bool UKodSimSubsystem::SoakMemorySlopeOverBudget(const int32* TimeSeconds, const uint64* Bytes, int32 Count)
{
	if (TimeSeconds == nullptr || Bytes == nullptr || Count < 3)
	{
		return false;
	}
	const int32 Span = TimeSeconds[Count - 1] - TimeSeconds[0];
	if (Span < 120)
	{
		return false;
	}
	int64 SumT = 0;
	int64 SumT2 = 0;
	int64 SumY = 0;
	int64 SumTY = 0;
	const int64 Samples = Count;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const int64 Time = static_cast<int64>(TimeSeconds[Index] - TimeSeconds[0]);
		const int64 Kilo = static_cast<int64>(Bytes[Index] / 1024ull);
		SumT += Time;
		SumT2 += Time * Time;
		SumY += Kilo;
		SumTY += Time * Kilo;
	}
	const int64 Denom = Samples * SumT2 - SumT * SumT;
	if (Denom <= 0)
	{
		return false;
	}
	const int64 Numer = Samples * SumTY - SumT * SumY;
	if (Numer <= 0)
	{
		return false;
	}
	return Numer * 300 > Denom * 50 * 1024;
}

int32 UKodSimSubsystem::CountLivingOutposts() const
{
	int32 Living = 0;
	TArray<int32> StructureKeys;
	Structures.GetKeys(StructureKeys);
	StructureKeys.Sort();
	for (int32 StructureKey : StructureKeys)
	{
		const FKodStructureState* Found = Structures.Find(StructureKey);
		if (!Found || Found->TeamId != 1 || Found->DefinitionId != FName(TEXT("KodEnemyOutpost")))
		{
			continue;
		}
		const FKodSimEntityState* Combat = States.Find(StructureKey);
		if (Combat && Combat->Health > 0.f)
		{
			++Living;
		}
	}
	return Living;
}

void UKodSimSubsystem::OrderFaultlineWave()
{
	FVector Point(static_cast<float>(FaultlineHomeX), static_cast<float>(FaultlineHomeY), static_cast<float>(FaultlineHomeZ));
	FKodEntityId Target;
	bool bFound = false;
	int64 Best = 0;
	TArray<int32> StructureKeys;
	Structures.GetKeys(StructureKeys);
	StructureKeys.Sort();
	for (int32 Key : StructureKeys)
	{
		const FKodStructureState* Structure = Structures.Find(Key);
		if (!Structure || Structure->TeamId != 0)
		{
			continue;
		}
		const FKodSimEntityState* Combat = States.Find(Structure->Id.Value);
		if (Combat && Combat->Health <= 0.f)
		{
			continue;
		}
		if (Structure->DefinitionId == FName(KodWardenPaths::Id_CommandCenter))
		{
			Target = Structure->Id;
			Point = Structure->Position;
			bFound = true;
			break;
		}
		const int64 DX = static_cast<int64>(FMath::RoundToInt(Structure->Position.X));
		const int64 DY = static_cast<int64>(FMath::RoundToInt(Structure->Position.Y));
		const int64 Dist = DX * DX + DY * DY;
		if (!bFound || Dist < Best)
		{
			bFound = true;
			Best = Dist;
			Target = Structure->Id;
			Point = Structure->Position;
		}
	}
	if (!Target.IsValid())
	{
		TArray<int32> Keys;
		States.GetKeys(Keys);
		Keys.Sort();
		for (int32 Key : Keys)
		{
			const FKodSimEntityState* State = States.Find(Key);
			if (!State || State->TeamId != 0 || State->Health <= 0.f || !State->bMobile)
			{
				continue;
			}
			const int64 DX = static_cast<int64>(FMath::RoundToInt(State->Position.X));
			const int64 DY = static_cast<int64>(FMath::RoundToInt(State->Position.Y));
			const int64 Dist = DX * DX + DY * DY;
			if (Target.IsValid() && Dist >= Best)
			{
				continue;
			}
			Best = Dist;
			Target = State->Id;
			Point = State->Position;
		}
	}
	FVector MarchFrom(static_cast<float>(FaultlineWaveX), static_cast<float>(FaultlineWaveY), static_cast<float>(FaultlineWaveZ));
	if (FaultlineLanePoints.IsValidIndex(WaveLaneCursor))
	{
		float BestLead = 0.f;
		bool bLead = false;
		const FVector Mark = FaultlineLanePoints[WaveLaneCursor];
		for (const FKodScriptedUnit& Row : ScriptedUnits)
		{
			if (Row.Role != 2)
			{
				continue;
			}
			const FKodSimEntityState* State = States.Find(Row.Id);
			if (!State || State->Health <= 0.f)
			{
				continue;
			}
			const float Dist = FVector::DistSquared2D(State->Position, Mark);
			if (!bLead || Dist < BestLead)
			{
				bLead = true;
				BestLead = Dist;
				MarchFrom = State->Position;
			}
		}
	}
	FVector LanePoint;
	bool bRetarget = false;
	const bool bOnLane = NextLanePoint(1, WaveLaneCursor, MarchFrom, LanePoint);
	if (bOnLane)
	{
		Target = FKodEntityId();
		Point = LanePoint;
	}
	else
	{
		int32 WaveHp = 0;
		for (const FKodScriptedUnit& Row : ScriptedUnits)
		{
			if (Row.Role != 2)
			{
				continue;
			}
			const FKodSimEntityState* State = States.Find(Row.Id);
			if (State && State->Health > 0.f)
			{
				WaveHp += FMath::RoundToInt(State->Health);
			}
		}
		const bool bContact = FaultlineWaveStartHp > 0 && WaveHp < FaultlineWaveStartHp;
		if (const FKodSimEntityState* Focus = States.Find(FaultlineWaveFocusId))
		{
			if (Focus->Health <= 0.f || Focus->TeamId != 0)
			{
				FaultlineWaveFocusId = 0;
			}
		}
		else
		{
			FaultlineWaveFocusId = 0;
		}
		if (!bContact && FaultlineWaveFocusId == 0)
		{
			int32 NearCombat = 0;
			float NearCombatX = 0.f;
			float NearCombatY = 0.f;
			float BestCombat = 0.f;
			int32 NearStructure = 0;
			float NearStructureX = 0.f;
			float NearStructureY = 0.f;
			float BestStructure = 0.f;
			TArray<int32> Keys;
			States.GetKeys(Keys);
			for (int32 Key : Keys)
			{
				const FKodSimEntityState* State = States.Find(Key);
				if (!State || State->TeamId != 0 || State->Health <= 0.f)
				{
					continue;
				}
				const float DX = State->Position.X - MarchFrom.X;
				const float DY = State->Position.Y - MarchFrom.Y;
				const float Dist = DX * DX + DY * DY;
				if (State->bMobile && !State->bCanGather)
				{
					if (NearCombat == 0 || Dist < BestCombat)
					{
						NearCombat = State->Id.Value;
						NearCombatX = State->Position.X;
						NearCombatY = State->Position.Y;
						BestCombat = Dist;
					}
				}
				else if (!State->bMobile && Structures.Contains(Key))
				{
					if (NearStructure == 0 || Dist < BestStructure)
					{
						NearStructure = State->Id.Value;
						NearStructureX = State->Position.X;
						NearStructureY = State->Position.Y;
						BestStructure = Dist;
					}
				}
			}
			int32 RetargetId = 0;
			float RetargetX = 0.f;
			float RetargetY = 0.f;
			if (FaultlineLaneEndRetarget(
				true,
				false,
				MarchFrom.X,
				MarchFrom.Y,
				NearCombat,
				NearCombatX,
				NearCombatY,
				NearStructure,
				NearStructureX,
				NearStructureY,
				RetargetId,
				RetargetX,
				RetargetY))
			{
				FaultlineWaveFocusId = RetargetId;
			}
		}
		if (!bContact && FaultlineWaveFocusId > 0)
		{
			if (const FKodSimEntityState* Focus = States.Find(FaultlineWaveFocusId))
			{
				Target = Focus->Id;
				Point = Focus->Position;
				bRetarget = true;
			}
		}
	}
	int32 Ordered = 0;
	for (FKodScriptedUnit& Row : ScriptedUnits)
	{
		if (Row.Role != 2)
		{
			continue;
		}
		FKodSimEntityState* State = States.Find(Row.Id);
		if (!State || State->Health <= 0.f)
		{
			continue;
		}
		if (Target.IsValid() && State->Order == EKodSimOrderType::Attack && State->AttackTarget == Target)
		{
			continue;
		}
		if (State->Order == EKodSimOrderType::AttackMove
			&& FVector::DistSquared2D(State->MoveTarget, Point) <= FMath::Square(128.f))
		{
			continue;
		}
		// Already fighting along the lane. Restating the march every tick was the order flood.
		if (!Target.IsValid() && State->Order == EKodSimOrderType::Attack && State->AttackTarget.IsValid())
		{
			continue;
		}
		if (Target.IsValid())
		{
			DeferAttack(State->Id, Target);
		}
		else
		{
			DeferAttackMove(State->Id, Point);
		}
		Row.bOrdered = 1;
		++Ordered;
	}
	if (Ordered > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("KodFaultline Wave Order Count=%d Target=%d X=%.0f Y=%.0f Retarget=%d"),
			Ordered,
			Target.Value,
			Point.X,
			Point.Y,
			bRetarget ? 1 : 0);
	}
}

void UKodSimSubsystem::LogMatchPulse(const TCHAR* Reason)
{
	int32 Combat = 0;
	int32 Workers = 0;
	int32 Living = 0;
	TArray<int32> Keys;
	States.GetKeys(Keys);
	Keys.Sort();
	for (int32 Key : Keys)
	{
		const FKodSimEntityState* State = States.Find(Key);
		if (!State || State->Health <= 0.f)
		{
			continue;
		}
		++Living;
		if (State->TeamId != 0)
		{
			continue;
		}
		if (State->bCanGather)
		{
			++Workers;
		}
		else if (State->bMobile)
		{
			++Combat;
		}
	}
	const int32 Objects = GUObjectArray.GetObjectArrayNum();
	int32 Actors = 0;
	if (const UWorld* World = GetWorld())
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			++Actors;
		}
	}
	int32 SeenCount = 0;
	for (const TPair<int32, TArray<FKodSeenUnit>>& Pair : SeenUnits)
	{
		SeenCount += Pair.Value.Num();
	}
	const int32 StateCount = Keys.Num();
	const int32 Paths = StoredPaths.Num();
	const int32 Samples = Samples0.Num() + Samples1.Num() + OlderSamples0.Num() + OlderSamples1.Num();
	const FPlatformMemoryStats Stats = FPlatformMemory::GetStats();
	UE_LOG(LogTemp, Log, TEXT("KodSoak %s Tick=%lld Phase=%d Outposts=%d Kills0=%d Kills1=%d Combat=%d Workers=%d Living=%d WaveSent=%d"),
		Reason,
		static_cast<long long>(SimTickIndex),
		static_cast<int32>(FaultlineWavePhase),
		CountLivingOutposts(),
		Totals0.Killed,
		Totals1.Killed,
		Combat,
		Workers,
		Living,
		bFaultlineWaveSent ? 1 : 0);
	UE_LOG(LogTemp, Log, TEXT("KodSoak Grow Used=%llu dUsed=%lld Objects=%d dObj=%d Actors=%d States=%d dStates=%d Paths=%d dPaths=%d Samples=%d dSamples=%d Alerts=%d Pending=%d Deferred=%d Seen=%d"),
		static_cast<unsigned long long>(Stats.UsedPhysical),
		static_cast<long long>(Stats.UsedPhysical) - static_cast<long long>(SoakMemoryAnchor),
		Objects,
		Objects - SoakLastObjects,
		Actors,
		StateCount,
		StateCount - SoakLastStates,
		Paths,
		Paths - SoakLastPaths,
		Samples,
		Samples - SoakLastSamples,
		AttackAlerts.Num(),
		PendingSpawns.Num(),
		DeferredCommands.Num(),
		SeenCount);
	SoakLastObjects = Objects;
	SoakLastStates = StateCount;
	SoakLastPaths = Paths;
	SoakLastSamples = Samples;
}

void UKodSimSubsystem::DeferOrder(FKodEntityId Id, EKodSimOrderType Kind, FVector Point, FKodEntityId Target, bool bQueued)
{
	FKodDeferredCommand Command;
	Command.Kind = 1;
	Command.Order = static_cast<uint8>(Kind);
	Command.bQueued = bQueued ? 1 : 0;
	Command.Sequence = DeferredSequence++;
	Command.Unit = Id.Value;
	Command.Target = Target.Value;
	Command.X = FMath::RoundToInt(Point.X);
	Command.Y = FMath::RoundToInt(Point.Y);
	Command.Z = FMath::RoundToInt(Point.Z);
	DeferredCommands.Add(Command);
}

void UKodSimSubsystem::DeferMove(FKodEntityId Id, FVector Point)
{
	DeferOrder(Id, EKodSimOrderType::Move, Point, FKodEntityId(), false);
}

void UKodSimSubsystem::DeferAttackMove(FKodEntityId Id, FVector Point)
{
	DeferOrder(Id, EKodSimOrderType::AttackMove, Point, FKodEntityId(), false);
}

void UKodSimSubsystem::DeferAttack(FKodEntityId Id, FKodEntityId Target)
{
	DeferOrder(Id, EKodSimOrderType::Attack, FVector::ZeroVector, Target, false);
}

void UKodSimSubsystem::DeferGather(FKodEntityId Id, FKodEntityId Node)
{
	FKodDeferredCommand Command;
	Command.Kind = 3;
	Command.Sequence = DeferredSequence++;
	Command.Unit = Id.Value;
	Command.Target = Node.Value;
	DeferredCommands.Add(Command);
}

void UKodSimSubsystem::DeferTrain(FKodEntityId StructureId, FName UnitId)
{
	FKodDeferredCommand Command;
	Command.Kind = 2;
	Command.Sequence = DeferredSequence++;
	Command.Unit = StructureId.Value;
	Command.TrainId = UnitId;
	DeferredCommands.Add(Command);
}

void UKodSimSubsystem::DeferScriptAdopt(FKodEntityId Id, const FKodPendingUnitSpawn& Request)
{
	FKodDeferredCommand Command;
	Command.Kind = 0;
	Command.Sequence = DeferredSequence++;
	Command.Unit = Id.Value;
	Command.Spawn = Request;
	DeferredCommands.Add(Command);
}

void UKodSimSubsystem::FlushDeferredCommands()
{
	if (DeferredCommands.Num() == 0)
	{
		return;
	}
	DeferredCommands.Sort([](const FKodDeferredCommand& A, const FKodDeferredCommand& B)
	{
		return A.Sequence < B.Sequence;
	});
	const TArray<FKodDeferredCommand> Batch = DeferredCommands;
	DeferredCommands.Reset();
	for (const FKodDeferredCommand& Command : Batch)
	{
		FKodEntityId Unit;
		Unit.Value = Command.Unit;
		FKodEntityId Target;
		Target.Value = Command.Target;
		const FVector Point(static_cast<float>(Command.X), static_cast<float>(Command.Y), static_cast<float>(Command.Z));
		if (Command.Kind == 0)
		{
			AdoptScriptedSpawn(Unit, Command.Spawn);
		}
		else if (Command.Kind == 2)
		{
			EnqueueTrain(Unit, Command.TrainId);
		}
		else if (Command.Kind == 3)
		{
			IssueGather(Unit, Target);
		}
		else
		{
			IssueOrder(Unit, static_cast<EKodSimOrderType>(Command.Order), Point, Target, Command.bQueued != 0);
		}
	}
}

void UKodSimSubsystem::RequestSoakExit(uint8 Code)
{
	if (Code != 0)
	{
		GIsCriticalError = true;
	}
	FPlatformMisc::RequestExitWithStatus(true, Code, TEXT("KodSoak"));
}

bool UKodSimSubsystem::RunAutoplayDeterminismCheck(int32 Ticks)
{
	const int32 Count = FMath::Clamp(Ticks, 1, 256);
	const bool bSavedAutoplay = bPlayerAutoplay;
	const int32 SavedNextId = NextId;
	const int64 SavedPlayerWave = PlayerNextWaveTick;
	DeferredCommands.Reset();
	const TArray<uint8> Baseline = ExportSimBlob();
	auto Restore = [this, &Baseline, SavedNextId, SavedPlayerWave]()
	{
		DeferredCommands.Reset();
		ImportSimBlob(Baseline);
		NextId = SavedNextId;
		PlayerNextWaveTick = SavedPlayerWave;
	};
	SetPlayerAutoplay(true);
	AdvanceSimTicks(Count);
	const FString FirstHash = ComputeIdleHash();
	const int64 FirstTick = SimTickIndex;
	Restore();
	SetPlayerAutoplay(true);
	AdvanceSimTicks(Count);
	const FString SecondHash = ComputeIdleHash();
	const int64 SecondTick = SimTickIndex;
	const bool bSame = FirstHash == SecondHash && FirstTick == SecondTick;
	Restore();
	if (bSavedAutoplay)
	{
		SetPlayerAutoplay(true);
	}
	else
	{
		bPlayerAutoplay = false;
	}
	if (!bSame)
	{
		UE_LOG(LogTemp, Error, TEXT("KodSoak Desync Tick=%lld"), static_cast<long long>(FirstTick));
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("KodSoak Determinism Ticks=%d Hash=%s"), Count, *FirstHash);
	}
	return bSame;
}

void UKodSimSubsystem::StepSim(float FixedDt)
{
	FlushDeferredCommands();
	RebuildAcquireGrid();
	// Stamp before orders so the first tick can see units that just spawned.
	// The stamp after separation matches the poses presentation hides against.
	StepVision();

	TArray<int32> Keys;
	States.GetKeys(Keys);
	Keys.Sort();

	for (int32 Key : Keys)
	{
		FKodSimEntityState* State = States.Find(Key);
		if (!State)
		{
			continue;
		}
		if (bCheatNoCooldown && State->TeamId == 0)
		{
			State->CooldownTicksRemaining = 0;
			State->AbilityCooldownTicks = 0;
		}
		if (State->CooldownTicksRemaining > 0)
		{
			--State->CooldownTicksRemaining;
			State->CooldownRemaining = static_cast<float>(State->CooldownTicksRemaining) * FixedDt;
		}
		else
		{
			State->CooldownRemaining = 0.f;
		}
		if (State->bMobile
			&& State->Order == EKodSimOrderType::None
			&& State->GatherPhase == EKodGatherPhase::None
			&& IsCellBlocked(KodBuildGrid::CellOf(State->Position.X), KodBuildGrid::CellOf(State->Position.Y), State->TeamId, true))
		{
			SnapOutOfFootprint(*State);
			QuantizePose(*State);
		}

		if (State->InsideBunker.IsValid())
		{
			continue;
		}

		if (State->GatherPhase != EKodGatherPhase::None || State->Order == EKodSimOrderType::Gather)
		{
			// Do not use State after this if a later unregister rehashes. Gather does not unregister workers.
			StepEntityGather(*State, FixedDt);
			continue;
		}

		switch (State->Order)
		{
		case EKodSimOrderType::Move:
			StepEntityMove(*State, FixedDt);
			break;
		case EKodSimOrderType::Attack:
			StepEntityAttack(*State, FixedDt);
			break;
		case EKodSimOrderType::AttackMove:
			StepAttackMove(*State, FixedDt);
			break;
		case EKodSimOrderType::Hold:
			StepHold(*State, FixedDt);
			break;
		case EKodSimOrderType::Patrol:
			StepPatrol(*State, FixedDt);
			break;
		case EKodSimOrderType::Follow:
			StepFollow(*State, FixedDt, false);
			break;
		case EKodSimOrderType::Guard:
			StepFollow(*State, FixedDt, true);
			break;
		default:
			// Idle: auto-acquire or hold a quantized pose. An explicit Move is not this branch.
			if (!TryAutoAcquire(*State, FixedDt))
			{
				QuantizePose(*State);
			}
			break;
		}
	}
	StepSeparation();
	StepVision();
	StepProduction();
	StepConstruction();
	StepRepair();
	StepAbilities();
	EvaluateMatch();
	StepAI();
	StepTech();
	StepFaultlineWave();
	StepOrbital();
	StepLiftoff();
	StepPowers();
	StepExposed();
	FlushDeferredCommands();
	RecordMatchSample();
	RecordScoreSample();
}

void UKodSimSubsystem::SetEntityGroundPosition(FKodEntityId Id, const FVector& Position)
{
	FKodSimEntityState* State = States.Find(Id.Value);
	if (!State)
	{
		return;
	}
	State->Position = Position;
	QuantizePose(*State);
	PrevPositions.Add(Id.Value, State->Position);
}

void UKodSimSubsystem::SetPlayerFaction(int32 Faction)
{
	PlayerFaction = Faction == 1 ? 1 : 0;
}

void UKodSimSubsystem::ResetFaultlineScript()
{
	FaultlineWavePhase = 0;
	FaultlineWaveDueTick = 0;
	FaultlineWaveStartTick = 0;
	FaultlineRetreatTick = 0;
	FaultlineWaveStartHp = 0;
	FaultlineWaveExpected = 0;
	bFaultlineGuardsSent = 0;
	FaultlineOutpostX = 0;
	FaultlineOutpostY = 0;
	FaultlineOutpostZ = 0;
	FaultlineLaneX = 0;
	FaultlineLaneY = 0;
	FaultlineLaneZ = 0;
	bFaultlineHasLane = 0;
	FaultlineLanePoints.Reset();
	WaveLaneCursor = 0;
	FaultlineWaveFocusId = 0;
	AutoplayLaneCursor = -1;
	FaultlineBarracksX = 0;
	FaultlineBarracksY = 0;
	FaultlineBarracksZ = 0;
	FaultlineDelayTicks = 0;
	FaultlineRetreatTicks = 0;
	FaultlineHpPermille = 1000;
	FaultlineGuardFighters = 0;
	FaultlineGuardBlades = 0;
	FaultlineGuardWorkers = 0;
	FaultlineWaveFighters = 0;
	FaultlineWaveBlades = 0;
	FaultlineFighterLeashUU = 0;
	FaultlineFighterAggroUU = 0;
	FaultlineBladeLeashUU = 0;
	FaultlineBladeAggroUU = 0;
	FaultlineFighterHp = 1;
	FaultlineFighterDamage = 0;
	FaultlineFighterRangeUU = 0;
	FaultlineFighterSpeed = 0;
	FaultlineFighterArmor = 0;
	FaultlineFighterCooldownTicks = 1;
	FaultlineFighterRegenMilli = 0;
	FaultlineBladeHp = 1;
	FaultlineBladeDamage = 0;
	FaultlineBladeRangeUU = 0;
	FaultlineBladeSpeed = 0;
	FaultlineBladeArmor = 0;
	FaultlineBladeCooldownTicks = 1;
	FaultlineBladeRegenMilli = 0;
	FaultlineWorkerHp = 1;
	FaultlineWorkerSpeed = 0;
	FaultlineWorkerRegenMilli = 0;
	ScriptedUnits.Reset();
}

void UKodSimSubsystem::SetStructureVitals(FKodEntityId Id, int32 MaxHp, int32 Armor, int32 RegenMilliPerSec)
{
	const int32 Hp = FMath::Max(1, MaxHp);
	if (FKodStructureState* Structure = Structures.Find(Id.Value))
	{
		Structure->MaxHealth = static_cast<float>(Hp);
	}
	if (FKodSimEntityState* State = States.Find(Id.Value))
	{
		State->MaxHealth = static_cast<float>(Hp);
		State->Health = static_cast<float>(Hp);
		State->ArmorPoints = FMath::Max(0, Armor);
	}
	FKodScriptedUnit Row;
	Row.Id = Id.Value;
	Row.Role = 4;
	Row.RegenMilliPerSec = FMath::Max(0, RegenMilliPerSec);
	Row.LastHealth = Hp;
	ScriptedUnits.RemoveAll([Id](const FKodScriptedUnit& Existing) { return Existing.Id == Id.Value; });
	ScriptedUnits.Add(Row);
	ScriptedUnits.Sort([](const FKodScriptedUnit& A, const FKodScriptedUnit& B) { return A.Id < B.Id; });
}

void UKodSimSubsystem::AdoptScriptedSpawn(FKodEntityId Id, const FKodPendingUnitSpawn& Request)
{
	if (Request.ScriptRole == 0)
	{
		return;
	}
	FKodSimEntityState* State = States.Find(Id.Value);
	if (!State)
	{
		return;
	}
	if (Request.ScriptHealth > 0)
	{
		State->MaxHealth = static_cast<float>(Request.ScriptHealth);
		State->Health = static_cast<float>(Request.ScriptHealth);
	}
	State->MoveSpeed = static_cast<float>(FMath::Max(0, Request.ScriptSpeed));
	State->ArmorPoints = FMath::Max(0, Request.ScriptArmor);
	if (Request.ScriptRole == 3)
	{
		State->WeaponDamage = 0.f;
		State->bCanGather = false;
		State->bBiological = true;
		State->bInfantry = false;
	}
	else
	{
		State->WeaponDamage = static_cast<float>(FMath::Max(0, Request.ScriptDamage));
		State->WeaponRange = static_cast<float>(FMath::Max(0, Request.ScriptRangeUU));
		State->WeaponCooldownTicks = FMath::Max(1, Request.ScriptCooldownTicks);
		State->bBiological = true;
		State->bInfantry = true;
	}
	FKodScriptedUnit Row;
	Row.Id = Id.Value;
	Row.Role = Request.ScriptRole;
	Row.HomeX = Request.ScriptHomeX;
	Row.HomeY = Request.ScriptHomeY;
	Row.LeashUU = Request.ScriptLeashUU;
	Row.AggroUU = Request.ScriptAggroUU;
	Row.RegenMilliPerSec = FMath::Max(0, Request.ScriptRegenMilli);
	Row.LastHealth = FMath::RoundToInt(State->Health);
	ScriptedUnits.RemoveAll([Id](const FKodScriptedUnit& Existing) { return Existing.Id == Id.Value; });
	ScriptedUnits.Add(Row);
	ScriptedUnits.Sort([](const FKodScriptedUnit& A, const FKodScriptedUnit& B) { return A.Id < B.Id; });
}

void UKodSimSubsystem::EnqueueFaultlineUnit(
	FName UnitId,
	int32 Slot,
	uint8 Role,
	int32 Health,
	int32 Damage,
	int32 RangeUU,
	int32 Speed,
	int32 Armor,
	int32 CooldownTicks,
	int32 RegenMilli,
	int32 HomeX,
	int32 HomeY,
	int32 LeashUU,
	int32 AggroUU,
	const FVector& Rally,
	bool bAttackMove,
	FName Tag)
{
	FKodPendingUnitSpawn Spawn;
	Spawn.UnitId = UnitId;
	Spawn.TeamId = 1;
	Spawn.Location = FVector(
		static_cast<float>(HomeX + (Slot - 1) * 160),
		static_cast<float>(HomeY + (Role == 2 ? 220 : 80)),
		static_cast<float>(FaultlineOutpostZ));
	if (Role == 2)
	{
		Spawn.Location.Z = static_cast<float>(FaultlineWaveZ);
	}
	Spawn.bAttackMove = bAttackMove;
	Spawn.RallyPoint = Rally;
	Spawn.ActorTag = Tag;
	Spawn.ScriptRole = Role;
	Spawn.ScriptHealth = Health;
	Spawn.ScriptDamage = Damage;
	Spawn.ScriptRangeUU = RangeUU;
	Spawn.ScriptSpeed = Speed;
	Spawn.ScriptArmor = Armor;
	Spawn.ScriptCooldownTicks = CooldownTicks;
	Spawn.ScriptRegenMilli = RegenMilli;
	Spawn.ScriptHomeX = HomeX;
	Spawn.ScriptHomeY = HomeY;
	Spawn.ScriptLeashUU = LeashUU;
	Spawn.ScriptAggroUU = AggroUU;
	PendingSpawns.Add(Spawn);
}

void UKodSimSubsystem::DespawnFaultlineRole(uint8 Role)
{
	TArray<int32> DropIds;
	for (const FKodScriptedUnit& Row : ScriptedUnits)
	{
		if (Row.Role == Role)
		{
			DropIds.Add(Row.Id);
		}
	}
	for (int32 DropId : DropIds)
	{
		FKodEntityId Drop;
		Drop.Value = DropId;
		AActor* Actor = ResolveEntity(Drop);
		UnregisterEntity(Drop);
		if (Actor && !Actor->IsActorBeingDestroyed())
		{
			Actor->Destroy();
		}
	}
	ScriptedUnits.RemoveAll([Role](const FKodScriptedUnit& Row) { return Row.Role == Role; });
}

void UKodSimSubsystem::ArmFaultlineScript(const FKodFaultlineScript& Script)
{
	bFaultlineTraining = true;
	TArray<FKodScriptedUnit> StructureRegen;
	for (const FKodScriptedUnit& Row : ScriptedUnits)
	{
		if (Row.Role == 4)
		{
			StructureRegen.Add(Row);
		}
	}
	ResetFaultlineScript();
	ScriptedUnits = MoveTemp(StructureRegen);
	bFaultlineWaveSent = false;
	bFaultlineWaveErrorLogged = false;
	FaultlineOutpostCount = FMath::Max(0, Script.OutpostCount);
	FaultlineWaveX = Script.SpawnX;
	FaultlineWaveY = Script.SpawnY;
	FaultlineWaveZ = Script.SpawnZ;
	FaultlineOutpostX = Script.OutpostX;
	FaultlineOutpostY = Script.OutpostY;
	FaultlineOutpostZ = Script.OutpostZ;
	FaultlineHomeX = Script.HomeX;
	FaultlineHomeY = Script.HomeY;
	FaultlineHomeZ = Script.HomeZ;
	FaultlineLaneX = Script.LaneX;
	FaultlineLaneY = Script.LaneY;
	FaultlineLaneZ = Script.LaneZ;
	bFaultlineHasLane = Script.bHasLane;
	FaultlineDelayTicks = FMath::Max(0, Script.DelayTicks);
	FaultlineRetreatTicks = FMath::Max(1, Script.RetreatTicks);
	FaultlineHpPermille = FMath::Clamp(Script.HpPermille, 1, 1000);
	FaultlineGuardFighters = FMath::Max(0, Script.GuardFighters);
	FaultlineGuardBlades = FMath::Max(0, Script.GuardBlades);
	FaultlineGuardWorkers = FMath::Max(0, Script.GuardWorkers);
	FaultlineWaveFighters = FMath::Max(0, Script.WaveFighters);
	FaultlineWaveBlades = FMath::Max(0, Script.WaveBlades);
	FaultlineFighterLeashUU = FMath::Max(0, Script.FighterLeashUU);
	FaultlineFighterAggroUU = FMath::Max(0, Script.FighterAggroUU);
	FaultlineBladeLeashUU = FMath::Max(0, Script.BladeLeashUU);
	FaultlineBladeAggroUU = FMath::Max(0, Script.BladeAggroUU);
	FaultlineFighterHp = FMath::Max(1, Script.FighterHp);
	FaultlineFighterDamage = FMath::Max(0, Script.FighterDamage);
	FaultlineFighterRangeUU = FMath::Max(0, Script.FighterRangeUU);
	FaultlineFighterSpeed = FMath::Max(0, Script.FighterSpeed);
	FaultlineFighterArmor = FMath::Max(0, Script.FighterArmor);
	FaultlineFighterCooldownTicks = FMath::Max(1, Script.FighterCooldownTicks);
	FaultlineFighterRegenMilli = FMath::Max(0, Script.FighterRegenMilli);
	FaultlineBladeHp = FMath::Max(1, Script.BladeHp);
	FaultlineBladeDamage = FMath::Max(0, Script.BladeDamage);
	FaultlineBladeRangeUU = FMath::Max(0, Script.BladeRangeUU);
	FaultlineBladeSpeed = FMath::Max(0, Script.BladeSpeed);
	FaultlineBladeArmor = FMath::Max(0, Script.BladeArmor);
	FaultlineBladeCooldownTicks = FMath::Max(1, Script.BladeCooldownTicks);
	FaultlineBladeRegenMilli = FMath::Max(0, Script.BladeRegenMilli);
	FaultlineWorkerHp = FMath::Max(1, Script.WorkerHp);
	FaultlineWorkerSpeed = FMath::Max(0, Script.WorkerSpeed);
	FaultlineWorkerRegenMilli = FMath::Max(0, Script.WorkerRegenMilli);
	if (FaultlineOutpostCount <= 0)
	{
		bFaultlineWaveErrorLogged = true;
		UE_LOG(LogTemp, Error, TEXT("KodFaultline Wave skipped: no outposts registered"));
	}
}

void UKodSimSubsystem::StepFaultlineWave()
{
	if (!bFaultlineTraining)
	{
		return;
	}

	for (FKodScriptedUnit& Row : ScriptedUnits)
	{
		FKodSimEntityState* State = States.Find(Row.Id);
		if (!State || State->Health <= 0.f || Row.RegenMilliPerSec <= 0)
		{
			continue;
		}
		if (State->Health >= State->MaxHealth)
		{
			continue;
		}
		Row.RegenAcc += Row.RegenMilliPerSec;
		while (Row.RegenAcc >= 16000 && State->Health < State->MaxHealth)
		{
			Row.RegenAcc -= 16000;
			State->Health = FMath::Min(State->MaxHealth, State->Health + 1.f);
		}
	}

	if (FaultlineOutpostCount <= 0)
	{
		if (!bFaultlineWaveErrorLogged)
		{
			bFaultlineWaveErrorLogged = true;
			UE_LOG(LogTemp, Error, TEXT("KodFaultline Wave skipped: no outposts registered"));
		}
		return;
	}

	auto Scaled = [this](int32 Value) -> int32
	{
		if (Value <= 0)
		{
			return 0;
		}
		return FMath::Max(1, (Value * FaultlineHpPermille + 500) / 1000);
	};

	if (!bFaultlineGuardsSent)
	{
		bFaultlineGuardsSent = 1;
		for (int32 Slot = 0; Slot < FaultlineGuardFighters; ++Slot)
		{
			EnqueueFaultlineUnit(
				FName(KodWardenPaths::Id_ScnFighter), Slot, 1,
				Scaled(FaultlineFighterHp), Scaled(FaultlineFighterDamage), FaultlineFighterRangeUU,
				FaultlineFighterSpeed, FaultlineFighterArmor, FaultlineFighterCooldownTicks, FaultlineFighterRegenMilli,
				FaultlineOutpostX, FaultlineOutpostY, FaultlineFighterLeashUU, FaultlineFighterAggroUU,
				FVector::ZeroVector, false, FName(TEXT("KodOutpostGuard")));
		}
		for (int32 Slot = 0; Slot < FaultlineGuardBlades; ++Slot)
		{
			EnqueueFaultlineUnit(
				FName(KodWardenPaths::Id_ScnTwinblade), Slot + FaultlineGuardFighters, 1,
				Scaled(FaultlineBladeHp), Scaled(FaultlineBladeDamage), FaultlineBladeRangeUU,
				FaultlineBladeSpeed, FaultlineBladeArmor, FaultlineBladeCooldownTicks, FaultlineBladeRegenMilli,
				FaultlineOutpostX, FaultlineOutpostY, FaultlineBladeLeashUU, FaultlineBladeAggroUU,
				FVector::ZeroVector, false, FName(TEXT("KodOutpostGuard")));
		}
		for (int32 Slot = 0; Slot < FaultlineGuardWorkers; ++Slot)
		{
			EnqueueFaultlineUnit(
				FName(KodWardenPaths::Id_ScnWorker), Slot, 3,
				Scaled(FaultlineWorkerHp), 0, 0,
				FaultlineWorkerSpeed, 0, 1, FaultlineWorkerRegenMilli,
				FaultlineOutpostX, FaultlineOutpostY, 0, 0,
				FVector::ZeroVector, false, FName(TEXT("KodOutpostWorker")));
		}
		UE_LOG(LogTemp, Log, TEXT("KodFaultline Guards Fighters=%d Blades=%d Workers=%d"),
			FaultlineGuardFighters, FaultlineGuardBlades, FaultlineGuardWorkers);
	}

	for (FKodScriptedUnit& Row : ScriptedUnits)
	{
		FKodSimEntityState* State = States.Find(Row.Id);
		if (!State || State->Health <= 0.f || Row.Role == 2 || Row.Role == 4)
		{
			continue;
		}
		const FVector Home(static_cast<float>(Row.HomeX), static_cast<float>(Row.HomeY), State->Position.Z);
		const float DistHome = FVector::Dist2D(State->Position, Home);
		const int32 HpNow = FMath::RoundToInt(State->Health);
		if (Row.Role == 3)
		{
			if (HpNow < Row.LastHealth)
			{
				FVector Away(180.f, 180.f, 0.f);
				float Best = 0.f;
				bool bSeen = false;
				TArray<int32> Keys;
				States.GetKeys(Keys);
				Keys.Sort();
				for (int32 Key : Keys)
				{
					const FKodSimEntityState* Other = States.Find(Key);
					if (!Other || Other->Health <= 0.f || Other->TeamId == State->TeamId)
					{
						continue;
					}
					const float Dist = FVector::DistSquared2D(State->Position, Other->Position);
					if (!bSeen || Dist < Best)
					{
						bSeen = true;
						Best = Dist;
						Away = State->Position - Other->Position;
						Away.Z = 0.f;
					}
				}
				if (Away.SizeSquared2D() < 1.f)
				{
					Away = FVector(1.f, 0.f, 0.f);
				}
				Away = Away.GetSafeNormal2D() * 600.f;
				DeferMove(State->Id, Home + Away);
			}
			Row.LastHealth = HpNow;
			continue;
		}
		if (Row.LeashUU > 0 && DistHome > static_cast<float>(Row.LeashUU))
		{
			if (State->Order != EKodSimOrderType::Move)
			{
				DeferMove(State->Id, Home);
			}
			Row.LastHealth = HpNow;
			continue;
		}
		FKodEntityId Nearest;
		float NearestDist = 0.f;
		bool bNearest = false;
		if (Row.AggroUU > 0)
		{
			TArray<int32> Keys;
			States.GetKeys(Keys);
			Keys.Sort();
			for (int32 Key : Keys)
			{
				const FKodSimEntityState* Other = States.Find(Key);
				if (!Other || Other->Health <= 0.f || Other->TeamId == State->TeamId || Other->InsideBunker.IsValid())
				{
					continue;
				}
				const float Dist = FVector::DistSquared2D(State->Position, Other->Position);
				if (Dist > FMath::Square(static_cast<float>(Row.AggroUU)))
				{
					continue;
				}
				if (!bNearest || Dist < NearestDist)
				{
					bNearest = true;
					NearestDist = Dist;
					Nearest.Value = Key;
				}
			}
		}
		if (bNearest)
		{
			if (State->Order != EKodSimOrderType::Attack || State->AttackTarget != Nearest)
			{
				DeferAttack(State->Id, Nearest);
			}
		}
		else if (DistHome > 180.f && State->Order != EKodSimOrderType::Move)
		{
			DeferMove(State->Id, Home);
		}
		Row.LastHealth = HpNow;
	}

	if (FaultlineWavePhase == 0)
	{
		const int32 LivingOutposts = CountLivingOutposts();
		if (FaultlineWaveDue(FaultlineWavePhase, LivingOutposts, SimTickIndex, FaultlineDelayTicks, bPlayerAutoplay))
		{
			const bool bOutpostsDown = LivingOutposts <= 0;
			FaultlineWavePhase = 1;
			FaultlineWaveDueTick = bOutpostsDown
				? SimTickIndex + static_cast<int64>(FaultlineDelayTicks)
				: SimTickIndex;
			UE_LOG(LogTemp, Log, TEXT("KodFaultline Wave Scheduled DueTick=%lld Reason=%s Outposts=%d"),
				static_cast<long long>(FaultlineWaveDueTick),
				bOutpostsDown ? TEXT("Outposts") : TEXT("Autoplay"),
				LivingOutposts);
		}
		return;
	}
	if (FaultlineWavePhase == 1)
	{
		if (SimTickIndex < FaultlineWaveDueTick)
		{
			return;
		}
		FaultlineBarracksX = FaultlineHomeX;
		FaultlineBarracksY = FaultlineHomeY;
		FaultlineBarracksZ = FaultlineHomeZ;
		float BestBarracks = 0.f;
		bool bBarracks = false;
		TArray<int32> StructureKeys;
		Structures.GetKeys(StructureKeys);
		StructureKeys.Sort();
		for (int32 StructureKey : StructureKeys)
		{
			const FKodStructureState* Found = Structures.Find(StructureKey);
			if (!Found || Found->TeamId != 0 || Found->DefinitionId != FName(KodWardenPaths::Id_Barracks))
			{
				continue;
			}
			const float Dist = FVector::DistSquared2D(
				FVector(static_cast<float>(FaultlineWaveX), static_cast<float>(FaultlineWaveY), 0.f),
				Found->Position);
			if (!bBarracks || Dist < BestBarracks)
			{
				bBarracks = true;
				BestBarracks = Dist;
				FaultlineBarracksX = FMath::RoundToInt(Found->Position.X);
				FaultlineBarracksY = FMath::RoundToInt(Found->Position.Y);
				FaultlineBarracksZ = FMath::RoundToInt(Found->Position.Z);
			}
		}
		const FVector Rally = bFaultlineHasLane
			? FVector(static_cast<float>(FaultlineLaneX), static_cast<float>(FaultlineLaneY), static_cast<float>(FaultlineLaneZ))
			: FVector(static_cast<float>(FaultlineBarracksX), static_cast<float>(FaultlineBarracksY), static_cast<float>(FaultlineBarracksZ));
		for (int32 Slot = 0; Slot < FaultlineWaveFighters; ++Slot)
		{
			EnqueueFaultlineUnit(
				FName(KodWardenPaths::Id_ScnFighter), Slot, 2,
				Scaled(FaultlineFighterHp), Scaled(FaultlineFighterDamage), FaultlineFighterRangeUU,
				FaultlineFighterSpeed, FaultlineFighterArmor, FaultlineFighterCooldownTicks, FaultlineFighterRegenMilli,
				FaultlineWaveX, FaultlineWaveY, 0, 0,
				Rally, true, FName(TEXT("KodCounterWave")));
		}
		for (int32 Slot = 0; Slot < FaultlineWaveBlades; ++Slot)
		{
			EnqueueFaultlineUnit(
				FName(KodWardenPaths::Id_ScnTwinblade), Slot + FaultlineWaveFighters, 2,
				Scaled(FaultlineBladeHp), Scaled(FaultlineBladeDamage), FaultlineBladeRangeUU,
				FaultlineBladeSpeed, FaultlineBladeArmor, FaultlineBladeCooldownTicks, FaultlineBladeRegenMilli,
				FaultlineWaveX, FaultlineWaveY, 0, 0,
				Rally, true, FName(TEXT("KodCounterWave")));
		}
		bFaultlineWaveSent = true;
		FaultlineWaveFocusId = 0;
		FaultlineWavePhase = 2;
		FaultlineWaveStartTick = SimTickIndex;
		FaultlineWaveExpected = FaultlineWaveFighters + FaultlineWaveBlades;
		FaultlineWaveStartHp = Scaled(FaultlineFighterHp) * FaultlineWaveFighters + Scaled(FaultlineBladeHp) * FaultlineWaveBlades;
		UE_LOG(LogTemp, Log, TEXT("KodFaultline Wave Start Fighters=%d Blades=%d Hp=%d Lane=%d Tick=%lld"),
			FaultlineWaveFighters, FaultlineWaveBlades, FaultlineWaveStartHp, bFaultlineHasLane ? 1 : 0,
			static_cast<long long>(SimTickIndex));
		return;
	}

	if (FaultlineWavePhase == 2)
	{
		OrderFaultlineWave();
		int32 LiveHp = 0;
		int32 Seen = 0;
		for (const FKodScriptedUnit& Row : ScriptedUnits)
		{
			if (Row.Role != 2)
			{
				continue;
			}
			const FKodSimEntityState* State = States.Find(Row.Id);
			if (!State)
			{
				continue;
			}
			++Seen;
			if (State->Health > 0.f)
			{
				LiveHp += FMath::RoundToInt(State->Health);
			}
		}
		int32 PlayerCombat = 0;
		{
			TArray<int32> Keys;
			States.GetKeys(Keys);
			for (int32 Key : Keys)
			{
				const FKodSimEntityState* State = States.Find(Key);
				if (State && State->TeamId == 0 && State->Health > 0.f && State->bMobile && !State->bCanGather)
				{
					++PlayerCombat;
				}
			}
		}
		const int64 Elapsed = SimTickIndex - FaultlineWaveStartTick;
		const bool bBroken = FaultlineWaveStartHp > 0 && LiveHp < FaultlineWaveStartHp && LiveHp * 1000 <= FaultlineWaveStartHp * 300;
		if (Seen > 0 && FaultlineWaveShouldRetreat(LiveHp, FaultlineWaveStartHp, PlayerCombat, Elapsed))
		{
			FaultlineWavePhase = 3;
			FaultlineRetreatTick = SimTickIndex;
			const FVector Back(static_cast<float>(FaultlineWaveX), static_cast<float>(FaultlineWaveY), static_cast<float>(FaultlineWaveZ));
			for (const FKodScriptedUnit& Row : ScriptedUnits)
			{
				if (Row.Role != 2)
				{
					continue;
				}
				const FKodSimEntityState* State = States.Find(Row.Id);
				if (State && State->Health > 0.f)
				{
					DeferMove(State->Id, Back);
				}
			}
			const TCHAR* Reason = bBroken ? TEXT("Hp") : (PlayerCombat <= 0 ? TEXT("Win") : TEXT("Time"));
			UE_LOG(LogTemp, Log, TEXT("KodFaultline Wave Retreat Reason=%s LiveHp=%d StartHp=%d Combat=%d Elapsed=%lld"),
				Reason,
				LiveHp,
				FaultlineWaveStartHp,
				PlayerCombat,
				static_cast<long long>(Elapsed));
		}
		return;
	}
	if (FaultlineWavePhase == 3)
	{
		bool bHome = true;
		int32 Living = 0;
		for (const FKodScriptedUnit& Row : ScriptedUnits)
		{
			if (Row.Role != 2)
			{
				continue;
			}
			const FKodSimEntityState* State = States.Find(Row.Id);
			if (!State || State->Health <= 0.f)
			{
				continue;
			}
			++Living;
			const float Dist = FVector::Dist2D(
				State->Position,
				FVector(static_cast<float>(FaultlineWaveX), static_cast<float>(FaultlineWaveY), State->Position.Z));
			if (Dist > 250.f)
			{
				bHome = false;
			}
		}
		if (Living == 0 || bHome || SimTickIndex >= FaultlineRetreatTick + 8 * 16)
		{
			DespawnFaultlineRole(2);
			FaultlineWavePhase = 4;
			FaultlineRetreatTick = SimTickIndex;
			ClearStoredPaths(TEXT("WaveEnd"));
			UE_LOG(LogTemp, Log, TEXT("KodFaultline Wave End Tick=%lld Kills0=%d Kills1=%d Living=%d"),
				static_cast<long long>(SimTickIndex),
				Totals0.Killed,
				Totals1.Killed,
				Living);
		}
		return;
	}
	if (FaultlineWavePhase == 4)
	{
		if (!FaultlineWaveRearmDue(SimTickIndex, FaultlineRetreatTick, FaultlineDelayTicks))
		{
			return;
		}
		bFaultlineWaveSent = false;
		FaultlineWavePhase = 0;
		UE_LOG(LogTemp, Log, TEXT("KodFaultline Wave Rearm Tick=%lld WaveSent=0"), static_cast<long long>(SimTickIndex));
	}
}

void UKodSimSubsystem::StepEntityMove(FKodSimEntityState& State, float FixedDt)
{
	if (!State.bMobile || State.MoveSpeed <= 0.f)
	{
		State.Order = EKodSimOrderType::None;
		QuantizePose(State);
		return;
	}

	const bool bFollowPath = State.PathId != 0;
	const FVector Goal = GetSteeredGoal(State);
	const bool bPathDone = State.PathId == 0;
	const FVector ToTarget = Goal - State.Position;
	const float DistSq = ToTarget.SizeSquared2D();
	const float Accept = FMath::Max(1.f, State.AcceptanceRadius * 0.5f);
	if (!bFollowPath || bPathDone)
	{
		if (DistSq <= FMath::Square(Accept))
		{
			State.Position.X = State.MoveTarget.X;
			State.Position.Y = State.MoveTarget.Y;
			if (State.Order == EKodSimOrderType::Patrol)
			{
				const FVector Next = State.PatrolAnchor;
				State.PatrolAnchor = State.MoveTarget;
				State.MoveTarget = Next;
				State.PathId = FMath::Max(0, PlanMovePath(State.Position, State.MoveTarget, State.TeamId));
				State.PathCursor = 0;
				QuantizePose(State);
				return;
			}
			if (State.Order == EKodSimOrderType::Move)
			{
				CompleteCurrentOrder(State, FixedDt);
				return;
			}
			QuantizePose(State);
			return;
		}
	}
	else if (DistSq <= FMath::Square(Accept))
	{
		QuantizePose(State);
		return;
	}

	FVector Dir = ToTarget;
	Dir.Z = 0.f;
	Dir.Normalize();
	FVector Delta = SteerAroundBuildings(State, Dir * (EffectiveMoveSpeed(State) * FixedDt));
	if (Delta.SizeSquared2D() > KINDA_SMALL_NUMBER)
	{
		State.YawDegrees = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));
		State.Position += Delta;
	}
	else
	{
		State.YawDegrees = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
	}
	// Keep Z from existing (ground projection is presentation/assist later — no NavMesh)
}

void UKodSimSubsystem::StepEntityAttack(FKodSimEntityState& State, float FixedDt)
{
	FKodSimEntityState* Target = States.Find(State.AttackTarget.Value);
	if (!Target || Target->Health <= 0.f)
	{
		State.Order = EKodSimOrderType::None;
		State.AttackTarget = FKodEntityId();
		QuantizePose(State);
		return;
	}
	if (Target->TeamId != State.TeamId && !TeamSeesPoint(State.TeamId, Target->Position.X, Target->Position.Y))
	{
		State.BackswingTicksRemaining = 0;
		if (State.bExplicitAttack && State.bMobile && State.MoveSpeed > 0.f)
		{
			// Keep the attack. Attack-move would forget the target, and the vision
			// grid is what failed to record it. Walk to last-seen, or the unit itself.
			const FVector Dest = HiddenTargetPoint(State.TeamId, *Target, State.Position.Z);
			const FKodEntityId KeptTarget = Target->Id;
			const bool bDestChanged = FVector::DistSquared2D(State.MoveTarget, Dest) > FMath::Square(64.f);
			State.MoveTarget = Dest;
			State.PathId = 0;
			State.Order = EKodSimOrderType::Attack;
			StepEntityMove(State, FixedDt);
			State.Order = EKodSimOrderType::Attack;
			State.AttackTarget = KeptTarget;
			State.bExplicitAttack = true;
			if (bDestChanged)
			{
				UE_LOG(LogTemp, Verbose, TEXT("KodOrder FogChase Unit=%d Dest=%.0f,%.0f"), State.Id.Value, Dest.X, Dest.Y);
			}
			return;
		}
		State.AttackTarget = FKodEntityId();
		State.bExplicitAttack = false;
		if (State.Order == EKodSimOrderType::Attack)
		{
			CompleteCurrentOrder(State, FixedDt);
		}
		else
		{
			QuantizePose(State);
		}
		return;
	}

	// Seek until the shot can land, then hold. StepEntityMove may clear the order
	// when it arrives inside acceptance; put the attack back before the range recheck.
	// Melee claims a ring slot so attackers do not stack on the target centre.
	const float Range = FMath::Max(0.f, State.WeaponRange);
	const bool bMelee = Range <= static_cast<float>(KodBuildGrid::CellUU) * 0.5f + 1.f;
	const FVector SeekPoint = bMelee ? MeleeSlotPoint(State, *Target) : Target->Position;
	float DistSq = FVector::DistSquared2D(State.Position, Target->Position);
	const bool bOffSlot = bMelee && FVector::DistSquared2D(State.Position, SeekPoint) > FMath::Square(32.f);
	if (DistSq > FMath::Square(Range) || bOffSlot)
	{
		const FKodEntityId KeptTarget = Target->Id;
		State.MoveTarget = SeekPoint;
		StepEntityMove(State, FixedDt);
		State.Order = EKodSimOrderType::Attack;
		State.AttackTarget = KeptTarget;
		DistSq = FVector::DistSquared2D(State.Position, Target->Position);
		if (DistSq > FMath::Square(Range))
		{
			return;
		}
	}

	// In range: stop integrating and face the target. Presentation chases this yaw.
	const FVector ToTarget = Target->Position - State.Position;
	if (ToTarget.SizeSquared2D() > KINDA_SMALL_NUMBER)
	{
		State.YawDegrees = FMath::RadiansToDegrees(FMath::Atan2(ToTarget.Y, ToTarget.X));
	}

	if (State.WeaponDamage > 0.f && TryApplyWeapon(State, *Target))
	{
		return;
	}

	QuantizePose(State);
}

FString UKodSimSubsystem::GetEntityActorName(int32 IdValue) const
{
	if (const TWeakObjectPtr<AActor>* Found = Entities.Find(IdValue))
	{
		if (const AActor* NamedActor = Found->Get())
		{
			return NamedActor->GetName();
		}
	}
	return FString(TEXT("None"));
}

namespace
{
	constexpr int32 AcquireCellUU = 1024;

	int64 PackAcquireCell(int32 X, int32 Y)
	{
		return (static_cast<int64>(static_cast<uint32>(X)) << 32) | static_cast<uint32>(Y);
	}
}

void UKodSimSubsystem::RebuildAcquireGrid()
{
	AcquireGrid.Empty(FMath::Max(AcquireGrid.Num(), 64));
	TArray<int32> Keys;
	States.GetKeys(Keys);
	Keys.Sort();
	for (int32 Key : Keys)
	{
		const FKodSimEntityState* State = States.Find(Key);
		if (!State || State->Health <= 0.f)
		{
			continue;
		}
		const int32 CellX = FMath::FloorToInt(State->Position.X / static_cast<float>(AcquireCellUU));
		const int32 CellY = FMath::FloorToInt(State->Position.Y / static_cast<float>(AcquireCellUU));
		AcquireGrid.FindOrAdd(PackAcquireCell(CellX, CellY)).Add(Key);
	}
	MeleeAttackersByTarget.Empty(FMath::Max(MeleeAttackersByTarget.Num(), 32));
}

void UKodSimSubsystem::CollectAcquireCandidates(float OriginX, float OriginY, float Range, TArray<int32>& OutIds) const
{
	OutIds.Reset();
	const float SafeRange = FMath::Max(0.f, Range);
	const int32 MinX = FMath::FloorToInt((OriginX - SafeRange) / static_cast<float>(AcquireCellUU));
	const int32 MaxX = FMath::FloorToInt((OriginX + SafeRange) / static_cast<float>(AcquireCellUU));
	const int32 MinY = FMath::FloorToInt((OriginY - SafeRange) / static_cast<float>(AcquireCellUU));
	const int32 MaxY = FMath::FloorToInt((OriginY + SafeRange) / static_cast<float>(AcquireCellUU));
	for (int32 X = MinX; X <= MaxX; ++X)
	{
		for (int32 Y = MinY; Y <= MaxY; ++Y)
		{
			if (const TArray<int32>* Cell = AcquireGrid.Find(PackAcquireCell(X, Y)))
			{
				OutIds.Append(*Cell);
			}
		}
	}
	OutIds.Sort();
}

FKodEntityId UKodSimSubsystem::FindNearestEnemyInRange(const FKodSimEntityState& State) const
{
	// Resource nodes are not in States, so they are not enemies and do not auto-acquire.
	FKodEntityId BestId;
	float BestDistSq = 0.f;
	bool bFound = false;
	const float Range = FMath::Max(0.f, State.WeaponRange);
	const float RangeSq = FMath::Square(Range);

	TArray<int32> Keys;
	CollectAcquireCandidates(State.Position.X, State.Position.Y, Range, Keys);
	for (int32 Key : Keys)
	{
		if (Key == State.Id.Value)
		{
			continue;
		}
		const FKodSimEntityState* Other = States.Find(Key);
		if (!Other || Other->Health <= 0.f || Other->TeamId == State.TeamId || Other->bFlyer)
		{
			continue;
		}
		// Structures stay out of idle acquire. An explicit attack still damages them.
		if (!Other->bMobile)
		{
			continue;
		}
		// Unarmed workers stay out of the marine duel. RMB Attack can still target them.
		if (Other->bCanGather && Other->WeaponDamage <= 0.f)
		{
			continue;
		}
		if (!TeamSeesPoint(State.TeamId, Other->Position.X, Other->Position.Y))
		{
			continue;
		}
		const float DistSq = FVector::DistSquared2D(State.Position, Other->Position);
		if (DistSq > RangeSq)
		{
			continue;
		}
		// Keys are sorted, so the first unit at a distance is the lowest id.
		// A later id replaces it only when strictly closer.
		if (!bFound || DistSq < BestDistSq)
		{
			bFound = true;
			BestDistSq = DistSq;
			BestId.Value = Key;
		}
	}
	return BestId;
}

FKodEntityId UKodSimSubsystem::FindNearestEnemyStructureInRange(const FKodSimEntityState& State) const
{
	FKodEntityId BestId;
	float BestDistSq = 0.f;
	bool bFound = false;
	const float Range = FMath::Max(0.f, State.WeaponRange);
	const float RangeSq = FMath::Square(Range);

	TArray<int32> Keys;
	CollectAcquireCandidates(State.Position.X, State.Position.Y, Range, Keys);
	for (int32 Key : Keys)
	{
		if (Key == State.Id.Value)
		{
			continue;
		}
		const FKodSimEntityState* Other = States.Find(Key);
		if (!Other || Other->bMobile || Other->Health <= 0.f || Other->TeamId == State.TeamId)
		{
			continue;
		}
		if (!TeamSeesPoint(State.TeamId, Other->Position.X, Other->Position.Y))
		{
			continue;
		}
		const float DistSq = FVector::DistSquared2D(State.Position, Other->Position);
		if (DistSq > RangeSq)
		{
			continue;
		}
		if (!bFound || DistSq < BestDistSq)
		{
			bFound = true;
			BestDistSq = DistSq;
			BestId.Value = Key;
		}
	}
	return BestId;
}

bool UKodSimSubsystem::TryAutoAcquire(FKodSimEntityState& State, float FixedDt)
{
	if (State.bCanGather || State.Order != EKodSimOrderType::None || State.GatherPhase != EKodGatherPhase::None || State.WeaponDamage <= 0.f || State.Health <= 0.f)
	{
		return false;
	}

	FKodEntityId Chosen;
	const TCHAR* Reason = nullptr;

	if (State.RetaliateTarget.IsValid())
	{
		const FKodEntityId Pending = State.RetaliateTarget;
		State.RetaliateTarget = FKodEntityId();
		if (const FKodSimEntityState* Aggro = States.Find(Pending.Value))
		{
			if (Aggro->Health > 0.f && Aggro->TeamId != State.TeamId && Aggro->Id.Value != State.Id.Value
				&& TeamSeesPoint(State.TeamId, Aggro->Position.X, Aggro->Position.Y))
			{
				Chosen = Aggro->Id;
				Reason = TEXT("Retaliate");
			}
		}
	}

	if (!Chosen.IsValid())
	{
		Chosen = FindNearestEnemyInRange(State);
		if (Chosen.IsValid())
		{
			Reason = TEXT("InRange");
		}
	}
	if (!Chosen.IsValid())
	{
		Chosen = FindNearestEnemyStructureInRange(State);
		if (Chosen.IsValid())
		{
			Reason = TEXT("Structure");
		}
	}

	if (!Chosen.IsValid() || !Reason)
	{
		return false;
	}

	State.Order = EKodSimOrderType::Attack;
	State.bExplicitAttack = false;
	State.AttackTarget = Chosen;
	UE_LOG(LogTemp, Log, TEXT("KodSim AutoAcquire Unit=%s Target=%s Reason=%s"),
		*GetEntityActorName(State.Id.Value),
		*GetEntityActorName(Chosen.Value),
		Reason);
	// Same hitscan path as an explicit attack. May unregister the target.
	// Do not use State after this call; TMap::Remove can rehash.
	StepEntityAttack(State, FixedDt);
	return true;
}

void UKodSimSubsystem::QuantizePose(FKodSimEntityState& State) const
{
	QuantizeResourcePosition(State.Position);
	State.YawDegrees = FMath::RoundToFloat(State.YawDegrees * 100.f) / 100.f;
}

void UKodSimSubsystem::QuantizeResourcePosition(FVector& Position) const
{
	const float Q = FMath::Max(KINDA_SMALL_NUMBER, PoseQuantizeUU);
	Position.X = FMath::RoundToFloat(Position.X / Q) * Q;
	Position.Y = FMath::RoundToFloat(Position.Y / Q) * Q;
	Position.Z = FMath::RoundToFloat(Position.Z / Q) * Q;
}

void UKodSimSubsystem::SyncActorPresentation(float Alpha) const
{
	int32 LocalTeam = 0;
	if (const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		if (const AKodPlayerState* PlayerState = PC->GetPlayerState<AKodPlayerState>())
		{
			LocalTeam = PlayerState->TeamId;
		}
	}

	for (const TPair<int32, FKodSimEntityState>& Pair : States)
	{
		const TWeakObjectPtr<AActor>* ActorPtr = Entities.Find(Pair.Key);
		AActor* Actor = ActorPtr ? ActorPtr->Get() : nullptr;
		if (!Actor)
		{
			continue;
		}
		const FVector* Prev = PrevPositions.Find(Pair.Key);
		const FVector From = Prev ? *Prev : Pair.Value.Position;
		const FVector To = Pair.Value.Position;
		const FVector Lerped = FMath::Lerp(From, To, Alpha);
		Actor->SetActorLocation(Lerped, false, nullptr, ETeleportType::None);

		const FKodStructureState* Lift = Structures.Find(Pair.Key);
		const bool bLandingEase = Lift && Lift->bAirborne;
		if (!bLandingEase)
		{
			const float* PrevYaw = PrevYaws.Find(Pair.Key);
			const float YawFrom = PrevYaw ? *PrevYaw : Pair.Value.YawDegrees;
			const float YawTo = Pair.Value.YawDegrees;
			const float Yaw = YawFrom + FMath::FindDeltaAngleDegrees(YawFrom, YawTo) * Alpha;
			FRotator Rot = Actor->GetActorRotation();
			Rot.Yaw = Yaw;
			Rot.Pitch = 0.f;
			Rot.Roll = 0.f;
			// None keeps the previous transform so motion vectors stay continuous.
			// TeleportPhysics would zero them and smear the skinned mesh under TAA.
			Actor->SetActorRotation(Rot, ETeleportType::None);
		}

		if (Pair.Value.InsideBunker.IsValid())
		{
			Actor->SetActorHiddenInGame(true);
			continue;
		}
		if (Pair.Value.TeamId == LocalTeam)
		{
			Actor->SetActorHiddenInGame(false);
		}
		else
		{
			// Enemy units and structures are live only in current vision.
			// Explored cells keep a last-seen structure snapshot on the HUD.
			Actor->SetActorHiddenInGame(GetTeamVision(LocalTeam, Pair.Value.Position.X, Pair.Value.Position.Y) != 2);
		}
	}
}

FKodEntityId UKodSimSubsystem::RegisterMapDestructible(AActor* Actor, int32 MinX, int32 MinY, int32 MaxX, int32 MaxY, float HP, int32 Armor)
{
	FKodEntityId Id = RegisterEntity(Actor);
	if (!Id.IsValid() || !Actor)
	{
		return Id;
	}
	ConfigureEntity(Id, FName(TEXT("KodDestructible")), HP, 0.f, 0.f, false, 0.f, 0.f, 0.f, 2, false, 80, 0);
	if (FKodSimEntityState* State = States.Find(Id.Value))
	{
		State->ArmorPoints = Armor;
		State->Health = HP;
		State->MaxHealth = HP;
		State->Position = Actor->GetActorLocation();
		QuantizePose(*State);
	}
	FKodDestructFoot Foot;
	Foot.Id = Id.Value;
	Foot.MinX = MinX;
	Foot.MinY = MinY;
	Foot.MaxX = MaxX;
	Foot.MaxY = MaxY;
	DestructFoots.Add(Foot);
	DestructFoots.Sort([](const FKodDestructFoot& A, const FKodDestructFoot& B)
	{
		return A.Id < B.Id;
	});
	UE_LOG(LogTemp, Log, TEXT("KodDestructible Id=%d HP=%.0f Armor=%d Cells=%d,%d-%d,%d"),
		Id.Value, HP, Armor, MinX, MinY, MaxX, MaxY);
	return Id;
}

FKodEntityId UKodSimSubsystem::RegisterEntity(AActor* Actor)
{
	FKodEntityId Id;
	if (!Actor)
	{
		return Id;
	}
	Id.Value = NextId++;
	Entities.Add(Id.Value, Actor);

	FKodSimEntityState State;
	State.Id = Id;
	State.Position = Actor->GetActorLocation();
	State.YawDegrees = Actor->GetActorRotation().Yaw;
	QuantizePose(State);
	States.Add(Id.Value, State);
	PrevPositions.Add(Id.Value, State.Position);
	PrevYaws.Add(Id.Value, State.YawDegrees);
	return Id;
}

void UKodSimSubsystem::UnregisterEntityIfActor(FKodEntityId Id, AActor* Actor)
{
	if (bLoadRespawn)
	{
		AActor* Bound = ResolveEntity(Id);
		if (!Bound || Bound == Actor)
		{
			return;
		}
	}
	AActor* Bound = ResolveEntity(Id);
	if (Actor && Bound && Bound != Actor)
	{
		return;
	}
	UnregisterEntity(Id);
}

void UKodSimSubsystem::UnregisterEntity(FKodEntityId Id)
{
	int32 SupplyTeam = INDEX_NONE;
	if (const FKodSimEntityState* State = States.Find(Id.Value))
	{
		SupplyTeam = State->TeamId;
	}
	ReleaseCountedSupply(Id);
	ReleaseStructureRecord(Id);
	Entities.Remove(Id.Value);
	States.Remove(Id.Value);
	PrevPositions.Remove(Id.Value);
	PrevYaws.Remove(Id.Value);
	if (SupplyTeam != INDEX_NONE)
	{
		RecalcSupplyCap(SupplyTeam);
	}
}

AActor* UKodSimSubsystem::ResolveEntity(FKodEntityId Id) const
{
	if (const TWeakObjectPtr<AActor>* Found = Entities.Find(Id.Value))
	{
		return Found->Get();
	}
	return nullptr;
}

FKodEntityId UKodSimSubsystem::FindIdForActor(AActor* Actor) const
{
	FKodEntityId Result;
	if (!Actor)
	{
		return Result;
	}
	for (const TPair<int32, TWeakObjectPtr<AActor>>& Pair : Entities)
	{
		if (Pair.Value.Get() == Actor)
		{
			Result.Value = Pair.Key;
			return Result;
		}
	}
	return Result;
}

void UKodSimSubsystem::ConfigureEntity(
	FKodEntityId Id,
	FName DefinitionId,
	float MaxHealth,
	float MoveSpeed,
	float AcceptanceRadius,
	bool bMobile,
	float WeaponDamage,
	float WeaponRange,
	float WeaponCooldownSeconds,
		int32 TeamId,
	bool bCanGather,
	int32 BodyRadiusUU,
	int32 SightRadiusUU)
{
	FKodSimEntityState* State = States.Find(Id.Value);
	if (!State)
	{
		return;
	}
	State->DefinitionId = DefinitionId;
	State->MaxHealth = MaxHealth;
	State->Health = MaxHealth; // HP from DA only at configure time
	State->MoveSpeed = MoveSpeed;
	State->AcceptanceRadius = AcceptanceRadius;
	State->bMobile = bMobile;
	State->WeaponDamage = WeaponDamage;
	State->WeaponRange = WeaponRange;
	State->WeaponCooldownSeconds = WeaponCooldownSeconds;
	State->WeaponCooldownTicks = FMath::Max(0, FMath::RoundToInt(WeaponCooldownSeconds * static_cast<float>(KodBuildTicks::SimHz)));
	State->CooldownTicksRemaining = 0;
	State->CooldownRemaining = 0.f;
	State->TeamId = TeamId;
	State->bCanGather = bCanGather;
	State->BodyRadiusUU = FMath::Clamp(BodyRadiusUU, 8, 256);
	State->SightRadiusUU = FMath::Clamp(SightRadiusUU, 0, 10000);
	State->WeaponBackswingTicks = 2;
	if (DefinitionId == FName(KodWardenPaths::Id_Dozer) && WeaponTable)
	{
		if (const FKodWeaponBalanceRow* Melee = WeaponTable->FindRow<FKodWeaponBalanceRow>(FName(KodWardenPaths::Id_DozerMelee), TEXT("KodConfigure"), false))
		{
			State->WeaponDamage = static_cast<float>(Melee->Damage);
			State->WeaponRange = static_cast<float>(Melee->Range);
			State->WeaponCooldownTicks = FMath::Max(0, Melee->CooldownTicks);
			State->WeaponCooldownSeconds = static_cast<float>(State->WeaponCooldownTicks) * KodBuildTicks::SimDt;
		}
	}
	ApplyRoster(Id);
}

void UKodSimSubsystem::SetPresentationShield(FKodEntityId Id, int32 Shield, int32 ShieldMax)
{
	FKodSimEntityState* State = States.Find(Id.Value);
	if (!State)
	{
		return;
	}
	State->ShieldMax = FMath::Max(0, ShieldMax);
	State->Shield = FMath::Clamp(Shield, 0, State->ShieldMax > 0 ? State->ShieldMax : Shield);
}

namespace
{
	void FaceStateToward(FKodSimEntityState& State, const FVector& WorldPoint)
	{
		FVector Dir = WorldPoint - State.Position;
		Dir.Z = 0.f;
		if (Dir.SizeSquared() <= KINDA_SMALL_NUMBER)
		{
			return;
		}
		State.YawDegrees = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
	}
}

FVector UKodSimSubsystem::MeleeSlotPoint(const FKodSimEntityState& State, const FKodSimEntityState& Target)
{
	constexpr int32 SlotCount = 8;
	constexpr float MeleeLimit = static_cast<float>(KodBuildGrid::CellUU) * 0.5f + 1.f;
	const float Radius = FMath::Max(48.f, State.WeaponRange - 16.f);
	TArray<int32> Attackers;
	if (const TArray<int32>* Cached = MeleeAttackersByTarget.Find(Target.Id.Value))
	{
		Attackers = *Cached;
	}
	else
	{
		TArray<int32> Candidates;
		CollectAcquireCandidates(Target.Position.X, Target.Position.Y, 1600.f, Candidates);
		for (int32 Key : Candidates)
		{
			const FKodSimEntityState* Other = States.Find(Key);
			if (!Other || Other->Health <= 0.f || Other->AttackTarget != Target.Id)
			{
				continue;
			}
			if (Other->WeaponRange > MeleeLimit)
			{
				continue;
			}
			if (Other->bCanGather && !Other->bExplicitAttack)
			{
				continue;
			}
			if (Other->Order != EKodSimOrderType::Attack && Other->Order != EKodSimOrderType::AttackMove)
			{
				continue;
			}
			Attackers.Add(Key);
		}
		MeleeAttackersByTarget.Add(Target.Id.Value, Attackers);
	}
	const bool bJoinsRing = State.Health > 0.f
		&& State.AttackTarget == Target.Id
		&& State.WeaponRange <= MeleeLimit
		&& !(State.bCanGather && !State.bExplicitAttack)
		&& (State.Order == EKodSimOrderType::Attack || State.Order == EKodSimOrderType::AttackMove);
	if (bJoinsRing && !Attackers.Contains(State.Id.Value))
	{
		Attackers.Add(State.Id.Value);
		Attackers.Sort();
		MeleeAttackersByTarget.Add(Target.Id.Value, Attackers);
	}

	bool Taken[SlotCount] = {};
	int32 MySlot = 0;
	for (int32 Index = 0; Index < Attackers.Num(); ++Index)
	{
		const int32 AttackerId = Attackers[Index];
		const FKodSimEntityState* Attacker = AttackerId == State.Id.Value ? &State : States.Find(AttackerId);
		const FVector From = Attacker ? Attacker->Position : State.Position;
		int32 BestSlot = Index % SlotCount;
		bool bFree = false;
		float BestDist = 0.f;
		for (int32 Slot = 0; Slot < SlotCount; ++Slot)
		{
			if (Taken[Slot])
			{
				continue;
			}
			const float Angle = (2.f * PI * static_cast<float>(Slot)) / static_cast<float>(SlotCount);
			const FVector Point = Target.Position + FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.f);
			const float Dist = FVector::DistSquared2D(From, Point);
			if (!bFree || Dist < BestDist)
			{
				bFree = true;
				BestDist = Dist;
				BestSlot = Slot;
			}
		}
		if (bFree)
		{
			Taken[BestSlot] = true;
		}
		if (AttackerId == State.Id.Value)
		{
			MySlot = BestSlot;
		}
	}

	const float Angle = (2.f * PI * static_cast<float>(MySlot)) / static_cast<float>(SlotCount);
	FVector SlotPoint = Target.Position + FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.f);
	SlotPoint.Z = State.Position.Z;
	return SlotPoint;
}

void UKodSimSubsystem::StepAttackMove(FKodSimEntityState& State, float FixedDt)
{
	if (!State.bMobile || State.Health <= 0.f || State.MoveSpeed <= 0.f)
	{
		State.Order = EKodSimOrderType::None;
		QuantizePose(State);
		return;
	}

	const FVector Dest = State.MoveTarget;
	const int32 SelfId = State.Id.Value;
	if (State.bCanGather)
	{
		State.RetaliateTarget = FKodEntityId();
		State.AttackTarget = FKodEntityId();
		State.bExplicitAttack = false;
		State.Order = EKodSimOrderType::AttackMove;
		State.MoveTarget = Dest;
		StepEntityMove(State, FixedDt);
		return;
	}
	FKodEntityId Threat;
	bool bFromHit = false;
	if (State.RetaliateTarget.IsValid() && State.GatherPhase == EKodGatherPhase::None)
	{
		const FKodSimEntityState* Aggro = States.Find(State.RetaliateTarget.Value);
		if (Aggro && Aggro->Health > 0.f && Aggro->TeamId != State.TeamId
			&& TeamSeesPoint(State.TeamId, Aggro->Position.X, Aggro->Position.Y))
		{
			Threat = Aggro->Id;
			bFromHit = true;
		}
		else
		{
			State.RetaliateTarget = FKodEntityId();
		}
	}
	if (!Threat.IsValid())
	{
		Threat = FindNearestEnemyInRange(State);
		if (!Threat.IsValid())
		{
			Threat = FindNearestEnemyStructureInRange(State);
		}
	}
	if (Threat.IsValid())
	{
		State.AttackTarget = Threat;
		State.bExplicitAttack = false;
		State.Order = EKodSimOrderType::Attack;
		StepEntityAttack(State, FixedDt);
		if (FKodSimEntityState* Live = States.Find(SelfId))
		{
			if (Live->Health > 0.f)
			{
				Live->Order = EKodSimOrderType::AttackMove;
				Live->MoveTarget = Dest;
				Live->bExplicitAttack = false;
				if (bFromHit)
				{
					const FKodSimEntityState* Still = States.Find(Threat.Value);
					if (!Still || Still->Health <= 0.f || !TeamSeesPoint(Live->TeamId, Still->Position.X, Still->Position.Y))
					{
						Live->RetaliateTarget = FKodEntityId();
					}
				}
			}
		}
		return;
	}

	State.AttackTarget = FKodEntityId();
	State.Order = EKodSimOrderType::AttackMove;
	State.MoveTarget = Dest;
	StepEntityMove(State, FixedDt);
}

bool UKodSimSubsystem::IsAttackable(FKodEntityId TargetId, int32 AttackerTeam) const
{
	if (!TargetId.IsValid() || IsResourceNode(TargetId))
	{
		return false;
	}
	const FKodSimEntityState* Target = States.Find(TargetId.Value);
	if (!Target || Target->Health <= 0.f)
	{
		return false;
	}
	if (IsDropOff(TargetId) && Target->TeamId == AttackerTeam)
	{
		return false;
	}
	return true;
}

namespace
{
	int32 SaturatingAddNonNegative(int32 Have, int32 Add)
	{
		if (Add <= 0)
		{
			return Have;
		}
		if (Have > MAX_int32 - Add)
		{
			return MAX_int32;
		}
		return Have + Add;
	}
}

void UKodSimSubsystem::SetTeamBank(int32 TeamId, int32 Jadeite, int32 Luminene)
{
	FKodResourceCost& Bank = Banks.FindOrAdd(TeamId);
	Bank.Jadeite = FMath::Max(0, Jadeite);
	Bank.Luminene = FMath::Max(0, Luminene);
}

bool UKodSimSubsystem::TryGetBank(int32 TeamId, FKodResourceCost& OutBank) const
{
	if (const FKodResourceCost* Bank = Banks.Find(TeamId))
	{
		OutBank = *Bank;
		return true;
	}
	OutBank = FKodResourceCost();
	return false;
}

bool UKodSimSubsystem::CanAfford(int32 TeamId, FKodResourceCost Cost) const
{
	const int32 NeedJadeite = FMath::Max(0, Cost.Jadeite);
	const int32 NeedLuminene = FMath::Max(0, Cost.Luminene);
	FKodResourceCost Bank;
	TryGetBank(TeamId, Bank);
	return Bank.Jadeite >= NeedJadeite && Bank.Luminene >= NeedLuminene;
}

bool UKodSimSubsystem::Spend(int32 TeamId, FKodResourceCost Cost)
{
	const int32 NeedJadeite = FMath::Max(0, Cost.Jadeite);
	const int32 NeedLuminene = FMath::Max(0, Cost.Luminene);
	FKodResourceCost* Bank = Banks.Find(TeamId);
	const int32 HaveJadeite = Bank ? Bank->Jadeite : 0;
	const int32 HaveLuminene = Bank ? Bank->Luminene : 0;
	if (HaveJadeite < NeedJadeite || HaveLuminene < NeedLuminene)
	{
		UE_LOG(LogTemp, Log, TEXT("KodEcon Spend Reject Team=%d J=%d L=%d Bank=J%d,L%d"),
			TeamId,
			NeedJadeite,
			NeedLuminene,
			HaveJadeite,
			HaveLuminene);
		return false;
	}
	if (NeedJadeite == 0 && NeedLuminene == 0)
	{
		return true;
	}
	if (!Bank)
	{
		return false;
	}
	Bank->Jadeite -= NeedJadeite;
	Bank->Luminene -= NeedLuminene;
	ScoreTracker.NoteSpend(TeamId, NeedJadeite, NeedLuminene);
	UE_LOG(LogTemp, Log, TEXT("KodEcon Spend Team=%d J=%d L=%d Bank=J%d,L%d"),
		TeamId,
		NeedJadeite,
		NeedLuminene,
		Bank->Jadeite,
		Bank->Luminene);
	return true;
}

void UKodSimSubsystem::Deposit(int32 TeamId, FKodResourceCost Amount)
{
	NoteResources(TeamId, FMath::Max(0, Amount.Jadeite) + FMath::Max(0, Amount.Luminene));
	ScoreTracker.NoteGather(TeamId, Amount.Jadeite, Amount.Luminene);
	const int32 AddJadeite = FMath::Max(0, Amount.Jadeite);
	const int32 AddLuminene = FMath::Max(0, Amount.Luminene);
	FKodResourceCost& Bank = Banks.FindOrAdd(TeamId);
	Bank.Jadeite = SaturatingAddNonNegative(Bank.Jadeite, AddJadeite);
	Bank.Luminene = SaturatingAddNonNegative(Bank.Luminene, AddLuminene);
	UE_LOG(LogTemp, Log, TEXT("KodEcon Deposit Team=%d J=%d L=%d"),
		TeamId,
		AddJadeite,
		AddLuminene);
}

FKodEntityId UKodSimSubsystem::RegisterResourceNode(AActor* Actor, const FKodResourceNodeSpec& Spec)
{
	FKodEntityId Id;
	if (!Actor || Spec.Amount <= 0)
	{
		return Id;
	}

	const FKodEntityId Existing = FindIdForActor(Actor);
	if (Existing.IsValid())
	{
		return IsResourceNode(Existing) ? Existing : Id;
	}

	Id.Value = NextId++;
	Entities.Add(Id.Value, Actor);

	FKodResourceNodeState Node;
	Node.Id = Id;
	Node.Type = Spec.Type;
	Node.DefinitionId = Spec.DefinitionId;
	// Actor origin (the ground point). A mesh-center offset must not live on the root,
	// or this copies (0, 0, half-height) instead of the arc.
	Node.Position = Actor->GetActorLocation();
	QuantizeResourcePosition(Node.Position);
	Node.Remaining = Spec.Amount;
	Node.HarvestPerTrip = FMath::Max(0, Spec.HarvestPerTrip);
	Node.HarvestTicks = FMath::Max(1, Spec.HarvestTicks);
	Node.TricklePerTrip = FMath::Max(0, Spec.TricklePerTrip);
	Node.GatherStandUU = FMath::Max(1, Spec.GatherStandUU);
	Node.bDepletionLogged = false;
	ResourceNodes.Add(Id.Value, Node);
	return Id;
}

FKodEntityId UKodSimSubsystem::RegisterDropOff(AActor* Actor, int32 TeamId, int32 StandUU)
{
	FKodEntityId Id;
	if (!Actor)
	{
		return Id;
	}

	const FKodEntityId Existing = FindIdForActor(Actor);
	if (Existing.IsValid())
	{
		return IsDropOff(Existing) ? Existing : Id;
	}

	Id.Value = NextId++;
	Entities.Add(Id.Value, Actor);

	FKodDropOffState Drop;
	Drop.Id = Id;
	Drop.TeamId = TeamId;
	Drop.Position = Actor->GetActorLocation();
	QuantizeResourcePosition(Drop.Position);
	Drop.StandUU = FMath::Max(1, StandUU);
	DropOffs.Add(Id.Value, Drop);
	UE_LOG(LogTemp, Log, TEXT("KodEcon DropOff Id=%d Team=%d Loc=%.0f,%.0f,%.0f"),
		Id.Value,
		TeamId,
		Drop.Position.X,
		Drop.Position.Y,
		Drop.Position.Z);
	return Id;
}

void UKodSimSubsystem::PushJuiceText(const FVector& Position, int32 Amount, uint8 Kind)
{
	if (bTestIsolation || Amount == 0)
	{
		return;
	}
	OnJuiceText.Broadcast(Position, Amount, Kind);
}

void UKodSimSubsystem::UnregisterDropOff(FKodEntityId Id)
{
	DropOffs.Remove(Id.Value);
	Entities.Remove(Id.Value);
}

bool UKodSimSubsystem::IsDropOff(FKodEntityId Id) const
{
	return Id.IsValid() && DropOffs.Contains(Id.Value);
}

FKodEntityId UKodSimSubsystem::FindNearestDropOff(int32 TeamId, FVector Origin) const
{
	FKodEntityId Best;
	bool bFound = false;
	int64 BestDistSq = 0;
	const float Q = FMath::Max(KINDA_SMALL_NUMBER, PoseQuantizeUU);
	const int32 OriginX = FMath::RoundToInt(Origin.X / Q);
	const int32 OriginY = FMath::RoundToInt(Origin.Y / Q);

	TArray<int32> Keys;
	DropOffs.GetKeys(Keys);
	Keys.Sort();
	for (int32 Key : Keys)
	{
		const FKodDropOffState* Drop = DropOffs.Find(Key);
		if (!Drop || Drop->TeamId != TeamId)
		{
			continue;
		}
		if (const FKodStructureState* Structure = Structures.Find(Key))
		{
			if (Structure->bAirborne || !Structure->bComplete)
			{
				continue;
			}
		}
		const int32 DropX = FMath::RoundToInt(Drop->Position.X / Q);
		const int32 DropY = FMath::RoundToInt(Drop->Position.Y / Q);
		const int64 DX = static_cast<int64>(DropX) - static_cast<int64>(OriginX);
		const int64 DY = static_cast<int64>(DropY) - static_cast<int64>(OriginY);
		const int64 DistSq = DX * DX + DY * DY;
		if (!bFound || DistSq < BestDistSq)
		{
			bFound = true;
			BestDistSq = DistSq;
			Best.Value = Key;
		}
	}
	return Best;
}

void UKodSimSubsystem::UnregisterResourceNode(FKodEntityId Id)
{
	ResourceNodes.Remove(Id.Value);
	Entities.Remove(Id.Value);
}

bool UKodSimSubsystem::IsResourceNode(FKodEntityId Id) const
{
	return Id.IsValid() && ResourceNodes.Contains(Id.Value);
}

bool UKodSimSubsystem::TryGetResourceNode(FKodEntityId Id, FKodResourceNodeState& OutNode) const
{
	if (const FKodResourceNodeState* Node = ResourceNodes.Find(Id.Value))
	{
		OutNode = *Node;
		return true;
	}
	return false;
}

FKodEntityId UKodSimSubsystem::FindNearestResourceNode(FVector Origin) const
{
	FKodEntityId Best;
	bool bFound = false;
	int64 BestDistSq = 0;
	const float Q = FMath::Max(KINDA_SMALL_NUMBER, PoseQuantizeUU);
	const int32 OriginX = FMath::RoundToInt(Origin.X / Q);
	const int32 OriginY = FMath::RoundToInt(Origin.Y / Q);

	TArray<int32> Keys;
	ResourceNodes.GetKeys(Keys);
	Keys.Sort();
	for (int32 Key : Keys)
	{
		const FKodResourceNodeState* Node = ResourceNodes.Find(Key);
		if (!Node || (Node->Remaining <= 0 && Node->TricklePerTrip <= 0))
		{
			continue;
		}
		const int32 NodeX = FMath::RoundToInt(Node->Position.X / Q);
		const int32 NodeY = FMath::RoundToInt(Node->Position.Y / Q);
		const int64 DX = static_cast<int64>(NodeX) - static_cast<int64>(OriginX);
		const int64 DY = static_cast<int64>(NodeY) - static_cast<int64>(OriginY);
		const int64 DistSq = DX * DX + DY * DY;
		// Keys are sorted, so an equal distance keeps the lower id.
		if (!bFound || DistSq < BestDistSq)
		{
			bFound = true;
			BestDistSq = DistSq;
			Best.Value = Key;
		}
	}
	return Best;
}

FKodEntityId UKodSimSubsystem::FindNearestResourceNodeOfType(FVector Origin, EKodResourceType Type, bool bRequireRemaining) const
{
	FKodEntityId Best;
	bool bFound = false;
	int64 BestDistSq = 0;
	const float Q = FMath::Max(KINDA_SMALL_NUMBER, PoseQuantizeUU);
	const int32 OriginX = FMath::RoundToInt(Origin.X / Q);
	const int32 OriginY = FMath::RoundToInt(Origin.Y / Q);

	TArray<int32> Keys;
	ResourceNodes.GetKeys(Keys);
	Keys.Sort();
	for (int32 Key : Keys)
	{
		const FKodResourceNodeState* Node = ResourceNodes.Find(Key);
		if (!Node || Node->Type != Type)
		{
			continue;
		}
		if (bRequireRemaining)
		{
			if (Node->Remaining <= 0)
			{
				continue;
			}
		}
		else if (Node->Remaining <= 0 && Node->TricklePerTrip <= 0)
		{
			continue;
		}
		const int32 NodeX = FMath::RoundToInt(Node->Position.X / Q);
		const int32 NodeY = FMath::RoundToInt(Node->Position.Y / Q);
		const int64 DX = static_cast<int64>(NodeX) - static_cast<int64>(OriginX);
		const int64 DY = static_cast<int64>(NodeY) - static_cast<int64>(OriginY);
		const int64 DistSq = DX * DX + DY * DY;
		if (!bFound || DistSq < BestDistSq)
		{
			bFound = true;
			BestDistSq = DistSq;
			Best.Value = Key;
		}
	}
	return Best;
}

void UKodSimSubsystem::DestroyResourceNodeActor(FKodEntityId Id)
{
	AActor* Actor = ResolveEntity(Id);
	UnregisterResourceNode(Id);
	if (!Actor)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			if (AKodPlayerController* KodPC = Cast<AKodPlayerController>(It->Get()))
			{
				KodPC->RemoveFromLocalSelection(Actor);
			}
		}
	}
	if (!Actor->IsActorBeingDestroyed())
	{
		Actor->Destroy();
	}
}

void UKodSimSubsystem::ClearGatherFields(FKodSimEntityState& State)
{
	State.GatherPhase = EKodGatherPhase::None;
	State.GatherNode = FKodEntityId();
	State.GatherDropOff = FKodEntityId();
	State.GatherType = EKodResourceType::Jadeite;
	State.CargoJadeite = 0;
	State.CargoLuminene = 0;
	State.bCargoFromTrickle = false;
	State.HarvestTicksRemaining = 0;
	State.GatherStallTicks = 0;
	State.bGatherStallRetargeted = false;
	if (State.Order == EKodSimOrderType::Gather)
	{
		State.Order = EKodSimOrderType::None;
	}
}

void UKodSimSubsystem::RefundGatherCargo(FKodSimEntityState& State)
{
	const int32 Jadeite = State.CargoJadeite;
	const int32 Luminene = State.CargoLuminene;
	if (Jadeite <= 0 && Luminene <= 0)
	{
		return;
	}

	const bool bTrickle = State.bCargoFromTrickle;
	const FKodEntityId NodeId = State.GatherNode;
	const int32 TeamId = State.TeamId;
	State.CargoJadeite = 0;
	State.CargoLuminene = 0;
	State.bCargoFromTrickle = false;
	if (bTrickle)
	{
		return;
	}

	if (FKodResourceNodeState* Node = ResourceNodes.Find(NodeId.Value))
	{
		if (Node->Type == EKodResourceType::Luminene)
		{
			Node->Remaining = SaturatingAddNonNegative(Node->Remaining, Luminene);
		}
		else
		{
			Node->Remaining = SaturatingAddNonNegative(Node->Remaining, Jadeite);
		}
		if (Node->Remaining > 0)
		{
			Node->bDepletionLogged = false;
		}
		return;
	}

	// The pile is already gone. Put the carried ints in the bank so the trip is not deleted.
	FKodResourceCost Gain;
	Gain.Jadeite = Jadeite;
	Gain.Luminene = Luminene;
	Deposit(TeamId, Gain);
}

void UKodSimSubsystem::AbortGather(FKodSimEntityState& State)
{
	RefundGatherCargo(State);
	ClearGatherFields(State);
	State.AttackTarget = FKodEntityId();
	QuantizePose(State);
}

void UKodSimSubsystem::BeginGatherLeg(FKodSimEntityState& State, EKodGatherPhase Phase)
{
	State.GatherPhase = Phase;
	State.Order = EKodSimOrderType::Gather;
	State.GatherStallTicks = 0;
	State.bGatherStallRetargeted = false;
	State.AttackTarget = FKodEntityId();
	State.RetaliateTarget = FKodEntityId();
}

bool UKodSimSubsystem::RetargetGatherNode(FKodSimEntityState& State)
{
	const EKodResourceType Type = State.GatherType;
	FKodEntityId Next = FindNearestResourceNodeOfType(State.Position, Type, /*bRequireRemaining*/ true);
	if (!Next.IsValid())
	{
		Next = FindNearestResourceNodeOfType(State.Position, Type, /*bRequireRemaining*/ false);
	}
	if (Type == EKodResourceType::Luminene && Next.IsValid() && !FindCompletedRig(State.TeamId, Next).IsValid())
	{
		Next = FKodEntityId();
		TArray<int32> Keys;
		ResourceNodes.GetKeys(Keys);
		Keys.Sort();
		int64 BestDist = MAX_int64;
		for (int32 Key : Keys)
		{
			const FKodResourceNodeState* Node = ResourceNodes.Find(Key);
			if (!Node || Node->Type != EKodResourceType::Luminene)
			{
				continue;
			}
			if (!FindCompletedRig(State.TeamId, Node->Id).IsValid())
			{
				continue;
			}
			const int64 DX = FMath::RoundToInt(Node->Position.X - State.Position.X);
			const int64 DY = FMath::RoundToInt(Node->Position.Y - State.Position.Y);
			const int64 Dist = DX * DX + DY * DY;
			if (!Next.IsValid() || Dist < BestDist)
			{
				BestDist = Dist;
				Next.Value = Key;
			}
		}
	}
	if (!Next.IsValid())
	{
		State.GatherNode = FKodEntityId();
		return false;
	}
	State.GatherNode = Next;
	if (const FKodResourceNodeState* Node = ResourceNodes.Find(Next.Value))
	{
		State.GatherType = Node->Type;
	}
	return true;
}

bool UKodSimSubsystem::AdvanceGatherMove(FKodSimEntityState& State, const FVector& Target, float AcceptUU, float FixedDt)
{
	const float Accept = FMath::Max(1.f, AcceptUU);
	FVector ToTarget = Target - State.Position;
	ToTarget.Z = 0.f;
	const float Dist = ToTarget.Size();
	if (Dist <= Accept)
	{
		State.GatherStallTicks = 0;
		State.bGatherStallRetargeted = false;
		FaceStateToward(State, Target);
		QuantizePose(State);
		return true;
	}

	auto NoteStall = [&State](float MovedSq)
	{
		if (MovedSq < 0.25f)
		{
			++State.GatherStallTicks;
		}
		else
		{
			State.GatherStallTicks = 0;
		}
	};

	if (!State.bMobile || State.MoveSpeed <= 0.f)
	{
		NoteStall(0.f);
		return false;
	}

	const FVector Before = State.Position;
	const FVector Dir = ToTarget / Dist;
	State.YawDegrees = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
	const float Step = FMath::Max(0.f, EffectiveMoveSpeed(State)) * FixedDt;
	// Land on the stand ring. Do not snap onto the node or building origin.
	const float Travel = FMath::Min(Step, FMath::Max(0.f, Dist - Accept));
	if (Travel <= KINDA_SMALL_NUMBER)
	{
		NoteStall(0.f);
		QuantizePose(State);
		return true;
	}
	FVector Delta = SteerAroundBuildings(State, Dir * Travel);
	State.Position += Delta;
	NoteStall(FVector::DistSquared2D(Before, State.Position));
	return false;
}

bool UKodSimSubsystem::RecoverGatherStall(FKodSimEntityState& State, float Dist, float AcceptUU)
{
	if (State.GatherStallTicks < KodEconomyDefaults::GatherStallTicks)
	{
		return false;
	}
	if (Dist <= AcceptUU * 1.25f)
	{
		State.GatherStallTicks = 0;
		State.bGatherStallRetargeted = false;
		QuantizePose(State);
		return true;
	}
	if (!State.bGatherStallRetargeted)
	{
		State.bGatherStallRetargeted = true;
		State.GatherStallTicks = 0;
		if (State.GatherPhase == EKodGatherPhase::ToDropOff)
		{
			State.GatherDropOff = FindNearestDropOff(State.TeamId, State.Position);
		}
		else
		{
			RetargetGatherNode(State);
		}
		return false;
	}

	UE_LOG(LogTemp, Warning, TEXT("KodEcon Gather Abort Worker=%d Phase=%d"),
		State.Id.Value,
		static_cast<int32>(State.GatherPhase));
	if (State.IsCarryingCargo())
	{
		FKodResourceCost Gain;
		Gain.Jadeite = State.CargoJadeite;
		Gain.Luminene = State.CargoLuminene;
		State.CargoJadeite = 0;
		State.CargoLuminene = 0;
		State.bCargoFromTrickle = false;
		if (Gain.Jadeite > 0 || Gain.Luminene > 0)
		{
			Deposit(State.TeamId, Gain);
		}
	}
	ClearGatherFields(State);
	QuantizePose(State);
	return false;
}

FKodNodeTakeResult UKodSimSubsystem::TakeFromNode(FKodEntityId Id, int32 Requested)
{
	FKodNodeTakeResult Result;
	FKodResourceNodeState* Node = ResourceNodes.Find(Id.Value);
	if (!Node || Requested <= 0)
	{
		return Result;
	}

	Result.bFound = true;
	Result.Type = Node->Type;
	const int32 IdValue = Id.Value;

	if (Node->Remaining > 0)
	{
		Result.Amount = FMath::Min(Requested, Node->Remaining);
		Node->Remaining -= Result.Amount;
		const int32 Remaining = Node->Remaining;
		const EKodResourceType Type = Node->Type;
		const bool bDestroy = Remaining <= 0 && Node->TricklePerTrip <= 0;
		if (Remaining <= 0 && !Node->bDepletionLogged)
		{
			Node->bDepletionLogged = true;
			Result.bDepleted = true;
			UE_LOG(LogTemp, Log, TEXT("KodEcon NodeDepleted Id=%d"), IdValue);
		}
		UE_LOG(LogTemp, Log, TEXT("KodEcon Node Id=%d Type=%s Remaining=%d"),
			IdValue,
			KodResourceTypeName(Type),
			Remaining);
		if (bDestroy)
		{
			DestroyResourceNodeActor(Id);
		}
		return Result;
	}

	if (Node->TricklePerTrip > 0)
	{
		Result.Amount = Node->TricklePerTrip;
		Result.bFromTrickle = true;
		return Result;
	}

	Result.bFound = false;
	return Result;
}

void UKodSimSubsystem::StepEntityGather(FKodSimEntityState& State, float FixedDt)
{
	if (!State.bCanGather || State.Health <= 0.f)
	{
		AbortGather(State);
		return;
	}

	State.Order = EKodSimOrderType::Gather;

	if (State.GatherPhase == EKodGatherPhase::ToNode)
	{
		const FKodResourceNodeState* Node = ResourceNodes.Find(State.GatherNode.Value);
		const bool bUsable = Node && (Node->Remaining > 0 || Node->TricklePerTrip > 0);
		if (!bUsable)
		{
			if (!RetargetGatherNode(State))
			{
				if (State.IsCarryingCargo())
				{
					BeginGatherLeg(State, EKodGatherPhase::ToDropOff);
				}
				else
				{
					AbortGather(State);
				}
				return;
			}
			Node = ResourceNodes.Find(State.GatherNode.Value);
			if (!Node)
			{
				AbortGather(State);
				return;
			}
		}

		FVector Approach = Node->Position;
		if (!TryGetGatherApproach(State.TeamId, *Node, Approach))
		{
			if (!RetargetGatherNode(State))
			{
				AbortGather(State);
			}
			return;
		}
		const float Stand = static_cast<float>(FMath::Max(1, Node->GatherStandUU));
		const float Dist = FVector::Dist2D(State.Position, Approach);
		if (Dist > Stand && State.GatherStallTicks >= KodEconomyDefaults::GatherStallTicks)
		{
			if (RecoverGatherStall(State, Dist, Stand))
			{
				BeginGatherLeg(State, EKodGatherPhase::Harvest);
				const int32 Rate = FMath::Max(1, State.GatherRatePermille > 0 ? State.GatherRatePermille : 1000);
				State.HarvestTicksRemaining = FMath::Max(1, (Node->HarvestTicks * 1000) / Rate);
			}
			return;
		}
		if (AdvanceGatherMove(State, Approach, Stand, FixedDt))
		{
			BeginGatherLeg(State, EKodGatherPhase::Harvest);
			if (const FKodResourceNodeState* Arrived = ResourceNodes.Find(State.GatherNode.Value))
			{
				const int32 Rate = FMath::Max(1, State.GatherRatePermille > 0 ? State.GatherRatePermille : 1000);
				State.HarvestTicksRemaining = FMath::Max(1, (Arrived->HarvestTicks * 1000) / Rate);
			}
			else
			{
				State.HarvestTicksRemaining = KodEconomyDefaults::JadeiteHarvestTicks;
			}
		}
		return;
	}

	if (State.GatherPhase == EKodGatherPhase::Harvest)
	{
		const FKodResourceNodeState* Node = ResourceNodes.Find(State.GatherNode.Value);
		if (!Node || (Node->Remaining <= 0 && Node->TricklePerTrip <= 0))
		{
			if (State.IsCarryingCargo())
			{
				BeginGatherLeg(State, EKodGatherPhase::ToDropOff);
			}
			else if (!RetargetGatherNode(State))
			{
				AbortGather(State);
			}
			else
			{
				BeginGatherLeg(State, EKodGatherPhase::ToNode);
			}
			return;
		}

		FaceStateToward(State, Node->Position);
		QuantizePose(State);
		State.HarvestTicksRemaining = FMath::Max(0, State.HarvestTicksRemaining - 1);
		if (State.HarvestTicksRemaining > 0)
		{
			return;
		}

		const int32 Requested = FMath::Max(1, Node->HarvestPerTrip);
		const FKodEntityId NodeId = State.GatherNode;
		const FKodNodeTakeResult Taken = TakeFromNode(NodeId, Requested);
		if (Taken.Amount > 0)
		{
			if (Taken.Type == EKodResourceType::Luminene)
			{
				State.CargoLuminene = Taken.Amount;
				State.CargoJadeite = 0;
			}
			else
			{
				State.CargoJadeite = Taken.Amount;
				State.CargoLuminene = 0;
			}
			State.bCargoFromTrickle = Taken.bFromTrickle;
			State.GatherType = Taken.Type;
			UE_LOG(LogTemp, Log, TEXT("KodEcon Gather Worker=%d Node=%d Amt=%d"),
				State.Id.Value,
				NodeId.Value,
				Taken.Amount);
		}

		if (Taken.bDepleted || !ResourceNodes.Contains(NodeId.Value))
		{
			const FKodEntityId Richer = FindNearestResourceNodeOfType(State.Position, State.GatherType, /*bRequireRemaining*/ true);
			if (Richer.IsValid())
			{
				State.GatherNode = Richer;
			}
			else if (!ResourceNodes.Contains(NodeId.Value) || !RetargetGatherNode(State))
			{
				State.GatherNode = FKodEntityId();
			}
		}
		else if (const FKodResourceNodeState* After = ResourceNodes.Find(NodeId.Value))
		{
			if (After->Remaining <= 0)
			{
				const FKodEntityId Richer = FindNearestResourceNodeOfType(State.Position, After->Type, /*bRequireRemaining*/ true);
				if (Richer.IsValid())
				{
					State.GatherNode = Richer;
				}
			}
		}

		if (State.IsCarryingCargo())
		{
			BeginGatherLeg(State, EKodGatherPhase::ToDropOff);
		}
		else if (State.GatherNode.IsValid() && ResourceNodes.Contains(State.GatherNode.Value))
		{
			BeginGatherLeg(State, EKodGatherPhase::ToNode);
		}
		else
		{
			AbortGather(State);
		}
		return;
	}

	if (State.GatherPhase == EKodGatherPhase::ToDropOff)
	{
		if (!State.IsCarryingCargo())
		{
			if (State.GatherNode.IsValid() && ResourceNodes.Contains(State.GatherNode.Value))
			{
				BeginGatherLeg(State, EKodGatherPhase::ToNode);
			}
			else if (!RetargetGatherNode(State))
			{
				AbortGather(State);
			}
			else
			{
				BeginGatherLeg(State, EKodGatherPhase::ToNode);
			}
			return;
		}

		if (!IsDropOff(State.GatherDropOff))
		{
			State.GatherDropOff = FindNearestDropOff(State.TeamId, State.Position);
		}
		const FKodDropOffState* Drop = DropOffs.Find(State.GatherDropOff.Value);
		if (!Drop)
		{
			QuantizePose(State);
			return;
		}

		const float Stand = static_cast<float>(FMath::Max(1, Drop->StandUU));
		const float Dist = FVector::Dist2D(State.Position, Drop->Position);
		if (Dist > Stand && State.GatherStallTicks >= KodEconomyDefaults::GatherStallTicks)
		{
			if (!RecoverGatherStall(State, Dist, Stand))
			{
				return;
			}
		}
		else if (!AdvanceGatherMove(State, Drop->Position, Stand, FixedDt))
		{
			return;
		}

		FKodResourceCost Gain;
		Gain.Jadeite = State.CargoJadeite;
		Gain.Luminene = State.CargoLuminene;
		State.CargoJadeite = 0;
		State.CargoLuminene = 0;
		State.bCargoFromTrickle = false;
		if (Gain.Jadeite > 0 || Gain.Luminene > 0)
		{
			Deposit(State.TeamId, Gain);
			const int32 Shown = Gain.Jadeite > 0 ? Gain.Jadeite : Gain.Luminene;
			PushJuiceText(Drop->Position, Shown, 1);
			RaiseCue(FName(TEXT("resource_drop")), Drop->Position, true);
		}

		if (!State.GatherNode.IsValid() || !ResourceNodes.Contains(State.GatherNode.Value))
		{
			if (!RetargetGatherNode(State))
			{
				ClearGatherFields(State);
				QuantizePose(State);
				return;
			}
		}
		else if (const FKodResourceNodeState* Next = ResourceNodes.Find(State.GatherNode.Value))
		{
			if (Next->Remaining <= 0 && Next->TricklePerTrip <= 0)
			{
				if (!RetargetGatherNode(State))
				{
					ClearGatherFields(State);
					QuantizePose(State);
					return;
				}
			}
			else if (Next->Remaining <= 0)
			{
				const FKodEntityId Richer = FindNearestResourceNodeOfType(State.Position, Next->Type, /*bRequireRemaining*/ true);
				if (Richer.IsValid())
				{
					State.GatherNode = Richer;
				}
			}
		}
		BeginGatherLeg(State, EKodGatherPhase::ToNode);
		return;
	}

	AbortGather(State);
}

bool UKodSimSubsystem::IssueGather(FKodEntityId WorkerId, FKodEntityId NodeId)
{
	FKodSimEntityState* State = States.Find(WorkerId.Value);
	const FKodResourceNodeState* Node = ResourceNodes.Find(NodeId.Value);
	if (!State || !State->bCanGather || State->Health <= 0.f || !Node)
	{
		return false;
	}
	if (Node->Remaining <= 0 && Node->TricklePerTrip <= 0)
	{
		return false;
	}

	State->GatherType = Node->Type;
	if (Node->Remaining <= 0)
	{
		const FKodEntityId Richer = FindNearestResourceNodeOfType(State->Position, Node->Type, /*bRequireRemaining*/ true);
		if (Richer.IsValid())
		{
			NodeId = Richer;
			Node = ResourceNodes.Find(NodeId.Value);
		}
	}
	if (!Node)
	{
		return false;
	}
	FVector Approach = Node->Position;
	if (!TryGetGatherApproach(State->TeamId, *Node, Approach))
	{
		return false;
	}

	State->GatherNode = NodeId;
	State->GatherType = Node->Type;
	State->GatherDropOff = FindNearestDropOff(State->TeamId, State->Position);
	if (State->IsCarryingCargo())
	{
		BeginGatherLeg(*State, EKodGatherPhase::ToDropOff);
	}
	else
	{
		State->bCargoFromTrickle = false;
		BeginGatherLeg(*State, EKodGatherPhase::ToNode);
	}
	ClearBuildAssignment(*State);
	FaceStateToward(*State, Approach);
	UE_LOG(LogTemp, Log, TEXT("KodEcon GatherOrder Worker=%d Node=%d Type=%s"),
		WorkerId.Value,
		NodeId.Value,
		KodResourceTypeName(State->GatherType));
	return true;
}

int32 UKodSimSubsystem::HarvestNode(FKodEntityId Id, int32 Amount, int32 TeamId)
{
	const FKodNodeTakeResult Taken = TakeFromNode(Id, Amount);
	if (!Taken.bFound || Taken.Amount <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("KodEcon Harvest Miss Id=%d"), Id.Value);
		return 0;
	}

	FKodResourceCost Gain;
	if (Taken.Type == EKodResourceType::Luminene)
	{
		Gain.Luminene = Taken.Amount;
	}
	else
	{
		Gain.Jadeite = Taken.Amount;
	}
	Deposit(TeamId, Gain);
	return Taken.Amount;
}

void UKodSimSubsystem::CopyEntitySnapshot(TArray<FKodSimEntityState>& OutStates, TArray<AActor*>& OutActors) const
{
	OutStates.Reset();
	OutActors.Reset();

	TArray<int32> Keys;
	States.GetKeys(Keys);
	Keys.Sort();
	OutStates.Reserve(Keys.Num());
	OutActors.Reserve(Keys.Num());

	for (int32 Key : Keys)
	{
		const FKodSimEntityState* State = States.Find(Key);
		const TWeakObjectPtr<AActor>* ActorPtr = Entities.Find(Key);
		AActor* Actor = ActorPtr ? ActorPtr->Get() : nullptr;
		if (!State || !Actor)
		{
			continue;
		}
		OutStates.Add(*State);
		OutActors.Add(Actor);
	}
}

void UKodSimSubsystem::CopyResourceNodes(TArray<FKodResourceNodeState>& OutNodes) const
{
	OutNodes.Reset();
	TArray<int32> Keys;
	ResourceNodes.GetKeys(Keys);
	Keys.Sort();
	OutNodes.Reserve(Keys.Num());
	for (int32 Key : Keys)
	{
		if (const FKodResourceNodeState* Node = ResourceNodes.Find(Key))
		{
			OutNodes.Add(*Node);
		}
	}
}

void UKodSimSubsystem::CopyStructures(TArray<FKodStructureState>& OutStructures) const
{
	OutStructures.Reset();
	TArray<int32> Keys;
	Structures.GetKeys(Keys);
	Keys.Sort();
	OutStructures.Reserve(Keys.Num());
	for (int32 Key : Keys)
	{
		if (const FKodStructureState* Structure = Structures.Find(Key))
		{
			OutStructures.Add(*Structure);
		}
	}
}

bool UKodSimSubsystem::TryGetState(FKodEntityId Id, FKodSimEntityState& OutState) const
{
	if (const FKodSimEntityState* State = States.Find(Id.Value))
	{
		OutState = *State;
		return true;
	}
	return false;
}

FVector UKodSimSubsystem::GetPresentationPosition(FKodEntityId Id, float Alpha) const
{
	const FKodSimEntityState* State = States.Find(Id.Value);
	if (!State)
	{
		return FVector::ZeroVector;
	}
	const FVector* Prev = PrevPositions.Find(Id.Value);
	const FVector From = Prev ? *Prev : State->Position;
	return FMath::Lerp(From, State->Position, Alpha);
}

int32 UKodSimSubsystem::GetPendingOrderCount() const
{
	int32 Count = 0;
	for (const TPair<int32, FKodSimEntityState>& Pair : States)
	{
		if (Pair.Value.HasPendingOrder())
		{
			++Count;
		}
	}
	return Count;
}

bool UKodSimSubsystem::IsWorldIdle() const
{
	return GetPendingOrderCount() == 0;
}

FString UKodSimSubsystem::ComputeIdleHash() const
{
	// Stable when idle (no orders / no micro-integrate). Quantized pose + DA-driven HP.
	// TeamId on combat entities is omitted so that prefix stays ids / pose / HP / order / gather.
	// Banks, supply, resource nodes, drop-offs, and structure queues are appended after that prefix as integers.
	// Visual bob, actor yaw, cargo mesh, and the resource readout are not appended.
	// RallyPoint is. UnitExit/Door replaces the footprint exit and that rally, so the
	// logged hash changes when a building has that socket. That change is expected.
	// Kod.Soak.AutoplayDeterminism compares two runs; it does not keep a golden hash.
	TArray<uint8> Bytes;
	auto Append = [&Bytes](const void* Data, int32 Size)
	{
		const int32 Offset = Bytes.Num();
		Bytes.AddUninitialized(Size);
		FMemory::Memcpy(Bytes.GetData() + Offset, Data, Size);
	};

	TArray<int32> Keys;
	States.GetKeys(Keys);
	Keys.Sort();

	for (int32 Key : Keys)
	{
		const FKodSimEntityState& S = States.FindChecked(Key);
		Append(&Key, sizeof(Key));

		const FString DefinitionName = S.DefinitionId.ToString();
		const int32 DefHash = static_cast<int32>(FCrc::StrCrc32(*DefinitionName));
		Append(&DefHash, sizeof(DefHash));

		const float Q = FMath::Max(KINDA_SMALL_NUMBER, PoseQuantizeUU);
		const int32 QX = FMath::RoundToInt(S.Position.X / Q);
		const int32 QY = FMath::RoundToInt(S.Position.Y / Q);
		const int32 QZ = FMath::RoundToInt(S.Position.Z / Q);
		Append(&QX, sizeof(QX));
		Append(&QY, sizeof(QY));
		Append(&QZ, sizeof(QZ));

		const int32 HP = FMath::RoundToInt(S.Health);
		const int32 MaxHP = FMath::RoundToInt(S.MaxHealth);
		Append(&HP, sizeof(HP));
		Append(&MaxHP, sizeof(MaxHP));

		const uint8 OrderByte = static_cast<uint8>(S.Order);
		Append(&OrderByte, sizeof(OrderByte));
		const uint8 ExplicitAttack = S.bExplicitAttack ? 1 : 0;
		Append(&ExplicitAttack, sizeof(ExplicitAttack));

		const uint8 GatherPhase = static_cast<uint8>(S.GatherPhase);
		Append(&GatherPhase, sizeof(GatherPhase));
		Append(&S.GatherNode.Value, sizeof(S.GatherNode.Value));
		Append(&S.GatherDropOff.Value, sizeof(S.GatherDropOff.Value));
		const uint8 GatherType = static_cast<uint8>(S.GatherType);
		Append(&GatherType, sizeof(GatherType));
		Append(&S.CargoJadeite, sizeof(S.CargoJadeite));
		Append(&S.CargoLuminene, sizeof(S.CargoLuminene));
		const uint8 TrickleCargo = S.bCargoFromTrickle ? 1 : 0;
		Append(&TrickleCargo, sizeof(TrickleCargo));
		Append(&S.HarvestTicksRemaining, sizeof(S.HarvestTicksRemaining));
		Append(&S.GatherStallTicks, sizeof(S.GatherStallTicks));
		const uint8 StallRetarget = S.bGatherStallRetargeted ? 1 : 0;
		Append(&StallRetarget, sizeof(StallRetarget));
		Append(&S.SupplyCost, sizeof(S.SupplyCost));
		Append(&S.WeaponCooldownTicks, sizeof(S.WeaponCooldownTicks));
		Append(&S.CooldownTicksRemaining, sizeof(S.CooldownTicksRemaining));
		Append(&S.BodyRadiusUU, sizeof(S.BodyRadiusUU));
		Append(&S.SightRadiusUU, sizeof(S.SightRadiusUU));
		Append(&S.ArmorPoints, sizeof(S.ArmorPoints));
		Append(&S.Energy, sizeof(S.Energy));
		Append(&S.EnergyMax, sizeof(S.EnergyMax));
		Append(&S.EnergyRegenAcc, sizeof(S.EnergyRegenAcc));
		Append(&S.HealAcc, sizeof(S.HealAcc));
		const uint8 Autocast = S.bAutocastHeal ? 1 : 0;
		Append(&Autocast, sizeof(Autocast));
		const uint8 Biological = S.bBiological ? 1 : 0;
		Append(&Biological, sizeof(Biological));
		const uint8 Infantry = S.bInfantry ? 1 : 0;
		Append(&Infantry, sizeof(Infantry));
		const uint8 Healer = S.bHealer ? 1 : 0;
		Append(&Healer, sizeof(Healer));
		const uint8 Flyer = S.bFlyer ? 1 : 0;
		Append(&Flyer, sizeof(Flyer));
		Append(&S.CargoSize, sizeof(S.CargoSize));
		Append(&S.InsideBunker.Value, sizeof(S.InsideBunker.Value));
		Append(&S.LoadTarget.Value, sizeof(S.LoadTarget.Value));
		Append(&S.WeaponBackswingTicks, sizeof(S.WeaponBackswingTicks));
		Append(&S.BackswingTicksRemaining, sizeof(S.BackswingTicksRemaining));
		Append(&S.FollowTarget.Value, sizeof(S.FollowTarget.Value));
		Append(&S.PathId, sizeof(S.PathId));
		Append(&S.PathCursor, sizeof(S.PathCursor));
		Append(&S.PathOffsetX, sizeof(S.PathOffsetX));
		Append(&S.PathOffsetY, sizeof(S.PathOffsetY));
		const int32 PatrolX = FMath::RoundToInt(S.PatrolAnchor.X);
		const int32 PatrolY = FMath::RoundToInt(S.PatrolAnchor.Y);
		Append(&PatrolX, sizeof(PatrolX));
		Append(&PatrolY, sizeof(PatrolY));
		const int32 QueueCount = S.OrderQueue.Num();
		Append(&QueueCount, sizeof(QueueCount));
		for (const FKodQueuedOrder& Step : S.OrderQueue)
		{
			const uint8 Kind = static_cast<uint8>(Step.Order);
			Append(&Kind, sizeof(Kind));
			Append(&Step.X, sizeof(Step.X));
			Append(&Step.Y, sizeof(Step.Y));
			Append(&Step.TargetId, sizeof(Step.TargetId));
			Append(&Step.PathId, sizeof(Step.PathId));
			Append(&Step.OffsetX, sizeof(Step.OffsetX));
			Append(&Step.OffsetY, sizeof(Step.OffsetY));
		}
		const uint8 SupplyCounted = S.bSupplyCounted ? 1 : 0;
		Append(&SupplyCounted, sizeof(SupplyCounted));
		Append(&S.BuildSite.Value, sizeof(S.BuildSite.Value));
		const FString AbilityName = S.ActiveAbilityId.ToString();
		const int32 AbilityLen = AbilityName.Len();
		Append(&AbilityLen, sizeof(AbilityLen));
		if (AbilityLen > 0)
		{
			Append(*AbilityName, AbilityLen * static_cast<int32>(sizeof(TCHAR)));
		}
		Append(&S.AbilityTicksRemaining, sizeof(S.AbilityTicksRemaining));
		Append(&S.AbilityCooldownTicks, sizeof(S.AbilityCooldownTicks));
		Append(&S.AbilityMovePermille, sizeof(S.AbilityMovePermille));
		Append(&S.AbilityAttackPermille, sizeof(S.AbilityAttackPermille));
		const int32 GatherRate = S.GatherRatePermille > 0 ? S.GatherRatePermille : 1000;
		Append(&GatherRate, sizeof(GatherRate));
		Append(&S.OrbitAngle, sizeof(S.OrbitAngle));
		Append(&S.OrbitCenterX, sizeof(S.OrbitCenterX));
		Append(&S.OrbitCenterY, sizeof(S.OrbitCenterY));
		Append(&S.OrbitRadiusUU, sizeof(S.OrbitRadiusUU));
		Append(&S.OrbitTicksRemaining, sizeof(S.OrbitTicksRemaining));
	}

	// Economy is integers only: team id + bank, then node id + type + remaining + quantized position,
	// then drop-off id + team + quantized position.
	TArray<int32> TeamKeys;
	Banks.GetKeys(TeamKeys);
	TArray<int32> SupplyKeys;
	Supply.GetKeys(SupplyKeys);
	for (int32 SupplyTeam : SupplyKeys)
	{
		TeamKeys.AddUnique(SupplyTeam);
	}
	TeamKeys.Sort();
	const int32 BankCount = TeamKeys.Num();
	Append(&BankCount, sizeof(BankCount));
	for (int32 TeamId : TeamKeys)
	{
		const FKodResourceCost* BankPtr = Banks.Find(TeamId);
		const FKodResourceCost Bank = BankPtr ? *BankPtr : FKodResourceCost();
		Append(&TeamId, sizeof(TeamId));
		Append(&Bank.Jadeite, sizeof(Bank.Jadeite));
		Append(&Bank.Luminene, sizeof(Bank.Luminene));
		const FKodSupplyState* SupplyRow = Supply.Find(TeamId);
		const int32 SupplyUsed = SupplyRow ? SupplyRow->Used : 0;
		const int32 SupplyCap = SupplyRow ? SupplyRow->Cap : 0;
		Append(&SupplyUsed, sizeof(SupplyUsed));
		Append(&SupplyCap, sizeof(SupplyCap));
	}

	TArray<int32> NodeKeys;
	ResourceNodes.GetKeys(NodeKeys);
	NodeKeys.Sort();
	const int32 NodeCount = NodeKeys.Num();
	Append(&NodeCount, sizeof(NodeCount));
	const float NodeQ = FMath::Max(KINDA_SMALL_NUMBER, PoseQuantizeUU);
	for (int32 NodeKey : NodeKeys)
	{
		const FKodResourceNodeState& Node = ResourceNodes.FindChecked(NodeKey);
		Append(&NodeKey, sizeof(NodeKey));
		const uint8 TypeByte = static_cast<uint8>(Node.Type);
		Append(&TypeByte, sizeof(TypeByte));
		Append(&Node.Remaining, sizeof(Node.Remaining));
		const int32 QX = FMath::RoundToInt(Node.Position.X / NodeQ);
		const int32 QY = FMath::RoundToInt(Node.Position.Y / NodeQ);
		const int32 QZ = FMath::RoundToInt(Node.Position.Z / NodeQ);
		Append(&QX, sizeof(QX));
		Append(&QY, sizeof(QY));
		Append(&QZ, sizeof(QZ));
	}

	TArray<int32> DropKeys;
	DropOffs.GetKeys(DropKeys);
	DropKeys.Sort();
	const int32 DropCount = DropKeys.Num();
	Append(&DropCount, sizeof(DropCount));
	for (int32 DropKey : DropKeys)
	{
		const FKodDropOffState& Drop = DropOffs.FindChecked(DropKey);
		Append(&DropKey, sizeof(DropKey));
		Append(&Drop.TeamId, sizeof(Drop.TeamId));
		const int32 QX = FMath::RoundToInt(Drop.Position.X / NodeQ);
		const int32 QY = FMath::RoundToInt(Drop.Position.Y / NodeQ);
		const int32 QZ = FMath::RoundToInt(Drop.Position.Z / NodeQ);
		Append(&QX, sizeof(QX));
		Append(&QY, sizeof(QY));
		Append(&QZ, sizeof(QZ));
	}

	TArray<int32> StructureKeys;
	Structures.GetKeys(StructureKeys);
	StructureKeys.Sort();
	const int32 StructureCount = StructureKeys.Num();
	Append(&StructureCount, sizeof(StructureCount));
	for (int32 StructureKey : StructureKeys)
	{
		const FKodStructureState& Structure = Structures.FindChecked(StructureKey);
		Append(&StructureKey, sizeof(StructureKey));
		Append(&Structure.TeamId, sizeof(Structure.TeamId));
		Append(&Structure.SupplyProvided, sizeof(Structure.SupplyProvided));
		const int32 SlotCount = Structure.Slots.Num();
		Append(&SlotCount, sizeof(SlotCount));
		for (const FKodProductionSlot& Slot : Structure.Slots)
		{
			const FString UnitName = Slot.UnitId.ToString();
			const int32 NameLen = UnitName.Len();
			Append(&NameLen, sizeof(NameLen));
			if (NameLen > 0)
			{
				Append(*UnitName, NameLen * static_cast<int32>(sizeof(TCHAR)));
			}
			Append(&Slot.BuildTime, sizeof(Slot.BuildTime));
			const uint8 Started = Slot.bStarted ? 1 : 0;
			Append(&Started, sizeof(Started));
		}
		Append(&Structure.ActiveTicksRemaining, sizeof(Structure.ActiveTicksRemaining));
		Append(&Structure.CellX, sizeof(Structure.CellX));
		Append(&Structure.CellY, sizeof(Structure.CellY));
		Append(&Structure.FootprintX, sizeof(Structure.FootprintX));
		Append(&Structure.FootprintY, sizeof(Structure.FootprintY));
		Append(&Structure.BuildTimeTotal, sizeof(Structure.BuildTimeTotal));
		Append(&Structure.BuildTicksDone, sizeof(Structure.BuildTicksDone));
		const uint8 BuildComplete = Structure.bComplete ? 1 : 0;
		Append(&BuildComplete, sizeof(BuildComplete));
		Append(&Structure.BuilderId.Value, sizeof(Structure.BuilderId.Value));
		Append(&Structure.VentId.Value, sizeof(Structure.VentId.Value));
		const uint8 Blocked = Structure.bSupplyBlocked ? 1 : 0;
		Append(&Blocked, sizeof(Blocked));
		const int32 RallyNode = Structure.bRallyNode ? Structure.RallyNode.Value : 0;
		Append(&RallyNode, sizeof(RallyNode));
		Append(&Structure.RallyUnit.Value, sizeof(Structure.RallyUnit.Value));
		const uint8 Lowered = Structure.bLowered ? 1 : 0;
		Append(&Lowered, sizeof(Lowered));
		Append(&Structure.CargoCap, sizeof(Structure.CargoCap));
		TArray<int32> Cargo = Structure.Cargo;
		Cargo.Sort();
		const int32 CargoCount = Cargo.Num();
		Append(&CargoCount, sizeof(CargoCount));
		for (int32 Rider : Cargo)
		{
			Append(&Rider, sizeof(Rider));
		}
		const int32 ResearchCount = Structure.Research.Num();
		Append(&ResearchCount, sizeof(ResearchCount));
		Append(&Structure.ResearchTicksRemaining, sizeof(Structure.ResearchTicksRemaining));
		for (const FKodProductionSlot& Job : Structure.Research)
		{
			const FString UpgradeName = Job.UnitId.ToString();
			const int32 UpgradeCrc = static_cast<int32>(FCrc::StrCrc32(*UpgradeName));
			Append(&UpgradeCrc, sizeof(UpgradeCrc));
			Append(&Job.BuildTime, sizeof(Job.BuildTime));
		}
		const int32 RQX = FMath::RoundToInt(Structure.RallyPoint.X / NodeQ);
		const int32 RQY = FMath::RoundToInt(Structure.RallyPoint.Y / NodeQ);
		const int32 RQZ = FMath::RoundToInt(Structure.RallyPoint.Z / NodeQ);
		Append(&RQX, sizeof(RQX));
		Append(&RQY, sizeof(RQY));
		Append(&RQZ, sizeof(RQZ));
		Append(&Structure.BonusSupply, sizeof(Structure.BonusSupply));
		Append(&Structure.BonusSupplyTicksRemaining, sizeof(Structure.BonusSupplyTicksRemaining));
		Append(&Structure.GathererCd, sizeof(Structure.GathererCd));
		Append(&Structure.SupplyCd, sizeof(Structure.SupplyCd));
		Append(&Structure.IsrCd, sizeof(Structure.IsrCd));
		const uint8 Airborne = Structure.bAirborne ? 1 : 0;
		const uint8 LiftMoving = Structure.bLiftMoving ? 1 : 0;
		Append(&Airborne, sizeof(Airborne));
		Append(&LiftMoving, sizeof(LiftMoving));
		Append(&Structure.LiftGoalX, sizeof(Structure.LiftGoalX));
		Append(&Structure.LiftGoalY, sizeof(Structure.LiftGoalY));
		Append(&Structure.LiftSpeedUU, sizeof(Structure.LiftSpeedUU));
		const uint8 Turret = Structure.bTurret ? 1 : 0;
		const uint8 TurretAir = Structure.bAttacksAir ? 1 : 0;
		Append(&Turret, sizeof(Turret));
		Append(&Structure.AttackDamage, sizeof(Structure.AttackDamage));
		Append(&Structure.AttackRangeUU, sizeof(Structure.AttackRangeUU));
		Append(&TurretAir, sizeof(TurretAir));
		Append(&Structure.TurretHpBonus, sizeof(Structure.TurretHpBonus));
		Append(&Structure.BonusVsLight, sizeof(Structure.BonusVsLight));
		Append(&Structure.SalvageTicksRemaining, sizeof(Structure.SalvageTicksRemaining));
		Append(&Structure.RepairCreditMilli, sizeof(Structure.RepairCreditMilli));
		Append(&Structure.RepairGasMilli, sizeof(Structure.RepairGasMilli));
	}

	const uint8 Outcome = static_cast<uint8>(MatchOutcome);
	Append(&Outcome, sizeof(Outcome));
	const uint8 AiOn = bAiEnabled ? 1 : 0;
	Append(&AiOn, sizeof(AiOn));
	Append(&AiDifficulty, sizeof(AiDifficulty));
	Append(&AiWavesLaunched, sizeof(AiWavesLaunched));
	Append(&AiNextWaveTick, sizeof(AiNextWaveTick));
	const uint8 Seeded = bAiSeeded ? 1 : 0;
	Append(&Seeded, sizeof(Seeded));
	const uint8 ScoutSent = bAiScoutSent ? 1 : 0;
	Append(&ScoutSent, sizeof(ScoutSent));
	const uint8 HasEnemyStart = bHasEnemyStart ? 1 : 0;
	Append(&HasEnemyStart, sizeof(HasEnemyStart));
	Append(&EnemyStartX, sizeof(EnemyStartX));
	Append(&EnemyStartY, sizeof(EnemyStartY));
	TArray<int32> TechTeams;
	TeamTech.GetKeys(TechTeams);
	TechTeams.Sort();
	const int32 TechCount = TechTeams.Num();
	Append(&TechCount, sizeof(TechCount));
	for (int32 TeamId : TechTeams)
	{
		const FKodTeamTech& Tech = TeamTech.FindChecked(TeamId);
		Append(&TeamId, sizeof(TeamId));
		Append(&Tech.InfantryWeapons, sizeof(Tech.InfantryWeapons));
		Append(&Tech.InfantryArmor, sizeof(Tech.InfantryArmor));
		Append(&Tech.SatelliteFeed, sizeof(Tech.SatelliteFeed));
	}
	TArray<int32> FacingTeams;
	TeamFacingYaw.GetKeys(FacingTeams);
	FacingTeams.Sort();
	const int32 FacingCount = FacingTeams.Num();
	Append(&FacingCount, sizeof(FacingCount));
	for (int32 TeamId : FacingTeams)
	{
		const int32 Yaw = TeamFacingYaw.FindChecked(TeamId);
		Append(&TeamId, sizeof(TeamId));
		Append(&Yaw, sizeof(Yaw));
	}
	const uint8 Cheat = bAiCheatVision ? 1 : 0;
	Append(&Cheat, sizeof(Cheat));

	Append(&VisionOriginX, sizeof(VisionOriginX));
	Append(&VisionOriginY, sizeof(VisionOriginY));
	Append(&VisionWidth, sizeof(VisionWidth));
	Append(&VisionHeight, sizeof(VisionHeight));
	TArray<int32> VisionTeams;
	TeamVision.GetKeys(VisionTeams);
	VisionTeams.Sort();
	const int32 VisionTeamCount = VisionTeams.Num();
	Append(&VisionTeamCount, sizeof(VisionTeamCount));
	for (int32 TeamId : VisionTeams)
	{
		Append(&TeamId, sizeof(TeamId));
		const TArray<uint8>& Grid = TeamVision.FindChecked(TeamId);
		const int32 CellCount = Grid.Num();
		Append(&CellCount, sizeof(CellCount));
		if (CellCount > 0)
		{
			Append(Grid.GetData(), CellCount);
		}
		const TArray<FKodSeenStructure>* Seen = SeenStructures.Find(TeamId);
		const int32 SeenCount = Seen ? Seen->Num() : 0;
		Append(&SeenCount, sizeof(SeenCount));
		if (Seen)
		{
			for (const FKodSeenStructure& Entry : *Seen)
			{
				Append(&Entry.Id, sizeof(Entry.Id));
				Append(&Entry.TeamId, sizeof(Entry.TeamId));
				const FString SeenName = Entry.DefinitionId.ToString();
				const int32 SeenDef = static_cast<int32>(FCrc::StrCrc32(*SeenName));
				Append(&SeenDef, sizeof(SeenDef));
				Append(&Entry.PosX, sizeof(Entry.PosX));
				Append(&Entry.PosY, sizeof(Entry.PosY));
				Append(&Entry.FootprintX, sizeof(Entry.FootprintX));
				Append(&Entry.FootprintY, sizeof(Entry.FootprintY));
				Append(&Entry.Health, sizeof(Entry.Health));
				Append(&Entry.MaxHealth, sizeof(Entry.MaxHealth));
			}
		}
		const TArray<FKodSeenUnit>* SeenMobile = SeenUnits.Find(TeamId);
		const int32 SeenUnitCount = SeenMobile ? SeenMobile->Num() : 0;
		Append(&SeenUnitCount, sizeof(SeenUnitCount));
		if (SeenMobile)
		{
			for (const FKodSeenUnit& Entry : *SeenMobile)
			{
				Append(&Entry.Id, sizeof(Entry.Id));
				Append(&Entry.TeamId, sizeof(Entry.TeamId));
				Append(&Entry.PosX, sizeof(Entry.PosX));
				Append(&Entry.PosY, sizeof(Entry.PosY));
			}
		}
	}

	const uint8 ExposedRuleByte = bExposedRule ? 1 : 0;
	Append(&ExposedRuleByte, sizeof(ExposedRuleByte));
	Append(&ExposedTicks, sizeof(ExposedTicks));
	const uint8 Ex0 = bTeam0Exposed ? 1 : 0;
	const uint8 Ex1 = bTeam1Exposed ? 1 : 0;
	const uint8 Warn0 = bTeam0ExposedWarned ? 1 : 0;
	const uint8 Warn1 = bTeam1ExposedWarned ? 1 : 0;
	Append(&Ex0, sizeof(Ex0));
	Append(&Ex1, sizeof(Ex1));
	Append(&Warn0, sizeof(Warn0));
	Append(&Warn1, sizeof(Warn1));
	auto AppendTotals = [&](const FKodTeamTotals& Totals)
	{
		Append(&Totals.Collected, sizeof(Totals.Collected));
		Append(&Totals.Trained, sizeof(Totals.Trained));
		Append(&Totals.Killed, sizeof(Totals.Killed));
		Append(&Totals.Built, sizeof(Totals.Built));
		Append(&Totals.Razed, sizeof(Totals.Razed));
	};
	AppendTotals(Totals0);
	AppendTotals(Totals1);
	auto AppendSamples = [&](const TArray<FKodStatSample>& Samples)
	{
		const int32 Count = Samples.Num();
		Append(&Count, sizeof(Count));
		for (const FKodStatSample& Sample : Samples)
		{
			Append(&Sample.Tick, sizeof(Sample.Tick));
			Append(&Sample.Collected, sizeof(Sample.Collected));
			Append(&Sample.Trained, sizeof(Sample.Trained));
			Append(&Sample.ArmyValue, sizeof(Sample.ArmyValue));
			Append(&Sample.Income, sizeof(Sample.Income));
			Append(&Sample.Killed, sizeof(Sample.Killed));
			Append(&Sample.Built, sizeof(Sample.Built));
			Append(&Sample.Razed, sizeof(Sample.Razed));
		}
	};
	AppendSamples(Samples0);
	AppendSamples(Samples1);
	AppendSamples(OlderSamples0);
	AppendSamples(OlderSamples1);
	const uint8 Cheated = bMatchCheated ? 1 : 0;
	const uint8 Competitive = bCompetitive ? 1 : 0;
	const uint8 Invincible = bCheatInvincible ? 1 : 0;
	const uint8 Instant = bCheatInstantBuild ? 1 : 0;
	const uint8 NoSupply = bCheatNoSupply ? 1 : 0;
	const uint8 Reveal = bCheatReveal ? 1 : 0;
	const uint8 NoCooldown = bCheatNoCooldown ? 1 : 0;
	Append(&Cheated, sizeof(Cheated));
	Append(&Competitive, sizeof(Competitive));
	Append(&Invincible, sizeof(Invincible));
	Append(&Instant, sizeof(Instant));
	Append(&NoSupply, sizeof(NoSupply));
	Append(&Reveal, sizeof(Reveal));
	Append(&NoCooldown, sizeof(NoCooldown));
	Append(&DamageDealt0, sizeof(DamageDealt0));
	Append(&DamageDealt1, sizeof(DamageDealt1));
	Append(&DamageTaken0, sizeof(DamageTaken0));
	Append(&DamageTaken1, sizeof(DamageTaken1));
	Append(&AiOpener, sizeof(AiOpener));
	const uint8 OpenerChosen = bAiOpenerChosen ? 1 : 0;
	const uint8 ScoutSaw = bScoutSawEnemy ? 1 : 0;
	const uint8 Retreating = bAiRetreating ? 1 : 0;
	Append(&OpenerChosen, sizeof(OpenerChosen));
	Append(&ScoutSaw, sizeof(ScoutSaw));
	Append(&Retreating, sizeof(Retreating));
	AppendBalanceHash(Bytes);
	const uint8 PowersOn = bGeneralPowers ? 1 : 0;
	Append(&PowersOn, sizeof(PowersOn));
	Append(&PowerPoints0, sizeof(PowerPoints0));
	Append(&PowerPoints1, sizeof(PowerPoints1));
	const uint8 PowerStartGranted = bPowerStartGranted ? 1 : 0;
	Append(&PowerStartGranted, sizeof(PowerStartGranted));
	Append(&ArtilleryCd0, sizeof(ArtilleryCd0));
	Append(&ArtilleryCd1, sizeof(ArtilleryCd1));
	Append(&ReconCd0, sizeof(ReconCd0));
	Append(&ReconCd1, sizeof(ReconCd1));
	Append(&RepairCd0, sizeof(RepairCd0));
	Append(&RepairCd1, sizeof(RepairCd1));
	const int32 PowerCount = ScheduledPowers.Num();
	Append(&PowerCount, sizeof(PowerCount));
	for (const FKodScheduledPower& Power : ScheduledPowers)
	{
		Append(&Power.TeamId, sizeof(Power.TeamId));
		const int32 NameCrc = static_cast<int32>(FCrc::StrCrc32(*Power.PowerId.ToString()));
		Append(&NameCrc, sizeof(NameCrc));
		Append(&Power.X, sizeof(Power.X));
		Append(&Power.Y, sizeof(Power.Y));
		Append(&Power.TicksLeft, sizeof(Power.TicksLeft));
		Append(&Power.Phase, sizeof(Power.Phase));
		Append(&Power.RadiusUU, sizeof(Power.RadiusUU));
		Append(&Power.Amount, sizeof(Power.Amount));
		Append(&Power.DurationTicks, sizeof(Power.DurationTicks));
	}
	const int32 RampCount = RampCells.Num();
	Append(&RampCount, sizeof(RampCount));
	for (int64 Ramp : RampCells)
	{
		Append(&Ramp, sizeof(Ramp));
	}
	const int32 HighCount = HighGroundCells.Num();
	Append(&HighCount, sizeof(HighCount));
	for (int64 HighCell : HighGroundCells)
	{
		Append(&HighCell, sizeof(HighCell));
	}
	const int32 DressCount = DressBlockCells.Num();
	Append(&DressCount, sizeof(DressCount));
	for (int64 DressCell : DressBlockCells)
	{
		Append(&DressCell, sizeof(DressCell));
	}
	const int32 VisionCount = VisionBlockCells.Num();
	Append(&VisionCount, sizeof(VisionCount));
	for (int64 VisionCell : VisionBlockCells)
	{
		Append(&VisionCell, sizeof(VisionCell));
	}
	const int32 DestructCount = DestructFoots.Num();
	Append(&DestructCount, sizeof(DestructCount));
	for (const FKodDestructFoot& Foot : DestructFoots)
	{
		Append(&Foot.Id, sizeof(Foot.Id));
		Append(&Foot.MinX, sizeof(Foot.MinX));
		Append(&Foot.MinY, sizeof(Foot.MinY));
		Append(&Foot.MaxX, sizeof(Foot.MaxX));
		Append(&Foot.MaxY, sizeof(Foot.MaxY));
	}
	const uint8 ModeByte = static_cast<uint8>(MatchMode);
	Append(&ModeByte, sizeof(ModeByte));
	const int32 CheatCount = CheatUses.Num();
	Append(&CheatCount, sizeof(CheatCount));
	for (const FKodCheatUse& Use : CheatUses)
	{
		Append(&Use.Effect, sizeof(Use.Effect));
		Append(&Use.Count, sizeof(Use.Count));
		Append(&Use.bEnabled, sizeof(Use.bEnabled));
	}
	const int32 AttackAlertCount = AttackAlerts.Num();
	Append(&AttackAlertCount, sizeof(AttackAlertCount));
	for (const FKodAttackAlert& Alert : AttackAlerts)
	{
		Append(&Alert.TeamId, sizeof(Alert.TeamId));
		Append(&Alert.X, sizeof(Alert.X));
		Append(&Alert.Y, sizeof(Alert.Y));
		Append(&Alert.Tick, sizeof(Alert.Tick));
	}
	Append(&PlayerFaction, sizeof(PlayerFaction));
	const uint8 TrainingByte = bFaultlineTraining ? 1 : 0;
	const uint8 WaveSentByte = bFaultlineWaveSent ? 1 : 0;
	Append(&TrainingByte, sizeof(TrainingByte));
	Append(&WaveSentByte, sizeof(WaveSentByte));
	Append(&FaultlineOutpostCount, sizeof(FaultlineOutpostCount));
	Append(&FaultlineWaveX, sizeof(FaultlineWaveX));
	Append(&FaultlineWaveY, sizeof(FaultlineWaveY));
	Append(&FaultlineWaveZ, sizeof(FaultlineWaveZ));
	Append(&FaultlineHomeX, sizeof(FaultlineHomeX));
	Append(&FaultlineHomeY, sizeof(FaultlineHomeY));
	Append(&FaultlineHomeZ, sizeof(FaultlineHomeZ));
	Append(&FaultlineWavePhase, sizeof(FaultlineWavePhase));
	Append(&FaultlineWaveDueTick, sizeof(FaultlineWaveDueTick));
	Append(&FaultlineWaveStartTick, sizeof(FaultlineWaveStartTick));
	Append(&FaultlineRetreatTick, sizeof(FaultlineRetreatTick));
	Append(&FaultlineWaveStartHp, sizeof(FaultlineWaveStartHp));
	Append(&FaultlineWaveExpected, sizeof(FaultlineWaveExpected));
	Append(&bFaultlineGuardsSent, sizeof(bFaultlineGuardsSent));
	Append(&FaultlineOutpostX, sizeof(FaultlineOutpostX));
	Append(&FaultlineOutpostY, sizeof(FaultlineOutpostY));
	Append(&FaultlineOutpostZ, sizeof(FaultlineOutpostZ));
	Append(&FaultlineLaneX, sizeof(FaultlineLaneX));
	Append(&FaultlineLaneY, sizeof(FaultlineLaneY));
	Append(&FaultlineLaneZ, sizeof(FaultlineLaneZ));
	Append(&bFaultlineHasLane, sizeof(bFaultlineHasLane));
	Append(&FaultlineBarracksX, sizeof(FaultlineBarracksX));
	Append(&FaultlineBarracksY, sizeof(FaultlineBarracksY));
	Append(&FaultlineBarracksZ, sizeof(FaultlineBarracksZ));
	Append(&FaultlineDelayTicks, sizeof(FaultlineDelayTicks));
	Append(&FaultlineRetreatTicks, sizeof(FaultlineRetreatTicks));
	Append(&FaultlineHpPermille, sizeof(FaultlineHpPermille));
	Append(&FaultlineGuardFighters, sizeof(FaultlineGuardFighters));
	Append(&FaultlineGuardBlades, sizeof(FaultlineGuardBlades));
	Append(&FaultlineGuardWorkers, sizeof(FaultlineGuardWorkers));
	Append(&FaultlineWaveFighters, sizeof(FaultlineWaveFighters));
	Append(&FaultlineWaveBlades, sizeof(FaultlineWaveBlades));
	Append(&FaultlineFighterLeashUU, sizeof(FaultlineFighterLeashUU));
	Append(&FaultlineFighterAggroUU, sizeof(FaultlineFighterAggroUU));
	Append(&FaultlineBladeLeashUU, sizeof(FaultlineBladeLeashUU));
	Append(&FaultlineBladeAggroUU, sizeof(FaultlineBladeAggroUU));
	Append(&FaultlineFighterHp, sizeof(FaultlineFighterHp));
	Append(&FaultlineFighterDamage, sizeof(FaultlineFighterDamage));
	Append(&FaultlineFighterRangeUU, sizeof(FaultlineFighterRangeUU));
	Append(&FaultlineFighterSpeed, sizeof(FaultlineFighterSpeed));
	Append(&FaultlineFighterArmor, sizeof(FaultlineFighterArmor));
	Append(&FaultlineFighterCooldownTicks, sizeof(FaultlineFighterCooldownTicks));
	Append(&FaultlineFighterRegenMilli, sizeof(FaultlineFighterRegenMilli));
	Append(&FaultlineBladeHp, sizeof(FaultlineBladeHp));
	Append(&FaultlineBladeDamage, sizeof(FaultlineBladeDamage));
	Append(&FaultlineBladeRangeUU, sizeof(FaultlineBladeRangeUU));
	Append(&FaultlineBladeSpeed, sizeof(FaultlineBladeSpeed));
	Append(&FaultlineBladeArmor, sizeof(FaultlineBladeArmor));
	Append(&FaultlineBladeCooldownTicks, sizeof(FaultlineBladeCooldownTicks));
	Append(&FaultlineBladeRegenMilli, sizeof(FaultlineBladeRegenMilli));
	Append(&FaultlineWorkerHp, sizeof(FaultlineWorkerHp));
	Append(&FaultlineWorkerSpeed, sizeof(FaultlineWorkerSpeed));
	Append(&FaultlineWorkerRegenMilli, sizeof(FaultlineWorkerRegenMilli));
	const int32 ScriptedCount = ScriptedUnits.Num();
	Append(&ScriptedCount, sizeof(ScriptedCount));
	for (const FKodScriptedUnit& Scripted : ScriptedUnits)
	{
		Append(&Scripted.Id, sizeof(Scripted.Id));
		Append(&Scripted.Role, sizeof(Scripted.Role));
		Append(&Scripted.HomeX, sizeof(Scripted.HomeX));
		Append(&Scripted.HomeY, sizeof(Scripted.HomeY));
		Append(&Scripted.LeashUU, sizeof(Scripted.LeashUU));
		Append(&Scripted.AggroUU, sizeof(Scripted.AggroUU));
		Append(&Scripted.RegenMilliPerSec, sizeof(Scripted.RegenMilliPerSec));
		Append(&Scripted.RegenAcc, sizeof(Scripted.RegenAcc));
		Append(&Scripted.bOrdered, sizeof(Scripted.bOrdered));
		Append(&Scripted.LastHealth, sizeof(Scripted.LastHealth));
	}
	const int32 LaneCount = FaultlineLanePoints.Num();
	Append(&LaneCount, sizeof(LaneCount));
	for (const FVector& LanePoint : FaultlineLanePoints)
	{
		const int32 LaneX = FMath::RoundToInt(LanePoint.X);
		const int32 LaneY = FMath::RoundToInt(LanePoint.Y);
		Append(&LaneX, sizeof(LaneX));
		Append(&LaneY, sizeof(LaneY));
	}
	Append(&WaveLaneCursor, sizeof(WaveLaneCursor));
	Append(&AutoplayLaneCursor, sizeof(AutoplayLaneCursor));
	Append(&FaultlineWaveFocusId, sizeof(FaultlineWaveFocusId));

	const uint64 Hash = Bytes.Num() > 0
		? CityHash64(reinterpret_cast<const char*>(Bytes.GetData()), Bytes.Num())
		: 0ull;
	return FString::Printf(TEXT("%016llX"), Hash);
}
