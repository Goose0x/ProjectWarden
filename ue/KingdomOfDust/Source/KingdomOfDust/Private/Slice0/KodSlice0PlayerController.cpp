#include "Slice0/KodSlice0PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
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
	if (Sim->IsResourceNode(TargetId) || Sim->IsDropOff(TargetId))
	{
		UE_LOG(LogTemp, Log, TEXT("Attack Reject Node Target=%s Id=%d"), *Target->GetName(), TargetId.Value);
		return;
	}

	FKodCommand Cmd;
	Cmd.Type = EKodCommandType::Attack;
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
	GatherCmd.IssuerPlayerId = GetLocalPlayer() ? GetLocalPlayer()->GetControllerId() : 0;
	GatherCmd.TargetEntity = NodeId;
	GatherCmd.TargetLocation = Node->GetActorLocation();

	FKodCommand MoveCmd;
	MoveCmd.Type = EKodCommandType::Move;
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
		if (WasInputKeyJustPressed(EKeys::D))
		{
			PlaceType = FName(KodWardenPaths::Id_SupplyDepot);
		}
		else if (WasInputKeyJustPressed(EKeys::R))
		{
			PlaceType = FName(KodWardenPaths::Id_Barracks);
		}
		else if (WasInputKeyJustPressed(EKeys::E))
		{
			PlaceType = FName(KodWardenPaths::Id_Rig);
		}
	}
	else if (!bBuildMenu)
	{
		PlaceType = NAME_None;
	}
	UpdateBuildGhost();
	if (WasInputKeyJustPressed(EKeys::S))
	{
		EnqueueTrainOnSelectedStructures();
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
	if (IsInputKeyDown(EKeys::LeftControl) || IsInputKeyDown(EKeys::RightControl))
	{
		return;
	}

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
		const float Previous = LastGroupTapSeconds[Index];
		LastGroupTapSeconds[Index] = Now;
		if (Previous < 0.f || (Now - Previous) > DoubleTapSeconds)
		{
			continue;
		}
		LastGroupTapSeconds[Index] = -1.f;
		const TArray<FKodEntityId> Ids = Store->GetGroup(Index);
		if (Ids.Num() == 0)
		{
			continue;
		}
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

void AKodSlice0PlayerController::EnqueueTrainOnSelectedStructures()
{
	UWorld* World = GetWorld();
	UKodCommandSubsystem* Commands = World ? World->GetSubsystem<UKodCommandSubsystem>() : nullptr;
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Commands || !Sim)
	{
		return;
	}
	FKodCommand Cmd;
	Cmd.Type = EKodCommandType::Train;
	Cmd.IssuerPlayerId = GetLocalPlayer() ? GetLocalPlayer()->GetControllerId() : 0;
	for (const FKodEntityId& Id : GetLocalSelectedEntityIds())
	{
		FName TrainId;
		if (Sim->IsStructure(Id) && Sim->TryGetPrimaryTrain(Id, TrainId))
		{
			FKodCommand One = Cmd;
			One.PayloadName = TrainId;
			One.SourceEntities.Add(Id);
			Commands->Enqueue(One);
		}
	}
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
		if (Sim->IsStructure(Id))
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
		if (Sim->TryGetState(Id, State) && State.bCanGather && State.Health > 0.f)
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
