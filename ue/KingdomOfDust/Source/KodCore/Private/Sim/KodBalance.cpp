#include "Sim/KodSimSubsystem.h"

#include "Engine/DataTable.h"
#include "Misc/Crc.h"
#include "Sim/KodBuildTicks.h"
#include "Sim/KodResourceTypes.h"
#include "UObject/UnrealType.h"
#include "Warden/KodWardenPaths.h"

namespace
{
	UDataTable* MakeTable(UObject* Outer, UScriptStruct* RowStruct, const FName& Name)
	{
		UDataTable* Table = NewObject<UDataTable>(Outer, Name);
		Table->RowStruct = RowStruct;
		return Table;
	}

	template <typename RowType>
	RowType* FindMutableRow(UDataTable* Table, FName RowName)
	{
		if (!Table)
		{
			return nullptr;
		}
		return Table->FindRow<RowType>(RowName, TEXT("KodBalance"), false);
	}

	void HashTable(const UDataTable* Table, TArray<uint8>& Bytes)
	{
		auto Append = [&Bytes](const void* Data, int32 Size)
		{
			const int32 Offset = Bytes.Num();
			Bytes.AddUninitialized(Size);
			FMemory::Memcpy(Bytes.GetData() + Offset, Data, Size);
		};
		if (!Table || !Table->GetRowStruct())
		{
			const int32 Zero = 0;
			Append(&Zero, sizeof(Zero));
			return;
		}
		TArray<FName> Names;
		Table->GetRowMap().GetKeys(Names);
		Names.Sort([](const FName& A, const FName& B)
		{
			return A.ToString() < B.ToString();
		});
		const int32 Count = Names.Num();
		Append(&Count, sizeof(Count));
		for (const FName& Name : Names)
		{
			const FString Label = Name.ToString();
			const int32 NameCrc = static_cast<int32>(FCrc::StrCrc32(*Label));
			Append(&NameCrc, sizeof(NameCrc));
			uint8* const* RowPtr = Table->GetRowMap().Find(Name);
			if (!RowPtr || !*RowPtr)
			{
				continue;
			}
			for (TFieldIterator<FIntProperty> It(Table->GetRowStruct()); It; ++It)
			{
				const int32 Value = It->GetPropertyValue_InContainer(*RowPtr);
				Append(&Value, sizeof(Value));
			}
		}
	}
}

void UKodSimSubsystem::EnsureBalanceTables()
{
	if (!AbilityTable)
	{
		AbilityTable = MakeTable(this, FKodAbilityBalanceRow::StaticStruct(), FName(TEXT("Ability")));
	}
	if (!UnitTable)
	{
		UnitTable = MakeTable(this, FKodUnitBalanceRow::StaticStruct(), FName(TEXT("Unit")));
	}
	if (!WeaponTable)
	{
		WeaponTable = MakeTable(this, FKodWeaponBalanceRow::StaticStruct(), FName(TEXT("Weapon")));
	}
	if (!StructureTable)
	{
		StructureTable = MakeTable(this, FKodStructureBalanceRow::StaticStruct(), FName(TEXT("Structure")));
	}
	if (!UpgradeTable)
	{
		UpgradeTable = MakeTable(this, FKodUpgradeBalanceRow::StaticStruct(), FName(TEXT("Upgrade")));
	}

	auto AddAbility = [this](const TCHAR* Id, int32 HealthCost, int32 Duration, int32 MovePermille, int32 AttackPermille)
	{
		if (AbilityTable->FindRow<FKodAbilityBalanceRow>(FName(Id), TEXT("KodBalance"), false))
		{
			return;
		}
		FKodAbilityBalanceRow Row;
		Row.HealthCost = HealthCost;
		Row.DurationTicks = Duration;
		Row.MovePermille = MovePermille;
		Row.AttackPermille = AttackPermille;
		AbilityTable->AddRow(FName(Id), Row);
	};
	auto AddUnit = [this](const TCHAR* Id, const FKodUnitBalanceRow& Row)
	{
		if (UnitTable->FindRow<FKodUnitBalanceRow>(FName(Id), TEXT("KodBalance"), false))
		{
			return;
		}
		UnitTable->AddRow(FName(Id), Row);
	};
	auto AddWeapon = [this](const TCHAR* Id, int32 Damage, int32 Range, int32 CooldownTicks)
	{
		if (WeaponTable->FindRow<FKodWeaponBalanceRow>(FName(Id), TEXT("KodBalance"), false))
		{
			return;
		}
		FKodWeaponBalanceRow Row;
		Row.Damage = Damage;
		Row.Range = Range;
		Row.CooldownTicks = CooldownTicks;
		WeaponTable->AddRow(FName(Id), Row);
	};
	auto AddStructure = [this](const TCHAR* Id, const FKodStructureBalanceRow& Row)
	{
		if (StructureTable->FindRow<FKodStructureBalanceRow>(FName(Id), TEXT("KodBalance"), false))
		{
			return;
		}
		StructureTable->AddRow(FName(Id), Row);
	};
	auto AddUpgrade = [this](const TCHAR* Id, int32 WeaponBonus, int32 ArmorBonus)
	{
		if (UpgradeTable->FindRow<FKodUpgradeBalanceRow>(FName(Id), TEXT("KodBalance"), false))
		{
			return;
		}
		FKodUpgradeBalanceRow Row;
		Row.Credits = 100;
		Row.Luminene = 100;
		Row.BuildTicks = 114 * 16;
		Row.WeaponBonus = WeaponBonus;
		Row.ArmorBonus = ArmorBonus;
		UpgradeTable->AddRow(FName(Id), Row);
	};

	AddAbility(KodWardenPaths::Id_CombatStim, 10, 11 * 16, 1500, 1500);
	AddWeapon(KodWardenPaths::Id_RangerRifle, 12, 900, 13);

	FKodUnitBalanceRow Dozer;
	Dozer.MaxHealth = 200;
	Dozer.Armor = 1;
	Dozer.MoveSpeed = 350;
	Dozer.Sight = 1200;
	Dozer.Credits = KodEconomyDefaults::AmpCredits;
	Dozer.Luminene = KodEconomyDefaults::AmpLuminene;
	Dozer.Supply = KodEconomyDefaults::AmpSupply;
	Dozer.BuildTicks = KodEconomyDefaults::AmpBuildTicks;
	Dozer.Biological = 1;
	Dozer.CanGather = 1;
	AddUnit(KodWardenPaths::Id_Dozer, Dozer);

	FKodUnitBalanceRow Ranger;
	Ranger.MaxHealth = 120;
	Ranger.Armor = 0;
	Ranger.MoveSpeed = 450;
	Ranger.Sight = 1600;
	Ranger.Credits = 75;
	Ranger.Supply = 1;
	Ranger.BuildTicks = 80;
	Ranger.Biological = 1;
	Ranger.Infantry = 1;
	Ranger.CargoSize = 1;
	AddUnit(KodWardenPaths::Id_Ranger, Ranger);

	FKodUnitBalanceRow Medic;
	Medic.MaxHealth = 60;
	Medic.Armor = 1;
	Medic.MoveSpeed = 400;
	Medic.Sight = 1400;
	Medic.Credits = 50;
	Medic.Luminene = 25;
	Medic.Supply = 1;
	Medic.BuildTicks = 20 * 16;
	Medic.Biological = 1;
	Medic.Infantry = 1;
	Medic.Healer = 1;
	Medic.CargoSize = 1;
	Medic.EnergyMax = 200;
	Medic.EnergyStart = 50;
	AddUnit(KodWardenPaths::Id_Medic, Medic);

	auto Structure = [](int32 Health, int32 Credits, int32 Ticks, int32 Supply, int32 FootX, int32 FootY, int32 Sight)
	{
		FKodStructureBalanceRow Row;
		Row.MaxHealth = Health;
		Row.Credits = Credits;
		Row.BuildTicks = Ticks;
		Row.SupplyProvided = Supply;
		Row.FootprintX = FootX;
		Row.FootprintY = FootY;
		Row.Sight = Sight;
		return Row;
	};
	FKodStructureBalanceRow Cc = Structure(2500, 400, 71 * 16, KodEconomyDefaults::SupplyPerHq, 5, 5, 2000);
	Cc.TownHall = 1;
	Cc.DropOff = 1;
	AddStructure(KodWardenPaths::Id_CommandCenter, Cc);
	FKodStructureBalanceRow Depot = Structure(400, 100, 21 * 16, KodEconomyDefaults::SupplyPerDepot, 2, 2, 0);
	Depot.CanLower = 1;
	AddStructure(KodWardenPaths::Id_SupplyDepot, Depot);
	AddStructure(KodWardenPaths::Id_Barracks, Structure(1500, 150, 46 * 16, 0, 3, 3, 1400));
	FKodStructureBalanceRow Rig = Structure(500, 75, 30 * 16, 0, 3, 3, 0);
	Rig.Extractor = 1;
	AddStructure(KodWardenPaths::Id_Rig, Rig);
	AddStructure(KodWardenPaths::Id_EngineeringBay, Structure(850, 125, 25 * 16, 0, 3, 3, 1400));
	FKodStructureBalanceRow Bunker = Structure(400, 100, 30 * 16, 0, 3, 3, 1400);
	Bunker.CargoCap = 4;
	AddStructure(KodWardenPaths::Id_Bunker, Bunker);
	FKodStructureBalanceRow Turret = Structure(250, 100, 25 * 16, 0, 2, 2, 1400);
	Turret.MenuTier = 1;
	Turret.AttackDamage = 12;
	Turret.AttackRangeUU = 7 * 200;
	Turret.AttacksAir = 1;
	AddStructure(KodWardenPaths::Id_Turret, Turret);

	AddUpgrade(KodWardenPaths::Id_InfantryWeapons, 1, 0);
	AddUpgrade(KodWardenPaths::Id_InfantryArmor, 0, 1);
}

void UKodSimSubsystem::ApplyBalance()
{
	EnsureBalanceTables();
	TArray<FName> UnitNames;
	UnitTable->GetRowMap().GetKeys(UnitNames);
	UnitNames.Sort([](const FName& A, const FName& B) { return A.ToString() < B.ToString(); });
	for (const FName& Name : UnitNames)
	{
		const FKodUnitBalanceRow* Row = FindMutableRow<FKodUnitBalanceRow>(UnitTable, Name);
		if (!Row)
		{
			continue;
		}
		if (FKodTrainSpec* Spec = TrainSpecs.Find(Name))
		{
			Spec->Cost.Credits = Row->Credits;
			Spec->Cost.Luminene = Row->Luminene;
			Spec->Cost.Supply = Row->Supply;
			Spec->Cost.BuildTime = Row->BuildTicks;
			Spec->bBiological = Row->Biological != 0;
			Spec->bInfantry = Row->Infantry != 0;
			Spec->bFlyer = Row->Flyer != 0;
			Spec->bHealer = Row->Healer != 0;
			Spec->bCanGather = Row->CanGather != 0;
			Spec->CargoSize = Row->CargoSize;
			Spec->BaseArmor = Row->Armor;
			Spec->EnergyMax = Row->EnergyMax;
			Spec->EnergyStart = Row->EnergyStart;
		}
		const FKodWeaponBalanceRow* Weapon = nullptr;
		if (Name == FName(KodWardenPaths::Id_Ranger))
		{
			Weapon = FindMutableRow<FKodWeaponBalanceRow>(WeaponTable, FName(KodWardenPaths::Id_RangerRifle));
		}
		TArray<int32> Keys;
		States.GetKeys(Keys);
		Keys.Sort();
		for (int32 Key : Keys)
		{
			FKodSimEntityState* State = States.Find(Key);
			if (!State || State->DefinitionId != Name)
			{
				continue;
			}
			const int32 OldMax = FMath::Max(1, FMath::RoundToInt(State->MaxHealth));
			const bool bFull = State->Health + 0.5f >= static_cast<float>(OldMax);
			State->MaxHealth = static_cast<float>(FMath::Max(1, Row->MaxHealth));
			State->Health = bFull ? State->MaxHealth : FMath::Min(State->Health, State->MaxHealth);
			State->MoveSpeed = static_cast<float>(FMath::Max(0, Row->MoveSpeed));
			State->ArmorPoints = FMath::Max(0, Row->Armor);
			State->SightRadiusUU = FMath::Max(0, Row->Sight);
			State->bBiological = Row->Biological != 0;
			State->bInfantry = Row->Infantry != 0;
			State->bFlyer = Row->Flyer != 0;
			State->bHealer = Row->Healer != 0;
			State->bCanGather = Row->CanGather != 0;
			State->CargoSize = Row->CargoSize;
			if (State->EnergyMax <= 0)
			{
				State->EnergyMax = Row->EnergyMax;
				State->Energy = Row->EnergyStart;
			}
			if (Weapon)
			{
				State->WeaponDamage = static_cast<float>(Weapon->Damage);
				State->WeaponRange = static_cast<float>(Weapon->Range);
				State->WeaponCooldownTicks = FMath::Max(0, Weapon->CooldownTicks);
				State->WeaponCooldownSeconds = static_cast<float>(State->WeaponCooldownTicks) * KodBuildTicks::SimDt;
			}
		}
	}

	TArray<FName> StructureNames;
	StructureTable->GetRowMap().GetKeys(StructureNames);
	StructureNames.Sort([](const FName& A, const FName& B) { return A.ToString() < B.ToString(); });
	for (const FName& Name : StructureNames)
	{
		const FKodStructureBalanceRow* Row = FindMutableRow<FKodStructureBalanceRow>(StructureTable, Name);
		if (!Row)
		{
			continue;
		}
		if (FKodBuildSpec* Spec = BuildSpecs.Find(Name))
		{
			Spec->Cost.Credits = Row->Credits;
			Spec->Cost.Luminene = Row->Luminene;
			Spec->Cost.BuildTime = Row->BuildTicks;
			Spec->MaxHealth = static_cast<float>(FMath::Max(1, Row->MaxHealth));
			Spec->SupplyProvided = Row->SupplyProvided;
			Spec->FootprintX = FMath::Max(1, Row->FootprintX);
			Spec->FootprintY = FMath::Max(1, Row->FootprintY);
			Spec->MenuTier = Row->MenuTier;
			Spec->bTownHall = Row->TownHall != 0;
			Spec->bCanLower = Row->CanLower != 0;
			Spec->CargoCap = Row->CargoCap;
			Spec->AttackDamage = Row->AttackDamage;
			Spec->AttackRangeUU = Row->AttackRangeUU;
			Spec->bAttacksAir = Row->AttacksAir != 0;
			Spec->bExtractor = Row->Extractor != 0;
			Spec->bIsDropOff = Row->DropOff != 0;
		}
		TArray<int32> Keys;
		Structures.GetKeys(Keys);
		Keys.Sort();
		for (int32 Key : Keys)
		{
			FKodStructureState* Structure = Structures.Find(Key);
			if (!Structure || Structure->DefinitionId != Name)
			{
				continue;
			}
			Structure->MaxHealth = static_cast<float>(FMath::Max(1, Row->MaxHealth));
			Structure->SupplyProvided = Row->SupplyProvided;
			Structure->CargoCap = Row->CargoCap;
			Structure->AttackDamage = Row->AttackDamage;
			Structure->AttackRangeUU = Row->AttackRangeUU;
			Structure->bAttacksAir = Row->AttacksAir != 0;
			if (FKodSimEntityState* Combat = States.Find(Key))
			{
				const int32 OldMax = FMath::Max(1, FMath::RoundToInt(Combat->MaxHealth));
				const bool bFull = Combat->Health + 0.5f >= static_cast<float>(OldMax);
				Combat->MaxHealth = Structure->MaxHealth;
				Combat->Health = bFull ? Combat->MaxHealth : FMath::Min(Combat->Health, Combat->MaxHealth);
				Combat->SightRadiusUU = FMath::Max(Combat->SightRadiusUU, Row->Sight);
				if (Row->AttackDamage > 0)
				{
					Combat->WeaponDamage = static_cast<float>(Row->AttackDamage);
					Combat->WeaponRange = static_cast<float>(Row->AttackRangeUU);
				}
			}
		}
	}

	TArray<FName> AbilityNames;
	AbilityTable->GetRowMap().GetKeys(AbilityNames);
	AbilityNames.Sort([](const FName& A, const FName& B) { return A.ToString() < B.ToString(); });
	for (const FName& Name : AbilityNames)
	{
		const FKodAbilityBalanceRow* Row = FindMutableRow<FKodAbilityBalanceRow>(AbilityTable, Name);
		FKodAbilitySpec* Spec = AbilitySpecs.Find(Name);
		if (!Row || !Spec)
		{
			continue;
		}
		Spec->Credits = Row->Credits;
		Spec->Luminene = Row->Luminene;
		Spec->HealthCost = Row->HealthCost;
		Spec->CooldownTicks = Row->CooldownTicks;
		Spec->DurationTicks = Row->DurationTicks;
		Spec->MovePermille = Row->MovePermille;
		Spec->AttackPermille = Row->AttackPermille;
	}

	TArray<FName> UpgradeNames;
	UpgradeTable->GetRowMap().GetKeys(UpgradeNames);
	UpgradeNames.Sort([](const FName& A, const FName& B) { return A.ToString() < B.ToString(); });
	for (const FName& Name : UpgradeNames)
	{
		const FKodUpgradeBalanceRow* Row = FindMutableRow<FKodUpgradeBalanceRow>(UpgradeTable, Name);
		FKodUpgradeSpec* Spec = UpgradeSpecs.Find(Name);
		if (!Row || !Spec)
		{
			continue;
		}
		Spec->Credits = Row->Credits;
		Spec->Luminene = Row->Luminene;
		Spec->BuildTicks = Row->BuildTicks;
		Spec->WeaponBonus = Row->WeaponBonus;
		Spec->ArmorBonus = Row->ArmorBonus;
	}

	RecalcSupplyCap(0);
	RecalcSupplyCap(1);
}

bool UKodSimSubsystem::SetBalanceField(FName TableName, FName RowName, FName Field, int32 Value)
{
	EnsureBalanceTables();
	UDataTable* Table = nullptr;
	const FString Which = TableName.ToString();
	if (Which.Equals(TEXT("Ability"), ESearchCase::IgnoreCase))
	{
		Table = AbilityTable;
	}
	else if (Which.Equals(TEXT("Unit"), ESearchCase::IgnoreCase))
	{
		Table = UnitTable;
	}
	else if (Which.Equals(TEXT("Weapon"), ESearchCase::IgnoreCase))
	{
		Table = WeaponTable;
	}
	else if (Which.Equals(TEXT("Structure"), ESearchCase::IgnoreCase))
	{
		Table = StructureTable;
	}
	else if (Which.Equals(TEXT("Upgrade"), ESearchCase::IgnoreCase))
	{
		Table = UpgradeTable;
	}
	if (!Table || !Table->GetRowStruct())
	{
		UE_LOG(LogTemp, Warning, TEXT("KodBalance Miss Table=%s"), *Which);
		return false;
	}
	uint8** RowPtr = Table->GetRowMap().Find(RowName);
	if (!RowPtr || !*RowPtr)
	{
		UE_LOG(LogTemp, Warning, TEXT("KodBalance Miss Row=%s"), *RowName.ToString());
		return false;
	}
	FIntProperty* Prop = FindFProperty<FIntProperty>(Table->GetRowStruct(), Field);
	if (!Prop)
	{
		UE_LOG(LogTemp, Warning, TEXT("KodBalance Miss Field=%s"), *Field.ToString());
		return false;
	}
	Prop->SetPropertyValue_InContainer(*RowPtr, Value);
	ApplyBalance();
	UE_LOG(LogTemp, Log, TEXT("KodBalance Table=%s Row=%s Field=%s Value=%d"),
		*Which,
		*RowName.ToString(),
		*Field.ToString(),
		Value);
	return true;
}

void UKodSimSubsystem::AppendBalanceHash(TArray<uint8>& Bytes) const
{
	HashTable(AbilityTable, Bytes);
	HashTable(UnitTable, Bytes);
	HashTable(WeaponTable, Bytes);
	HashTable(StructureTable, Bytes);
	HashTable(UpgradeTable, Bytes);
}
