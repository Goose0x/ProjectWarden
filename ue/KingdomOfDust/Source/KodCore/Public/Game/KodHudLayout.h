#pragma once

#include "CoreMinimal.h"

/**
 * Vanguard HUD spec, in 1920×1080 pixels, before AKodHUD::MapSpec scales them.
 * Adjust the v5 rects here. Draw code should not hardcode these numbers.
 * Single-unit silhouette is 160×160 inside the existing info well.
 * Each multi-select cell keeps its hit rect and draws a 48×48 silhouette inside it.
 * The 160 and 48 slots draw the unit icon through the HP tint.
 * Command icons are the same 128 art, scaled to the cell.
 * An optional zone mask can split the tint. Overall HP is the fallback.
 */
namespace KodHudLayout
{
	constexpr float SingleWellX = 368.f;
	constexpr float SingleWellY = 820.f;
	constexpr float SingleWellW = 200.f;
	constexpr float SingleWellH = 222.f;
	constexpr float SingleSilhouette = 160.f;
	constexpr float SingleSilhouetteX = SingleWellX + (SingleWellW - SingleSilhouette) * 0.5f;
	constexpr float SingleSilhouetteY = SingleWellY + (SingleWellH - SingleSilhouette) * 0.5f;
	constexpr float SingleInfoX = 584.f;
	constexpr float SingleInfoY = 820.f;
	constexpr float SingleInfoW = 466.f;
	constexpr float SingleInfoH = 112.f;

	constexpr float GroupOriginX = 525.f;
	constexpr float GroupOriginY = 872.f;
	constexpr float GroupStepX = 90.f;
	constexpr float GroupStepY = 68.f;
	constexpr float GroupCellW = 84.f;
	constexpr float GroupCellH = 62.f;
	constexpr float GroupSilhouette = 48.f;
	constexpr float GroupSilhouetteInsetX = (GroupCellW - GroupSilhouette) * 0.5f;
	constexpr float GroupSilhouetteInsetY = (GroupCellH - GroupSilhouette) * 0.5f;
	constexpr float GroupGridX = 525.f;
	constexpr float GroupGridY = 872.f;
	constexpr float GroupGridW = 714.f;
	constexpr float GroupGridH = 198.f;
	constexpr float GroupRailX = 1245.f;
	constexpr float GroupRailY = 872.f;
	constexpr float GroupRailW = 32.f;
	constexpr float GroupRailH = 198.f;
	constexpr float GroupPageX = 332.f;
	constexpr float GroupPageY = 820.f;
	constexpr float GroupPageStepY = 37.f;
	constexpr float GroupPageW = 28.f;
	constexpr float GroupPageH = 32.f;

	constexpr float QueueX[5] = { 584.f, 668.f, 740.f, 812.f, 884.f };
	constexpr float QueueY[5] = { 948.f, 956.f, 956.f, 956.f, 956.f };
	constexpr float QueueW[5] = { 72.f, 64.f, 64.f, 64.f, 64.f };
	constexpr float QueueH[5] = { 72.f, 64.f, 64.f, 64.f, 64.f };
	constexpr float QueueProgX = 584.f;
	constexpr float QueueProgY = 1014.f;
	constexpr float QueueProgW = 72.f;
	constexpr float QueueProgH = 6.f;

	constexpr float CommandCardX = 1507.f;
	constexpr float CommandCardY = 826.f;
	constexpr float CommandCardW = 404.f;
	constexpr float CommandCardH = 240.f;
	constexpr float CommandButton = 76.f;
	constexpr float CommandGap = 6.f;
	constexpr int32 CommandCols = 5;
	constexpr int32 CommandRows = 3;

	/** Authored icon and wireframe textures. Drawn scaled to the slot. */
	constexpr int32 IconSourceTexels = 128;

	/** Addendum H/I. Boxless readout. No chip under the values. */
	constexpr float ResourceOriginX[4] = { 1384.f, 1520.f, 1656.f, 1792.f };
	constexpr float ResourceIcon = 28.f;
	constexpr float ResourceValueGap = 33.f;

	/** Addendum A. Painted frame layers. Missing textures keep the flat panels. */
	constexpr float FrameMinimapX = 0.f;
	constexpr float FrameMinimapY = 750.f;
	constexpr float FrameMinimapW = 517.f;
	constexpr float FrameMinimapH = 330.f;
	constexpr float FrameCentreX = 512.f;
	constexpr float FrameCentreY = 824.f;
	constexpr float FrameCentreW = 778.f;
	constexpr float FrameCentreH = 256.f;
	constexpr float FramePortraitX = 1290.f;
	constexpr float FramePortraitY = 856.f;
	constexpr float FramePortraitW = 190.f;
	constexpr float FramePortraitH = 224.f;
	constexpr float FrameCardX = 1474.f;
	constexpr float FrameCardY = 770.f;
	constexpr float FrameCardW = 446.f;
	constexpr float FrameCardH = 310.f;

	/** Addendum G. Command-card wing. Help is drawn, and its click stays locked. */
	constexpr float WingButtonY = 788.f;
	constexpr float WingButtonW = 56.f;
	constexpr float WingButtonH = 28.f;
	constexpr float WingHelpX = 1730.f;
	constexpr float WingLogX = 1792.f;
	constexpr float WingMenuX = 1854.f;

	/** WORLD_UI unit bar at 1080p. Buildings use the footprint instead of this width. */
	constexpr float UnitBarW = 44.f;
	constexpr float UnitBarH = 4.f;
	constexpr float BuildingBarH = 6.f;
	constexpr float BarGapAbove = 12.f;
	constexpr int32 UnitHpPerSegment = 25;
	constexpr int32 BuildingSegmentCap = 20;
}

/** One rectangle in the 1920×1080 painted HUD v3 design. */
struct FKodHudBox
{
	float X = 0.f;
	float Y = 0.f;
	float W = 0.f;
	float H = 0.f;
};

/** Painted HUD v3. Rects are design pixels. Textures live under /Game/Warden/UI/Painted_v3. */
struct FKodHudV3
{
	FKodHudBox Minimap;
	FKodHudBox Info;
	FKodHudBox Portrait;
	FKodHudBox Command;
	FKodHudBox Wing[3];
	FKodHudBox Menu;
	FKodHudBox MinimapTab[2];
	FKodHudBox MapButton[3];
	FKodHudBox Tab[10];
	FKodHudBox Banner;
	FKodHudBox Clock;
	bool bClockBox = true;
	float StoryW = 0.f;
	float StoryH = 0.f;
	FKodHudBox StoryInner;

	static int32 TabCount() { return 10; }

	/** Soft object path /Game/Warden/UI/Painted_v3/T_<Stem>.T_<Stem>. */
	static FString TexturePath(const TCHAR* Stem)
	{
		return FString::Printf(TEXT("/Game/Warden/UI/Painted_v3/T_%s.T_%s"), Stem, Stem);
	}

	static FString FramePath(bool bScinder)
	{
		return TexturePath(bScinder ? TEXT("sc_hud_frame") : TEXT("vg_hud_frame"));
	}

	static FString LightsPath()
	{
		return TexturePath(TEXT("vg_lights"));
	}

	static FString BannerPath(bool bScinder)
	{
		return TexturePath(bScinder ? TEXT("sc_achievement_banner") : TEXT("vg_achievement_banner"));
	}

	static FString TabPath(bool bScinder, int32 TabIndex, bool bAssigned)
	{
		const int32 Clamped = FMath::Clamp(TabIndex, 0, 9);
		const int32 Digit = Clamped == 9 ? 0 : Clamped + 1;
		return TexturePath(*FString::Printf(TEXT("%s_numtab_%d_%s"),
			bScinder ? TEXT("sc") : TEXT("vg"),
			Digit,
			bAssigned ? TEXT("assigned") : TEXT("empty")));
	}

	static FString StoryPath(bool bScinder)
	{
		return TexturePath(bScinder ? TEXT("sc_story_window_2x") : TEXT("vg_story_window_2x"));
	}

	static FString StoryMaskPath(bool bScinder)
	{
		return TexturePath(bScinder ? TEXT("sc_story_window_inner_mask_2x") : TEXT("vg_story_window_inner_mask_2x"));
	}

	static FString ClockBoxPath()
	{
		return TexturePath(TEXT("vg_clock_box"));
	}

	static FKodHudV3 ForFaction(bool bScinder);
};
