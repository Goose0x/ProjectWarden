#include "Game/KodHUD.h"

#include "Game/KodCameraSettings.h"
#include "Game/KodHudLayout.h"
#include "Game/KodWireTint.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "Game/KodPlayerController.h"
#include "Game/KodPlayerState.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Game/KodPortraitStage.h"
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
		return Label.IsNone() ? FName(TEXT("Unit")) : Label;
	}

	FLinearColor TeamInk(int32 TeamId, int32 LocalTeam)
	{
		if (TeamId == LocalTeam)
		{
			return FLinearColor(FColor(0x59, 0x78, 0x8A));
		}
		return FLinearColor(FColor(0x8C, 0x5A, 0x5A));
	}

	FLinearColor PanelHealthInk(float Fraction, int32 HealthyPermille, int32 WarningPermille)
	{
		return KodWireTint::FromRemainPermille(Fraction, HealthyPermille, WarningPermille, false);
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

	FLinearColor Tok(uint8 R, uint8 G, uint8 B, float A = 1.f)
	{
		return FLinearColor(FColor(R, G, B, static_cast<uint8>(FMath::Clamp(A, 0.f, 1.f) * 255.f)));
	}
}

bool AKodHUD::IsMinimapWorldRevealed(float WorldX, float WorldY) const
{
	if (!bMapFog)
	{
		return true;
	}
	const UWorld* World = GetWorld();
	const UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Sim)
	{
		return true;
	}
	return Sim->GetTeamVision(ReadLocalTeamId(), WorldX, WorldY) != 0;
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

	const FKodHudV3 Layout = FKodHudV3::ForFaction(HudSkin == 1);
	const FKodScreenRect MapRect = MapBox(Layout.Minimap);
	const float Scale = FrameScale;
	const float MapSize = MapRect.W;
	MapMinX = MapRect.X;
	MapMinY = MapRect.Y;
	MapMaxX = MapRect.X + MapRect.W;
	MapMaxY = MapRect.Y + MapRect.H;

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
	const TArray<FKodSimEntityState>& States = FrameStates;
	const TArray<FKodStructureState>& Structures = FrameStructures;
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

	DrawPlate(MapMinX, MapMinY, MapSize, MapSize, Tok(0x1F, 0x23, 0x27, 0.94f), TEXT("T_UI_VG_Bay_Minimap"), 24.f, 24.f, 24.f, 24.f);
	if (bHave && bMapTerrain)
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
	if (bMapFog)
	{
		const int32 Cell = KodBuildGrid::CellUU;
		const int32 FogX0 = KodBuildGrid::CellOf(MapWorldMinX);
		const int32 FogX1 = KodBuildGrid::CellOf(MapWorldMaxX);
		const int32 FogY0 = KodBuildGrid::CellOf(MapWorldMinY);
		const int32 FogY1 = KodBuildGrid::CellOf(MapWorldMaxY);
		for (int32 CellY = FogY0; CellY <= FogY1; ++CellY)
		{
			for (int32 CellX = FogX0; CellX <= FogX1; ++CellX)
			{
				const uint8 Vision = Sim->GetTeamVision(
					LocalTeam,
					KodBuildGrid::CellCenter(CellX),
					KodBuildGrid::CellCenter(CellY));
				if (Vision == 2)
				{
					continue;
				}
				float Left = 0.f;
				float Top = 0.f;
				float Right = 0.f;
				float Bottom = 0.f;
				WorldToMinimap(static_cast<float>(CellX * Cell), static_cast<float>((CellY + 1) * Cell), Left, Top);
				WorldToMinimap(static_cast<float>((CellX + 1) * Cell), static_cast<float>(CellY * Cell), Right, Bottom);
				const float Alpha = Vision == 0 ? 0.72f : 0.38f;
				DrawRect(
					FLinearColor(0.01f, 0.012f, 0.02f, Alpha),
					Left,
					Top,
					FMath::Max(1.f, Right - Left),
					FMath::Max(1.f, Bottom - Top));
			}
		}
	}

	for (const FKodResourceNodeState& Node : Nodes)
	{
		if (!bMapResources)
		{
			break;
		}
		if (bMapFog && Sim->GetTeamVision(LocalTeam, Node.Position.X, Node.Position.Y) == 0)
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
			? Tok(0xFF, 0xD2, 0x3F)
			: Tok(0xB0, 0x8D, 0x57);
		const float Dot = 5.f * Scale;
		DrawRect(Ink, X - Dot * 0.5f, Y - Dot * 0.5f, Dot, Dot);
	}

	for (const FKodStructureState& Structure : Structures)
	{
		if (!bMapUnits)
		{
			break;
		}
		if (bMapOwnOnly && Structure.TeamId != LocalTeam)
		{
			continue;
		}
		const uint8 Vision = Sim->GetTeamVision(LocalTeam, Structure.Position.X, Structure.Position.Y);
		if (bMapFog && Structure.TeamId != LocalTeam && Vision != 2)
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

	if (bMapUnits && bMapFog && !bMapOwnOnly)
	{
		TArray<int32> SeenTeam;
		TArray<int32> SeenX;
		TArray<int32> SeenY;
		TArray<int32> SeenFootX;
		TArray<int32> SeenFootY;
		Sim->CopySeenStructureBlips(LocalTeam, SeenTeam, SeenX, SeenY, SeenFootX, SeenFootY);
		for (int32 Index = 0; Index < SeenX.Num(); ++Index)
		{
			if (Sim->GetTeamVision(LocalTeam, static_cast<float>(SeenX[Index]), static_cast<float>(SeenY[Index])) == 2)
			{
				continue;
			}
			float X = 0.f;
			float Y = 0.f;
			if (!WorldToMinimap(static_cast<float>(SeenX[Index]), static_cast<float>(SeenY[Index]), X, Y))
			{
				continue;
			}
			const float SpanX = FMath::Max(1.f, MapWorldMaxX - MapWorldMinX);
			const float Px = (MapMaxX - MapMinX) / SpanX;
			const float Width = FMath::Max(4.f * Scale, static_cast<float>(FMath::Max(1, SeenFootX[Index]) * KodBuildGrid::CellUU) * Px);
			const float Height = FMath::Max(4.f * Scale, static_cast<float>(FMath::Max(1, SeenFootY[Index]) * KodBuildGrid::CellUU) * Px);
			FLinearColor Ink = TeamInk(SeenTeam[Index], LocalTeam);
			Ink.A = 0.45f;
			DrawRect(Ink, X - Width * 0.5f, Y - Height * 0.5f, Width, Height);
		}
	}

	const FName EnemyCcId(KodWardenPaths::Id_CommandCenter);
	for (const FKodStructureState& StartBase : Structures)
	{
		if (StartBase.TeamId == LocalTeam || StartBase.DefinitionId != EnemyCcId)
		{
			continue;
		}
		const uint8 StartVision = Sim->GetTeamVision(LocalTeam, StartBase.Position.X, StartBase.Position.Y);
		if (!bMapFog || StartVision == 2)
		{
			continue;
		}
		float MarkerX = 0.f;
		float MarkerY = 0.f;
		if (!WorldToMinimap(StartBase.Position.X, StartBase.Position.Y, MarkerX, MarkerY))
		{
			continue;
		}
		const FLinearColor MarkerInk(FColor(0xE5, 0x38, 0x3B));
		const float MarkerR = 6.f * Scale;
		constexpr int32 MarkerSteps = 12;
		FVector2D MarkerPrev(MarkerX + MarkerR, MarkerY);
		for (int32 MarkerStep = 1; MarkerStep <= MarkerSteps; ++MarkerStep)
		{
			const float MarkerAngle = (static_cast<float>(MarkerStep) / static_cast<float>(MarkerSteps)) * 2.f * PI;
			const FVector2D MarkerNext(MarkerX + FMath::Cos(MarkerAngle) * MarkerR, MarkerY + FMath::Sin(MarkerAngle) * MarkerR);
			DrawLine(MarkerPrev.X, MarkerPrev.Y, MarkerNext.X, MarkerNext.Y, MarkerInk, 1.5f);
			MarkerPrev = MarkerNext;
		}
		const float MarkerDot = 3.f * Scale;
		DrawRect(MarkerInk, MarkerX - MarkerDot * 0.5f, MarkerY - MarkerDot * 0.5f, MarkerDot, MarkerDot);
	}

	for (const FKodSimEntityState& State : States)
	{
		if (!bMapUnits || !State.bMobile)
		{
			continue;
		}
		if (bMapOwnOnly && State.TeamId != LocalTeam)
		{
			continue;
		}
		if (bMapFog && State.TeamId != LocalTeam && Sim->GetTeamVision(LocalTeam, State.Position.X, State.Position.Y) != 2)
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
			const KodCameraSettings::FKodMapRect View = KodCameraSettings::ClampRect(
				KodCameraSettings::AxisAlignedBounds(X0, Y0, X1, Y1, X2, Y2, X3, Y3),
				MapMinX,
				MapMinY,
				MapMaxX,
				MapMaxY);
			if (View.bValid)
			{
				const FLinearColor White(1.f, 1.f, 1.f, 0.95f);
				DrawLine(View.MinX, View.MinY, View.MaxX, View.MinY, White, 1.5f);
				DrawLine(View.MaxX, View.MinY, View.MaxX, View.MaxY, White, 1.5f);
				DrawLine(View.MaxX, View.MaxY, View.MinX, View.MaxY, White, 1.5f);
				DrawLine(View.MinX, View.MaxY, View.MinX, View.MinY, White, 1.5f);
			}
		}
		FVector Alert;
		if (bMapAlerts && CameraPawn->GetActiveAlert(Alert) && IsMinimapWorldRevealed(Alert.X, Alert.Y))
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

	if (bMapAlerts)
	{
		const float Now = World->GetTimeSeconds();
		for (int32 Index = MinimapPings.Num() - 1; Index >= 0; --Index)
		{
			if (Index >= MinimapPingUntil.Num() || MinimapPingUntil[Index] < Now)
			{
				MinimapPings.RemoveAt(Index);
				if (Index < MinimapPingUntil.Num())
				{
					MinimapPingUntil.RemoveAt(Index);
				}
				continue;
			}
			float X = 0.f;
			float Y = 0.f;
			if (!WorldToMinimap(MinimapPings[Index].X, MinimapPings[Index].Y, X, Y))
			{
				continue;
			}
			const float Life = FMath::Clamp((MinimapPingUntil[Index] - Now) / 6.f, 0.f, 1.f);
			const float Grow = 1.f - Life;
			const float R = (4.f + 16.f * Grow) * Scale;
			const float Flash = 0.35f + 0.65f * FMath::Abs(FMath::Sin(Now * 8.f));
			const FLinearColor Ping = Tok(0xFF, 0xD2, 0x3F, Flash);
			const int32 Steps = 10;
			FVector2D Prev(X + R, Y);
			for (int32 Step = 1; Step <= Steps; ++Step)
			{
				const float Angle = (static_cast<float>(Step) / static_cast<float>(Steps)) * 2.f * PI;
				const FVector2D Next(X + FMath::Cos(Angle) * R, Y + FMath::Sin(Angle) * R);
				DrawLine(Prev.X, Prev.Y, Next.X, Next.Y, Ping, 1.5f);
				Prev = Next;
			}
		}
	}

	const TCHAR* ChipLabels[] = { TEXT("Ping"), TEXT("Ter"), TEXT("Ally") };
	const int32 ChipLayers[] = { 4, 0, 5 };
	const bool ChipOn[] = { bMapAlerts, bMapTerrain, bMapOwnOnly };
	const FKodHudBox ChipBoxes[] = { Layout.MapButton[0], Layout.MapButton[1], Layout.MapButton[2] };
	UFont* LayerFont = HudFont();
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const FKodScreenRect Chip = MapBox(ChipBoxes[Index]);
		DrawPlate(Chip.X, Chip.Y, Chip.W, Chip.H, ChipOn[Index] ? Tok(0x5C, 0x64, 0x6C) : Tok(0x3A, 0x3F, 0x45), TEXT("T_UI_VG_Btn_Chip"), 8.f, 8.f, 8.f, 8.f);
		DrawText(ChipLabels[Index], Tok(0xE6, 0xE6, 0xE0), Chip.X + 4.f * Scale, Chip.Y + 12.f * Scale, LayerFont, 0.65f * Scale, false);
		RememberChrome(FName(TEXT("Layer")), ChipLayers[Index], FName(), Chip.X, Chip.Y, Chip.X + Chip.W, Chip.Y + Chip.H);
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
	const float Scale = FrameScale;
	const int32 LocalTeam = ReadLocalTeamId();
	int32 Idle = 0;
	for (const FKodSimEntityState& State : FrameStates)
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

	const FKodScreenRect Slot = MapBox(FKodHudV3::ForFaction(HudSkin == 1).MinimapTab[0]);
	if (Idle <= 0)
	{
		return;
	}
	DrawPlate(Slot.X, Slot.Y, Slot.W, Slot.H, Tok(0x5C, 0x64, 0x6C), TEXT("T_UI_VG_Btn_Chip"), 8.f, 8.f, 8.f, 8.f);
	UFont* Font = HudFont();
	DrawText(FString::Printf(TEXT("%d"), Idle), Tok(0xE6, 0xE6, 0xE0), Slot.X + 8.f * Scale, Slot.Y + 8.f * Scale, Font, Scale, false);
	RememberChrome(FName(TEXT("Idle")), Idle, FName(), Slot.X, Slot.Y, Slot.X + Slot.W, Slot.Y + Slot.H);
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
	const float Scale = FrameScale;
	const int32 Order[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 0};
	const FKodHudV3 Layout = FKodHudV3::ForFaction(HudSkin == 1);
	UFont* Font = HudFont();
	for (int32 Visual = 0; Visual < FKodHudV3::TabCount(); ++Visual)
	{
		const int32 Index = Order[Visual];
		const int32 Count = PC->GetControlGroupCount(Index);
		const FKodScreenRect Tab = MapBox(Layout.Tab[Visual]);
		const float X = Tab.X;
		const float TabY = Tab.Y;
		const float TabW = Tab.W;
		const float TabH = Tab.H;
		if (UTexture2D* TabTex = ResolveSoftTexture(*FKodHudV3::TabPath(HudSkin == 1, Visual, Count > 0)))
		{
			DrawTexture(TabTex, X, TabY, TabW, TabH, 0.f, 0.f, 1.f, 1.f);
		}
		else
		{
			const FLinearColor Plate = Count > 0
				? Tok(0x5C, 0x64, 0x6C, 0.95f)
				: Tok(0x3A, 0x3F, 0x45, 0.8f);
			DrawRect(Plate, X, TabY, TabW, TabH);
			const FString Label = FString::Printf(TEXT("%d"), Index == 0 ? 0 : Index);
			DrawText(Label, Count > 0 ? FLinearColor::White : FLinearColor(0.45f, 0.45f, 0.45f, 1.f), X + 2.f, TabY + 2.f, Font, 0.7f * Scale, false);
		}
		RememberChrome(FName(TEXT("Group")), Index, FName(), X, TabY, X + TabW, TabY + TabH);
	}
}

void AKodHUD::DrawPortrait()
{
	bHasPortrait = false;
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
	const float Scale = FrameScale;
	const FKodHudBox PortraitBox = FKodHudV3::ForFaction(HudSkin == 1).Portrait;
	const FKodScreenRect Art = MapSpec(PortraitBox.X, PortraitBox.Y, PortraitBox.W, FMath::Max(16.f, PortraitBox.H - 32.f));
	const float Width = Art.W;
	const float PanelH = Art.H;
	const float X = Art.X;
	const float Y = Art.Y;

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
	AActor* PortraitActor = PC->GetLocalSelection().Num() > 0 ? PC->GetLocalSelection()[0] : nullptr;
	if (PortraitStage && PortraitActor)
	{
		int32 TeamId = 0;
		const FKodEntityId PortraitId = Sim->FindIdForActor(PortraitActor);
		if (const FKodSimEntityState* PortraitState = FindFrameState(PortraitId.Value))
		{
			TeamId = PortraitState->TeamId;
		}
		if (PortraitSubject.Get() != PortraitActor)
		{
			PortraitStage->ShowSubject(PortraitActor, TeamId);
			PortraitSubject = PortraitActor;
			PortraitCaptureAccum = 1.f;
		}
		PortraitCaptureAccum += World ? World->GetDeltaSeconds() : 0.f;
		if (PortraitCaptureAccum >= 1.f / 12.f)
		{
			PortraitCaptureAccum = 0.f;
			PortraitStage->CapturePortrait();
		}
		if (UTextureRenderTarget2D* Target = PortraitStage->GetRenderTarget())
		{
			DrawTexture(Target, X + 4.f * Scale, Y + 8.f * Scale, Width - 8.f * Scale, PanelH - 36.f * Scale, 0.f, 0.f, 1.f, 1.f);
		}
	}
	if (!DrawUnitPortrait(FName(*Name), X, Y, Width, PanelH))
	{
		UFont* Font = GEngine ? GEngine->GetLargeFont() : HudFont();
		DrawText(Letter, FLinearColor(0.85f, 0.78f, 0.62f, 1.f), X + Width * 0.28f, Y + PanelH * 0.22f, Font, 3.f * Scale, false);
		DrawText(Name, FLinearColor::White, X + 8.f * Scale, Y + PanelH - 28.f * Scale, HudFont(), 0.8f * Scale, false);
	}
	RememberChrome(FName(TEXT("Portrait")), 0, FName(*Name), X, Y, X + Width, Y + PanelH);
	int32 ShownHP = 1;
	int32 MaxHP = 1;
	int32 Energy = 0;
	int32 EnergyMax = 0;
	if (PortraitActor)
	{
		const int32 BarId = Sim->FindIdForActor(PortraitActor).Value;
		if (const FKodSimEntityState* BarState = FindFrameState(BarId))
		{
			ShownHP = FMath::Max(0, FMath::FloorToInt(BarState->Health));
			MaxHP = FMath::Max(1, FMath::FloorToInt(BarState->MaxHealth));
			Energy = BarState->Energy;
			EnergyMax = BarState->EnergyMax;
		}
	}
	const float HpFraction = FMath::Clamp(static_cast<float>(ShownHP) / static_cast<float>(MaxHP), 0.f, 1.f);
	const UKodHudProposed& Style = HudStyle();
	const FKodScreenRect HP = MapSpec(PortraitBox.X, PortraitBox.Y + PortraitBox.H - 30.f, PortraitBox.W, 14.f);
	const FKodScreenRect EN = MapSpec(PortraitBox.X, PortraitBox.Y + PortraitBox.H - 14.f, PortraitBox.W, 12.f);
	DrawRect(Tok(0x1F, 0x23, 0x27), HP.X, HP.Y, HP.W, HP.H);
	DrawRect(PanelHealthInk(HpFraction, Style.WireHealthyPermille, Style.WireWarningPermille), HP.X, HP.Y, HP.W * HpFraction, HP.H);
	DrawText(FString::Printf(TEXT("%d/%d"), ShownHP, MaxHP), Tok(0xE6, 0xE6, 0xE0), HP.X + 4.f * Scale, HP.Y, HudFont(), 0.65f * Scale, false);
	DrawRect(Tok(0x1F, 0x23, 0x27), EN.X, EN.Y, EN.W, EN.H);
	if (EnergyMax > 0)
	{
		const float EnergyFrac = FMath::Clamp(static_cast<float>(Energy) / static_cast<float>(EnergyMax), 0.f, 1.f);
		const FLinearColor EnergyInk = HudSkin == 1 ? Tok(0x3D, 0xFF, 0xB0) : Tok(0x2F, 0x7B, 0xFF);
		DrawRect(EnergyInk, EN.X, EN.Y, EN.W * EnergyFrac, EN.H);
	}
	bHasPortrait = true;
	PortraitMinX = X;
	PortraitMinY = Y;
	PortraitMaxX = X + Width;
	PortraitMaxY = EN.Y + EN.H;
}

void AKodHUD::DrawSelectionPanel()
{
	bHasSelectionPanel = false;
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
		int32 Energy = 0;
		int32 EnergyMax = 0;
		int32 Shield = 0;
		int32 ShieldMax = 0;
		TArray<int32> Cargo;
	};

	TArray<int32> Wanted;
	TSet<int32> SelectedIds;
	for (AActor* Actor : PC->GetLocalSelection())
	{
		if (!Actor)
		{
			continue;
		}
		const int32 IdValue = Sim->FindIdForActor(Actor).Value;
		if (IdValue > 0)
		{
			SelectedIds.Add(IdValue);
			Wanted.AddUnique(IdValue);
		}
	}
	for (int32 Group = 0; Group < 10; ++Group)
	{
		const TArray<FKodEntityId> Members = PC->GetControlGroupIds(Group);
		bool bOverlap = false;
		for (const FKodEntityId& Member : Members)
		{
			if (SelectedIds.Contains(Member.Value))
			{
				bOverlap = true;
				break;
			}
		}
		if (!bOverlap)
		{
			continue;
		}
		for (const FKodEntityId& Member : Members)
		{
			Wanted.AddUnique(Member.Value);
		}
	}
	Wanted.Sort();

	TArray<FTile> Tiles;
	for (int32 IdValue : Wanted)
	{
		FKodEntityId Id;
		Id.Value = IdValue;
		if (!Id.IsValid())
		{
			continue;
		}
		FTile Tile;
		Tile.EntityId = Id.Value;
		const FKodStructureState* Structure = FindFrameStructure(Id.Value);
		const FKodSimEntityState* State = FindFrameState(Id.Value);
		const bool bStructure = Structure != nullptr;
		const bool bState = State != nullptr;
		if (!bStructure && !bState)
		{
			continue;
		}
		Tile.bStructure = bStructure;
		Tile.TeamId = bState ? State->TeamId : Structure->TeamId;
		Tile.TypeId = bStructure ? Structure->DefinitionId : State->DefinitionId;
		Tile.Name = PanelFacingName(Sim, bStructure
			? (Structure->DisplayId.IsNone() ? Structure->DefinitionId : Structure->DisplayId)
			: State->DefinitionId).ToString();
		Tile.TypeName = Tile.Name;
		if (bState)
		{
			Tile.Health = State->Health;
			Tile.MaxHealth = FMath::Max(1.f, State->MaxHealth);
			Tile.Order = OrderLabel(*State);
			if (State->AbilityTicksRemaining > 0)
			{
				Tile.Stim = FString::Printf(TEXT("Stim %ds"), FMath::Max(1, State->AbilityTicksRemaining / 16));
			}
			else if (Sim->UnitHasAbility(State->DefinitionId, FName(KodWardenPaths::Id_CombatStim)))
			{
				Tile.Stim = TEXT("Stim off");
			}
			Tile.Energy = State->Energy;
			Tile.EnergyMax = State->EnergyMax;
			Tile.Shield = State->Shield;
			Tile.ShieldMax = State->ShieldMax;
		}
		else
		{
			Tile.Health = Structure->MaxHealth;
			Tile.MaxHealth = FMath::Max(1.f, Structure->MaxHealth);
		}
		if (bStructure)
		{
			if (!Structure->bComplete && Structure->BuildTimeTotal > 0)
			{
				const int32 Percent = (Structure->BuildTicksDone * 100) / Structure->BuildTimeTotal;
				Tile.Order = FString::Printf(TEXT("Build %d%%"), Percent);
			}
			else if (Structure->Slots.Num() > 0)
			{
				FString Queue;
				for (const FKodProductionSlot& Slot : Structure->Slots)
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

	int32 PrimaryId = 0;
	if (PC->GetLocalSelection().Num() > 0)
	{
		if (AActor* FirstSelected = PC->GetLocalSelection()[0])
		{
			PrimaryId = Sim->FindIdForActor(FirstSelected).Value;
		}
	}

	Tiles.Sort([](const FTile& A, const FTile& B)
	{
		if (A.TypeName == B.TypeName)
		{
			return A.EntityId < B.EntityId;
		}
		return A.TypeName < B.TypeName;
	});

	const float Scale = FrameScale;
	WireMidCursor = 0;
	const FKodScreenRect Grid = MapInfo(KodHudLayout::GroupGridX, KodHudLayout::GroupGridY, KodHudLayout::GroupGridW, KodHudLayout::GroupGridH);
	const FKodScreenRect Rail = MapInfo(KodHudLayout::GroupRailX, KodHudLayout::GroupRailY, KodHudLayout::GroupRailW, KodHudLayout::GroupRailH);
	UFont* Font = HudFont();
	bHasSelectionPanel = true;
	PanelMinX = Rail.X;
	PanelMinY = Grid.Y;
	PanelMaxX = Grid.X + Grid.W;
	PanelMaxY = Grid.Y + Grid.H;
	RememberChrome(FName(TEXT("Panel")), 0, FName(), PanelMinX, PanelMinY, PanelMaxX, PanelMaxY);

	auto DrawQueue = [this, Sim, Font, Scale](int32 EntityId)
	{
		const FKodStructureState* Structure = FindFrameStructure(EntityId);
		if (!Structure || (Structure->Trains.Num() == 0 && Structure->Slots.Num() == 0))
		{
			return;
		}
		for (int32 Slot = 0; Slot < 5; ++Slot)
		{
			const FKodScreenRect Cell = MapInfo(KodHudLayout::QueueX[Slot], KodHudLayout::QueueY[Slot], KodHudLayout::QueueW[Slot], KodHudLayout::QueueH[Slot]);
			const bool bFilled = Slot < Structure->Slots.Num();
			DrawPlate(Cell.X, Cell.Y, Cell.W, Cell.H, bFilled ? Tok(0x5C, 0x64, 0x6C) : Tok(0x3A, 0x3F, 0x45, 0.55f), TEXT("T_UI_VG_Btn_Cell"), 10.f, 10.f, 10.f, 10.f);
			if (!bFilled)
			{
				DrawIconSlot(FName(), Cell.X, Cell.Y, Cell.W, Cell.H, 0.45f);
				continue;
			}
			const FKodProductionSlot& Job = Structure->Slots[Slot];
			const FName JobId = Job.UnitId;
			DrawIconSlot(JobId, Cell.X, Cell.Y, Cell.W, Cell.H, 1.f);
			const FString Name = PanelFacingName(Sim, Job.DisplayId.IsNone() ? Job.UnitId : Job.DisplayId).ToString();
			DrawText(Name.Left(8), Tok(0xE6, 0xE6, 0xE0), Cell.X + 4.f * Scale, Cell.Y + Cell.H - 14.f * Scale, Font, 0.55f * Scale, false);
			if (Slot == 0)
			{
				float Fraction = 0.f;
				if (Job.bStarted && Structure->ActiveBuildTime > 0)
				{
					Fraction = 1.f - (static_cast<float>(Structure->ActiveTicksRemaining) / static_cast<float>(Structure->ActiveBuildTime));
					Fraction = FMath::Clamp(Fraction, 0.f, 1.f);
				}
				const FKodScreenRect Prog = MapInfo(KodHudLayout::QueueProgX, KodHudLayout::QueueProgY, KodHudLayout::QueueProgW, KodHudLayout::QueueProgH);
				DrawRect(Tok(0x1F, 0x23, 0x27), Prog.X, Prog.Y, Prog.W, Prog.H);
				DrawRect(Tok(0xFF, 0xD2, 0x3F), Prog.X, Prog.Y, Prog.W * Fraction, Prog.H);
			}
			FKodHudCommandHit Hit;
			Hit.bInsideChrome = true;
			Hit.bEnabled = true;
			Hit.Action = TEXT("CancelSlot");
			Hit.SlotIndex = Slot;
			Hit.StructureId = Structure->Id.Value;
			Hit.MinX = Cell.X;
			Hit.MinY = Cell.Y;
			Hit.MaxX = Cell.X + Cell.W;
			Hit.MaxY = Cell.Y + Cell.H;
			Hit.Tooltip = TEXT("Cancel");
			CommandHits.Add(Hit);
		}
	};

	if (Tiles.Num() == 1)
	{
		const FTile& Tile = Tiles[0];
		const FKodScreenRect Wire = MapInfo(KodHudLayout::SingleWellX, KodHudLayout::SingleWellY, KodHudLayout::SingleWellW, KodHudLayout::SingleWellH);
		const FKodScreenRect Info = MapInfo(KodHudLayout::SingleInfoX, KodHudLayout::SingleInfoY, KodHudLayout::SingleInfoW, KodHudLayout::SingleInfoH);
		const FKodScreenRect Silhouette = MapInfo(KodHudLayout::SingleSilhouetteX, KodHudLayout::SingleSilhouetteY, KodHudLayout::SingleSilhouette, KodHudLayout::SingleSilhouette);
		const int32 ShownHP = FMath::Max(0, FMath::FloorToInt(Tile.Health));
		const int32 MaxHP = FMath::Max(1, FMath::FloorToInt(Tile.MaxHealth));
		const float Fraction = FMath::Clamp(static_cast<float>(ShownHP) / static_cast<float>(MaxHP), 0.f, 1.f);
		const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
		const float* HitUntil = WireHitUntil.Find(Tile.EntityId);
		const bool bFlash = HitUntil && *HitUntil > Now;
		DrawRect(Tok(0x1F, 0x23, 0x27, 0.92f), Wire.X, Wire.Y, Wire.W, Wire.H);
		DrawWireSilhouette(Silhouette.X, Silhouette.Y, Silhouette.W, Silhouette.H, Tile.TypeId, ShownHP, MaxHP, Tile.bStructure, bFlash, KodWireTint::ESlot::Info160, Tile.Shield, Tile.ShieldMax, nullptr);
		DrawRect(Tok(0xB0, 0x8D, 0x57), Wire.X, Wire.Y, Wire.W, 2.f * Scale);
		float TextY = Info.Y + 6.f * Scale;
		const float Line = 16.f * Scale;
		DrawText(Tile.Name, Tok(0xE6, 0xE6, 0xE0), Info.X, TextY, Font, 0.9f * Scale, false);
		TextY += Line;
		const UKodHudProposed& PanelStyle = HudStyle();
		DrawText(FString::Printf(TEXT("HP %d/%d"), ShownHP, MaxHP), PanelHealthInk(Fraction, PanelStyle.WireHealthyPermille, PanelStyle.WireWarningPermille), Info.X, TextY, Font, 0.8f * Scale, false);
		TextY += Line;
		if (!Tile.Stim.IsEmpty())
		{
			DrawText(Tile.Stim, Tok(0xFF, 0x7A, 0x1A), Info.X, TextY, Font, 0.75f * Scale, false);
			TextY += Line;
		}
		DrawText(Tile.Order, Tok(0xB0, 0x8D, 0x57), Info.X, TextY, Font, 0.75f * Scale, false);
		TextY += Line;
		if (Tile.EnergyMax > 0)
		{
			DrawText(FString::Printf(TEXT("Energy %d/%d"), Tile.Energy, Tile.EnergyMax), Tok(0x2F, 0x7B, 0xFF), Info.X, TextY, Font, 0.75f * Scale, false);
			TextY += Line;
		}
		if (const FKodStructureState* CargoStructure = FindFrameStructure(Tile.EntityId))
		{
			if (CargoStructure->CargoCap > 0 && TextY < Info.Y + Info.H - Line)
			{
				DrawText(TEXT("Unload all"), Tok(0xE6, 0xE6, 0xE0), Info.X, TextY, Font, 0.7f * Scale, false);
				RememberChrome(FName(TEXT("UnloadAll")), CargoStructure->Id.Value, FName(), Info.X, TextY, Info.X + 120.f * Scale, TextY + Line);
				TextY += Line;
				const float Icon = 28.f * Scale;
				const int32 SlotCount = FMath::Clamp(CargoStructure->CargoCap, 1, 8);
				for (int32 Slot = 0; Slot < SlotCount; ++Slot)
				{
					if (TextY + Icon > Info.Y + Info.H)
					{
						break;
					}
					const bool bFilled = CargoStructure->Cargo.IsValidIndex(Slot);
					FName IconId;
					int32 Rider = 0;
					if (bFilled)
					{
						Rider = CargoStructure->Cargo[Slot];
						FKodEntityId RiderId;
						RiderId.Value = Rider;
						FKodSimEntityState RiderState;
						if (Sim->TryGetState(RiderId, RiderState))
						{
							IconId = RiderState.DefinitionId;
						}
					}
					DrawIconSlot(IconId, Info.X, TextY, Icon, Icon, bFilled ? 1.f : 0.35f);
					if (bFilled)
					{
						RememberChrome(FName(TEXT("Unload")), Rider, IconId, Info.X, TextY, Info.X + Icon, TextY + Icon);
					}
					TextY += Icon + 2.f * Scale;
				}
			}
		}
		RememberChrome(FName(TEXT("Tile")), Tile.EntityId, Tile.TypeId, Wire.X, Wire.Y, Info.X + Info.W, Info.Y + Info.H);
		DrawQueue(Tile.EntityId);
		return;
	}

	constexpr int32 PageSize = 24;
	const int32 Pages = FMath::Max(1, FMath::DivideAndRoundUp(Tiles.Num(), PageSize));
	if (SelectionPage >= Pages || SelectionPage >= 6)
	{
		SelectionPage = 0;
	}
	const int32 PageStart = SelectionPage * PageSize;
	const int32 Shown = FMath::Min(PageSize, Tiles.Num() - PageStart);
	if (Pages > 1)
	{
		const int32 ShownPages = FMath::Min(Pages, 6);
		for (int32 Page = 0; Page < ShownPages; ++Page)
		{
			const FKodScreenRect Tab = MapInfo(KodHudLayout::GroupPageX, KodHudLayout::GroupPageY + KodHudLayout::GroupPageStepY * static_cast<float>(Page), KodHudLayout::GroupPageW, KodHudLayout::GroupPageH);
			const bool bOn = Page == SelectionPage;
			DrawPlate(Tab.X, Tab.Y, Tab.W, Tab.H, bOn ? Tok(0xB0, 0x8D, 0x57) : Tok(0x3A, 0x3F, 0x45), TEXT("T_UI_VG_Tab_Group"), 8.f, 6.f, 8.f, 6.f);
			DrawText(FString::FromInt(Page + 1), Tok(0xE6, 0xE6, 0xE0), Tab.X + 8.f * Scale, Tab.Y + 14.f * Scale, Font, 0.7f * Scale, false);
			RememberChrome(FName(TEXT("Page")), Page, FName(), Tab.X, Tab.Y, Tab.X + Tab.W, Tab.Y + Tab.H);
		}
	}
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	for (int32 Index = 0; Index < Shown; ++Index)
	{
		const FTile& Tile = Tiles[PageStart + Index];
		const float* HitUntil = WireHitUntil.Find(Tile.EntityId);
		const bool bFlash = HitUntil && *HitUntil > Now;
		const int32 Col = Index % 8;
		const int32 Row = Index / 8;
		const FKodScreenRect Cell = MapInfo(
			KodHudLayout::GroupOriginX + KodHudLayout::GroupStepX * static_cast<float>(Col),
			KodHudLayout::GroupOriginY + KodHudLayout::GroupStepY * static_cast<float>(Row),
			KodHudLayout::GroupCellW,
			KodHudLayout::GroupCellH);
		const int32 ShownHP = FMath::Max(0, FMath::FloorToInt(Tile.Health));
		const int32 MaxHP = FMath::Max(1, FMath::FloorToInt(Tile.MaxHealth));
		DrawIconSlot(Tile.TypeId, Cell.X, Cell.Y, Cell.W, Cell.H, 1.f);
		const FKodScreenRect Silhouette = MapInfo(
			KodHudLayout::GroupOriginX + KodHudLayout::GroupStepX * static_cast<float>(Col) + KodHudLayout::GroupSilhouetteInsetX,
			KodHudLayout::GroupOriginY + KodHudLayout::GroupStepY * static_cast<float>(Row) + KodHudLayout::GroupSilhouetteInsetY,
			KodHudLayout::GroupSilhouette,
			KodHudLayout::GroupSilhouette);
		DrawWireSilhouette(Silhouette.X, Silhouette.Y, Silhouette.W, Silhouette.H, Tile.TypeId, ShownHP, MaxHP, Tile.bStructure, bFlash, KodWireTint::ESlot::Group48, Tile.Shield, Tile.ShieldMax, nullptr);
		const FLinearColor Frame = Tile.EntityId == PrimaryId ? Tok(0xD9, 0xB8, 0x78) : Tok(0x5C, 0x64, 0x6C);
		const float Edge = FMath::Max(1.f, (Tile.EntityId == PrimaryId ? 2.f : 1.f) * Scale);
		DrawRect(Frame, Cell.X, Cell.Y, Cell.W, Edge);
		DrawRect(Frame, Cell.X, Cell.Y + Cell.H - Edge, Cell.W, Edge);
		DrawRect(Frame, Cell.X, Cell.Y, Edge, Cell.H);
		DrawRect(Frame, Cell.X + Cell.W - Edge, Cell.Y, Edge, Cell.H);
		DrawRect(TeamInk(Tile.TeamId, ReadLocalTeamId()), Cell.X, Cell.Y, 2.f * Scale, Cell.H);
		if (Tile.EnergyMax > 0)
		{
			const float EnergyFrac = FMath::Clamp(static_cast<float>(Tile.Energy) / static_cast<float>(Tile.EnergyMax), 0.f, 1.f);
			DrawRect(Tok(0x1F, 0x23, 0x27), Cell.X, Cell.Y + Cell.H - 4.f * Scale, Cell.W, 4.f * Scale);
			DrawRect(Tok(0x2F, 0x7B, 0xFF), Cell.X, Cell.Y + Cell.H - 4.f * Scale, Cell.W * EnergyFrac, 4.f * Scale);
		}
		RememberChrome(FName(TEXT("Tile")), Tile.EntityId, Tile.TypeId, Cell.X, Cell.Y, Cell.X + Cell.W, Cell.Y + Cell.H);
	}
}

void AKodHUD::CacheSimSnapshot()
{
	FrameStates.Reset();
	FrameActors.Reset();
	FrameStructures.Reset();
	const UWorld* World = GetWorld();
	const UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Sim)
	{
		return;
	}
	Sim->CopyEntitySnapshot(FrameStates, FrameActors);
	Sim->CopyStructures(FrameStructures);
}

const FKodSimEntityState* AKodHUD::FindFrameState(int32 EntityId) const
{
	int32 Low = 0;
	int32 High = FrameStates.Num();
	while (Low < High)
	{
		const int32 Mid = (Low + High) / 2;
		const int32 Key = FrameStates[Mid].Id.Value;
		if (Key < EntityId)
		{
			Low = Mid + 1;
		}
		else
		{
			High = Mid;
		}
	}
	if (Low < FrameStates.Num() && FrameStates[Low].Id.Value == EntityId)
	{
		return &FrameStates[Low];
	}
	return nullptr;
}

const FKodStructureState* AKodHUD::FindFrameStructure(int32 EntityId) const
{
	int32 Low = 0;
	int32 High = FrameStructures.Num();
	while (Low < High)
	{
		const int32 Mid = (Low + High) / 2;
		const int32 Key = FrameStructures[Mid].Id.Value;
		if (Key < EntityId)
		{
			Low = Mid + 1;
		}
		else
		{
			High = Mid;
		}
	}
	if (Low < FrameStructures.Num() && FrameStructures[Low].Id.Value == EntityId)
	{
		return &FrameStructures[Low];
	}
	return nullptr;
}

bool AKodHUD::BlocksWorldSelect(float MouseX, float MouseY) const
{
	if (bFactionPrompt)
	{
		return true;
	}
	if (MenuPage != 0)
	{
		return true;
	}
	if (bHasMinimap && PointIn(MouseX, MouseY, MapMinX, MapMinY, MapMaxX, MapMaxY))
	{
		return true;
	}
	if (bHasSelectionPanel && PointIn(MouseX, MouseY, PanelMinX, PanelMinY, PanelMaxX, PanelMaxY))
	{
		return true;
	}
	if (bHasPortrait && PointIn(MouseX, MouseY, PortraitMinX, PortraitMinY, PortraitMaxX, PortraitMaxY))
	{
		return true;
	}
	if (bHasPowerPanel && PointIn(MouseX, MouseY, PowerMinX, PowerMinY, PowerMaxX, PowerMaxY))
	{
		return true;
	}
	FKodHudChromeHit Chrome;
	return GetChromeHit(MouseX, MouseY, Chrome);
}

void AKodHUD::ToggleHotkeyLegend()
{
	bHotkeyLegend = !bHotkeyLegend;
}

void AKodHUD::DrawHotkeyLegend()
{
	if (!Canvas || !bHotkeyLegend)
	{
		return;
	}
	const float Scale = FMath::Max(0.5f, Canvas->SizeY / 1080.f);
	const TCHAR* Lines[] = {
		TEXT("LMB  Select    Shift+LMB  Add    Drag  Box"),
		TEXT("RMB  Move / Attack / Follow / Gather"),
		TEXT("M Move  A Attack  H Hold  P Patrol  G Guard"),
		TEXT("S  Stop units and Train on a structure"),
		TEXT("Shift+click  Queue the next order"),
		TEXT("B  Build    D Depot    R Barracks    E Rig"),
		TEXT("T Stim    Backspace  Cancel    Tab  Subgroup"),
		TEXT("F1  Next idle AMP"),
		TEXT("1-0  Select group    Double-tap  Centre"),
		TEXT("Ctrl+1-0  Set    Shift+1-0  Add"),
		TEXT("F5-F8  Recall    Ctrl+F5-F8  Save    Space  Alert"),
		TEXT("F9  Jadeite colour (campaign and offline)"),
		TEXT("Alt  Show all health bars"),
		TEXT("Alt+click minimap  Ping"),
		TEXT("F12 or ?  Hide this list")
	};
	constexpr int32 LineCount = 15;
	UFont* Font = HudFont();
	const float Width = 560.f * Scale;
	const float LineH = 22.f * Scale;
	const float Height = 36.f * Scale + LineCount * LineH;
	const float X = (Canvas->SizeX - Width) * 0.5f;
	const float Y = (Canvas->SizeY - Height) * 0.5f;
	DrawRect(FLinearColor(0.02f, 0.03f, 0.04f, 0.94f), X, Y, Width, Height);
	DrawRect(FLinearColor(0.75f, 0.62f, 0.28f, 1.f), X, Y, Width, 3.f * Scale);
	DrawText(TEXT("Hotkeys"), FLinearColor(0.95f, 0.86f, 0.45f, 1.f), X + 16.f * Scale, Y + 10.f * Scale, Font, Scale, false);
	for (int32 Index = 0; Index < LineCount; ++Index)
	{
		DrawText(Lines[Index], FLinearColor::White, X + 16.f * Scale, Y + 36.f * Scale + Index * LineH, Font, 0.85f * Scale, false);
	}
}
