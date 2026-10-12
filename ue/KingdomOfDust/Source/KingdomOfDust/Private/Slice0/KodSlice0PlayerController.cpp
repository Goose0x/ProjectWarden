#include "Slice0/KodSlice0PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Game/KodHUD.h"
#include "Game/KodRTSCameraPawn.h"
#include "Game/KodSpectatorPawn.h"
#include "Data/KodBuildingDefinition.h"
#include "Engine/HitResult.h"
#include "GameFramework/Actor.h"
#include "Slice0/KodBuildGhost.h"
#include "Slice0/KodSlice0Bootstrap.h"
#include "InputCoreTypes.h"
#include "KodCommandSubsystem.h"
#include "KodCommandTypes.h"
#include "KodControlGroupStore.h"
#include "Sim/KodSimSubsystem.h"
#include "Warden/KodWardenPaths.h"

AKodSlice0PlayerController::AKodSlice0PlayerController()
{
	LastGroupTapSeconds.Init(-1.f, 10);
}

void AKodSlice0PlayerController::IssueMoveToSelection_Implementation(FVector WorldLocation)
{
	UWorld* World = GetWorld();
	UKodCommandSubsystem* Commands = World ? World->GetSubsystem<UKodCommandSubsystem>() : nullptr;
	if (!Commands)
	{
		Super::IssueMoveToSelection_Implementation(WorldLocation);
		return;
	}

	FKodCommand Cmd;
	Cmd.Type = EKodCommandType::Move;
	Cmd.IssuerPlayerId = GetLocalPlayer() ? GetLocalPlayer()->GetControllerId() : 0;
	Cmd.bQueued = ShiftQueued();
	Cmd.SourceEntities = GetLocalSelectedEntityIds();
	Cmd.TargetLocation = WorldLocation;
	UE_LOG(LogTemp, Log, TEXT("Move Issued Sources=%d Dest=%.0f,%.0f,%.0f"),
		Cmd.SourceEntities.Num(),
		WorldLocation.X,
		WorldLocation.Y,
		WorldLocation.Z);
	if (Cmd.SourceEntities.Num() > 0)
	{
		Commands->Enqueue(Cmd);
	}
}

void AKodSlice0PlayerController::IssueAttackToSelection_Implementation(AActor* Target)
{
	UWorld* World = GetWorld();
	UKodCommandSubsystem* Commands = World ? World->GetSubsystem<UKodCommandSubsystem>() : nullptr;
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Commands || !Sim || !Target)
	{
		Super::IssueAttackToSelection_Implementation(Target);
		return;
	}

	const FKodEntityId TargetId = Sim->FindIdForActor(Target);
	if (!TargetId.IsValid())
	{
		// Do not Move to the actor pivot. RMB already Moves to ImpactPoint for non-sim hits.
		UE_LOG(LogTemp, Log, TEXT("Attack Reject NonSim Target=%s"), *Target->GetName());
		return;
	}
	if (!Sim->IsAttackable(TargetId, GetLocalTeamId()))
	{
		UE_LOG(LogTemp, Log, TEXT("Attack Reject Node Target=%s Id=%d"), *Target->GetName(), TargetId.Value);
		return;
	}

	FKodCommand Cmd;
	Cmd.Type = EKodCommandType::Attack;
	Cmd.bQueued = ShiftQueued();
	Cmd.IssuerPlayerId = GetLocalPlayer() ? GetLocalPlayer()->GetControllerId() : 0;
	Cmd.SourceEntities = GetLocalSelectedEntityIds();
	Cmd.SourceEntities.Remove(TargetId);
	Cmd.TargetEntity = TargetId;
	Cmd.TargetLocation = Target->GetActorLocation();
	if (Cmd.SourceEntities.Num() == 0)
	{
		UE_LOG(LogTemp, Log, TEXT("Attack Reject Self Target=%s Id=%d"), *Target->GetName(), TargetId.Value);
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("Attack Issued Sources=%d Target=%s Id=%d"),
		Cmd.SourceEntities.Num(),
		*Target->GetName(),
		TargetId.Value);
	Commands->Enqueue(Cmd);
}

void AKodSlice0PlayerController::IssueGatherToSelection_Implementation(AActor* Node, FVector NonGathererMovePoint)
{
	UWorld* World = GetWorld();
	UKodCommandSubsystem* Commands = World ? World->GetSubsystem<UKodCommandSubsystem>() : nullptr;
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Commands || !Sim || !Node)
	{
		Super::IssueGatherToSelection_Implementation(Node, NonGathererMovePoint);
		return;
	}

	const FKodEntityId NodeId = Sim->FindIdForActor(Node);
	if (!Sim->IsResourceNode(NodeId))
	{
		UE_LOG(LogTemp, Log, TEXT("Gather Reject NonNode Target=%s"), *Node->GetName());
		return;
	}

	FKodCommand GatherCmd;
	GatherCmd.Type = EKodCommandType::Gather;
	GatherCmd.bQueued = ShiftQueued();
	GatherCmd.IssuerPlayerId = GetLocalPlayer() ? GetLocalPlayer()->GetControllerId() : 0;
	GatherCmd.TargetEntity = NodeId;
	GatherCmd.TargetLocation = Node->GetActorLocation();

	FKodCommand MoveCmd;
	MoveCmd.Type = EKodCommandType::Move;
	MoveCmd.bQueued = GatherCmd.bQueued;
	MoveCmd.IssuerPlayerId = GatherCmd.IssuerPlayerId;
	MoveCmd.TargetLocation = NonGathererMovePoint;

	for (const FKodEntityId& Id : GetLocalSelectedEntityIds())
	{
		FKodSimEntityState State;
		if (!Sim->TryGetState(Id, State))
		{
			continue;
		}
		if (State.bCanGather)
		{
			GatherCmd.SourceEntities.Add(Id);
		}
		else
		{
			MoveCmd.SourceEntities.Add(Id);
		}
	}

	if (GatherCmd.SourceEntities.Num() == 0)
	{
		Super::IssueGatherToSelection_Implementation(Node, NonGathererMovePoint);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("Gather Issued Sources=%d Node=%d"), GatherCmd.SourceEntities.Num(), NodeId.Value);
	Commands->Enqueue(GatherCmd);
	if (MoveCmd.SourceEntities.Num() > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("Move Issued Sources=%d Dest=%.0f,%.0f,%.0f"),
			MoveCmd.SourceEntities.Num(),
			NonGathererMovePoint.X,
			NonGathererMovePoint.Y,
			NonGathererMovePoint.Z);
		Commands->Enqueue(MoveCmd);
	}
}

void AKodSlice0PlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	if (!IsLocalPlayerController() || Cast<AKodSpectatorPawn>(GetPawn()))
	{
		return;
	}
	if (const UWorld* MatchWorld = GetWorld())
	{
		if (const UKodSimSubsystem* MatchSim = MatchWorld->GetSubsystem<UKodSimSubsystem>())
		{
			if (MatchSim->IsMatchOver())
			{
				return;
			}
		}
	}
	if (WasInputKeyJustPressed(EKeys::B) && HasSelectedWorker())
	{
		bBuildMenu = !bBuildMenu;
		if (!bBuildMenu)
		{
			CancelPlacement();
		}
	}
	if (bBuildMenu && HasSelectedWorker())
	{
		FName HotkeyPlace = NAME_None;
		if (WasInputKeyJustPressed(EKeys::D))
		{
			HotkeyPlace = FName(KodWardenPaths::Id_SupplyDepot);
		}
		else if (WasInputKeyJustPressed(EKeys::R))
		{
			HotkeyPlace = FName(KodWardenPaths::Id_Barracks);
		}
		else if (WasInputKeyJustPressed(EKeys::E))
		{
			HotkeyPlace = FName(KodWardenPaths::Id_Rig);
		}
		if (!HotkeyPlace.IsNone())
		{
			TryBeginPlace(HotkeyPlace);
		}
	}
	else if (!bBuildMenu)
	{
		PlaceType = NAME_None;
	}
	UpdateBuildGhost();
	if (WasInputKeyJustPressed(EKeys::M))
	{
		ArmOrder(FName(TEXT("Move")));
	}
	if (WasInputKeyJustPressed(EKeys::A))
	{
		ArmOrder(FName(TEXT("Attack")));
	}
	if (WasInputKeyJustPressed(EKeys::P))
	{
		ArmOrder(FName(TEXT("Patrol")));
	}
	if (WasInputKeyJustPressed(EKeys::G) && !bBuildMenu)
	{
		ArmOrder(FName(TEXT("Guard")));
	}
	if (WasInputKeyJustPressed(EKeys::H))
	{
		IssueHoldOnSelectedUnits();
	}
	if (WasInputKeyJustPressed(EKeys::S))
	{
		EnqueueTrainOnSelectedStructures();
		IssueStopOnSelectedUnits();
	}
	if (WasInputKeyJustPressed(EKeys::T))
	{
		IssueAbility(FName(KodWardenPaths::Id_CombatStim));
	}
	if (WasInputKeyJustPressed(EKeys::BackSpace))
	{
		if (!PlaceType.IsNone())
		{
			CancelPlacement();
			bBuildMenu = false;
		}
		else if (!CancelSelectedConstruction())
		{
			CancelTrainOnSelectedStructures();
		}
	}
	TryCenterControlGroup();
}

void AKodSlice0PlayerController::TryCenterControlGroup()
{
	UWorld* World = GetWorld();
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UKodControlGroupStore* Store = LocalPlayer ? LocalPlayer->GetSubsystem<UKodControlGroupStore>() : nullptr;
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	AKodRTSCameraPawn* CameraPawn = Cast<AKodRTSCameraPawn>(GetPawn());
	if (!World || !Store || !Sim || !CameraPawn)
	{
		return;
	}

	const bool bCtrl = IsInputKeyDown(EKeys::LeftControl) || IsInputKeyDown(EKeys::RightControl);
	const bool bShift = IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift);
	const FKey Keys[10] = {
		EKeys::Zero, EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four,
		EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine
	};
	const float Now = World->GetTimeSeconds();
	constexpr float DoubleTapSeconds = 0.3f;
	for (int32 Index = 0; Index < 10; ++Index)
	{
		if (!WasInputKeyJustPressed(Keys[Index]))
		{
			continue;
		}
		if (bCtrl)
		{
			AssignControlGroup(Index);
			continue;
		}
		if (bShift)
		{
			AddToControlGroup(Index);
			continue;
		}

		const TArray<FKodEntityId> Ids = Store->GetGroup(Index);
		SetLocalSelectionFromIds(Ids, false);
		const float Previous = LastGroupTapSeconds[Index];
		LastGroupTapSeconds[Index] = Now;
		if (Previous < 0.f || (Now - Previous) > DoubleTapSeconds || Ids.Num() == 0)
		{
			continue;
		}
		LastGroupTapSeconds[Index] = -1.f;
		FVector Sum = FVector::ZeroVector;
		int32 Count = 0;
		for (const FKodEntityId& Id : Ids)
		{
			if (AActor* Actor = Sim->ResolveEntity(Id))
			{
				Sum += Actor->GetActorLocation();
				++Count;
			}
		}
		if (Count > 0)
		{
			CameraPawn->JumpTo(Sum / static_cast<float>(Count));
		}
	}
}

int32 AKodSlice0PlayerController::GetControlGroupCount(int32 GroupIndex) const
{
	return GetControlGroupIds(GroupIndex).Num();
}

TArray<FKodEntityId> AKodSlice0PlayerController::GetControlGroupIds(int32 GroupIndex) const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	const UKodControlGroupStore* Store = LocalPlayer ? LocalPlayer->GetSubsystem<UKodControlGroupStore>() : nullptr;
	if (!Store)
	{
		return TArray<FKodEntityId>();
	}
	return Store->GetGroup(GroupIndex);
}

void AKodSlice0PlayerController::AssignControlGroup(int32 GroupIndex)
{
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UKodControlGroupStore* Store = LocalPlayer ? LocalPlayer->GetSubsystem<UKodControlGroupStore>() : nullptr;
	if (!Store)
	{
		return;
	}
	Store->AssignGroup(GroupIndex, GetLocalSelectedEntityIds());
}

void AKodSlice0PlayerController::AddToControlGroup(int32 GroupIndex)
{
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UKodControlGroupStore* Store = LocalPlayer ? LocalPlayer->GetSubsystem<UKodControlGroupStore>() : nullptr;
	if (!Store)
	{
		return;
	}
	Store->AddToGroup(GroupIndex, GetLocalSelectedEntityIds());
}

void AKodSlice0PlayerController::IssueMinimapGroundOrder(FVector WorldLocation)
{
	UWorld* World = GetWorld();
	UKodCommandSubsystem* Commands = World ? World->GetSubsystem<UKodCommandSubsystem>() : nullptr;
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Commands || !Sim)
	{
		Super::IssueMinimapGroundOrder(WorldLocation);
		return;
	}

	FKodCommand MoveCmd;
	MoveCmd.Type = EKodCommandType::Move;
	MoveCmd.IssuerPlayerId = GetLocalPlayer() ? GetLocalPlayer()->GetControllerId() : 0;
	MoveCmd.TargetLocation = WorldLocation;
	FKodCommand AttackCmd;
	AttackCmd.Type = EKodCommandType::AttackMove;
	AttackCmd.IssuerPlayerId = MoveCmd.IssuerPlayerId;
	AttackCmd.TargetLocation = WorldLocation;
	for (const FKodEntityId& Id : GetLocalSelectedEntityIds())
	{
		FKodSimEntityState State;
		if (!Sim->TryGetState(Id, State) || !State.bMobile || State.Health <= 0.f)
		{
			continue;
		}
		if (State.WeaponDamage > 0.f)
		{
			AttackCmd.SourceEntities.Add(Id);
		}
		else
		{
			MoveCmd.SourceEntities.Add(Id);
		}
	}
	if (MoveCmd.SourceEntities.Num() > 0)
	{
		Commands->Enqueue(MoveCmd);
	}
	if (AttackCmd.SourceEntities.Num() > 0)
	{
		Commands->Enqueue(AttackCmd);
	}
	UE_LOG(LogTemp, Log, TEXT("Minimap Order Move=%d AttackMove=%d Dest=%.0f,%.0f"),
		MoveCmd.SourceEntities.Num(),
		AttackCmd.SourceEntities.Num(),
		WorldLocation.X,
		WorldLocation.Y);
}

bool AKodSlice0PlayerController::IsActiveCommandSubgroup(FName DefinitionId) const
{
	const AKodHUD* HUD = Cast<AKodHUD>(GetHUD());
	if (!HUD || HUD->GetCommandSubgroupCount() <= 1)
	{
		return true;
	}
	return DefinitionId == HUD->GetActiveCommandSubgroup();
}

bool AKodSlice0PlayerController::ApplySelectedStructureRally(const FKodCursorCommand& Cmd)
{
	UWorld* World = GetWorld();
	UKodCommandSubsystem* Commands = World ? World->GetSubsystem<UKodCommandSubsystem>() : nullptr;
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Commands || !Sim)
	{
		return Super::ApplySelectedStructureRally(Cmd);
	}

	TArray<FKodEntityId> Structures;
	bool bHasUnits = false;
	for (const FKodEntityId& Id : GetLocalSelectedEntityIds())
	{
		if (Sim->IsStructure(Id))
		{
			Structures.Add(Id);
		}
		else
		{
			FKodSimEntityState State;
			if (Sim->TryGetState(Id, State))
			{
				bHasUnits = true;
			}
		}
	}
	if (Structures.Num() == 0)
	{
		return false;
	}

	FKodCommand Rally;
	Rally.Type = EKodCommandType::SetRally;
	Rally.IssuerPlayerId = GetLocalPlayer() ? GetLocalPlayer()->GetControllerId() : 0;
	Rally.SourceEntities = Structures;
	Rally.TargetLocation = Cmd.bHasMovePoint ? Cmd.MovePoint : FVector::ZeroVector;
	if (Cmd.GatherActor)
	{
		Rally.TargetEntity = Cmd.GatherId;
		Rally.TargetLocation = Cmd.GatherActor->GetActorLocation();
	}
	Commands->Enqueue(Rally);
	return !bHasUnits;
}

bool AKodSlice0PlayerController::EnqueueTrainOnSelectedStructures()
{
	UWorld* World = GetWorld();
	UKodCommandSubsystem* Commands = World ? World->GetSubsystem<UKodCommandSubsystem>() : nullptr;
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Commands || !Sim)
	{
		return false;
	}
	bool bAny = false;
	FKodCommand Cmd;
	Cmd.Type = EKodCommandType::Train;
	Cmd.IssuerPlayerId = GetLocalPlayer() ? GetLocalPlayer()->GetControllerId() : 0;
	for (const FKodEntityId& Id : GetLocalSelectedEntityIds())
	{
		FName TrainId;
		FKodStructureState Structure;
		if (Sim->IsStructure(Id) && Sim->TryGetStructure(Id, Structure) && !IsActiveCommandSubgroup(Structure.DefinitionId))
		{
			continue;
		}
		if (Sim->IsStructure(Id) && Sim->TryGetPrimaryTrain(Id, TrainId))
		{
			if (!AllowTrain(TrainId))
			{
				continue;
			}
			FKodCommand One = Cmd;
			One.PayloadName = TrainId;
			One.SourceEntities.Add(Id);
			Commands->Enqueue(One);
			bAny = true;
		}
	}
	return bAny;
}

void AKodSlice0PlayerController::CancelTrainOnSelectedStructures()
{
	UWorld* World = GetWorld();
	UKodCommandSubsystem* Commands = World ? World->GetSubsystem<UKodCommandSubsystem>() : nullptr;
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Commands || !Sim)
	{
		return;
	}
	FKodCommand Cmd;
	Cmd.Type = EKodCommandType::CancelTrain;
	Cmd.IssuerPlayerId = GetLocalPlayer() ? GetLocalPlayer()->GetControllerId() : 0;
	for (const FKodEntityId& Id : GetLocalSelectedEntityIds())
	{
		FKodStructureState Structure;
		if (Sim->IsStructure(Id) && Sim->TryGetStructure(Id, Structure) && IsActiveCommandSubgroup(Structure.DefinitionId))
		{
			Cmd.SourceEntities.Add(Id);
		}
	}
	if (Cmd.SourceEntities.Num() > 0)
	{
		Commands->Enqueue(Cmd);
	}
}

bool AKodSlice0PlayerController::HasSelectedWorker() const
{
	return FirstSelectedWorker().IsValid();
}

FKodEntityId AKodSlice0PlayerController::FirstSelectedWorker() const
{
	FKodEntityId Found;
	UWorld* World = GetWorld();
	const UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Sim)
	{
		return Found;
	}
	for (const FKodEntityId& Id : GetLocalSelectedEntityIds())
	{
		FKodSimEntityState State;
		if (Sim->TryGetState(Id, State) && State.bCanGather && State.Health > 0.f && IsActiveCommandSubgroup(State.DefinitionId))
		{
			return Id;
		}
	}
	return Found;
}

void AKodSlice0PlayerController::CancelPlacement()
{
	PlaceType = NAME_None;
	if (AKodBuildGhost* Ghost = BuildGhost.Get())
	{
		Ghost->Destroy();
	}
	BuildGhost = nullptr;
}

void AKodSlice0PlayerController::UpdateBuildGhost()
{
	if (PlaceType.IsNone() || !HasSelectedWorker())
	{
		if (AKodBuildGhost* Ghost = BuildGhost.Get())
		{
			Ghost->SetActorHiddenInGame(true);
		}
		return;
	}
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!World || !Sim)
	{
		return;
	}
	FHitResult Hit;
	if (!GetHitResultUnderCursor(ECC_Visibility, true, Hit))
	{
		return;
	}
	const FKodPlacementResult Placement = Sim->QueryPlacement(0, PlaceType, Hit.ImpactPoint);
	if (!BuildGhost.IsValid())
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		BuildGhost = World->SpawnActor<AKodBuildGhost>(AKodBuildGhost::StaticClass(), FTransform(Placement.WorldCenter), Params);
	}
	AKodBuildGhost* Ghost = BuildGhost.Get();
	if (!Ghost)
	{
		return;
	}
	Ghost->SetActorHiddenInGame(false);
	Ghost->SetActorLocation(Placement.WorldCenter);
	if (const UKodBuildingDefinition* Def = UKodSlice0Bootstrap::ResolveBuilding(PlaceType, this))
	{
		Ghost->SetFootprint(Def->FootprintX, Def->FootprintY);
	}
	Ghost->SetValidTint(Placement.bValid);
}

bool AKodSlice0PlayerController::ConsumePlacementLeftClick()
{
	if (PlaceType.IsNone() || !HasSelectedWorker())
	{
		return false;
	}
	UWorld* World = GetWorld();
	UKodCommandSubsystem* Commands = World ? World->GetSubsystem<UKodCommandSubsystem>() : nullptr;
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Commands || !Sim)
	{
		return true;
	}
	FHitResult Hit;
	if (!GetHitResultUnderCursor(ECC_Visibility, true, Hit))
	{
		return true;
	}
	const FKodPlacementResult Placement = Sim->QueryPlacement(0, PlaceType, Hit.ImpactPoint);
	FKodCommand Cmd;
	Cmd.Type = EKodCommandType::PlaceBuild;
	Cmd.IssuerPlayerId = GetLocalPlayer() ? GetLocalPlayer()->GetControllerId() : 0;
	Cmd.PayloadName = PlaceType;
	Cmd.TargetLocation = Hit.ImpactPoint;
	Cmd.SourceEntities.Add(FirstSelectedWorker());
	Commands->Enqueue(Cmd);
	const bool bShift = IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift);
	if (Placement.bValid && !bShift)
	{
		CancelPlacement();
		bBuildMenu = false;
	}
	return true;
}

bool AKodSlice0PlayerController::ConsumePlacementRightClick()
{
	if (PlaceType.IsNone())
	{
		return false;
	}
	CancelPlacement();
	bBuildMenu = false;
	return true;
}

FString AKodSlice0PlayerController::GetBuildMenuHint() const
{
	if (!bBuildMenu && PlaceType.IsNone())
	{
		return FString();
	}
	if (PlaceType.IsNone())
	{
		return TEXT("Build   D Depot   R Barracks   E Rig   RMB Cancel");
	}
	return FString::Printf(TEXT("Place %s   LMB Build   RMB Cancel   Shift Queues"), *PlaceType.ToString());
}

void AKodSlice0PlayerController::HandleRightClickCommand()
{
	float MouseX = 0.f;
	float MouseY = 0.f;
	if (GetMousePosition(MouseX, MouseY))
	{
		if (const AKodHUD* HUD = Cast<AKodHUD>(GetHUD()))
		{
			FVector WorldPoint;
			if (HUD->MinimapPixelToWorld(MouseX, MouseY, WorldPoint, false) || HUD->IsPassiveHudChrome(MouseX, MouseY))
			{
				Super::HandleRightClickCommand();
				return;
			}
		}
	}
	if (TryResumeBuildUnderCursor())
	{
		return;
	}
	Super::HandleRightClickCommand();
}

bool AKodSlice0PlayerController::TryResumeBuildUnderCursor()
{
	const FKodEntityId Worker = FirstSelectedWorker();
	if (!Worker.IsValid())
	{
		return false;
	}
	UWorld* World = GetWorld();
	UKodCommandSubsystem* Commands = World ? World->GetSubsystem<UKodCommandSubsystem>() : nullptr;
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!World || !Commands || !Sim)
	{
		return false;
	}
	FHitResult Hit;
	if (!GetHitResultUnderCursor(ECC_Pawn, true, Hit) || !Hit.GetActor())
	{
		return false;
	}
	const FKodEntityId HitId = Sim->FindIdForActor(Hit.GetActor());
	FKodStructureState Structure;
	if (!Sim->TryGetStructure(HitId, Structure) || Structure.bComplete)
	{
		return false;
	}
	FKodCommand Cmd;
	Cmd.Type = EKodCommandType::ResumeBuild;
	Cmd.IssuerPlayerId = GetLocalPlayer() ? GetLocalPlayer()->GetControllerId() : 0;
	Cmd.TargetEntity = HitId;
	Cmd.SourceEntities.Add(Worker);
	Commands->Enqueue(Cmd);
	return true;
}

bool AKodSlice0PlayerController::CancelSelectedConstruction()
{
	UWorld* World = GetWorld();
	UKodCommandSubsystem* Commands = World ? World->GetSubsystem<UKodCommandSubsystem>() : nullptr;
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Commands || !Sim)
	{
		return false;
	}
	FKodCommand Cmd;
	Cmd.Type = EKodCommandType::CancelBuild;
	Cmd.IssuerPlayerId = GetLocalPlayer() ? GetLocalPlayer()->GetControllerId() : 0;
	for (const FKodEntityId& Id : GetLocalSelectedEntityIds())
	{
		FKodStructureState Structure;
		if (Sim->TryGetStructure(Id, Structure) && !Structure.bComplete)
		{
			Cmd.SourceEntities.Add(Id);
		}
	}
	if (Cmd.SourceEntities.Num() <= 0)
	{
		return false;
	}
	Commands->Enqueue(Cmd);
	return true;
}

bool AKodSlice0PlayerController::IsWorkerBuildMenuOpen() const
{
	return bBuildMenu || !PlaceType.IsNone();
}

bool AKodSlice0PlayerController::AllowTrain(FName UnitId)
{
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	AKodHUD* HUD = Cast<AKodHUD>(GetHUD());
	if (!Sim || !HUD)
	{
		return true;
	}
	FKodTrainSpec Spec;
	if (!Sim->TryGetTrainSpec(UnitId, Spec))
	{
		return true;
	}
	const FName Fail = HUD->PeekResourceFail(Spec.Cost.Credits, Spec.Cost.Luminene, Spec.Cost.Supply);
	if (!Fail.IsNone())
	{
		HUD->ShowResourceFlash(Fail);
		return false;
	}
	return true;
}

void AKodSlice0PlayerController::TryBeginPlace(FName TypeId)
{
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	AKodHUD* HUD = Cast<AKodHUD>(GetHUD());
	if (Sim && HUD)
	{
		FKodBuildSpec Spec;
		if (Sim->TryGetBuildSpec(TypeId, Spec))
		{
			const FName Fail = HUD->PeekResourceFail(Spec.Cost.Credits, Spec.Cost.Luminene, 0);
			if (!Fail.IsNone())
			{
				HUD->ShowResourceFlash(Fail);
				return;
			}
		}
	}
	PlaceType = TypeId;
	bBuildMenu = true;
}

void AKodSlice0PlayerController::IssueTrain(FName UnitId, int32 StructureId)
{
	if (!AllowTrain(UnitId))
	{
		return;
	}
	UWorld* World = GetWorld();
	UKodCommandSubsystem* Commands = World ? World->GetSubsystem<UKodCommandSubsystem>() : nullptr;
	if (!Commands || StructureId <= 0)
	{
		return;
	}
	FKodCommand Cmd;
	Cmd.Type = EKodCommandType::Train;
	Cmd.IssuerPlayerId = GetLocalPlayer() ? GetLocalPlayer()->GetControllerId() : 0;
	Cmd.PayloadName = UnitId;
	FKodEntityId Id;
	Id.Value = StructureId;
	Cmd.SourceEntities.Add(Id);
	Commands->Enqueue(Cmd);
}

void AKodSlice0PlayerController::IssueCancelSlot(int32 StructureId, int32 SlotIndex)
{
	UWorld* World = GetWorld();
	UKodCommandSubsystem* Commands = World ? World->GetSubsystem<UKodCommandSubsystem>() : nullptr;
	if (!Commands || StructureId <= 0 || SlotIndex < 0)
	{
		return;
	}
	FKodCommand Cmd;
	Cmd.Type = EKodCommandType::CancelTrain;
	Cmd.IssuerPlayerId = GetLocalPlayer() ? GetLocalPlayer()->GetControllerId() : 0;
	Cmd.PayloadIndex = SlotIndex;
	FKodEntityId Id;
	Id.Value = StructureId;
	Cmd.SourceEntities.Add(Id);
	Commands->Enqueue(Cmd);
}

bool AKodSlice0PlayerController::ConsumeHudCommandClick()
{
	float MouseX = 0.f;
	float MouseY = 0.f;
	if (!GetMousePosition(MouseX, MouseY))
	{
		return false;
	}
	AKodHUD* HUD = Cast<AKodHUD>(GetHUD());
	if (!HUD)
	{
		return false;
	}
	FKodHudCommandHit Hit;
	if (!HUD->GetCommandHit(MouseX, MouseY, Hit) || !Hit.bInsideChrome)
	{
		return false;
	}
	if (Hit.Action.IsNone())
	{
		return true;
	}
	if (!Hit.bEnabled)
	{
		if (Hit.FailReason == TEXT("Credits") || Hit.FailReason == TEXT("Luminene") || Hit.FailReason == TEXT("Supply"))
		{
			HUD->ShowResourceFlash(Hit.FailReason);
		}
		return true;
	}
	if (Hit.Action == TEXT("Train"))
	{
		IssueTrain(Hit.Payload, Hit.StructureId);
	}
	else if (Hit.Action == TEXT("BuildMenu"))
	{
		bBuildMenu = true;
	}
	else if (Hit.Action == TEXT("Place"))
	{
		TryBeginPlace(Hit.Payload);
	}
	else if (Hit.Action == TEXT("CancelSlot"))
	{
		IssueCancelSlot(Hit.StructureId, Hit.SlotIndex);
	}
	else if (Hit.Action == TEXT("Ability"))
	{
		IssueAbility(Hit.Payload);
	}
	else if (Hit.Action == TEXT("OrderStop") || Hit.Action == TEXT("Stop"))
	{
		IssueStopOnSelectedUnits();
	}
	else if (Hit.Action == TEXT("OrderHold") || Hit.Action == TEXT("Hold"))
	{
		IssueHoldOnSelectedUnits();
	}
	else if (Hit.Action == TEXT("OrderMove") || Hit.Action == TEXT("Move")
		|| Hit.Action == TEXT("OrderAttack") || Hit.Action == TEXT("Attack")
		|| Hit.Action == TEXT("OrderPatrol") || Hit.Action == TEXT("Patrol")
		|| Hit.Action == TEXT("OrderGuard") || Hit.Action == TEXT("Guard"))
	{
		ArmOrder(Hit.Action);
	}
	return true;
}

bool AKodSlice0PlayerController::ShiftQueued() const
{
	return IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift);
}

void AKodSlice0PlayerController::ArmOrder(FName OrderName)
{
	ArmedOrder = ArmedOrder == OrderName ? FName() : OrderName;
	UE_LOG(LogTemp, Log, TEXT("KodOrder Arm %s"), ArmedOrder.IsNone() ? TEXT("Clear") : *ArmedOrder.ToString());
}

FName AKodSlice0PlayerController::GetArmedOrderName() const
{
	return ArmedOrder;
}

void AKodSlice0PlayerController::IssueStopOnSelectedUnits()
{
	UWorld* World = GetWorld();
	UKodCommandSubsystem* Commands = World ? World->GetSubsystem<UKodCommandSubsystem>() : nullptr;
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Commands || !Sim)
	{
		return;
	}
	FKodCommand Cmd;
	Cmd.Type = EKodCommandType::Stop;
	Cmd.IssuerPlayerId = GetLocalPlayer() ? GetLocalPlayer()->GetControllerId() : 0;
	for (const FKodEntityId& Id : GetLocalSelectedEntityIds())
	{
		FKodSimEntityState State;
		if (Sim->TryGetState(Id, State) && State.bMobile)
		{
			Cmd.SourceEntities.Add(Id);
		}
	}
	if (Cmd.SourceEntities.Num() > 0)
	{
		Commands->Enqueue(Cmd);
	}
}

void AKodSlice0PlayerController::IssueHoldOnSelectedUnits()
{
	UWorld* World = GetWorld();
	UKodCommandSubsystem* Commands = World ? World->GetSubsystem<UKodCommandSubsystem>() : nullptr;
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Commands || !Sim)
	{
		return;
	}
	FKodCommand Cmd;
	Cmd.Type = EKodCommandType::Hold;
	Cmd.IssuerPlayerId = GetLocalPlayer() ? GetLocalPlayer()->GetControllerId() : 0;
	for (const FKodEntityId& Id : GetLocalSelectedEntityIds())
	{
		FKodSimEntityState State;
		if (Sim->TryGetState(Id, State) && State.bMobile)
		{
			Cmd.SourceEntities.Add(Id);
		}
	}
	if (Cmd.SourceEntities.Num() > 0)
	{
		Commands->Enqueue(Cmd);
	}
}

void AKodSlice0PlayerController::IssueFollowToSelection(AActor* Target, bool bGuard)
{
	UWorld* World = GetWorld();
	UKodCommandSubsystem* Commands = World ? World->GetSubsystem<UKodCommandSubsystem>() : nullptr;
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Commands || !Sim || !Target)
	{
		Super::IssueFollowToSelection(Target, bGuard);
		return;
	}
	const FKodEntityId TargetId = Sim->FindIdForActor(Target);
	FKodCommand Cmd;
	Cmd.Type = bGuard ? EKodCommandType::Guard : EKodCommandType::Follow;
	Cmd.bQueued = ShiftQueued();
	Cmd.IssuerPlayerId = GetLocalPlayer() ? GetLocalPlayer()->GetControllerId() : 0;
	Cmd.TargetEntity = TargetId;
	Cmd.TargetLocation = Target->GetActorLocation();
	for (const FKodEntityId& Id : GetLocalSelectedEntityIds())
	{
		if (Id != TargetId)
		{
			Cmd.SourceEntities.Add(Id);
		}
	}
	if (Cmd.SourceEntities.Num() > 0)
	{
		Commands->Enqueue(Cmd);
	}
}

bool AKodSlice0PlayerController::ConsumeArmedOrderClick()
{
	if (ArmedOrder.IsNone())
	{
		return false;
	}
	UWorld* World = GetWorld();
	UKodCommandSubsystem* Commands = World ? World->GetSubsystem<UKodCommandSubsystem>() : nullptr;
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Commands || !Sim)
	{
		return false;
	}
	const FKodCursorCommand Cursor = TraceCursorCommand();
	FKodCommand Cmd;
	Cmd.bQueued = ShiftQueued();
	Cmd.IssuerPlayerId = GetLocalPlayer() ? GetLocalPlayer()->GetControllerId() : 0;
	Cmd.SourceEntities = GetLocalSelectedEntityIds();
	if (ArmedOrder == FName(TEXT("OrderMove")) || ArmedOrder == FName(TEXT("Move")))
	{
		if (!Cursor.bHasMovePoint)
		{
			return true;
		}
		Cmd.Type = EKodCommandType::Move;
		Cmd.TargetLocation = Cursor.MovePoint;
	}
	else if (ArmedOrder == FName(TEXT("OrderPatrol")) || ArmedOrder == FName(TEXT("Patrol")))
	{
		if (!Cursor.bHasMovePoint)
		{
			return true;
		}
		Cmd.Type = EKodCommandType::Patrol;
		Cmd.TargetLocation = Cursor.MovePoint;
	}
	else if (ArmedOrder == FName(TEXT("OrderAttack")) || ArmedOrder == FName(TEXT("Attack")))
	{
		if (Cursor.ClickedActor && !Sim->IsResourceNode(Cursor.ClickedId))
		{
			Cmd.Type = EKodCommandType::Attack;
			Cmd.TargetEntity = Cursor.ClickedId;
			Cmd.TargetLocation = Cursor.ClickedActor->GetActorLocation();
			Cmd.SourceEntities.Remove(Cmd.TargetEntity);
		}
		else if (Cursor.bHasMovePoint)
		{
			Cmd.Type = EKodCommandType::AttackMove;
			Cmd.TargetLocation = Cursor.MovePoint;
		}
		else
		{
			return true;
		}
	}
	else if (ArmedOrder == FName(TEXT("OrderGuard")) || ArmedOrder == FName(TEXT("Guard")))
	{
		AActor* Subject = Cursor.FollowActor ? Cursor.FollowActor : Cursor.AttackActor;
		if (!Subject)
		{
			return true;
		}
		Cmd.Type = EKodCommandType::Guard;
		Cmd.TargetEntity = Cursor.FollowActor ? Cursor.FollowId : Cursor.AttackId;
		Cmd.TargetLocation = Subject->GetActorLocation();
		Cmd.SourceEntities.Remove(Cmd.TargetEntity);
	}
	else
	{
		ArmedOrder = FName();
		return false;
	}
	if (Cmd.SourceEntities.Num() > 0)
	{
		Commands->Enqueue(Cmd);
	}
	if (!Cmd.bQueued)
	{
		ArmedOrder = FName();
	}
	return true;
}

void AKodSlice0PlayerController::IssueAbility(FName AbilityId)
{
	UWorld* World = GetWorld();
	UKodCommandSubsystem* Commands = World ? World->GetSubsystem<UKodCommandSubsystem>() : nullptr;
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Commands || !Sim || AbilityId.IsNone())
	{
		return;
	}
	FKodCommand Cmd;
	Cmd.Type = EKodCommandType::UseAbility;
	Cmd.IssuerPlayerId = GetLocalPlayer() ? GetLocalPlayer()->GetControllerId() : 0;
	Cmd.PayloadName = AbilityId;
	for (const FKodEntityId& Id : GetLocalSelectedEntityIds())
	{
		FKodSimEntityState State;
		if (Sim->TryGetState(Id, State)
			&& IsActiveCommandSubgroup(State.DefinitionId)
			&& Sim->UnitHasAbility(State.DefinitionId, AbilityId))
		{
			Cmd.SourceEntities.Add(Id);
		}
	}
	if (Cmd.SourceEntities.Num() > 0)
	{
		Commands->Enqueue(Cmd);
	}
}
