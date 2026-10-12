#include "Game/KodHUD.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "Game/KodPlayerController.h"
#include "Game/KodPlayerState.h"
#include "Game/KodRTSCameraPawn.h"
#include "Sim/KodBuildTypes.h"
#include "Sim/KodResourceTypes.h"
#include "Sim/KodSimSubsystem.h"
#include "Warden/KodWardenPaths.h"

namespace
{
	FName PanelFacingName(const UKodSimSubsystem* Sim, FName EngineId)
	{
		FName Label = EngineId;
		FKodTrainSpec Spec;
		if (Sim && Sim->TryGetTrainSpec(EngineId, Spec) && !Spec.DisplayId.IsNone())
		{
			Label = Spec.DisplayId;
		}
		if (Label == FName(KodWardenPaths::Id_Dozer) || EngineId == FName(KodWardenPaths::Id_Dozer))
		{
			return FName(TEXT("AMP"));
		}
		if (Label == FName(KodWardenPaths::Id_Ranger) || EngineId == FName(KodWardenPaths::Id_Ranger))
		{
			return FName(TEXT("Marine"));
		}
		return Label.IsNone() ? FName(TEXT("Unit")) : Label;
	}

	FLinearColor TeamInk(int32 TeamId, int32 LocalTeam)
	{
		if (TeamId == LocalTeam)
		{
			return FLinearColor(0.3f, 0.9f, 0.38f, 1.f);
		}
		return FLinearColor(0.92f, 0.22f, 0.16f, 1.f);
	}

	UFont* HudFont()
	{
		if (!GEngine)
		{
			return nullptr;
		}
		if (UFont* Font = GEngine->GetMediumFont())
		{
			return Font;
		}
		return GEngine->GetSmallFont();
	}

	FString OrderLabel(const FKodSimEntityState& State)
	{
		switch (State.Order)
		{
		case EKodSimOrderType::Move:
			return TEXT("Move");
		case EKodSimOrderType::Attack:
			return TEXT("Attack");
		case EKodSimOrderType::Gather:
			return TEXT("Gather");
		case EKodSimOrderType::AttackMove:
			return TEXT("Attack-move");
		default:
			return TEXT("Idle");
		}
	}

	bool PointIn(float X, float Y, float MinX, float MinY, float MaxX, float MaxY)
	{
		return X >= MinX && X <= MaxX && Y >= MinY && Y <= MaxY;
	}
}

bool AKodHUD::IsMinimapWorldRevealed(float WorldX, float WorldY) const
{
	(void)WorldX;
	(void)WorldY;
	return true;
}

bool AKodHUD::DrawUnitPortrait(FName DisplayName, float X, float Y, float Width, float Height)
{
	(void)DisplayName;
	(void)X;
	(void)Y;
	(void)Width;
	(void)Height;
	return false;
}

void AKodHUD::RememberChrome(FName Kind, int32 IntParam, FName NameParam, float MinX, float MinY, float MaxX, float MaxY)
{
	FKodHudChromeHit Hit;
	Hit.Kind = Kind;
	Hit.IntParam = IntParam;
	Hit.NameParam = NameParam;
	Hit.MinX = MinX;
	Hit.MinY = MinY;
	Hit.MaxX = MaxX;
	Hit.MaxY = MaxY;
	ChromeHits.Add(Hit);
}

bool AKodHUD::WorldToMinimap(float WorldX, float WorldY, float& OutX, float& OutY) const
{
	if (!bHasMinimap)
	{
		return false;
	}
	const float SpanX = FMath::Max(1.f, MapWorldMaxX - MapWorldMinX);
	const float SpanY = FMath::Max(1.f, MapWorldMaxY - MapWorldMinY);
	const float U = FMath::Clamp((WorldX - MapWorldMinX) / SpanX, 0.f, 1.f);
	const float V = FMath::Clamp((WorldY - MapWorldMinY) / SpanY, 0.f, 1.f);
	OutX = MapMinX + U * (MapMaxX - MapMinX);
	OutY = MapMaxY - V * (MapMaxY - MapMinY);
	return true;
}

bool AKodHUD::MinimapPixelToWorld(float MouseX, float MouseY, FVector& OutWorld, bool bClampToMap) const
{
	if (!bHasMinimap)
	{
		return false;
	}
	if (!bClampToMap && !PointIn(MouseX, MouseY, MapMinX, MapMinY, MapMaxX, MapMaxY))
	{
		return false;
	}
	const float ClampedX = FMath::Clamp(MouseX, MapMinX, MapMaxX);
	const float ClampedY = FMath::Clamp(MouseY, MapMinY, MapMaxY);
	const float Width = FMath::Max(1.f, MapMaxX - MapMinX);
	const float Height = FMath::Max(1.f, MapMaxY - MapMinY);
	const float U = (ClampedX - MapMinX) / Width;
	const float V = 1.f - ((ClampedY - MapMinY) / Height);
	OutWorld.X = MapWorldMinX + U * (MapWorldMaxX - MapWorldMinX);
	OutWorld.Y = MapWorldMinY + V * (MapWorldMaxY - MapWorldMinY);
	OutWorld.Z = MapWorldZ;
	return true;
}

bool AKodHUD::GetChromeHit(float MouseX, float MouseY, FKodHudChromeHit& Out) const
{
	for (int32 Index = ChromeHits.Num() - 1; Index >= 0; --Index)
	{
		const FKodHudChromeHit& Hit = ChromeHits[Index];
		if (PointIn(MouseX, MouseY, Hit.MinX, Hit.MinY, Hit.MaxX, Hit.MaxY))
		{
			Out = Hit;
			return true;
		}
	}
	return false;
}

bool AKodHUD::IsPassiveHudChrome(float MouseX, float MouseY) const
{
	FKodHudChromeHit Hit;
	return GetChromeHit(MouseX, MouseY, Hit);
}

void AKodHUD::DrawMinimap()
{
	ChromeHits.Reset();
	bHasMinimap = false;
	bHasPassiveChrome = false;
	if (!Canvas)
	{
		return;
	}
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Sim)
	{
		return;
	}

	const float Scale = FMath::Max(0.5f, Canvas->SizeY / 1080.f);
	const float MapSize = 220.f * Scale;
	const float Margin = 16.f * Scale;
	MapMinX = Margin;
	MapMaxX = Margin + MapSize;
	MapMaxY = Canvas->SizeY - Margin;
	MapMinY = MapMaxY - MapSize;

	int32 CellMinX = 0;
	int32 CellMinY = 0;
	int32 CellMaxX = 0;
	int32 CellMaxY = 0;
	bool bHave = Sim->TryGetPlayableCellBounds(CellMinX, CellMinY, CellMaxX, CellMaxY);
	if (bHave)
	{
		MapWorldMinX = static_cast<float>(CellMinX * KodBuildGrid::CellUU);
		MapWorldMinY = static_cast<float>(CellMinY * KodBuildGrid::CellUU);
		MapWorldMaxX = static_cast<float>((CellMaxX + 1) * KodBuildGrid::CellUU);
		MapWorldMaxY = static_cast<float>((CellMaxY + 1) * KodBuildGrid::CellUU);
	}
	else
	{
		const APawn* Pawn = GetOwningPawn();
		const FVector Origin = Pawn ? Pawn->GetActorLocation() : FVector::ZeroVector;
		MapWorldMinX = Origin.X - 4000.f;
		MapWorldMinY = Origin.Y - 4000.f;
		MapWorldMaxX = Origin.X + 4000.f;
		MapWorldMaxY = Origin.Y + 4000.f;
	}
	MapWorldZ = GetOwningPawn() ? GetOwningPawn()->GetActorLocation().Z : 0.f;

	auto Expand = [this](float X, float Y)
	{
		MapWorldMinX = FMath::Min(MapWorldMinX, X);
		MapWorldMinY = FMath::Min(MapWorldMinY, Y);
		MapWorldMaxX = FMath::Max(MapWorldMaxX, X);
		MapWorldMaxY = FMath::Max(MapWorldMaxY, Y);
	};

	TArray<FKodResourceNodeState> Nodes;
	Sim->CopyResourceNodes(Nodes);
	TArray<FKodSimEntityState> States;
	TArray<AActor*> Actors;
	Sim->CopyEntitySnapshot(States, Actors);
	TArray<FKodStructureState> Structures;
	Sim->CopyStructures(Structures);
	for (const FKodResourceNodeState& Node : Nodes)
	{
		Expand(Node.Position.X, Node.Position.Y);
	}
	for (const FKodSimEntityState& State : States)
	{
		Expand(State.Position.X, State.Position.Y);
	}

	const float Pad = 200.f;
	MapWorldMinX -= Pad;
	MapWorldMinY -= Pad;
	MapWorldMaxX += Pad;
	MapWorldMaxY += Pad;
	bHasMinimap = true;

	DrawRect(FLinearColor(0.02f, 0.025f, 0.03f, 0.94f), MapMinX, MapMinY, MapSize, MapSize);
	if (bHave)
	{
		float Left = 0.f;
		float Top = 0.f;
		float Right = 0.f;
		float Bottom = 0.f;
		const float PlayMinX = static_cast<float>(CellMinX * KodBuildGrid::CellUU);
		const float PlayMinY = static_cast<float>(CellMinY * KodBuildGrid::CellUU);
		const float PlayMaxX = static_cast<float>((CellMaxX + 1) * KodBuildGrid::CellUU);
		const float PlayMaxY = static_cast<float>((CellMaxY + 1) * KodBuildGrid::CellUU);
		WorldToMinimap(PlayMinX, PlayMaxY, Left, Top);
		WorldToMinimap(PlayMaxX, PlayMinY, Right, Bottom);
		DrawRect(FLinearColor(0.08f, 0.09f, 0.07f, 1.f), Left, Top, Right - Left, Bottom - Top);
		DrawLine(Left, Top, Right, Top, FLinearColor(0.55f, 0.55f, 0.5f, 1.f), 1.f);
		DrawLine(Right, Top, Right, Bottom, FLinearColor(0.55f, 0.55f, 0.5f, 1.f), 1.f);
		DrawLine(Right, Bottom, Left, Bottom, FLinearColor(0.55f, 0.55f, 0.5f, 1.f), 1.f);
		DrawLine(Left, Bottom, Left, Top, FLinearColor(0.55f, 0.55f, 0.5f, 1.f), 1.f);
	}

	const int32 LocalTeam = ReadLocalTeamId();
	for (const FKodResourceNodeState& Node : Nodes)
	{
		if (!IsMinimapWorldRevealed(Node.Position.X, Node.Position.Y))
		{
			continue;
		}
		float X = 0.f;
		float Y = 0.f;
		if (!WorldToMinimap(Node.Position.X, Node.Position.Y, X, Y))
		{
			continue;
		}
		const FLinearColor Ink = Node.Type == EKodResourceType::Luminene
			? KodResourceColors::LumineneCore()
			: KodResourceColors::Jadeite();
		const float Dot = 5.f * Scale;
		DrawRect(Ink, X - Dot * 0.5f, Y - Dot * 0.5f, Dot, Dot);
	}

	for (const FKodStructureState& Structure : Structures)
	{
		if (!IsMinimapWorldRevealed(Structure.Position.X, Structure.Position.Y))
		{
			continue;
		}
		float X = 0.f;
		float Y = 0.f;
		if (!WorldToMinimap(Structure.Position.X, Structure.Position.Y, X, Y))
		{
			continue;
		}
		const float SpanX = FMath::Max(1.f, MapWorldMaxX - MapWorldMinX);
		const float Px = (MapMaxX - MapMinX) / SpanX;
		const float Width = FMath::Max(4.f * Scale, static_cast<float>(FMath::Max(1, Structure.FootprintX) * KodBuildGrid::CellUU) * Px);
		const float Height = FMath::Max(4.f * Scale, static_cast<float>(FMath::Max(1, Structure.FootprintY) * KodBuildGrid::CellUU) * Px);
		DrawRect(TeamInk(Structure.TeamId, LocalTeam), X - Width * 0.5f, Y - Height * 0.5f, Width, Height);
	}

	for (const FKodSimEntityState& State : States)
	{
		if (!State.bMobile || !IsMinimapWorldRevealed(State.Position.X, State.Position.Y))
		{
			continue;
		}
		float X = 0.f;
		float Y = 0.f;
		if (!WorldToMinimap(State.Position.X, State.Position.Y, X, Y))
		{
			continue;
		}
		const float Dot = 4.f * Scale;
		DrawRect(TeamInk(State.TeamId, LocalTeam), X - Dot * 0.5f, Y - Dot * 0.5f, Dot, Dot);
	}

	if (const AKodRTSCameraPawn* CameraPawn = Cast<AKodRTSCameraPawn>(GetOwningPawn()))
	{
		FVector NearLeft;
		FVector NearRight;
		FVector FarRight;
		FVector FarLeft;
		if (CameraPawn->GetGroundViewCorners(NearLeft, NearRight, FarRight, FarLeft))
		{
			float X0 = 0.f;
			float Y0 = 0.f;
			float X1 = 0.f;
			float Y1 = 0.f;
			float X2 = 0.f;
			float Y2 = 0.f;
			float X3 = 0.f;
			float Y3 = 0.f;
			WorldToMinimap(NearLeft.X, NearLeft.Y, X0, Y0);
			WorldToMinimap(NearRight.X, NearRight.Y, X1, Y1);
			WorldToMinimap(FarRight.X, FarRight.Y, X2, Y2);
			WorldToMinimap(FarLeft.X, FarLeft.Y, X3, Y3);
			const FLinearColor White(1.f, 1.f, 1.f, 0.95f);
			DrawLine(X0, Y0, X1, Y1, White, 1.5f);
			DrawLine(X1, Y1, X2, Y2, White, 1.5f);
			DrawLine(X2, Y2, X3, Y3, White, 1.5f);
			DrawLine(X3, Y3, X0, Y0, White, 1.5f);
		}
		FVector Alert;
		if (CameraPawn->GetActiveAlert(Alert) && IsMinimapWorldRevealed(Alert.X, Alert.Y))
		{
			float X = 0.f;
			float Y = 0.f;
			if (WorldToMinimap(Alert.X, Alert.Y, X, Y))
			{
				const float Pulse = 0.45f + 0.55f * FMath::Abs(FMath::Sin(World->GetTimeSeconds() * 6.f));
				const FLinearColor Ping(1.f, 0.85f, 0.2f, Pulse);
				const float R = 7.f * Scale;
				DrawLine(X - R, Y, X + R, Y, Ping, 2.f);
				DrawLine(X, Y - R, X, Y + R, Ping, 2.f);
			}
		}
	}
}

void AKodHUD::DrawIdleWorkerButton()
{
	if (!Canvas || !bHasMinimap)
	{
		return;
	}
	UWorld* World = GetWorld();
	const UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Sim)
	{
		return;
	}
	const float Scale = FMath::Max(0.5f, Canvas->SizeY / 1080.f);
	const int32 LocalTeam = ReadLocalTeamId();
	int32 Idle = 0;
	TArray<FKodSimEntityState> States;
	TArray<AActor*> Actors;
	Sim->CopyEntitySnapshot(States, Actors);
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
			++Idle;
		}
	}

	const float Gap = 6.f * Scale;
	const float Height = 28.f * Scale;
	const float X = MapMinX;
	const float Y = MapMinY - Gap - Height;
	const float Width = MapMaxX - MapMinX;
	DrawRect(FLinearColor(0.04f, 0.06f, 0.04f, 0.94f), X, Y, Width, Height);
	DrawRect(FLinearColor(0.2f, 0.55f, 0.25f, 1.f), X, Y, 4.f * Scale, Height);
	UFont* Font = HudFont();
	DrawText(FString::Printf(TEXT("Idle %d"), Idle), FLinearColor::White, X + 10.f * Scale, Y + 4.f * Scale, Font, Scale, false);
	RememberChrome(FName(TEXT("Idle")), Idle, FName(), X, Y, X + Width, Y + Height);
}

void AKodHUD::DrawControlGroupTabs()
{
	if (!Canvas)
	{
		return;
	}
	const AKodPlayerController* PC = Cast<AKodPlayerController>(GetOwningPlayerController());
	if (!PC)
	{
		return;
	}
	const float Scale = FMath::Max(0.5f, Canvas->SizeY / 1080.f);
	const float PanelW = 420.f * Scale;
	const float PanelH = 168.f * Scale;
	const float Bottom = 18.f * Scale;
	const float PanelX = (Canvas->SizeX - PanelW) * 0.5f;
	const float PanelY = Canvas->SizeY - Bottom - PanelH;
	const float TabH = 22.f * Scale;
	const float TabGap = 3.f * Scale;
	const float TabY = PanelY - 6.f * Scale - TabH;
	const float TabW = (PanelW - TabGap * 9.f) / 10.f;
	const int32 Order[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 0};
	UFont* Font = HudFont();
	for (int32 Visual = 0; Visual < 10; ++Visual)
	{
		const int32 Index = Order[Visual];
		const int32 Count = PC->GetControlGroupCount(Index);
		const float X = PanelX + Visual * (TabW + TabGap);
		const FLinearColor Plate = Count > 0
			? FLinearColor(0.1f, 0.14f, 0.1f, 0.95f)
			: FLinearColor(0.06f, 0.06f, 0.06f, 0.8f);
		DrawRect(Plate, X, TabY, TabW, TabH);
		const FString Label = FString::Printf(TEXT("%d %d"), Index == 0 ? 0 : Index, Count);
		DrawText(Label, Count > 0 ? FLinearColor::White : FLinearColor(0.45f, 0.45f, 0.45f, 1.f), X + 2.f, TabY + 2.f, Font, 0.7f * Scale, false);
		RememberChrome(FName(TEXT("Group")), Index, FName(), X, TabY, X + TabW, TabY + TabH);
	}
}

void AKodHUD::DrawPortrait()
{
	if (!Canvas)
	{
		return;
	}
	const AKodPlayerController* PC = Cast<AKodPlayerController>(GetOwningPlayerController());
	UWorld* World = GetWorld();
	const UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!PC || !Sim || PC->GetLocalSelection().Num() == 0)
	{
		return;
	}
	const float Scale = FMath::Max(0.5f, Canvas->SizeY / 1080.f);
	const float PanelW = 420.f * Scale;
	const float PanelH = 168.f * Scale;
	const float Bottom = 18.f * Scale;
	const float PanelX = (Canvas->SizeX - PanelW) * 0.5f;
	const float PanelY = Canvas->SizeY - Bottom - PanelH;
	const float Gap = 8.f * Scale;
	const float Width = 132.f * Scale;
	const float X = PanelX - Gap - Width;
	const float Y = PanelY;

	FName Shown = ActiveCommandSubgroup;
	if (PC->GetLocalSelection().Num() == 1)
	{
		if (AActor* Only = PC->GetLocalSelection()[0])
		{
			const FKodEntityId Id = Sim->FindIdForActor(Only);
			FKodStructureState Structure;
			FKodSimEntityState State;
			if (Sim->TryGetStructure(Id, Structure))
			{
				Shown = Structure.DisplayId.IsNone() ? Structure.DefinitionId : Structure.DisplayId;
			}
			else if (Sim->TryGetState(Id, State))
			{
				Shown = State.DefinitionId;
			}
		}
	}
	const FString Name = PanelFacingName(Sim, Shown).ToString();
	const FString Letter = Name.IsEmpty() ? TEXT("?") : Name.Left(1);

	DrawRect(FLinearColor(0.03f, 0.04f, 0.05f, 0.94f), X, Y, Width, PanelH);
	DrawRect(FLinearColor(0.45f, 0.38f, 0.22f, 1.f), X, Y, Width, 3.f * Scale);
	if (!DrawUnitPortrait(FName(*Name), X, Y, Width, PanelH))
	{
		UFont* Font = GEngine ? GEngine->GetLargeFont() : HudFont();
		DrawText(Letter, FLinearColor(0.85f, 0.78f, 0.62f, 1.f), X + Width * 0.28f, Y + PanelH * 0.22f, Font, 3.f * Scale, false);
		DrawText(Name, FLinearColor::White, X + 8.f * Scale, Y + PanelH - 28.f * Scale, HudFont(), 0.8f * Scale, false);
	}
	RememberChrome(FName(TEXT("Portrait")), 0, FName(*Name), X, Y, X + Width, Y + PanelH);
}

void AKodHUD::DrawSelectionPanel()
{
	if (!Canvas)
	{
		return;
	}
	const AKodPlayerController* PC = Cast<AKodPlayerController>(GetOwningPlayerController());
	UWorld* World = GetWorld();
	const UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!PC || !Sim)
	{
		return;
	}

	struct FTile
	{
		int32 EntityId = 0;
		FName TypeId;
		FString TypeName;
		FString Name;
		float Health = 0.f;
		float MaxHealth = 1.f;
		int32 TeamId = 0;
		FString Order;
		FString Stim;
		bool bStructure = false;
	};

	TArray<FTile> Tiles;
	for (AActor* Actor : PC->GetLocalSelection())
	{
		if (!Actor)
		{
			continue;
		}
		const FKodEntityId Id = Sim->FindIdForActor(Actor);
		if (!Id.IsValid())
		{
			continue;
		}
		FTile Tile;
		Tile.EntityId = Id.Value;
		FKodStructureState Structure;
		FKodSimEntityState State;
		const bool bStructure = Sim->TryGetStructure(Id, Structure);
		const bool bState = Sim->TryGetState(Id, State);
		if (!bStructure && !bState)
		{
			continue;
		}
		Tile.bStructure = bStructure;
		Tile.TeamId = bState ? State.TeamId : Structure.TeamId;
		Tile.TypeId = bStructure ? Structure.DefinitionId : State.DefinitionId;
		Tile.Name = PanelFacingName(Sim, bStructure
			? (Structure.DisplayId.IsNone() ? Structure.DefinitionId : Structure.DisplayId)
			: State.DefinitionId).ToString();
		Tile.TypeName = Tile.Name;
		if (bState)
		{
			Tile.Health = State.Health;
			Tile.MaxHealth = FMath::Max(1.f, State.MaxHealth);
			Tile.Order = OrderLabel(State);
			if (State.AbilityTicksRemaining > 0)
			{
				Tile.Stim = FString::Printf(TEXT("Stim %ds"), FMath::Max(1, State.AbilityTicksRemaining / 16));
			}
			else if (Sim->UnitHasAbility(State.DefinitionId, FName(KodWardenPaths::Id_CombatStim)))
			{
				Tile.Stim = TEXT("Stim off");
			}
		}
		else
		{
			Tile.Health = Structure.MaxHealth;
			Tile.MaxHealth = FMath::Max(1.f, Structure.MaxHealth);
		}
		if (bStructure)
		{
			if (!Structure.bComplete && Structure.BuildTimeTotal > 0)
			{
				const int32 Percent = (Structure.BuildTicksDone * 100) / Structure.BuildTimeTotal;
				Tile.Order = FString::Printf(TEXT("Build %d%%"), Percent);
			}
			else if (Structure.Slots.Num() > 0)
			{
				FString Queue;
				for (const FKodProductionSlot& Slot : Structure.Slots)
				{
					if (!Queue.IsEmpty())
					{
						Queue += TEXT(", ");
					}
					Queue += PanelFacingName(Sim, Slot.DisplayId.IsNone() ? Slot.UnitId : Slot.DisplayId).ToString();
				}
				Tile.Order = Queue;
			}
			else if (Tile.Order.IsEmpty())
			{
				Tile.Order = TEXT("Idle");
			}
		}
		Tiles.Add(Tile);
	}
	if (Tiles.Num() == 0)
	{
		return;
	}

	Tiles.Sort([](const FTile& A, const FTile& B)
	{
		if (A.TypeName == B.TypeName)
		{
			return A.EntityId < B.EntityId;
		}
		return A.TypeName < B.TypeName;
	});

	const float Scale = FMath::Max(0.5f, Canvas->SizeY / 1080.f);
	const float PanelW = 420.f * Scale;
	const float PanelH = 168.f * Scale;
	const float Bottom = 18.f * Scale;
	const float X = (Canvas->SizeX - PanelW) * 0.5f;
	const float Y = Canvas->SizeY - Bottom - PanelH;
	const int32 LocalTeam = ReadLocalTeamId();
	UFont* Font = HudFont();
	DrawRect(FLinearColor(0.03f, 0.035f, 0.04f, 0.94f), X, Y, PanelW, PanelH);
	RememberChrome(FName(TEXT("Panel")), 0, FName(), X, Y, X + PanelW, Y + PanelH);

	if (Tiles.Num() == 1)
	{
		const FTile& Tile = Tiles[0];
		DrawText(Tile.Name, FLinearColor::White, X + 14.f * Scale, Y + 12.f * Scale, Font, 1.1f * Scale, false);
		const float BarX = X + 14.f * Scale;
		const float BarY = Y + 42.f * Scale;
		const float BarW = PanelW - 28.f * Scale;
		const float Fraction = FMath::Clamp(Tile.Health / FMath::Max(1.f, Tile.MaxHealth), 0.f, 1.f);
		DrawRect(FLinearColor(0.12f, 0.12f, 0.12f, 1.f), BarX, BarY, BarW, 12.f * Scale);
		DrawRect(TeamInk(Tile.TeamId, LocalTeam), BarX, BarY, BarW * Fraction, 12.f * Scale);
		DrawText(FString::Printf(TEXT("%.0f / %.0f"), Tile.Health, Tile.MaxHealth), FLinearColor::White, BarX, BarY + 16.f * Scale, Font, 0.85f * Scale, false);
		float TextY = BarY + 40.f * Scale;
		if (!Tile.Stim.IsEmpty())
		{
			DrawText(Tile.Stim, FLinearColor(1.f, 0.55f, 0.2f, 1.f), BarX, TextY, Font, 0.9f * Scale, false);
			TextY += 22.f * Scale;
		}
		DrawText(Tile.Order, FLinearColor(0.85f, 0.85f, 0.8f, 1.f), BarX, TextY, Font, 0.9f * Scale, false);
		RememberChrome(FName(TEXT("Tile")), Tile.EntityId, Tile.TypeId, X, Y, X + PanelW, Y + PanelH);
		return;
	}

	const int32 Shown = FMath::Min(Tiles.Num(), 24);
	constexpr int32 Cols = 8;
	const float TileW = (PanelW - 16.f * Scale) / Cols;
	const float TileH = (PanelH - 12.f * Scale) / 3.f;
	for (int32 Index = 0; Index < Shown; ++Index)
	{
		const FTile& Tile = Tiles[Index];
		const int32 Col = Index % Cols;
		const int32 Row = Index / Cols;
		const float TileX = X + 8.f * Scale + Col * TileW;
		const float TileY = Y + 6.f * Scale + Row * TileH;
		const float Box = TileW - 4.f * Scale;
		const float BoxH = TileH - 8.f * Scale;
		const FLinearColor Ink = TeamInk(Tile.TeamId, LocalTeam);
		const float Edge = FMath::Max(1.f, 1.5f * Scale);
		DrawRect(Ink, TileX, TileY, Box, Edge);
		DrawRect(Ink, TileX, TileY + BoxH - Edge, Box, Edge);
		DrawRect(Ink, TileX, TileY, Edge, BoxH);
		DrawRect(Ink, TileX + Box - Edge, TileY, Edge, BoxH);
		const float Fraction = FMath::Clamp(Tile.Health / FMath::Max(1.f, Tile.MaxHealth), 0.f, 1.f);
		DrawRect(FLinearColor(0.1f, 0.1f, 0.1f, 1.f), TileX, TileY + BoxH - 4.f * Scale, Box, 3.f * Scale);
		DrawRect(Ink, TileX, TileY + BoxH - 4.f * Scale, Box * Fraction, 3.f * Scale);
		RememberChrome(FName(TEXT("Tile")), Tile.EntityId, Tile.TypeId, TileX, TileY, TileX + Box, TileY + BoxH);
	}
}
