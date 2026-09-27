#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "KodHUD.generated.h"

class UUserWidget;

/**
 * Hosts the in-game HUD. DrawHUD stays empty — the widget is the HUD.
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

	/** Soft ref to WBP_KodHUD (assign in BP / defaults). */
	UPROPERTY(EditDefaultsOnly, Category = "Kod|UI")
	TSoftClassPtr<UUserWidget> HudWidgetClass;

protected:
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> HudWidgetInstance;
};
