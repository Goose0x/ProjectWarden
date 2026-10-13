#pragma once

/**
 * Vanguard HUD spec, in 1920×1080 pixels, before AKodHUD::MapSpec scales them.
 * Adjust the v5 rects here. Draw code should not hardcode these numbers.
 * Single-unit silhouette is 160×160 inside the existing info well.
 * Each multi-select cell keeps its hit rect and draws a 48×48 silhouette inside it.
 * Imported wireframe PNGs are 160 and 48. Zone masks match those sizes.
 * Command icons are 128 PNGs scaled to the cell.
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
}
