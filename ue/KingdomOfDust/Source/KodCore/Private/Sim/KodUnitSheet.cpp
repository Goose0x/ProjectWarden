#include "Sim/KodUnitSheet.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Sim/KodBuildTicks.h"
#include "Sim/KodBuildTypes.h"
#include "Warden/KodWardenPaths.h"

bool FKodUnitCsvRow::HasTag(const TCHAR* Tag) const
{
	TArray<FString> Parts;
	Tags.ParseIntoArray(Parts, TEXT("|"), true);
	for (const FString& Part : Parts)
	{
		if (Part.TrimStartAndEnd().Equals(Tag, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}
	return false;
}

int32 FKodUnitCsvRow::BuildTicks() const
{
	return FMath::Max(0, FMath::RoundToInt(BuildSeconds * static_cast<float>(KodBuildTicks::SimHz)));
}

float FKodUnitCsvRow::MoveSpeedUU() const
{
	return MoveSpeedCells * static_cast<float>(KodBuildGrid::CellUU);
}

int32 FKodUnitCsvRow::SightUU() const
{
	return FMath::Max(0, FMath::RoundToInt(SightCells * static_cast<float>(KodBuildGrid::CellUU)));
}

FKodTrainSpec FKodUnitCsvRow::ToTrainSpec() const
{
	FKodTrainSpec Spec;
	Spec.UnitId = Id;
	Spec.DisplayId = FName(*DisplayName);
	Spec.Cost.Credits = Credits;
	Spec.Cost.Luminene = Luminene;
	Spec.Cost.Supply = SupplyCost;
	Spec.Cost.BuildTime = BuildTicks();
	Spec.bBiological = HasTag(TEXT("Biological")) || HasTag(TEXT("Organic"));
	Spec.bOrganic = Spec.bBiological;
	Spec.bInfantry = HasTag(TEXT("Infantry"));
	Spec.bFlyer = HasTag(TEXT("Air")) || HasTag(TEXT("Flyer"));
	Spec.bArmored = ArmorType.Equals(TEXT("Armored"), ESearchCase::IgnoreCase) || HasTag(TEXT("Armored"));
	Spec.bSoftCollision = HasTag(TEXT("SoftCollision"));
	Spec.bFromSheet = true;
	Spec.BaseArmor = Armor;
	Spec.SupplyProvided = SupplyProvided;
	Spec.SheetHealth = Hp;
	Spec.SheetMoveSpeed = MoveSpeedUU();
	Spec.SheetSightUU = SightUU();
	Spec.SheetWeaponDamage = Damage;
	if (ProducedAt.Equals(TEXT("SCI_HQ"), ESearchCase::IgnoreCase)
		|| ProducedAt.Equals(TEXT("ScinderHQ"), ESearchCase::IgnoreCase))
	{
		Spec.BuildingId = FName(KodWardenPaths::Id_ScinderHQ);
	}
	else if (!ProducedAt.IsEmpty())
	{
		Spec.BuildingId = FName(*ProducedAt);
	}
	return Spec;
}

FString FKodUnitSheet::DefaultPath()
{
	return FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("KodBalance/units.csv"));
}

namespace
{
	void SplitCsv(const FString& Line, TArray<FString>& Out)
	{
		Out.Reset();
		FString Field;
		bool bQuote = false;
		for (int32 Index = 0; Index < Line.Len(); ++Index)
		{
			const TCHAR Char = Line[Index];
			if (Char == TEXT('"'))
			{
				bQuote = !bQuote;
				continue;
			}
			if (Char == TEXT(',') && !bQuote)
			{
				Out.Add(Field.TrimStartAndEnd());
				Field.Reset();
				continue;
			}
			Field.AppendChar(Char);
		}
		Out.Add(Field.TrimStartAndEnd());
	}

	int32 Column(const TArray<FString>& Header, const TCHAR* Name)
	{
		for (int32 Index = 0; Index < Header.Num(); ++Index)
		{
			if (Header[Index].Equals(Name, ESearchCase::IgnoreCase))
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	FString Cell(const TArray<FString>& Fields, int32 Index)
	{
		return Fields.IsValidIndex(Index) ? Fields[Index] : FString();
	}
}

bool FKodUnitSheet::Find(FName Id, FKodUnitCsvRow& OutRow, const FString& Path, bool bLogPlaceholder)
{
	const FString File = Path.IsEmpty() ? DefaultPath() : Path;
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *File))
	{
		UE_LOG(LogTemp, Warning, TEXT("KodBalance units.csv miss %s"), *File);
		return false;
	}
	TArray<FString> Lines;
	Text.ParseIntoArrayLines(Lines, true);
	if (Lines.Num() < 2)
	{
		return false;
	}
	TArray<FString> Header;
	SplitCsv(Lines[0], Header);
	const int32 IdCol = Column(Header, TEXT("unit_id"));
	const int32 NameCol = Column(Header, TEXT("RowName"));
	if (IdCol == INDEX_NONE && NameCol == INDEX_NONE)
	{
		return false;
	}
	for (int32 LineIndex = 1; LineIndex < Lines.Num(); ++LineIndex)
	{
		TArray<FString> Fields;
		SplitCsv(Lines[LineIndex], Fields);
		const FString IdText = Cell(Fields, IdCol);
		const FString NameText = Cell(Fields, NameCol);
		if (!IdText.Equals(Id.ToString(), ESearchCase::IgnoreCase)
			&& !NameText.Equals(Id.ToString(), ESearchCase::IgnoreCase))
		{
			continue;
		}
		OutRow = FKodUnitCsvRow();
		OutRow.Id = FName(*IdText);
		OutRow.DisplayName = Cell(Fields, Column(Header, TEXT("display_name")));
		OutRow.ProducedAt = Cell(Fields, Column(Header, TEXT("produced_at")));
		OutRow.Credits = FCString::Atoi(*Cell(Fields, Column(Header, TEXT("credits_cost"))));
		OutRow.Luminene = FCString::Atoi(*Cell(Fields, Column(Header, TEXT("luminene_cost"))));
		OutRow.SupplyCost = FCString::Atoi(*Cell(Fields, Column(Header, TEXT("supply"))));
		OutRow.SupplyProvided = FCString::Atoi(*Cell(Fields, Column(Header, TEXT("supply_provided"))));
		OutRow.BuildSeconds = FCString::Atof(*Cell(Fields, Column(Header, TEXT("build_time_s"))));
		OutRow.Hp = FCString::Atof(*Cell(Fields, Column(Header, TEXT("hp"))));
		OutRow.Armor = FCString::Atoi(*Cell(Fields, Column(Header, TEXT("armor"))));
		OutRow.Damage = FCString::Atof(*Cell(Fields, Column(Header, TEXT("damage"))));
		OutRow.MoveSpeedCells = FCString::Atof(*Cell(Fields, Column(Header, TEXT("move_speed"))));
		OutRow.SightCells = FCString::Atof(*Cell(Fields, Column(Header, TEXT("sight"))));
		OutRow.ArmorType = Cell(Fields, Column(Header, TEXT("armor_type")));
		OutRow.Tags = Cell(Fields, Column(Header, TEXT("tags")));
		OutRow.bPlaceholder = Cell(Fields, Column(Header, TEXT("placeholder"))).Equals(TEXT("PLACEHOLDER"), ESearchCase::IgnoreCase);
		if (OutRow.bPlaceholder && bLogPlaceholder)
		{
			UE_LOG(LogTemp, Warning, TEXT("KodBalance Placeholder %s HP=%.0f Armor=%d Cost=%d Build=%.0fs SupplyCost=%d SupplyProvided=%d Speed=%.2f Sight=%.0f from units.csv"),
				*OutRow.Id.ToString(),
				OutRow.Hp,
				OutRow.Armor,
				OutRow.Credits,
				OutRow.BuildSeconds,
				OutRow.SupplyCost,
				OutRow.SupplyProvided,
				OutRow.MoveSpeedCells,
				OutRow.SightCells);
		}
		return true;
	}
	return false;
}
