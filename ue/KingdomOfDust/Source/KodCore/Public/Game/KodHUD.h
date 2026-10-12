#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Sim/KodSimSubsystem.h"
#include "KodHUD.generated.h"

class UUserWidget;

/** One clickable canvas command. Presentation only. Not sim state. */
struct KODCORE_API FKodHudCommandHit
{
	bool bInsideChrome = false;
	bool bEnabled = true;
	FName Action;
	FName Payload;
	int32 SlotIndex = INDEX_NONE;
	int32 StructureId = 0;
	FName FailReason;
	float MinX = 0.f;
	float MinY = 0.f;
	float MaxX = 0.f;
	float MaxY = 0.f;
	FString Tooltip;
};

/** Clickable canvas chrome outside the command card. Presentation only. */
struct KODCORE_API FKodHudChromeHit
{
	FName Kind;
	int32 IntParam = 0;
	FName NameParam;
	float MinX = 0.f;
	float MinY = 0.f;
	float MaxX = 0.f;
	float MaxY = 0.f;
};

/**
 * Hosts the in-game HUD. DrawHUD paints the resource readout, selection marquee,
 * unit HP bars, and the name/remaining label on a selected resource node
 * (engine canvas, no Content texture).
 * The readout is canvas text only: Credits, Luminene, and a grey supply line.
 * HudWidgetClass must be a Widget Blueprint parented to UKodHudRootWidget
 * (Ironstock). Expected content path: /Game/UI/HUD/WBP_KodHUD.
 * KodCore does not link KodUI; the soft class keeps that boundary.
 */
UCLASS(Blueprintable)
class KODCORE_API AKodHUD : public AHUD
{
	GENERATED_BODY()

public:
	AKodHUD();

	virtual void BeginPlay() override;
	virtual void DrawHUD() override;

	/** Last frame's command card. True when the cursor is on the card, including an empty cell. */
	bool GetCommandHit(float MouseX, float MouseY, FKodHudCommandHit& Out) const;

	/** Selection tiles, idle-worker button, and control-group tabs from the last draw. */
	bool GetChromeHit(float MouseX, float MouseY, FKodHudChromeHit& Out) const;

	/** True when the pixel is inside the minimap square from the last draw. */
	bool MinimapPixelToWorld(float MouseX, float MouseY, FVector& OutWorld, bool bClampToMap) const;

	/** Minimap, selection panel, or portrait. A left click here must not start a world box. */
	bool BlocksWorldSelect(float MouseX, float MouseY) const;

	void ToggleHotkeyLegend();

	void AddMinimapPing(const FVector& WorldPoint);
	void ToggleMinimapLayer(int32 LayerIndex);

	/** Selection panel, portrait, tabs, and the idle button. The minimap is separate. */
	bool IsPassiveHudChrome(float MouseX, float MouseY) const;

	int32 GetCommandSubgroupCount() const { return CommandSubgroupCount; }
	FName GetActiveCommandSubgroup() const { return ActiveCommandSubgroup; }

	/** Explored or visible for the local team. The fog layer toggle reveals everything. */
	virtual bool IsMinimapWorldRevealed(float WorldX, float WorldY) const;

	/**
	 * Render-target portrait hook. The default draws nothing and returns false
	 * so the canvas paints a silhouette letter instead.
	 */
	virtual bool DrawUnitPortrait(FName DisplayName, float X, float Y, float Width, float Height);

	/** Centre flash. Reason is Credits, Luminene, or Supply. */
	void ShowResourceFlash(FName Reason);

	/** Credits, Luminene, Supply, or None. Reads the local team's sim bank and supply. */
	FName PeekResourceFail(int32 Credits, int32 Luminene, int32 Supply) const;

	/** Soft ref to WBP_KodHUD (assign in BP / defaults). */
	UPROPERTY(EditDefaultsOnly, Category = "Kod|UI")
	TSoftClassPtr<UUserWidget> HudWidgetClass;

protected:
	void RefreshReadout();
	void DrawCanvasReadout(int32 Jadeite, int32 Luminene, int32 SupplyUsed, int32 SupplyCap);
	void DrawProductionPanel();
	void DrawBuildHint();
	void DrawCommandCard();
	void DrawSelectionSummary();
	void DrawResourceFlash();
	void DrawMatchOutcome();
	int32 ReadLocalTeamId() const;
	bool ReadTeamBank(int32& OutJadeite, int32& OutLuminene) const;
	void DrawUnitHealthBars();
	void DrawSelectedResourceNodes();
	void DrawSelectionMarquee();
	void DrawMinimap();
	void DrawIdleWorkerButton();
	void DrawPortrait();
	void DrawSelectionPanel();
	void DrawControlGroupTabs();
	void DrawHotkeyLegend();
	void DrawWorldFog();
	void DrawGameClock();
	void CacheSimSnapshot();
	const FKodSimEntityState* FindFrameState(int32 EntityId) const;
	const FKodStructureState* FindFrameStructure(int32 EntityId) const;
	void RememberChrome(FName Kind, int32 IntParam, FName NameParam, float MinX, float MinY, float MaxX, float MaxY);
	bool WorldToMinimap(float WorldX, float WorldY, float& OutX, float& OutY) const;

	bool bLoggedFirstDraw = false;
	int32 ShownJadeite = MIN_int32;
	int32 ShownLuminene = MIN_int32;
	int32 ShownSupplyUsed = 0;
	int32 ShownSupplyCap = 0;
	float LastReadoutPollSeconds = -1.f;
	TArray<FKodHudCommandHit> CommandHits;
	TArray<FKodHudChromeHit> ChromeHits;
	bool bCommandChrome = false;
	bool bHasMinimap = false;
	bool bHasSelectionPanel = false;
	bool bHasPortrait = false;
	float PanelMinX = 0.f;
	float PanelMinY = 0.f;
	float PanelMaxX = 0.f;
	float PanelMaxY = 0.f;
	float PortraitMinX = 0.f;
	float PortraitMinY = 0.f;
	float PortraitMaxX = 0.f;
	float PortraitMaxY = 0.f;
	bool bHotkeyLegend = false;
	bool bMapTerrain = true;
	bool bMapUnits = true;
	bool bMapResources = true;
	bool bMapFog = true;
	bool bMapAlerts = true;
	bool bMapOwnOnly = false;
	TArray<FVector> MinimapPings;
	TArray<float> MinimapPingUntil;
	TArray<FKodSimEntityState> FrameStates;
	TArray<AActor*> FrameActors;
	TArray<FKodStructureState> FrameStructures;
	float MapMinX = 0.f;
	float MapMinY = 0.f;
	float MapMaxX = 0.f;
	float MapMaxY = 0.f;
	float MapWorldMinX = 0.f;
	float MapWorldMinY = 0.f;
	float MapWorldMaxX = 1.f;
	float MapWorldMaxY = 1.f;
	float MapWorldZ = 0.f;
	float PassiveMinX = 0.f;
	float PassiveMinY = 0.f;
	float PassiveMaxX = 0.f;
	float PassiveMaxY = 0.f;
	bool bHasPassiveChrome = false;
	int32 CommandSubgroupCount = 0;
	FName ActiveCommandSubgroup;
	float ChromeMinX = 0.f;
	float ChromeMinY = 0.f;
	float ChromeMaxX = 0.f;
	float ChromeMaxY = 0.f;
	FString FlashText;
	float FlashUntilSeconds = -1.f;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> HudWidgetInstance;
};
