#pragma once

#include "CommonButtonBase.h"
#include "Input/Events.h"
#include "Layout/Geometry.h"
#include "Style/KodUITypes.h"
#include "KodFactionTileWidget.generated.h"

class UCommonTextBlock;
class UImage;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FKodFactionTileClicked, UKodFactionTileWidget*, Tile);

/**
 * One Versus faction tile. Cyan glass lobby chrome.
 * States: Idle, Hover, Selected, Disabled.
 * Parent WBP: /Game/UI/Lobby/WBP_FactionTile.
 * Named widgets: IconImage, LabelText, SelectionGlow.
 */
UCLASS(Blueprintable)
class KODUI_API UKodFactionTileWidget : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Kod|UI|Lobby")
	bool SetFaction(EKodVersusFaction InFaction);

	UFUNCTION(BlueprintPure, Category = "Kod|UI|Lobby")
	EKodVersusFaction GetFaction() const { return Faction; }

	UFUNCTION(BlueprintCallable, Category = "Kod|UI|Lobby")
	void SetTileState(EKodFactionTileState InState);

	UFUNCTION(BlueprintPure, Category = "Kod|UI|Lobby")
	EKodFactionTileState GetTileState() const { return TileState; }

	UPROPERTY(BlueprintAssignable, Category = "Kod|UI|Lobby")
	FKodFactionTileClicked OnTileClicked;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativePreConstruct() override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

	void ApplyState();
	void HandleInternalClicked();

	UPROPERTY(BlueprintReadOnly, Category = "Kod|UI|Lobby")
	EKodVersusFaction Faction = EKodVersusFaction::USA;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|UI|Lobby")
	EKodFactionTileState TileState = EKodFactionTileState::Idle;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UImage> IconImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UCommonTextBlock> LabelText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UImage> SelectionGlow;
};
