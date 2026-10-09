#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "KodHUD.generated.h"

class UUserWidget;
class UKodResourceReadoutWidget;

/**
 * Hosts the in-game HUD. Chrome stays on the widget; DrawHUD paints the
 * selection marquee, unit HP bars, and the name/remaining label on a selected
 * resource node (engine canvas, no Content texture).
 * A C++ resource readout (Jadeite, Oil, grey 0/10 supply) is added in BeginPlay.
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

	/** Soft ref to WBP_KodHUD (assign in BP / defaults). */
	UPROPERTY(EditDefaultsOnly, Category = "Kod|UI")
	TSoftClassPtr<UUserWidget> HudWidgetClass;

protected:
	void DrawUnitHealthBars();
	void DrawSelectedResourceNodes();
	void DrawSelectionMarquee();

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> HudWidgetInstance;

	UPROPERTY(Transient)
	TObjectPtr<UKodResourceReadoutWidget> ResourceReadout;
};
