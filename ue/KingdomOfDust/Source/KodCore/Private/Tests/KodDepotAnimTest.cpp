#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Sim/KodBuildTypes.h"
#include "Sim/KodDepotClips.h"
#include "Sim/KodProductionTypes.h"
#include "Sim/KodSimSubsystem.h"
#include "Warden/KodWardenPaths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKodDepotAnimTest,
	"Kod.Build.DepotAnim",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace
{
	AActor* SpawnRooted(UWorld* World, const FVector& At)
	{
		AActor* Actor = World->SpawnActor<AActor>();
		if (!Actor)
		{
			return nullptr;
		}
		USceneComponent* Root = NewObject<USceneComponent>(Actor, TEXT("Root"));
		Actor->SetRootComponent(Root);
		Root->RegisterComponent();
		Actor->SetActorLocation(At);
		return Actor;
	}

	FKodEntityId SpawnDepot(UWorld* World, UKodSimSubsystem* Sim, int32 CellX, int32 CellY)
	{
		const FVector At(KodBuildGrid::CellCenter(CellX), KodBuildGrid::CellCenter(CellY), 0.f);
		AActor* Actor = SpawnRooted(World, At);
		FKodStructureSpec Spec;
		Spec.DefinitionId = FName(KodWardenPaths::Id_SupplyDepot);
		Spec.FootprintX = 2;
		Spec.FootprintY = 2;
		Spec.MaxHealth = 400.f;
		Spec.TeamId = 0;
		return Sim->RegisterStructure(Actor, Spec);
	}

	FKodEntityId SpawnUnit(UWorld* World, UKodSimSubsystem* Sim, const FVector& At, int32 TeamId, float Damage)
	{
		AActor* Actor = SpawnRooted(World, At);
		const FKodEntityId Id = Sim->RegisterEntity(Actor);
		Sim->ConfigureEntity(Id, FName(TEXT("DepotTester")), 120.f, 400.f, 16.f, true, Damage, 2000.f, 0.05f, TeamId, false, 34, 2000);
		return Id;
	}

	bool FootBlocked(UKodSimSubsystem* Sim, const FKodStructureState& Structure)
	{
		const int32 MaxX = Structure.CellX + FMath::Max(1, Structure.FootprintX);
		const int32 MaxY = Structure.CellY + FMath::Max(1, Structure.FootprintY);
		for (int32 X = Structure.CellX; X < MaxX; ++X)
		{
			for (int32 Y = Structure.CellY; Y < MaxY; ++Y)
			{
				if (!Sim->IsCellBlocked(X, Y, 0, true))
				{
					return false;
				}
			}
		}
		return true;
	}

	bool FootOpen(UKodSimSubsystem* Sim, const FKodStructureState& Structure)
	{
		const int32 MaxX = Structure.CellX + FMath::Max(1, Structure.FootprintX);
		const int32 MaxY = Structure.CellY + FMath::Max(1, Structure.FootprintY);
		for (int32 X = Structure.CellX; X < MaxX; ++X)
		{
			for (int32 Y = Structure.CellY; Y < MaxY; ++Y)
			{
				if (Sim->IsCellBlocked(X, Y, 0, true))
				{
					return false;
				}
			}
		}
		return true;
	}

	bool KillDepot(UKodSimSubsystem* Sim, UWorld* World, FKodEntityId DepotId, const FVector& At)
	{
		const FKodEntityId Attacker = SpawnUnit(World, Sim, At, 1, 8000.f);
		Sim->IssueAttack(Attacker, DepotId);
		for (int32 Step = 0; Step < 8; ++Step)
		{
			Sim->AdvanceSimTicks(1);
			FKodSimEntityState Combat;
			if (!Sim->TryGetState(DepotId, Combat) || Combat.Health <= 0.f)
			{
				return true;
			}
		}
		return false;
	}
}

bool FKodDepotAnimTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FKodDepotClipTable& Clips = FKodDepotClipTable::Get();
	const FName LowerId = FName(KodDepotClipId::Lower);
	const FName RaiseId = FName(KodDepotClipId::Raise);
	const FKodDepotNotify* Nav = Clips.FindNotify(LowerId, FName(KodDepotClipId::NavWalkable));
	const int32 LowerTicks = Clips.ClipTicks(LowerId);
	const int32 RaiseTicks = Clips.ClipTicks(RaiseId);
	const int32 OpenAt = Nav ? Nav->Tick() : LowerTicks;
	TestEqual(TEXT("walkable follows the notify or the lower clip"), Clips.WalkableTick(), OpenAt);
	const FKodDepotNotify* RaiseStart = Clips.FindNotify(RaiseId, FName(KodDepotClipId::RaiseStart));
	TestEqual(TEXT("raise blocks at the notify or at 0"), Clips.RaiseBlockTick(), RaiseStart ? RaiseStart->Tick() : 0);
	const FKodDepotClip* DeathClip = Clips.FindClip(FName(KodDepotClipId::Death));
	const FKodDepotClip* LoweredDeathClip = Clips.FindClip(FName(KodDepotClipId::DeathLowered));
	TestNotNull(TEXT("death clip"), DeathClip);
	TestNotNull(TEXT("lowered death clip"), LoweredDeathClip);
	const int32 DeathTicks = DeathClip ? DeathClip->Ticks() : 0;
	TestEqual(TEXT("death length"), Clips.ClipTicks(FName(KodDepotClipId::Death)), DeathTicks);
	TestEqual(TEXT("lowered death length"), Clips.ClipTicks(FName(KodDepotClipId::DeathLowered)), LoweredDeathClip ? LoweredDeathClip->Ticks() : 0);

	const FString AnimV4 = TEXT("{\"fps\":30,\"clips\":{\"A_SupplyDepot_Lower\":{\"frames\":26,\"loop\":false,\"notifies\":[{\"name\":\"AN_Depot_Slam\",\"frame\":8,\"time_s\":0.266667,\"sockets\":[\"SOCKET_Dust_Ground\"]},{\"name\":\"AN_Depot_Lock\",\"frame\":16,\"time_s\":0.533333,\"sockets\":[]},{\"name\":\"AN_Depot_Latch\",\"frame\":26,\"time_s\":0.866667,\"sockets\":[\"SOCKET_Door\"]}]},\"A_SupplyDepot_Raise\":{\"frames\":25,\"loop\":false,\"notifies\":[]}}}");
	const FKodDepotClipTable V4 = FKodDepotClipTable::Parse(AnimV4);
	const int32 V4LowerTicks = KodBuildTicks::SecondsToBuildTicks(26.f / 30.f);
	const int32 V4RaiseTicks = KodBuildTicks::SecondsToBuildTicks(25.f / 30.f);
	TestEqual(TEXT("v4 lower length"), V4.ClipTicks(LowerId), V4LowerTicks);
	TestEqual(TEXT("v4 raise length"), V4.ClipTicks(RaiseId), V4RaiseTicks);
	TestEqual(TEXT("v4 walkable fallback is the lower clip"), V4.WalkableTick(), V4LowerTicks);
	TestEqual(TEXT("v4 raise blocks at 0"), V4.RaiseBlockTick(), 0);
	const FName UnknownNames[] = {FName(TEXT("AN_Depot_Slam")), FName(TEXT("AN_Depot_Lock")), FName(TEXT("AN_Depot_Latch"))};
	for (const FName& UnknownName : UnknownNames)
	{
		TestNotNull(*FString::Printf(TEXT("%s accepted"), *UnknownName.ToString()), V4.FindNotify(LowerId, UnknownName));
		const FKodDepotFxSlot* Slot = V4.FindFx(UnknownName);
		TestNotNull(*FString::Printf(TEXT("%s fx slot"), *UnknownName.ToString()), Slot);
		if (Slot)
		{
			TestTrue(*FString::Printf(TEXT("%s empty slot is a no-op"), *UnknownName.ToString()), Slot->IsEmpty());
		}
	}
	const FString AnimV4Nav = TEXT("{\"fps\":30,\"clips\":{\"A_SupplyDepot_Lower\":{\"frames\":26,\"loop\":false,\"notifies\":[{\"name\":\"AN_Depot_NavWalkable\",\"frame\":26,\"time_s\":0.866667,\"sockets\":[]}]}}}");
	const FKodDepotClipTable V4Nav = FKodDepotClipTable::Parse(AnimV4Nav);
	TestEqual(TEXT("v4 walkable uses the notify time"), V4Nav.WalkableTick(), KodBuildTicks::SecondsToBuildTicks(26.f / 30.f));

	const int32 OpenSteps = FMath::Max(Clips.WalkableTick(), 1);
	const int32 LowerSteps = FMath::Max(LowerTicks, 1);
	const int32 RaiseSteps = FMath::Max(RaiseTicks, 1);
	const int32 DeathSteps = FMath::Max(DeathTicks, 1);
	const int32 MidSteps = OpenSteps > 2 ? OpenSteps / 2 : 1;
	const int32 RaiseMid = RaiseSteps > 2 ? RaiseSteps / 2 : 1;
	const FKodDepotNotify* Vent = Clips.FindNotify(FName(KodDepotClipId::Idle), FName(KodDepotClipId::VentTick));
	TestNotNull(TEXT("vent tick"), Vent);
	if (Vent)
	{
		TestEqual(TEXT("vent frame"), Vent->Frame, 44);
		TestEqual(TEXT("vent socket"), Vent->Sockets.Num(), 1);
		TestTrue(TEXT("vent chance"), Vent->Chance > 0.34f && Vent->Chance < 0.36f);
	}
	const FKodDepotNotify* LoweredDeath = Clips.FindNotify(FName(KodDepotClipId::DeathLowered), FName(KodDepotClipId::DeathExplosion));
	TestNotNull(TEXT("lowered death explosion"), LoweredDeath);
	if (LoweredDeath)
	{
		TestEqual(TEXT("lowered death sockets"), LoweredDeath->Sockets.Num(), 2);
	}
	const FKodDepotFxSlot* EmptyFx = Clips.FindFx(FName(KodDepotClipId::Dust));
	TestNotNull(TEXT("dust fx slot"), EmptyFx);
	if (EmptyFx)
	{
		TestTrue(TEXT("empty fx is a no-op"), EmptyFx->IsEmpty());
	}

	const FString PackageName = FString::Printf(TEXT("/Temp/KodDepotAnim_%s"), *FGuid::NewGuid().ToString());
	UPackage* Package = CreatePackage(*PackageName);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("KodDepotAnim")), Package, true);
	TestNotNull(TEXT("world"), World);
	if (!World)
	{
		return false;
	}
	FURL Url;
	World->InitializeActorsForPlay(Url);
	UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>();
	TestNotNull(TEXT("sim"), Sim);
	if (!Sim)
	{
		World->DestroyWorld(false);
		return false;
	}

	FKodBuildSpec DepotSpec;
	DepotSpec.TypeId = FName(KodWardenPaths::Id_SupplyDepot);
	DepotSpec.bCanLower = true;
	DepotSpec.FootprintX = 2;
	DepotSpec.FootprintY = 2;
	DepotSpec.MaxHealth = 400.f;
	Sim->RegisterBuildSpec(DepotSpec);
	FKodBuildSpec BarracksSpec;
	BarracksSpec.TypeId = FName(KodWardenPaths::Id_Barracks);
	BarracksSpec.FootprintX = 3;
	BarracksSpec.FootprintY = 3;
	BarracksSpec.MaxHealth = 1000.f;
	Sim->RegisterBuildSpec(BarracksSpec);
	Sim->SetPlayableWorldBox(FBox(FVector(-4000.f, -4000.f, -500.f), FVector(40000.f, 40000.f, 2000.f)));

	const FKodEntityId DepotId = SpawnDepot(World, Sim, 10, 10);
	FKodStructureState Depot;
	TestTrue(TEXT("depot registered"), Sim->TryGetStructure(DepotId, Depot));
	TestTrue(TEXT("depot can lower"), Depot.bCanLower);
	TestEqual(TEXT("starts raised"), static_cast<int32>(Depot.DepotPose), static_cast<int32>(EKodDepotPose::Raised));
	TestTrue(TEXT("raised blocks the footprint"), FootBlocked(Sim, Depot));
	TestTrue(TEXT("raised clip"), KodDepotActiveClip(Depot) == FName(KodDepotClipId::Idle));

	TestTrue(TEXT("lower command"), Sim->ToggleDepot(DepotId));
	Sim->TryGetStructure(DepotId, Depot);
	TestTrue(TEXT("command is lowered"), Depot.bLowered);
	TestEqual(TEXT("lowering"), static_cast<int32>(Depot.DepotPose), static_cast<int32>(EKodDepotPose::Lowering));
	TestTrue(TEXT("lower clip"), KodDepotActiveClip(Depot) == FName(KodDepotClipId::Lower));
	TestTrue(TEXT("still blocked on the lower command"), FootBlocked(Sim, Depot));
	if (OpenSteps > 1)
	{
		Sim->AdvanceSimTicks(OpenSteps - 1);
		Sim->TryGetStructure(DepotId, Depot);
		TestEqual(TEXT("still lowering before the notify"), static_cast<int32>(Depot.DepotPose), static_cast<int32>(EKodDepotPose::Lowering));
		TestTrue(TEXT("blocked before the walkable notify"), FootBlocked(Sim, Depot));
	}
	Sim->AdvanceSimTicks(1);
	if (LowerSteps > OpenSteps)
	{
		Sim->AdvanceSimTicks(LowerSteps - OpenSteps);
	}
	Sim->TryGetStructure(DepotId, Depot);
	TestEqual(TEXT("lowered idle"), static_cast<int32>(Depot.DepotPose), static_cast<int32>(EKodDepotPose::Lowered));
	TestTrue(TEXT("walkable at the notify"), Depot.bDepotWalkable);
	TestTrue(TEXT("footprint is open"), FootOpen(Sim, Depot));
	TestTrue(TEXT("placement still sees the building"), Sim->IsCellBlocked(Depot.CellX, Depot.CellY, 0, false));

	TestTrue(TEXT("raise command"), Sim->ToggleDepot(DepotId));
	Sim->TryGetStructure(DepotId, Depot);
	TestFalse(TEXT("raise clears the command"), Depot.bLowered);
	TestEqual(TEXT("raising"), static_cast<int32>(Depot.DepotPose), static_cast<int32>(EKodDepotPose::Raising));
	TestFalse(TEXT("raise blocks on frame 0"), Depot.bDepotWalkable);
	TestTrue(TEXT("blocked on the raise command"), FootBlocked(Sim, Depot));
	TestTrue(TEXT("raise clip"), KodDepotActiveClip(Depot) == FName(KodDepotClipId::Raise));
	Sim->AdvanceSimTicks(RaiseSteps);
	Sim->TryGetStructure(DepotId, Depot);
	TestEqual(TEXT("back to raised idle"), static_cast<int32>(Depot.DepotPose), static_cast<int32>(EKodDepotPose::Raised));
	TestTrue(TEXT("stays blocked when raised"), FootBlocked(Sim, Depot));

	TestTrue(TEXT("lower again"), Sim->ToggleDepot(DepotId));
	Sim->AdvanceSimTicks(MidSteps);
	TestTrue(TEXT("reverse mid-lower"), Sim->ToggleDepot(DepotId));
	Sim->TryGetStructure(DepotId, Depot);
	TestEqual(TEXT("mid-lower becomes raise"), static_cast<int32>(Depot.DepotPose), static_cast<int32>(EKodDepotPose::Raising));
	TestEqual(TEXT("raise starts over"), Depot.DepotPoseTicks, 0);
	TestFalse(TEXT("mid-lower never opened"), Depot.bDepotWalkable);
	TestTrue(TEXT("mid-lower reverse stays blocked"), FootBlocked(Sim, Depot));
	Sim->AdvanceSimTicks(RaiseSteps);
	Sim->TryGetStructure(DepotId, Depot);
	TestEqual(TEXT("reversed raise finishes raised"), static_cast<int32>(Depot.DepotPose), static_cast<int32>(EKodDepotPose::Raised));

	TestTrue(TEXT("lower for the raise reverse"), Sim->ToggleDepot(DepotId));
	Sim->AdvanceSimTicks(LowerSteps);
	Sim->TryGetStructure(DepotId, Depot);
	TestTrue(TEXT("open before the mid-raise reverse"), Depot.bDepotWalkable);
	TestTrue(TEXT("start raise"), Sim->ToggleDepot(DepotId));
	Sim->AdvanceSimTicks(RaiseMid);
	TestTrue(TEXT("reverse mid-raise"), Sim->ToggleDepot(DepotId));
	Sim->TryGetStructure(DepotId, Depot);
	TestEqual(TEXT("mid-raise becomes lower"), static_cast<int32>(Depot.DepotPose), static_cast<int32>(EKodDepotPose::Lowering));
	TestEqual(TEXT("lower starts over"), Depot.DepotPoseTicks, 0);
	TestFalse(TEXT("fresh lower is not walkable yet"), Depot.bDepotWalkable);
	if (OpenSteps > 1)
	{
		Sim->AdvanceSimTicks(OpenSteps - 1);
		Sim->TryGetStructure(DepotId, Depot);
		TestTrue(TEXT("still blocked before the fresh notify"), FootBlocked(Sim, Depot));
	}
	Sim->AdvanceSimTicks(1);
	if (LowerSteps > OpenSteps)
	{
		Sim->AdvanceSimTicks(LowerSteps - OpenSteps);
	}
	Sim->TryGetStructure(DepotId, Depot);
	TestTrue(TEXT("walkable only after a fresh lower"), Depot.bDepotWalkable);
	TestEqual(TEXT("lowered after the fresh lower"), static_cast<int32>(Depot.DepotPose), static_cast<int32>(EKodDepotPose::Lowered));

	const FVector OnDepot(KodBuildGrid::CellCenter(Depot.CellX), KodBuildGrid::CellCenter(Depot.CellY), 0.f);
	const FKodEntityId Occupant = SpawnUnit(World, Sim, OnDepot, 0, 0.f);
	TestTrue(TEXT("raise pushes"), Sim->ToggleDepot(DepotId));
	FKodSimEntityState Pushed;
	TestTrue(TEXT("occupant still exists"), Sim->TryGetState(Occupant, Pushed));
	const int32 PushedX = KodBuildGrid::CellOf(Pushed.Position.X);
	const int32 PushedY = KodBuildGrid::CellOf(Pushed.Position.Y);
	const bool bStillOn = PushedX >= Depot.CellX && PushedX < Depot.CellX + Depot.FootprintX
		&& PushedY >= Depot.CellY && PushedY < Depot.CellY + Depot.FootprintY;
	TestFalse(TEXT("occupant was pushed off"), bStillOn);
	Sim->TryGetStructure(DepotId, Depot);
	TestTrue(TEXT("push raise blocks immediately"), FootBlocked(Sim, Depot));

	const FVector RaisedAt(KodBuildGrid::CellCenter(30), KodBuildGrid::CellCenter(10), 0.f);
	const FKodEntityId RaisedId = SpawnDepot(World, Sim, 30, 10);
	TestTrue(TEXT("kill raised"), KillDepot(Sim, World, RaisedId, RaisedAt));
	FKodStructureState Raised;
	TestTrue(TEXT("raised wreck waits"), Sim->TryGetStructure(RaisedId, Raised));
	TestEqual(TEXT("dying"), static_cast<int32>(Raised.DepotPose), static_cast<int32>(EKodDepotPose::Dying));
	TestTrue(TEXT("raised death clip"), KodDepotActiveClip(Raised) == FName(KodDepotClipId::Death));
	TestEqual(TEXT("death frame 0"), Raised.DepotPoseTicks, 0);
	TestFalse(TEXT("no toggle during death"), Sim->ToggleDepot(RaisedId));
	TestTrue(TEXT("structure remains on death frame 0"), Sim->IsStructure(RaisedId));
	if (DeathSteps > 1)
	{
		Sim->AdvanceSimTicks(DeathSteps - 1);
	}
	TestTrue(TEXT("present until the clip ends"), Sim->IsStructure(RaisedId));
	Sim->AdvanceSimTicks(1);
	TestFalse(TEXT("wreck handoff"), Sim->IsStructure(RaisedId));

	const FKodEntityId LoweredId = SpawnDepot(World, Sim, 40, 10);
	TestTrue(TEXT("lower before death"), Sim->ToggleDepot(LoweredId));
	Sim->AdvanceSimTicks(LowerSteps);
	const FVector LoweredAt(KodBuildGrid::CellCenter(40), KodBuildGrid::CellCenter(10), 0.f);
	TestTrue(TEXT("kill lowered"), KillDepot(Sim, World, LoweredId, LoweredAt));
	FKodStructureState LoweredDead;
	TestTrue(TEXT("lowered wreck waits"), Sim->TryGetStructure(LoweredId, LoweredDead));
	TestTrue(TEXT("lowered death clip"), KodDepotActiveClip(LoweredDead) == FName(KodDepotClipId::DeathLowered));

	const FKodEntityId LoweringId = SpawnDepot(World, Sim, 50, 10);
	TestTrue(TEXT("start lower"), Sim->ToggleDepot(LoweringId));
	Sim->AdvanceSimTicks(MidSteps);
	const FVector LoweringAt(KodBuildGrid::CellCenter(50), KodBuildGrid::CellCenter(10), 0.f);
	TestTrue(TEXT("kill while lowering"), KillDepot(Sim, World, LoweringId, LoweringAt));
	FKodStructureState LoweringDead;
	TestTrue(TEXT("lowering wreck waits"), Sim->TryGetStructure(LoweringId, LoweringDead));
	TestTrue(TEXT("lowering uses the lowered death"), KodDepotActiveClip(LoweringDead) == FName(KodDepotClipId::DeathLowered));

	const FKodEntityId RaisingId = SpawnDepot(World, Sim, 60, 10);
	TestTrue(TEXT("lower before raise-death"), Sim->ToggleDepot(RaisingId));
	Sim->AdvanceSimTicks(LowerSteps);
	TestTrue(TEXT("raise before death"), Sim->ToggleDepot(RaisingId));
	Sim->AdvanceSimTicks(RaiseMid);
	const FVector RaisingAt(KodBuildGrid::CellCenter(60), KodBuildGrid::CellCenter(10), 0.f);
	TestTrue(TEXT("kill while raising"), KillDepot(Sim, World, RaisingId, RaisingAt));
	FKodStructureState RaisingDead;
	TestTrue(TEXT("raising wreck waits"), Sim->TryGetStructure(RaisingId, RaisingDead));
	TestTrue(TEXT("raising uses the raised death"), KodDepotActiveClip(RaisingDead) == FName(KodDepotClipId::Death));

	const FKodEntityId WallA = SpawnDepot(World, Sim, 20, 40);
	const FKodEntityId WallB = SpawnDepot(World, Sim, 22, 40);
	AActor* BarracksActor = SpawnRooted(World, FVector(KodBuildGrid::CellCenter(25), KodBuildGrid::CellCenter(40), 0.f));
	FKodStructureSpec BarracksRow;
	BarracksRow.DefinitionId = FName(KodWardenPaths::Id_Barracks);
	BarracksRow.FootprintX = 3;
	BarracksRow.FootprintY = 3;
	BarracksRow.MaxHealth = 1000.f;
	const FKodEntityId BarracksId = Sim->RegisterStructure(BarracksActor, BarracksRow);
	FKodStructureState WallDepot;
	FKodStructureState Barracks;
	TestTrue(TEXT("wall depot"), Sim->TryGetStructure(WallA, WallDepot));
	TestTrue(TEXT("wall barracks"), Sim->TryGetStructure(BarracksId, Barracks));
	Sim->RegisterRampCell(WallDepot.CellX, WallDepot.CellY);
	Sim->RegisterRampCell(Barracks.CellX, Barracks.CellY);
	TestTrue(TEXT("raised depot walls the ramp"), Sim->IsRampWalled(WallDepot.CellX, WallDepot.CellY, 0));
	TestTrue(TEXT("barracks walls the ramp"), Sim->IsRampWalled(Barracks.CellX, Barracks.CellY, 0));
	TestTrue(TEXT("lower the wall depots"), Sim->ToggleDepot(WallA) && Sim->ToggleDepot(WallB));
	if (OpenSteps > 1)
	{
		Sim->AdvanceSimTicks(OpenSteps - 1);
		TestTrue(TEXT("wall holds until the notify"), Sim->IsRampWalled(WallDepot.CellX, WallDepot.CellY, 0));
		TestTrue(TEXT("depot cells blocked during the drop"), FootBlocked(Sim, WallDepot));
	}
	Sim->AdvanceSimTicks(1);
	if (LowerSteps > OpenSteps)
	{
		Sim->AdvanceSimTicks(LowerSteps - OpenSteps);
	}
	Sim->TryGetStructure(WallA, WallDepot);
	FKodStructureState WallBState;
	Sim->TryGetStructure(WallB, WallBState);
	TestTrue(TEXT("lowered depot ramp is walkable"), !Sim->IsRampWalled(WallDepot.CellX, WallDepot.CellY, 0));
	TestTrue(TEXT("both lowered depots are open"), FootOpen(Sim, WallDepot) && FootOpen(Sim, WallBState));
	TestTrue(TEXT("adjacent barracks stays a full wall"), FootBlocked(Sim, Barracks));
	TestTrue(TEXT("barracks ramp stays walled"), Sim->IsRampWalled(Barracks.CellX, Barracks.CellY, 0));

	const FKodEntityId SaveId = SpawnDepot(World, Sim, 70, 10);
	TestTrue(TEXT("save lower"), Sim->ToggleDepot(SaveId));
	Sim->AdvanceSimTicks(MidSteps);
	const TArray<uint8> Mid = Sim->ExportSimBlob();
	Sim->ApplySimBlob(Mid);
	FKodStructureState Saved;
	TestTrue(TEXT("v15 reloaded"), Sim->TryGetStructure(SaveId, Saved));
	TestEqual(TEXT("v15 keeps lowering"), static_cast<int32>(Saved.DepotPose), static_cast<int32>(EKodDepotPose::Lowering));
	TestEqual(TEXT("v15 keeps the clock"), Saved.DepotPoseTicks, MidSteps);
	TestFalse(TEXT("v15 still blocked"), Saved.bDepotWalkable);

	World->DestroyWorld(false);
	return true;
}
