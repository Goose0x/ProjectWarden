#include "Game/KodHUD.h"

#include "Blueprint/UserWidget.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
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
}

AKodHUD::AKodHUD()
{
	bShowHUD = true;
}

void AKodHUD::DrawHUD()
{
	Super::DrawHUD();
	bShowHUD = true;

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
	if (bChatOpen && Canvas)
	{
		UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
		const FString Line = FString::Printf(TEXT("> %s_"), *ChatLine);
		DrawRect(FLinearColor(0.02f, 0.02f, 0.02f, 0.9f), 16.f, Canvas->SizeY - 36.f, 640.f, 28.f);
		DrawText(Line, FLinearColor(0.9f, 0.95f, 0.7f, 1.f), 24.f, Canvas->SizeY - 32.f, Font, 1.f, false);
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

void AKodHUD::AddMinimapPing(const FVector& WorldPoint)
{
	const UWorld* World = GetWorld();
	const float Until = World ? World->GetTimeSeconds() + 6.f : 0.f;
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
		FVector Screen;
		if (!PC->ProjectWorldLocationToScreen(Marker.World, Screen, true))
		{
			continue;
		}
		const float Life = 1.f - (Age / FMath::Max(0.05f, Marker.Life));
		const float Pulse = 0.65f + 0.35f * FMath::Abs(FMath::Sin(Age * 16.f));
		const float Radius = 10.f + 34.f * (Age / FMath::Max(0.05f, Marker.Life));
		FLinearColor Color = FLinearColor(0.25f, 0.95f, 0.35f, Life * Pulse);
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

void AKodHUD::DrawSelectionPulse()
{
	AKodPlayerController* PC = Cast<AKodPlayerController>(GetOwningPlayerController());
	UWorld* World = GetWorld();
	if (!Canvas || !PC || !World)
	{
		return;
	}
	TArray<AActor*> Selected = PC->GetLocalSelection();
	Selected.Sort([](const AActor* A, const AActor* B)
	{
		const uint32 IdA = A ? A->GetUniqueID() : 0u;
		const uint32 IdB = B ? B->GetUniqueID() : 0u;
		return IdA < IdB;
	});
	TArray<uint32> Ids;
	Ids.Reserve(Selected.Num());
	for (const AActor* Actor : Selected)
	{
		if (Actor)
		{
			Ids.Add(Actor->GetUniqueID());
		}
	}
	if (Ids != PulseSelectionIds)
	{
		PulseSelectionIds = Ids;
		SelectPulseUntil = World->GetTimeSeconds() + 0.35f;
	}
	const float Now = World->GetTimeSeconds();
	const bool bPulse = Now < SelectPulseUntil;
	const float Wave = bPulse
		? 0.55f + 0.45f * FMath::Abs(FMath::Sin((SelectPulseUntil - Now) * 18.f))
		: 0.35f + 0.12f * FMath::Abs(FMath::Sin(Now * 3.f));
	const float Radius = bPulse ? 26.f + 14.f * ((SelectPulseUntil - Now) / 0.35f) : 18.f;
	const FLinearColor Color(0.35f, 0.95f, 0.45f, Wave);
	for (AActor* Actor : Selected)
	{
		if (!Actor)
		{
			continue;
		}
		FVector Screen;
		if (!PC->ProjectWorldLocationToScreen(Actor->GetActorLocation(), Screen, true))
		{
			continue;
		}
		const int32 Steps = 12;
		FVector2D Prev(Screen.X + Radius, Screen.Y);
		for (int32 Step = 1; Step <= Steps; ++Step)
		{
			const float Angle = (static_cast<float>(Step) / static_cast<float>(Steps)) * 2.f * PI;
			const FVector2D Next(Screen.X + FMath::Cos(Angle) * Radius, Screen.Y + FMath::Sin(Angle) * Radius);
			DrawLine(Prev.X, Prev.Y, Next.X, Next.Y, Color, bPulse ? 2.5f : 1.25f);
			Prev = Next;
		}
	}
}

void AKodHUD::DrawJuiceFloaters()
{
	APlayerController* PC = GetOwningPlayerController();
	UWorld* World = GetWorld();
	if (!Canvas || !PC || !World)
	{
		return;
	}
	if (UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>())
	{
		TArray<FKodJuiceText> Fresh;
		Sim->ConsumeJuiceText(Fresh);
		const float Now = World->GetTimeSeconds();
		for (const FKodJuiceText& Text : Fresh)
		{
			FKodHudFloater Floater;
			Floater.World = Text.Position;
			Floater.Amount = Text.Amount;
			Floater.Kind = Text.Kind;
			Floater.Born = Now;
			JuiceFloaters.Add(Floater);
		}
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
		FVector Screen;
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
	if (!Canvas || !bMapFog)
	{
		return;
	}
	UWorld* World = GetWorld();
	const UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	APlayerController* PC = GetOwningPlayerController();
	const AKodRTSCameraPawn* Camera = Cast<AKodRTSCameraPawn>(GetOwningPawn());
	if (!Sim || !PC || !Camera)
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
	const int32 LocalTeam = ReadLocalTeamId();
	const float GroundZ = (NearLeft.Z + NearRight.Z + FarRight.Z + FarLeft.Z) * 0.25f;
	const float MinX = FMath::Min(FMath::Min(NearLeft.X, NearRight.X), FMath::Min(FarRight.X, FarLeft.X));
	const float MaxX = FMath::Max(FMath::Max(NearLeft.X, NearRight.X), FMath::Max(FarRight.X, FarLeft.X));
	const float MinY = FMath::Min(FMath::Min(NearLeft.Y, NearRight.Y), FMath::Min(FarRight.Y, FarLeft.Y));
	const float MaxY = FMath::Max(FMath::Max(NearLeft.Y, NearRight.Y), FMath::Max(FarRight.Y, FarLeft.Y));
	const int32 Cell = KodBuildGrid::CellUU;
	const int32 X0 = KodBuildGrid::CellOf(MinX);
	const int32 X1 = KodBuildGrid::CellOf(MaxX);
	const int32 Y0 = KodBuildGrid::CellOf(MinY);
	const int32 Y1 = KodBuildGrid::CellOf(MaxY);
	for (int32 CellY = Y0; CellY <= Y1; ++CellY)
	{
		for (int32 CellX = X0; CellX <= X1; ++CellX)
		{
			const uint8 Vision = Sim->GetTeamVision(
				LocalTeam,
				KodBuildGrid::CellCenter(CellX),
				KodBuildGrid::CellCenter(CellY));
			if (Vision == 2)
			{
				continue;
			}
			const float Alpha = Vision == 0 ? 0.72f : 0.4f;
			const FVector Corners[4] = {
				FVector(static_cast<float>(CellX * Cell), static_cast<float>(CellY * Cell), GroundZ),
				FVector(static_cast<float>((CellX + 1) * Cell), static_cast<float>(CellY * Cell), GroundZ),
				FVector(static_cast<float>((CellX + 1) * Cell), static_cast<float>((CellY + 1) * Cell), GroundZ),
				FVector(static_cast<float>(CellX * Cell), static_cast<float>((CellY + 1) * Cell), GroundZ)
			};
			float MinSX = MAX_flt;
			float MinSY = MAX_flt;
			float MaxSX = -MAX_flt;
			float MaxSY = -MAX_flt;
			bool bAny = false;
			for (const FVector& Corner : Corners)
			{
				FVector2D Screen;
				if (!PC->ProjectWorldLocationToScreen(Corner, Screen, true))
				{
					continue;
				}
				bAny = true;
				MinSX = FMath::Min(MinSX, static_cast<float>(Screen.X));
				MinSY = FMath::Min(MinSY, static_cast<float>(Screen.Y));
				MaxSX = FMath::Max(MaxSX, static_cast<float>(Screen.X));
				MaxSY = FMath::Max(MaxSY, static_cast<float>(Screen.Y));
			}
			if (!bAny || MaxSX <= MinSX || MaxSY <= MinSY)
			{
				continue;
			}
			DrawRect(FLinearColor(0.f, 0.f, 0.02f, Alpha), MinSX, MinSY, MaxSX - MinSX, MaxSY - MinSY);
		}
	}

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
			? FLinearColor(0.3f, 0.9f, 0.38f, 0.55f)
			: FLinearColor(0.92f, 0.22f, 0.16f, 0.55f);
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
	const float Width = 12.f * static_cast<float>(Label.Len());
	DrawText(Label, FLinearColor(0.92f, 0.9f, 0.82f, 1.f), (Canvas->SizeX - Width) * 0.5f, 16.f, Font, 1.f, false);
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

	const FString Credits = FString::Printf(TEXT("Credits %d"), Jadeite);
	const FString LumineneText = FString::Printf(TEXT("Luminene %d"), Luminene);
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
	const FString Supply = FString::Printf(TEXT("Supply %d/%d"), SupplyUsed, SupplyCap);
	float CreditsW = 0.f;
	float CreditsH = 0.f;
	float LumineneW = 0.f;
	float LumineneH = 0.f;
	float SupplyW = 0.f;
	float SupplyH = 0.f;
	Canvas->TextSize(Font, Credits, CreditsW, CreditsH);
	Canvas->TextSize(Font, LumineneText, LumineneW, LumineneH);
	Canvas->TextSize(Font, Supply, SupplyW, SupplyH);

	const float Gap = 22.f;
	const float Pad = 10.f;
	const float TextH = FMath::Max3(CreditsH, LumineneH, SupplyH);
	const float TotalW = CreditsW + Gap + LumineneW + Gap + SupplyW;
	const float X = Canvas->SizeX - TotalW - 24.f;
	const float Y = 16.f;
	DrawRect(FLinearColor(0.02f, 0.02f, 0.02f, 0.88f), X - Pad, Y - 6.f, TotalW + Pad * 2.f, TextH + 12.f);
	DrawText(Credits, KodResourceColors::Jadeite(), X, Y, Font, 1.f, false);
	DrawText(LumineneText, KodResourceColors::LumineneCore(), X + CreditsW + Gap, Y, Font, 1.f, false);
	const FLinearColor SupplyInk = bSupplyBlocked
		? FLinearColor(0.95f, 0.22f, 0.18f, 1.f)
		: FLinearColor(0.78f, 0.78f, 0.78f, 1.f);
	DrawText(Supply, SupplyInk, X + CreditsW + Gap + LumineneW + Gap, Y, Font, 1.f, false);
}

void AKodHUD::DrawProductionPanel()
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

	FKodStructureState Structure;
	bool bFound = false;
	for (AActor* Actor : PC->GetLocalSelection())
	{
		if (!Actor)
		{
			continue;
		}
		const FKodEntityId Id = Sim->FindIdForActor(Actor);
		if (Sim->TryGetStructure(Id, Structure))
		{
			bFound = true;
			break;
		}
	}
	if (!bFound)
	{
		return;
	}

	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	if (!Font && GEngine)
	{
		Font = GEngine->GetSmallFont();
	}
	const FString StructureName = Structure.DisplayId.IsNone() ? Structure.DefinitionId.ToString() : Structure.DisplayId.ToString();
	FString Status = StructureName;
	FKodSimEntityState Combat;
	if (Sim->TryGetState(Structure.Id, Combat) && Combat.MaxHealth > 0.f)
	{
		Status += FString::Printf(TEXT("   HP %.0f/%.0f"), Combat.Health, Combat.MaxHealth);
	}
	if (!Structure.bComplete && Structure.BuildTimeTotal > 0)
	{
		const int32 Percent = (Structure.BuildTicksDone * 100) / Structure.BuildTimeTotal;
		Status += FString::Printf(TEXT("   Build %d%%"), Percent);
	}
	DrawText(Status, FLinearColor::White, 24.f, 80.f, Font, 1.f, false);
	if (!Structure.bComplete || Structure.Trains.Num() <= 0)
	{
		return;
	}

	const float X = 24.f;
	float Y = 108.f;
	const FString TrainName = PlayerFacingName(Sim, Structure.Trains[0]).ToString();
	const FString Title = FString::Printf(TEXT("S Train %s  Backspace Cancel"), *TrainName);
	DrawRect(FLinearColor(0.02f, 0.02f, 0.02f, 0.82f), X - 8.f, Y - 8.f, 280.f, 36.f + 22.f * 5.f);
	DrawText(Title, FLinearColor::White, X, Y, Font, 1.f, false);
	Y += 22.f;
	if (Structure.bSupplyBlocked)
	{
		DrawText(TEXT("Supply blocked"), FLinearColor(0.9f, 0.25f, 0.2f, 1.f), X, Y, Font, 1.f, false);
		Y += 18.f;
	}
	for (int32 Index = 0; Index < Structure.Slots.Num(); ++Index)
	{
		const FKodProductionSlot& Slot = Structure.Slots[Index];
		const FString Name = PlayerFacingName(Sim, Slot.DisplayId.IsNone() ? Slot.UnitId : Slot.DisplayId).ToString();
		DrawText(Name, KodResourceColors::Jadeite(), X, Y, Font, 1.f, false);
		float Fraction = 0.f;
		if (Index == 0 && Slot.bStarted && Structure.ActiveBuildTime > 0)
		{
			Fraction = 1.f - (static_cast<float>(Structure.ActiveTicksRemaining) / static_cast<float>(Structure.ActiveBuildTime));
			Fraction = FMath::Clamp(Fraction, 0.f, 1.f);
		}
		DrawRect(FLinearColor(0.15f, 0.15f, 0.15f, 1.f), X + 70.f, Y + 2.f, 140.f, 10.f);
		DrawRect(KodResourceColors::Jadeite(), X + 70.f, Y + 2.f, 140.f * Fraction, 10.f);
		Y += 20.f;
	}
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
	DrawText(TEXT("ISR"), FLinearColor(0.55f, 0.85f, 1.f, 1.f), X, Y - 18.f, Font, 1.f, false);
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
	DrawRect(FLinearColor(0.02f, 0.02f, 0.02f, 0.82f), 16.f, 140.f, 720.f, 28.f);
	DrawText(Hint, KodResourceColors::Jadeite(), 24.f, 144.f, Font, 1.f, false);
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
		}
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
		Text = TEXT("Base under attack");
	}
	else if (CueId == FName(TEXT("units_under_attack")))
	{
		Text = TEXT("Units under attack");
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
	else if (CueId == FName(TEXT("expand_or_be_exposed")))
	{
		Text = TEXT("Expand or be exposed");
	}
	else if (CueId == FName(TEXT("exposed")))
	{
		Text = TEXT("You are exposed");
	}
	else
	{
		return;
	}
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	const bool bAttack = CueId == FName(TEXT("base_under_attack")) || CueId == FName(TEXT("units_under_attack"));
	if (bAttack)
	{
		if (const float* Until = AlertThrottleUntil.Find(CueId))
		{
			if (Now < *Until)
			{
				return;
			}
		}
		AlertThrottleUntil.Add(CueId, Now + 4.f);
	}
	FKodAlertLine Line;
	Line.Text = Text;
	Line.CueId = CueId;
	Line.World = Location;
	Line.Until = Now + 5.f;
	AlertLines.Add(Line);
	while (AlertLines.Num() > 6)
	{
		AlertLines.RemoveAt(0);
	}
	if (!Location.IsNearlyZero())
	{
		AddMinimapPing(Location);
		if (AKodRTSCameraPawn* CameraPawn = Cast<AKodRTSCameraPawn>(GetOwningPawn()))
		{
			CameraPawn->NoteAlert(Location);
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
	const float Scale = FMath::Max(0.5f, Canvas->SizeY / 1080.f);
	float Y = 72.f * Scale;
	if (Cast<AKodSpectatorPawn>(GetOwningPawn()))
	{
		Y = 250.f * Scale;
	}
	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	for (int32 Index = AlertLines.Num() - 1; Index >= 0; --Index)
	{
		const FKodAlertLine& Line = AlertLines[Index];
		const float Alpha = FMath::Clamp((Line.Until - Now) / 1.5f, 0.25f, 1.f);
		const FString Glyph = Line.Text.Left(2).ToUpper();
		DrawRect(FLinearColor(0.05f, 0.06f, 0.07f, 0.86f * Alpha), 16.f * Scale, Y, 280.f * Scale, 28.f * Scale);
		DrawRect(FLinearColor(0.25f, 0.45f, 0.32f, Alpha), 20.f * Scale, Y + 4.f * Scale, 22.f * Scale, 20.f * Scale);
		DrawText(Glyph, FLinearColor(1.f, 1.f, 1.f, Alpha), 22.f * Scale, Y + 6.f * Scale, Font, 0.55f * Scale, false);
		DrawText(Line.Text, FLinearColor(0.92f, 0.9f, 0.82f, Alpha), 48.f * Scale, Y + 6.f * Scale, Font, 0.8f * Scale, false);
		Y += 32.f * Scale;
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
	const float Scale = FMath::Max(0.5f, Canvas->SizeY / 1080.f);
	const float X = 16.f * Scale;
	const float Y = 16.f * Scale;
	const float W = 360.f * Scale;
	DrawRect(FLinearColor(0.03f, 0.04f, 0.05f, 0.92f), X, Y, W, 220.f * Scale);
	UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;
	DrawText(TEXT("Production"), FLinearColor(0.95f, 0.86f, 0.45f, 1.f), X + 8.f * Scale, Y + 4.f * Scale, Font, Scale, false);
	float Row = Y + 24.f * Scale;
	for (int32 TeamId = 0; TeamId <= 1; ++TeamId)
	{
		FKodResourceCost Bank;
		Sim->TryGetBank(TeamId, Bank);
		int32 Used = 0;
		int32 Cap = 0;
		Sim->TryGetSupply(TeamId, Used, Cap);
		const FLinearColor Ink = TeamId == 0 ? FLinearColor(0.3f, 0.85f, 0.4f, 1.f) : FLinearColor(0.9f, 0.25f, 0.18f, 1.f);
		const float BarW = (W - 16.f * Scale) * 0.5f;
		DrawRect(Ink, X + 8.f * Scale + TeamId * BarW, Row, BarW - 4.f * Scale, 8.f * Scale);
		DrawText(FString::Printf(TEXT("T%d  %dC  %dL  %d/%d"), TeamId, Bank.Jadeite, Bank.Luminene, Used, Cap),
			FLinearColor::White, X + 8.f * Scale, Row + 10.f * Scale, Font, 0.75f * Scale, false);
		Row += 28.f * Scale;
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
		float Fraction = 0.f;
		FString Label = Structure.DisplayId.IsNone() ? Structure.DefinitionId.ToString() : Structure.DisplayId.ToString();
		if (bTrain)
		{
			Label = FString::Printf(TEXT("T%d %s"), Structure.TeamId, *Structure.Slots[0].UnitId.ToString());
			if (Structure.ActiveBuildTime > 0)
			{
				Fraction = 1.f - static_cast<float>(Structure.ActiveTicksRemaining) / static_cast<float>(Structure.ActiveBuildTime);
			}
		}
		else if (bResearch)
		{
			Label = FString::Printf(TEXT("T%d %s"), Structure.TeamId, *Structure.Research[0].UnitId.ToString());
			if (Structure.ResearchBuildTime > 0)
			{
				Fraction = 1.f - static_cast<float>(Structure.ResearchTicksRemaining) / static_cast<float>(Structure.ResearchBuildTime);
			}
		}
		else
		{
			Label = FString::Printf(TEXT("T%d %s"), Structure.TeamId, *Label);
			Fraction = static_cast<float>(Structure.BuildTicksDone) / static_cast<float>(Structure.BuildTimeTotal);
		}
		Fraction = FMath::Clamp(Fraction, 0.f, 1.f);
		DrawText(Label, FLinearColor::White, X + 8.f * Scale, Row, Font, 0.7f * Scale, false);
		DrawRect(FLinearColor(0.1f, 0.1f, 0.1f, 1.f), X + 170.f * Scale, Row + 2.f * Scale, 160.f * Scale, 8.f * Scale);
		DrawRect(FLinearColor(0.3f, 0.75f, 0.4f, 1.f), X + 170.f * Scale, Row + 2.f * Scale, 160.f * Scale * Fraction, 8.f * Scale);
		Row += 16.f * Scale;
		++Shown;
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
		const FLinearColor Frame(0.6f, 0.85f, 0.7f, 0.35f + 0.4f * Alpha);
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
	const float X = 16.f;
	const float Y = 180.f;
	const float W = 220.f;
	const float H = 132.f;
	PowerMinX = X;
	PowerMinY = Y;
	PowerMaxX = X + W;
	PowerMaxY = Y + H;
	bHasPowerPanel = true;
	DrawRect(FLinearColor(0.03f, 0.04f, 0.05f, 0.88f), X, Y, W, H);
	DrawText(FString::Printf(TEXT("Powers %d"), Sim->GetPowerPoints(TeamId)), FLinearColor(0.9f, 0.82f, 0.45f, 1.f), X + 8.f, Y + 6.f, Font, 1.f, false);
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
		const bool bReady = bHave && Cooldown <= 0 && Sim->GetPowerPoints(TeamId) >= Cost;
		const float ButtonY = Y + 28.f + Index * 32.f;
		DrawRect(bReady ? FLinearColor(0.12f, 0.18f, 0.14f, 1.f) : FLinearColor(0.08f, 0.08f, 0.08f, 1.f), X + 8.f, ButtonY, W - 16.f, 28.f);
		const FString Line = Cooldown > 0
			? FString::Printf(TEXT("%s %s  %ds"), Keys[Index], Labels[Index], Cooldown / 16)
			: FString::Printf(TEXT("%s %s  %d"), Keys[Index], Labels[Index], Cost);
		DrawText(Line, bReady ? FLinearColor::White : FLinearColor(0.6f, 0.6f, 0.6f, 1.f), X + 14.f, ButtonY + 6.f, Font, 0.85f, false);
		FKodHudCommandHit Hit;
		Hit.bInsideChrome = true;
		Hit.bEnabled = bReady;
		Hit.Action = FName(TEXT("Power"));
		Hit.Payload = Ids[Index];
		Hit.FailReason = bReady ? FName() : FName(TEXT("Energy"));
		Hit.MinX = X + 8.f;
		Hit.MinY = ButtonY;
		Hit.MaxX = X + W - 8.f;
		Hit.MaxY = ButtonY + 28.f;
		Hit.Tooltip = Labels[Index];
		CommandHits.Add(Hit);
	}
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
	if (!bTrainCard && !bBuildCard && !bMarineCard && !bMedic && !bBay && !bDepot && !bBunker)
	{
		return;
	}

	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	if (!Font && GEngine)
	{
		Font = GEngine->GetSmallFont();
	}

	constexpr float Button = 58.f;
	constexpr float Gap = 6.f;
	constexpr int32 Cols = 5;
	constexpr int32 Rows = 3;
	const float CardW = Cols * Button + (Cols - 1) * Gap;
	const float CardH = Rows * Button + (Rows - 1) * Gap;
	const float QueueH = bTrainCard ? 46.f : 0.f;
	const float OriginX = Canvas->SizeX - CardW - 24.f;
	const float OriginY = Canvas->SizeY - CardH - QueueH - 28.f;
	ChromeMinX = OriginX - 8.f;
	ChromeMinY = OriginY - 8.f;
	ChromeMaxX = OriginX + CardW + 8.f;
	ChromeMaxY = OriginY + CardH + QueueH + 8.f;
	bCommandChrome = true;
	DrawRect(FLinearColor(0.03f, 0.04f, 0.03f, 0.9f), ChromeMinX, ChromeMinY, ChromeMaxX - ChromeMinX, ChromeMaxY - ChromeMinY);

	auto AddButton = [this, Font, Button, Gap](int32 Col, int32 Row, float OriginXIn, float OriginYIn, const FString& Label, const FString& Hotkey, FName Action, FName Payload, int32 StructureId, int32 SlotIndex, bool bEnabled, FName Fail, const FString& Tooltip)
	{
		const float X = OriginXIn + Col * (Button + Gap);
		const float Y = OriginYIn + Row * (Button + Gap);
		const bool bRequires = Fail == FName(TEXT("Requires"));
		const FLinearColor Plate = bEnabled
			? FLinearColor(0.08f, 0.16f, 0.1f, 1.f)
			: (bRequires ? FLinearColor(0.14f, 0.14f, 0.14f, 1.f) : FLinearColor(0.06f, 0.09f, 0.07f, 0.85f));
		const FLinearColor Ink = bEnabled
			? FLinearColor::White
			: (bRequires ? FLinearColor(0.45f, 0.45f, 0.45f, 1.f) : FLinearColor(0.62f, 0.66f, 0.6f, 0.8f));
		DrawRect(Plate, X, Y, Button, Button);
		if (!TryDrawIcon(Payload.IsNone() ? FName(*Label) : Payload, X + 4.f, Y + 4.f, 18.f, 18.f))
		{
			const FString Glyph = Label.Left(2).ToUpper();
			DrawRect(FLinearColor(0.2f, 0.35f, 0.28f, 1.f), X + 4.f, Y + 4.f, 18.f, 18.f);
			DrawText(Glyph, FLinearColor::White, X + 5.f, Y + 5.f, Font, 0.55f, false);
		}
		DrawText(Label, Ink, X + 6.f, Y + 24.f, Font, 0.65f, false);
		DrawText(Hotkey, KodResourceColors::LumineneCore(), X + Button - 16.f, Y + Button - 18.f, Font, 0.75f, false);
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

		const float QueueY = OriginY + CardH + 8.f;
		for (int32 Slot = 0; Slot < KodEconomyDefaults::ProductionSlotCap; ++Slot)
		{
			const float X = OriginX + Slot * (Button + Gap);
			const bool bFilled = Slot < Trainer->Slots.Num();
			DrawRect(FLinearColor(0.08f, 0.08f, 0.08f, 1.f), X, QueueY, Button, 36.f);
			if (bFilled)
			{
				const FKodProductionSlot& Job = Trainer->Slots[Slot];
				const FString Name = PlayerFacingName(Sim, Job.DisplayId.IsNone() ? Job.UnitId : Job.DisplayId).ToString();
				DrawText(Name, FLinearColor::White, X + 4.f, QueueY + 2.f, Font, 0.7f, false);
				float Fraction = 0.f;
				if (Slot == 0 && Job.bStarted && Trainer->ActiveBuildTime > 0)
				{
					Fraction = 1.f - (static_cast<float>(Trainer->ActiveTicksRemaining) / static_cast<float>(Trainer->ActiveBuildTime));
					Fraction = FMath::Clamp(Fraction, 0.f, 1.f);
				}
				DrawRect(FLinearColor(0.15f, 0.15f, 0.15f, 1.f), X + 4.f, QueueY + 22.f, Button - 8.f, 8.f);
				DrawRect(KodResourceColors::Jadeite(), X + 4.f, QueueY + 22.f, (Button - 8.f) * Fraction, 8.f);
				FKodHudCommandHit Hit;
				Hit.bInsideChrome = true;
				Hit.bEnabled = true;
				Hit.Action = TEXT("CancelSlot");
				Hit.SlotIndex = Slot;
				Hit.StructureId = Trainer->Id.Value;
				Hit.MinX = X;
				Hit.MinY = QueueY;
				Hit.MaxX = X + Button;
				Hit.MaxY = QueueY + 36.f;
				Hit.Tooltip = TEXT("Cancel");
				CommandHits.Add(Hit);
			}
		}
	}
	else
	{
		if (bMarineCard || bWorker)
		{
			AddButton(0, 1, OriginX, OriginY, TEXT("Move"), TEXT("M"), TEXT("Move"), FName(), 0, INDEX_NONE, true, FName(), TEXT("Move"));
			AddButton(1, 1, OriginX, OriginY, TEXT("Stop"), TEXT("S"), TEXT("Stop"), FName(), 0, INDEX_NONE, true, FName(), TEXT("Stop"));
			AddButton(2, 1, OriginX, OriginY, TEXT("Hold"), TEXT("H"), TEXT("Hold"), FName(), 0, INDEX_NONE, true, FName(), TEXT("Hold"));
			AddButton(3, 1, OriginX, OriginY, TEXT("Patrol"), TEXT("P"), TEXT("Patrol"), FName(), 0, INDEX_NONE, true, FName(), TEXT("Patrol"));
			AddButton(4, 1, OriginX, OriginY, TEXT("Attack"), TEXT("A"), TEXT("Attack"), FName(), 0, INDEX_NONE, true, FName(), TEXT("Attack"));
			AddButton(4, 2, OriginX, OriginY, TEXT("Guard"), TEXT("G"), TEXT("Guard"), FName(), 0, INDEX_NONE, true, FName(), TEXT("Guard"));
		}
		if (bMarineCard)
		{
			FKodAbilitySpec Ability;
			const bool bHave = Sim->TryGetAbilitySpec(FName(KodWardenPaths::Id_CombatStim), Ability);
			const FString Label = (bHave && !Ability.DisplayName.IsNone()) ? Ability.DisplayName.ToString() : TEXT("Stim");
			const FString Hotkey = (bHave && !Ability.Hotkey.IsNone()) ? Ability.Hotkey.ToString() : TEXT("T");
			const int32 Seconds = (bHave ? Ability.DurationTicks : StimDuration) / 16;
			const FString Tooltip = FString::Printf(TEXT("%s\n%d HP\n%ds\n+50%% speed"), *Label, bHave ? Ability.HealthCost : 10, Seconds);
			const FName Fail = bStimReady ? FName() : FName(TEXT("Health"));
			AddButton(0, 0, OriginX, OriginY, Label, Hotkey, TEXT("Ability"), FName(KodWardenPaths::Id_CombatStim), 0, INDEX_NONE, bStimReady, Fail, Tooltip);
			if (StimRemaining > 0)
			{
				const float Fraction = FMath::Clamp(static_cast<float>(StimRemaining) / static_cast<float>(StimDuration), 0.f, 1.f);
				const float X = OriginX;
				const float Y = OriginY;
				DrawRect(FLinearColor(1.f, 0.45f, 0.1f, 0.45f), X, Y, Button, Button * Fraction);
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
		AddButton(0, 0, OriginX, OriginY, Label, TEXT("R"), TEXT("Depot"), FName(), SelectedBuilding.Id.Value, INDEX_NONE, true, FName(), Label);
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
	const bool bVictory = Sim->GetMatchOutcome() == EKodMatchOutcome::Victory;
	const FString Line = bVictory ? TEXT("VICTORY") : TEXT("DEFEAT");
	UFont* Font = GEngine ? GEngine->GetLargeFont() : nullptr;
	if (!Font && GEngine)
	{
		Font = GEngine->GetMediumFont();
	}
	const float Scale = 2.f;
	float Width = 0.f;
	float Height = 0.f;
	Canvas->TextSize(Font, Line, Width, Height);
	Width *= Scale;
	Height *= Scale;
	const float X = (Canvas->SizeX - Width) * 0.5f;
	const float Y = Canvas->SizeY * 0.38f;
	const FLinearColor Ink = bVictory ? FLinearColor(0.55f, 0.9f, 0.45f, 1.f) : FLinearColor(0.95f, 0.28f, 0.22f, 1.f);
	DrawRect(FLinearColor(0.02f, 0.02f, 0.02f, 0.9f), X - 24.f, Y - 12.f, Width + 48.f, Height + 24.f);
	DrawText(Line, Ink, X, Y, Font, Scale, false);
	if (Sim->IsMatchCheated())
	{
		DrawText(TEXT("CHEATED"), FLinearColor(0.95f, 0.75f, 0.2f, 1.f), X, Y + Height + 8.f, Font, 1.f, false);
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
	float Width = 0.f;
	float Height = 0.f;
	Canvas->TextSize(Font, FlashText, Width, Height);
	const float X = (Canvas->SizeX - Width) * 0.5f;
	const float Y = Canvas->SizeY * 0.42f;
	DrawRect(FLinearColor(0.15f, 0.02f, 0.02f, 0.88f), X - 16.f, Y - 8.f, Width + 32.f, Height + 16.f);
	DrawText(FlashText, FLinearColor(1.f, 0.85f, 0.35f, 1.f), X, Y, Font, 1.f, false);
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

	constexpr float BarWidth = 60.f;
	constexpr float BarHeight = 7.f;
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
		const bool bFull = HpState.Health >= HpState.MaxHealth - 0.5f;
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
		const bool bHover = bHaveMouse && FMath::Abs(MouseX - Screen.X) <= 42.f && FMath::Abs(MouseY - Screen.Y) <= 28.f;
		if (HealthBarMode != 2 && bFull && !bSelected && !bHover && !bShowAll)
		{
			continue;
		}

		const float Fraction = FMath::Clamp(HpState.Health / HpState.MaxHealth, 0.f, 1.f);
		const FLinearColor Fill = Fraction >= 0.66f
			? FLinearColor(0.2f, 0.85f, 0.28f, 0.95f)
			: (Fraction >= 0.33f ? FLinearColor(0.92f, 0.78f, 0.16f, 0.95f) : FLinearColor(0.9f, 0.18f, 0.14f, 0.95f));
		const float Left = Screen.X - (BarWidth * 0.5f);
		const float Top = Screen.Y - BarHeight;
		DrawRect(FLinearColor(0.04f, 0.04f, 0.04f, 0.85f), Left, Top, BarWidth, BarHeight);
		DrawRect(Fill, Left, Top, BarWidth * Fraction, BarHeight);
		int32 Step = 10;
		const int32 MaxHP = FMath::Max(1, FMath::RoundToInt(HpState.MaxHealth));
		while (Step < MaxHP && MaxHP / Step > 16)
		{
			Step *= 2;
		}
		const int32 Segments = FMath::Max(1, MaxHP / Step);
		for (int32 Segment = 1; Segment < Segments; ++Segment)
		{
			const float X = Left + BarWidth * (static_cast<float>(Segment) / static_cast<float>(Segments));
			DrawRect(FLinearColor(0.02f, 0.02f, 0.02f, 0.9f), X, Top, 1.f, BarHeight);
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
			? KodResourceColors::LumineneHighlight()
			: KodResourceColors::Jadeite();
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

void AKodHUD::SubmitChat()
{
	const FString Line = ChatLine;
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
			Sim->ApplyCheatLine(Line);
		}
	}
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
	else if (Choice >= 20 && Choice <= 23)
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
	const float Scale = FMath::Max(0.5f, Canvas->SizeY / 1080.f);
	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	auto Button = [this, Font, Scale](float X, float Y, float W, const FString& Label, int32 Choice)
	{
		DrawRect(FLinearColor(0.08f, 0.12f, 0.09f, 0.96f), X, Y, W, 36.f * Scale);
		DrawText(Label, FLinearColor::White, X + 12.f * Scale, Y + 8.f * Scale, Font, Scale, false);
		RememberChrome(FName(TEXT("Menu")), Choice, FName(), X, Y, X + W, Y + 36.f * Scale);
	};
	const float W = 420.f * Scale;
	const float X = (Canvas->SizeX - W) * 0.5f;
	float Y = Canvas->SizeY * 0.22f;
	const float PanelH = (MenuPage == 2 ? 760.f : 520.f) * Scale;
	DrawRect(FLinearColor(0.02f, 0.03f, 0.03f, 0.94f), X - 16.f * Scale, Y - 20.f * Scale, W + 32.f * Scale, PanelH);
	if (MenuPage == 1)
	{
		DrawText(TEXT("Paused"), FLinearColor(0.95f, 0.86f, 0.45f, 1.f), X, Y, Font, 1.4f * Scale, false);
		Y += 48.f * Scale;
		Button(X, Y, W, TEXT("Resume"), 0); Y += 44.f * Scale;
		Button(X, Y, W, TEXT("Save Game"), 1); Y += 44.f * Scale;
		Button(X, Y, W, TEXT("Load Game"), 2); Y += 44.f * Scale;
		Button(X, Y, W, TEXT("Settings"), 3); Y += 44.f * Scale;
		Button(X, Y, W, TEXT("Restart"), 4); Y += 44.f * Scale;
		Button(X, Y, W, TEXT("Quit to Main Menu"), 5);
	}
	else if (MenuPage == 3 || MenuPage == 4)
	{
		const bool bSave = MenuPage == 3;
		DrawText(bSave ? TEXT("Save Game") : TEXT("Load Game"), FLinearColor(0.95f, 0.86f, 0.45f, 1.f), X, Y, Font, 1.2f * Scale, false);
		Y += 48.f * Scale;
		for (int32 Slot = 1; Slot <= 3; ++Slot)
		{
			Button(X, Y, W, FString::Printf(TEXT("Slot %d"), Slot), (bSave ? 9 : 12) + Slot);
			Y += 44.f * Scale;
		}
		Button(X, Y, W, TEXT("Back"), 30);
	}
	else if (MenuPage == 2)
	{
		UKodUserSettings* Settings = UKodUserSettings::Get();
		DrawText(TEXT("Settings"), FLinearColor(0.95f, 0.86f, 0.45f, 1.f), X, Y, Font, 1.2f * Scale, false);
		Y += 40.f * Scale;
		const TCHAR* Bars[] = { TEXT("Selected"), TEXT("Damaged"), TEXT("Always") };
		Button(X, Y, W, FString::Printf(TEXT("Master %.0f%%"), Settings->MasterVolume * 100.f), 40); Y += 40.f * Scale;
		Button(X, Y, W, FString::Printf(TEXT("Music %.0f%%"), Settings->MusicVolume * 100.f), 41); Y += 40.f * Scale;
		Button(X, Y, W, FString::Printf(TEXT("SFX %.0f%%"), Settings->SfxVolume * 100.f), 42); Y += 40.f * Scale;
		Button(X, Y, W, FString::Printf(TEXT("Voice %.0f%%"), Settings->VoiceVolume * 100.f), 43); Y += 40.f * Scale;
		Button(X, Y, W, FString::Printf(TEXT("Edge scroll %.0f"), Settings->EdgeScrollSpeed), 44); Y += 40.f * Scale;
		Button(X, Y, W, FString::Printf(TEXT("Key scroll %.0f"), Settings->KeyboardScrollSpeed), 45); Y += 40.f * Scale;
		Button(X, Y, W, FString::Printf(TEXT("Mouse %.1f"), Settings->MouseSensitivity), 46); Y += 40.f * Scale;
		Button(X, Y, W, FString::Printf(TEXT("Health bars %s"), Bars[FMath::Clamp(Settings->HealthBarMode, 0, 2)]), 47); Y += 40.f * Scale;
		Button(X, Y, W, Settings->bShowHotkeyGrid ? TEXT("Hotkey grid On") : TEXT("Hotkey grid Off"), 48); Y += 40.f * Scale;
		Button(X, Y, W, FString::Printf(TEXT("Graphics %d"), Settings->GraphicsQuality), 49); Y += 40.f * Scale;
		Button(X, Y, W, Sim && Sim->IsExposedRuleEnabled() ? TEXT("Exposed rule On") : TEXT("Exposed rule Off"), 50); Y += 40.f * Scale;
		Button(X, Y, W, Sim && Sim->IsCompetitive() ? TEXT("Competitive On") : TEXT("Competitive Off"), 51); Y += 40.f * Scale;
		Button(X, Y, W, Settings->bCameraShake ? TEXT("Camera shake On") : TEXT("Camera shake Off"), 52); Y += 40.f * Scale;
		Button(X, Y, W, Settings->bShowDamageNumbers ? TEXT("Damage numbers On") : TEXT("Damage numbers Off"), 53); Y += 40.f * Scale;
		Button(X, Y, W, TEXT("Back"), 30);
	}
	else if (MenuPage == 6)
	{
		const bool bVictory = Sim && Sim->GetMatchOutcome() == EKodMatchOutcome::Victory;
		DrawText(bVictory ? TEXT("VICTORY") : TEXT("DEFEAT"), bVictory ? FLinearColor(0.55f, 0.9f, 0.45f, 1.f) : FLinearColor(0.95f, 0.28f, 0.22f, 1.f), X, Y, Font, 1.6f * Scale, false);
		Y += 56.f * Scale;
		if (Sim && Sim->IsMatchCheated())
		{
			DrawText(TEXT("CHEATED"), FLinearColor(0.95f, 0.75f, 0.2f, 1.f), X, Y, Font, Scale, false);
			Y += 36.f * Scale;
		}
		Button(X, Y, W, TEXT("Continue Watching"), 6); Y += 44.f * Scale;
		Button(X, Y, W, TEXT("Score Screen"), 7); Y += 44.f * Scale;
		Button(X, Y, W, TEXT("Restart Skirmish"), 4); Y += 44.f * Scale;
		Button(X, Y, W, TEXT("Main Menu"), 5);
	}
	else if (MenuPage == 8)
	{
		DrawText(TEXT("Kingdom of Dust"), FLinearColor(0.95f, 0.86f, 0.45f, 1.f), X, Y, Font, 1.4f * Scale, false);
		Y += 56.f * Scale;
		Button(X, Y, W, TEXT("Skirmish"), 8); Y += 44.f * Scale;
		Button(X, Y, W, TEXT("Settings"), 3); Y += 44.f * Scale;
		Button(X, Y, W, TEXT("Quit"), 9);
	}
	else if (MenuPage == 5 && Sim)
	{
		const TCHAR* Tabs[] = { TEXT("Overview"), TEXT("Production"), TEXT("Army"), TEXT("Income") };
		DrawText(TEXT("Results"), FLinearColor(0.95f, 0.86f, 0.45f, 1.f), X, Y, Font, 1.2f * Scale, false);
		Y += 40.f * Scale;
		if (Sim->IsMatchCheated())
		{
			DrawText(TEXT("CHEATED"), FLinearColor(0.95f, 0.75f, 0.2f, 1.f), X, Y, Font, Scale, false);
			Y += 32.f * Scale;
		}
		for (int32 Tab = 0; Tab < 4; ++Tab)
		{
			Button(X + Tab * (W / 4.f), Y, W / 4.f - 4.f, Tabs[Tab], 20 + Tab);
		}
		Y += 48.f * Scale;
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
		TArray<UKodSimSubsystem::FKodStatSample> Samples;
		Sim->CopyStatSamples(0, Samples);
		if (ScoreTab == 0)
		{
			DrawText(FString::Printf(TEXT("Time %02d:%02d"), Seconds / 60, Seconds % 60), FLinearColor::White, X, Y, Font, Scale, false); Y += 28.f * Scale;
			DrawText(FString::Printf(TEXT("APM %d  peak %d"), Average, Peak), FLinearColor::White, X, Y, Font, Scale, false); Y += 28.f * Scale;
			DrawText(FString::Printf(TEXT("Collected %d"), Collected), FLinearColor::White, X, Y, Font, Scale, false); Y += 28.f * Scale;
			DrawText(FString::Printf(TEXT("Trained %d   Killed %d"), Trained, Killed), FLinearColor::White, X, Y, Font, Scale, false); Y += 28.f * Scale;
			DrawText(FString::Printf(TEXT("Built %d   Razed %d"), Built, Razed), FLinearColor::White, X, Y, Font, Scale, false);
		}
		else
		{
			float MaxValue = 1.f;
			for (const UKodSimSubsystem::FKodStatSample& Sample : Samples)
			{
				const int32 Value = ScoreTab == 1 ? Sample.Trained + Sample.Built : (ScoreTab == 2 ? Sample.ArmyValue : Sample.Income);
				MaxValue = FMath::Max(MaxValue, static_cast<float>(Value));
			}
			const float ChartW = W;
			const float ChartH = 160.f * Scale;
			FVector2D Prev(X, Y + ChartH);
			for (int32 Index = 0; Index < Samples.Num(); ++Index)
			{
				const int32 Value = ScoreTab == 1 ? Samples[Index].Trained + Samples[Index].Built : (ScoreTab == 2 ? Samples[Index].ArmyValue : Samples[Index].Income);
				const float Px = X + ChartW * (Samples.Num() <= 1 ? 0.f : static_cast<float>(Index) / static_cast<float>(Samples.Num() - 1));
				const float Py = Y + ChartH - ChartH * (static_cast<float>(Value) / MaxValue);
				if (Index > 0)
				{
					DrawLine(Prev.X, Prev.Y, Px, Py, FLinearColor(0.4f, 0.85f, 0.45f, 1.f), 2.f);
				}
				Prev = FVector2D(Px, Py);
			}
		}
		Y += 200.f * Scale;
		Button(X, Y, W, TEXT("Back"), 30);
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
