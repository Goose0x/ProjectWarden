#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
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

	bool bLoggedFirstDraw = false;
	int32 ShownJadeite = MIN_int32;
	int32 ShownLuminene = MIN_int32;
	int32 ShownSupplyUsed = 0;
	int32 ShownSupplyCap = 0;
	float LastReadoutPollSeconds = -1.f;
	TArray<FKodHudCommandHit> CommandHits;
	bool bCommandChrome = false;
	float ChromeMinX = 0.f;
	float ChromeMinY = 0.f;
	float ChromeMaxX = 0.f;
	float ChromeMaxY = 0.f;
	FString FlashText;
	float FlashUntilSeconds = -1.f;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> HudWidgetInstance;
};
