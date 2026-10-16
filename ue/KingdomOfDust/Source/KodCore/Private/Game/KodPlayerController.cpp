#include "Game/KodPlayerController.h"
#include "Audio/KodCueSubsystem.h"
#include "Game/KodJadeitePalette.h"
#include "Game/KodJadeiteTunerWidget.h"
#include "Misc/Char.h"
#include "Components/PrimitiveComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Game/KodCollision.h"
#include "Game/KodGameMode.h"
#include "Game/KodHUD.h"
#include "Game/KodPortraitStage.h"
#include "Game/KodRTSCameraPawn.h"
#include "Game/KodSpectatorPawn.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Sim/KodSimSubsystem.h"
#include "Sim/KodResourceTypes.h"
#include "Game/KodPlayerState.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "CollisionQueryParams.h"
#include "Math/RotationMatrix.h"
#include "GameFramework/HUD.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/SoftObjectPath.h"

namespace
{
	/** TeamId on the sim entity. Missing state stays selectable (pre-team behavior). Resource nodes are not local units. */
	bool IsLocalTeamSimId(const UKodSimSubsystem* Sim, FKodEntityId Id, int32 LocalTeam)
	{
		if (!Sim || !Id.IsValid())
		{
			return false;
		}
		if (Sim->IsResourceNode(Id) || Sim->IsDropOff(Id))
		{
			return false;
		}
		FKodSimEntityState State;
		if (!Sim->TryGetState(Id, State))
		{
			return true;
		}
		return State.TeamId == LocalTeam;
	}

	/** Local-team units, plus resource nodes (neutral, selectable, not attackable). */
	bool IsSelectableSimId(const UKodSimSubsystem* Sim, FKodEntityId Id, int32 LocalTeam)
	{
		if (!Sim || !Id.IsValid())
		{
			return false;
		}
		if (Sim->IsResourceNode(Id))
		{
			return true;
		}
		if (Sim->IsStructure(Id))
		{
			FKodStructureState Structure;
			return Sim->TryGetStructure(Id, Structure) && Structure.TeamId == LocalTeam;
		}
		return IsLocalTeamSimId(Sim, Id, LocalTeam);
	}
}

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
	if (JadeiteTuner)
	{
		JadeiteTuner->RemoveFromParent();
		JadeiteTuner = nullptr;
	}
	bJadeiteTunerOpen = false;
	ClearSelectionHighlight();
	Super::EndPlay(EndPlayReason);
}

void AKodPlayerController::ToggleJadeiteTuner()
{
	UWorld* World = GetWorld();
	UKodJadeitePaletteSubsystem* Palette = World ? World->GetSubsystem<UKodJadeitePaletteSubsystem>() : nullptr;
	if (!bJadeiteTunerOpen && (!Palette || !Palette->IsTuningAllowed()))
	{
		UE_LOG(LogTemp, Log, TEXT("KodJadeite tuner refused (campaign and offline skirmish only)"));
		return;
	}
	if (!JadeiteTuner)
	{
		JadeiteTuner = CreateWidget<UKodJadeiteTunerWidget>(this, UKodJadeiteTunerWidget::StaticClass());
		if (!JadeiteTuner)
		{
			return;
		}
	}
	bJadeiteTunerOpen = !bJadeiteTunerOpen;
	if (bJadeiteTunerOpen)
	{
		if (!JadeiteTuner->IsInViewport())
		{
			JadeiteTuner->AddToViewport(40);
		}
		JadeiteTuner->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		JadeiteTuner->SetVisibility(ESlateVisibility::Collapsed);
	}
}

bool AKodPlayerController::IsJadeiteTunerUnderCursor() const
{
	return bJadeiteTunerOpen && JadeiteTuner && JadeiteTuner->IsCursorOverPanel();
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

void AKodPlayerController::UpdateHiddenComponents(const FVector& ViewLocation, TSet<FPrimitiveComponentId>& HiddenComponents)
{
	Super::UpdateHiddenComponents(ViewLocation, HiddenComponents);
	if (const AKodHUD* HUD = Cast<AKodHUD>(GetHUD()))
	{
		if (const AKodPortraitStage* Stage = HUD->GetPortraitStage())
		{
			Stage->AppendHiddenPrimitives(HiddenComponents);
		}
	}
}

void AKodPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	AKodGameMode* MatchMode = GetWorld() ? GetWorld()->GetAuthGameMode<AKodGameMode>() : nullptr;
	AKodHUD* PromptHUD = Cast<AKodHUD>(GetHUD());
	if (MatchMode && MatchMode->IsAwaitingFactionPrompt() && PromptHUD && !PromptHUD->IsFactionPromptOpen())
	{
		PromptHUD->OpenFactionPrompt();
	}
	if (PromptHUD && PromptHUD->IsFactionPromptOpen())
	{
		// The Play click and a held Enter arrive on the first tick. Ignore them
		// until the prompt has drawn and those keys have been released.
		if (!PromptHUD->HasDrawnFactionPrompt())
		{
			bFactionPromptLatched = true;
			bFactionKeysReleased = false;
			return;
		}
		if (!bFactionPromptLatched)
		{
			bFactionPromptLatched = true;
			bFactionKeysReleased = false;
		}
		if (!bFactionKeysReleased)
		{
			const bool bEnterDown = IsInputKeyDown(EKeys::Enter);
			const bool bClickDown = IsInputKeyDown(EKeys::LeftMouseButton);
			bFactionKeysReleased = !bEnterDown && !bClickDown;
			return;
		}
		if (WasInputKeyJustPressed(EKeys::One) || WasInputKeyJustPressed(EKeys::V))
		{
			PromptHUD->SetFactionPromptIndex(0);
		}
		if (WasInputKeyJustPressed(EKeys::Two) || WasInputKeyJustPressed(EKeys::S))
		{
			PromptHUD->SetFactionPromptIndex(1);
		}
		bool bConfirm = WasInputKeyJustPressed(EKeys::Enter);
		if (WasInputKeyJustPressed(EKeys::LeftMouseButton))
		{
			float MouseX = 0.f;
			float MouseY = 0.f;
			FKodHudChromeHit Hit;
			if (GetMousePosition(MouseX, MouseY) && PromptHUD->GetChromeHit(MouseX, MouseY, Hit) && Hit.Kind == FName(TEXT("FactionPrompt")))
			{
				PromptHUD->SetFactionPromptIndex(Hit.IntParam);
				bConfirm = true;
			}
		}
		if (bConfirm)
		{
			if (MatchMode)
			{
				MatchMode->ApplyStartFaction(PromptHUD->GetFactionPromptIndex());
			}
			bFactionPromptLatched = false;
			bFactionKeysReleased = false;
		}
		return;
	}
	bFactionPromptLatched = false;
	bFactionKeysReleased = false;
	if (AKodHUD* ChatHUD = Cast<AKodHUD>(GetHUD()))
	{
		if (ChatHUD->IsChatOpen())
		{
			if (WasInputKeyJustPressed(EKeys::Escape))
			{
				ChatHUD->CloseChat();
				return;
			}
			if (WasInputKeyJustPressed(EKeys::Enter))
			{
				ChatHUD->SubmitChat();
				return;
			}
			if (WasInputKeyJustPressed(EKeys::BackSpace))
			{
				ChatHUD->BackspaceChat();
				return;
			}
			if (WasInputKeyJustPressed(EKeys::SpaceBar))
			{
				ChatHUD->AppendChatText(TEXT(" "));
				return;
			}
			const FKey Keys[] = {
				EKeys::A, EKeys::B, EKeys::C, EKeys::D, EKeys::E, EKeys::F, EKeys::G, EKeys::H, EKeys::I, EKeys::J,
				EKeys::K, EKeys::L, EKeys::M, EKeys::N, EKeys::O, EKeys::P, EKeys::Q, EKeys::R, EKeys::S, EKeys::T,
				EKeys::U, EKeys::V, EKeys::W, EKeys::X, EKeys::Y, EKeys::Z
			};
			const TCHAR* Letters = TEXT("abcdefghijklmnopqrstuvwxyz");
			for (int32 Index = 0; Index < UE_ARRAY_COUNT(Keys); ++Index)
			{
				if (WasInputKeyJustPressed(Keys[Index]))
				{
					const bool bShift = IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift);
					FString Glyph;
					Glyph.AppendChar(bShift ? FChar::ToUpper(Letters[Index]) : Letters[Index]);
					ChatHUD->AppendChatText(Glyph);
					return;
				}
			}
			const FKey Digits[] = {
				EKeys::Zero, EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four,
				EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine
			};
			for (int32 Index = 0; Index < UE_ARRAY_COUNT(Digits); ++Index)
			{
				if (WasInputKeyJustPressed(Digits[Index]))
				{
					ChatHUD->AppendChatText(FString::FromInt(Index));
					return;
				}
			}
			return;
		}
		if (WasInputKeyJustPressed(EKeys::BackSpace) && !ChatHUD->IsMenuOpen())
		{
			const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
			if (LastBaseCameraTap >= 0.f && (Now - LastBaseCameraTap) <= 0.3f)
			{
				LastBaseCameraTap = -1.f;
				if (AKodRTSCameraPawn* CameraPawn = Cast<AKodRTSCameraPawn>(GetPawn()))
				{
					CameraPawn->SnapToPlayerStart();
				}
			}
			else
			{
				LastBaseCameraTap = Now;
			}
		}
		if (WasInputKeyJustPressed(EKeys::Enter) && !ChatHUD->IsMenuOpen())
		{
			ChatHUD->OpenChat();
			return;
		}
	}
	if (WasInputKeyJustPressed(EKeys::F9))
	{
		ToggleJadeiteTuner();
	}
	if (bJadeiteTunerOpen)
	{
		if (const UWorld* TunerWorld = GetWorld())
		{
			if (const UKodJadeitePaletteSubsystem* Palette = TunerWorld->GetSubsystem<UKodJadeitePaletteSubsystem>())
			{
				if (!Palette->IsTuningAllowed())
				{
					ToggleJadeiteTuner();
				}
			}
		}
	}
	if (WasInputKeyJustPressed(EKeys::F10) || WasInputKeyJustPressed(EKeys::Escape))
	{
		if (AKodHUD* MenuHUD = Cast<AKodHUD>(GetHUD()))
		{
			MenuHUD->TogglePauseMenu();
			if (MenuHUD->IsMenuOpen())
			{
				return;
			}
		}
	}

	// A swapped-away controller must not keep selecting. A spectator pawn keeps the camera and drops orders.
	if (!IsLocalPlayerController() || Cast<AKodSpectatorPawn>(GetPawn()))
	{
		return;
	}
	const bool bQuestion = WasInputKeyJustPressed(EKeys::Slash)
		&& (IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift));
	if (WasInputKeyJustPressed(EKeys::F12) || bQuestion)
	{
		if (AKodHUD* LegendHUD = Cast<AKodHUD>(GetHUD()))
		{
			LegendHUD->ToggleHotkeyLegend();
		}
	}
	if (const UWorld* World = GetWorld())
	{
		if (const UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>())
		{
			if (Sim->IsMatchOver())
			{
				bSelectPressed = false;
				bMarqueeActive = false;
				return;
			}
		}
	}

	const AKodRTSCameraPawn* CameraPawn = Cast<AKodRTSCameraPawn>(GetPawn());
	const bool bAltDrag = CameraPawn && CameraPawn->ConsumesAltDrag();

	SyncCommandSubgroup();
	if (WasInputKeyJustPressed(EKeys::Tab))
	{
		if (const AKodHUD* HUD = Cast<AKodHUD>(GetHUD()))
		{
			const int32 Subgroups = HUD->GetCommandSubgroupCount();
			if (Subgroups > 1)
			{
				CommandSubgroupIndex = (CommandSubgroupIndex + 1) % Subgroups;
			}
		}
	}
	if (WasInputKeyJustPressed(EKeys::F1))
	{
		SelectIdleWorker();
	}
	if (bMinimapDrag)
	{
		if (IsInputKeyDown(EKeys::LeftMouseButton))
		{
			JumpCameraToMinimap(true);
		}
		else
		{
			bMinimapDrag = false;
		}
	}

	// Greybox mouse fallback until Enhanced Input assets exist.
	if (IsJadeiteTunerUnderCursor())
	{
		bSelectPressed = false;
		bMarqueeActive = false;
		return;
	}
	bool bBlockWorldSelect = false;
	float PressX = 0.f;
	float PressY = 0.f;
	AKodHUD* PressHUD = Cast<AKodHUD>(GetHUD());
	FKodHudChromeHit PressChrome;
	bool bPortraitClick = false;
	if (GetMousePosition(PressX, PressY))
	{
		if (PressHUD)
		{
			bBlockWorldSelect = PressHUD->BlocksWorldSelect(PressX, PressY);
			bPortraitClick = PressHUD->GetChromeHit(PressX, PressY, PressChrome)
				&& PressChrome.Kind == FName(TEXT("Portrait"));
		}
	}
	if (WasInputKeyJustPressed(EKeys::LeftMouseButton) && !bAltDrag && bPortraitClick)
	{
		if (PressHUD->ConsumePortraitClick())
		{
			if (AKodRTSCameraPawn* FocusPawn = Cast<AKodRTSCameraPawn>(GetPawn()))
			{
				if (GetLocalSelection().Num() > 0 && GetLocalSelection()[0])
				{
					// Screen center is the rig focus, so the portrait unit lands in the middle.
					FocusPawn->JumpTo(GetLocalSelection()[0]->GetActorLocation());
				}
			}
		}
		bSelectPressed = false;
		bMarqueeActive = false;
	}
	else if (WasInputKeyJustPressed(EKeys::LeftMouseButton) && !bAltDrag && bBlockWorldSelect)
	{
		if (PressHUD)
		{
			PressHUD->CancelPortraitClick();
		}
		if (!ConsumeHudCommandClick())
		{
			ConsumeMinimapClick();
			ConsumeChromeClick();
		}
		bSelectPressed = false;
		bMarqueeActive = false;
	}
	else if (WasInputKeyJustPressed(EKeys::LeftMouseButton) && !bAltDrag
		&& !ConsumeArmedOrderClick()
		&& !ConsumeHudCommandClick()
		&& !ConsumePlacementLeftClick())
	{
		if (PressHUD)
		{
			PressHUD->CancelPortraitClick();
		}
		HandleSelectStarted();
	}
	if (bSelectPressed)
	{
		float X = 0.f, Y = 0.f;
		// A failed read used to snap the corner to (0,0) and turn a click into a box.
		if (GetMousePosition(X, Y))
		{
			UpdateMarquee(FVector2D(X, Y));
		}
	}
	if (WasInputKeyJustReleased(EKeys::LeftMouseButton) && bSelectPressed)
	{
		HandleSelectCompleted();
	}
	if (WasInputKeyJustPressed(EKeys::RightMouseButton) && !IsCursorOverCommandCard() && !ConsumePlacementRightClick())
	{
		HandleRightClickCommand();
	}
}

bool AKodPlayerController::IsCursorOverCommandCard() const
{
	float MouseX = 0.f;
	float MouseY = 0.f;
	if (!GetMousePosition(MouseX, MouseY))
	{
		return false;
	}
	const AKodHUD* HUD = Cast<AKodHUD>(GetHUD());
	if (!HUD)
	{
		return false;
	}
	FKodHudCommandHit Hit;
	return HUD->GetCommandHit(MouseX, MouseY, Hit) && Hit.bInsideChrome;
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

void AKodPlayerController::RemoveFromLocalSelection(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}
	const int32 Removed = LocalSelection.RemoveAll([Actor](const TWeakObjectPtr<AActor>& Ptr)
	{
		return Ptr.Get() == Actor;
	});
	if (Removed > 0)
	{
		ApplySelectionHighlight();
	}
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

int32 AKodPlayerController::GetLocalTeamId() const
{
	if (const AKodPlayerState* KodPS = GetPlayerState<AKodPlayerState>())
	{
		return KodPS->TeamId;
	}
	return 0;
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
	if (!DeprojectScreenWithFallback(FVector2D(MouseX, MouseY), Origin, Direction))
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

	FVector GroundPoint = FVector::ZeroVector;
	if (IntersectGroundPlane(Origin, Direction, GroundPoint))
	{
		Cmd.bHasMovePoint = true;
		Cmd.MovePoint = GroundPoint;
		Cmd.HitName = TEXT("Ground");
	}

	// KodSelect walks units and buildings. Ground and greybox meshes ignore that channel.
	constexpr int32 MaxSteps = 32;
	for (int32 Step = 0; Step < MaxSteps; ++Step)
	{
		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit, Origin, End, KodCollision::Select, Params) || !Hit.bBlockingHit)
		{
			break;
		}

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
		// A node is never an attack target. Stop here so a unit behind it is not acquired.
		// A friendly command center stays a rally. An enemy structure on that drop-off can be shot.
		if (Id.IsValid() && Sim && Sim->IsResourceNode(Id))
		{
			Cmd.GatherActor = HitActor;
			Cmd.GatherId = Id;
			return Cmd;
		}
		if (Id.IsValid() && Sim && Sim->IsDropOff(Id))
		{
			Cmd.ClickedActor = HitActor;
			Cmd.ClickedId = Id;
			if (Sim->IsAttackable(Id, GetLocalTeamId()) && !IsInLocalSelection(HitActor))
			{
				Cmd.AttackActor = HitActor;
				Cmd.AttackId = Id;
			}
			return Cmd;
		}
		if (Id.IsValid() && Sim && !IsInLocalSelection(HitActor))
		{
			Cmd.ClickedActor = HitActor;
			Cmd.ClickedId = Id;
			FKodSimEntityState HitState;
			if (Sim->TryGetState(Id, HitState) && HitState.TeamId == GetLocalTeamId())
			{
				if (HitState.bMobile)
				{
					Cmd.FollowActor = HitActor;
					Cmd.FollowId = Id;
				}
				return Cmd;
			}
			Cmd.AttackActor = HitActor;
			Cmd.AttackId = Id;
			return Cmd;
		}

		Params.AddIgnoredActor(HitActor);
	}

	if (Cmd.bHasMovePoint)
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
			if (Id.IsValid() && Sim && Sim->IsResourceNode(Id))
			{
				Cmd.GatherActor = HitActor;
				Cmd.GatherId = Id;
			}
			else if (Id.IsValid() && Sim && Sim->IsDropOff(Id))
			{
				if (Sim->IsAttackable(Id, GetLocalTeamId()) && HitActor && !IsInLocalSelection(HitActor))
				{
					Cmd.AttackActor = HitActor;
					Cmd.AttackId = Id;
				}
			}
			else if (Id.IsValid() && Sim && HitActor && !IsInLocalSelection(HitActor))
			{
				FKodSimEntityState HitState;
				if (Sim->TryGetState(Id, HitState) && HitState.TeamId == GetLocalTeamId())
				{
					if (HitState.bMobile)
					{
						Cmd.FollowActor = HitActor;
						Cmd.FollowId = Id;
					}
				}
				else
				{
					Cmd.AttackActor = HitActor;
					Cmd.AttackId = Id;
				}
			}
		}
	}

	if (Cmd.HitName.IsEmpty())
	{
		Cmd.HitName = TEXT("none");
	}
	return Cmd;
}

bool AKodPlayerController::ApplySelectedStructureRally(const FKodCursorCommand& Cmd)
{
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Sim)
	{
		return false;
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
	FKodEntityId NodeId;
	FVector Point = Cmd.bHasMovePoint ? Cmd.MovePoint : FVector::ZeroVector;
	FKodEntityId RallyUnit;
	if (Cmd.GatherActor)
	{
		NodeId = Cmd.GatherId;
		Point = Cmd.GatherActor->GetActorLocation();
	}
	else if (Cmd.FollowActor)
	{
		RallyUnit = Cmd.FollowId;
		Point = Cmd.FollowActor->GetActorLocation();
	}
	for (const FKodEntityId& Id : Structures)
	{
		Sim->SetStructureRally(Id, Point, NodeId, RallyUnit);
	}
	return !bHasUnits;
}

void AKodPlayerController::HandleRightClickCommand()
{
	float MouseX = 0.f;
	float MouseY = 0.f;
	if (GetMousePosition(MouseX, MouseY))
	{
		if (const AKodHUD* HUD = Cast<AKodHUD>(GetHUD()))
		{
			FVector WorldPoint;
			if (HUD->MinimapPixelToWorld(MouseX, MouseY, WorldPoint, false))
			{
				IssueMinimapGroundOrder(WorldPoint);
				return;
			}
			if (HUD->IsPassiveHudChrome(MouseX, MouseY))
			{
				return;
			}
		}
	}

	const FKodCursorCommand Cmd = TraceCursorCommand();
	if (ApplySelectedStructureRally(Cmd))
	{
		return;
	}
	if (Cmd.FollowActor)
	{
		UE_LOG(LogTemp, Log, TEXT("RMB Follow LocalSelection=%d Target=%s Id=%d"),
			LocalSelection.Num(),
			*Cmd.FollowActor->GetName(),
			Cmd.FollowId.Value);
		IssueFollowToSelection(Cmd.FollowActor, false);
		return;
	}
	if (Cmd.AttackActor)
	{
		UE_LOG(LogTemp, Log, TEXT("RMB Attack LocalSelection=%d Target=%s Id=%d"),
			LocalSelection.Num(),
			*Cmd.AttackActor->GetName(),
			Cmd.AttackId.Value);
		IssueAttackToSelection(Cmd.AttackActor);
		return;
	}
	if (Cmd.GatherActor)
	{
		bool bAnyGatherer = false;
		if (const UWorld* World = GetWorld())
		{
			if (const UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>())
			{
				for (const FKodEntityId& Id : GetLocalSelectedEntityIds())
				{
					FKodSimEntityState State;
					if (Sim->TryGetState(Id, State) && State.bCanGather)
					{
						bAnyGatherer = true;
						break;
					}
				}
			}
		}
		if (bAnyGatherer)
		{
			UE_LOG(LogTemp, Log, TEXT("RMB Gather LocalSelection=%d Node=%s Id=%d"),
				LocalSelection.Num(),
				*Cmd.GatherActor->GetName(),
				Cmd.GatherId.Value);
			const FVector MovePoint = Cmd.bHasMovePoint ? Cmd.MovePoint : Cmd.GatherActor->GetActorLocation();
			IssueGatherToSelection(Cmd.GatherActor, MovePoint);
			return;
		}
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

void AKodPlayerController::IssueFollowToSelection(AActor* Target, bool bGuard)
{
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Sim || !Target)
	{
		return;
	}
	const FKodEntityId TargetId = Sim->FindIdForActor(Target);
	const EKodSimOrderType Kind = bGuard ? EKodSimOrderType::Guard : EKodSimOrderType::Follow;
	for (const FKodEntityId& Id : GetLocalSelectedEntityIds())
	{
		if (Id != TargetId)
		{
			Sim->IssueOrder(Id, Kind, Target->GetActorLocation(), TargetId, false);
		}
	}
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
	if (Sources.Num() > 0)
	{
		NotifyCommandMarker(WorldLocation, false);
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
	if (!Sim->IsAttackable(TargetId, GetLocalTeamId()))
	{
		UE_LOG(LogTemp, Log, TEXT("Attack Reject Node Target=%s Id=%d"), *Target->GetName(), TargetId.Value);
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
	NotifyCommandMarker(Target->GetActorLocation(), true);
}

void AKodPlayerController::IssueGatherToSelection_Implementation(AActor* Node, FVector NonGathererMovePoint)
{
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Sim || !Node)
	{
		return;
	}
	const FKodEntityId NodeId = Sim->FindIdForActor(Node);
	if (!Sim->IsResourceNode(NodeId))
	{
		UE_LOG(LogTemp, Log, TEXT("Gather Reject NonNode Target=%s"), *Node->GetName());
		return;
	}

	int32 Gathered = 0;
	int32 Moved = 0;
	for (const FKodEntityId& Id : GetLocalSelectedEntityIds())
	{
		FKodSimEntityState State;
		if (!Sim->TryGetState(Id, State))
		{
			continue;
		}
		if (State.bCanGather)
		{
			if (Sim->IssueGather(Id, NodeId))
			{
				++Gathered;
			}
		}
		else
		{
			Sim->IssueMove(Id, NonGathererMovePoint);
			++Moved;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("Gather Issued Sources=%d Node=%d"), Gathered, NodeId.Value);
	if (Moved > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("Move Issued Sources=%d Dest=%.0f,%.0f,%.0f"),
			Moved,
			NonGathererMovePoint.X,
			NonGathererMovePoint.Y,
			NonGathererMovePoint.Z);
	}
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

void AKodPlayerController::GetMarqueeBounds(FVector2D& OutMin, FVector2D& OutMax) const
{
	OutMin.X = FMath::Min(MarqueeStart.X, MarqueeEnd.X);
	OutMin.Y = FMath::Min(MarqueeStart.Y, MarqueeEnd.Y);
	OutMax.X = FMath::Max(MarqueeStart.X, MarqueeEnd.X);
	OutMax.Y = FMath::Max(MarqueeStart.Y, MarqueeEnd.Y);
}

bool AKodPlayerController::GetSelectionMarqueeRect(FVector2D& OutMin, FVector2D& OutMax) const
{
	if (!bSelectPressed || !bMarqueeActive)
	{
		return false;
	}
	if (FVector2D::Distance(MarqueeStart, MarqueeEnd) < BoxSelectDragThresholdPx)
	{
		return false;
	}
	GetMarqueeBounds(OutMin, OutMax);
	return true;
}

bool AKodPlayerController::ProjectActorToMarqueeSpace(const AActor* Actor, FVector2D& OutScreen) const
{
	if (!Actor)
	{
		return false;
	}
	const FVector WorldPoint = Actor->GetActorLocation();
	if (ProjectWorldWithFallback(WorldPoint, OutScreen))
	{
		return true;
	}
	FVector Grounded = WorldPoint;
	Grounded.Z = GetPawn() ? GetPawn()->GetActorLocation().Z : 0.f;
	return ProjectWorldWithFallback(Grounded, OutScreen);
}

void AKodPlayerController::EndMarquee(bool bAddToSelection)
{
	bMarqueeActive = false;
	FVector2D BoxMin;
	FVector2D BoxMax;
	GetMarqueeBounds(BoxMin, BoxMax);

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

	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	FString HitLog;
	for (AActor* A : Hits)
	{
		if (!A)
		{
			continue;
		}
		if (!HitLog.IsEmpty())
		{
			HitLog += TEXT(",");
		}
		FVector2D Screen = FVector2D::ZeroVector;
		ProjectActorToMarqueeSpace(A, Screen);
		const FKodEntityId Id = Sim ? Sim->FindIdForActor(A) : FKodEntityId();
		HitLog += FString::Printf(
			TEXT("%s(id=%d,screen=%.0f,%.0f)"),
			*A->GetName(),
			Id.Value,
			Screen.X,
			Screen.Y);
	}

	UE_LOG(LogTemp, Log, TEXT("DragSelect LocalSelection=%d Additive=%d Box=%.0f,%.0f-%.0f,%.0f Hits=%s"),
		LocalSelection.Num(),
		bAddToSelection ? 1 : 0,
		BoxMin.X,
		BoxMin.Y,
		BoxMax.X,
		BoxMax.Y,
		HitLog.IsEmpty() ? TEXT("none") : *HitLog);

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
	if (!const_cast<AKodPlayerController*>(this)->DeprojectScreenPositionToWorld(MouseX, MouseY, Origin, Direction))
	{
		OutHitLog = TEXT("deproject_failed");
		return nullptr;
	}

	UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>();
	const int32 LocalTeam = GetLocalTeamId();
	// Same length GetHitResultUnderCursor uses. A channel trace stops on the first
	// BlockAll hit, so a PROXY mesh in front of a KodUnit used to win and get discarded.
	// Re-trace, ignoring actors with no sim id, until a registered entity or a miss.
	// Foreign-team sim actors are ignored the same way: RMB can still Attack them.
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

			if (IsSelectableSimId(Sim, Id, LocalTeam))
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

	const FChannelWalk SelectWalk = WalkChannel(KodCollision::Select, TEXT("Select"));
	if (SelectWalk.Picked)
	{
		return SelectWalk.Picked;
	}
	FVector GroundPoint = FVector::ZeroVector;
	if (IntersectGroundPlane(Origin, Direction, GroundPoint))
	{
		return FindOwnActorNear(GroundPoint, 220.f);
	}
	return nullptr;
}

bool AKodPlayerController::ViewPose(FVector& OutLocation, FRotator& OutRotation, float& OutFov, FIntPoint& OutSize) const
{
	OutLocation = FVector::ZeroVector;
	OutRotation = FRotator::ZeroRotator;
	OutFov = 90.f;
	OutSize = FIntPoint(1920, 1080);
	if (const AKodRTSCameraPawn* CameraPawn = Cast<AKodRTSCameraPawn>(GetPawn()))
	{
		if (CameraPawn->Camera)
		{
			OutLocation = CameraPawn->Camera->GetComponentLocation();
			OutRotation = CameraPawn->Camera->GetComponentRotation();
			OutFov = FMath::Max(1.f, CameraPawn->Camera->FieldOfView);
		}
	}
	else
	{
		GetPlayerViewPoint(OutLocation, OutRotation);
		if (PlayerCameraManager)
		{
			OutFov = FMath::Max(1.f, PlayerCameraManager->GetCameraCacheView().FOV);
		}
	}
	int32 SizeX = 0;
	int32 SizeY = 0;
	GetViewportSize(SizeX, SizeY);
	if (SizeX > 0 && SizeY > 0)
	{
		OutSize = FIntPoint(SizeX, SizeY);
	}
	return OutFov > 1.f;
}

bool AKodPlayerController::ProjectWorldWithFallback(const FVector& WorldPoint, FVector2D& OutScreen) const
{
	if (ProjectWorldLocationToScreen(WorldPoint, OutScreen, /*bPlayerViewportRelative*/ true))
	{
		return true;
	}
	FVector Location = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
	float Fov = 90.f;
	FIntPoint Size(1920, 1080);
	if (!ViewPose(Location, Rotation, Fov, Size))
	{
		return false;
	}
	const FVector Delta = WorldPoint - Location;
	const FVector Forward = Rotation.Vector();
	const float Depth = FVector::DotProduct(Delta, Forward);
	if (Depth <= 1.f)
	{
		return false;
	}
	const FVector Right = FRotationMatrix(Rotation).GetScaledAxis(EAxis::Y);
	const FVector Up = FRotationMatrix(Rotation).GetScaledAxis(EAxis::Z);
	const float Aspect = static_cast<float>(Size.X) / static_cast<float>(FMath::Max(1, Size.Y));
	const float HalfH = FMath::Tan(FMath::DegreesToRadians(Fov * 0.5f));
	const float HalfW = HalfH * Aspect;
	const float Nx = FVector::DotProduct(Delta, Right) / Depth;
	const float Ny = FVector::DotProduct(Delta, Up) / Depth;
	OutScreen.X = (0.5f + (0.5f * Nx / FMath::Max(0.01f, HalfW))) * static_cast<float>(Size.X);
	OutScreen.Y = (0.5f - (0.5f * Ny / FMath::Max(0.01f, HalfH))) * static_cast<float>(Size.Y);
	return true;
}

bool AKodPlayerController::DeprojectScreenWithFallback(const FVector2D& Screen, FVector& OutOrigin, FVector& OutDirection) const
{
	if (const_cast<AKodPlayerController*>(this)->DeprojectScreenPositionToWorld(Screen.X, Screen.Y, OutOrigin, OutDirection))
	{
		return OutDirection.Normalize();
	}
	FRotator Rotation = FRotator::ZeroRotator;
	float Fov = 90.f;
	FIntPoint Size(1920, 1080);
	if (!ViewPose(OutOrigin, Rotation, Fov, Size))
	{
		return false;
	}
	const float Aspect = static_cast<float>(Size.X) / static_cast<float>(FMath::Max(1, Size.Y));
	const float HalfH = FMath::Tan(FMath::DegreesToRadians(Fov * 0.5f));
	const float HalfW = HalfH * Aspect;
	const float Nx = ((Screen.X / static_cast<float>(Size.X)) - 0.5f) * 2.f * HalfW;
	const float Ny = (0.5f - (Screen.Y / static_cast<float>(Size.Y))) * 2.f * HalfH;
	const FVector Forward = Rotation.Vector();
	const FVector Right = FRotationMatrix(Rotation).GetScaledAxis(EAxis::Y);
	const FVector Up = FRotationMatrix(Rotation).GetScaledAxis(EAxis::Z);
	OutDirection = (Forward + Right * Nx + Up * Ny).GetSafeNormal();
	return !OutDirection.IsNearlyZero();
}

bool AKodPlayerController::IntersectGroundPlane(const FVector& Origin, const FVector& Direction, FVector& OutPoint) const
{
	UWorld* World = GetWorld();
	if (World && !Direction.IsNearlyZero())
	{
		FCollisionQueryParams Params(TEXT("KodGroundPlane"), false);
		if (AActor* Controlled = GetPawn())
		{
			Params.AddIgnoredActor(Controlled);
		}
		FHitResult Hit;
		const FVector End = Origin + Direction * 200000.f;
		if (World->LineTraceSingleByChannel(Hit, Origin, End, KodCollision::Ground, Params) && Hit.bBlockingHit)
		{
			OutPoint = Hit.ImpactPoint;
			return true;
		}
	}
	const float PlaneZ = GetPawn() ? GetPawn()->GetActorLocation().Z : 0.f;
	if (FMath::IsNearlyZero(Direction.Z))
	{
		return false;
	}
	const float T = (PlaneZ - Origin.Z) / Direction.Z;
	if (T <= 0.f)
	{
		return false;
	}
	OutPoint = Origin + Direction * T;
	return true;
}

AActor* AKodPlayerController::FindOwnActorNear(const FVector& WorldPoint, float RadiusUU) const
{
	UWorld* World = GetWorld();
	const UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Sim)
	{
		return nullptr;
	}
	const int32 LocalTeam = GetLocalTeamId();
	const float RadiusSq = RadiusUU * RadiusUU;
	AActor* BestUnit = nullptr;
	AActor* BestBuilding = nullptr;
	float BestUnitSq = RadiusSq;
	float BestBuildingSq = RadiusSq;
	TArray<FKodSimEntityState> States;
	TArray<AActor*> Actors;
	Sim->CopyEntitySnapshot(States, Actors);
	for (const FKodSimEntityState& State : States)
	{
		if (State.TeamId != LocalTeam || State.Health <= 0.f)
		{
			continue;
		}
		const FKodEntityId Id = State.Id;
		if (Sim->IsResourceNode(Id))
		{
			continue;
		}
		const float DistSq = FVector::DistSquared2D(State.Position, WorldPoint);
		AActor* Actor = Sim->ResolveEntity(Id);
		if (!Actor)
		{
			continue;
		}
		if (Sim->IsStructure(Id))
		{
			if (DistSq <= BestBuildingSq)
			{
				BestBuildingSq = DistSq;
				BestBuilding = Actor;
			}
			continue;
		}
		if (DistSq <= BestUnitSq)
		{
			BestUnitSq = DistSq;
			BestUnit = Actor;
		}
	}
	return BestUnit ? BestUnit : BestBuilding;
}

AActor* AKodPlayerController::SelectAlongRay(const FVector& Origin, const FVector& Direction, bool bAddToSelection, FString& OutHitLog)
{
	(void)bAddToSelection;
	UWorld* World = GetWorld();
	if (!World || Direction.IsNearlyZero())
	{
		return nullptr;
	}
	const FVector End = Origin + Direction * 200000.f;
	FCollisionQueryParams Params(TEXT("KodSelectRay"), false);
	if (AActor* Controlled = GetPawn())
	{
		Params.AddIgnoredActor(Controlled);
	}
	const UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>();
	const int32 LocalTeam = GetLocalTeamId();
	for (int32 Step = 0; Step < 32; ++Step)
	{
		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit, Origin, End, KodCollision::Select, Params) || !Hit.bBlockingHit)
		{
			break;
		}
		AActor* HitActor = Hit.GetActor();
		const FKodEntityId Id = (Sim && HitActor) ? Sim->FindIdForActor(HitActor) : FKodEntityId();
		if (!OutHitLog.IsEmpty())
		{
			OutHitLog += TEXT(",");
		}
		OutHitLog += FString::Printf(TEXT("%s(Select,sim=%d,id=%d)"),
			HitActor ? *HitActor->GetName() : TEXT("null"),
			Id.IsValid() ? 1 : 0,
			Id.Value);
		if (HitActor && IsSelectableSimId(Sim, Id, LocalTeam))
		{
			return HitActor;
		}
		if (!HitActor)
		{
			break;
		}
		Params.AddIgnoredActor(HitActor);
	}
	FVector GroundPoint = FVector::ZeroVector;
	if (IntersectGroundPlane(Origin, Direction, GroundPoint))
	{
		return FindOwnActorNear(GroundPoint, 220.f);
	}
	return nullptr;
}

AActor* AKodPlayerController::SelectAtScreen(const FVector2D& Screen, bool bAddToSelection)
{
	FString HitLog;
	FVector Origin = FVector::ZeroVector;
	FVector Direction = FVector::ZeroVector;
	AActor* Picked = nullptr;
	if (DeprojectScreenWithFallback(Screen, Origin, Direction))
	{
		Picked = SelectAlongRay(Origin, Direction, bAddToSelection, HitLog);
	}
	if (!bAddToSelection)
	{
		LocalSelection.Reset();
	}
	if (Picked)
	{
		LocalSelection.AddUnique(Picked);
		if (!bAddToSelection)
		{
			if (UWorld* VoiceWorld = GetWorld())
			{
				if (UKodSimSubsystem* VoiceSim = VoiceWorld->GetSubsystem<UKodSimSubsystem>())
				{
					if (UKodCueSubsystem* Cues = VoiceWorld->GetSubsystem<UKodCueSubsystem>())
					{
						const FKodEntityId VoiceId = VoiceSim->FindIdForActor(Picked);
						FKodSimEntityState VoiceState;
						if (VoiceSim->TryGetState(VoiceId, VoiceState))
						{
							Cues->PlayUnitVoice(VoiceState.DefinitionId, FName(TEXT("Select")), VoiceId.Value, Picked->GetActorLocation());
						}
					}
				}
			}
		}
	}
	ApplySelectionHighlight();
	UE_LOG(LogTemp, Log, TEXT("ClickSelectAtCursor LocalSelection=%d Picked=%s Hits=%s"),
		LocalSelection.Num(),
		Picked ? *Picked->GetName() : TEXT("None"),
		HitLog.IsEmpty() ? TEXT("none") : *HitLog);
	return Picked;
}

void AKodPlayerController::ClickSelectAtCursor(bool bAddToSelection)
{
	float MouseX = 0.f;
	float MouseY = 0.f;
	if (!GetMousePosition(MouseX, MouseY))
	{
		UE_LOG(LogTemp, Log, TEXT("ClickSelectAtCursor LocalSelection=%d Picked=None Hits=none"), LocalSelection.Num());
		return;
	}
	AActor* Picked = SelectAtScreen(FVector2D(MouseX, MouseY), bAddToSelection);
	if (Picked)
	{
		if (UWorld* World = GetWorld())
		{
			if (const UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>())
			{
				FKodResourceNodeState Node;
				if (Sim->TryGetResourceNode(Sim->FindIdForActor(Picked), Node))
				{
					UE_LOG(LogTemp, Log, TEXT("Select Node Id=%d Type=%s Remaining=%d"),
						Node.Id.Value,
						KodResourceTypeName(Node.Type),
						Node.Remaining);
				}
			}
		}
	}
}

namespace KodSelectionHighlight
{
	constexpr int32 StencilValue = 1;
	constexpr const TCHAR* OverlayMaterialPath = TEXT("/Game/Warden/FX/Selection/M_SelectionRim.M_SelectionRim");

	bool IsHighlightMesh(const UMeshComponent* Mesh)
	{
		if (!Mesh || !Mesh->IsVisible() || Mesh->bHiddenInGame)
		{
			return false;
		}
		// Muzzle flash and the shot tracer are presentation meshes on the unit.
		if (Mesh->ComponentHasTag(FName(TEXT("KodFx"))))
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

	/** Overlay plus a render-state rebuild so the proxy's used-material list includes the rim. */
	void PushSelectionOverlay(UMeshComponent* Mesh, UMaterialInterface* Parent)
	{
		if (!Mesh)
		{
			return;
		}
		UMaterialInterface* Applied = nullptr;
		if (Parent)
		{
			Applied = UMaterialInstanceDynamic::Create(Parent, Mesh);
			if (!Applied)
			{
				Applied = Parent;
			}
		}
		Mesh->SetOverlayMaterial(Applied);
		Mesh->MarkRenderStateDirty();
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
		KodSelectionHighlight::PushSelectionOverlay(Mesh, nullptr);
		// Slot restore is unused after the rim overlay. An empty OriginalMaterials list is a no-op.
		const int32 Slots = Mesh->GetNumMaterials();
		for (int32 Index = 0; Index < State.OriginalMaterials.Num() && Index < Slots; ++Index)
		{
			Mesh->SetMaterial(Index, State.OriginalMaterials[Index]);
			Mesh->MarkRenderStateDirty();
		}
		Mesh->SetRenderCustomDepth(State.bHadCustomDepth);
		Mesh->SetCustomDepthStencilValue(State.OriginalStencil);
	}
	SelectionHighlights.Reset();
}

UMaterialInterface* AKodPlayerController::GetSelectionOverlayMaterial()
{
	if (bSelectionOverlayResolved)
	{
		return SelectionOverlayMaterial;
	}
	bSelectionOverlayResolved = true;

	const FSoftObjectPath Path(KodSelectionHighlight::OverlayMaterialPath);
	SelectionOverlayMaterial = Cast<UMaterialInterface>(Path.TryLoad());
	if (!SelectionOverlayMaterial)
	{
		UE_LOG(LogTemp, Warning, TEXT("SelectionHighlight overlay failed to load %s; using stencil only"),
			*Path.ToString());
	}
	return SelectionOverlayMaterial;
}

void AKodPlayerController::ApplySelectionHighlight()
{
	ClearSelectionHighlight();

	UMaterialInterface* Overlay = GetSelectionOverlayMaterial();
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

			if (Overlay)
			{
				KodSelectionHighlight::PushSelectionOverlay(Mesh, Overlay);
			}

			Mesh->SetRenderCustomDepth(true);
			Mesh->SetCustomDepthStencilValue(KodSelectionHighlight::StencilValue);
			SelectionHighlights.Add(State);
			++MeshCount;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("SelectionHighlight LocalSelection=%d Meshes=%d Overlay=%d Stencil=%d"),
		LocalSelection.Num(),
		MeshCount,
		Overlay ? 1 : 0,
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

	FVector2D Min;
	FVector2D Max;
	GetMarqueeBounds(Min, Max);
	const AActor* ControlledPawn = GetPawn();
	const int32 LocalTeam = GetLocalTeamId();
	TArray<AActor*> Units;
	TArray<AActor*> Buildings;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || Actor == ControlledPawn)
		{
			continue;
		}
		const FKodEntityId Id = Sim->FindIdForActor(Actor);
		if (!IsSelectableSimId(Sim, Id, LocalTeam) || Sim->IsResourceNode(Id))
		{
			continue;
		}
		FVector2D Screen;
		if (!ProjectActorToMarqueeSpace(Actor, Screen))
		{
			continue;
		}
		if (Screen.X < Min.X || Screen.X > Max.X || Screen.Y < Min.Y || Screen.Y > Max.Y)
		{
			continue;
		}
		if (Sim->IsStructure(Id))
		{
			Buildings.Add(Actor);
		}
		else
		{
			Units.Add(Actor);
		}
	}
	// Own units, else own buildings. Resource nodes stay click-only.
	OutActors = Units.Num() > 0 ? Units : Buildings;
}

int32 AKodPlayerController::GetControlGroupCount(int32 GroupIndex) const
{
	(void)GroupIndex;
	return 0;
}

TArray<FKodEntityId> AKodPlayerController::GetControlGroupIds(int32 GroupIndex) const
{
	(void)GroupIndex;
	return TArray<FKodEntityId>();
}

void AKodPlayerController::AssignControlGroup(int32 GroupIndex)
{
	(void)GroupIndex;
}

void AKodPlayerController::AddToControlGroup(int32 GroupIndex)
{
	(void)GroupIndex;
}

void AKodPlayerController::SetLocalSelectionFromIds(const TArray<FKodEntityId>& Ids, bool bAdd)
{
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!bAdd)
	{
		LocalSelection.Reset();
	}
	if (Sim)
	{
		const int32 LocalTeam = GetLocalTeamId();
		for (const FKodEntityId& Id : Ids)
		{
			AActor* Actor = Sim->ResolveEntity(Id);
			if (Actor && IsSelectableSimId(Sim, Id, LocalTeam))
			{
				LocalSelection.AddUnique(Actor);
			}
		}
	}
	ApplySelectionHighlight();
	if (!bAdd && LocalSelection.Num() >= 1)
	{
		if (AActor* VoiceActor = LocalSelection[0].Get())
		{
			if (UWorld* VoiceWorld = GetWorld())
			{
				if (UKodSimSubsystem* VoiceSim = VoiceWorld->GetSubsystem<UKodSimSubsystem>())
				{
					if (UKodCueSubsystem* Cues = VoiceWorld->GetSubsystem<UKodCueSubsystem>())
					{
						const FKodEntityId VoiceId = VoiceSim->FindIdForActor(VoiceActor);
						FKodSimEntityState VoiceState;
						if (VoiceSim->TryGetState(VoiceId, VoiceState))
						{
							Cues->PlayUnitVoice(VoiceState.DefinitionId, FName(TEXT("Select")), VoiceId.Value, VoiceActor->GetActorLocation());
						}
					}
				}
			}
		}
	}
	if (!bAdd && LocalSelection.Num() == 1)
	{
		AActor* Only = LocalSelection[0].Get();
		bool bCommandCenter = Only && Only->ActorHasTag(FName(TEXT("KodBuildingCommandCenter")));
		if (!bCommandCenter && Only && Sim)
		{
			FKodStructureState Structure;
			const FKodEntityId Id = Sim->FindIdForActor(Only);
			if (Sim->TryGetStructure(Id, Structure) && Structure.DefinitionId == FName(TEXT("CommandCenter")))
			{
				bCommandCenter = true;
			}
		}
		if (bCommandCenter)
		{
			if (AKodRTSCameraPawn* CameraPawn = Cast<AKodRTSCameraPawn>(GetPawn()))
			{
				CameraPawn->SnapToPlayerStart();
			}
		}
	}
}

void AKodPlayerController::SelectPlayerCommandCenter()
{
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	AKodRTSCameraPawn* CameraPawn = Cast<AKodRTSCameraPawn>(GetPawn());
	if (!Sim || !CameraPawn)
	{
		return;
	}
	AActor* Anchor = CameraPawn->FindCommandCenter();
	if (!Anchor)
	{
		return;
	}
	const FKodEntityId Id = Sim->FindIdForActor(Anchor);
	if (!Id.IsValid())
	{
		return;
	}
	TArray<FKodEntityId> Ids;
	Ids.Add(Id);
	SetLocalSelectionFromIds(Ids, false);
}

void AKodPlayerController::SelectIdleWorker()
{
	UWorld* World = GetWorld();
	const UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Sim)
	{
		return;
	}
	TArray<FKodSimEntityState> States;
	TArray<AActor*> Actors;
	Sim->CopyEntitySnapshot(States, Actors);
	const int32 LocalTeam = GetLocalTeamId();
	TArray<int32> IdleIds;
	for (const FKodSimEntityState& State : States)
	{
		if (State.TeamId == LocalTeam
			&& State.bCanGather
			&& State.bMobile
			&& State.Health > 0.f
			&& State.Order == EKodSimOrderType::None
			&& State.GatherPhase == EKodGatherPhase::None
			&& !State.BuildSite.IsValid())
		{
			IdleIds.Add(State.Id.Value);
		}
	}
	if (IdleIds.Num() == 0)
	{
		return;
	}
	IdleIds.Sort();
	int32 Pick = IdleIds[0];
	for (int32 IdValue : IdleIds)
	{
		if (IdValue > LastIdleWorkerId)
		{
			Pick = IdValue;
			break;
		}
	}
	LastIdleWorkerId = Pick;
	FKodEntityId Id;
	Id.Value = Pick;
	TArray<FKodEntityId> Only;
	Only.Add(Id);
	SetLocalSelectionFromIds(Only, false);
}

void AKodPlayerController::IssueMinimapGroundOrder(FVector WorldLocation)
{
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Sim)
	{
		return;
	}
	bool bIssued = false;
	bool bAttack = false;
	for (const FKodEntityId& Id : GetLocalSelectedEntityIds())
	{
		FKodSimEntityState State;
		if (!Sim->TryGetState(Id, State) || !State.bMobile || State.Health <= 0.f)
		{
			continue;
		}
		if (State.WeaponDamage > 0.f)
		{
			Sim->IssueAttackMove(Id, WorldLocation);
			bAttack = true;
		}
		else
		{
			Sim->IssueMove(Id, WorldLocation);
		}
		bIssued = true;
	}
	if (bIssued)
	{
		NotifyCommandMarker(WorldLocation, bAttack);
	}
}

void AKodPlayerController::NotifyCommandMarker(const FVector& WorldLocation, bool bAttack)
{
	if (AKodHUD* HUD = Cast<AKodHUD>(GetHUD()))
	{
		HUD->AddCommandMarker(WorldLocation, bAttack ? 1 : 0);
	}
	if (UWorld* World = GetWorld())
	{
		if (UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>())
		{
			FName VoiceUnit;
			int32 VoiceEntity = 0;
			FVector VoiceAt = WorldLocation;
			for (const FKodEntityId& VoiceId : GetLocalSelectedEntityIds())
			{
				FKodSimEntityState VoiceState;
				if (Sim->TryGetState(VoiceId, VoiceState) && VoiceState.bMobile)
				{
					VoiceUnit = VoiceState.DefinitionId;
					VoiceEntity = VoiceId.Value;
					VoiceAt = VoiceState.Position;
					break;
				}
			}
			const FName VoiceCategory = bAttack ? FName(TEXT("Attack")) : FName(TEXT("Ack"));
			bool bVoiced = false;
			if (!VoiceUnit.IsNone())
			{
				if (UKodCueSubsystem* Cues = World->GetSubsystem<UKodCueSubsystem>())
				{
					bVoiced = Cues->PlayUnitVoice(VoiceUnit, VoiceCategory, VoiceEntity, VoiceAt);
				}
			}
			if (!bVoiced)
			{
				const FName CueId = bAttack ? FName(TEXT("command_attack")) : FName(TEXT("command_move"));
				Sim->RaiseCue(CueId, WorldLocation, true);
			}
		}
	}
}

void AKodPlayerController::SyncCommandSubgroup()
{
	TArray<int32> Ids;
	for (const FKodEntityId& Id : GetLocalSelectedEntityIds())
	{
		if (Id.IsValid())
		{
			Ids.Add(Id.Value);
		}
	}
	Ids.Sort();
	if (Ids != CommandSelectionIds)
	{
		CommandSelectionIds = Ids;
		CommandSubgroupIndex = 0;
	}
}

void AKodPlayerController::JumpCameraToMinimap(bool bClampToMap)
{
	float MouseX = 0.f;
	float MouseY = 0.f;
	if (!GetMousePosition(MouseX, MouseY))
	{
		return;
	}
	const AKodHUD* HUD = Cast<AKodHUD>(GetHUD());
	AKodRTSCameraPawn* CameraPawn = Cast<AKodRTSCameraPawn>(GetPawn());
	if (!HUD || !CameraPawn)
	{
		return;
	}
	FVector WorldPoint;
	if (HUD->MinimapPixelToWorld(MouseX, MouseY, WorldPoint, bClampToMap))
	{
		CameraPawn->JumpTo(WorldPoint);
	}
}

bool AKodPlayerController::ConsumeMinimapClick()
{
	float MouseX = 0.f;
	float MouseY = 0.f;
	if (!GetMousePosition(MouseX, MouseY))
	{
		return false;
	}
	const AKodHUD* HUD = Cast<AKodHUD>(GetHUD());
	if (!HUD)
	{
		return false;
	}
	FVector WorldPoint;
	if (!HUD->MinimapPixelToWorld(MouseX, MouseY, WorldPoint, false))
	{
		return false;
	}
	if (IsInputKeyDown(EKeys::LeftAlt) || IsInputKeyDown(EKeys::RightAlt))
	{
		const_cast<AKodHUD*>(HUD)->AddMinimapPing(WorldPoint);
		return true;
	}
	bMinimapDrag = true;
	if (AKodRTSCameraPawn* CameraPawn = Cast<AKodRTSCameraPawn>(GetPawn()))
	{
		CameraPawn->JumpTo(WorldPoint);
	}
	return true;
}

bool AKodPlayerController::ConsumeChromeClick()
{
	float MouseX = 0.f;
	float MouseY = 0.f;
	if (!GetMousePosition(MouseX, MouseY))
	{
		return false;
	}
	const AKodHUD* HUD = Cast<AKodHUD>(GetHUD());
	if (!HUD)
	{
		return false;
	}
	FKodHudChromeHit Hit;
	if (!HUD->GetChromeHit(MouseX, MouseY, Hit))
	{
		return false;
	}
	const bool bShift = IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift);
	const bool bCtrl = IsInputKeyDown(EKeys::LeftControl) || IsInputKeyDown(EKeys::RightControl);
	if (Hit.Kind == FName(TEXT("Faction")))
	{
		if (AKodHUD* MutableHUD = Cast<AKodHUD>(GetHUD()))
		{
			MutableHUD->SetHudSkin(Hit.IntParam);
		}
		return true;
	}
	if (Hit.Kind == FName(TEXT("MenuBtn")))
	{
		if (AKodHUD* MutableHUD = Cast<AKodHUD>(GetHUD()))
		{
			MutableHUD->TogglePauseMenu();
		}
		return true;
	}
	if (Hit.Kind == FName(TEXT("ChatBtn")))
	{
		if (AKodHUD* MutableHUD = Cast<AKodHUD>(GetHUD()))
		{
			MutableHUD->OpenChat();
		}
		return true;
	}
	if (Hit.Kind == FName(TEXT("Menu")))
	{
		if (AKodHUD* MutableHUD = Cast<AKodHUD>(GetHUD()))
		{
			MutableHUD->HandleMenuChoice(Hit.IntParam);
		}
		return true;
	}
	if (Hit.Kind == FName(TEXT("Scrub")))
	{
		if (AKodHUD* MutableHUD = Cast<AKodHUD>(GetHUD()))
		{
			MutableHUD->ScrubScoreGraph(MouseX);
		}
		return true;
	}
	if (Hit.Kind == FName(TEXT("Page")))
	{
		if (AKodHUD* MutableHUD = Cast<AKodHUD>(GetHUD()))
		{
			MutableHUD->SetSelectionPage(Hit.IntParam);
		}
		return true;
	}
	if (Hit.Kind == FName(TEXT("Portrait")))
	{
		return true;
	}
	if (Hit.Kind == FName(TEXT("Unload")) || Hit.Kind == FName(TEXT("UnloadAll")))
	{
		if (UWorld* World = GetWorld())
		{
			if (UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>())
			{
				if (Hit.Kind == FName(TEXT("UnloadAll")))
				{
					FKodEntityId BunkerId;
					BunkerId.Value = Hit.IntParam;
					Sim->UnloadAll(BunkerId);
				}
				else
				{
					TArray<FKodStructureState> Structures;
					Sim->CopyStructures(Structures);
					for (const FKodStructureState& Structure : Structures)
					{
						if (Structure.Cargo.Contains(Hit.IntParam))
						{
							Sim->UnloadUnit(Structure.Id, Hit.IntParam);
							break;
						}
					}
				}
			}
		}
		return true;
	}
	if (Hit.Kind == FName(TEXT("Layer")))
	{
		if (AKodHUD* MutableHUD = Cast<AKodHUD>(GetHUD()))
		{
			MutableHUD->ToggleMinimapLayer(Hit.IntParam);
		}
		return true;
	}
	if (Hit.Kind == FName(TEXT("Idle")))
	{
		SelectIdleWorker();
		return true;
	}
	if (Hit.Kind == FName(TEXT("Group")))
	{
		SetLocalSelectionFromIds(GetControlGroupIds(Hit.IntParam), bShift);
		return true;
	}
	if (Hit.Kind == FName(TEXT("Tile")))
	{
		if (bCtrl)
		{
			UWorld* World = GetWorld();
			const UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
			TArray<FKodEntityId> SameType;
			if (Sim)
			{
				for (const FKodEntityId& Id : GetLocalSelectedEntityIds())
				{
					FKodStructureState Structure;
					FKodSimEntityState State;
					FName TypeId;
					if (Sim->TryGetStructure(Id, Structure))
					{
						TypeId = Structure.DefinitionId;
					}
					else if (Sim->TryGetState(Id, State))
					{
						TypeId = State.DefinitionId;
					}
					if (TypeId == Hit.NameParam)
					{
						SameType.Add(Id);
					}
				}
			}
			SetLocalSelectionFromIds(SameType, false);
		}
		else if (bShift)
		{
			UWorld* World = GetWorld();
			UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
			FKodEntityId Id;
			Id.Value = Hit.IntParam;
			if (Sim)
			{
				RemoveFromLocalSelection(Sim->ResolveEntity(Id));
			}
		}
		else
		{
			FKodEntityId Id;
			Id.Value = Hit.IntParam;
			TArray<FKodEntityId> Only;
			Only.Add(Id);
			SetLocalSelectionFromIds(Only, false);
		}
		return true;
	}
	return true;
}
