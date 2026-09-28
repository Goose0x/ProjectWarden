#pragma once

#include "KodUIScreenRoot.h"
#include "Style/KodUITypes.h"
#include "KodHudRootWidget.generated.h"

class UHorizontalBox;
class UImage;
class UKodChatMenuClusterWidget;
class UKodCommandCardWidget;
class UKodMinimapWidget;
class UKodPortraitFrameWidget;
class UKodResourceBarWidget;
class UKodSelectionPanelWidget;
class USpacer;
class UWidget;

/**
 * In-game HUD root. Ironstock only — cyan glass is not a legal material here.
 * Assign a Widget Blueprint at /Game/UI/HUD/WBP_KodHUD on AKodHUD.
 *
 * Canvas children:
 *   ResourceBar — top-right, icon + number, outside the bottom band
 *   HudBand — lower 28% of the viewport
 * HudColumns (left to right, weights 18 / 42 / 15 / 20 / 5):
 *   Column_Minimap, Column_Selection, Column_Portrait, Column_Command, Column_Gutter
 * ChatMenuCluster sits above CommandCard inside the command column.
 */
UCLASS(Abstract, Blueprintable)
class KODUI_API UKodHudRootWidget : public UKodUIScreenRoot
{
	GENERATED_BODY()

public:
	UKodHudRootWidget();

	UFUNCTION(BlueprintPure, Category = "Kod|UI|HUD")
	UKodResourceBarWidget* GetResourceBar() const { return ResourceBar; }

	UFUNCTION(BlueprintPure, Category = "Kod|UI|HUD")
	UKodMinimapWidget* GetMinimap() const { return Minimap; }

	UFUNCTION(BlueprintPure, Category = "Kod|UI|HUD")
	UKodSelectionPanelWidget* GetSelectionPanel() const { return SelectionPanel; }

	UFUNCTION(BlueprintPure, Category = "Kod|UI|HUD")
	UKodPortraitFrameWidget* GetPortrait() const { return Portrait; }

	UFUNCTION(BlueprintPure, Category = "Kod|UI|HUD")
	UKodCommandCardWidget* GetCommandCard() const { return CommandCard; }

	UFUNCTION(BlueprintPure, Category = "Kod|UI|HUD")
	UKodChatMenuClusterWidget* GetChatMenuCluster() const { return ChatMenuCluster; }

	/**
	 * Stores a HUD accent theme id. USA_Ironstock or RSF_RustOrange.
	 * Does not retint materials or colors. USA, RSF, CN, and RU palettes are on HOLD.
	 * Proportions stay on layout v4. Cyan glass is rejected.
	 */
	UFUNCTION(BlueprintCallable, Category = "Kod|UI|HUD")
	void ApplyFactionAccentTheme(EKodFactionAccentTheme Theme);

	UFUNCTION(BlueprintPure, Category = "Kod|UI|HUD")
	EKodFactionAccentTheme GetFactionAccentTheme() const { return AccentTheme; }

protected:
	virtual void NativePreConstruct() override;
	void ApplyStampProportions();

	UPROPERTY(BlueprintReadOnly, Category = "Kod|UI|HUD")
	EKodFactionAccentTheme AccentTheme = EKodFactionAccentTheme::USA_Ironstock;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|HUD")
	TObjectPtr<UKodResourceBarWidget> ResourceBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|HUD")
	TObjectPtr<UWidget> HudBand;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|HUD")
	TObjectPtr<UHorizontalBox> HudColumns;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|HUD")
	TObjectPtr<UWidget> Column_Minimap;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|HUD")
	TObjectPtr<UWidget> Column_Selection;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|HUD")
	TObjectPtr<UWidget> Column_Portrait;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|HUD")
	TObjectPtr<UWidget> Column_Command;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|HUD")
	TObjectPtr<USpacer> Column_Gutter;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|HUD")
	TObjectPtr<UKodMinimapWidget> Minimap;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|HUD")
	TObjectPtr<UKodSelectionPanelWidget> SelectionPanel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|HUD")
	TObjectPtr<UKodPortraitFrameWidget> Portrait;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|HUD")
	TObjectPtr<UKodCommandCardWidget> CommandCard;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|HUD")
	TObjectPtr<UKodChatMenuClusterWidget> ChatMenuCluster;

	/** Optional dark plate. Tint stays metal. Do not assign a cyan glass material. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Kod|UI|HUD")
	TObjectPtr<UImage> IronstockPlate;
};
