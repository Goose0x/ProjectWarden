#include "Game/KodPlayerController.h"
#include "Game/KodRTSCameraPawn.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Sim/KodSimSubsystem.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "CollisionQueryParams.h"
#include "GameFramework/HUD.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

AKodPlayerController::AKodPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	DefaultMouseCursor = EMouseCursor::Default;
	bEnableClickEvents = true;
	PrimaryActorTick.bCanEverTick = true;
}

void AKodPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearSelectionHighlight();
	Super::EndPlay(EndPlayReason);
}

void AKodPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// PIE: GameAndUI + unlocked cursor so LMB/RMB reach PlayerTick (WasInputKeyJustPressed).
	// Viewport capture is CaptureDuringMouseDown (DefaultInput.ini).
	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);
	bShowMouseCursor = true;

	if (ULocalPlayer* LP = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			if (UInputMappingContext* IMC = DefaultMappingContext.LoadSynchronous())
			{
				Subsystem->AddMappingContext(IMC, 0);
			}
		}
	}
}

void AKodPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	// Bind UInputAction assets (IA_Select / IA_CommandMove / IA_CommandAttack) via
	// UEnhancedInputComponent once Content/Core/Input assets exist.
	// Fallback: PlayerTick polls mouse buttons for Slice 0 greybox PIE.
}

void AKodPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	// Greybox mouse fallback until Enhanced Input assets exist.
	if (WasInputKeyJustPressed(EKeys::LeftMouseButton))
	{
		HandleSelectStarted();
	}
	if (bSelectPressed)
	{
		float X = 0.f, Y = 0.f;
		GetMousePosition(X, Y);
		UpdateMarquee(FVector2D(X, Y));
	}
	if (WasInputKeyJustReleased(EKeys::LeftMouseButton))
	{
		HandleSelectCompleted();
	}
	if (WasInputKeyJustPressed(EKeys::RightMouseButton))
	{
		HandleRightClickCommand();
	}
}

void AKodPlayerController::PanCamera(FVector2D AxisValue)
{
	if (AKodRTSCameraPawn* Cam = Cast<AKodRTSCameraPawn>(GetPawn()))
	{
		Cam->Pan(AxisValue);
	}
}

void AKodPlayerController::ZoomCamera(float AxisValue)
{
	if (AKodRTSCameraPawn* Cam = Cast<AKodRTSCameraPawn>(GetPawn()))
	{
		Cam->Zoom(AxisValue);
	}
}

bool AKodPlayerController::GetGroundHitUnderCursor(FHitResult& OutHit) const
{
	return const_cast<AKodPlayerController*>(this)->GetHitResultUnderCursor(ECC_Visibility, true, OutHit);
}

TArray<AActor*> AKodPlayerController::GetLocalSelection() const
{
	TArray<AActor*> Actors;
	Actors.Reserve(LocalSelection.Num());
	for (const TWeakObjectPtr<AActor>& Ptr : LocalSelection)
	{
		if (AActor* Actor = Ptr.Get())
		{
			Actors.Add(Actor);
		}
	}
	return Actors;
}

TArray<FKodEntityId> AKodPlayerController::GetLocalSelectedEntityIds() const
{
	TArray<FKodEntityId> Ids;
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Sim)
	{
		return Ids;
	}
	for (const TWeakObjectPtr<AActor>& Ptr : LocalSelection)
	{
		if (AActor* A = Ptr.Get())
		{
			FKodEntityId Id = Sim->FindIdForActor(A);
			if (Id.IsValid())
			{
				Ids.Add(Id);
			}
		}
	}
	return Ids;
}

void AKodPlayerController::HandleSelectStarted()
{
	float X = 0.f, Y = 0.f;
	GetMousePosition(X, Y);
	BeginMarquee(FVector2D(X, Y));
	bSelectPressed = true;
}

void AKodPlayerController::HandleSelectCompleted()
{
	bSelectPressed = false;
	const float Drag = FVector2D::Distance(MarqueeStart, MarqueeEnd);
	if (Drag >= BoxSelectDragThresholdPx)
	{
		EndMarquee(IsInputKeyDown(EKeys::LeftShift));
	}
	else
	{
		bMarqueeActive = false;
		ClickSelectAtCursor(IsInputKeyDown(EKeys::LeftShift));
	}
}

void AKodPlayerController::HandleCommandMove()
{
	FHitResult Hit;
	if (!GetGroundHitUnderCursor(Hit))
	{
		return;
	}
	IssueMoveToSelection(Hit.ImpactPoint);
}

void AKodPlayerController::HandleCommandAttack()
{
	const FKodCursorCommand Cmd = TraceCursorCommand();
	if (Cmd.AttackActor)
	{
		IssueAttackToSelection(Cmd.AttackActor);
	}
}

bool AKodPlayerController::IsInLocalSelection(const AActor* Actor) const
{
	if (!Actor)
	{
		return false;
	}
	for (const TWeakObjectPtr<AActor>& Ptr : LocalSelection)
	{
		if (Ptr.Get() == Actor)
		{
			return true;
		}
	}
	return false;
}

AKodPlayerController::FKodCursorCommand AKodPlayerController::TraceCursorCommand() const
{
	FKodCursorCommand Cmd;

	UWorld* World = GetWorld();
	if (!World)
	{
		Cmd.HitName = TEXT("no_world");
		return Cmd;
	}

	float MouseX = 0.f;
	float MouseY = 0.f;
	if (!GetMousePosition(MouseX, MouseY))
	{
		Cmd.HitName = TEXT("no_mouse");
		return Cmd;
	}
	if (AHUD* HUD = GetHUD())
	{
		if (HUD->GetHitBoxAtCoordinates(FVector2D(MouseX, MouseY), true))
		{
			Cmd.HitName = TEXT("hud");
			return Cmd;
		}
	}

	FVector Origin;
	FVector Direction;
	if (!DeprojectScreenPositionToWorld(MouseX, MouseY, Origin, Direction))
	{
		Cmd.HitName = TEXT("deproject_failed");
		return Cmd;
	}

	UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>();
	const FVector End = Origin + Direction * HitResultTraceDistance;

		FCollisionQueryParams Params(TEXT("KodRMB"), /*bTraceComplex*/ false);
		if (AActor* ControlledPawn = GetPawn())
		{
			Params.AddIgnoredActor(ControlledPawn);
		}

	// Ground and PROXY_* are often the first Pawn hit, in front of the unit.
	// Keep that surface as the Move point, and keep walking until a foreign sim entity.
	bool bSawPawnHit = false;
	constexpr int32 MaxSteps = 32;
	for (int32 Step = 0; Step < MaxSteps; ++Step)
	{
		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit, Origin, End, ECC_Pawn, Params) || !Hit.bBlockingHit)
		{
			break;
		}

		bSawPawnHit = true;
		AActor* HitActor = Hit.GetActor();
		if (!Cmd.bHasMovePoint)
		{
			Cmd.bHasMovePoint = true;
			Cmd.MovePoint = Hit.ImpactPoint;
			Cmd.HitName = HitActor ? HitActor->GetName() : TEXT("None");
			const FKodEntityId FirstId = (Sim && HitActor) ? Sim->FindIdForActor(HitActor) : FKodEntityId();
			Cmd.bSim = FirstId.IsValid();
		}
		if (!HitActor)
		{
			break;
		}

		const FKodEntityId Id = Sim ? Sim->FindIdForActor(HitActor) : FKodEntityId();
		if (Id.IsValid() && !IsInLocalSelection(HitActor))
		{
			Cmd.AttackActor = HitActor;
			Cmd.AttackId = Id;
			return Cmd;
		}

		Params.AddIgnoredActor(HitActor);
	}

	if (bSawPawnHit && Cmd.bHasMovePoint)
	{
		return Cmd;
	}

	FHitResult GroundHit;
	if (GetGroundHitUnderCursor(GroundHit) && GroundHit.bBlockingHit)
	{
		AActor* HitActor = GroundHit.GetActor();
		if (!HitActor || HitActor != GetPawn())
		{
			Cmd.bHasMovePoint = true;
			Cmd.MovePoint = GroundHit.ImpactPoint;
			Cmd.HitName = HitActor ? HitActor->GetName() : TEXT("None");
			const FKodEntityId Id = (Sim && HitActor) ? Sim->FindIdForActor(HitActor) : FKodEntityId();
			Cmd.bSim = Id.IsValid();
			if (Id.IsValid() && HitActor && !IsInLocalSelection(HitActor))
			{
				Cmd.AttackActor = HitActor;
				Cmd.AttackId = Id;
			}
		}
	}

	if (Cmd.HitName.IsEmpty())
	{
		Cmd.HitName = TEXT("none");
	}
	return Cmd;
}

void AKodPlayerController::HandleRightClickCommand()
{
	const FKodCursorCommand Cmd = TraceCursorCommand();
	if (Cmd.AttackActor)
	{
		UE_LOG(LogTemp, Log, TEXT("RMB Attack LocalSelection=%d Target=%s Id=%d"),
			LocalSelection.Num(),
			*Cmd.AttackActor->GetName(),
			Cmd.AttackId.Value);
		IssueAttackToSelection(Cmd.AttackActor);
		return;
	}
	if (Cmd.bHasMovePoint)
	{
		UE_LOG(LogTemp, Log, TEXT("RMB Move LocalSelection=%d Dest=%.0f,%.0f,%.0f Hit=%s sim=%d"),
			LocalSelection.Num(),
			Cmd.MovePoint.X,
			Cmd.MovePoint.Y,
			Cmd.MovePoint.Z,
			Cmd.HitName.IsEmpty() ? TEXT("None") : *Cmd.HitName,
			Cmd.bSim ? 1 : 0);
		IssueMoveToSelection(Cmd.MovePoint);
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("RMB Miss %s"), Cmd.HitName.IsEmpty() ? TEXT("none") : *Cmd.HitName);
}

void AKodPlayerController::IssueMoveToSelection_Implementation(FVector WorldLocation)
{
	// Default path: sim IssueMove (walk gate). Prefer UKodCommandSubsystem via Slice0 PC subclass.
	const TArray<FKodEntityId> Sources = GetLocalSelectedEntityIds();
	UE_LOG(LogTemp, Log, TEXT("Move Issued Sources=%d Dest=%.0f,%.0f,%.0f"),
		Sources.Num(),
		WorldLocation.X,
		WorldLocation.Y,
		WorldLocation.Z);

	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Sim)
	{
		return;
	}
	for (const FKodEntityId& Id : Sources)
	{
		Sim->IssueMove(Id, WorldLocation);
	}
}

void AKodPlayerController::IssueAttackToSelection_Implementation(AActor* Target)
{
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Sim || !Target)
	{
		return;
	}
	const FKodEntityId TargetId = Sim->FindIdForActor(Target);
	if (!TargetId.IsValid())
	{
		// Pivot fallback used to walk the selection to the actor origin (~0,0) on greybox floors.
		UE_LOG(LogTemp, Log, TEXT("Attack Reject NonSim Target=%s"), *Target->GetName());
		return;
	}

	int32 Issued = 0;
	for (const FKodEntityId& Id : GetLocalSelectedEntityIds())
	{
		if (Id != TargetId)
		{
			Sim->IssueAttack(Id, TargetId);
			++Issued;
		}
	}
	if (Issued == 0)
	{
		UE_LOG(LogTemp, Log, TEXT("Attack Reject Self Target=%s Id=%d"), *Target->GetName(), TargetId.Value);
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("Attack Issued Sources=%d Target=%s Id=%d"),
		Issued,
		*Target->GetName(),
		TargetId.Value);
}

void AKodPlayerController::BeginMarquee(FVector2D ScreenPos)
{
	bMarqueeActive = true;
	MarqueeStart = ScreenPos;
	MarqueeEnd = ScreenPos;
}

void AKodPlayerController::UpdateMarquee(FVector2D ScreenPos)
{
	if (bMarqueeActive)
	{
		MarqueeEnd = ScreenPos;
	}
}

void AKodPlayerController::EndMarquee(bool bAddToSelection)
{
	bMarqueeActive = false;
	TArray<AActor*> Hits;
	CollectActorsInMarquee(Hits);
	if (!bAddToSelection)
	{
		LocalSelection.Reset();
	}
	for (AActor* A : Hits)
	{
		LocalSelection.AddUnique(A);
	}
	ApplySelectionHighlight();
}

AActor* AKodPlayerController::TraceSelectableUnderCursor(FString& OutHitLog) const
{
	OutHitLog.Reset();

	UWorld* World = GetWorld();
	if (!World)
	{
		OutHitLog = TEXT("no_world");
		return nullptr;
	}

	float MouseX = 0.f;
	float MouseY = 0.f;
	if (!GetMousePosition(MouseX, MouseY))
	{
		OutHitLog = TEXT("no_mouse");
		return nullptr;
	}
	// Same HUD hit-box early-out as GetHitResultUnderCursor.
	if (AHUD* HUD = GetHUD())
	{
		if (HUD->GetHitBoxAtCoordinates(FVector2D(MouseX, MouseY), true))
		{
			OutHitLog = TEXT("hud");
			return nullptr;
		}
	}

	FVector Origin;
	FVector Direction;
	if (!DeprojectScreenPositionToWorld(MouseX, MouseY, Origin, Direction))
	{
		OutHitLog = TEXT("deproject_failed");
		return nullptr;
	}

	UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>();
	// Same length GetHitResultUnderCursor uses. A channel trace stops on the first
	// BlockAll hit, so a PROXY mesh in front of a KodUnit used to win and get discarded.
	// Re-trace, ignoring actors with no sim id, until a registered entity or a miss.
	const FVector End = Origin + Direction * HitResultTraceDistance;

	struct FChannelWalk
	{
		AActor* Picked = nullptr;
		bool bSawBlocker = false;
	};

	auto WalkChannel = [&](ECollisionChannel Channel, const TCHAR* ChannelName) -> FChannelWalk
	{
		FChannelWalk Result;
		FCollisionQueryParams Params(TEXT("ClickSelect"), /*bTraceComplex*/ false);
		constexpr int32 MaxSteps = 32;
		for (int32 Step = 0; Step < MaxSteps; ++Step)
		{
			FHitResult Hit;
			if (!World->LineTraceSingleByChannel(Hit, Origin, End, Channel, Params))
			{
				return Result;
			}

			Result.bSawBlocker = true;
			AActor* HitActor = Hit.GetActor();
			if (!HitActor)
			{
				if (!OutHitLog.IsEmpty())
				{
					OutHitLog += TEXT(",");
				}
				OutHitLog += FString::Printf(TEXT("null(%s,sim=0)"), ChannelName);
				return Result;
			}

			const FKodEntityId Id = Sim ? Sim->FindIdForActor(HitActor) : FKodEntityId();
			const FVector Loc = HitActor->GetActorLocation();
			if (!OutHitLog.IsEmpty())
			{
				OutHitLog += TEXT(",");
			}
			OutHitLog += FString::Printf(
				TEXT("%s(%s,sim=%d,id=%d,loc=%.0f,%.0f,%.0f)"),
				*HitActor->GetName(),
				ChannelName,
				Id.IsValid() ? 1 : 0,
				Id.Value,
				Loc.X,
				Loc.Y,
				Loc.Z);

			if (Id.IsValid())
			{
				Result.Picked = HitActor;
				return Result;
			}

			Params.AddIgnoredActor(HitActor);
		}

		if (!OutHitLog.IsEmpty())
		{
			OutHitLog += TEXT(",");
		}
		OutHitLog += TEXT("truncated");
		return Result;
	};

	const FChannelWalk PawnWalk = WalkChannel(ECC_Pawn, TEXT("Pawn"));
	if (PawnWalk.Picked)
	{
		return PawnWalk.Picked;
	}
	// Pawn already hit unregistered blockers (PROXY_* / ground). That click clears.
	// Visibility runs only when the pawn channel hit nothing at all.
	if (PawnWalk.bSawBlocker)
	{
		return nullptr;
	}

	return WalkChannel(ECC_Visibility, TEXT("Visibility")).Picked;
}

void AKodPlayerController::ClickSelectAtCursor(bool bAddToSelection)
{
	FString HitLog;
	AActor* Picked = TraceSelectableUnderCursor(HitLog);

	if (!bAddToSelection)
	{
		LocalSelection.Reset();
	}
	if (Picked)
	{
		LocalSelection.AddUnique(Picked);
	}

	ApplySelectionHighlight();

	UE_LOG(LogTemp, Log, TEXT("ClickSelectAtCursor LocalSelection=%d Picked=%s Hits=%s"),
		LocalSelection.Num(),
		Picked ? *Picked->GetName() : TEXT("None"),
		HitLog.IsEmpty() ? TEXT("none") : *HitLog);
}

namespace KodSelectionHighlight
{
	constexpr int32 StencilValue = 1;
	const FLinearColor TintColor(1.f, 0.42f, 0.05f, 1.f);

	bool IsHighlightMesh(const UMeshComponent* Mesh)
	{
		if (!Mesh || !Mesh->IsVisible() || Mesh->bHiddenInGame)
		{
			return false;
		}
		if (const UStaticMeshComponent* StaticMesh = Cast<UStaticMeshComponent>(Mesh))
		{
			return StaticMesh->GetStaticMesh() != nullptr;
		}
		if (const USkeletalMeshComponent* Skel = Cast<USkeletalMeshComponent>(Mesh))
		{
			return Skel->GetSkeletalMeshAsset() != nullptr;
		}
		return Mesh->GetNumMaterials() > 0;
	}
}

void AKodPlayerController::ClearSelectionHighlight()
{
	if (const UWorld* World = GetWorld())
	{
		if (World->bIsTearingDown)
		{
			SelectionHighlights.Reset();
			return;
		}
	}

	for (const FKodSelectionHighlightState& State : SelectionHighlights)
	{
		UMeshComponent* Mesh = State.Mesh;
		if (!Mesh)
		{
			continue;
		}
		const int32 Slots = Mesh->GetNumMaterials();
		for (int32 Index = 0; Index < State.OriginalMaterials.Num() && Index < Slots; ++Index)
		{
			Mesh->SetMaterial(Index, State.OriginalMaterials[Index]);
		}
		Mesh->SetRenderCustomDepth(State.bHadCustomDepth);
		Mesh->SetCustomDepthStencilValue(State.OriginalStencil);
	}
	SelectionHighlights.Reset();
}

UMaterialInstanceDynamic* AKodPlayerController::GetSelectionTintMaterial()
{
	if (SelectionTintMid)
	{
		return SelectionTintMid;
	}

	UMaterialInterface* Base = LoadObject<UMaterialInterface>(
		nullptr,
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (!Base)
	{
		return nullptr;
	}

	SelectionTintMid = UMaterialInstanceDynamic::Create(Base, this);
	if (SelectionTintMid)
	{
		SelectionTintMid->SetVectorParameterValue(TEXT("Color"), KodSelectionHighlight::TintColor);
		SelectionTintMid->SetVectorParameterValue(TEXT("BaseColor"), KodSelectionHighlight::TintColor);
	}
	return SelectionTintMid;
}

void AKodPlayerController::ApplySelectionHighlight()
{
	ClearSelectionHighlight();

	UMaterialInstanceDynamic* Tint = GetSelectionTintMaterial();
	int32 MeshCount = 0;
	for (AActor* Actor : GetLocalSelection())
	{
		if (!Actor)
		{
			continue;
		}
		TInlineComponentArray<UMeshComponent*> Meshes;
		Actor->GetComponents(Meshes);
		for (UMeshComponent* Mesh : Meshes)
		{
			if (!KodSelectionHighlight::IsHighlightMesh(Mesh))
			{
				continue;
			}

			FKodSelectionHighlightState State;
			State.Mesh = Mesh;
			State.bHadCustomDepth = Mesh->bRenderCustomDepth;
			State.OriginalStencil = Mesh->CustomDepthStencilValue;

			const int32 Slots = Mesh->GetNumMaterials();
			State.OriginalMaterials.Reserve(Slots);
			for (int32 Index = 0; Index < Slots; ++Index)
			{
				State.OriginalMaterials.Add(Mesh->GetMaterial(Index));
				if (Tint)
				{
					Mesh->SetMaterial(Index, Tint);
				}
			}

			Mesh->SetRenderCustomDepth(true);
			Mesh->SetCustomDepthStencilValue(KodSelectionHighlight::StencilValue);
			SelectionHighlights.Add(State);
			++MeshCount;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("SelectionHighlight LocalSelection=%d Meshes=%d Tint=%d Stencil=%d"),
		LocalSelection.Num(),
		MeshCount,
		Tint ? 1 : 0,
		KodSelectionHighlight::StencilValue);
}

void AKodPlayerController::CollectActorsInMarquee(TArray<AActor*>& OutActors) const
{
	OutActors.Reset();
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!World || !Sim)
	{
		return;
	}

	const float MinX = FMath::Min(MarqueeStart.X, MarqueeEnd.X);
	const float MaxX = FMath::Max(MarqueeStart.X, MarqueeEnd.X);
	const float MinY = FMath::Min(MarqueeStart.Y, MarqueeEnd.Y);
	const float MaxY = FMath::Max(MarqueeStart.Y, MarqueeEnd.Y);

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || !Sim->FindIdForActor(Actor).IsValid())
		{
			continue;
		}
		FVector2D Screen;
		if (!ProjectWorldLocationToScreen(Actor->GetActorLocation(), Screen))
		{
			continue;
		}
		if (Screen.X >= MinX && Screen.X <= MaxX && Screen.Y >= MinY && Screen.Y <= MaxY)
		{
			OutActors.Add(Actor);
		}
	}
}
