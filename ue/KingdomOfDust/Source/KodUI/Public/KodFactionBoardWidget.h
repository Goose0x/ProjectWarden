#pragma once

#include "CommonUserWidget.h"
#include "Style/KodUITypes.h"
#include "KodFactionBoardWidget.generated.h"

class UKodFactionTileWidget;
class UWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FKodFactionBoardChosen, EKodVersusFaction, Faction);

/**
 * Versus faction column. Cyan glass.
 * FactionBoardRoot holds FactionTile_USA and FactionTile_RSF only.
 * Parent WBP: /Game/UI/Lobby/WBP_FactionBoard (W_FactionBoard).
 */
UCLASS(Abstract, Blueprintable)
class KODUI_API UKodFactionBoardWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Kod|UI|Lobby")
	bool SetSelectedFaction(EKodVersusFaction Faction);

	UFUNCTION(BlueprintPure, Category = "Kod|UI|Lobby")
	EKodVersusFaction GetSelectedFaction() const { return SelectedFaction; }

	UPROPERTY(BlueprintAssignable, Category = "Kod|UI|Lobby")
	FKodFactionBoardChosen OnFactionChosen;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativePreConstruct() override;

	void ApplyTiles();

	UFUNCTION()
	void HandleTileClicked(UKodFactionTileWidget* Tile);

	UPROPERTY(BlueprintReadOnly, Category = "Kod|UI|Lobby")
	EKodVersusFaction SelectedFaction = EKodVersusFaction::USA;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UWidget> FactionBoardRoot;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UKodFactionTileWidget> FactionTile_USA;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UKodFactionTileWidget> FactionTile_RSF;
};
