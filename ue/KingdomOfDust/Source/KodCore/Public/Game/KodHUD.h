#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "KodHUD.generated.h"

class UUserWidget;

/**
 * Hosts UMG HUD root (WBP_KodHUD). DrawHUD kept minimal — prefer widgets.
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
