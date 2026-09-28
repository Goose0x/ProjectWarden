#include "KodFactionBoardWidget.h"
#include "KodFactionTileWidget.h"
#include "KodUI.h"

void UKodFactionBoardWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (FactionTile_USA)
	{
		FactionTile_USA->SetFaction(EKodVersusFaction::USA);
		FactionTile_USA->OnTileClicked.AddUniqueDynamic(this, &UKodFactionBoardWidget::HandleTileClicked);
	}
	if (FactionTile_RSF)
	{
		FactionTile_RSF->SetFaction(EKodVersusFaction::RSF);
		FactionTile_RSF->OnTileClicked.AddUniqueDynamic(this, &UKodFactionBoardWidget::HandleTileClicked);
	}
	ApplyTiles();
}

void UKodFactionBoardWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	if (FactionTile_USA)
	{
		FactionTile_USA->SetFaction(EKodVersusFaction::USA);
	}
	if (FactionTile_RSF)
	{
		FactionTile_RSF->SetFaction(EKodVersusFaction::RSF);
	}
	ApplyTiles();
}

bool UKodFactionBoardWidget::SetSelectedFaction(EKodVersusFaction Faction)
{
	const bool bAllowed = Faction == EKodVersusFaction::USA || Faction == EKodVersusFaction::RSF;
	if (!bAllowed)
	{
		UE_LOG(LogKodUI, Error, TEXT("Faction board is USA and RSF only."));
		return false;
	}
	UKodFactionTileWidget* Tile = Faction == EKodVersusFaction::RSF ? FactionTile_RSF.Get() : FactionTile_USA.Get();
	if (Tile && Tile->GetTileState() == EKodFactionTileState::Disabled)
	{
		UE_LOG(LogKodUI, Error, TEXT("A disabled faction tile cannot be selected."));
		return false;
	}
	SelectedFaction = Faction;
	ApplyTiles();
	return true;
}

void UKodFactionBoardWidget::ApplyTiles()
{
	auto Paint = [this](UKodFactionTileWidget* Tile, EKodVersusFaction Faction)
	{
		if (!Tile || Tile->GetTileState() == EKodFactionTileState::Disabled)
		{
			return;
		}
		Tile->SetTileState(SelectedFaction == Faction
			? EKodFactionTileState::Selected
			: EKodFactionTileState::Idle);
	};
	Paint(FactionTile_USA, EKodVersusFaction::USA);
	Paint(FactionTile_RSF, EKodVersusFaction::RSF);
}

void UKodFactionBoardWidget::HandleTileClicked(UKodFactionTileWidget* Tile)
{
	if (!Tile || !SetSelectedFaction(Tile->GetFaction()))
	{
		return;
	}
	OnFactionChosen.Broadcast(SelectedFaction);
}
