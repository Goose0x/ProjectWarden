#include "Game/KodHUD.h"

#include "Algo/Sort.h"
#include "Blueprint/UserWidget.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Canvas.h"
#include "CanvasItem.h"
#include "Engine/Texture.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "Sim/KodBuildTypes.h"
#include "StaticMeshAttributes.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Game/KodIsrMonitor.h"
#include "Game/KodPlayerController.h"
#include "Game/KodApmTracker.h"
#include "Game/KodPlayerState.h"
#include "Audio/KodCueSubsystem.h"
#include "Game/KodUserSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Game/KodPortraitStage.h"
#include "Game/KodRTSCameraPawn.h"
#include "Game/KodSpectatorPawn.h"
#include "Sim/KodResourceTypes.h"
#include "Sim/KodSimSubsystem.h"
#include "Warden/KodWardenPaths.h"

namespace
{
	FName PlayerFacingName(const UKodSimSubsystem* Sim, FName EngineId)
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
		return Label;
	}

	/** HP fill. Green at 66% and above, yellow from 33%, red below. */
	FLinearColor HealthBarInk(float Fraction)
	{
		const float Clamped = FMath::Clamp(Fraction, 0.f, 1.f);
		if (Clamped >= 0.66f)
		{
			return FLinearColor(FColor(0x4C, 0xD9, 0x64));
		}
		if (Clamped >= 0.33f)
		{
			return FLinearColor(FColor(0xF2, 0xC2, 0x30));
		}
		return FLinearColor(FColor(0xE5, 0x38, 0x3B));
	}

	struct FKodSpecRow
	{
		FString Label;
		float Fraction = -1.f;
		FLinearColor Cap = FLinearColor::White;
	};

	FLinearColor Tok(uint8 R, uint8 G, uint8 B, float A = 1.f)
	{
		return FLinearColor(FColor(R, G, B, static_cast<uint8>(FMath::Clamp(A, 0.f, 1.f) * 255.f)));
	}

	float DpiForShortest(float Shortest)
	{
		const float Stops[] = { 720.f, 1080.f, 1440.f, 2160.f };
		const float Values[] = { 0.667f, 1.f, 1.333f, 2.f };
		if (Shortest <= Stops[0])
		{
			return Values[0] * (Shortest / Stops[0]);
		}
		for (int32 Index = 0; Index < 3; ++Index)
		{
			if (Shortest <= Stops[Index + 1])
			{
				const float T = (Shortest - Stops[Index]) / (Stops[Index + 1] - Stops[Index]);
				return FMath::Lerp(Values[Index], Values[Index + 1], T);
			}
		}
		return Values[3] * (Shortest / Stops[3]);
	}
}

AKodHUD::AKodHUD()
{
	bShowHUD = true;
}

void AKodHUD::BuildVanguardFrame()
{
	FrameScale = 1.f;
	FrameOriginX = 0.f;
	FrameOriginY = 0.f;
	if (!Canvas)
	{
		return;
	}
	const float ViewW = Canvas->SizeX;
	const float ViewH = Canvas->SizeY;
	const float User = FMath::Clamp(HudUserScale, 0.8f, 1.2f);
	float Scale = DpiForShortest(FMath::Min(ViewW, ViewH)) * User;
	float FrameW = 1920.f * Scale;
	float FrameH = 1080.f * Scale;
	const float SafeW = ViewW * 0.9f;
	const float SafeH = ViewH * 0.9f;
	if (FrameW > SafeW || FrameH > SafeH)
	{
		Scale *= FMath::Min(SafeW / FMath::Max(1.f, FrameW), SafeH / FMath::Max(1.f, FrameH));
		FrameW = 1920.f * Scale;
		FrameH = 1080.f * Scale;
	}
	FrameScale = Scale;
	FrameOriginX = (ViewW - FrameW) * 0.5f;
	FrameOriginY = (ViewH - FrameH) * 0.5f;
	SafeMinX = ViewW * 0.05f;
	SafeMaxX = ViewW * 0.95f;
}

AKodHUD::FKodScreenRect AKodHUD::MapSpec(float X, float Y, float W, float H) const
{
	FKodScreenRect Out;
	Out.X = FrameOriginX + X * FrameScale;
	Out.Y = FrameOriginY + Y * FrameScale;
	Out.W = W * FrameScale;
	Out.H = H * FrameScale;
	return Out;
}

void AKodHUD::DrawPlate(float X, float Y, float W, float H, const FLinearColor& Fill, const TCHAR* SliceName, float MarginL, float MarginT, float MarginR, float MarginB)
{
	UTexture2D* Texture = nullptr;
	if (SliceName && *SliceName)
	{
		const FName Key(SliceName);
		if (!PlateMiss.Contains(Key))
		{
			if (const TObjectPtr<UTexture2D>* Found = PlateHits.Find(Key))
			{
				Texture = Found->Get();
			}
			else
			{
				const FString Path = FString::Printf(TEXT("/Game/UI/Vanguard/%s.%s"), SliceName, SliceName);
				Texture = Cast<UTexture2D>(FSoftObjectPath(Path).TryLoad());
				if (Texture)
				{
					PlateHits.Add(Key, Texture);
				}
				else
				{
					PlateMiss.Add(Key);
				}
			}
		}
	}
	if (!Texture || W <= 1.f || H <= 1.f)
	{
		DrawRect(Fill, X, Y, W, H);
		return;
	}
	const float L = FMath::Min(MarginL * FrameScale, W * 0.5f);
	const float T = FMath::Min(MarginT * FrameScale, H * 0.5f);
	const float R = FMath::Min(MarginR * FrameScale, W * 0.5f);
	const float B = FMath::Min(MarginB * FrameScale, H * 0.5f);
	const float TW = FMath::Max(1.f, static_cast<float>(Texture->GetSizeX()));
	const float TH = FMath::Max(1.f, static_cast<float>(Texture->GetSizeY()));
	const float UL = MarginL / TW;
	const float UT = MarginT / TH;
	const float UR = MarginR / TW;
	const float UB = MarginB / TH;
	auto Tile = [this, Texture](float DX, float DY, float DW, float DH, float U, float V, float UW, float VH)
	{
		if (DW > 0.5f && DH > 0.5f && UW > 0.f && VH > 0.f)
		{
			DrawTexture(Texture, DX, DY, DW, DH, U, V, UW, VH);
		}
	};
	Tile(X, Y, L, T, 0.f, 0.f, UL, UT);
	Tile(X + L, Y, W - L - R, T, UL, 0.f, 1.f - UL - UR, UT);
	Tile(X + W - R, Y, R, T, 1.f - UR, 0.f, UR, UT);
	Tile(X, Y + T, L, H - T - B, 0.f, UT, UL, 1.f - UT - UB);
	Tile(X + L, Y + T, W - L - R, H - T - B, UL, UT, 1.f - UL - UR, 1.f - UT - UB);
	Tile(X + W - R, Y + T, R, H - T - B, 1.f - UR, UT, UR, 1.f - UT - UB);
	Tile(X, Y + H - B, L, B, 0.f, 1.f - UB, UL, UB);
	Tile(X + L, Y + H - B, W - L - R, B, UL, 1.f - UB, 1.f - UL - UR, UB);
	Tile(X + W - R, Y + H - B, R, B, 1.f - UR, 1.f - UB, UR, UB);
}

void AKodHUD::DrawVanguardChrome()
{
	ChromeHits.Reset();
	if (!Canvas)
	{
		return;
	}
	const float Bottom = Canvas->SizeY;
	const float TopRaised = MapSpec(0.f, 756.f, 1.f, 1.f).Y;
	const float TopWell = MapSpec(0.f, 862.f, 1.f, 1.f).Y;
	const float SeamX = MapSpec(442.f, 0.f, 1.f, 1.f).X;
	const float PortX = MapSpec(1474.f, 0.f, 1.f, 1.f).X;
	const bool bScinder = HudSkin == 1;
	const FLinearColor Gunmetal = bScinder ? Tok(0x6E, 0x5B, 0x4F, 0.97f) : Tok(0x3A, 0x3F, 0x45, 0.97f);
	const FLinearColor Steel = bScinder ? Tok(0xA0, 0x78, 0x55, 0.96f) : Tok(0x5C, 0x64, 0x6C, 0.96f);
	const FLinearColor Well = bScinder ? Tok(0x1B, 0x43, 0x32, 0.94f) : Tok(0x1F, 0x23, 0x27, 0.94f);
	const FLinearColor Brass = bScinder ? Tok(0xDF, 0xD4, 0xC0, 1.f) : Tok(0xB0, 0x8D, 0x57, 1.f);
	auto FillTrap = [this](float X0, float Top0, float X1, float Top1, float Y1, const FLinearColor& Ink)
	{
		const FTexture* White = (Canvas && Canvas->DefaultTexture) ? Canvas->DefaultTexture->GetResource() : nullptr;
		if (!White || X1 <= X0)
		{
			DrawRect(Ink, X0, FMath::Min(Top0, Top1), FMath::Max(1.f, X1 - X0), Y1 - FMath::Min(Top0, Top1));
			return;
		}
		TArray<FCanvasUVTri> Tris;
		auto Push = [&Tris, Ink](const FVector2D& A, const FVector2D& B, const FVector2D& C)
		{
			FCanvasUVTri Tri;
			Tri.V0_Pos = A;
			Tri.V1_Pos = B;
			Tri.V2_Pos = C;
			Tri.V0_Color = Ink;
			Tri.V1_Color = Ink;
			Tri.V2_Color = Ink;
			Tris.Add(Tri);
		};
		const FVector2D P0(X0, Top0);
		const FVector2D P1(X1, Top1);
		const FVector2D P2(X1, Y1);
		const FVector2D P3(X0, Y1);
		Push(P0, P1, P2);
		Push(P0, P2, P3);
		FCanvasTriangleItem Item(Tris[0].V0_Pos, Tris[0].V1_Pos, Tris[0].V2_Pos, White);
		Item.TriangleList = Tris;
		Item.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Item);
	};
	FillTrap(0.f, TopRaised, SeamX, TopWell, Bottom, Gunmetal);
	FillTrap(SeamX, TopWell, PortX, TopWell, Bottom, Steel);
	FillTrap(PortX, TopWell, Canvas->SizeX, TopRaised, Bottom, Gunmetal);
	DrawLine(0.f, TopRaised, SeamX, TopWell, Brass, 2.f);
	DrawLine(SeamX, TopWell, PortX, TopWell, Brass, 2.f);
	DrawLine(PortX, TopWell, Canvas->SizeX, TopRaised, Brass, 2.f);
	DrawLine(SeamX, TopWell, SeamX, Bottom, Brass, 1.5f);
	DrawLine(PortX, TopWell, PortX, Bottom, Brass, 1.5f);

	const FKodScreenRect MapWell = MapSpec(8.f, 792.f, 280.f, 280.f);
	DrawRect(Well, MapWell.X, MapWell.Y, MapWell.W, MapWell.H);
	const FKodScreenRect SelWell = MapSpec(517.f, 870.f, 768.f, 202.f);
	DrawRect(Tok(0x1F, 0x23, 0x27, 0.88f), SelWell.X, SelWell.Y, SelWell.W, SelWell.H);
	const FKodScreenRect PortWell = MapSpec(1301.f, 862.f, 173.f, 205.f);
	DrawRect(Well, PortWell.X, PortWell.Y, PortWell.W, PortWell.H);
	DrawRect(Brass, PortWell.X, PortWell.Y, PortWell.W, 2.f * FrameScale);
	const FKodScreenRect CardWell = MapSpec(1507.f, 826.f, 404.f, 240.f);
	DrawRect(Well, CardWell.X, CardWell.Y, CardWell.W, CardWell.H);
	DrawTopBar();
}

void AKodHUD::DrawTopBar()
{
	if (!Canvas)
	{
		return;
	}
	UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;
	const FLinearColor Plate = HudSkin == 1 ? Tok(0x4F, 0x40, 0x38, 0.85f) : Tok(0x1F, 0x23, 0x27, 0.55f);
	const FLinearColor Ink = HudSkin == 1 ? Tok(0xED, 0xE6, 0xD8) : Tok(0xE6, 0xE6, 0xE0);
	const TCHAR* Labels[] = { TEXT("MENU  F10"), TEXT("LOG  F11"), TEXT("HELP  F12") };
	const float Xs[] = { 0.f, 108.f, 260.f };
	const float Ws[] = { 104.f, 148.f, 100.f };
	const FName Kinds[] = { FName(TEXT("MenuBtn")), FName(TEXT("ChatBtn")), FName(TEXT("HelpBtn")) };
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const FKodScreenRect Tab = MapSpec(Xs[Index], 0.f, Ws[Index], 28.f);
		DrawRect(Plate, Tab.X, Tab.Y, Tab.W, Tab.H);
		DrawText(Labels[Index], Ink, Tab.X + 8.f * FrameScale, Tab.Y + 6.f * FrameScale, Font, 0.7f * FrameScale, false);
		RememberChrome(Kinds[Index], 0, FName(), Tab.X, Tab.Y, Tab.X + Tab.W, Tab.Y + Tab.H);
	}
}

void AKodHUD::SetHudSkin(int32 Skin)
{
	HudSkin = Skin == 1 ? 1 : 0;
	if (UWorld* World = GetWorld())
	{
		if (UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>())
		{
			Sim->SetPlayerFaction(HudSkin);
		}
	}
}

void AKodHUD::DrawFaultlineObjectives()
{
	if (!Canvas)
	{
		return;
	}
	UWorld* World = GetWorld();
	const UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Sim || !Sim->IsFaultlineTraining())
	{
		return;
	}
	int32 Collected = 0;
	int32 Trained = 0;
	int32 Killed = 0;
	int32 Built = 0;
	int32 Razed = 0;
	Sim->GetTeamTotals(0, Collected, Trained, Killed, Built, Razed);
	int32 Depots = 0;
	int32 Barracks = 0;
	int32 Infantry = 0;
	int32 Outposts = 0;
	int32 Bases = 0;
	for (const FKodStructureState& Structure : FrameStructures)
	{
		if (Structure.TeamId == 0 && Structure.bComplete && Structure.DefinitionId == FName(KodWardenPaths::Id_SupplyDepot))
		{
			++Depots;
		}
		if (Structure.TeamId == 0 && Structure.bComplete && Structure.DefinitionId == FName(KodWardenPaths::Id_Barracks))
		{
			++Barracks;
		}
		if (Structure.TeamId == 1 && Structure.DefinitionId == FName(TEXT("KodEnemyOutpost")))
		{
			FKodSimEntityState State;
			if (Sim->TryGetState(Structure.Id, State) && State.Health > 0.f)
			{
				++Outposts;
			}
		}
		if (Structure.TeamId == 1 && Structure.DefinitionId == FName(TEXT("KodEnemyBase")))
		{
			FKodSimEntityState State;
			if (Sim->TryGetState(Structure.Id, State) && State.Health > 0.f)
			{
				++Bases;
			}
		}
	}
	for (const FKodSimEntityState& State : FrameStates)
	{
		if (State.TeamId == 0 && State.bInfantry && State.Health > 0.f)
		{
			++Infantry;
		}
	}
	const bool Done[] = {
		Collected >= 300,
		Depots >= 1,
		Barracks >= 1,
		Infantry >= 5 || Trained >= 5,
		Outposts == 0,
		Bases == 0
	};
	const TCHAR* Lines[] = {
		TEXT("Gather 300 Jadeite"),
		TEXT("Build a supply structure"),
		TEXT("Build a barracks"),
		TEXT("Train 5 infantry"),
		TEXT("Destroy the enemy outpost"),
		TEXT("Destroy the enemy base")
	};
	const FKodScreenRect Panel = MapSpec(16.f, 40.f, 460.f, 220.f);
	const FLinearColor Back = HudSkin == 1 ? Tok(0x1B, 0x43, 0x32, 0.7f) : Tok(0x1F, 0x23, 0x27, 0.7f);
	const FLinearColor Accent = HudSkin == 1 ? Tok(0xDF, 0xD4, 0xC0) : Tok(0xFF, 0xD2, 0x3F);
	DrawRect(Back, Panel.X, Panel.Y, Panel.W, Panel.H);
	UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;
	DrawText(TEXT("MAIN OBJECTIVES"), Accent, Panel.X + 10.f * FrameScale, Panel.Y + 6.f * FrameScale, Font, 0.85f * FrameScale, false);
	for (int32 Index = 0; Index < 6; ++Index)
	{
		const float Y = Panel.Y + (28.f + 26.f * static_cast<float>(Index)) * FrameScale;
		const float X = Panel.X + 10.f * FrameScale;
		DrawRect(Done[Index] ? Tok(0x4C, 0xD9, 0x64) : Tok(0xE6, 0xE6, 0xE0, 0.15f), X, Y, 14.f * FrameScale, 14.f * FrameScale);
		DrawText(Lines[Index], Done[Index] ? Tok(0x8E, 0x94, 0x9A) : Tok(0xE6, 0xE6, 0xE0), X + 22.f * FrameScale, Y, Font, 0.75f * FrameScale, false);
		const uint8 Bit = static_cast<uint8>(1 << Index);
		if (Done[Index] && (ObjectiveDoneMask & Bit) == 0)
		{
			ObjectiveDoneMask |= Bit;
			if (UKodSimSubsystem* Live = World->GetSubsystem<UKodSimSubsystem>())
			{
				Live->RaiseCue(FName(TEXT("objective_complete")), FVector::ZeroVector, false);
			}
		}
	}
}

void AKodHUD::DrawHUD()
{
	Super::DrawHUD();
	bShowHUD = true;
	BuildVanguardFrame();
	DrawVanguardChrome();
	DrawFaultlineObjectives();

	if (!bLoggedFirstDraw)
	{
		bLoggedFirstDraw = true;
		const float SizeX = Canvas ? Canvas->SizeX : 0.f;
		const float SizeY = Canvas ? Canvas->SizeY : 0.f;
		UE_LOG(LogTemp, Log, TEXT("KodHUD DrawHUD First SizeX=%.0f SizeY=%.0f"), SizeX, SizeY);
	}

	CacheSimSnapshot();
	DrawWorldFog();
	DrawScaffolds();
	RefreshReadout();
	DrawGameClock();
	DrawAlertFeed();
	DrawSpectatorPanel();
	DrawProductionPanel();
	DrawCommandCard();
	DrawPowersPanel();
	DrawBuildHint();
	DrawMinimap();
	DrawIdleWorkerButton();
	DrawControlGroupTabs();
	DrawPortrait();
	DrawSelectionPanel();
	DrawResourceFlash();
	DrawMatchOutcome();
	DrawUnitHealthBars();
	DrawClickMarkers();
	DrawSelectionPulse();
	DrawJuiceFloaters();
	DrawSelectedResourceNodes();
	DrawSelectionMarquee();
	DrawHotkeyLegend();
	DrawThreatRings();
	DrawIsrPopup();
	DrawShellMenus();
	if (Canvas && (bChatOpen || ChatLog.Num() > 0))
	{
		UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
		const float RowH = 22.f * FrameScale;
		const FKodScreenRect Feed = MapSpec(16.f, 420.f, 380.f, 28.f);
		const float X = Feed.X + (SafeMinX - FrameOriginX);
		const int32 Lines = ChatLog.Num() + (bChatOpen ? 1 : 0);
		float Y = Feed.Y - 8.f * FrameScale - static_cast<float>(Lines) * RowH;
		Y = FMath::Max(FrameOriginY + 8.f * FrameScale, Y);
		for (const FString& Msg : ChatLog)
		{
			DrawText(Msg, Tok(0xE6, 0xE6, 0xE0), X, Y, Font, FrameScale, false);
			Y += RowH;
		}
		if (bChatOpen)
		{
			const FString Line = FString::Printf(TEXT("> %s_"), *ChatLine);
			DrawRect(Tok(0x1F, 0x23, 0x27, 0.9f), X, Y, Feed.W, RowH);
			DrawText(Line, Tok(0xFF, 0xD2, 0x3F), X + 8.f * FrameScale, Y, Font, FrameScale, false);
		}
	}
}

void AKodHUD::AddCommandMarker(const FVector& WorldPoint, int32 Kind)
{
	FKodClickMarker Marker;
	Marker.World = WorldPoint;
	Marker.Kind = Kind;
	Marker.Born = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	Marker.Life = Kind == 2 ? 0.85f : 0.6f;
	ClickMarkers.Add(Marker);
	while (ClickMarkers.Num() > 12)
	{
		ClickMarkers.RemoveAt(0);
	}
}

void AKodHUD::AddMinimapPing(const FVector& WorldPoint, float Seconds)
{
	const UWorld* World = GetWorld();
	const float Until = World ? World->GetTimeSeconds() + FMath::Max(0.5f, Seconds) : 0.f;
	MinimapPings.Add(WorldPoint);
	MinimapPingUntil.Add(Until);
	UE_LOG(LogTemp, Log, TEXT("KodMap Ping X=%.0f Y=%.0f"), WorldPoint.X, WorldPoint.Y);
}

void AKodHUD::DrawClickMarkers()
{
	if (!Canvas)
	{
		return;
	}
	APlayerController* PC = GetOwningPlayerController();
	UWorld* World = GetWorld();
	if (!PC || !World)
	{
		return;
	}
	const float Now = World->GetTimeSeconds();
	for (int32 Index = ClickMarkers.Num() - 1; Index >= 0; --Index)
	{
		const FKodClickMarker& Marker = ClickMarkers[Index];
		const float Age = Now - Marker.Born;
		if (Age < 0.f || Age > Marker.Life)
		{
			ClickMarkers.RemoveAt(Index);
			continue;
		}
		FVector2D Screen;
		if (!PC->ProjectWorldLocationToScreen(Marker.World, Screen, true))
		{
			continue;
		}
		const float Life = 1.f - (Age / FMath::Max(0.05f, Marker.Life));
		const float Pulse = 0.65f + 0.35f * FMath::Abs(FMath::Sin(Age * 16.f));
		const float Radius = 10.f + 34.f * (Age / FMath::Max(0.05f, Marker.Life));
		FLinearColor Color = FLinearColor(FColor(0x3D, 0xFF, 0x6A, static_cast<uint8>(FMath::Clamp(Life * Pulse, 0.f, 1.f) * 255.f)));
		if (Marker.Kind == 1)
		{
			Color = FLinearColor(0.95f, 0.18f, 0.14f, Life * Pulse);
		}
		else if (Marker.Kind == 2)
		{
			Color = FLinearColor(0.95f, 0.82f, 0.28f, Life * Pulse);
		}
		const int32 Steps = 14;
		FVector2D Prev(Screen.X + Radius, Screen.Y);
		for (int32 Step = 1; Step <= Steps; ++Step)
		{
			const float Angle = (static_cast<float>(Step) / static_cast<float>(Steps)) * 2.f * PI;
			const FVector2D Next(Screen.X + FMath::Cos(Angle) * Radius, Screen.Y + FMath::Sin(Angle) * Radius);
			DrawLine(Prev.X, Prev.Y, Next.X, Next.Y, Color, 2.f);
			Prev = Next;
		}
	}
}

namespace
{
	UTexture2D* MakeTintedRing(const FColor& Color)
	{
		constexpr int32 RingTexels = 64;
		UTexture2D* Tex = UTexture2D::CreateTransient(RingTexels, RingTexels, PF_B8G8R8A8);
		if (!Tex)
		{
			return nullptr;
		}
		Tex->SRGB = true;
		Tex->Filter = TF_Bilinear;
		Tex->AddressX = TA_Clamp;
		Tex->AddressY = TA_Clamp;
		Tex->NeverStream = true;
		FTexturePlatformData* Platform = Tex->GetPlatformData();
		if (!Platform || Platform->Mips.Num() == 0)
		{
			return nullptr;
		}
		uint8* Bits = static_cast<uint8*>(Platform->Mips[0].BulkData.Lock(LOCK_READ_WRITE));
		for (int32 PixelY = 0; PixelY < RingTexels; ++PixelY)
		{
			for (int32 PixelX = 0; PixelX < RingTexels; ++PixelX)
			{
				const float Dx = ((static_cast<float>(PixelX) + 0.5f) / static_cast<float>(RingTexels)) * 2.f - 1.f;
				const float Dy = ((static_cast<float>(PixelY) + 0.5f) / static_cast<float>(RingTexels)) * 2.f - 1.f;
				const float Dist = FMath::Sqrt(Dx * Dx + Dy * Dy);
				float Band = 0.f;
				if (Dist >= 0.62f && Dist <= 0.96f)
				{
					const float Inner = FMath::Clamp((Dist - 0.62f) / 0.08f, 0.f, 1.f);
					const float Outer = FMath::Clamp((0.96f - Dist) / 0.08f, 0.f, 1.f);
					Band = FMath::Min(Inner, Outer);
				}
				const int32 Offset = (PixelY * RingTexels + PixelX) * 4;
				Bits[Offset + 0] = Color.B;
				Bits[Offset + 1] = Color.G;
				Bits[Offset + 2] = Color.R;
				Bits[Offset + 3] = static_cast<uint8>(FMath::RoundToInt(Band * 255.f));
			}
		}
		Platform->Mips[0].BulkData.Unlock();
		Tex->UpdateResource();
		return Tex;
	}
}

UStaticMesh* AKodHUD::GetPresentationQuad()
{
	if (PresentationQuad)
	{
		return PresentationQuad;
	}
	UStaticMesh* Mesh = NewObject<UStaticMesh>(this, NAME_None, RF_Transient);
	if (!Mesh)
	{
		return nullptr;
	}
	FMeshDescription Desc;
	FStaticMeshAttributes Attributes(Desc);
	Attributes.Register();
	const FVector3f Positions[4] = {
		FVector3f(-50.f, -50.f, 0.f),
		FVector3f(50.f, -50.f, 0.f),
		FVector3f(50.f, 50.f, 0.f),
		FVector3f(-50.f, 50.f, 0.f)
	};
	const FVector2f UVs[4] = {
		FVector2f(0.f, 0.f),
		FVector2f(1.f, 0.f),
		FVector2f(1.f, 1.f),
		FVector2f(0.f, 1.f)
	};
	TArray<FVertexInstanceID> Corners;
	Corners.SetNum(4);
	for (int32 Corner = 0; Corner < 4; ++Corner)
	{
		const FVertexID Vertex = Desc.CreateVertex();
		Attributes.GetVertexPositions()[Vertex] = Positions[Corner];
		const FVertexInstanceID Instance = Desc.CreateVertexInstance(Vertex);
		Attributes.GetVertexInstanceUVs().Set(Instance, 0, UVs[Corner]);
		Attributes.GetVertexInstanceNormals().Set(Instance, FVector3f(0.f, 0.f, 1.f));
		Corners[Corner] = Instance;
	}
	const FPolygonGroupID Group = Desc.CreatePolygonGroup();
	Attributes.GetPolygonGroupMaterialSlotNames()[Group] = FName(TEXT("Main"));
	TArray<FVertexInstanceID> FirstTri;
	FirstTri.Add(Corners[0]);
	FirstTri.Add(Corners[1]);
	FirstTri.Add(Corners[2]);
	TArray<FVertexInstanceID> SecondTri;
	SecondTri.Add(Corners[0]);
	SecondTri.Add(Corners[2]);
	SecondTri.Add(Corners[3]);
	Desc.CreatePolygon(Group, FirstTri);
	Desc.CreatePolygon(Group, SecondTri);
	UStaticMesh::FBuildMeshDescriptionsParams BuildParams;
	BuildParams.bBuildSimpleCollision = false;
	BuildParams.bFastBuild = true;
	TArray<const FMeshDescription*> Descs;
	Descs.Add(&Desc);
	Mesh->BuildFromMeshDescriptions(Descs, BuildParams);
	PresentationQuad = Mesh;
	return PresentationQuad;
}

UMaterialInterface* AKodHUD::GetPassThroughMaterial()
{
	if (PassThroughMaterial)
	{
		return PassThroughMaterial;
	}
	const TCHAR* Paths[] = {
		TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Translucent_OneSided.Widget3DPassThrough_Translucent_OneSided"),
		TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Translucent.Widget3DPassThrough_Translucent")
	};
	for (const TCHAR* Path : Paths)
	{
		if (UMaterialInterface* Loaded = LoadObject<UMaterialInterface>(nullptr, Path))
		{
			PassThroughMaterial = Loaded;
			return PassThroughMaterial;
		}
	}
	static bool bLoggedPass = false;
	if (!bLoggedPass)
	{
		bLoggedPass = true;
		UE_LOG(LogTemp, Warning, TEXT("KodFog Material Miss"));
	}
	return nullptr;
}

UMaterialInstanceDynamic* AKodHUD::RingMaterialFor(int32 Kind)
{
	UMaterialInstanceDynamic* Existing = Kind == 0 ? RingMaterialOwn.Get() : (Kind == 1 ? RingMaterialAlly.Get() : RingMaterialEnemy.Get());
	if (Existing)
	{
		return Existing;
	}
	UMaterialInterface* Parent = GetPassThroughMaterial();
	if (!Parent)
	{
		return nullptr;
	}
	const FColor Ink = Kind == 0
		? FColor(0x4C, 0xD9, 0x64)
		: (Kind == 1 ? FColor(0xFF, 0xD2, 0x3F) : FColor(0xE5, 0x38, 0x3B));
	UTexture2D* RingTex = MakeTintedRing(Ink);
	if (!RingTex)
	{
		return nullptr;
	}
	UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Parent, this);
	if (!Mid)
	{
		return nullptr;
	}
	Mid->SetTextureParameterValue(TEXT("SlateUI"), RingTex);
	if (Kind == 0)
	{
		RingMaterialOwn = Mid;
	}
	else if (Kind == 1)
	{
		RingMaterialAlly = Mid;
	}
	else
	{
		RingMaterialEnemy = Mid;
	}
	return Mid;
}

void AKodHUD::SyncGroundRings(const TMap<AActor*, int32>& Rings, const UKodSimSubsystem* Sim)
{
	UWorld* World = GetWorld();
	UStaticMesh* Quad = GetPresentationQuad();
	if (!World || !Sim || !Quad || !GetPassThroughMaterial())
	{
		return;
	}
	TSet<AActor*> Live;
	for (const TPair<AActor*, int32>& Ring : Rings)
	{
		AActor* Actor = Ring.Key;
		if (!Actor || !Actor->GetRootComponent())
		{
			continue;
		}
		Live.Add(Actor);
		UStaticMeshComponent* Comp = nullptr;
		for (FKodRingSlot& Slot : GroundRings)
		{
			if (Slot.Actor.Get() == Actor)
			{
				Comp = Slot.Mesh.Get();
				break;
			}
		}
		if (!Comp)
		{
			Comp = NewObject<UStaticMeshComponent>(Actor);
			if (!Comp)
			{
				continue;
			}
			Comp->ComponentTags.Add(FName(TEXT("KodSelectRing")));
			Comp->SetMobility(EComponentMobility::Movable);
			Comp->SetStaticMesh(Quad);
			Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Comp->SetCastShadow(false);
			Comp->SetCanEverAffectNavigation(false);
			Comp->SetupAttachment(Actor->GetRootComponent());
			Comp->SetUsingAbsoluteLocation(true);
			Comp->SetUsingAbsoluteRotation(true);
			Comp->SetUsingAbsoluteScale(true);
			Comp->RegisterComponent();
			FKodRingSlot Added;
			Added.Actor = Actor;
			Added.Mesh = Comp;
			GroundRings.Add(Added);
		}
		const FVector RootPoint = Actor->GetActorLocation();
		float FloorZ = RootPoint.Z;
		FHitResult Hit;
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(KodSelectRing), false, Actor);
		const bool bHit = World->LineTraceSingleByChannel(
			Hit,
			RootPoint + FVector(0.f, 0.f, 800.f),
			RootPoint - FVector(0.f, 0.f, 4000.f),
			ECC_Visibility,
			QueryParams);
		if (bHit)
		{
			FloorZ = Hit.ImpactPoint.Z + 6.f;
		}
		float Radius = 40.f;
		const FKodEntityId Id = Sim->FindIdForActor(Actor);
		if (const FKodStructureState* Structure = FindFrameStructure(Id.Value))
		{
			const float Cells = static_cast<float>(FMath::Max(Structure->FootprintX, Structure->FootprintY));
			Radius = 0.5f * Cells * static_cast<float>(KodBuildGrid::CellUU);
		}
		else if (const FKodSimEntityState* State = FindFrameState(Id.Value))
		{
			Radius = static_cast<float>(FMath::Max(16, State->BodyRadiusUU));
		}
		const float RingScale = FMath::Max(0.05f, (Radius * 2.f) / 100.f);
		Comp->SetWorldLocation(FVector(RootPoint.X, RootPoint.Y, FloorZ));
		Comp->SetWorldRotation(FRotator::ZeroRotator);
		Comp->SetWorldScale3D(FVector(RingScale, RingScale, 1.f));
		if (UMaterialInstanceDynamic* InkMat = RingMaterialFor(Ring.Value))
		{
			Comp->SetMaterial(0, InkMat);
		}
		Comp->SetHiddenInGame(false);
		Comp->SetVisibility(true);
	}
	for (FKodRingSlot& Slot : GroundRings)
	{
		AActor* Actor = Slot.Actor.Get();
		UStaticMeshComponent* Mesh = Slot.Mesh.Get();
		if (!Mesh)
		{
			continue;
		}
		if (!Actor || !Live.Contains(Actor))
		{
			Mesh->SetHiddenInGame(true);
			Mesh->SetVisibility(false);
		}
	}
}

void AKodHUD::SyncFogSheet(const UKodSimSubsystem* Sim, int32 LocalTeam)
{
	UWorld* World = GetWorld();
	if (!World || !Sim || !bMapFog)
	{
		if (AStaticMeshActor* Sheet = FogSheetActor.Get())
		{
			Sheet->SetActorHiddenInGame(true);
		}
		return;
	}
	int32 MinX = 0;
	int32 MinY = 0;
	int32 MaxX = 0;
	int32 MaxY = 0;
	if (!Sim->TryGetPlayableCellBounds(MinX, MinY, MaxX, MaxY))
	{
		return;
	}
	const int32 GridW = MaxX - MinX + 1;
	const int32 GridH = MaxY - MinY + 1;
	if (GridW <= 0 || GridH <= 0 || GridW > 512 || GridH > 512)
	{
		return;
	}
	const bool bResize = !FogSheetTexture || FogCellsX != GridW || FogCellsY != GridH || FogOriginX != MinX || FogOriginY != MinY;
	if (bResize)
	{
		FogSheetTexture = UTexture2D::CreateTransient(GridW, GridH, PF_B8G8R8A8);
		if (!FogSheetTexture)
		{
			return;
		}
		FogSheetTexture->SRGB = false;
		FogSheetTexture->Filter = TF_Bilinear;
		FogSheetTexture->AddressX = TA_Clamp;
		FogSheetTexture->AddressY = TA_Clamp;
		FogSheetTexture->NeverStream = true;
		FogSheetTexture->MipGenSettings = TMGS_NoMipmaps;
		FogSheetTexture->UpdateResource();
		FogCellsX = GridW;
		FogCellsY = GridH;
		FogOriginX = MinX;
		FogOriginY = MinY;
		FogSheetTick = MIN_int64;
		FogSheetMaterial = nullptr;
	}
	if (FogSheetTexture && FogSheetTick != Sim->GetSimTickIndex())
	{
		FTexturePlatformData* Platform = FogSheetTexture->GetPlatformData();
		if (Platform && Platform->Mips.Num() > 0)
		{
			uint8* Bits = static_cast<uint8*>(Platform->Mips[0].BulkData.Lock(LOCK_READ_WRITE));
			for (int32 PixelY = 0; PixelY < GridH; ++PixelY)
			{
				for (int32 PixelX = 0; PixelX < GridW; ++PixelX)
				{
					const int32 CellX = MinX + PixelX;
					const int32 CellY = MinY + PixelY;
					const uint8 Vision = Sim->GetTeamVision(LocalTeam, KodBuildGrid::CellCenter(CellX), KodBuildGrid::CellCenter(CellY));
					uint8 Alpha = 0;
					if (Vision == 0)
					{
						Alpha = 153;
					}
					else if (Vision != 2)
					{
						Alpha = 89;
					}
					const int32 Offset = (PixelY * GridW + PixelX) * 4;
					Bits[Offset + 0] = 0;
					Bits[Offset + 1] = 0;
					Bits[Offset + 2] = 0;
					Bits[Offset + 3] = Alpha;
				}
			}
			struct FKodFogUpload
			{
				TArray<uint8> Bytes;
				FUpdateTextureRegion2D Region;
			};
			FKodFogUpload* Upload = new FKodFogUpload();
			const int32 ByteCount = GridW * GridH * 4;
			Upload->Bytes.SetNumUninitialized(ByteCount);
			FMemory::Memcpy(Upload->Bytes.GetData(), Bits, ByteCount);
			Platform->Mips[0].BulkData.Unlock();
			Upload->Region = FUpdateTextureRegion2D(0, 0, 0, 0, static_cast<uint32>(GridW), static_cast<uint32>(GridH));
			FogSheetTexture->UpdateTextureRegions(
				0,
				1,
				&Upload->Region,
				static_cast<uint32>(GridW * 4),
				4,
				Upload->Bytes.GetData(),
				[Upload](uint8*, const FUpdateTextureRegion2D*)
				{
					delete Upload;
				});
		}
		FogSheetTick = Sim->GetSimTickIndex();
	}
	UMaterialInterface* Parent = GetPassThroughMaterial();
	UStaticMesh* Quad = GetPresentationQuad();
	if (!Parent || !Quad || !FogSheetTexture)
	{
		return;
	}
	if (!FogSheetMaterial)
	{
		FogSheetMaterial = UMaterialInstanceDynamic::Create(Parent, this);
		if (FogSheetMaterial)
		{
			FogSheetMaterial->SetTextureParameterValue(TEXT("SlateUI"), FogSheetTexture);
		}
	}
	AStaticMeshActor* Sheet = FogSheetActor.Get();
	if (!Sheet)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Sheet = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform::Identity, Params);
		if (!Sheet)
		{
			return;
		}
		Sheet->Tags.Add(FName(TEXT("KodFogSheet")));
		Sheet->SetActorEnableCollision(false);
		if (UStaticMeshComponent* Born = Sheet->GetStaticMeshComponent())
		{
			Born->SetMobility(EComponentMobility::Movable);
			Born->SetStaticMesh(Quad);
			Born->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Born->SetCastShadow(false);
			Born->SetCanEverAffectNavigation(false);
		}
		FogSheetActor = Sheet;
	}
	const float WorldW = static_cast<float>(GridW * KodBuildGrid::CellUU);
	const float WorldH = static_cast<float>(GridH * KodBuildGrid::CellUU);
	const float OriginWorldX = static_cast<float>(MinX * KodBuildGrid::CellUU);
	const float OriginWorldY = static_cast<float>(MinY * KodBuildGrid::CellUU);
	Sheet->SetActorHiddenInGame(false);
	Sheet->SetActorLocation(FVector(OriginWorldX + WorldW * 0.5f, OriginWorldY + WorldH * 0.5f, 2.f));
	Sheet->SetActorScale3D(FVector(FMath::Max(0.01f, WorldW / 100.f), FMath::Max(0.01f, WorldH / 100.f), 1.f));
	if (UStaticMeshComponent* SheetMesh = Sheet->GetStaticMeshComponent())
	{
		SheetMesh->SetMobility(EComponentMobility::Movable);
		if (FogSheetMaterial)
		{
			SheetMesh->SetMaterial(0, FogSheetMaterial);
		}
	}
}

void AKodHUD::DrawSelectionPulse()
{
	AKodPlayerController* PC = Cast<AKodPlayerController>(GetOwningPlayerController());
	UWorld* World = GetWorld();
	const UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Canvas || !PC || !World || !Sim)
	{
		return;
	}
	const int32 LocalTeam = ReadLocalTeamId();
	// Rebuilt every frame from the live actor. 0 own, 1 ally, 2 enemy.
	TMap<AActor*, int32> Rings;
	auto Consider = [&Rings](AActor* Actor, int32 Kind)
	{
		if (!Actor)
		{
			return;
		}
		if (int32* Found = Rings.Find(Actor))
		{
			*Found = FMath::Max(*Found, Kind);
			return;
		}
		Rings.Add(Actor, Kind);
	};
	auto TeamOf = [this](FKodEntityId Id, int32 Fallback) -> int32
	{
		if (const FKodSimEntityState* State = FindFrameState(Id.Value))
		{
			return State->TeamId;
		}
		if (const FKodStructureState* Structure = FindFrameStructure(Id.Value))
		{
			return Structure->TeamId;
		}
		return Fallback;
	};
	for (AActor* Actor : PC->GetLocalSelection())
	{
		if (!Actor)
		{
			continue;
		}
		const FKodEntityId Id = Sim->FindIdForActor(Actor);
		const int32 Team = TeamOf(Id, LocalTeam);
		const int32 Opponent = LocalTeam == 0 ? 1 : 0;
		const int32 Kind = (Team == LocalTeam) ? 0 : (Team == Opponent ? 2 : 1);
		Consider(Actor, Kind);
		if (const FKodSimEntityState* State = FindFrameState(Id.Value))
		{
			if (State->AttackTarget.IsValid())
			{
				if (AActor* TargetActor = Sim->ResolveEntity(State->AttackTarget))
				{
					if (TeamOf(State->AttackTarget, Team) != LocalTeam)
					{
						Consider(TargetActor, 2);
					}
				}
			}
		}
	}
	FHitResult Hover;
	if (PC->GetHitResultUnderCursor(ECC_Visibility, true, Hover))
	{
		if (AActor* HoverActor = Hover.GetActor())
		{
			const FKodEntityId HoverId = Sim->FindIdForActor(HoverActor);
			const int32 HoverTeam = TeamOf(HoverId, LocalTeam);
			const bool bKnown = FindFrameState(HoverId.Value) != nullptr || FindFrameStructure(HoverId.Value) != nullptr;
			if (bKnown && HoverTeam != LocalTeam)
			{
				Consider(HoverActor, 2);
			}
		}
	}
	SyncGroundRings(Rings, Sim);
}

void AKodHUD::DrawJuiceFloaters()
{
	APlayerController* PC = GetOwningPlayerController();
	UWorld* World = GetWorld();
	if (!Canvas || !PC || !World)
	{
		return;
	}
	const UKodUserSettings* Settings = UKodUserSettings::Get();
	const bool bNumbers = Settings && Settings->bShowDamageNumbers;
	const float Now = World->GetTimeSeconds();
	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	for (int32 Index = JuiceFloaters.Num() - 1; Index >= 0; --Index)
	{
		const FKodHudFloater& Floater = JuiceFloaters[Index];
		const float Age = Now - Floater.Born;
		if (Age > 0.9f)
		{
			JuiceFloaters.RemoveAt(Index);
			continue;
		}
		if (Floater.Kind == 0 && !bNumbers)
		{
			continue;
		}
		FVector2D Screen;
		if (!PC->ProjectWorldLocationToScreen(Floater.World + FVector(0.f, 0.f, 160.f), Screen, true))
		{
			continue;
		}
		const float Alpha = 1.f - (Age / 0.9f);
		const float Rise = Age * 36.f;
		const FString Label = Floater.Kind == 1
			? FString::Printf(TEXT("+%d"), Floater.Amount)
			: FString::FromInt(Floater.Amount);
		const FLinearColor Color = Floater.Kind == 1
			? FLinearColor(0.55f, 0.95f, 0.35f, Alpha)
			: FLinearColor(1.f, 0.35f, 0.28f, Alpha);
		DrawText(Label, Color, Screen.X - 8.f, Screen.Y - Rise, Font, 1.1f, false);
	}
}

void AKodHUD::ToggleMinimapLayer(int32 LayerIndex)
{
	switch (LayerIndex)
	{
	case 10:
		MapChipCycle = (MapChipCycle + 1) % 4;
		bMapTerrain = MapChipCycle == 0;
		bMapUnits = MapChipCycle == 1;
		bMapResources = MapChipCycle == 2;
		bMapFog = MapChipCycle == 3;
		break;
	case 0:
		bMapTerrain = !bMapTerrain;
		break;
	case 1:
		bMapUnits = !bMapUnits;
		break;
	case 2:
		bMapResources = !bMapResources;
		break;
	case 3:
		bMapFog = !bMapFog;
		break;
	case 4:
		bMapAlerts = !bMapAlerts;
		break;
	case 5:
		bMapOwnOnly = !bMapOwnOnly;
		break;
	default:
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("KodMap Layer=%d"), LayerIndex);
}

void AKodHUD::DrawWorldFog()
{
	UWorld* World = GetWorld();
	const UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	const int32 LocalTeam = ReadLocalTeamId();
	if (!bMapFog || !Sim)
	{
		if (AStaticMeshActor* Sheet = FogSheetActor.Get())
		{
			Sheet->SetActorHiddenInGame(true);
		}
		return;
	}
	SyncFogSheet(Sim, LocalTeam);
	if (!Canvas)
	{
		return;
	}
	APlayerController* PC = GetOwningPlayerController();
	const AKodRTSCameraPawn* Camera = Cast<AKodRTSCameraPawn>(GetOwningPawn());
	if (!PC || !Camera)
	{
		return;
	}
	FVector NearLeft;
	FVector NearRight;
	FVector FarRight;
	FVector FarLeft;
	if (!Camera->GetGroundViewCorners(NearLeft, NearRight, FarRight, FarLeft))
	{
		return;
	}
	const float GroundZ = (NearLeft.Z + NearRight.Z + FarRight.Z + FarLeft.Z) * 0.25f;
	const int32 Cell = KodBuildGrid::CellUU;


	TArray<int32> SeenTeam;
	TArray<int32> SeenX;
	TArray<int32> SeenY;
	TArray<int32> SeenFootX;
	TArray<int32> SeenFootY;
	Sim->CopySeenStructureBlips(LocalTeam, SeenTeam, SeenX, SeenY, SeenFootX, SeenFootY);
	for (int32 Index = 0; Index < SeenX.Num(); ++Index)
	{
		const float CenterX = static_cast<float>(SeenX[Index]);
		const float CenterY = static_cast<float>(SeenY[Index]);
		if (Sim->GetTeamVision(LocalTeam, CenterX, CenterY) != 1)
		{
			continue;
		}
		const float HalfX = 0.5f * static_cast<float>(FMath::Max(1, SeenFootX[Index]) * Cell);
		const float HalfY = 0.5f * static_cast<float>(FMath::Max(1, SeenFootY[Index]) * Cell);
		const FVector Foot[2] = {
			FVector(CenterX - HalfX, CenterY - HalfY, GroundZ),
			FVector(CenterX + HalfX, CenterY + HalfY, GroundZ)
		};
		float BoxMinX = MAX_flt;
		float BoxMinY = MAX_flt;
		float BoxMaxX = -MAX_flt;
		float BoxMaxY = -MAX_flt;
		bool bProjected = false;
		for (const FVector& Corner : Foot)
		{
			FVector2D Screen;
			if (!PC->ProjectWorldLocationToScreen(Corner, Screen, true))
			{
				continue;
			}
			bProjected = true;
			BoxMinX = FMath::Min(BoxMinX, static_cast<float>(Screen.X));
			BoxMinY = FMath::Min(BoxMinY, static_cast<float>(Screen.Y));
			BoxMaxX = FMath::Max(BoxMaxX, static_cast<float>(Screen.X));
			BoxMaxY = FMath::Max(BoxMaxY, static_cast<float>(Screen.Y));
		}
		if (!bProjected || BoxMaxX <= BoxMinX || BoxMaxY <= BoxMinY)
		{
			continue;
		}
		const FLinearColor Ink = SeenTeam[Index] == LocalTeam
			? FLinearColor(FColor(0x59, 0x78, 0x8A, 140))
			: FLinearColor(FColor(0x8C, 0x5A, 0x5A, 140));
		DrawRect(Ink, BoxMinX, BoxMinY, BoxMaxX - BoxMinX, BoxMaxY - BoxMinY);
	}
}

void AKodHUD::DrawGameClock()
{
	if (!Canvas)
	{
		return;
	}
	const UWorld* World = GetWorld();
	const UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Sim)
	{
		return;
	}
	const int32 Seconds = static_cast<int32>(Sim->GetSimTickIndex() / 16);
	const int32 Minutes = Seconds / 60;
	const int32 Remain = Seconds % 60;
	const FString Label = FString::Printf(TEXT("%02d:%02d"), Minutes, Remain);
	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	if (!Font && GEngine)
	{
		Font = GEngine->GetSmallFont();
	}
	const FKodScreenRect Clock = MapSpec(208.f, 762.f, 80.f, 26.f);
	DrawPlate(Clock.X, Clock.Y, Clock.W, Clock.H, Tok(0x1F, 0x23, 0x27, 0.9f), TEXT("T_UI_VG_Chip_Resource"), 12.f, 8.f, 12.f, 8.f);
	DrawText(Label, Tok(0xB0, 0x8D, 0x57), Clock.X + 8.f * FrameScale, Clock.Y + 6.f * FrameScale, Font, FrameScale, false);
}

bool AKodHUD::ReadTeamBank(int32& OutJadeite, int32& OutLuminene) const
{
	OutJadeite = 0;
	OutLuminene = 0;
	const UWorld* World = GetWorld();
	const UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Sim)
	{
		return false;
	}

	int32 TeamId = 0;
	if (const APlayerController* PC = GetOwningPlayerController())
	{
		if (const AKodPlayerState* KodPS = PC->GetPlayerState<AKodPlayerState>())
		{
			TeamId = KodPS->TeamId;
		}
	}

	FKodResourceCost Bank;
	if (!Sim->TryGetBank(TeamId, Bank))
	{
		return false;
	}
	OutJadeite = Bank.Jadeite;
	OutLuminene = Bank.Luminene;
	return true;
}

void AKodHUD::RefreshReadout()
{
	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;
	const bool bDue = LastReadoutPollSeconds < 0.f || (Now - LastReadoutPollSeconds) >= 0.25f;
	if (bDue)
	{
		LastReadoutPollSeconds = Now;
		int32 Jadeite = 0;
		int32 Luminene = 0;
		ReadTeamBank(Jadeite, Luminene);
		int32 SupplyUsed = 0;
		int32 SupplyCap = 0;
		if (const UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr)
		{
			int32 TeamId = 0;
			if (const APlayerController* PC = GetOwningPlayerController())
			{
				if (const AKodPlayerState* KodPS = PC->GetPlayerState<AKodPlayerState>())
				{
					TeamId = KodPS->TeamId;
				}
			}
			Sim->TryGetSupply(TeamId, SupplyUsed, SupplyCap);
		}
		if (Jadeite != ShownJadeite || Luminene != ShownLuminene)
		{
			ShownJadeite = Jadeite;
			ShownLuminene = Luminene;
			UE_LOG(LogTemp, Log, TEXT("KodHUD Readout Update J=%d L=%d"), Jadeite, Luminene);
		}
		ShownSupplyUsed = SupplyUsed;
		ShownSupplyCap = SupplyCap;
	}

	const int32 DrawJadeite = ShownJadeite == MIN_int32 ? 0 : ShownJadeite;
	const int32 DrawLuminene = ShownLuminene == MIN_int32 ? 0 : ShownLuminene;
	DrawCanvasReadout(DrawJadeite, DrawLuminene, ShownSupplyUsed, ShownSupplyCap);
}

void AKodHUD::DrawCanvasReadout(int32 Jadeite, int32 Luminene, int32 SupplyUsed, int32 SupplyCap)
{
	if (!Canvas)
	{
		return;
	}

	UFont* Font = nullptr;
	if (GEngine)
	{
		Font = GEngine->GetMediumFont();
		if (!Font)
		{
			Font = GEngine->GetLargeFont();
		}
		if (!Font)
		{
			Font = GEngine->GetSmallFont();
		}
	}

	const FString Credits = FString::Printf(TEXT("%d"), Jadeite);
	const FString LumineneText = FString::Printf(TEXT("%d"), Luminene);
	bool bSupplyBlocked = SupplyCap > 0 && SupplyUsed >= SupplyCap;
	if (!bSupplyBlocked)
	{
		const int32 TeamId = ReadLocalTeamId();
		for (const FKodStructureState& Structure : FrameStructures)
		{
			if (Structure.TeamId == TeamId && Structure.bSupplyBlocked)
			{
				bSupplyBlocked = true;
				break;
			}
		}
	}
	const FString Supply = FString::Printf(TEXT("%d/%d"), SupplyUsed, SupplyCap);
	int32 Workers = 0;
	for (const FKodSimEntityState& State : FrameStates)
	{
		if (State.TeamId == ReadLocalTeamId() && State.bCanGather && State.Health > 0.f)
		{
			++Workers;
		}
	}
	const FString WorkerText = FString::Printf(TEXT("%d"), Workers);
	const float EdgeShift = SafeMaxX - (FrameOriginX + 1904.f * FrameScale);
	const float ChipX[4] = { 1376.f, 1514.f, 1652.f, 1790.f };
	const FString ChipText[4] = { WorkerText, Credits, LumineneText, Supply };
	const FLinearColor ChipInk[4] = {
		Tok(0xE6, 0xE6, 0xE0),
		HudSkin == 1 ? Tok(0xDF, 0xD4, 0xC0) : Tok(0xB0, 0x8D, 0x57),
		Tok(0xFF, 0xD2, 0x3F),
		bSupplyBlocked ? Tok(0xE5, 0x38, 0x3B) : Tok(0xE6, 0xE6, 0xE0)
	};
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const FKodScreenRect Chip = MapSpec(ChipX[Index], 0.f, 130.f, 28.f);
		const float X = Chip.X + EdgeShift;
		DrawPlate(X, Chip.Y, Chip.W, Chip.H, Tok(0x1F, 0x23, 0x27, 0.92f), TEXT("T_UI_VG_Chip_Resource"), 12.f, 8.f, 12.f, 8.f);
		DrawText(ChipText[Index], ChipInk[Index], X + 10.f * FrameScale, Chip.Y + 8.f * FrameScale, Font, FrameScale, false);
	}
}

void AKodHUD::DrawProductionPanel()
{
	// The five-slot queue is drawn in the selection panel.
}

void AKodHUD::DrawThreatRings()
{
	if (!Canvas)
	{
		return;
	}
	APlayerController* PC = GetOwningPlayerController();
	UWorld* World = GetWorld();
	const UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	const AKodPlayerController* KodPC = Cast<AKodPlayerController>(PC);
	if (!PC || !Sim || !KodPC)
	{
		return;
	}
	const int32 LocalTeam = ReadLocalTeamId();
	auto DrawRing = [this, PC](const FVector& Center, float Radius)
	{
		constexpr int32 Segments = 32;
		FVector2D Previous = FVector2D::ZeroVector;
		bool bHavePrevious = false;
		for (int32 Index = 0; Index <= Segments; ++Index)
		{
			const float Angle = (2.f * PI * static_cast<float>(Index)) / static_cast<float>(Segments);
			const FVector Point = Center + FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 40.f);
			FVector2D Screen = FVector2D::ZeroVector;
			if (!PC->ProjectWorldLocationToScreen(Point, Screen, true))
			{
				bHavePrevious = false;
				continue;
			}
			if (bHavePrevious)
			{
				DrawLine(Previous.X, Previous.Y, Screen.X, Screen.Y, FLinearColor(0.95f, 0.35f, 0.2f, 0.9f), 1.5f);
			}
			Previous = Screen;
			bHavePrevious = true;
		}
	};
	for (AActor* Actor : KodPC->GetLocalSelection())
	{
		if (!Actor)
		{
			continue;
		}
		const FKodEntityId Id = Sim->FindIdForActor(Actor);
		FKodStructureState Structure;
		if (!Sim->TryGetStructure(Id, Structure) || Structure.TeamId != LocalTeam || Structure.AttackRangeUU <= 0 || !Structure.bAttacksAir)
		{
			continue;
		}
		DrawRing(Structure.Position, static_cast<float>(Structure.AttackRangeUU));
	}
	FName PlaceType;
	FVector PlaceCenter = FVector::ZeroVector;
	if (KodPC->TryGetPlacementAim(PlaceType, PlaceCenter))
	{
		FKodBuildSpec Spec;
		if (Sim->TryGetBuildSpec(PlaceType, Spec) && Spec.bAttacksAir && Spec.AttackRangeUU > 0)
		{
			DrawRing(PlaceCenter, static_cast<float>(Spec.AttackRangeUU));
		}
	}
}

void AKodHUD::DrawIsrPopup()
{
	if (!Canvas)
	{
		return;
	}
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!World || !Sim || Sim->IsTestIsolation())
	{
		return;
	}
	FVector Center = FVector::ZeroVector;
	int32 Sight = 0;
	int32 IsrId = 0;
	if (!Sim->TryGetOwnedIsr(ReadLocalTeamId(), Center, Sight, IsrId))
	{
		if (IsrMonitor)
		{
			IsrMonitor->SetMonitorVisible(false);
		}
		return;
	}
	if (!IsrMonitor)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		IsrMonitor = World->SpawnActor<AKodIsrMonitor>(AKodIsrMonitor::StaticClass(), FTransform(Center), Params);
	}
	if (!IsrMonitor)
	{
		return;
	}
	IsrMonitor->SetMonitorVisible(true);
	IsrMonitor->AimAt(Center, Sight);
	IsrMonitor->CaptureView();
	const float W = 240.f;
	const float H = 168.f;
	const float X = Canvas->SizeX - W - 28.f;
	const float Y = 72.f;
	DrawRect(FLinearColor(0.02f, 0.03f, 0.02f, 0.92f), X - 6.f, Y - 22.f, W + 12.f, H + 30.f);
	UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;
	DrawText(TEXT("ISR"), Tok(0xB0, 0x8D, 0x57), X, Y - 18.f, Font, 1.f, false);
	if (UTextureRenderTarget2D* Target = IsrMonitor->GetRenderTarget())
	{
		DrawTexture(Target, X, Y, W, H, 0.f, 0.f, 1.f, 1.f);
	}
}

void AKodHUD::DrawBuildHint()
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
	FString Hint = PC->GetBuildMenuHint();
	const FName Armed = PC->GetArmedOrderName();
	if (!Armed.IsNone())
	{
		Hint = FString::Printf(TEXT("%s click    Shift queues"), *Armed.ToString());
	}
	if (Hint.IsEmpty())
	{
		return;
	}
	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	if (!Font && GEngine)
	{
		Font = GEngine->GetSmallFont();
	}
	const FKodScreenRect HintBox = MapSpec(402.f, 690.f, 480.f, 28.f);
	DrawRect(Tok(0x1F, 0x23, 0x27, 0.88f), HintBox.X, HintBox.Y, HintBox.W, HintBox.H);
	DrawText(Hint, Tok(0xB0, 0x8D, 0x57), HintBox.X + 8.f * FrameScale, HintBox.Y + 4.f * FrameScale, Font, FrameScale, false);
}

int32 AKodHUD::ReadLocalTeamId() const
{
	if (const APlayerController* PC = GetOwningPlayerController())
	{
		if (const AKodPlayerState* KodPS = PC->GetPlayerState<AKodPlayerState>())
		{
			return KodPS->TeamId;
		}
	}
	return 0;
}

FName AKodHUD::PeekResourceFail(int32 Credits, int32 Luminene, int32 Supply) const
{
	const UWorld* World = GetWorld();
	const UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Sim)
	{
		return NAME_None;
	}
	const int32 TeamId = ReadLocalTeamId();
	FKodResourceCost Bank;
	Sim->TryGetBank(TeamId, Bank);
	if (FMath::Max(0, Credits) > Bank.Jadeite)
	{
		return TEXT("Credits");
	}
	if (FMath::Max(0, Luminene) > Bank.Luminene)
	{
		return TEXT("Luminene");
	}
	if (Supply > 0)
	{
		int32 Used = 0;
		int32 Cap = 0;
		Sim->TryGetSupply(TeamId, Used, Cap);
		if (Used + Supply > Cap)
		{
			return TEXT("Supply");
		}
	}
	return NAME_None;
}

void AKodHUD::ShowCenterMessage(const FString& Text)
{
	FlashText = Text;
	FlashUntilSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() + 1.6f : 0.f;
}

void AKodHUD::SetSelectionPage(int32 Page)
{
	SelectionPage = FMath::Max(0, Page);
}

bool AKodHUD::ConsumePortraitClick()
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	const bool bDouble = PortraitClickSeconds >= 0.f && Now - PortraitClickSeconds < 0.3f;
	PortraitClickSeconds = bDouble ? -1.f : Now;
	return bDouble;
}

void AKodHUD::CancelPortraitClick()
{
	PortraitClickSeconds = -1.f;
}

float AKodHUD::WireZoneRemain(int32 UnitId, int32 ZoneIndex, int32 ZoneCount, int32 Health, int32 MaxHealth)
{
	const int32 Count = FMath::Max(1, ZoneCount);
	const int32 MaxH = FMath::Max(1, MaxHealth);
	const int32 Hp = FMath::Clamp(Health, 0, MaxH);
	const int32 Missing = MaxH - Hp;
	TArray<int32> Order;
	Order.Reserve(Count);
	for (int32 Zone = 0; Zone < Count; ++Zone)
	{
		Order.Add(Zone);
	}
	Order.Sort([UnitId](int32 A, int32 B)
	{
		const uint32 KeyA = (static_cast<uint32>(UnitId) * 2654435761u) ^ (static_cast<uint32>(A) * 2246822519u);
		const uint32 KeyB = (static_cast<uint32>(UnitId) * 2654435761u) ^ (static_cast<uint32>(B) * 2246822519u);
		return KeyA < KeyB;
	});
	int32 Rank = 0;
	for (int32 Index = 0; Index < Order.Num(); ++Index)
	{
		if (Order[Index] == ZoneIndex)
		{
			Rank = Index;
			break;
		}
	}
	const int32 Base = MaxH / Count;
	const int32 Extra = MaxH % Count;
	auto CapacityAt = [Base, Extra](int32 R)
	{
		return Base + (R < Extra ? 1 : 0);
	};
	int32 Before = 0;
	for (int32 R = 0; R < Rank; ++R)
	{
		Before += CapacityAt(R);
	}
	const int32 Capacity = FMath::Max(1, CapacityAt(Rank));
	const int32 Taken = FMath::Clamp(Missing - Before, 0, Capacity);
	return static_cast<float>(Capacity - Taken) / static_cast<float>(Capacity);
}

void AKodHUD::BindAlertFeed()
{
	if (UWorld* World = GetWorld())
	{
		if (UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>())
		{
			Sim->OnCue.AddDynamic(this, &AKodHUD::HandleAlertCue);
			Sim->OnUnitHit.AddDynamic(this, &AKodHUD::HandleWireHit);
			Sim->OnJuiceText.RemoveAll(this);
			Sim->OnJuiceText.AddUObject(this, &AKodHUD::HandleJuiceText);
		}
	}
}

void AKodHUD::HandleJuiceText(FVector Position, int32 Amount, uint8 Kind)
{
	FKodHudFloater Floater;
	Floater.World = Position;
	Floater.Amount = Amount;
	Floater.Kind = Kind;
	Floater.Born = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	JuiceFloaters.Add(Floater);
	while (JuiceFloaters.Num() > 48)
	{
		JuiceFloaters.RemoveAt(0);
	}
}

void AKodHUD::HandleWireHit(AActor* Attacker, AActor* Target, float Health, int32 TargetId)
{
	(void)Attacker;
	(void)Health;
	if (TargetId <= 0 || !GetWorld())
	{
		return;
	}
	WireHitUntil.Add(TargetId, GetWorld()->GetTimeSeconds() + 0.35f);
}

void AKodHUD::HandleAlertCue(FName CueId, FVector Location, bool bAtLocation)
{
	if (const UWorld* World = GetWorld())
	{
		if (const UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>())
		{
			if (Sim->IsTestIsolation())
			{
				return;
			}
		}
	}
	(void)bAtLocation;
	if (CueId == FName(TEXT("artillery_strike")) || CueId == FName(TEXT("building_destroyed")))
	{
		if (AKodRTSCameraPawn* CameraPawn = Cast<AKodRTSCameraPawn>(GetOwningPawn()))
		{
			const float Amp = CueId == FName(TEXT("artillery_strike")) ? 18.f : 14.f;
			CameraPawn->AddShakeAt(Location, Amp);
		}
	}
	if (CueId == FName(TEXT("structure_complete")))
	{
		AddCommandMarker(Location, 2);
	}
	if (PortraitStage && UKodCueSubsystem::IsVoiceCue(CueId))
	{
		PortraitStage->NotifyVoice(1.6f);
	}
	FString Text;
	if (CueId == FName(TEXT("base_under_attack")))
	{
		Text = TEXT("Your base is under attack");
	}
	else if (CueId == FName(TEXT("units_under_attack")))
	{
		Text = TEXT("Your forces are under attack");
	}
	else if (CueId == FName(TEXT("unit_ready_amp")) || CueId == FName(TEXT("unit_ready_marine")) || CueId == FName(TEXT("unit_ready_medic")))
	{
		Text = TEXT("Unit ready");
	}
	else if (CueId == FName(TEXT("structure_complete")))
	{
		Text = TEXT("Construction complete");
	}
	else if (CueId == FName(TEXT("research_complete")))
	{
		Text = TEXT("Research complete");
	}
	else if (CueId == FName(TEXT("not_enough_credits")))
	{
		Text = TEXT("Not enough Credits");
	}
	else if (CueId == FName(TEXT("not_enough_luminene")))
	{
		Text = TEXT("Not enough Luminene");
	}
	else if (CueId == FName(TEXT("not_enough_supply")))
	{
		Text = TEXT("Not enough supply");
	}
	else if (CueId == FName(TEXT("cant_build")))
	{
		Text = TEXT("Can't build here");
	}
	else if (CueId == FName(TEXT("depot_raise_blocked")))
	{
		Text = TEXT("Can't raise, units blocking");
	}
	else if (CueId == FName(TEXT("expand_or_be_exposed")))
	{
		Text = TEXT("Expand or be exposed");
	}
	else if (CueId == FName(TEXT("exposed")))
	{
		Text = TEXT("You are exposed");
	}
	else if (CueId == FName(TEXT("artillery_strike")))
	{
		Text = TEXT("Artillery strike");
	}
	else if (CueId == FName(TEXT("recon_sweep")))
	{
		Text = TEXT("Recon sweep");
	}
	else if (CueId == FName(TEXT("cheat_enabled")))
	{
		Text = TEXT("Cheat enabled");
		if (const UWorld* CueWorld = GetWorld())
		{
			if (const UKodSimSubsystem* CueSim = CueWorld->GetSubsystem<UKodSimSubsystem>())
			{
				const FString Named = CueSim->GetCheatFeedbackLine();
				if (!Named.IsEmpty())
				{
					Text = Named;
				}
			}
		}
	}
	else
	{
		return;
	}
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	const bool bAttack = CueId == FName(TEXT("base_under_attack")) || CueId == FName(TEXT("units_under_attack"));
	FKodAlertLine Line;
	Line.Text = Text;
	Line.CueId = CueId;
	Line.World = Location;
	Line.Until = Now + 5.f;
	AlertLines.Add(Line);
	while (AlertLines.Num() > 5)
	{
		AlertLines.RemoveAt(0);
	}
	if (!Location.IsNearlyZero())
	{
		AddMinimapPing(Location, bAttack ? 3.f : 6.f);
		if (AKodRTSCameraPawn* CameraPawn = Cast<AKodRTSCameraPawn>(GetOwningPawn()))
		{
			CameraPawn->NoteAlert(Location, bAttack ? 3.f : 4.f);
		}
	}
}

void AKodHUD::DrawWireHeat(float X, float Y, float W, float H, int32 UnitId, int32 Health, int32 MaxHealth, bool bStructure, bool bFlash)
{
	const int32 Zones = bStructure ? 4 : 6;
	auto ColorOf = [this, UnitId, Health, MaxHealth, Zones, bFlash](int32 Zone)
	{
		if (bFlash)
		{
			return FLinearColor(0.95f, 0.15f, 0.12f, 1.f);
		}
		const float Remain = WireZoneRemain(UnitId, Zone, Zones, Health, MaxHealth);
		if (Remain >= 0.75f)
		{
			return FLinearColor(0.25f, 0.85f, 0.3f, 1.f);
		}
		if (Remain >= 0.5f)
		{
			return FLinearColor(0.92f, 0.82f, 0.2f, 1.f);
		}
		if (Remain >= 0.25f)
		{
			return FLinearColor(0.95f, 0.5f, 0.12f, 1.f);
		}
		return FLinearColor(0.9f, 0.16f, 0.12f, 1.f);
	};
	auto Seg = [this, X, Y, W, H](float U0, float V0, float U1, float V1, const FLinearColor& Ink)
	{
		DrawLine(X + U0 * W, Y + V0 * H, X + U1 * W, Y + V1 * H, Ink, 2.f);
	};
	if (bStructure)
	{
		Seg(0.15f, 0.15f, 0.85f, 0.15f, ColorOf(0));
		Seg(0.85f, 0.15f, 0.85f, 0.85f, ColorOf(1));
		Seg(0.85f, 0.85f, 0.15f, 0.85f, ColorOf(2));
		Seg(0.15f, 0.85f, 0.15f, 0.15f, ColorOf(3));
		Seg(0.5f, 0.15f, 0.5f, 0.85f, ColorOf(0));
		Seg(0.15f, 0.5f, 0.85f, 0.5f, ColorOf(2));
		return;
	}
	Seg(0.5f, 0.08f, 0.5f, 0.22f, ColorOf(0));
	Seg(0.38f, 0.16f, 0.62f, 0.16f, ColorOf(0));
	Seg(0.5f, 0.22f, 0.5f, 0.55f, ColorOf(1));
	Seg(0.5f, 0.28f, 0.22f, 0.48f, ColorOf(2));
	Seg(0.5f, 0.28f, 0.78f, 0.48f, ColorOf(3));
	Seg(0.5f, 0.55f, 0.32f, 0.92f, ColorOf(4));
	Seg(0.5f, 0.55f, 0.68f, 0.92f, ColorOf(5));
}

void AKodHUD::DrawAlertFeed()
{
	if (!Canvas)
	{
		return;
	}
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	for (int32 Index = AlertLines.Num() - 1; Index >= 0; --Index)
	{
		if (AlertLines[Index].Until < Now)
		{
			AlertLines.RemoveAt(Index);
		}
	}
	const FKodScreenRect Feed = MapSpec(16.f, 420.f, 380.f, 292.f);
	const float EdgeShift = SafeMinX - FrameOriginX;
	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	const int32 MaxRows = 5;
	int32 Drawn = 0;
	for (int32 Index = AlertLines.Num() - 1; Index >= 0 && Drawn < MaxRows; --Index)
	{
		const FKodAlertLine& Line = AlertLines[Index];
		const float Alpha = FMath::Clamp((Line.Until - Now) / 1.5f, 0.25f, 1.f);
		const bool bCheatLine = Line.CueId == FName(TEXT("cheat_enabled"));
		const FString Glyph = Line.Text.Left(2).ToUpper();
		const float RowY = Feed.Y + static_cast<float>(MaxRows - 1 - Drawn) * (52.f + 8.f) * FrameScale;
		const float RowX = Feed.X + EdgeShift;
		const float RowW = Feed.W;
		const float RowH = 52.f * FrameScale;
		DrawRect(bCheatLine ? Tok(0x3A, 0x32, 0x14, 0.9f * Alpha) : Tok(0x1F, 0x23, 0x27, 0.88f * Alpha), RowX, RowY, RowW, RowH);
		DrawRect(bCheatLine ? Tok(0xB0, 0x8D, 0x57, Alpha) : Tok(0xE5, 0x38, 0x3B, Alpha), RowX + 6.f * FrameScale, RowY + 6.f * FrameScale, 40.f * FrameScale, 40.f * FrameScale);
		DrawText(Glyph, Tok(0xF2, 0xF2, 0xF2, Alpha), RowX + 12.f * FrameScale, RowY + 14.f * FrameScale, Font, 0.7f * FrameScale, false);
		DrawText(Line.Text, bCheatLine ? Tok(0xFF, 0xD2, 0x3F, Alpha) : Tok(0xE6, 0xE6, 0xE0, Alpha), RowX + 54.f * FrameScale, RowY + 14.f * FrameScale, Font, 0.85f * FrameScale, false);
		++Drawn;
	}
}

void AKodHUD::DrawSpectatorPanel()
{
	if (!Canvas || !Cast<AKodSpectatorPawn>(GetOwningPawn()))
	{
		return;
	}
	UWorld* World = GetWorld();
	const UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Sim)
	{
		return;
	}
	const FKodScreenRect Tab = MapSpec(16.f, 40.f, 560.f, 36.f);
	const float Scale = FrameScale;
	const float X = Tab.X + (SafeMinX - FrameOriginX);
	const float Y = Tab.Y;
	const float W = Tab.W;
	TArray<FKodSpecRow> Rows;
	for (int32 TeamId = 0; TeamId <= 1; ++TeamId)
	{
		FKodResourceCost Bank;
		Sim->TryGetBank(TeamId, Bank);
		int32 Used = 0;
		int32 Cap = 0;
		Sim->TryGetSupply(TeamId, Used, Cap);
		FKodSpecRow Row;
		Row.Label = FString::Printf(TEXT("T%d  %d  %d  %d/%d"), TeamId, Bank.Jadeite, Bank.Luminene, Used, Cap);
		Row.Cap = TeamId == 0 ? FLinearColor(FColor(0x59, 0x78, 0x8A)) : FLinearColor(FColor(0x8C, 0x5A, 0x5A));
		Rows.Add(Row);
	}
	int32 Shown = 0;
	for (const FKodStructureState& Structure : FrameStructures)
	{
		if (Shown >= 6)
		{
			break;
		}
		const bool bBuilding = !Structure.bComplete && Structure.BuildTimeTotal > 0;
		const bool bTrain = Structure.Slots.Num() > 0;
		const bool bResearch = Structure.Research.Num() > 0;
		if (!bBuilding && !bTrain && !bResearch)
		{
			continue;
		}
		FKodSpecRow Row;
		Row.Cap = Structure.TeamId == 0 ? FLinearColor(FColor(0x59, 0x78, 0x8A)) : FLinearColor(FColor(0x8C, 0x5A, 0x5A));
		FString Label = Structure.DisplayId.IsNone() ? Structure.DefinitionId.ToString() : Structure.DisplayId.ToString();
		if (bTrain)
		{
			Label = FString::Printf(TEXT("T%d %s"), Structure.TeamId, *Structure.Slots[0].UnitId.ToString());
			if (Structure.ActiveBuildTime > 0)
			{
				Row.Fraction = 1.f - static_cast<float>(Structure.ActiveTicksRemaining) / static_cast<float>(Structure.ActiveBuildTime);
			}
		}
		else if (bResearch)
		{
			Label = FString::Printf(TEXT("T%d %s"), Structure.TeamId, *Structure.Research[0].UnitId.ToString());
			if (Structure.ResearchBuildTime > 0)
			{
				Row.Fraction = 1.f - static_cast<float>(Structure.ResearchTicksRemaining) / static_cast<float>(Structure.ResearchBuildTime);
			}
		}
		else
		{
			Label = FString::Printf(TEXT("T%d %s"), Structure.TeamId, *Label);
			Row.Fraction = static_cast<float>(Structure.BuildTicksDone) / static_cast<float>(Structure.BuildTimeTotal);
		}
		Row.Fraction = FMath::Clamp(Row.Fraction, 0.f, 1.f);
		Row.Label = Label;
		Rows.Add(Row);
		++Shown;
	}
	const float PlateH = Tab.H * static_cast<float>(FMath::Max(1, Rows.Num()));
	DrawPlate(X, Y, W, PlateH, Tok(0x1F, 0x23, 0x27, 0.92f), TEXT("T_UI_VG_Modal_Panel"), 32.f, 32.f, 32.f, 32.f);
	UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;
	for (int32 Index = 0; Index < Rows.Num(); ++Index)
	{
		const FKodSpecRow& Row = Rows[Index];
		const float RowY = Y + Tab.H * static_cast<float>(Index);
		DrawRect(Row.Cap, X, RowY, 6.f * Scale, Tab.H);
		DrawText(Row.Label, Tok(0xE6, 0xE6, 0xE0), X + 12.f * Scale, RowY + 8.f * Scale, Font, 0.8f * Scale, false);
		if (Row.Fraction >= 0.f)
		{
			const float BarX = X + 220.f * Scale;
			const float BarW = FMath::Max(40.f, W - 236.f * Scale);
			DrawRect(Tok(0x3A, 0x3F, 0x45), BarX, RowY + 14.f * Scale, BarW, 8.f * Scale);
			DrawRect(Tok(0xFF, 0xD2, 0x3F), BarX, RowY + 14.f * Scale, BarW * Row.Fraction, 8.f * Scale);
		}
	}
}

void AKodHUD::ShowAlertLine(const FString& Text)
{
	if (Text.IsEmpty())
	{
		return;
	}
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	FKodAlertLine Line;
	Line.Text = Text;
	Line.CueId = FName();
	Line.Until = Now + 5.f;
	AlertLines.Add(Line);
	while (AlertLines.Num() > 5)
	{
		AlertLines.RemoveAt(0);
	}
}

void AKodHUD::ShowResourceFlash(FName Reason)
{
	FName CueId;
	if (Reason == TEXT("Credits"))
	{
		FlashText = TEXT("Not enough Credits");
		CueId = FName(TEXT("not_enough_credits"));
	}
	else if (Reason == TEXT("Luminene"))
	{
		FlashText = TEXT("Not enough Luminene");
		CueId = FName(TEXT("not_enough_luminene"));
	}
	else if (Reason == TEXT("Supply"))
	{
		FlashText = TEXT("Not enough supply");
		CueId = FName(TEXT("not_enough_supply"));
	}
	else
	{
		return;
	}
	FlashUntilSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() + 1.6f : 0.f;
	if (UWorld* World = GetWorld())
	{
		if (UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>())
		{
			Sim->RaiseCue(CueId, FVector::ZeroVector, false);
		}
	}
}

bool AKodHUD::TryDrawIcon(FName IconId, float X, float Y, float SizeX, float SizeY)
{
	if (IconId.IsNone() || IconMiss.Contains(IconId))
	{
		return false;
	}
	if (const TObjectPtr<UTexture2D>* Found = IconHits.Find(IconId))
	{
		if (*Found)
		{
			DrawTexture(*Found, X, Y, SizeX, SizeY, 0.f, 0.f, 1.f, 1.f);
			return true;
		}
	}
	const FString Path = FString::Printf(TEXT("/Game/UI/Icons/%s.%s"), *IconId.ToString(), *IconId.ToString());
	UTexture2D* Texture = Cast<UTexture2D>(FSoftObjectPath(Path).TryLoad());
	if (!Texture)
	{
		IconMiss.Add(IconId);
		return false;
	}
	IconHits.Add(IconId, Texture);
	DrawTexture(Texture, X, Y, SizeX, SizeY, 0.f, 0.f, 1.f, 1.f);
	return true;
}

void AKodHUD::DrawScaffolds()
{
	if (!Canvas)
	{
		return;
	}
	APlayerController* PC = GetOwningPlayerController();
	UWorld* World = GetWorld();
	const UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!PC || !Sim)
	{
		return;
	}
	for (const FKodStructureState& Structure : FrameStructures)
	{
		if (Structure.bComplete || Structure.BuildTimeTotal <= 0)
		{
			continue;
		}
		const float Alpha = FMath::Clamp(static_cast<float>(Structure.BuildTicksDone) / static_cast<float>(Structure.BuildTimeTotal), 0.f, 1.f);
		const float SpanX = static_cast<float>(FMath::Max(1, Structure.FootprintX)) * static_cast<float>(KodBuildGrid::CellUU);
		const float SpanY = static_cast<float>(FMath::Max(1, Structure.FootprintY)) * static_cast<float>(KodBuildGrid::CellUU);
		const FVector Center = Structure.Position;
		const FVector Corners[4] = {
			Center + FVector(-SpanX * 0.5f, -SpanY * 0.5f, 40.f * Alpha),
			Center + FVector(SpanX * 0.5f, -SpanY * 0.5f, 40.f * Alpha),
			Center + FVector(SpanX * 0.5f, SpanY * 0.5f, 40.f * Alpha),
			Center + FVector(-SpanX * 0.5f, SpanY * 0.5f, 40.f * Alpha)
		};
		FVector2D Screen[4];
		bool bAll = true;
		for (int32 Index = 0; Index < 4; ++Index)
		{
			if (!PC->ProjectWorldLocationToScreen(Corners[Index], Screen[Index], true))
			{
				bAll = false;
				break;
			}
		}
		if (!bAll)
		{
			continue;
		}
		const FLinearColor Frame = Tok(0xB0, 0x8D, 0x57, 0.35f + 0.4f * Alpha);
		DrawLine(Screen[0].X, Screen[0].Y, Screen[1].X, Screen[1].Y, Frame, 2.f);
		DrawLine(Screen[1].X, Screen[1].Y, Screen[2].X, Screen[2].Y, Frame, 2.f);
		DrawLine(Screen[2].X, Screen[2].Y, Screen[3].X, Screen[3].Y, Frame, 2.f);
		DrawLine(Screen[3].X, Screen[3].Y, Screen[0].X, Screen[0].Y, Frame, 2.f);
	}
}

bool AKodHUD::GetCommandHit(float MouseX, float MouseY, FKodHudCommandHit& Out) const
{
	for (const FKodHudCommandHit& Hit : CommandHits)
	{
		if (MouseX >= Hit.MinX && MouseX <= Hit.MaxX && MouseY >= Hit.MinY && MouseY <= Hit.MaxY)
		{
			Out = Hit;
			Out.bInsideChrome = true;
			return true;
		}
	}
	if (bCommandChrome && MouseX >= ChromeMinX && MouseX <= ChromeMaxX && MouseY >= ChromeMinY && MouseY <= ChromeMaxY)
	{
		Out = FKodHudCommandHit();
		Out.bInsideChrome = true;
		return true;
	}
	return false;
}

void AKodHUD::DrawPowersPanel()
{
	bHasPowerPanel = false;
	if (!Canvas)
	{
		return;
	}
	const UWorld* World = GetWorld();
	const UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Sim || !Sim->AreGeneralPowersEnabled() || Sim->IsTestIsolation())
	{
		return;
	}
	const int32 TeamId = ReadLocalTeamId();
	UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;
	const float EdgeShift = SafeMaxX - (FrameOriginX + 1904.f * FrameScale);
	const FKodScreenRect Bar = MapSpec(1500.f, 60.f, 404.f, 6.f);
	const float X = Bar.X + EdgeShift;
	const float Y = Bar.Y;
	const float W = Bar.W;
	const float H = Bar.H;
	const AKodPlayerController* Player = Cast<AKodPlayerController>(GetOwningPlayerController());
	const FName ArmedPower = Player ? Player->GetArmedOrderName() : FName();
	PowerMinX = X;
	PowerMinY = Y;
	PowerMaxX = X + W;
	PowerMaxY = Y + H;
	bHasPowerPanel = true;
	const int32 Points = Sim->GetPowerPoints(TeamId);
	const float PointFrac = FMath::Clamp(static_cast<float>(Points) / 200.f, 0.f, 1.f);
	DrawRect(Tok(0x1F, 0x23, 0x27), X, Y, W, H);
	DrawRect(Tok(0x2F, 0x7B, 0xFF), X, Y, W * PointFrac, H);
	const float ButtonTop = Y + H + 4.f * FrameScale;
	const FName Ids[3] = {
		FName(KodWardenPaths::Id_ArtilleryStrike),
		FName(KodWardenPaths::Id_ReconSweep),
		FName(KodWardenPaths::Id_FieldRepair)
	};
	const TCHAR* Labels[3] = { TEXT("Artillery"), TEXT("Recon"), TEXT("Repair") };
	const TCHAR* Keys[3] = { TEXT("K"), TEXT("O"), TEXT("X") };
	for (int32 Index = 0; Index < 3; ++Index)
	{
		FKodPowerSpec Spec;
		const bool bHave = Sim->TryGetPowerSpec(Ids[Index], Spec);
		const int32 Cooldown = Sim->GetPowerCooldown(TeamId, Ids[Index]);
		const int32 Cost = bHave ? Spec.PointCost : 0;
		const bool bArmed = ArmedPower == Ids[Index];
		FName Fail = FName();
		if (!bHave)
		{
			Fail = FName(TEXT("Energy"));
		}
		else if (Cooldown > 0)
		{
			Fail = FName(TEXT("Cooldown"));
		}
		else if (Sim->GetPowerPoints(TeamId) < Cost)
		{
			Fail = FName(TEXT("Points"));
		}
		const bool bReady = Fail.IsNone();
		const float ButtonY = ButtonTop + Index * 28.f * FrameScale;
		const FLinearColor Fill = bArmed
			? Tok(0xB0, 0x8D, 0x57)
			: (bReady ? Tok(0x5C, 0x64, 0x6C) : Tok(0x3A, 0x3F, 0x45));
		DrawRect(Fill, X + 8.f * FrameScale, ButtonY, W - 16.f * FrameScale, 24.f * FrameScale);
		const FString Line = FString::Printf(TEXT("%s %s  %d  %ds"), Keys[Index], Labels[Index], Cost, FMath::Max(0, Cooldown) / 16);
		DrawText(Line, bReady ? FLinearColor::White : FLinearColor(0.6f, 0.6f, 0.6f, 1.f), X + 14.f, ButtonY + 6.f, Font, 0.85f, false);
		FKodHudCommandHit Hit;
		Hit.bInsideChrome = true;
		Hit.bEnabled = bReady;
		Hit.Action = FName(TEXT("Power"));
		Hit.Payload = Ids[Index];
		Hit.FailReason = Fail;
		Hit.MinX = X + 8.f;
		Hit.MinY = ButtonY;
		Hit.MaxX = X + W - 8.f;
		Hit.MaxY = ButtonY + 24.f * FrameScale;
		if (Fail == FName(TEXT("Cooldown")))
		{
			Hit.Tooltip = TEXT("Cooldown");
		}
		else if (Fail == FName(TEXT("Points")))
		{
			Hit.Tooltip = FString::Printf(TEXT("Need %d points"), Cost);
		}
		else
		{
			Hit.Tooltip = Labels[Index];
		}
		CommandHits.Add(Hit);
	}
	PowerMaxY = ButtonTop + 3.f * 28.f * FrameScale;
	TArray<int32> StrikeX;
	TArray<int32> StrikeY;
	TArray<int32> StrikeRadius;
	Sim->CopyArtilleryTelegraphs(TeamId, StrikeX, StrikeY, StrikeRadius);
	APlayerController* PC = GetOwningPlayerController();
	if (!PC)
	{
		return;
	}
	for (int32 Index = 0; Index < StrikeX.Num(); ++Index)
	{
		const FVector Center(static_cast<float>(StrikeX[Index]), static_cast<float>(StrikeY[Index]), 40.f);
		const float Radius = static_cast<float>(StrikeRadius[Index]);
		constexpr int32 Segments = 24;
		FVector2D Previous = FVector2D::ZeroVector;
		bool bHavePrevious = false;
		for (int32 Step = 0; Step <= Segments; ++Step)
		{
			const float Angle = (2.f * PI * static_cast<float>(Step)) / static_cast<float>(Segments);
			FVector2D Screen = FVector2D::ZeroVector;
			const FVector Point = Center + FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.f);
			if (!PC->ProjectWorldLocationToScreen(Point, Screen, true))
			{
				bHavePrevious = false;
				continue;
			}
			if (bHavePrevious)
			{
				DrawLine(Previous.X, Previous.Y, Screen.X, Screen.Y, FLinearColor(1.f, 0.45f, 0.15f, 0.95f), 2.f);
			}
			Previous = Screen;
			bHavePrevious = true;
		}
	}
	TArray<int32> ReconX;
	TArray<int32> ReconY;
	TArray<int32> ReconRadius;
	TArray<int32> ReconLeft;
	TArray<int32> ReconDuration;
	Sim->CopyReconPulses(TeamId, ReconX, ReconY, ReconRadius, ReconLeft, ReconDuration);
	for (int32 Index = 0; Index < ReconX.Num(); ++Index)
	{
		const float Span = static_cast<float>(FMath::Max(1, ReconDuration[Index]));
		const float Grown = 1.f - FMath::Clamp(static_cast<float>(ReconLeft[Index]) / Span, 0.f, 1.f);
		const FVector Center(static_cast<float>(ReconX[Index]), static_cast<float>(ReconY[Index]), 40.f);
		const float Radius = static_cast<float>(ReconRadius[Index]) * FMath::Max(0.08f, Grown);
		constexpr int32 Segments = 24;
		FVector2D Previous = FVector2D::ZeroVector;
		bool bHavePrevious = false;
		for (int32 Step = 0; Step <= Segments; ++Step)
		{
			const float Angle = (2.f * PI * static_cast<float>(Step)) / static_cast<float>(Segments);
			FVector2D Screen = FVector2D::ZeroVector;
			const FVector Point = Center + FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.f);
			if (!PC->ProjectWorldLocationToScreen(Point, Screen, true))
			{
				bHavePrevious = false;
				continue;
			}
			if (bHavePrevious)
			{
				DrawLine(Previous.X, Previous.Y, Screen.X, Screen.Y, FLinearColor(0.35f, 0.85f, 1.f, 0.9f), 2.f);
			}
			Previous = Screen;
			bHavePrevious = true;
		}
	}
}

void AKodHUD::DrawCommandCard()
{
	CommandHits.Reset();
	bCommandChrome = false;
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

	TArray<FName> SubgroupIds;
	auto SubgroupOf = [Sim](AActor* Actor) -> FName
	{
		if (!Actor || !Sim)
		{
			return FName();
		}
		const FKodEntityId Id = Sim->FindIdForActor(Actor);
		FKodStructureState Structure;
		if (Sim->TryGetStructure(Id, Structure))
		{
			return Structure.DefinitionId.IsNone() ? FName() : Structure.DefinitionId;
		}
		FKodSimEntityState State;
		if (Sim->TryGetState(Id, State))
		{
			return State.DefinitionId.IsNone() ? FName() : State.DefinitionId;
		}
		return FName();
	};
	for (AActor* Actor : PC->GetLocalSelection())
	{
		const FName Subgroup = SubgroupOf(Actor);
		if (!Subgroup.IsNone() && !SubgroupIds.Contains(Subgroup))
		{
			SubgroupIds.Add(Subgroup);
		}
	}
	SubgroupIds.Sort([](const FName& A, const FName& B)
	{
		return A.ToString() < B.ToString();
	});
	CommandSubgroupCount = SubgroupIds.Num();
	const int32 SubgroupIndex = CommandSubgroupCount > 0 ? PC->GetCommandSubgroupIndex() % CommandSubgroupCount : 0;
	ActiveCommandSubgroup = CommandSubgroupCount > 0 ? SubgroupIds[SubgroupIndex] : FName();

	const FKodStructureState* Trainer = nullptr;
	FKodStructureState TrainerState;
	bool bWorker = false;
	bool bMarine = false;
	bool bMedic = false;
	bool bInfantry = false;
	FKodStructureState SelectedBuilding;
	bool bHaveBuilding = false;
	bool bStimReady = false;
	int32 StimRemaining = 0;
	int32 StimDuration = 1;
	for (AActor* Actor : PC->GetLocalSelection())
	{
		if (!Actor)
		{
			continue;
		}
		if (CommandSubgroupCount > 1 && SubgroupOf(Actor) != ActiveCommandSubgroup)
		{
			continue;
		}
		const FKodEntityId Id = Sim->FindIdForActor(Actor);
		FKodStructureState Structure;
		if (!Trainer && Sim->TryGetStructure(Id, Structure) && Structure.bComplete && Structure.Trains.Num() > 0)
		{
			TrainerState = Structure;
			Trainer = &TrainerState;
		}
		if (!bHaveBuilding && Sim->TryGetStructure(Id, Structure) && Structure.bComplete)
		{
			SelectedBuilding = Structure;
			bHaveBuilding = true;
		}
		FKodSimEntityState State;
		if (!Sim->TryGetState(Id, State) || State.Health <= 0.f)
		{
			continue;
		}
		if (State.bCanGather)
		{
			bWorker = true;
		}
		if (State.bHealer)
		{
			bMedic = true;
		}
		if (State.bInfantry && State.CargoSize > 0)
		{
			bInfantry = true;
		}
		if (Sim->UnitHasAbility(State.DefinitionId, FName(KodWardenPaths::Id_CombatStim)))
		{
			bMarine = true;
			StimRemaining = FMath::Max(StimRemaining, State.AbilityTicksRemaining);
			FKodAbilitySpec Ability;
			if (Sim->TryGetAbilitySpec(FName(KodWardenPaths::Id_CombatStim), Ability))
			{
				StimDuration = FMath::Max(1, Ability.DurationTicks);
				if (State.AbilityCooldownTicks <= 0 && State.Health > static_cast<float>(Ability.HealthCost))
				{
					bStimReady = true;
				}
			}
		}
	}

	const bool bBuildMenu = PC->IsWorkerBuildMenuOpen();
	const bool bTrainCard = Trainer != nullptr;
	const bool bBuildCard = !bTrainCard && bWorker;
	const bool bMarineCard = !bTrainCard && bMarine;
	const bool bBay = bHaveBuilding && SelectedBuilding.DefinitionId == FName(KodWardenPaths::Id_EngineeringBay);
	const bool bDepot = bHaveBuilding && SelectedBuilding.bCanLower;
	const bool bBunker = bHaveBuilding && SelectedBuilding.CargoCap > 0;
	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	if (!Font && GEngine)
	{
		Font = GEngine->GetSmallFont();
	}

	const float Button = 76.f * FrameScale;
	const float Gap = 6.f * FrameScale;
	constexpr int32 Cols = 5;
	constexpr int32 Rows = 3;
	const FKodScreenRect Card = MapSpec(1507.f, 826.f, 404.f, 240.f);
	const float CardW = Card.W;
	const float CardH = Card.H;
	const float QueueH = 0.f;
	const float OriginX = Card.X;
	const float OriginY = Card.Y;
	ChromeMinX = OriginX - 8.f;
	ChromeMinY = OriginY;
	ChromeMaxX = OriginX + CardW + 8.f;
	ChromeMaxY = OriginY + CardH + QueueH + 8.f;
	bCommandChrome = true;
	DrawRect(Tok(0x1F, 0x23, 0x27, 0.9f), ChromeMinX, ChromeMinY, ChromeMaxX - ChromeMinX, ChromeMaxY - ChromeMinY);

	auto AddButton = [this, Font, Button, Gap](int32 Col, int32 Row, float OriginXIn, float OriginYIn, const FString& Label, const FString& Hotkey, FName Action, FName Payload, int32 StructureId, int32 SlotIndex, bool bEnabled, FName Fail, const FString& Tooltip)
	{
		const float X = OriginXIn + Col * (Button + Gap);
		const float Y = OriginYIn + Row * (Button + Gap);
		const bool bRequires = Fail == FName(TEXT("Requires"));
		const FLinearColor Plate = bEnabled
			? Tok(0x5C, 0x64, 0x6C)
			: (bRequires ? Tok(0x3A, 0x3F, 0x45) : Tok(0x2A, 0x2E, 0x32, 0.85f));
		const FLinearColor Ink = bEnabled
			? FLinearColor::White
			: (bRequires ? FLinearColor(0.45f, 0.45f, 0.45f, 1.f) : FLinearColor(0.62f, 0.66f, 0.6f, 0.8f));
		DrawRect(Plate, X, Y, Button, Button);
		if (!TryDrawIcon(Payload.IsNone() ? FName(*Label) : Payload, X + 4.f, Y + 4.f, 18.f, 18.f))
		{
			const FString Glyph = Label.Left(2).ToUpper();
			DrawRect(Tok(0x5C, 0x64, 0x6C), X + 4.f, Y + 4.f, 18.f, 18.f);
			DrawText(Glyph, FLinearColor::White, X + 5.f, Y + 5.f, Font, 0.55f, false);
		}
		DrawText(Label, Ink, X + 6.f, Y + 24.f, Font, 0.65f, false);
		const FLinearColor HotkeyInk = HudSkin == 1 ? Tok(0xDF, 0xD4, 0xC0) : Tok(0xFF, 0xD2, 0x3F);
		DrawText(Hotkey, bEnabled ? HotkeyInk : Tok(0x8E, 0x94, 0x9A, 0.6f), X + 4.f * FrameScale, Y + 2.f * FrameScale, Font, 0.75f * FrameScale, false);
		FKodHudCommandHit Hit;
		Hit.bInsideChrome = true;
		Hit.bEnabled = bEnabled;
		Hit.Action = Action;
		Hit.Payload = Payload;
		Hit.SlotIndex = SlotIndex;
		Hit.StructureId = StructureId;
		Hit.FailReason = Fail;
		Hit.MinX = X;
		Hit.MinY = Y;
		Hit.MaxX = X + Button;
		Hit.MaxY = Y + Button;
		Hit.Tooltip = Tooltip;
		CommandHits.Add(Hit);
	};

	if (bTrainCard)
	{
		int32 Shown = 0;
		for (const FName& UnitId : Trainer->Trains)
		{
			if (Shown >= Cols)
			{
				break;
			}
			FKodTrainSpec Spec;
			const bool bHaveSpec = Sim->TryGetTrainSpec(UnitId, Spec);
			const FString Label = PlayerFacingName(Sim, (bHaveSpec && !Spec.DisplayId.IsNone()) ? Spec.DisplayId : UnitId).ToString();
			const int32 Credits = bHaveSpec ? Spec.Cost.Credits : 0;
			const int32 Luminene = bHaveSpec ? Spec.Cost.Luminene : 0;
			const int32 Supply = bHaveSpec ? Spec.Cost.Supply : 0;
			const int32 Ticks = bHaveSpec ? Spec.Cost.BuildTime : 0;
			FName Fail = bHaveSpec ? PeekResourceFail(Credits, Luminene, Supply) : FName();
			FString RequireLine;
			if (bHaveSpec && !Spec.RequiresId.IsNone() && !Sim->HasCompletedStructure(ReadLocalTeamId(), Spec.RequiresId))
			{
				Fail = FName(TEXT("Requires"));
				RequireLine = FString::Printf(TEXT("\nRequires %s"), *Spec.RequiresId.ToString());
			}
			if (Fail.IsNone() && Trainer->bSupplyBlocked)
			{
				Fail = TEXT("Supply");
			}
			if (Fail.IsNone() && Trainer->Slots.Num() >= KodEconomyDefaults::ProductionSlotCap)
			{
				Fail = TEXT("Full");
			}
			const int32 Seconds = FMath::Max(0, Ticks) / 16;
			const FString Tooltip = FString::Printf(TEXT("%s\n%d Credits\n%d Luminene\n%d supply\n%ds%s"),
				*Label, Credits, Luminene, Supply, Seconds, *RequireLine);
			AddButton(Shown, 0, OriginX, OriginY, Label, TEXT("S"), TEXT("Train"), UnitId, Trainer->Id.Value, INDEX_NONE, Fail.IsNone(), Fail, Tooltip);
			++Shown;
		}
	}
	else if (bMarineCard || bWorker)
	{
		AddButton(0, 0, OriginX, OriginY, TEXT("Move"), TEXT("Q"), TEXT("Move"), FName(), 0, INDEX_NONE, true, FName(), TEXT("Move"));
		AddButton(1, 0, OriginX, OriginY, TEXT("Stop"), TEXT("W"), TEXT("Stop"), FName(), 0, INDEX_NONE, true, FName(), TEXT("Stop"));
		AddButton(2, 0, OriginX, OriginY, TEXT("Hold"), TEXT("E"), TEXT("Hold"), FName(), 0, INDEX_NONE, true, FName(), TEXT("Hold"));
		AddButton(3, 0, OriginX, OriginY, TEXT("Patrol"), TEXT("R"), TEXT("Patrol"), FName(), 0, INDEX_NONE, true, FName(), TEXT("Patrol"));
		AddButton(4, 0, OriginX, OriginY, TEXT("Attack"), TEXT("T"), TEXT("Attack"), FName(), 0, INDEX_NONE, true, FName(), TEXT("Attack"));
		AddButton(4, 2, OriginX, OriginY, TEXT("Guard"), TEXT("G"), TEXT("Guard"), FName(), 0, INDEX_NONE, true, FName(), TEXT("Guard"));
		if (bMarineCard)
		{
			FKodAbilitySpec Ability;
			const bool bHave = Sim->TryGetAbilitySpec(FName(KodWardenPaths::Id_CombatStim), Ability);
			const FString Label = (bHave && !Ability.DisplayName.IsNone()) ? Ability.DisplayName.ToString() : TEXT("Stim");
			const FString Hotkey = (bHave && !Ability.Hotkey.IsNone()) ? Ability.Hotkey.ToString() : TEXT("T");
			const int32 Seconds = (bHave ? Ability.DurationTicks : StimDuration) / 16;
			const FString Tooltip = FString::Printf(TEXT("%s\n%d HP\n%ds\n+50%% speed"), *Label, bHave ? Ability.HealthCost : 10, Seconds);
			const FName Fail = bStimReady ? FName() : FName(TEXT("Health"));
			AddButton(0, 1, OriginX, OriginY, Label, Hotkey, TEXT("Ability"), FName(KodWardenPaths::Id_CombatStim), 0, INDEX_NONE, bStimReady, Fail, Tooltip);
			if (StimRemaining > 0)
			{
				const float Fraction = FMath::Clamp(static_cast<float>(StimRemaining) / static_cast<float>(StimDuration), 0.f, 1.f);
				const float X = OriginX;
				const float Y = OriginY + Button + Gap;
				DrawRect(Tok(0xFF, 0x7A, 0x1A, 0.45f), X, Y, Button, Button * Fraction);
			}
		}
		if (bWorker && !bBuildMenu)
		{
			AddButton(0, 2, OriginX, OriginY, TEXT("Build"), TEXT("B"), TEXT("BuildMenu"), NAME_None, 0, INDEX_NONE, true, NAME_None, TEXT("Build"));
		}
		else if (bWorker)
		{
			const int32 Tier = PC->GetBuildMenuTier();
			const FName BasicTypes[] = {
				FName(KodWardenPaths::Id_CommandCenter),
				FName(KodWardenPaths::Id_SupplyDepot),
				FName(KodWardenPaths::Id_Barracks),
				FName(KodWardenPaths::Id_Rig),
				FName(KodWardenPaths::Id_EngineeringBay),
				FName(KodWardenPaths::Id_Bunker)
			};
			const TCHAR* BasicKeys[] = { TEXT("C"), TEXT("D"), TEXT("R"), TEXT("E"), TEXT("Y"), TEXT("N") };
			const TCHAR* BasicLabels[] = { TEXT("CC"), TEXT("Depot"), TEXT("Barracks"), TEXT("Rig"), TEXT("Bay"), TEXT("Bunker") };
			const FName AdvancedTypes[] = { FName(KodWardenPaths::Id_Turret) };
			const TCHAR* AdvancedKeys[] = { TEXT("T") };
			const TCHAR* AdvancedLabels[] = { TEXT("Turret") };
			const int32 TypeCount = Tier == 2 ? 1 : 6;
			const int32 FirstCol = bMarineCard ? 1 : 0;
			for (int32 Index = 0; Index < TypeCount; ++Index)
			{
				const FName TypeId = Tier == 2 ? AdvancedTypes[Index] : BasicTypes[Index];
				const FString Label = Tier == 2 ? AdvancedLabels[Index] : BasicLabels[Index];
				const FString Hotkey = Tier == 2 ? AdvancedKeys[Index] : BasicKeys[Index];
				FKodBuildSpec Spec;
				const bool bHave = Sim->TryGetBuildSpec(TypeId, Spec);
				const int32 Credits = bHave ? Spec.Cost.Credits : 0;
				const int32 Luminene = bHave ? Spec.Cost.Luminene : 0;
				const int32 Ticks = bHave ? Spec.Cost.BuildTime : 0;
				FName Fail = bHave ? PeekResourceFail(Credits, Luminene, 0) : FName();
				FString RequireLine;
				if (bHave && !Spec.RequiresId.IsNone() && !Sim->HasCompletedStructure(ReadLocalTeamId(), Spec.RequiresId))
				{
					Fail = FName(TEXT("Requires"));
					RequireLine = FString::Printf(TEXT("\nRequires %s"), *Spec.RequiresId.ToString());
				}
				const FString Tooltip = FString::Printf(TEXT("%s\n%d Credits\n%d Luminene\n%ds%s"),
					*Label, Credits, Luminene, FMath::Max(0, Ticks) / 16, *RequireLine);
				const int32 Col = (FirstCol + Index) % Cols;
				const int32 Row = (FirstCol + Index) / Cols;
				AddButton(Col, Row, OriginX, OriginY, Label, Hotkey, TEXT("Place"), TypeId, 0, INDEX_NONE, Fail.IsNone(), Fail, Tooltip);
			}
			if (Tier == 2)
			{
				AddButton(2, 0, OriginX, OriginY, TEXT("Soon"), TEXT(""), TEXT("Place"), FName(), 0, INDEX_NONE, false, FName(TEXT("Requires")), TEXT("Requires a later tech"));
				AddButton(3, 0, OriginX, OriginY, TEXT("Soon"), TEXT(""), TEXT("Place"), FName(), 0, INDEX_NONE, false, FName(TEXT("Requires")), TEXT("Requires a later tech"));
			}
		}
	}

	if (bMedic && !bTrainCard)
	{
		AddButton(1, 0, OriginX, OriginY, TEXT("Heal"), TEXT("C"), TEXT("Heal"), FName(TEXT("Heal")), 0, INDEX_NONE, true, FName(), TEXT("Heal\nRight-click toggles autocast"));
	}
	if (bInfantry && !bTrainCard && !bBuildCard)
	{
		AddButton(2, 0, OriginX, OriginY, TEXT("Load"), TEXT("L"), TEXT("Load"), FName(), 0, INDEX_NONE, true, FName(), TEXT("Load into a bunker"));
	}
	if (bDepot && !bTrainCard)
	{
		const FString Label = SelectedBuilding.bLowered ? TEXT("Raise") : TEXT("Lower");
		const int32 DepotRow = (bMarineCard || bWorker) ? 2 : 0;
		AddButton(0, DepotRow, OriginX, OriginY, Label, TEXT("R"), TEXT("Depot"), FName(), SelectedBuilding.Id.Value, INDEX_NONE, true, FName(), Label);
	}
	if (bBay && !bTrainCard)
	{
		const FName Upgrades[2] = { FName(KodWardenPaths::Id_InfantryWeapons), FName(KodWardenPaths::Id_InfantryArmor) };
		const TCHAR* Labels[2] = { TEXT("Weapons"), TEXT("Armor") };
		for (int32 Index = 0; Index < 2; ++Index)
		{
			FKodUpgradeSpec Spec;
			const bool bHave = Sim->TryGetUpgradeSpec(Upgrades[Index], Spec);
			const int32 Credits = bHave ? Spec.Credits : 0;
			const int32 Luminene = bHave ? Spec.Luminene : 0;
			FName Fail = bHave ? PeekResourceFail(Credits, Luminene, 0) : FName();
			const FString Tooltip = FString::Printf(TEXT("%s\n%d Credits\n%d Luminene\n%ds"),
				Labels[Index], Credits, Luminene, (bHave ? Spec.BuildTicks : 0) / 16);
			AddButton(Index, 0, OriginX, OriginY, Labels[Index], Index == 0 ? TEXT("W") : TEXT("A"), TEXT("Research"), Upgrades[Index], SelectedBuilding.Id.Value, INDEX_NONE, Fail.IsNone(), Fail, Tooltip);
		}
	}
	if (bBunker && !bTrainCard)
	{
		AddButton(0, 0, OriginX, OriginY, TEXT("Unload"), TEXT("U"), TEXT("UnloadAll"), FName(), SelectedBuilding.Id.Value, INDEX_NONE, SelectedBuilding.Cargo.Num() > 0, FName(), TEXT("Unload all"));
	}
	const bool bCc = bHaveBuilding && SelectedBuilding.DefinitionId == FName(KodWardenPaths::Id_CommandCenter);
	if (bCc)
	{
		FKodSimEntityState CcState;
		const bool bHaveCcState = Sim->TryGetState(SelectedBuilding.Id, CcState);
		const bool bSatellite = bHaveCcState && CcState.EnergyMax > 0;
		if (!bSatellite)
		{
			FKodUpgradeSpec FeedSpec;
			const bool bHaveFeed = Sim->TryGetUpgradeSpec(FName(KodWardenPaths::Id_SatelliteFeed), FeedSpec);
			FName Fail = bHaveFeed ? PeekResourceFail(FeedSpec.Credits, FeedSpec.Luminene, 0) : FName();
			const FString Tooltip = FString::Printf(TEXT("Satellite Feed\n%d Credits\n%d Luminene\n%ds"),
				bHaveFeed ? FeedSpec.Credits : 0,
				bHaveFeed ? FeedSpec.Luminene : 0,
				(bHaveFeed ? FeedSpec.BuildTicks : 0) / 16);
			AddButton(0, 2, OriginX, OriginY, TEXT("Feed"), TEXT("F"), TEXT("Research"), FName(KodWardenPaths::Id_SatelliteFeed), SelectedBuilding.Id.Value, INDEX_NONE, Fail.IsNone(), Fail, Tooltip);
		}
		else
		{
			auto OrbitalButton = [&](int32 Col, const TCHAR* Label, const TCHAR* Hotkey, FName AbilityId, int32 Cooldown)
			{
				FKodAbilitySpec Ability;
				const bool bHaveAbility = Sim->TryGetAbilitySpec(AbilityId, Ability);
				const int32 Cost = bHaveAbility ? Ability.EnergyCost : 0;
				const bool bReady = bHaveCcState && CcState.Energy >= Cost && Cooldown <= 0;
				const FName Fail = bReady ? FName() : FName(TEXT("Energy"));
				const FString Tooltip = FString::Printf(TEXT("%s\n%d energy"), Label, Cost);
				AddButton(Col, 2, OriginX, OriginY, Label, Hotkey, TEXT("Orbital"), AbilityId, SelectedBuilding.Id.Value, INDEX_NONE, bReady, Fail, Tooltip);
			};
			OrbitalButton(0, TEXT("Gatherer"), TEXT("N"), FName(KodWardenPaths::Id_OrbitalGathererDrop), SelectedBuilding.GathererCd);
			OrbitalButton(1, TEXT("Supply"), TEXT("Y"), FName(KodWardenPaths::Id_SupplyBoost), SelectedBuilding.SupplyCd);
			OrbitalButton(2, TEXT("ISR"), TEXT("I"), FName(KodWardenPaths::Id_IsrView), SelectedBuilding.IsrCd);
			if (bHaveCcState)
			{
				DrawText(FString::Printf(TEXT("Energy %d/%d"), CcState.Energy, CcState.EnergyMax), FLinearColor(0.45f, 0.75f, 1.f, 1.f), OriginX, OriginY - 22.f, Font, 0.75f, false);
			}
		}
	}
	const bool bLiftoff = bHaveBuilding && (SelectedBuilding.DefinitionId == FName(KodWardenPaths::Id_CommandCenter)
		|| SelectedBuilding.DefinitionId == FName(KodWardenPaths::Id_Barracks)
		|| SelectedBuilding.DefinitionId == FName(KodWardenPaths::Id_EngineeringBay));
	if (bLiftoff && SelectedBuilding.bComplete)
	{
		const FString Label = SelectedBuilding.bAirborne ? TEXT("Land") : TEXT("Lift");
		AddButton(4, 2, OriginX, OriginY, Label, TEXT("Z"), FName(TEXT("Liftoff")), FName(), SelectedBuilding.Id.Value, INDEX_NONE, true, FName(), Label);
	}

	float MouseX = 0.f;
	float MouseY = 0.f;
	if (const APlayerController* Owning = GetOwningPlayerController())
	{
		if (Owning->GetMousePosition(MouseX, MouseY))
		{
			for (const FKodHudCommandHit& Hit : CommandHits)
			{
				if (Hit.Tooltip.IsEmpty() || MouseX < Hit.MinX || MouseX > Hit.MaxX || MouseY < Hit.MinY || MouseY > Hit.MaxY)
				{
					continue;
				}
				TArray<FString> Lines;
				Hit.Tooltip.ParseIntoArray(Lines, TEXT("\n"), true);
				const float TipH = FMath::Max(1, Lines.Num()) * 16.f + 10.f;
				DrawRect(FLinearColor(0.02f, 0.02f, 0.02f, 0.92f), MouseX + 16.f, MouseY - TipH - 8.f, 168.f, TipH);
				float TipY = MouseY - TipH - 2.f;
				for (const FString& Line : Lines)
				{
					DrawText(Line, FLinearColor::White, MouseX + 22.f, TipY, Font, 0.75f, false);
					TipY += 16.f;
				}
				break;
			}
		}
	}
}

void AKodHUD::DrawSelectionSummary()
{
	if (!Canvas)
	{
		return;
	}
	const AKodPlayerController* PC = Cast<AKodPlayerController>(GetOwningPlayerController());
	const UWorld* World = GetWorld();
	const UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!PC || !Sim)
	{
		return;
	}
	TMap<FName, int32> Counts;
	for (AActor* Actor : PC->GetLocalSelection())
	{
		if (!Actor)
		{
			continue;
		}
		const FKodEntityId Id = Sim->FindIdForActor(Actor);
		FKodStructureState Structure;
		if (Sim->TryGetStructure(Id, Structure))
		{
			const FName Label = Structure.DisplayId.IsNone() ? Structure.DefinitionId : Structure.DisplayId;
			if (!Label.IsNone())
			{
				Counts.FindOrAdd(Label) += 1;
			}
			continue;
		}
		FKodSimEntityState State;
		if (!Sim->TryGetState(Id, State))
		{
			continue;
		}
		const FName Label = PlayerFacingName(Sim, State.DefinitionId);
		if (!Label.IsNone())
		{
			Counts.FindOrAdd(Label) += 1;
		}
	}
	if (Counts.Num() <= 0)
	{
		return;
	}
	TArray<FName> Keys;
	Counts.GetKeys(Keys);
	Keys.Sort([](const FName& A, const FName& B) { return A.ToString() < B.ToString(); });
	FString Line;
	for (const FName& Key : Keys)
	{
		if (!Line.IsEmpty())
		{
			Line += TEXT("    ");
		}
		Line += FString::Printf(TEXT("%d %s"), Counts[Key], *Key.ToString());
	}
	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	if (!Font && GEngine)
	{
		Font = GEngine->GetSmallFont();
	}
	float Width = 0.f;
	float Height = 0.f;
	Canvas->TextSize(Font, Line, Width, Height);
	const float X = (Canvas->SizeX - Width) * 0.5f;
	const float Y = Canvas->SizeY - 40.f;
	DrawRect(FLinearColor(0.02f, 0.02f, 0.02f, 0.82f), X - 10.f, Y - 6.f, Width + 20.f, Height + 12.f);
	DrawText(Line, FLinearColor::White, X, Y, Font, 1.f, false);
}

float AKodHUD::DrawCheatSummary(const UKodSimSubsystem* Sim, float X, float Y, float MaxWidth, float Scale, UFont* Font)
{
	if (!Sim || !Canvas)
	{
		return Y;
	}
	FString Rest = Sim->GetCheatResultLine();
	if (Rest.IsEmpty())
	{
		return Y;
	}
	const FLinearColor Gold(0.95f, 0.75f, 0.2f, 1.f);
	while (!Rest.IsEmpty())
	{
		FString Line = Rest;
		float Width = 0.f;
		float Height = 0.f;
		Canvas->TextSize(Font, Line, Width, Height);
		while (Line.Len() > 1 && Width * Scale > MaxWidth)
		{
			int32 Cut = Line.Find(TEXT(", "), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
			if (Cut <= 0)
			{
				Cut = FMath::Max(1, Line.Len() / 2);
			}
			Line.LeftInline(Cut);
			Canvas->TextSize(Font, Line, Width, Height);
		}
		DrawText(Line, Gold, X, Y, Font, Scale, false);
		Y += FMath::Max(16.f, Height) * Scale + 4.f;
		Rest.RightChopInline(Line.Len());
		Rest.TrimStartInline();
	}
	return Y;
}

void AKodHUD::DrawMatchOutcome()
{
	if (!Canvas)
	{
		return;
	}
	const UWorld* World = GetWorld();
	const UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Sim || !Sim->IsMatchOver())
	{
		return;
	}
	if (MenuPage == 6)
	{
		return;
	}
	const bool bVictory = Sim->GetMatchOutcome() == EKodMatchOutcome::Victory;
	const FString Line = bVictory ? TEXT("VICTORY") : TEXT("DEFEAT");
	UFont* Font = GEngine ? GEngine->GetLargeFont() : nullptr;
	if (!Font && GEngine)
	{
		Font = GEngine->GetMediumFont();
	}
	const FKodScreenRect Banner = MapSpec(360.f, 260.f, 1200.f, 160.f);
	DrawPlate(Banner.X, Banner.Y, Banner.W, Banner.H, Tok(0x1F, 0x23, 0x27, 0.94f), TEXT("T_UI_VG_Modal_Panel"), 32.f, 32.f, 32.f, 32.f);
	const FLinearColor Ink = bVictory ? Tok(0xD9, 0xB8, 0x78) : Tok(0xE5, 0x38, 0x3B);
	DrawText(Line, Ink, Banner.X + 48.f * FrameScale, Banner.Y + 48.f * FrameScale, Font, 2.f * FrameScale, false);
	if (Sim->IsMatchCheated())
	{
		UFont* SummaryFont = GEngine ? GEngine->GetMediumFont() : Font;
		DrawCheatSummary(Sim, Banner.X + 48.f * FrameScale, Banner.Y + 100.f * FrameScale, Banner.W - 96.f * FrameScale, FrameScale, SummaryFont);
	}
}

void AKodHUD::DrawResourceFlash()
{
	if (!Canvas || FlashText.IsEmpty())
	{
		return;
	}
	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;
	if (Now > FlashUntilSeconds)
	{
		return;
	}
	UFont* Font = GEngine ? GEngine->GetLargeFont() : nullptr;
	if (!Font && GEngine)
	{
		Font = GEngine->GetMediumFont();
	}
	const FKodScreenRect Flash = MapSpec(660.f, 790.f, 600.f, 40.f);
	DrawRect(Tok(0x1F, 0x23, 0x27, 0.9f), Flash.X, Flash.Y, Flash.W, Flash.H);
	DrawText(FlashText, Tok(0xFF, 0xD2, 0x3F), Flash.X + 12.f * FrameScale, Flash.Y + 6.f * FrameScale, Font, FrameScale, false);
}

void AKodHUD::DrawUnitHealthBars()
{
	if (!Canvas)
	{
		return;
	}

	UWorld* World = GetWorld();
	APlayerController* OwningPC = GetOwningPlayerController();
	if (!World || !OwningPC)
	{
		return;
	}

	UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>();
	if (!Sim)
	{
		return;
	}

	TSet<const AActor*> Selected;
	if (const AKodPlayerController* KodPC = Cast<AKodPlayerController>(OwningPC))
	{
		for (const AActor* SelectedActor : KodPC->GetLocalSelection())
		{
			if (SelectedActor)
			{
				Selected.Add(SelectedActor);
			}
		}
	}

	const TArray<FKodSimEntityState>& States = FrameStates;
	const TArray<AActor*>& BarActors = FrameActors;

	const int32 Count = FMath::Min(States.Num(), BarActors.Num());
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FKodSimEntityState& HpState = States[Index];
		AActor* BarActor = BarActors[Index];
		if (!BarActor || HpState.MaxHealth <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const int32 LocalTeam = ReadLocalTeamId();
		if (HpState.TeamId != LocalTeam && Sim->GetTeamVision(LocalTeam, HpState.Position.X, HpState.Position.Y) != 2)
		{
			continue;
		}
		const int32 ShownHP = FMath::Max(0, FMath::FloorToInt(HpState.Health));
		const int32 MaxHP = FMath::Max(1, FMath::FloorToInt(HpState.MaxHealth));
		const bool bFull = ShownHP >= MaxHP;
		const bool bSelected = Selected.Contains(BarActor);
		const bool bShowAll = HealthBarMode == 2 || OwningPC->IsInputKeyDown(EKeys::LeftAlt) || OwningPC->IsInputKeyDown(EKeys::RightAlt);
		if (HealthBarMode == 0 && !bSelected && !bShowAll)
		{
			continue;
		}
		float MouseX = 0.f;
		float MouseY = 0.f;
		const bool bHaveMouse = OwningPC->GetMousePosition(MouseX, MouseY);

		float ExtraZ = 120.f;
		if (const UCapsuleComponent* Capsule = BarActor->FindComponentByClass<UCapsuleComponent>())
		{
			ExtraZ = Capsule->GetScaledCapsuleHalfHeight() + 24.f;
		}
		const FVector Anchor = BarActor->GetActorLocation() + FVector(0.f, 0.f, ExtraZ);

		FVector2D Screen;
		if (!OwningPC->ProjectWorldLocationToScreen(Anchor, Screen, true))
		{
			continue;
		}
		const bool bStructureBar = FindFrameStructure(HpState.Id.Value) != nullptr;
		float BarWidth = 40.f;
		float BarHeight = 5.f;
		if (bStructureBar)
		{
			BarWidth = 128.f;
			BarHeight = 8.f;
		}
		else if (HpState.BodyRadiusUU >= 48)
		{
			BarWidth = 96.f;
			BarHeight = 7.f;
		}
		else if (HpState.BodyRadiusUU >= 24)
		{
			BarWidth = 64.f;
			BarHeight = 6.f;
		}
		const bool bHover = bHaveMouse && FMath::Abs(MouseX - Screen.X) <= BarWidth * 0.5f + 8.f && FMath::Abs(MouseY - Screen.Y) <= 28.f;
		if (HealthBarMode != 2 && bFull && !bSelected && !bHover && !bShowAll)
		{
			continue;
		}

		int32 Step = 25;
		if (MaxHP > 1500)
		{
			Step = 250;
		}
		else if (MaxHP > 600)
		{
			Step = 100;
		}
		else if (MaxHP > 250)
		{
			Step = 50;
		}
		int32 Segments = FMath::Max(1, FMath::DivideAndRoundUp(MaxHP, Step));
		while (Segments > 30)
		{
			Step *= 2;
			Segments = FMath::Max(1, FMath::DivideAndRoundUp(MaxHP, Step));
		}

		const float Fraction = FMath::Clamp(static_cast<float>(ShownHP) / static_cast<float>(MaxHP), 0.f, 1.f);
		const FLinearColor Fill = HealthBarInk(Fraction);
		const float Left = Screen.X - (BarWidth * 0.5f);
		const float Top = Screen.Y - BarHeight;
		DrawRect(Tok(0x1F, 0x23, 0x27), Left, Top, BarWidth, BarHeight);
		DrawRect(Fill, Left, Top, BarWidth * Fraction, BarHeight);
		const FLinearColor Tick(0.f, 0.f, 0.f, 0.4f);
		for (int32 Segment = 1; Segment < Segments; ++Segment)
		{
			const float TickX = Left + (BarWidth * static_cast<float>(Segment)) / static_cast<float>(Segments);
			DrawRect(Tick, TickX, Top, 1.f, BarHeight);
		}
		const FLinearColor TeamCap = HpState.TeamId == ReadLocalTeamId()
			? FLinearColor(FColor(0x59, 0x78, 0x8A))
			: FLinearColor(FColor(0x8C, 0x5A, 0x5A));
		DrawRect(TeamCap, Left, Top, 2.f, BarHeight);

		if (HpState.EnergyMax > 0)
		{
			const float EnergyFrac = FMath::Clamp(static_cast<float>(HpState.Energy) / static_cast<float>(HpState.EnergyMax), 0.f, 1.f);
			const float EnergyTop = Top + BarHeight + 1.f;
			DrawRect(Tok(0x1F, 0x23, 0x27), Left, EnergyTop, BarWidth, 3.f);
			DrawRect(Tok(0x2F, 0x7B, 0xFF), Left, EnergyTop, BarWidth * EnergyFrac, 3.f);
		}
	}
}

void AKodHUD::DrawSelectedResourceNodes()
{
	if (!Canvas)
	{
		return;
	}

	UWorld* World = GetWorld();
	APlayerController* OwningPC = GetOwningPlayerController();
	if (!World || !OwningPC)
	{
		return;
	}

	const UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>();
	const AKodPlayerController* KodPC = Cast<AKodPlayerController>(OwningPC);
	if (!Sim || !KodPC)
	{
		return;
	}

	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	if (!Font && GEngine)
	{
		Font = GEngine->GetSmallFont();
	}

	TArray<AActor*> LabelActors;
	for (AActor* SelectedActor : KodPC->GetLocalSelection())
	{
		if (!SelectedActor)
		{
			continue;
		}
		if (Sim->IsResourceNode(Sim->FindIdForActor(SelectedActor)))
		{
			LabelActors.AddUnique(SelectedActor);
		}
	}
	FHitResult CursorHit;
	if (OwningPC->GetHitResultUnderCursor(ECC_Pawn, true, CursorHit))
	{
		if (AActor* HoverActor = CursorHit.GetActor())
		{
			if (Sim->IsResourceNode(Sim->FindIdForActor(HoverActor)))
			{
				LabelActors.AddUnique(HoverActor);
			}
		}
	}

	for (AActor* SelectedActor : LabelActors)
	{
		if (!SelectedActor)
		{
			continue;
		}
		FKodResourceNodeState Node;
		if (!Sim->TryGetResourceNode(Sim->FindIdForActor(SelectedActor), Node))
		{
			continue;
		}

		const FBox Bounds = SelectedActor->GetComponentsBoundingBox(true);
		const FVector Anchor = Bounds.IsValid
			? FVector(Bounds.GetCenter().X, Bounds.GetCenter().Y, Bounds.Max.Z + 28.f)
			: SelectedActor->GetActorLocation() + FVector(0.f, 0.f, 160.f);
		FVector2D Screen;
		if (!OwningPC->ProjectWorldLocationToScreen(Anchor, Screen, true))
		{
			continue;
		}

		const FString Label = FString::Printf(TEXT("%s  %d"), KodResourceTypeName(Node.Type), Node.Remaining);
		const FLinearColor Color = (Node.Type == EKodResourceType::Luminene)
			? Tok(0xFF, 0xD2, 0x3F)
			: Tok(0xB0, 0x8D, 0x57);
		const float Width = 8.f * static_cast<float>(Label.Len()) + 10.f;
		DrawRect(FLinearColor(0.02f, 0.02f, 0.02f, 0.72f), Screen.X - 4.f, Screen.Y - 2.f, Width, 18.f);
		DrawText(Label, Color, Screen.X, Screen.Y, Font, 1.f, false);
	}
}

void AKodHUD::DrawSelectionMarquee()
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

	FVector2D Min;
	FVector2D Max;
	if (!PC->GetSelectionMarqueeRect(Min, Max))
	{
		return;
	}

	const float Width = Max.X - Min.X;
	const float Height = Max.Y - Min.Y;
	// Same orange as the selection tint. Canvas quad uses the engine white texture.
	const FLinearColor Fill(1.f, 0.42f, 0.05f, 0.18f);
	const FLinearColor Edge(1.f, 0.55f, 0.12f, 0.95f);
	DrawRect(Fill, Min.X, Min.Y, Width, Height);
	DrawLine(Min.X, Min.Y, Max.X, Min.Y, Edge, 2.f);
	DrawLine(Max.X, Min.Y, Max.X, Max.Y, Edge, 2.f);
	DrawLine(Max.X, Max.Y, Min.X, Max.Y, Edge, 2.f);
	DrawLine(Min.X, Max.Y, Min.X, Min.Y, Edge, 2.f);
}

void AKodHUD::TogglePauseMenu()
{
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (MenuPage == 1)
	{
		MenuPage = 0;
		if (Sim && !Sim->IsMenuMode())
		{
			Sim->SetSimPaused(false);
		}
		return;
	}
	if (MenuPage != 0 && MenuPage != 6)
	{
		MenuPage = Sim && Sim->IsMenuMode() ? 8 : 0;
		return;
	}
	MenuPage = 1;
	if (Sim)
	{
		Sim->SetSimPaused(true);
	}
}

void AKodHUD::SetMenuPage(int32 Page)
{
	MenuPage = Page;
}

void AKodHUD::OpenChat()
{
	bChatOpen = true;
	ChatLine.Reset();
}

void AKodHUD::CloseChat()
{
	bChatOpen = false;
	ChatLine.Reset();
}

void AKodHUD::AppendChatText(const FString& Text)
{
	if (ChatLine.Len() < 80)
	{
		ChatLine += Text;
	}
}

void AKodHUD::BackspaceChat()
{
	if (ChatLine.Len() > 0)
	{
		ChatLine.LeftChopInline(1);
	}
}

void AKodHUD::AddChatMessage(const FString& Text)
{
	if (Text.IsEmpty())
	{
		return;
	}
	ChatLog.Add(Text);
	while (ChatLog.Num() > 8)
	{
		ChatLog.RemoveAt(0);
	}
	UE_LOG(LogTemp, Log, TEXT("KodChat Line=%s"), *Text);
}

void AKodHUD::SubmitChat()
{
	const FString Line = ChatLine.TrimStartAndEnd();
	bChatOpen = false;
	ChatLine.Reset();
	if (Line.IsEmpty())
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		if (UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>())
		{
			if (Sim->SubmitCheatPhrase(Line) == EKodCheatSubmit::Applied)
			{
				return;
			}
		}
	}
	AddChatMessage(Line);
}

void AKodHUD::HandleMenuChoice(int32 Choice)
{
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	UKodUserSettings* Settings = UKodUserSettings::Get();
	auto Resume = [this, Sim]()
	{
		MenuPage = 0;
		if (Sim && !Sim->IsMenuMode())
		{
			Sim->SetSimPaused(false);
		}
	};
	if (Choice == 0)
	{
		Resume();
	}
	else if (Choice == 1)
	{
		MenuPage = 3;
	}
	else if (Choice == 2)
	{
		MenuPage = 4;
	}
	else if (Choice == 3)
	{
		MenuPage = 2;
	}
	else if (Choice == 4 && World)
	{
		UGameplayStatics::OpenLevel(World, FName(*UGameplayStatics::GetCurrentLevelName(World, true)));
	}
	else if (Choice == 5 && World)
	{
		UGameplayStatics::OpenLevel(World, FName(*UGameplayStatics::GetCurrentLevelName(World, true)), true, TEXT("Menu=1"));
	}
	else if (Choice == 6)
	{
		MenuPage = 7;
	}
	else if (Choice == 7)
	{
		MenuPage = 5;
		ScoreTab = 0;
	}
	else if (Choice == 8 && World)
	{
		UGameplayStatics::OpenLevel(World, FName(*UGameplayStatics::GetCurrentLevelName(World, true)));
	}
	else if (Choice == 9)
	{
		if (APlayerController* PC = GetOwningPlayerController())
		{
			PC->ConsoleCommand(TEXT("Quit"));
		}
	}
	else if (Choice >= 10 && Choice <= 12 && Sim)
	{
		Sim->SaveSkirmishSlot(Choice - 9);
		Resume();
	}
	else if (Choice >= 13 && Choice <= 15 && Sim)
	{
		Sim->LoadSkirmishSlot(Choice - 12);
		Resume();
	}
	else if (Choice >= 20 && Choice <= 24)
	{
		ScoreTab = Choice - 20;
	}
	else if (Choice == 30)
	{
		MenuPage = Sim && Sim->IsMenuMode() ? 8 : (Sim && Sim->IsMatchOver() ? 6 : 1);
	}
	else if (Choice == 40 && Settings)
	{
		Settings->MasterVolume = FMath::Clamp(Settings->MasterVolume + 0.1f, 0.f, 1.f);
		Settings->SaveNow();
		Settings->Apply(World);
	}
	else if (Choice == 41 && Settings)
	{
		Settings->MusicVolume = FMath::Clamp(Settings->MusicVolume + 0.1f, 0.f, 1.f);
		Settings->SaveNow();
		Settings->Apply(World);
	}
	else if (Choice == 42 && Settings)
	{
		Settings->SfxVolume = FMath::Clamp(Settings->SfxVolume + 0.1f, 0.f, 1.f);
		Settings->SaveNow();
		Settings->Apply(World);
	}
	else if (Choice == 43 && Settings)
	{
		Settings->VoiceVolume = FMath::Clamp(Settings->VoiceVolume + 0.1f, 0.f, 1.f);
		Settings->SaveNow();
		Settings->Apply(World);
	}
	else if (Choice == 44 && Settings)
	{
		Settings->EdgeScrollSpeed = FMath::Clamp(Settings->EdgeScrollSpeed + 400.f, 400.f, 12000.f);
		Settings->SaveNow();
		Settings->Apply(World);
	}
	else if (Choice == 45 && Settings)
	{
		Settings->KeyboardScrollSpeed = FMath::Clamp(Settings->KeyboardScrollSpeed + 400.f, 400.f, 12000.f);
		Settings->SaveNow();
		Settings->Apply(World);
	}
	else if (Choice == 46 && Settings)
	{
		Settings->MouseSensitivity = FMath::Clamp(Settings->MouseSensitivity + 0.1f, 0.2f, 3.f);
		Settings->SaveNow();
		Settings->Apply(World);
	}
	else if (Choice == 47 && Settings)
	{
		Settings->HealthBarMode = (Settings->HealthBarMode + 1) % 3;
		Settings->SaveNow();
		Settings->Apply(World);
	}
	else if (Choice == 48 && Settings)
	{
		Settings->bShowHotkeyGrid = !Settings->bShowHotkeyGrid;
		Settings->SaveNow();
		Settings->Apply(World);
	}
	else if (Choice == 49 && Settings)
	{
		Settings->GraphicsQuality = (Settings->GraphicsQuality + 1) % 4;
		Settings->SaveNow();
		Settings->Apply(World);
	}
	else if (Choice == 50 && Sim)
	{
		Sim->SetExposedRule(!Sim->IsExposedRuleEnabled());
	}
	else if (Choice == 51 && Sim)
	{
		Sim->SetCompetitive(!Sim->IsCompetitive());
	}
	else if (Choice == 52 && Settings)
	{
		Settings->bCameraShake = !Settings->bCameraShake;
		Settings->SaveNow();
	}
	else if (Choice == 53 && Settings)
	{
		Settings->bShowDamageNumbers = !Settings->bShowDamageNumbers;
		Settings->SaveNow();
	}
}

void AKodHUD::DrawShellMenus()
{
	if (!Canvas)
	{
		return;
	}
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (Sim && Sim->IsMenuMode() && MenuPage == 0)
	{
		MenuPage = 8;
	}
	if (Sim && Sim->IsMatchOver() && MenuPage == 0)
	{
		MenuPage = 6;
	}
	if (MenuPage == 0 || MenuPage == 7)
	{
		return;
	}
	const float Scale = FrameScale;
	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	const bool bResults = MenuPage == 5;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, bResults ? 0.82f : 0.6f), 0.f, 0.f, Canvas->SizeX, Canvas->SizeY);
	auto Button = [this, Font, Scale](float X, float Y, float W, float H, const FString& Label, int32 Choice)
	{
		DrawPlate(X, Y, W, H, Tok(0x5C, 0x64, 0x6C), TEXT("T_UI_VG_Btn_Chip"), 8.f, 8.f, 8.f, 8.f);
		DrawText(Label, Tok(0xE6, 0xE6, 0xE0), X + 12.f * Scale, Y + FMath::Max(4.f, (H - 18.f * Scale) * 0.5f), Font, 0.9f * Scale, false);
		RememberChrome(FName(TEXT("Menu")), Choice, FName(), X, Y, X + W, Y + H);
	};
	auto StackIn = [&](const FKodScreenRect& Panel, const FString& Title, const TArray<FString>& Labels, const TArray<int32>& Choices)
	{
		DrawPlate(Panel.X, Panel.Y, Panel.W, Panel.H, Tok(0x1F, 0x23, 0x27, 0.96f), TEXT("T_UI_VG_Modal_Panel"), 32.f, 32.f, 32.f, 32.f);
		DrawText(Title, Tok(0xD9, 0xB8, 0x78), Panel.X + 40.f * Scale, Panel.Y + 14.f * Scale, Font, 1.15f * Scale, false);
		const int32 Count = FMath::Min(Labels.Num(), Choices.Num());
		const float Top = Panel.Y + 52.f * Scale;
		const float Bottom = Panel.Y + Panel.H - 16.f * Scale;
		const float Gap = 8.f * Scale;
		const float Room = FMath::Max(1.f, Bottom - Top);
		const float H = FMath::Clamp((Room - Gap * static_cast<float>(FMath::Max(0, Count - 1))) / static_cast<float>(FMath::Max(1, Count)), 28.f * Scale, 56.f * Scale);
		const float W = FMath::Min(400.f * Scale, Panel.W - 40.f * Scale);
		const float X = Panel.X + (Panel.W - W) * 0.5f;
		float Y = Top;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Button(X, Y, W, H, Labels[Index], Choices[Index]);
			Y += H + Gap;
		}
	};

	const FKodScreenRect PausePanel = MapSpec(720.f, 330.f, 480.f, 420.f);
	if (MenuPage == 1)
	{
		TArray<FString> Labels;
		Labels.Add(TEXT("Resume"));
		Labels.Add(TEXT("Save"));
		Labels.Add(TEXT("Load"));
		Labels.Add(TEXT("Settings"));
		Labels.Add(TEXT("Restart"));
		Labels.Add(TEXT("Quit"));
		TArray<int32> Choices = { 0, 1, 2, 3, 4, 5 };
		StackIn(PausePanel, TEXT("Paused"), Labels, Choices);
	}
	else if (MenuPage == 3 || MenuPage == 4)
	{
		const bool bSave = MenuPage == 3;
		TArray<FString> Labels;
		TArray<int32> Choices;
		for (int32 Slot = 1; Slot <= 3; ++Slot)
		{
			Labels.Add(FString::Printf(TEXT("Slot %d"), Slot));
			Choices.Add((bSave ? 9 : 12) + Slot);
		}
		Labels.Add(TEXT("Back"));
		Choices.Add(30);
		StackIn(PausePanel, bSave ? FString(TEXT("Save Game")) : FString(TEXT("Load Game")), Labels, Choices);
	}
	else if (MenuPage == 2)
	{
		UKodUserSettings* Settings = UKodUserSettings::Get();
		const TCHAR* Bars[] = { TEXT("Selected"), TEXT("Damaged"), TEXT("Always") };
		TArray<FString> Labels;
		TArray<int32> Choices;
		auto Add = [&](const FString& Label, int32 Choice)
		{
			Labels.Add(Label);
			Choices.Add(Choice);
		};
		Add(FString::Printf(TEXT("Master %.0f%%"), Settings ? Settings->MasterVolume * 100.f : 0.f), 40);
		Add(FString::Printf(TEXT("Music %.0f%%"), Settings ? Settings->MusicVolume * 100.f : 0.f), 41);
		Add(FString::Printf(TEXT("SFX %.0f%%"), Settings ? Settings->SfxVolume * 100.f : 0.f), 42);
		Add(FString::Printf(TEXT("Voice %.0f%%"), Settings ? Settings->VoiceVolume * 100.f : 0.f), 43);
		Add(FString::Printf(TEXT("Edge scroll %.0f"), Settings ? Settings->EdgeScrollSpeed : 0.f), 44);
		Add(FString::Printf(TEXT("Key scroll %.0f"), Settings ? Settings->KeyboardScrollSpeed : 0.f), 45);
		Add(FString::Printf(TEXT("Mouse %.1f"), Settings ? Settings->MouseSensitivity : 1.f), 46);
		const int32 BarMode = Settings ? FMath::Clamp(Settings->HealthBarMode, 0, 2) : 0;
		Add(FString::Printf(TEXT("Health bars %s"), Bars[BarMode]), 47);
		Add(Settings && Settings->bShowHotkeyGrid ? TEXT("Hotkey grid On") : TEXT("Hotkey grid Off"), 48);
		Add(FString::Printf(TEXT("Graphics %d"), Settings ? Settings->GraphicsQuality : 0), 49);
		Add(Sim && Sim->IsExposedRuleEnabled() ? TEXT("Exposed rule On") : TEXT("Exposed rule Off"), 50);
		Add(Sim && Sim->IsCompetitive() ? TEXT("Competitive On") : TEXT("Competitive Off"), 51);
		Add(Settings && Settings->bCameraShake ? TEXT("Camera shake On") : TEXT("Camera shake Off"), 52);
		Add(Settings && Settings->bShowDamageNumbers ? TEXT("Damage numbers On") : TEXT("Damage numbers Off"), 53);
		Add(TEXT("Back"), 30);
		StackIn(MapSpec(720.f, 80.f, 480.f, 920.f), TEXT("Settings"), Labels, Choices);
	}
	else if (MenuPage == 6)
	{
		const bool bVictory = Sim && Sim->GetMatchOutcome() == EKodMatchOutcome::Victory;
		const FKodScreenRect Banner = MapSpec(360.f, 260.f, 1200.f, 160.f);
		DrawPlate(Banner.X, Banner.Y, Banner.W, Banner.H, Tok(0x1F, 0x23, 0x27, 0.96f), TEXT("T_UI_VG_Modal_Panel"), 32.f, 32.f, 32.f, 32.f);
		DrawText(bVictory ? TEXT("VICTORY") : TEXT("DEFEAT"), bVictory ? Tok(0xD9, 0xB8, 0x78) : Tok(0xE5, 0x38, 0x3B), Banner.X + 48.f * Scale, Banner.Y + 48.f * Scale, Font, 1.8f * Scale, false);
		if (Sim && Sim->IsMatchCheated())
		{
			DrawCheatSummary(Sim, Banner.X + 48.f * Scale, Banner.Y + 100.f * Scale, Banner.W - 96.f * Scale, Scale, Font);
		}
		const float BtnW = 280.f * Scale;
		const float BtnH = 56.f * Scale;
		const float Gap = 16.f * Scale;
		const float RowW = 4.f * BtnW + 3.f * Gap;
		const FKodScreenRect Row = MapSpec(0.f, 760.f, 1920.f, 56.f);
		float X = Row.X + (Row.W - RowW) * 0.5f;
		Button(X, Row.Y, BtnW, BtnH, TEXT("Continue"), 6);
		X += BtnW + Gap;
		Button(X, Row.Y, BtnW, BtnH, TEXT("Results"), 7);
		X += BtnW + Gap;
		Button(X, Row.Y, BtnW, BtnH, TEXT("Restart"), 4);
		X += BtnW + Gap;
		Button(X, Row.Y, BtnW, BtnH, TEXT("Main Menu"), 5);
	}
	else if (MenuPage == 8)
	{
		TArray<FString> Labels;
		Labels.Add(TEXT("Skirmish"));
		Labels.Add(TEXT("Settings"));
		Labels.Add(TEXT("Quit"));
		TArray<int32> Choices = { 8, 3, 9 };
		StackIn(PausePanel, TEXT("Kingdom of Dust"), Labels, Choices);
		const TCHAR* FactionNames[] = { TEXT("VANGUARD"), TEXT("SCINDER") };
		for (int32 Index = 0; Index < 2; ++Index)
		{
			const FKodScreenRect Tile = MapSpec(64.f, 168.f + 130.f * static_cast<float>(Index), 600.f, 120.f);
			const bool bOn = HudSkin == Index;
			const FLinearColor Glass = bOn ? Tok(0x23, 0xAD, 0xFF, 0.35f) : Tok(0x0A, 0x16, 0x22, 0.7f);
			DrawRect(Glass, Tile.X, Tile.Y, Tile.W, Tile.H);
			DrawText(FactionNames[Index], FLinearColor::White, Tile.X + 24.f * Scale, Tile.Y + 44.f * Scale, Font, Scale, false);
			RememberChrome(FName(TEXT("Faction")), Index, FName(), Tile.X, Tile.Y, Tile.X + Tile.W, Tile.Y + Tile.H);
		}
		const TCHAR* Locked[] = { TEXT("RSF"), TEXT("RU"), TEXT("CN") };
		for (int32 Index = 0; Index < 3; ++Index)
		{
			const FKodScreenRect Tile = MapSpec(64.f, 428.f + 130.f * static_cast<float>(Index), 600.f, 120.f);
			DrawRect(Tok(0x0A, 0x16, 0x22, 0.4f), Tile.X, Tile.Y, Tile.W, Tile.H);
			DrawText(FString::Printf(TEXT("%s  LOCKED"), Locked[Index]), Tok(0x8E, 0x94, 0x9A), Tile.X + 24.f * Scale, Tile.Y + 44.f * Scale, Font, Scale, false);
		}
	}
	else if (MenuPage == 5 && Sim)
	{
		const TCHAR* Tabs[] = { TEXT("APM"), TEXT("Supply"), TEXT("Production"), TEXT("Army Value"), TEXT("Resources") };
		for (int32 Tab = 0; Tab < 5; ++Tab)
		{
			const FKodScreenRect TabRect = MapSpec(394.f + 228.f * static_cast<float>(Tab), 120.f, 220.f, 48.f);
			const bool bOn = ScoreTab == Tab;
			DrawPlate(TabRect.X, TabRect.Y, TabRect.W, TabRect.H, bOn ? Tok(0xB0, 0x8D, 0x57) : Tok(0x5C, 0x64, 0x6C), TEXT("T_UI_VG_Tab_Group"), 8.f, 6.f, 8.f, 6.f);
			DrawText(Tabs[Tab], Tok(0xE6, 0xE6, 0xE0), TabRect.X + 12.f * Scale, TabRect.Y + 12.f * Scale, Font, 0.9f * Scale, false);
			RememberChrome(FName(TEXT("Menu")), 20 + Tab, FName(), TabRect.X, TabRect.Y, TabRect.X + TabRect.W, TabRect.Y + TabRect.H);
		}
		const FKodScreenRect Content = MapSpec(160.f, 190.f, 1600.f, 760.f);
		DrawPlate(Content.X, Content.Y, Content.W, Content.H, Tok(0x1F, 0x23, 0x27, 0.9f), TEXT("T_UI_VG_Modal_Panel"), 32.f, 32.f, 32.f, 32.f);
		float Y = Content.Y + 16.f * Scale;
		const float X = Content.X + 16.f * Scale;
		if (Sim->IsMatchCheated())
		{
			Y = DrawCheatSummary(Sim, X, Y, Content.W - 32.f * Scale, Scale, Font);
			Y += 8.f * Scale;
		}
		const int32 Seconds = static_cast<int32>(Sim->GetSimTickIndex() / 16);
		int32 Average = 0;
		int32 Peak = 0;
		if (UKodApmTracker* Apm = World->GetSubsystem<UKodApmTracker>())
		{
			Apm->GetApm(Average, Peak);
		}
		int32 Collected = 0;
		int32 Trained = 0;
		int32 Killed = 0;
		int32 Built = 0;
		int32 Razed = 0;
		Sim->GetTeamTotals(0, Collected, Trained, Killed, Built, Razed);
		if (ScoreTab == 0)
		{
			DrawText(FString::Printf(TEXT("Time %02d:%02d"), Seconds / 60, Seconds % 60), Tok(0xE6, 0xE6, 0xE0), X, Y, Font, Scale, false); Y += 28.f * Scale;
			DrawText(FString::Printf(TEXT("APM %d  peak %d"), Average, Peak), Tok(0xE6, 0xE6, 0xE0), X, Y, Font, Scale, false); Y += 28.f * Scale;
			DrawText(FString::Printf(TEXT("Collected %d"), Collected), Tok(0xE6, 0xE6, 0xE0), X, Y, Font, Scale, false); Y += 28.f * Scale;
			DrawText(FString::Printf(TEXT("Trained %d   Killed %d"), Trained, Killed), Tok(0xE6, 0xE6, 0xE0), X, Y, Font, Scale, false); Y += 28.f * Scale;
			DrawText(FString::Printf(TEXT("Built %d   Razed %d"), Built, Razed), Tok(0xE6, 0xE6, 0xE0), X, Y, Font, Scale, false);
		}
		else if (ScoreTab == 1)
		{
			for (int32 TeamId = 0; TeamId <= 1; ++TeamId)
			{
				int32 Used = 0;
				int32 Cap = 0;
				Sim->TryGetSupply(TeamId, Used, Cap);
				DrawText(FString::Printf(TEXT("Team %d supply %d/%d"), TeamId, Used, Cap), Tok(0xE6, 0xE6, 0xE0), X, Y, Font, Scale, false);
				Y += 28.f * Scale;
			}
		}
		else
		{
			TArray<UKodSimSubsystem::FKodStatSample> Samples;
			Sim->CopyStatSamples(0, Samples);
			float MaxValue = 1.f;
			for (const UKodSimSubsystem::FKodStatSample& Sample : Samples)
			{
				const int32 Value = ScoreTab == 2 ? Sample.Trained + Sample.Built : (ScoreTab == 3 ? Sample.ArmyValue : Sample.Income);
				MaxValue = FMath::Max(MaxValue, static_cast<float>(Value));
			}
			const float ChartW = Content.W - 32.f * Scale;
			const float ChartH = 280.f * Scale;
			FVector2D Prev(X, Y + ChartH);
			for (int32 Index = 0; Index < Samples.Num(); ++Index)
			{
				const int32 Value = ScoreTab == 2 ? Samples[Index].Trained + Samples[Index].Built : (ScoreTab == 3 ? Samples[Index].ArmyValue : Samples[Index].Income);
				const float Px = X + ChartW * (Samples.Num() <= 1 ? 0.f : static_cast<float>(Index) / static_cast<float>(Samples.Num() - 1));
				const float Py = Y + ChartH - ChartH * (static_cast<float>(Value) / MaxValue);
				if (Index > 0)
				{
					DrawLine(Prev.X, Prev.Y, Px, Py, Tok(0xB0, 0x8D, 0x57), 2.f);
				}
				Prev = FVector2D(Px, Py);
			}
		}
		const FKodScreenRect Back = MapSpec(1584.f, 990.f, 240.f, 56.f);
		Button(Back.X, Back.Y, Back.W, Back.H, TEXT("Back"), 30);
	}
}

void AKodHUD::BeginPlay()
{
	Super::BeginPlay();
	bShowHUD = true;
	BindAlertFeed();
	if (UWorld* SettingsWorld = GetWorld())
	{
		if (UKodUserSettings* Settings = UKodUserSettings::Get())
		{
			Settings->Apply(SettingsWorld);
		}
		if (UKodSimSubsystem* Sim = SettingsWorld->GetSubsystem<UKodSimSubsystem>())
		{
			if (Sim->IsMenuMode())
			{
				MenuPage = 8;
			}
		}
	}
	if (UWorld* PortraitWorld = GetWorld())
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		PortraitStage = PortraitWorld->SpawnActor<AKodPortraitStage>(
			AKodPortraitStage::StaticClass(),
			FVector(0.f, 0.f, -200000.f),
			FRotator::ZeroRotator,
			Params);
	}

	if (APlayerController* PC = GetOwningPlayerController())
	{
		if (UClass* WidgetClass = HudWidgetClass.LoadSynchronous())
		{
			HudWidgetInstance = CreateWidget<UUserWidget>(PC, WidgetClass);
			if (HudWidgetInstance)
			{
				bool bHudRoot = false;
				for (UClass* Class = HudWidgetInstance->GetClass(); Class; Class = Class->GetSuperClass())
				{
					if (Class->GetFName() == FName(TEXT("KodHudRootWidget")))
					{
						bHudRoot = true;
						break;
					}
				}
				if (!bHudRoot)
				{
					UE_LOG(LogTemp, Warning, TEXT("AKodHUD: HudWidgetClass should derive from UKodHudRootWidget (/Game/UI/HUD/WBP_KodHUD)."));
				}
				HudWidgetInstance->AddToViewport();
				if (UFunction* ActivateWidget = HudWidgetInstance->FindFunction(TEXT("ActivateWidget")))
				{
					HudWidgetInstance->ProcessEvent(ActivateWidget, nullptr);
				}
			}
		}
	}
}

void AKodHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AStaticMeshActor* Sheet = FogSheetActor.Get())
	{
		Sheet->Destroy();
		FogSheetActor = nullptr;
	}
	if (UWorld* World = GetWorld())
	{
		if (UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>())
		{
			Sim->OnJuiceText.RemoveAll(this);
		}
	}
	Super::EndPlay(EndPlayReason);
}
