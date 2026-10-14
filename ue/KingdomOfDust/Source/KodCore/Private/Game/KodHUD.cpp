#include "Game/KodHUD.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HighResScreenshot.h"
#include "Misc/Parse.h"
#include "UnrealClient.h"
#include "Game/KodHudLayout.h"
#include "Game/KodUiArt.h"
#include "Game/KodWireTint.h"

#include "Algo/Sort.h"
#include "Blueprint/UserWidget.h"
#include "Components/CapsuleComponent.h"
#include "Components/ActorComponent.h"
#include "Components/MeshComponent.h"
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
#include "Misc/App.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Game/KodIsrMonitor.h"
#include "Game/KodPlayerController.h"
#include "Game/KodPlayerState.h"
#include "Audio/KodCueSubsystem.h"
#include "Game/KodGameMode.h"
#include "Game/KodSkirmishSetup.h"
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
		return Label;
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
}

FKodHudV3 FKodHudV3::ForFaction(bool bScinder)
{
	FKodHudV3 Layout;
	auto Box = [](float X, float Y, float W, float H) -> FKodHudBox
	{
		FKodHudBox Out;
		Out.X = X;
		Out.Y = Y;
		Out.W = W;
		Out.H = H;
		return Out;
	};
	if (!bScinder)
	{
		Layout.Minimap = Box(15.f, 788.f, 276.f, 265.f);
		Layout.Info = Box(368.f, 862.f, 988.f, 191.f);
		Layout.Portrait = Box(1384.f, 860.f, 123.f, 198.f);
		Layout.Command = Box(1544.f, 825.f, 351.f, 189.f);
		Layout.Wing[0] = Box(1747.f, 760.f, 44.f, 59.f);
		Layout.Wing[1] = Box(1796.f, 761.f, 43.f, 58.f);
		Layout.Wing[2] = Box(1845.f, 758.f, 73.f, 61.f);
		Layout.Menu = Box(1876.f, 4.f, 39.f, 38.f);
		Layout.MinimapTab[0] = Box(6.f, 730.f, 71.f, 51.f);
		Layout.MinimapTab[1] = Box(81.f, 728.f, 78.f, 57.f);
		Layout.MapButton[0] = Box(307.f, 817.f, 49.f, 20.f);
		Layout.MapButton[1] = Box(308.f, 868.f, 52.f, 16.f);
		Layout.MapButton[2] = Box(307.f, 919.f, 53.f, 12.f);
		Layout.Banner = Box(730.f, 0.f, 458.f, 82.f);
		Layout.Clock = Box(267.f, 748.f, 81.f, 36.f);
		Layout.bClockBox = true;
		const float TabX[10] = { 375.f, 443.f, 511.f, 579.f, 647.f, 715.f, 783.f, 852.f, 921.f, 989.f };
		for (int32 Index = 0; Index < 10; ++Index)
		{
			Layout.Tab[Index] = Box(TabX[Index], 831.f, 42.f, 31.f);
		}
		Layout.StoryW = 868.f;
		Layout.StoryH = 1104.f;
		Layout.StoryInner = Box(87.f, 92.f, 693.f, 921.f);
	}
	else
	{
		Layout.Minimap = Box(38.f, 773.f, 265.f, 270.f);
		Layout.Info = Box(383.f, 852.f, 971.f, 190.f);
		Layout.Portrait = Box(1389.f, 844.f, 118.f, 199.f);
		Layout.Command = Box(1530.f, 795.f, 378.f, 261.f);
		Layout.Wing[0] = Box(1733.f, 744.f, 48.f, 66.f);
		Layout.Wing[1] = Box(1792.f, 744.f, 41.f, 66.f);
		Layout.Wing[2] = Box(1835.f, 744.f, 83.f, 66.f);
		Layout.Menu = Box(1882.f, 8.f, 29.f, 28.f);
		Layout.MinimapTab[0] = Box(2.f, 702.f, 83.f, 69.f);
		Layout.MinimapTab[1] = Box(87.f, 703.f, 93.f, 68.f);
		Layout.MapButton[0] = Box(310.f, 796.f, 55.f, 53.f);
		Layout.MapButton[1] = Box(310.f, 850.f, 58.f, 52.f);
		Layout.MapButton[2] = Box(311.f, 903.f, 55.f, 52.f);
		Layout.Banner = Box(727.f, 0.f, 465.f, 96.f);
		Layout.Clock = Box(185.f, 715.f, 80.f, 24.f);
		Layout.bClockBox = false;
		const float TabX[10] = { 384.f, 454.f, 521.f, 589.f, 656.f, 724.f, 793.f, 862.f, 930.f, 998.f };
		for (int32 Index = 0; Index < 10; ++Index)
		{
			Layout.Tab[Index] = Box(TabX[Index], 824.f, 39.f, 25.f);
		}
		Layout.StoryW = 867.f;
		Layout.StoryH = 1118.f;
		Layout.StoryInner = Box(173.f, 140.f, 516.f, 811.f);
	}
	return Layout;
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
	const float Aspect = ViewH > 1.f ? ViewW / ViewH : 1.f;
	float Scale = (Aspect > 2.2f ? (ViewW / 1920.f) : (ViewH / 1080.f)) * User;
	Scale = FMath::Max(Scale, 0.5f);
	const float FrameW = 1920.f * Scale;
	FrameScale = Scale;
	FrameOriginX = (ViewW - FrameW) * 0.5f;
	FrameOriginY = ViewH - 1080.f * Scale;
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

AKodHUD::FKodScreenRect AKodHUD::MapBox(const FKodHudBox& Box) const
{
	return MapSpec(Box.X, Box.Y, Box.W, Box.H);
}

AKodHUD::FKodScreenRect AKodHUD::MapInfo(float X, float Y, float W, float H) const
{
	const FKodHudBox Window = FKodHudV3::ForFaction(HudSkin == 1).Info;
	constexpr float Left = 332.f;
	constexpr float Top = 820.f;
	constexpr float SpanX = 1277.f - Left;
	constexpr float SpanY = 1070.f - Top;
	return MapSpec(
		Window.X + ((X - Left) / SpanX) * Window.W,
		Window.Y + ((Y - Top) / SpanY) * Window.H,
		(W / SpanX) * Window.W,
		(H / SpanY) * Window.H);
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

	const FKodHudV3 Layout = FKodHudV3::ForFaction(bScinder);
	const FKodScreenRect MapWell = MapBox(Layout.Minimap);
	DrawRect(Well, MapWell.X, MapWell.Y, MapWell.W, MapWell.H);
	const FKodScreenRect SelWell = MapBox(Layout.Info);
	DrawRect(Tok(0x1F, 0x23, 0x27, 0.88f), SelWell.X, SelWell.Y, SelWell.W, SelWell.H);
	const FKodScreenRect PortWell = MapBox(Layout.Portrait);
	DrawRect(Well, PortWell.X, PortWell.Y, PortWell.W, PortWell.H);
	DrawRect(Brass, PortWell.X, PortWell.Y, PortWell.W, 2.f * FrameScale);
	const FKodScreenRect CardWell = MapBox(Layout.Command);
	DrawRect(Well, CardWell.X, CardWell.Y, CardWell.W, CardWell.H);
	DrawPaintedFrames();
	DrawTopBar();
}

const UKodHudProposed& AKodHUD::HudStyle()
{
	if (!ProposedStyle)
	{
		ProposedStyle = LoadObject<UKodHudProposed>(nullptr, UKodHudProposed::AssetPath);
	}
	if (!ProposedStyle)
	{
		ProposedStyle = NewObject<UKodHudProposed>(this);
	}
	return *ProposedStyle;
}

void AKodHUD::DrawTopBar()
{
	if (!Canvas)
	{
		return;
	}
	UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;
	const bool bScinder = HudSkin == 1;
	const FLinearColor Plate = bScinder ? Tok(0x6E, 0x5B, 0x4F, 0.94f) : Tok(0x3A, 0x3F, 0x45, 0.94f);
	const FLinearColor Trim = bScinder ? Tok(0xE6, 0x5F, 0x2B) : Tok(0xC9, 0xA2, 0x4A);
	const FLinearColor Ink = bScinder ? Tok(0xDF, 0xD4, 0xC0) : Tok(0xF4, 0xEE, 0xDF);
	const FKodHudV3 Layout = FKodHudV3::ForFaction(bScinder);
	const TCHAR* Labels[] = { TEXT("LOG"), TEXT(""), TEXT("?") };
	const FName Kinds[] = { FName(TEXT("ChatBtn")), FName(), FName(TEXT("HelpBtn")) };
	const bool bLocked[] = { false, true, true };
	const FKodHudBox WingBoxes[] = { Layout.Wing[0], Layout.Wing[1], Layout.Wing[2] };
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const FKodScreenRect Tab = MapBox(WingBoxes[Index]);
		DrawRect(Plate, Tab.X, Tab.Y, Tab.W, Tab.H);
		DrawRect(Trim, Tab.X, Tab.Y, Tab.W, FrameScale);
		DrawRect(Trim, Tab.X, Tab.Y, FrameScale, Tab.H);
		DrawRect(Trim, Tab.X + Tab.W - FrameScale, Tab.Y, FrameScale, Tab.H);
		DrawRect(Trim, Tab.X, Tab.Y + Tab.H - FrameScale, Tab.W, FrameScale);
		DrawText(Labels[Index], Ink, Tab.X + 8.f * FrameScale, Tab.Y + 5.f * FrameScale, Font, FrameScale, false);
		if (!bLoggedWingHelp && Index == 2)
		{
			bLoggedWingHelp = true;
			UE_LOG(LogTemp, Log, TEXT("KodHUD WingBtn Help spec=%.0f,%.0f %.0fx%.0f viewport=%.0f,%.0f %.0fx%.0f canvas=%dx%d scale=%.3f"),
				WingBoxes[2].X, WingBoxes[2].Y, WingBoxes[2].W, WingBoxes[2].H,
				Tab.X, Tab.Y, Tab.W, Tab.H,
				static_cast<int32>(Canvas->SizeX), static_cast<int32>(Canvas->SizeY), FrameScale);
		}
		if (!bLocked[Index] && !Kinds[Index].IsNone())
		{
			RememberChrome(Kinds[Index], 0, FName(), Tab.X, Tab.Y, Tab.X + Tab.W, Tab.Y + Tab.H);
		}
	}
	const FKodScreenRect MenuTab = MapBox(Layout.Menu);
	DrawRect(Plate, MenuTab.X, MenuTab.Y, MenuTab.W, MenuTab.H);
	DrawRect(Trim, MenuTab.X, MenuTab.Y, MenuTab.W, FrameScale);
	DrawText(TEXT("M"), Ink, MenuTab.X + 6.f * FrameScale, MenuTab.Y + 6.f * FrameScale, Font, FrameScale, false);
	RememberChrome(FName(TEXT("MenuBtn")), 0, FName(), MenuTab.X, MenuTab.Y, MenuTab.X + MenuTab.W, MenuTab.Y + MenuTab.H);
}

UTexture2D* AKodHUD::ResolveSoftTexture(const TCHAR* Path)
{
	if (!Path)
	{
		return nullptr;
	}
	const FName Key(Path);
	if (TObjectPtr<UTexture2D>* Cached = SoftTextures.Find(Key))
	{
		return Cached->Get();
	}
	UTexture2D* Texture = Cast<UTexture2D>(FSoftObjectPath(Path).TryLoad());
	SoftTextures.Add(Key, Texture);
	return Texture;
}

void AKodHUD::DrawPaintedFrames()
{
	if (!Canvas)
	{
		return;
	}
	const bool bScinder = HudSkin == 1;
	if (ResolveSoftTexture(*FKodHudV3::FramePath(bScinder)))
	{
		return;
	}
	struct FLayer
	{
		const TCHAR* Path;
		float X;
		float Y;
		float W;
		float H;
		const TCHAR* Name;
	};
	const FLayer FrameLayers[] = {
		{ bScinder ? KodWardenPaths::FrameScMinimap : KodWardenPaths::FrameVgMinimap, KodHudLayout::FrameMinimapX, KodHudLayout::FrameMinimapY, KodHudLayout::FrameMinimapW, KodHudLayout::FrameMinimapH, TEXT("minimap_wing") },
		{ bScinder ? KodWardenPaths::FrameScCentre : KodWardenPaths::FrameVgCentre, KodHudLayout::FrameCentreX, KodHudLayout::FrameCentreY, KodHudLayout::FrameCentreW, KodHudLayout::FrameCentreH, TEXT("centre_info") },
		{ bScinder ? KodWardenPaths::FrameScPortrait : KodWardenPaths::FrameVgPortrait, KodHudLayout::FramePortraitX, KodHudLayout::FramePortraitY, KodHudLayout::FramePortraitW, KodHudLayout::FramePortraitH, TEXT("portrait_frame") },
		{ bScinder ? KodWardenPaths::FrameScCard : KodWardenPaths::FrameVgCard, KodHudLayout::FrameCardX, KodHudLayout::FrameCardY, KodHudLayout::FrameCardW, KodHudLayout::FrameCardH, TEXT("cmdcard_wing") }
	};
	int32 Drawn = 0;
	for (const FLayer& Layer : FrameLayers)
	{
		UTexture2D* Texture = ResolveSoftTexture(Layer.Path);
		if (!Texture)
		{
			continue;
		}
		const FKodScreenRect Rect = MapSpec(Layer.X, Layer.Y, Layer.W, Layer.H);
		DrawTexture(Texture, Rect.X, Rect.Y, Rect.W, Rect.H, 0.f, 0.f, 1.f, 1.f);
		++Drawn;
	}
	if (!bLoggedFrames)
	{
		bLoggedFrames = true;
		UE_LOG(LogTemp, Log, TEXT("KodHUD Frames Faction=%s Drawn=%d/4"), bScinder ? TEXT("SC") : TEXT("VG"), Drawn);
	}
}

void AKodHUD::ShowAchievement(const FText& Text)
{
	AchievementText = Text.ToString();
	AchievementBorn = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
}

void AKodHUD::ShowStoryWindow(const FText& Text)
{
	StoryText = Text.ToString();
	bStoryWindow = !StoryText.IsEmpty();
}

void AKodHUD::HideStoryWindow()
{
	bStoryWindow = false;
	StoryText.Reset();
}

void AKodHUD::DrawPaintedV3()
{
	if (!Canvas)
	{
		return;
	}
	const bool bScinder = HudSkin == 1;
	UTexture2D* Frame = ResolveSoftTexture(*FKodHudV3::FramePath(bScinder));
	if (!Frame)
	{
		return;
	}
	const FKodHudV3 Layout = FKodHudV3::ForFaction(bScinder);
	const FKodScreenRect Full = MapSpec(0.f, 0.f, 1920.f, 1080.f);
	if (Frame->HasAlphaChannel())
	{
		DrawTexture(Frame, Full.X, Full.Y, Full.W, Full.H, 0.f, 0.f, 1.f, 1.f);
	}
	else
	{
		static bool bLoggedFrame = false;
		if (!bLoggedFrame)
		{
			bLoggedFrame = true;
			UE_LOG(LogTemp, Warning, TEXT("KodHUD FrameSkip NoAlpha Size=%dx%d"), Frame->GetSizeX(), Frame->GetSizeY());
		}
	}
	if (!bScinder)
	{
		if (UTexture2D* Lights = ResolveSoftTexture(*FKodHudV3::LightsPath()))
		{
			// A missing or opaque lights sheet stretched over 1920x1080 paints the whole view orange, HUD included.
			if (Lights->GetResource() && Lights->HasAlphaChannel() && Lights->GetSizeX() >= 256 && Lights->GetSizeY() >= 256)
			{
				const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
				const float Alpha = 0.8f + 0.2f * FMath::Sin(Now * 2.f * PI / 2.4f);
				FCanvasTileItem Tile(FVector2D(Full.X, Full.Y), Lights->GetResource(), FVector2D(Full.W, Full.H), FLinearColor(1.f, 1.f, 1.f, Alpha));
				Tile.BlendMode = SE_BLEND_Additive;
				Canvas->DrawItem(Tile);
			}
			else
			{
				static bool bLoggedLights = false;
				if (!bLoggedLights)
				{
					bLoggedLights = true;
					UE_LOG(LogTemp, Warning, TEXT("KodHUD LightsSkip Alpha=%d Size=%dx%d"),
						Lights->HasAlphaChannel() ? 1 : 0,
						Lights->GetSizeX(),
						Lights->GetSizeY());
				}
			}
		}
	}
	const AKodPlayerController* PC = Cast<AKodPlayerController>(GetOwningPlayerController());
	for (int32 Index = 0; Index < FKodHudV3::TabCount(); ++Index)
	{
		const int32 Group = Index == 9 ? 0 : Index + 1;
		const bool bAssigned = PC && PC->GetControlGroupCount(Group) > 0;
		const FKodScreenRect Tab = MapBox(Layout.Tab[Index]);
		if (UTexture2D* TabTex = ResolveSoftTexture(*FKodHudV3::TabPath(bScinder, Index, bAssigned)))
		{
			DrawTexture(TabTex, Tab.X, Tab.Y, Tab.W, Tab.H, 0.f, 0.f, 1.f, 1.f);
		}
	}
	if (AchievementBorn >= 0.f)
	{
		const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
		const float Age = Now - AchievementBorn;
		float Alpha = 0.f;
		if (Age < 0.25f)
		{
			Alpha = Age / 0.25f;
		}
		else if (Age < 3.25f)
		{
			Alpha = 1.f;
		}
		else if (Age < 3.65f)
		{
			Alpha = 1.f - (Age - 3.25f) / 0.4f;
		}
		if (Alpha > 0.f)
		{
			const FKodScreenRect Banner = MapBox(Layout.Banner);
			if (UTexture2D* BannerTex = ResolveSoftTexture(*FKodHudV3::BannerPath(bScinder)))
			{
				DrawTexture(BannerTex, Banner.X, Banner.Y, Banner.W, Banner.H, 0.f, 0.f, 1.f, 1.f, FLinearColor(1.f, 1.f, 1.f, Alpha));
			}
			UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
			DrawText(AchievementText, FLinearColor(0.96f, 0.93f, 0.87f, Alpha), Banner.X + 16.f * FrameScale, Banner.Y + 28.f * FrameScale, Font, FrameScale, false);
		}
	}
	const UWorld* World = GetWorld();
	const UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	const bool bStoryMode = Sim && Sim->GetMatchMode() == EKodMatchMode::Campaign;
	if (bStoryWindow && bStoryMode)
	{
		const float DrawW = Layout.StoryW * 0.5f;
		const float DrawH = Layout.StoryH * 0.5f;
		const float OriginX = (1920.f - DrawW) * 0.5f;
		const float OriginY = (1080.f - DrawH) * 0.5f;
		const FKodScreenRect Window = MapSpec(OriginX, OriginY, DrawW, DrawH);
		const FKodScreenRect Glass = MapSpec(
			OriginX + Layout.StoryInner.X * 0.5f,
			OriginY + Layout.StoryInner.Y * 0.5f,
			Layout.StoryInner.W * 0.5f,
			Layout.StoryInner.H * 0.5f);
		DrawRect(FLinearColor(0.05f, 0.06f, 0.07f, 0.85f), Glass.X, Glass.Y, Glass.W, Glass.H);
		if (UTexture2D* StoryTex = ResolveSoftTexture(*FKodHudV3::StoryPath(bScinder)))
		{
			DrawTexture(StoryTex, Window.X, Window.Y, Window.W, Window.H, 0.f, 0.f, 1.f, 1.f);
		}
		ResolveSoftTexture(*FKodHudV3::StoryMaskPath(bScinder));
		UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;
		DrawText(StoryText, Tok(0xF4, 0xEE, 0xDF), Glass.X + 8.f * FrameScale, Glass.Y + 8.f * FrameScale, Font, FrameScale, false);
	}
}

namespace KodShotAfterState
{
	float Seconds = -1.f;
	int32 Rearm = 0;
}

bool AKodHUD::ParseShotAfterSeconds(const TCHAR* CommandLine, float& OutSeconds)
{
	float Seconds = -1.f;
	const bool bDashed = CommandLine && FParse::Value(CommandLine, TEXT("-kod.ShotAfter="), Seconds);
	const bool bPlain = !bDashed && CommandLine && FParse::Value(CommandLine, TEXT("kod.ShotAfter="), Seconds);
	if ((!bDashed && !bPlain) || Seconds < 0.f)
	{
		return false;
	}
	OutSeconds = Seconds;
	return true;
}

void AKodHUD::ArmShotAfter(float Seconds)
{
	KodShotAfterState::Seconds = FMath::Max(0.f, Seconds);
	++KodShotAfterState::Rearm;
}

void AKodHUD::LogShotScene()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	int32 AutoExposure = -1;
	float PreExposure = -1.f;
	if (IConsoleVariable* Exposure = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DefaultFeature.AutoExposure")))
	{
		AutoExposure = Exposure->GetInt();
	}
	if (IConsoleVariable* Override = IConsoleManager::Get().FindConsoleVariable(TEXT("r.EyeAdaptation.PreExposureOverride")))
	{
		PreExposure = Override->GetFloat();
		if (AutoExposure == 0 && PreExposure == 0.f)
		{
			Override->Set(1.f, ECVF_SetByCode);
			PreExposure = 1.f;
			UE_LOG(LogTemp, Log, TEXT("KodShotAfter PreExposureOverride=1 because AutoExposure is off"));
		}
	}
	int32 Directional = 0;
	int32 Sky = 0;
	int32 SkyRecaptured = 0;
	float SunIntensity = 0.f;
	FRotator SunRotation = FRotator::ZeroRotator;
	bool bHaveSun = false;
	for (TActorIterator<ADirectionalLight> It(World); It; ++It)
	{
		++Directional;
		if (!bHaveSun)
		{
			bHaveSun = true;
			SunRotation = It->GetActorRotation();
			if (const UDirectionalLightComponent* Light = Cast<UDirectionalLightComponent>(It->GetLightComponent()))
			{
				SunIntensity = Light->Intensity;
			}
		}
	}
	for (TActorIterator<ASkyLight> It(World); It; ++It)
	{
		++Sky;
		if (USkyLightComponent* SkyLight = It->GetLightComponent())
		{
			if (!SkyLight->bRealTimeCapture)
			{
				SkyLight->RecaptureSky();
				++SkyRecaptured;
			}
		}
	}
	int32 UnboundPost = 0;
	int32 ManualPost = 0;
	for (TActorIterator<APostProcessVolume> It(World); It; ++It)
	{
		if (!It->bUnbound)
		{
			continue;
		}
		++UnboundPost;
		if (It->Settings.bOverride_AutoExposureMethod && It->Settings.AutoExposureMethod == AEM_Manual)
		{
			++ManualPost;
		}
	}
	FVector CameraLocation = FVector::ZeroVector;
	FRotator CameraRotation = FRotator::ZeroRotator;
	FVector PawnLocation = FVector::ZeroVector;
	FString ViewName = TEXT("none");
	const APlayerController* PC = GetOwningPlayerController();
	if (!PC)
	{
		PC = World->GetFirstPlayerController();
	}
	if (PC)
	{
		if (const AActor* ViewTarget = PC->GetViewTarget())
		{
			ViewName = ViewTarget->GetName();
			PawnLocation = ViewTarget->GetActorLocation();
		}
		if (const APlayerCameraManager* Manager = PC->PlayerCameraManager)
		{
			CameraLocation = Manager->GetCameraLocation();
			CameraRotation = Manager->GetCameraRotation();
		}
	}
	FIntPoint ViewportSize(0, 0);
	if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
	{
		ViewportSize = GEngine->GameViewport->Viewport->GetSizeXY();
	}
	UE_LOG(LogTemp, Log, TEXT("KodShotAfter Scene View=%s Pawn=%.0f,%.0f,%.0f Camera=%.0f,%.0f,%.0f Rot=P%.1f Y%.1f R%.1f DirLights=%d SunIntensity=%.2f SunRot=P%.1f Y%.1f SkyLights=%d SkyRecapture=%d UnboundPost=%d ManualExposure=%d AutoExposure=%d PreExposure=%.2f Viewport=%dx%d"),
		*ViewName,
		PawnLocation.X, PawnLocation.Y, PawnLocation.Z,
		CameraLocation.X, CameraLocation.Y, CameraLocation.Z,
		CameraRotation.Pitch, CameraRotation.Yaw, CameraRotation.Roll,
		Directional, SunIntensity, SunRotation.Pitch, SunRotation.Yaw,
		Sky, SkyRecaptured, UnboundPost, ManualPost,
		AutoExposure, PreExposure,
		ViewportSize.X, ViewportSize.Y);
	UE_LOG(LogTemp, Log, TEXT("KodShotAfter HighResShot reads the scene view with the HUD drawn on top. A black world with a visible HUD is the scene, not a UI-only capture."));
}

void AKodHUD::PollShotAfter()
{
	UWorld* World = GetWorld();
	static bool bGateParsed = false;
	static bool bGateSelectMove = false;
	static bool bGateZoom = false;
	static bool bGateActed = false;
	static bool bGatePrepared = false;
	static bool bGateQueued = false;
	static bool bGateExitArmed = false;
	static float GateArmSeconds = 0.f;
	if (!bGateParsed)
	{
		bGateParsed = true;
		int32 SelectFlag = 0;
		int32 ZoomFlag = 0;
		const TCHAR* CommandLine = FCommandLine::Get();
		bGateSelectMove = CommandLine && FParse::Value(CommandLine, TEXT("-kod.ShotSelectMove="), SelectFlag) && SelectFlag != 0;
		bGateZoom = CommandLine && FParse::Value(CommandLine, TEXT("-kod.ShotZoom="), ZoomFlag) && ZoomFlag != 0;
	}
	if ((bGateSelectMove || bGateZoom) && World)
	{
		const float Now = World->GetTimeSeconds();
		if (!bGateActed)
		{
			bGateActed = true;
			GateArmSeconds = Now;
			AKodPlayerController* GatePC = Cast<AKodPlayerController>(GetOwningPlayerController());
			if (!GatePC)
			{
				GatePC = Cast<AKodPlayerController>(World->GetFirstPlayerController());
			}
			if (bGateSelectMove && GatePC)
			{
				int32 WorkerId = MAX_int32;
				const int32 TeamId = ReadLocalTeamId();
				for (const FKodSimEntityState& State : FrameStates)
				{
					if (State.TeamId == TeamId && State.bCanGather && State.bMobile && State.Health > 0.f && State.Id.Value < WorkerId)
					{
						WorkerId = State.Id.Value;
					}
				}
				if (WorkerId != MAX_int32)
				{
					FKodEntityId Worker;
					Worker.Value = WorkerId;
					TArray<FKodEntityId> Only;
					Only.Add(Worker);
					GatePC->SetLocalSelectionFromIds(Only, false);
					FVector WorkerAt = FVector::ZeroVector;
					for (const FKodSimEntityState& State : FrameStates)
					{
						if (State.Id.Value == WorkerId)
						{
							WorkerAt = State.Position;
							break;
						}
					}
					GatePC->IssueMoveToSelection(WorkerAt + FVector(800.f, 0.f, 0.f));
					UE_LOG(LogTemp, Log, TEXT("KodShotSelectMove Worker=%d"), WorkerId);
				}
			}
			if (bGateZoom)
			{
				if (AKodRTSCameraPawn* CameraPawn = GatePC ? Cast<AKodRTSCameraPawn>(GatePC->GetPawn()) : nullptr)
				{
					CameraPawn->Zoom(3.f);
					UE_LOG(LogTemp, Log, TEXT("KodShotZoom Steps=3 Arm=%.0f"), CameraPawn->GetCurrentArmLength());
				}
			}
		}
		const float Wait = bGateZoom ? 3.f : 2.f;
		if (!bGateQueued)
		{
			if ((Now - GateArmSeconds) < Wait)
			{
				return;
			}
			if (!bGatePrepared)
			{
				bGatePrepared = true;
				LogShotScene();
				return;
			}
			bGateQueued = true;
			GetHighResScreenshotConfig().SetResolution(1920, 1080);
			if (GEngine)
			{
				GEngine->Exec(World, TEXT("HighResShot 1920x1080"));
			}
			UE_LOG(LogTemp, Log, TEXT("KodShotGate HighResShot 1920x1080"));
			return;
		}
		if (!bGateExitArmed)
		{
			bGateExitArmed = true;
			return;
		}
		if (FScreenshotRequest::IsScreenshotRequested() && (Now - GateArmSeconds) < Wait + 8.f)
		{
			return;
		}
		UE_LOG(LogTemp, Log, TEXT("KodShotGate Exit"));
		FPlatformMisc::RequestExit(false);
		return;
	}
	if (!bShotAfterParsed)
	{
		bShotAfterParsed = true;
		float Seconds = 0.f;
		if (ParseShotAfterSeconds(FCommandLine::Get(), Seconds))
		{
			ArmShotAfter(Seconds);
		}
	}
	if (ShotRearmSeen != KodShotAfterState::Rearm)
	{
		ShotRearmSeen = KodShotAfterState::Rearm;
		ShotAfterSeconds = KodShotAfterState::Seconds;
		ShotArmWorldSeconds = World ? World->GetTimeSeconds() : 0.f;
		bShotPrepared = false;
		bShotQueued = false;
		bShotExitArmed = false;
		UE_LOG(LogTemp, Log, TEXT("KodShotAfter Start Seconds=%.2f"), ShotAfterSeconds);
	}
	if (ShotAfterSeconds < 0.f || !World)
	{
		return;
	}
	const float Now = World->GetTimeSeconds();
	if (!bShotQueued)
	{
		if ((Now - ShotArmWorldSeconds) < ShotAfterSeconds)
		{
			return;
		}
		if (!bShotPrepared)
		{
			bShotPrepared = true;
			LogShotScene();
			return;
		}
		bShotQueued = true;
		GetHighResScreenshotConfig().SetResolution(1920, 1080);
		if (GEngine)
		{
			GEngine->Exec(World, TEXT("HighResShot 1920x1080"));
		}
		UE_LOG(LogTemp, Log, TEXT("KodShotAfter HighResShot 1920x1080 scene+ui"));
		return;
	}
	if (!bShotExitArmed)
	{
		bShotExitArmed = true;
		return;
	}
	if (FScreenshotRequest::IsScreenshotRequested() && (Now - ShotArmWorldSeconds) < ShotAfterSeconds + 8.f)
	{
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("KodShotAfter Exit"));
	ShotAfterSeconds = -1.f;
	FPlatformMisc::RequestExit(false);
}

namespace
{
	void KodShotAfterCommand(const TArray<FString>& Args, UWorld* World)
	{
		(void)World;
		const float Seconds = Args.Num() > 0 ? FMath::Max(0.f, FCString::Atof(*Args[0])) : 0.f;
		AKodHUD::ArmShotAfter(Seconds);
		UE_LOG(LogTemp, Log, TEXT("KodShotAfter Armed Seconds=%.2f"), Seconds);
	}

	void KodAchievementCommand(const TArray<FString>& Args, UWorld* World)
	{
		FString Text = FString::Join(Args, TEXT(" "));
		if (Text.Len() >= 2 && Text.StartsWith(TEXT("\"")) && Text.EndsWith(TEXT("\"")))
		{
			Text = Text.Mid(1, Text.Len() - 2);
		}
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		AKodHUD* HUD = PC ? Cast<AKodHUD>(PC->GetHUD()) : nullptr;
		if (!HUD)
		{
			UE_LOG(LogTemp, Warning, TEXT("kod.Achievement: no HUD"));
			return;
		}
		HUD->ShowAchievement(FText::FromString(Text));
		UE_LOG(LogTemp, Log, TEXT("KodAchievement Show %s"), *Text);
	}

	FAutoConsoleCommandWithWorldAndArgs GKodAchievement(
		TEXT("kod.Achievement"),
		TEXT("Show the painted achievement banner. Usage: kod.Achievement \"text\""),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&KodAchievementCommand));

	FAutoConsoleCommandWithWorldAndArgs GKodShotAfter(
		TEXT("kod.ShotAfter"),
		TEXT("Take HighResShot 1920x1080 after N seconds, then exit. Usage: kod.ShotAfter 8 or -kod.ShotAfter=8"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&KodShotAfterCommand));
}

void AKodHUD::OpenFactionPrompt()
{
	bFactionPrompt = true;
	bFactionPromptDrawn = false;
	FactionPromptIndex = 0;
	UE_LOG(LogTemp, Log, TEXT("KodFaction Prompt Shown"));
}

void AKodHUD::CloseFactionPrompt()
{
	bFactionPrompt = false;
}

void AKodHUD::SetFactionPromptIndex(int32 Index)
{
	FactionPromptIndex = Index == 1 ? 1 : 0;
}

void AKodHUD::DrawFactionPrompt()
{
	if (!Canvas || !bFactionPrompt)
	{
		return;
	}
	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	if (!Font && GEngine)
	{
		Font = GEngine->GetSmallFont();
	}
	DrawRect(FLinearColor(0.02f, 0.03f, 0.04f, 0.72f), 0.f, 0.f, Canvas->SizeX, Canvas->SizeY);
	const FKodScreenRect Title = MapSpec(560.f, 220.f, 800.f, 48.f);
	DrawText(TEXT("CHOOSE A FACTION"), Tok(0xF4, 0xEE, 0xDF), Title.X, Title.Y, Font, FrameScale, false);
	const TCHAR* Names[] = { TEXT("VANGUARD"), TEXT("SCINDER") };
	const TCHAR* Marks[] = { TEXT("V"), TEXT("S") };
	for (int32 Index = 0; Index < 2; ++Index)
	{
		const FKodScreenRect Tile = MapSpec(660.f, 300.f + 160.f * static_cast<float>(Index), 600.f, 140.f);
		const bool bOn = FactionPromptIndex == Index;
		const FLinearColor Fill = bOn
			? (Index == 0 ? Tok(0x3A, 0x3F, 0x45, 0.95f) : Tok(0x6E, 0x5B, 0x4F, 0.95f))
			: Tok(0x14, 0x16, 0x18, 0.92f);
		const FLinearColor Trim = bOn ? Tok(0xC9, 0xA2, 0x4A) : Tok(0x5C, 0x64, 0x6C);
		DrawRect(Fill, Tile.X, Tile.Y, Tile.W, Tile.H);
		DrawRect(Trim, Tile.X, Tile.Y, Tile.W, 2.f * FrameScale);
		DrawText(Marks[Index], Tok(0xF4, 0xEE, 0xDF), Tile.X + 24.f * FrameScale, Tile.Y + 36.f * FrameScale, Font, 1.6f * FrameScale, false);
		DrawText(Names[Index], Tok(0xF4, 0xEE, 0xDF), Tile.X + 90.f * FrameScale, Tile.Y + 48.f * FrameScale, Font, FrameScale, false);
		RememberChrome(FName(TEXT("FactionPrompt")), Index, FName(), Tile.X, Tile.Y, Tile.X + Tile.W, Tile.Y + Tile.H);
	}
	const FKodScreenRect Hint = MapSpec(660.f, 640.f, 600.f, 32.f);
	DrawText(TEXT("1 Vanguard    2 Scinder    Enter"), Tok(0xC9, 0xA2, 0x4A), Hint.X, Hint.Y, Font, 0.8f * FrameScale, false);
	bFactionPromptDrawn = true;
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
	DrawConstructionBars();
	DrawPlacementGrid();
	DrawClickMarkers();
	DrawSelectionPulse();
	DrawJuiceFloaters();
	DrawSelectedResourceNodes();
	DrawSelectionMarquee();
	DrawHotkeyLegend();
	DrawThreatRings();
	DrawIsrPopup();
	DrawPaintedV3();
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
	DrawFactionPrompt();
	PollShotAfter();
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
		FogGpuTick = MIN_int64;
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
			const bool bUpload = FApp::CanEverRender()
				&& !bFogUploadBusy
				&& (FogGpuTick == MIN_int64 || Sim->GetSimTickIndex() - FogGpuTick >= 16);
			if (bUpload)
			{
				const int32 ByteCount = GridW * GridH * 4;
				FogUploadBytes.SetNumUninitialized(ByteCount);
				FMemory::Memcpy(FogUploadBytes.GetData(), Bits, ByteCount);
				FogUploadRegion = FUpdateTextureRegion2D(0, 0, 0, 0, static_cast<uint32>(GridW), static_cast<uint32>(GridH));
			}
			Platform->Mips[0].BulkData.Unlock();
			// Headless soaks never run the upload callback, so a new buffer every tick
			// leaked about 24 MB per 30 s. Skip the GPU copy when nothing will render.
			if (bUpload)
			{
				bFogUploadBusy = true;
				FogGpuTick = Sim->GetSimTickIndex();
				TWeakObjectPtr<AKodHUD> Self(this);
				FogSheetTexture->UpdateTextureRegions(
					0,
					1,
					&FogUploadRegion,
					static_cast<uint32>(GridW * 4),
					4,
					FogUploadBytes.GetData(),
					[Self](uint8*, const FUpdateTextureRegion2D*)
					{
						if (AKodHUD* Hud = Self.Get())
						{
							Hud->bFogUploadBusy = false;
						}
					});
			}
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
	const FKodHudV3 Layout = FKodHudV3::ForFaction(HudSkin == 1);
	const FKodScreenRect Clock = MapBox(Layout.Clock);
	if (Layout.bClockBox)
	{
		if (UTexture2D* ClockTex = ResolveSoftTexture(*FKodHudV3::ClockBoxPath()))
		{
			DrawTexture(ClockTex, Clock.X, Clock.Y, Clock.W, Clock.H, 0.f, 0.f, 1.f, 1.f);
		}
	}
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

	const FString JadeiteText = FString::Printf(TEXT("%d"), Jadeite);
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
	const bool bScinder = HudSkin == 1;
	const FLinearColor ValueInk = bScinder ? Tok(0xEF, 0xE6, 0xD4) : Tok(0xF4, 0xEE, 0xDF);
	const TCHAR* Icon28[4] = {
		bScinder ? KodWardenPaths::IconWorkerSc28 : KodWardenPaths::IconWorkerVg28,
		KodWardenPaths::IconJadeite28,
		KodWardenPaths::IconLuminene28,
		bScinder ? KodWardenPaths::IconSupplySc28 : KodWardenPaths::IconSupplyVg28
	};
	const TCHAR* Icon64[4] = {
		bScinder ? KodWardenPaths::IconWorkerSc64 : KodWardenPaths::IconWorkerVg64,
		KodWardenPaths::IconJadeite64,
		KodWardenPaths::IconLuminene64,
		bScinder ? KodWardenPaths::IconSupplySc64 : KodWardenPaths::IconSupplyVg64
	};
	const TCHAR* Placeholders[4] = { TEXT("W"), TEXT("J"), TEXT("L"), TEXT("S") };
	const FString ChipText[4] = { WorkerText, JadeiteText, LumineneText, Supply };
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	const float SupplyPulse = bSupplyBlocked ? (1.f + 0.08f * FMath::Abs(FMath::Sin(Now * PI / 0.15f))) : 1.f;
	if (bSupplyBlocked && !bSupplyErrorLatched)
	{
		PushErrorLine(TEXT("Not enough supply"));
		bSupplyErrorLatched = true;
	}
	else if (!bSupplyBlocked)
	{
		bSupplyErrorLatched = false;
	}
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const FKodScreenRect Icon = MapSpec(KodHudLayout::ResourceOriginX[Index], 1.f, KodHudLayout::ResourceIcon, KodHudLayout::ResourceIcon);
		UTexture2D* Texture = ResolveResourceIcon(Icon28[Index], Icon64[Index]);
		if (Texture)
		{
			DrawTexture(Texture, Icon.X, Icon.Y, Icon.W, Icon.H, 0.f, 0.f, 1.f, 1.f);
		}
		else
		{
			DrawRect(Tok(0x3A, 0x3F, 0x45, 0.85f), Icon.X, Icon.Y, Icon.W, Icon.H);
			DrawText(Placeholders[Index], ValueInk, Icon.X + 6.f * FrameScale, Icon.Y + 4.f * FrameScale, Font, 0.8f * FrameScale, false);
		}
		const bool bCap = Index == 3 && bSupplyBlocked;
		const FLinearColor Ink = bCap ? Tok(0xFF, 0x4A, 0x4A) : ValueInk;
		const float Scale = (bCap ? SupplyPulse : 1.f) * FrameScale;
		const FKodScreenRect Value = MapSpec(KodHudLayout::ResourceOriginX[Index] + KodHudLayout::ResourceValueGap, 2.f, 80.f, 24.f);
		DrawText(ChipText[Index], FLinearColor(0.f, 0.f, 0.f, 0.65f), Value.X + FrameScale, Value.Y + 2.f * FrameScale, Font, Scale, false);
		DrawText(ChipText[Index], Ink, Value.X, Value.Y, Font, Scale, false);
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
	PushErrorLine(Text);
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
	if (AlertLines.Num() > 0 && AlertLines.Last().CueId == CueId && AlertLines.Last().Text == Text && (Now - AlertLines.Last().Born) <= 4.f)
	{
		++AlertLines.Last().Count;
		AlertLines.Last().Until = Now + 5.f;
	}
	else
	{
	FKodAlertLine Line;
	Line.Text = Text;
	Line.CueId = CueId;
	Line.World = Location;
	Line.Until = Now + 5.f;
	Line.Born = Now;
	Line.Count = 1;
	AlertLines.Add(Line);
	while (AlertLines.Num() > 5)
	{
		AlertLines.RemoveAt(0);
	}
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
	(void)UnitId;
	const UKodHudProposed& HeatStyle = HudStyle();
	const FLinearColor Shared = bFlash ? KodWireTint::Critical() : KodWireTint::FromHitPointsPermille(Health, MaxHealth, HeatStyle.WireHealthyPermille, HeatStyle.WireWarningPermille, false);
	auto ColorOf = [Shared](int32 Zone)
	{
		(void)Zone;
		return Shared;
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
	int32 MaxRows = 5;
	if (ErrorLines.Num() > 0)
	{
		const float ClipY = MapSpec(0.f, HudStyle().AlertClipY, 1.f, 1.f).Y;
		const float Step = (52.f + 8.f) * FrameScale;
		const float RowH = 52.f * FrameScale;
		const float Room = ClipY - Feed.Y - RowH;
		MaxRows = FMath::Clamp(FMath::FloorToInt(Room / FMath::Max(1.f, Step)) + 1, 0, 5);
	}
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
		if (Line.Count > 1)
		{
			const float Badge = 18.f * FrameScale;
			const float BadgeX = RowX + 46.f * FrameScale - Badge * 0.5f;
			const float BadgeY = RowY + 46.f * FrameScale - Badge * 0.5f;
			DrawRect(Tok(0xC9, 0xA2, 0x4A, Alpha), BadgeX, BadgeY, Badge, Badge);
			DrawText(FString::FromInt(Line.Count), Tok(0x00, 0x00, 0x00, Alpha), BadgeX + 3.f * FrameScale, BadgeY + 1.f * FrameScale, Font, 0.55f * FrameScale, false);
		}
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
	if (AlertLines.Num() > 0 && AlertLines.Last().Text == Text && (Now - AlertLines.Last().Born) <= 4.f)
	{
		++AlertLines.Last().Count;
		AlertLines.Last().Until = Now + 5.f;
		return;
	}
	FKodAlertLine Line;
	Line.Text = Text;
	Line.CueId = FName();
	Line.Until = Now + 5.f;
	Line.Born = Now;
	Line.Count = 1;
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
		FlashText = TEXT("Not enough Jadeite");
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
	PushErrorLine(FlashText);
	FlashText.Reset();
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
	if (UTexture2D* Texture = ResolveArtTexture(IconId, false))
	{
		DrawTexture(Texture, X, Y, SizeX, SizeY, 0.f, 0.f, 1.f, 1.f);
		return true;
	}
	return false;
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

	const float Gap = 4.f * FrameScale;
	constexpr int32 Cols = KodHudLayout::CommandCols;
	constexpr int32 Rows = KodHudLayout::CommandRows;
	const FKodScreenRect Card = MapBox(FKodHudV3::ForFaction(HudSkin == 1).Command);
	const float FitW = (Card.W - Gap * static_cast<float>(Cols - 1)) / static_cast<float>(Cols);
	const float FitH = (Card.H - Gap * static_cast<float>(Rows - 1)) / static_cast<float>(Rows);
	const float Button = FMath::Max(8.f, FMath::Min(FitW, FitH));
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
		const FName IconId = Payload.IsNone() ? FName(*Label) : Payload;
		DrawIconSlot(IconId, X, Y, Button, Button, bEnabled ? 1.f : 0.45f);
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
	if (MenuPage == 5 || MenuPage == 6)
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

UTexture2D* AKodHUD::ResolveResourceIcon(const TCHAR* Size28, const TCHAR* Size64)
{
	const FName Key(Size28);
	if (TObjectPtr<UTexture2D>* Cached = ResourceIcons.Find(Key))
	{
		return Cached->Get();
	}
	UTexture2D* Texture = Cast<UTexture2D>(FSoftObjectPath(Size28).TryLoad());
	if (!Texture && Size64)
	{
		Texture = Cast<UTexture2D>(FSoftObjectPath(Size64).TryLoad());
	}
	ResourceIcons.Add(Key, Texture);
	return Texture;
}

void AKodHUD::PushErrorLine(const FString& Text)
{
	if (Text.IsEmpty())
	{
		return;
	}
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	if (ErrorLines.Num() > 0 && ErrorLines.Last().Text == Text)
	{
		ErrorLines.Last().Born = Now;
		return;
	}
	FKodErrorLine Line;
	Line.Text = Text;
	Line.Born = Now;
	ErrorLines.Add(Line);
	const int32 MaxRows = FMath::Max(1, HudStyle().ErrorMaxRows);
	while (ErrorLines.Num() > MaxRows)
	{
		ErrorLines.RemoveAt(0);
	}
}

void AKodHUD::DrawResourceFlash()
{
	if (!Canvas)
	{
		return;
	}
	const UKodHudProposed& Style = HudStyle();
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	const float Life = Style.ErrorHoldSeconds + Style.ErrorFadeSeconds;
	for (int32 Index = ErrorLines.Num() - 1; Index >= 0; --Index)
	{
		if (Now - ErrorLines[Index].Born > Life)
		{
			ErrorLines.RemoveAt(Index);
		}
	}
	if (ErrorLines.Num() == 0)
	{
		return;
	}
	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	const FKodScreenRect Stack = MapSpec(Style.ErrorX, Style.ErrorY, Style.ErrorW, Style.ErrorH);
	const int32 Rows = ErrorLines.Num();
	for (int32 Index = 0; Index < Rows; ++Index)
	{
		const int32 AgeRank = Rows - 1 - Index;
		float RankAlpha = 1.f;
		if (AgeRank == 1)
		{
			RankAlpha = Style.ErrorOlderAlpha;
		}
		else if (AgeRank >= 2)
		{
			RankAlpha = Style.ErrorOldestAlpha;
		}
		const float Age = Now - ErrorLines[Index].Born;
		float Fade = 1.f;
		if (Age > Style.ErrorHoldSeconds)
		{
			Fade = 1.f - (Age - Style.ErrorHoldSeconds) / FMath::Max(0.05f, Style.ErrorFadeSeconds);
		}
		const float Alpha = FMath::Clamp(RankAlpha * Fade, 0.f, 1.f);
		const float RowY = Stack.Y + Stack.H - static_cast<float>(Rows - Index) * Style.ErrorRowPx * FrameScale;
		const FLinearColor Ink = Tok(0xE5, 0x38, 0x3B, Alpha);
		DrawText(ErrorLines[Index].Text, FLinearColor(0.f, 0.f, 0.f, 0.65f * Alpha), Stack.X + FrameScale, RowY + 2.f * FrameScale, Font, FrameScale, false);
		DrawText(ErrorLines[Index].Text, Ink, Stack.X, RowY, Font, FrameScale, false);
	}
	if (FlashText.IsEmpty() || Now > FlashUntilSeconds)
	{
		return;
	}
	UFont* CenterFont = GEngine ? GEngine->GetLargeFont() : Font;
	const FKodScreenRect Flash = MapSpec(660.f, 790.f, 600.f, 40.f);
	DrawRect(Tok(0x1F, 0x23, 0x27, 0.9f), Flash.X, Flash.Y, Flash.W, Flash.H);
	DrawText(FlashText, Tok(0xFF, 0xD2, 0x3F), Flash.X + 12.f * FrameScale, Flash.Y + 6.f * FrameScale, CenterFont, FrameScale, false);
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
		const float Now = World->GetTimeSeconds();
		if (const int32* Previous = LastBarHealth.Find(HpState.Id.Value))
		{
			if (ShownHP < *Previous)
			{
				BarDamagedUntil.Add(HpState.Id.Value, Now + 3.f);
			}
		}
		LastBarHealth.Add(HpState.Id.Value, ShownHP);
		const bool bRecentDamage = BarDamagedUntil.FindRef(HpState.Id.Value) > Now;
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
		FVector2D Screen = FVector2D::ZeroVector;
		const UKodHudProposed& Style = HudStyle();
		const bool bStructureBar = FindFrameStructure(HpState.Id.Value) != nullptr;
		int32 Step = KodHudLayout::UnitHpPerSegment;
		int32 Segments = FMath::Max(1, FMath::DivideAndRoundUp(MaxHP, Step));
		if (bStructureBar)
		{
			const int32 Chunks = FMath::Max(1, FMath::DivideAndRoundUp(MaxHP, 500));
			Step = KodHudLayout::UnitHpPerSegment * Chunks;
			Segments = FMath::Max(1, FMath::DivideAndRoundUp(MaxHP, Step));
			if (Segments > KodHudLayout::BuildingSegmentCap)
			{
				Segments = KodHudLayout::BuildingSegmentCap;
				Step = FMath::DivideAndRoundUp(MaxHP, Segments);
			}
		}
		float Zoom = 1.f;
		if (const AKodRTSCameraPawn* CameraPawn = Cast<AKodRTSCameraPawn>(OwningPC->GetPawn()))
		{
			Zoom = FMath::Clamp(6800.f / FMath::Max(1.f, CameraPawn->GetCurrentArmLength()), 0.75f, 1.25f);
		}
		const float Ui = FrameScale * Zoom;
		float BarWidth = KodHudLayout::UnitBarW * Ui;
		float BarHeight = KodHudLayout::UnitBarH * Ui;
		FVector TopWorld = BarActor->GetActorLocation();
		if (const UCapsuleComponent* Capsule = BarActor->FindComponentByClass<UCapsuleComponent>())
		{
			TopWorld.Z += Capsule->GetScaledCapsuleHalfHeight();
		}
		else
		{
			FVector Origin = FVector::ZeroVector;
			FVector Extent = FVector::ZeroVector;
			BarActor->GetActorBounds(false, Origin, Extent);
			TopWorld = Origin + FVector(0.f, 0.f, Extent.Z);
		}
		if (bStructureBar)
		{
			BarHeight = KodHudLayout::BuildingBarH * Ui;
			FVector Origin = FVector::ZeroVector;
			FVector Extent = FVector::ZeroVector;
			BarActor->GetActorBounds(false, Origin, Extent);
			FVector2D LeftScreen;
			FVector2D RightScreen;
			const bool bLeft = OwningPC->ProjectWorldLocationToScreen(Origin - FVector(Extent.X, 0.f, 0.f), LeftScreen, true);
			const bool bRight = OwningPC->ProjectWorldLocationToScreen(Origin + FVector(Extent.X, 0.f, 0.f), RightScreen, true);
			if (bLeft && bRight)
			{
				BarWidth = FMath::Max(24.f * Ui, FMath::Abs(RightScreen.X - LeftScreen.X) * 0.8f);
			}
			TopWorld = Origin + FVector(0.f, 0.f, Extent.Z);
		}
		if (!OwningPC->ProjectWorldLocationToScreen(TopWorld, Screen, true))
		{
			continue;
		}
		const bool bHover = bHaveMouse && FMath::Abs(MouseX - Screen.X) <= BarWidth * 0.5f + 8.f && FMath::Abs(MouseY - Screen.Y) <= 28.f;
		if (HealthBarMode != 2 && bFull && !bSelected && !bHover && !bShowAll && !bRecentDamage)
		{
			continue;
		}

		const FLinearColor Fill = KodWireTint::FromHitPointsPermille(ShownHP, MaxHP, Style.WorldHealthyPermille, Style.WorldWarningPermille, true);
		const float Left = Screen.X - (BarWidth * 0.5f);
		const float Top = Screen.Y - KodHudLayout::BarGapAbove * Ui - BarHeight;
		const float FramePx = Ui;
		const float GapPx = Ui;
		const FLinearColor Empty = FLinearColor(FColor(0x1A, 0x1D, 0x1F));
		const FLinearColor FrameInk(0.f, 0.f, 0.f, 1.f);
		auto PaintSegments = [&](float RowTop, float RowHeight, int32 RowValue, int32 RowMax, const FLinearColor& RowFill)
		{
			DrawRect(FrameInk, Left, RowTop, BarWidth, RowHeight);
			const float InnerW = FMath::Max(1.f, BarWidth - 2.f * FramePx);
			const float InnerH = FMath::Max(1.f, RowHeight - 2.f * FramePx);
			const float SegW = (InnerW - GapPx * static_cast<float>(Segments - 1)) / static_cast<float>(Segments);
			int32 FullSegments = 0;
			float Partial = 0.f;
			if (RowMax > 0 && RowValue >= RowMax)
			{
				FullSegments = Segments;
			}
			else if (RowMax > 0)
			{
				const float Exact = static_cast<float>(FMath::Max(0, RowValue)) / static_cast<float>(RowMax) * static_cast<float>(Segments);
				FullSegments = FMath::Clamp(FMath::FloorToInt(Exact), 0, Segments);
				Partial = Exact - static_cast<float>(FullSegments);
			}
			for (int32 Segment = 0; Segment < Segments; ++Segment)
			{
				const float SegX = Left + FramePx + static_cast<float>(Segment) * (SegW + GapPx);
				DrawRect(Empty, SegX, RowTop + FramePx, FMath::Max(1.f, SegW), InnerH);
				float FillW = 0.f;
				if (Segment < FullSegments)
				{
					FillW = SegW;
				}
				else if (Segment == FullSegments && Partial > 0.f)
				{
					FillW = SegW * Partial;
				}
				if (FillW > 0.f)
				{
					DrawRect(RowFill, SegX, RowTop + FramePx, FillW, InnerH);
				}
			}
		};
		if (HpState.ShieldMax > 0)
		{
			const float ShieldTop = Top - Style.ShieldGapPx * Ui - BarHeight;
			PaintSegments(ShieldTop, BarHeight, FMath::Max(0, HpState.Shield), FMath::Max(1, HpState.ShieldMax), Style.ShieldColor);
		}
		PaintSegments(Top, BarHeight, ShownHP, MaxHP, Fill);
		if (HpState.EnergyMax > 0)
		{
			const float EnergyFrac = FMath::Clamp(static_cast<float>(HpState.Energy) / static_cast<float>(HpState.EnergyMax), 0.f, 1.f);
			const float EnergyTop = Top + BarHeight + Ui;
			const float EnergyH = 3.f * Ui;
			DrawRect(FrameInk, Left, EnergyTop, BarWidth, EnergyH);
			const FLinearColor EnergyInk = HudSkin == 1 ? FLinearColor(FColor(0x3D, 0xFF, 0xB0)) : FLinearColor(FColor(0x2F, 0x7B, 0xFF));
			DrawRect(EnergyInk, Left + FramePx, EnergyTop + FramePx * 0.5f, (BarWidth - 2.f * FramePx) * EnergyFrac, FMath::Max(1.f, EnergyH - FramePx));
		}
	}
}

void AKodHUD::DrawConstructionBars()
{
	if (!Canvas)
	{
		return;
	}
	APlayerController* PC = GetOwningPlayerController();
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!PC || !Sim)
	{
		return;
	}
	const UKodHudProposed& Style = HudStyle();
	UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;
	for (const FKodStructureState& Structure : FrameStructures)
	{
		AActor* Actor = Sim->ResolveEntity(Structure.Id);
		if (Structure.bComplete)
		{
			if (Actor && DimmedStructureIds.Contains(Structure.Id.Value))
			{
				TInlineComponentArray<UMeshComponent*> Meshes;
				Actor->GetComponents(Meshes);
				for (UMeshComponent* Mesh : Meshes)
				{
					if (Mesh)
					{
						Mesh->SetScalarParameterValueOnMaterials(TEXT("Opacity"), 1.f);
					}
				}
				DimmedStructureIds.Remove(Structure.Id.Value);
			}
			continue;
		}
		if (Actor)
		{
			TInlineComponentArray<UMeshComponent*> Meshes;
			Actor->GetComponents(Meshes);
			for (UMeshComponent* Mesh : Meshes)
			{
				if (Mesh)
				{
					Mesh->SetScalarParameterValueOnMaterials(TEXT("Opacity"), Style.IncompleteOpacity);
				}
			}
			DimmedStructureIds.Add(Structure.Id.Value);
		}
		if (Structure.BuildTimeTotal <= 0)
		{
			continue;
		}
		const float SpanX = static_cast<float>(FMath::Max(1, Structure.FootprintX)) * static_cast<float>(KodBuildGrid::CellUU);
		const float SpanY = static_cast<float>(FMath::Max(1, Structure.FootprintY)) * static_cast<float>(KodBuildGrid::CellUU);
		const FVector Center = Structure.Position;
		const FVector Corners[4] = {
			Center + FVector(-SpanX * 0.5f, -SpanY * 0.5f, 8.f),
			Center + FVector(SpanX * 0.5f, -SpanY * 0.5f, 8.f),
			Center + FVector(SpanX * 0.5f, SpanY * 0.5f, 8.f),
			Center + FVector(-SpanX * 0.5f, SpanY * 0.5f, 8.f)
		};
		float MinX = 0.f;
		float MaxX = 0.f;
		float MaxY = 0.f;
		bool bAny = false;
		for (const FVector& Corner : Corners)
		{
			FVector2D Screen;
			if (!PC->ProjectWorldLocationToScreen(Corner, Screen, true))
			{
				continue;
			}
			if (!bAny)
			{
				MinX = MaxX = Screen.X;
				MaxY = Screen.Y;
				bAny = true;
			}
			else
			{
				MinX = FMath::Min(MinX, Screen.X);
				MaxX = FMath::Max(MaxX, Screen.X);
				MaxY = FMath::Max(MaxY, Screen.Y);
			}
		}
		if (!bAny)
		{
			continue;
		}
		const float Alpha = FMath::Clamp(static_cast<float>(Structure.BuildTicksDone) / static_cast<float>(Structure.BuildTimeTotal), 0.f, 1.f);
		const float BarW = FMath::Max(24.f * FrameScale, (MaxX - MinX) * 0.8f);
		const float BarH = 6.f * FrameScale;
		const float BarX = (MinX + MaxX) * 0.5f - BarW * 0.5f;
		const float BarY = MaxY + 4.f * FrameScale;
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 1.f), BarX, BarY, BarW, BarH);
		DrawRect(Style.ConstructionColor, BarX + FrameScale, BarY + FrameScale, FMath::Max(1.f, (BarW - 2.f * FrameScale) * Alpha), FMath::Max(1.f, BarH - 2.f * FrameScale));
		const FString Label = FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Alpha * 100.f));
		DrawText(Label, Style.ConstructionLabelColor, BarX, BarY + BarH + 16.f * FrameScale, Font, FrameScale, false);
	}
}

void AKodHUD::DrawPlacementGrid()
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
	FName PlaceType;
	FVector PlaceCenter = FVector::ZeroVector;
	if (!PC->TryGetPlacementAim(PlaceType, PlaceCenter))
	{
		return;
	}
	FKodBuildSpec Spec;
	if (!Sim->TryGetBuildSpec(PlaceType, Spec))
	{
		return;
	}
	const UKodHudProposed& Style = HudStyle();
	const FKodPlacementResult Placement = Sim->QueryPlacement(ReadLocalTeamId(), PlaceType, PlaceCenter);
	const bool bNeedsSupply = Spec.SupplyProvided <= 0 && !Spec.bTownHall;
	if (bNeedsSupply)
	{
		const FLinearColor Ring = HudSkin == 1 ? Style.ToxicBlotchColor : Style.SupplyRingColor;
		for (const FKodStructureState& Structure : FrameStructures)
		{
			if (!Structure.bComplete || Structure.TeamId != ReadLocalTeamId() || Structure.SupplyProvided <= 0)
			{
				continue;
			}
			constexpr int32 Steps = 24;
			FVector2D Previous = FVector2D::ZeroVector;
			bool bPrevious = false;
			for (int32 StepIndex = 0; StepIndex <= Steps; ++StepIndex)
			{
				const float Angle = (static_cast<float>(StepIndex) / static_cast<float>(Steps)) * 2.f * PI;
				const FVector Point = Structure.Position + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Style.SupplyLatticeRadiusUU;
				FVector2D Screen;
				if (!PC->ProjectWorldLocationToScreen(Point, Screen, true))
				{
					bPrevious = false;
					continue;
				}
				if (bPrevious)
				{
					DrawLine(Previous.X, Previous.Y, Screen.X, Screen.Y, Ring, 2.f * FrameScale);
				}
				Previous = Screen;
				bPrevious = true;
			}
		}
	}
	const int32 SizeX = FMath::Max(1, Spec.FootprintX);
	const int32 SizeY = FMath::Max(1, Spec.FootprintY);
	const FLinearColor ValidFill(FColor(0x3D, 0xFF, 0x6A, static_cast<uint8>(0.27f * 255.f)));
	const FLinearColor InvalidFill(FColor(0xFF, 0x3D, 0x3D, static_cast<uint8>(0.27f * 255.f)));
	const FLinearColor ValidLine(FColor(0x3D, 0xFF, 0x6A, static_cast<uint8>(0.78f * 255.f)));
	const FLinearColor InvalidLine(FColor(0xFF, 0x3D, 0x3D, static_cast<uint8>(0.78f * 255.f)));
	const FLinearColor Slash(FColor(0x28, 0x00, 0x00, static_cast<uint8>(0.92f * 255.f)));
	for (int32 Y = 0; Y < SizeY; ++Y)
	{
		for (int32 X = 0; X < SizeX; ++X)
		{
			const int32 CellIndex = Y * SizeX + X;
			const bool bOpen = !Placement.CellOk.IsValidIndex(CellIndex) || Placement.CellOk[CellIndex] != 0;
			const int32 CellX = Placement.CellX + X;
			const int32 CellY = Placement.CellY + Y;
			const float X0 = static_cast<float>(CellX) * static_cast<float>(KodBuildGrid::CellUU);
			const float Y0 = static_cast<float>(CellY) * static_cast<float>(KodBuildGrid::CellUU);
			const float X1 = X0 + static_cast<float>(KodBuildGrid::CellUU);
			const float Y1 = Y0 + static_cast<float>(KodBuildGrid::CellUU);
			const FVector Corners[4] = {
				FVector(X0, Y0, PlaceCenter.Z + 4.f),
				FVector(X1, Y0, PlaceCenter.Z + 4.f),
				FVector(X1, Y1, PlaceCenter.Z + 4.f),
				FVector(X0, Y1, PlaceCenter.Z + 4.f)
			};
			FVector2D Screen[4];
			bool bAll = true;
			for (int32 Corner = 0; Corner < 4; ++Corner)
			{
				if (!PC->ProjectWorldLocationToScreen(Corners[Corner], Screen[Corner], true))
				{
					bAll = false;
					break;
				}
			}
			if (!bAll)
			{
				continue;
			}
			float MinX = Screen[0].X;
			float MaxX = Screen[0].X;
			float MinY = Screen[0].Y;
			float MaxY = Screen[0].Y;
			for (int32 Corner = 1; Corner < 4; ++Corner)
			{
				MinX = FMath::Min(MinX, Screen[Corner].X);
				MaxX = FMath::Max(MaxX, Screen[Corner].X);
				MinY = FMath::Min(MinY, Screen[Corner].Y);
				MaxY = FMath::Max(MaxY, Screen[Corner].Y);
			}
			const float W = FMath::Max(1.f, MaxX - MinX);
			const float H = FMath::Max(1.f, MaxY - MinY);
			DrawRect(bOpen ? ValidFill : InvalidFill, MinX, MinY, W, H);
			const FLinearColor Outline = bOpen ? ValidLine : InvalidLine;
			DrawLine(MinX, MinY, MaxX, MinY, Outline, 2.f * FrameScale);
			DrawLine(MaxX, MinY, MaxX, MaxY, Outline, 2.f * FrameScale);
			DrawLine(MaxX, MaxY, MinX, MaxY, Outline, 2.f * FrameScale);
			DrawLine(MinX, MaxY, MinX, MinY, Outline, 2.f * FrameScale);
			if (!bOpen)
			{
				const float Pitch = 10.f * FrameScale;
				for (float Offset = -H; Offset < W; Offset += Pitch)
				{
					float Ax = MinX;
					float Ay = MinY + Offset;
					float Bx = MaxX;
					float By = MinY + Offset + W;
					if (Ay < MinY)
					{
						Ax += MinY - Ay;
						Ay = MinY;
					}
					if (By > MaxY)
					{
						Bx -= By - MaxY;
						By = MaxY;
					}
					if (Ax < Bx && Ay <= MaxY && By >= MinY)
					{
						DrawLine(Ax, Ay, Bx, By, Slash, 3.f * FrameScale);
					}
				}
			}
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
			const EKodCheatSubmit CheatSubmit = Sim->SubmitCheatPhrase(Line);
			if (CheatSubmit == EKodCheatSubmit::Applied)
			{
				return;
			}
			if (CheatSubmit == EKodCheatSubmit::Refused)
			{
				const FString Refusal = Sim->GetCheatFeedbackLine();
				AddChatMessage(Refusal);
				ShowAlertLine(Refusal);
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
		UGameplayStatics::OpenLevel(World, FName(TEXT("/Game/Maps/Frontend/L_MainMenu")), true, FString());
	}
	else if (Choice == 6)
	{
		MenuPage = 7;
	}
	else if (Choice == 7)
	{
		MenuPage = 6;
		ScoreTab = 0;
		ScoreScrub = -1;
	}
	else if (Choice == 70 && World)
	{
		FString Options;
		if (AKodGameMode* Mode = World->GetAuthGameMode<AKodGameMode>())
		{
			Options = Mode->GetMatchOptions();
		}
		if (!Options.IsEmpty() && !Options.StartsWith(TEXT("?")) && !Options.Contains(TEXT("/")))
		{
			Options = FString(TEXT("?")) + Options;
		}
		UGameplayStatics::OpenLevel(World, FName(*UGameplayStatics::GetCurrentLevelName(World, true)), true, Options);
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
	else if (Choice >= 20 && Choice <= 25)
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

void AKodHUD::ScrubScoreGraph(float MouseX)
{
	const float Span = FMath::Max(1.f, ScoreGraphMaxX - ScoreGraphMinX);
	const float Alpha = FMath::Clamp((MouseX - ScoreGraphMinX) / Span, 0.f, 1.f);
	const int32 Last = FMath::Max(0, ScoreSampleCount - 1);
	ScoreScrub = FMath::Clamp(FMath::RoundToInt(Alpha * static_cast<float>(Last)), 0, Last);
}

void AKodHUD::DrawPostGame(const UKodSimSubsystem* Sim)
{
	if (!Canvas)
	{
		return;
	}
	const float Scale = FrameScale;
	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	const FLinearColor Paper = Tok(0xE6, 0xE6, 0xE0);
	const FLinearColor Dim = Tok(0x8E, 0x94, 0x9A);
	const bool bVictory = Sim && Sim->GetMatchOutcome() == EKodMatchOutcome::Victory;
	const FKodScreenRect Banner = MapSpec(40.f, 16.f, 1840.f, 100.f);
	DrawPlate(Banner.X, Banner.Y, Banner.W, Banner.H, Tok(0x1F, 0x23, 0x27, 0.96f), TEXT("T_UI_VG_Modal_Panel"), 32.f, 32.f, 32.f, 32.f);
	DrawText(bVictory ? TEXT("VICTORY") : TEXT("DEFEAT"), bVictory ? Tok(0xD9, 0xB8, 0x78) : Tok(0xE5, 0x38, 0x3B), Banner.X + 32.f * Scale, Banner.Y + 28.f * Scale, Font, 1.8f * Scale, false);

	float CheatY = Banner.Y + Banner.H + 8.f * Scale;
	if (Sim && Sim->IsMatchCheated())
	{
		CheatY = DrawCheatSummary(Sim, Banner.X + 32.f * Scale, CheatY, Banner.W - 64.f * Scale, Scale, Font);
	}

	const TCHAR* Tabs[] = { TEXT("Summary"), TEXT("Units"), TEXT("Structures"), TEXT("Production"), TEXT("Resources"), TEXT("Army") };
	const FKodScreenRect TabProbe = MapSpec(40.f, 152.f, 300.f, 44.f);
	const float TabY = FMath::Max(TabProbe.Y, CheatY + 8.f * Scale);
	for (int32 TabIndex = 0; TabIndex < 6; ++TabIndex)
	{
		const FKodScreenRect TabRect = MapSpec(40.f + 310.f * static_cast<float>(TabIndex), 152.f, 300.f, 44.f);
		const bool bOn = ScoreTab == TabIndex;
		DrawPlate(TabRect.X, TabY, TabRect.W, TabProbe.H, bOn ? Tok(0xB0, 0x8D, 0x57) : Tok(0x5C, 0x64, 0x6C), TEXT("T_UI_VG_Tab_Group"), 8.f, 6.f, 8.f, 6.f);
		DrawText(Tabs[TabIndex], Paper, TabRect.X + 12.f * Scale, TabY + 10.f * Scale, Font, 0.9f * Scale, false);
		RememberChrome(FName(TEXT("Menu")), 20 + TabIndex, FName(), TabRect.X, TabY, TabRect.X + TabRect.W, TabY + TabProbe.H);
	}

	const FKodScreenRect ContentProbe = MapSpec(40.f, 220.f, 1840.f, 700.f);
	const FKodScreenRect ButtonProbe = MapSpec(0.f, 980.f, 1920.f, 56.f);
	FKodScreenRect Content;
	Content.X = ContentProbe.X;
	Content.Y = TabY + TabProbe.H + 8.f * Scale;
	Content.W = ContentProbe.W;
	Content.H = FMath::Max(160.f * Scale, ButtonProbe.Y - 12.f * Scale - Content.Y);
	DrawPlate(Content.X, Content.Y, Content.W, Content.H, Tok(0x1F, 0x23, 0x27, 0.9f), TEXT("T_UI_VG_Modal_Panel"), 32.f, 32.f, 32.f, 32.f);
	FKodMatchScoreTracker Board;
	if (Sim)
	{
		Sim->CopyScoreboard(Board);
	}
	const int32 Seconds = Sim ? static_cast<int32>(Sim->GetSimTickIndex() / 16) : 0;
	float CursorY = Content.Y + 16.f * Scale;
	const float Left = Content.X + 16.f * Scale;
	DrawText(FString::Printf(TEXT("Time %02d:%02d"), Seconds / 60, Seconds % 60), Paper, Left, CursorY, Font, Scale, false);
	CursorY += 28.f * Scale;

	auto ValueKind = [](const FKodScoreSample& Sample, int32 Kind) -> int32
	{
		switch (Kind)
		{
		case 0: return Sample.Workers;
		case 1: return Sample.JadeiteCollected;
		case 2: return Sample.LumineneCollected;
		case 3: return Sample.ArmyValue;
		case 4: return Sample.SupplyUsed;
		default: return 0;
		}
	};
	int32 SampleCount = 0;
	for (const FKodScorePlayer& Player : Board.Players)
	{
		SampleCount = FMath::Max(SampleCount, Player.Samples.Num());
	}
	ScoreSampleCount = SampleCount;
	const int32 ScrubIndex = SampleCount <= 0 ? INDEX_NONE : (ScoreScrub < 0 ? SampleCount - 1 : FMath::Clamp(ScoreScrub, 0, SampleCount - 1));

	auto RateAt = [&](const FKodScorePlayer& Player, int32 Index, bool bJadeite) -> int32
	{
		if (!Player.Samples.IsValidIndex(Index))
		{
			return 0;
		}
		const FKodScoreSample& Newer = Player.Samples[Index];
		int32 OlderAmount = 0;
		int32 OlderTick = 0;
		if (Index > 0)
		{
			const FKodScoreSample& Older = Player.Samples[Index - 1];
			OlderAmount = bJadeite ? Older.JadeiteCollected : Older.LumineneCollected;
			OlderTick = Older.Tick;
		}
		const int32 NewerAmount = bJadeite ? Newer.JadeiteCollected : Newer.LumineneCollected;
		return FKodMatchScoreTracker::PerMinute(NewerAmount - OlderAmount, Newer.Tick - OlderTick);
	};

	const float ColW = (Content.W - 48.f * Scale) * 0.5f;
	const float HeaderY = CursorY;
	for (int32 PlayerIndex = 0; PlayerIndex < Board.Players.Num() && PlayerIndex < 2; ++PlayerIndex)
	{
		const FKodScorePlayer& Player = Board.Players[PlayerIndex];
		const float ColX = Left + static_cast<float>(PlayerIndex) * (ColW + 16.f * Scale);
		const FLinearColor Swatch = FKodSkirmishSetup::TeamSwatch(Player.ColorIndex);
		DrawRect(Swatch, ColX, HeaderY, ColW, 28.f * Scale);
		DrawText(PlayerIndex == 0 ? TEXT("You") : TEXT("AI"), Paper, ColX + 8.f * Scale, HeaderY + 4.f * Scale, Font, 0.9f * Scale, false);
	}
	CursorY += 36.f * Scale;

	auto ColumnLine = [&](int32 PlayerIndex, float& LineY, const FString& Text)
	{
		if (!Board.Players.IsValidIndex(PlayerIndex))
		{
			return;
		}
		const float ColX = Left + static_cast<float>(PlayerIndex) * (ColW + 16.f * Scale);
		DrawText(Text, Paper, ColX, LineY, Font, 0.85f * Scale, false);
	};

	if (ScoreTab == 0)
	{
		const int32 SimTick = Sim ? static_cast<int32>(FMath::Min<int64>(Sim->GetSimTickIndex(), MAX_int32)) : 0;
		float LineY = CursorY;
		for (int32 PlayerIndex = 0; PlayerIndex < Board.Players.Num() && PlayerIndex < 2; ++PlayerIndex)
		{
			const FKodScorePlayer& Player = Board.Players[PlayerIndex];
			float RowY = LineY;
			ColumnLine(PlayerIndex, RowY, FString::Printf(TEXT("APM %d   peak %d"), Board.AverageApm(Player.TeamId, SimTick), Board.PeakApm(Player.TeamId)));
			RowY += 26.f * Scale;
			ColumnLine(PlayerIndex, RowY, FString::Printf(TEXT("Units built %d   lost %d"), Player.UnitsTrained, Player.UnitsLost));
			RowY += 26.f * Scale;
			ColumnLine(PlayerIndex, RowY, FString::Printf(TEXT("Structures built %d   lost %d"), Player.StructuresBuilt, Player.StructuresLost));
			RowY += 26.f * Scale;
			ColumnLine(PlayerIndex, RowY, FString::Printf(TEXT("Gathered  Credits %d   Luminene %d"), Player.JadeiteCollected, Player.LumineneCollected));
		}
	}
	else if (ScoreTab == 1 || ScoreTab == 2)
	{
		const bool bUnits = ScoreTab == 1;
		TArray<FName> TypeIds;
		for (const FKodScorePlayer& Player : Board.Players)
		{
			const TArray<FKodScoreTypeRow>& Rows = bUnits ? Player.Units : Player.Structures;
			for (const FKodScoreTypeRow& Row : Rows)
			{
				TypeIds.AddUnique(Row.Id);
			}
		}
		TypeIds.Sort([](const FName& A, const FName& B) { return A.ToString() < B.ToString(); });
		DrawText(bUnits ? TEXT("Type") : TEXT("Structure"), Dim, Left, CursorY, Font, 0.8f * Scale, false);
		DrawText(bUnits ? TEXT("Trained / Killed / Lost") : TEXT("Built / Razed / Lost"), Dim, Left + 280.f * Scale, CursorY, Font, 0.8f * Scale, false);
		DrawText(bUnits ? TEXT("Trained / Killed / Lost") : TEXT("Built / Razed / Lost"), Dim, Left + 280.f * Scale + ColW, CursorY, Font, 0.8f * Scale, false);
		CursorY += 24.f * Scale;
		if (TypeIds.Num() == 0)
		{
			DrawText(TEXT("None"), Dim, Left, CursorY, Font, 0.85f * Scale, false);
		}
		const int32 Shown = FMath::Min(TypeIds.Num(), 16);
		for (int32 RowIndex = 0; RowIndex < Shown; ++RowIndex)
		{
			const FName TypeId = TypeIds[RowIndex];
			const FString Label = PlayerFacingName(Sim, TypeId).ToString();
			DrawText(Label, Paper, Left, CursorY, Font, 0.85f * Scale, false);
			for (int32 PlayerIndex = 0; PlayerIndex < Board.Players.Num() && PlayerIndex < 2; ++PlayerIndex)
			{
				const TArray<FKodScoreTypeRow>& Rows = bUnits ? Board.Players[PlayerIndex].Units : Board.Players[PlayerIndex].Structures;
				int32 Trained = 0;
				int32 Killed = 0;
				int32 Lost = 0;
				for (const FKodScoreTypeRow& Row : Rows)
				{
					if (Row.Id == TypeId)
					{
						Trained = Row.Trained;
						Killed = Row.Killed;
						Lost = Row.Lost;
						break;
					}
				}
				const float ColX = Left + 280.f * Scale + static_cast<float>(PlayerIndex) * ColW;
				DrawText(FString::Printf(TEXT("%d / %d / %d"), Trained, Killed, Lost), Paper, ColX, CursorY, Font, 0.85f * Scale, false);
			}
			CursorY += 24.f * Scale;
		}
	}
	else
	{
		float LineY = CursorY;
		for (int32 PlayerIndex = 0; PlayerIndex < Board.Players.Num() && PlayerIndex < 2; ++PlayerIndex)
		{
			const FKodScorePlayer& Player = Board.Players[PlayerIndex];
			const FKodScoreSample* Sample = (ScrubIndex != INDEX_NONE && Player.Samples.IsValidIndex(ScrubIndex)) ? &Player.Samples[ScrubIndex] : nullptr;
			float RowY = LineY;
			if (ScoreTab == 3)
			{
				ColumnLine(PlayerIndex, RowY, FString::Printf(TEXT("Workers %d"), Sample ? Sample->Workers : 0));
			}
			else if (ScoreTab == 4)
			{
				ColumnLine(PlayerIndex, RowY, FString::Printf(TEXT("Credits gathered %d   spent %d"), Player.JadeiteCollected, Player.JadeiteSpent));
				RowY += 26.f * Scale;
				ColumnLine(PlayerIndex, RowY, FString::Printf(TEXT("Luminene gathered %d   spent %d"), Player.LumineneCollected, Player.LumineneSpent));
				RowY += 26.f * Scale;
				ColumnLine(PlayerIndex, RowY, FString::Printf(TEXT("Rate  Credits %d/min   Luminene %d/min"), RateAt(Player, ScrubIndex, true), RateAt(Player, ScrubIndex, false)));
			}
			else
			{
				ColumnLine(PlayerIndex, RowY, FString::Printf(TEXT("Army %d"), Sample ? Sample->ArmyValue : 0));
				RowY += 26.f * Scale;
				ColumnLine(PlayerIndex, RowY, FString::Printf(TEXT("Supply %d/%d"), Sample ? Sample->SupplyUsed : 0, Sample ? Sample->SupplyCap : 0));
			}
		}
		CursorY += ScoreTab == 3 ? 40.f * Scale : 90.f * Scale;
		auto DrawKind = [&](int32 Kind, const FString& Title)
		{
			DrawText(Title, Dim, Left, CursorY, Font, 0.8f * Scale, false);
			CursorY += 22.f * Scale;
			const float GraphH = 140.f * Scale;
			const float GraphW = Content.W - 32.f * Scale;
			float MaxValue = 1.f;
			for (const FKodScorePlayer& Player : Board.Players)
			{
				for (const FKodScoreSample& Sample : Player.Samples)
				{
					MaxValue = FMath::Max(MaxValue, static_cast<float>(ValueKind(Sample, Kind)));
				}
			}
			for (const FKodScorePlayer& Player : Board.Players)
			{
				const FLinearColor Swatch = FKodSkirmishSetup::TeamSwatch(Player.ColorIndex);
				FVector2D Prev(0.f, 0.f);
				bool bHavePrev = false;
				const int32 Count = Player.Samples.Num();
				for (int32 SampleIndex = 0; SampleIndex < Count; ++SampleIndex)
				{
					const float Amount = static_cast<float>(ValueKind(Player.Samples[SampleIndex], Kind));
					const float Px = Left + GraphW * (Count <= 1 ? 0.f : static_cast<float>(SampleIndex) / static_cast<float>(Count - 1));
					const float Py = CursorY + GraphH - GraphH * (Amount / MaxValue);
					if (bHavePrev)
					{
						DrawLine(Prev.X, Prev.Y, Px, Py, Swatch, 2.f);
					}
					Prev = FVector2D(Px, Py);
					bHavePrev = true;
				}
			}
			if (ScrubIndex != INDEX_NONE && SampleCount > 0)
			{
				const float Marker = Left + GraphW * (SampleCount <= 1 ? 0.f : static_cast<float>(ScrubIndex) / static_cast<float>(SampleCount - 1));
				DrawLine(Marker, CursorY, Marker, CursorY + GraphH, Paper, 1.f);
			}
			ScoreGraphMinX = Left;
			ScoreGraphMaxX = Left + GraphW;
			CursorY += GraphH + 12.f * Scale;
		};
		if (ScoreTab == 3)
		{
			DrawKind(0, TEXT("Workers"));
		}
		else if (ScoreTab == 4)
		{
			DrawKind(1, TEXT("Jadeite Credits"));
			DrawKind(2, TEXT("Luminene"));
		}
		else
		{
			DrawKind(3, TEXT("Army value"));
			DrawKind(4, TEXT("Supply"));
		}
		const float BarW = ScoreGraphMaxX - ScoreGraphMinX;
		DrawRect(Tok(0x5C, 0x64, 0x6C), ScoreGraphMinX, CursorY, BarW, 16.f * Scale);
		if (ScrubIndex != INDEX_NONE && SampleCount > 0)
		{
			const float Marker = ScoreGraphMinX + BarW * (SampleCount <= 1 ? 0.f : static_cast<float>(ScrubIndex) / static_cast<float>(SampleCount - 1));
			DrawRect(Paper, Marker - 2.f, CursorY, 4.f, 16.f * Scale);
		}
		RememberChrome(FName(TEXT("Scrub")), 0, FName(), ScoreGraphMinX, CursorY, ScoreGraphMaxX, CursorY + 16.f * Scale);
	}

	const float BtnW = 280.f * Scale;
	const float BtnH = 56.f * Scale;
	const float Gap = 16.f * Scale;
	const FKodScreenRect ButtonRow = MapSpec(0.f, 980.f, 1920.f, 56.f);
	float ButtonX = ButtonRow.X + (ButtonRow.W - (3.f * BtnW + 2.f * Gap)) * 0.5f;
	auto MenuButton = [&](const FString& Label, int32 Choice, bool bEnabled)
	{
		DrawPlate(ButtonX, ButtonRow.Y, BtnW, BtnH, bEnabled ? Tok(0x5C, 0x64, 0x6C) : Tok(0x3A, 0x3F, 0x45), TEXT("T_UI_VG_Btn_Chip"), 8.f, 8.f, 8.f, 8.f);
		DrawText(Label, bEnabled ? Paper : Dim, ButtonX + 12.f * Scale, ButtonRow.Y + 16.f * Scale, Font, 0.9f * Scale, false);
		if (bEnabled)
		{
			RememberChrome(FName(TEXT("Menu")), Choice, FName(), ButtonX, ButtonRow.Y, ButtonX + BtnW, ButtonRow.Y + BtnH);
		}
		ButtonX += BtnW + Gap;
	};
	MenuButton(TEXT("Main Menu"), 5, true);
	MenuButton(TEXT("Rematch"), 70, true);
	MenuButton(TEXT("Watch Replay"), 71, false);
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
	const bool bResults = MenuPage == 5 || MenuPage == 6;
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
		DrawPostGame(Sim);
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
	else if (MenuPage == 5)
	{
		DrawPostGame(Sim);
	}
}

void AKodHUD::BeginPlay()
{
	Super::BeginPlay();
	bShowHUD = true;
	FKodUiArt::LoadManifest();
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
