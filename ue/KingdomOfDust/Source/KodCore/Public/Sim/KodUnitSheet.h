#pragma once

#include "CoreMinimal.h"
#include "Sim/KodProductionTypes.h"

/**
 * One row of Config/KodBalance/units.csv. Numbers stay in the sheet.
 * Speed and sight are sheet cells. One cell is KodBuildGrid::CellUU.
 * Build time is seconds. Ticks are seconds times the 16 Hz sim.
 */
struct KODCORE_API FKodUnitCsvRow
{
	FName Id;
	FString DisplayName;
	FString ProducedAt;
	int32 Credits = 0;
	int32 Luminene = 0;
	int32 SupplyCost = 0;
	int32 SupplyProvided = 0;
	float BuildSeconds = 0.f;
	float Hp = 0.f;
	int32 Armor = 0;
	float Damage = 0.f;
	float MoveSpeedCells = 0.f;
	float SightCells = 0.f;
	FString ArmorType;
	FString Tags;
	bool bPlaceholder = false;

	bool HasTag(const TCHAR* Tag) const;
	int32 BuildTicks() const;
	float MoveSpeedUU() const;
	int32 SightUU() const;
	FKodTrainSpec ToTrainSpec() const;
};

namespace FKodUnitSheet
{
	KODCORE_API FString DefaultPath();
	KODCORE_API bool Find(FName Id, FKodUnitCsvRow& OutRow, const FString& Path = FString(), bool bLogPlaceholder = true);
}
