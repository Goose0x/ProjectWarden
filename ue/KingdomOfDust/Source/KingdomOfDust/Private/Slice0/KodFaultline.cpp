#include "Slice0/KodFaultline.h"

#include "Actors/KodBuilding.h"
#include "Actors/KodResourceNode.h"
#include "Actors/KodUnit.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Data/KodBuildingDefinition.h"
#include "Data/KodResourceNodeDefinition.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Game/KodHUD.h"
#include "GameFramework/PlayerController.h"
#include "Sim/KodSimSubsystem.h"
#include "Slice0/KodSlice0Bootstrap.h"
#include "Slice0/KodSlice0GameMode.h"
#include "Slice0/KodWorldSettings.h"
#include "Warden/KodWardenPaths.h"

namespace
{

	bool LoadJson(const TCHAR* FileName, TSharedPtr<FJsonObject>& OutRoot)
	{
		const FString Path = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("KodDress/faultline_training"), FileName);
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Path))
		{
			UE_LOG(LogTemp, Warning, TEXT("KodFaultline File Miss %s"), *Path);
			return false;
		}
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		return FJsonSerializer::Deserialize(Reader, OutRoot) && OutRoot.IsValid();
	}

	FVector RectCenter(const TArray<TSharedPtr<FJsonValue>>* Rect, float Z)
	{
		int32 X = 0;
		int32 Y = 0;
		int32 W = 1;
		int32 H = 1;
		if (Rect && Rect->Num() >= 4)
		{
			X = FMath::RoundToInt((*Rect)[0]->AsNumber());
			Y = FMath::RoundToInt((*Rect)[1]->AsNumber());
			W = FMath::Max(1, FMath::RoundToInt((*Rect)[2]->AsNumber()));
			H = FMath::Max(1, FMath::RoundToInt((*Rect)[3]->AsNumber()));
		}
		return FVector((static_cast<float>(X) + static_cast<float>(W) * 0.5f) * 200.f - 11200.f,
			(static_cast<float>(Y) + static_cast<float>(H) * 0.5f) * 200.f - 11200.f,
			Z);
	}

	void SpawnOctagon(UWorld* World, const FVector& Center)
	{
		UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		if (!Cylinder)
		{
			return;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStaticMeshActor* Pad = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(Center), Params);
		if (!Pad)
		{
			return;
		}
		Pad->Tags.Add(FName(TEXT("KodBaseSocket")));
		UStaticMeshComponent* Mesh = Pad->GetStaticMeshComponent();
		if (!Mesh)
		{
			return;
		}
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetStaticMesh(Cylinder);
		Mesh->SetWorldScale3D(FVector(18.f, 18.f, 0.4f));
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCanEverAffectNavigation(false);
	}

	void PlaceResources(UWorld* World, UKodSimSubsystem* Sim)
	{
		TSharedPtr<FJsonObject> Root;
		if (!LoadJson(TEXT("layout.json"), Root))
		{
			return;
		}
		const TArray<TSharedPtr<FJsonValue>>* Resources = nullptr;
		if (!Root->TryGetArrayField(TEXT("resources"), Resources) || !Resources)
		{
			return;
		}
		UKodSlice0Bootstrap::EnsureCatalog(World->GetAuthGameMode());
		UKodResourceNodeDefinition* JadeiteDef = UKodSlice0Bootstrap::ResolveResourceNode(FName(KodWardenPaths::Id_JadeiteNode), World->GetAuthGameMode());
		UKodResourceNodeDefinition* LumineneDef = UKodSlice0Bootstrap::ResolveResourceNode(FName(KodWardenPaths::Id_LumineneVent), World->GetAuthGameMode());
		for (const TSharedPtr<FJsonValue>& Value : *Resources)
		{
			const TSharedPtr<FJsonObject> Row = Value.IsValid() ? Value->AsObject() : nullptr;
			if (!Row.IsValid())
			{
				continue;
			}
			FString Type;
			FString Base;
			Row->TryGetStringField(TEXT("type"), Type);
			Row->TryGetStringField(TEXT("base"), Base);
			const TArray<TSharedPtr<FJsonValue>>* Rect = nullptr;
			Row->TryGetArrayField(TEXT("rect"), Rect);
			const float Z = Base.Contains(TEXT("PLAYER_MAIN")) ? 300.f : 0.f;
			const FVector At = RectCenter(Rect, Z);
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AKodResourceNode* Node = World->SpawnActor<AKodResourceNode>(AKodResourceNode::StaticClass(), FTransform(At), Params);
			if (!Node)
			{
				continue;
			}
			const bool bVent = Type.Contains(TEXT("LUM")) || Type.Contains(TEXT("VENT"));
			const UKodResourceNodeDefinition* Def = bVent ? LumineneDef : JadeiteDef;
			if (Def)
			{
				Node->ApplyDefinition(Def);
			}
			else
			{
				Node->ApplyPlaceholder(bVent ? EKodResourceType::Luminene : EKodResourceType::Jadeite);
			}
			FKodResourceNodeSpec Spec;
			Spec.Type = bVent ? EKodResourceType::Luminene : EKodResourceType::Jadeite;
			double Amount = bVent ? 2250.0 : 1200.0;
			Row->TryGetNumberField(TEXT("amount"), Amount);
			Spec.Amount = FMath::RoundToInt(Amount);
			Spec.HarvestPerTrip = bVent ? 4 : 5;
			Spec.DefinitionId = FName(bVent ? KodWardenPaths::Id_LumineneVent : KodWardenPaths::Id_JadeiteNode);
			const FKodEntityId Id = Sim->RegisterResourceNode(Node, Spec);
			if (Id.IsValid())
			{
				Node->SetEntityId(Id);
				Node->ApplyJadeiteStandIn();
			}
			UE_LOG(LogTemp, Log, TEXT("KodFaultline Node %s Loc=%.0f,%.0f,%.0f"), *Type, At.X, At.Y, At.Z);
		}
	}

	void SeatOctagon(UWorld* World)
	{
		const FVector Socket(-7500.f, -7500.f, 340.f);
		SpawnOctagon(World, FVector(Socket.X, Socket.Y, 300.f));
		SpawnOctagon(World, FVector(900.f, -7900.f, 0.f));
		AActor* Anchor = nullptr;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (!Actor)
			{
				continue;
			}
			const FString Label = Actor->GetActorNameOrLabel();
			if (!Label.Contains(TEXT("PROXY_CC")) && !Actor->ActorHasTag(FName(TEXT("KodStartSpot_0"))))
			{
				continue;
			}
			if (!Anchor || Actor->GetName() < Anchor->GetName())
			{
				Anchor = Actor;
			}
		}
		if (!Anchor)
		{
			UE_LOG(LogTemp, Warning, TEXT("KodFaultline CC Miss Socket=-7500,-7500"));
			return;
		}
		TInlineComponentArray<UPrimitiveComponent*> Primitives;
		Anchor->GetComponents(Primitives);
		for (UPrimitiveComponent* Primitive : Primitives)
		{
			if (Primitive)
			{
				Primitive->SetMobility(EComponentMobility::Movable);
			}
		}
		if (USceneComponent* Root = Anchor->GetRootComponent())
		{
			Root->SetMobility(EComponentMobility::Movable);
		}
		Anchor->SetActorLocation(Socket, false, nullptr, ETeleportType::TeleportPhysics);
		const FVector Seated = Anchor->GetActorLocation();
		UE_LOG(LogTemp, Log, TEXT("KodFaultline CC Octagon Loc=%.0f,%.0f,%.0f"), Seated.X, Seated.Y, Seated.Z);
	}

	struct FScnBuilding
	{
		int32 Hp = 1000;
		int32 Armor = 1;
		int32 RegenMilli = 2000;
	};

	int32 CellUU(double Cells)
	{
		return FMath::Max(0, FMath::RoundToInt(Cells * 200.0));
	}

	int32 MeleeOrRangeUU(double Cells)
	{
		if (Cells < 1.0)
		{
			return 100;
		}
		return CellUU(Cells);
	}

	void RememberUnit(UKodSimSubsystem* Sim, const TCHAR* UnitId, const TCHAR* Display, int32 Hp, int32 Armor, int32 Speed, int32 Supply, bool bInfantry)
	{
		if (UKodUnitDefinition* Def = UKodSlice0Bootstrap::ResolveUnit(FName(UnitId), Sim))
		{
			Def->MaxHealth = static_cast<float>(Hp);
			Def->Armor = static_cast<float>(Armor);
			Def->MoveSpeed = static_cast<float>(Speed);
			Def->Cost.Supply = Supply;
			Def->bBiological = true;
			Def->bInfantry = bInfantry;
		}
		FKodTrainSpec Spec;
		Spec.UnitId = FName(UnitId);
		Spec.DisplayId = FName(Display);
		Spec.bBiological = true;
		Spec.bInfantry = bInfantry;
		Spec.bOrganic = true;
		Spec.BaseArmor = Armor;
		Spec.Cost.Supply = Supply;
		Sim->RegisterTrainSpec(Spec);
	}

	void LoadBalance(UKodSimSubsystem* Sim, bool bEasy, FKodFaultlineScript& Script, TMap<FString, FScnBuilding>& Buildings)
	{
		Script.GuardFighters = bEasy ? 2 : 3;
		Script.GuardBlades = bEasy ? 0 : 1;
		Script.GuardWorkers = 3;
		Script.WaveFighters = bEasy ? 4 : 5;
		Script.WaveBlades = 1;
		Script.FighterLeashUU = 12 * 200;
		Script.FighterAggroUU = 8 * 200;
		Script.BladeLeashUU = 10 * 200;
		Script.BladeAggroUU = 6 * 200;
		Script.DelayTicks = (bEasy ? 60 : 45) * 16;
		Script.RetreatTicks = 90 * 16;
		Script.HpPermille = bEasy ? 750 : 1000;
		Script.FighterHp = 35;
		Script.FighterDamage = 5;
		Script.FighterRangeUU = 800;
		Script.FighterSpeed = 700;
		Script.FighterArmor = 0;
		Script.FighterCooldownTicks = 10;
		Script.FighterRegenMilli = 1000;
		Script.BladeHp = 110;
		Script.BladeDamage = 12;
		Script.BladeRangeUU = 100;
		Script.BladeSpeed = 826;
		Script.BladeArmor = 1;
		Script.BladeCooldownTicks = 10;
		Script.BladeRegenMilli = 1000;
		Script.WorkerHp = 40;
		Script.WorkerSpeed = 788;
		Script.WorkerRegenMilli = 1000;
		Buildings.Add(TEXT("SCN_HQ"), FScnBuilding{ 1500, 1, 2000 });
		Buildings.Add(TEXT("SCN_OUTPOST"), FScnBuilding{ 1000, 1, 2000 });
		Buildings.Add(TEXT("SCN_DEPOT"), FScnBuilding{ 400, 1, 2000 });
		Buildings.Add(TEXT("SCN_EXTRACTOR"), FScnBuilding{ 400, 1, 2000 });
		Buildings.Add(TEXT("SCN_BARRACKS"), FScnBuilding{ 1000, 1, 2000 });
		Buildings.Add(TEXT("SCN_FACTORY"), FScnBuilding{ 1250, 1, 2000 });
		Buildings.Add(TEXT("SCN_TECH"), FScnBuilding{ 1500, 1, 2000 });

		TSharedPtr<FJsonObject> Root;
		const FString Path = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("KodBalance/faultline_training_balance_v1.json"));
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Path))
		{
			UE_LOG(LogTemp, Error, TEXT("KodFaultline Balance Miss %s"), *Path);
			return;
		}
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("KodFaultline Balance Parse %s"), *Path);
			return;
		}

		const TArray<TSharedPtr<FJsonValue>>* Units = nullptr;
		if (Root->TryGetArrayField(TEXT("units"), Units) && Units)
		{
			for (const TSharedPtr<FJsonValue>& Value : *Units)
			{
				const TSharedPtr<FJsonObject> Row = Value.IsValid() ? Value->AsObject() : nullptr;
				if (!Row.IsValid())
				{
					continue;
				}
				FString Id;
				Row->TryGetStringField(TEXT("id"), Id);
				double Hp = 0.0;
				double Armor = 0.0;
				double Damage = 0.0;
				double Attacks = 1.0;
				double Cooldown = 0.61;
				double Range = 1.0;
				double Speed = 3.0;
				double Supply = 1.0;
				double Regen = 0.0;
				Row->TryGetNumberField(TEXT("hp"), Hp);
				Row->TryGetNumberField(TEXT("armor"), Armor);
				Row->TryGetNumberField(TEXT("damage"), Damage);
				Row->TryGetNumberField(TEXT("attacks_per_cycle"), Attacks);
				Row->TryGetNumberField(TEXT("cooldown_s"), Cooldown);
				Row->TryGetNumberField(TEXT("range"), Range);
				Row->TryGetNumberField(TEXT("move_speed"), Speed);
				Row->TryGetNumberField(TEXT("supply"), Supply);
				Row->TryGetNumberField(TEXT("regen_hp_s"), Regen);
				const int32 CycleDamage = FMath::Max(0, FMath::RoundToInt(Damage * FMath::Max(1.0, Attacks)));
				const int32 CooldownTicks = FMath::Max(1, FMath::RoundToInt(Cooldown * 16.0));
				if (Id == TEXT("SCN_FIGHTER"))
				{
					Script.FighterHp = FMath::Max(1, FMath::RoundToInt(Hp));
					Script.FighterDamage = CycleDamage;
					Script.FighterRangeUU = MeleeOrRangeUU(Range);
					Script.FighterSpeed = CellUU(Speed);
					Script.FighterArmor = FMath::RoundToInt(Armor);
					Script.FighterCooldownTicks = CooldownTicks;
					Script.FighterRegenMilli = FMath::Max(0, FMath::RoundToInt(Regen * 1000.0));
					RememberUnit(Sim, KodWardenPaths::Id_ScnFighter, TEXT("Fighter"), Script.FighterHp, Script.FighterArmor, Script.FighterSpeed, FMath::RoundToInt(Supply), true);
				}
				else if (Id == TEXT("SCN_TWINBLADE"))
				{
					Script.BladeHp = FMath::Max(1, FMath::RoundToInt(Hp));
					Script.BladeDamage = CycleDamage;
					Script.BladeRangeUU = MeleeOrRangeUU(Range);
					Script.BladeSpeed = CellUU(Speed);
					Script.BladeArmor = FMath::RoundToInt(Armor);
					Script.BladeCooldownTicks = CooldownTicks;
					Script.BladeRegenMilli = FMath::Max(0, FMath::RoundToInt(Regen * 1000.0));
					RememberUnit(Sim, KodWardenPaths::Id_ScnTwinblade, TEXT("Twin-blade"), Script.BladeHp, Script.BladeArmor, Script.BladeSpeed, FMath::RoundToInt(Supply), true);
				}
				else if (Id == TEXT("SCN_WORKER"))
				{
					Script.WorkerHp = FMath::Max(1, FMath::RoundToInt(Hp));
					Script.WorkerSpeed = CellUU(Speed);
					Script.WorkerRegenMilli = FMath::Max(0, FMath::RoundToInt(Regen * 1000.0));
					RememberUnit(Sim, KodWardenPaths::Id_ScnWorker, TEXT("Worker"), Script.WorkerHp, FMath::RoundToInt(Armor), Script.WorkerSpeed, FMath::RoundToInt(Supply), false);
				}
			}
		}

		const TArray<TSharedPtr<FJsonValue>>* Structures = nullptr;
		if (Root->TryGetArrayField(TEXT("structures"), Structures) && Structures)
		{
			for (const TSharedPtr<FJsonValue>& Value : *Structures)
			{
				const TSharedPtr<FJsonObject> Row = Value.IsValid() ? Value->AsObject() : nullptr;
				if (!Row.IsValid())
				{
					continue;
				}
				FString Id;
				Row->TryGetStringField(TEXT("id"), Id);
				if (!Id.StartsWith(TEXT("SCN_")))
				{
					continue;
				}
				double Hp = 1000.0;
				double Armor = 1.0;
				double Regen = 2.0;
				Row->TryGetNumberField(TEXT("hp"), Hp);
				Row->TryGetNumberField(TEXT("armor"), Armor);
				Row->TryGetNumberField(TEXT("regen_hp_s"), Regen);
				FScnBuilding Building;
				Building.Hp = FMath::Max(1, FMath::RoundToInt(Hp));
				Building.Armor = FMath::Max(0, FMath::RoundToInt(Armor));
				Building.RegenMilli = FMath::Max(0, FMath::RoundToInt(Regen * 1000.0));
				Buildings.Add(Id, Building);
			}
		}

		const TSharedPtr<FJsonObject>* Ai = nullptr;
		if (Root->TryGetObjectField(TEXT("ai"), Ai) && Ai && Ai->IsValid())
		{
			const TArray<TSharedPtr<FJsonValue>>* Defense = nullptr;
			if ((*Ai)->TryGetArrayField(TEXT("static_defense"), Defense) && Defense)
			{
				for (const TSharedPtr<FJsonValue>& Value : *Defense)
				{
					const TSharedPtr<FJsonObject> Row = Value.IsValid() ? Value->AsObject() : nullptr;
					if (!Row.IsValid())
					{
						continue;
					}
					FString UnitId;
					Row->TryGetStringField(TEXT("unit"), UnitId);
					const TSharedPtr<FJsonObject>* Count = nullptr;
					int32 EasyCount = 0;
					int32 NormalCount = 0;
					if (Row->TryGetObjectField(TEXT("count"), Count) && Count && Count->IsValid())
					{
						double EasyValue = 0.0;
						double NormalValue = 0.0;
						(*Count)->TryGetNumberField(TEXT("easy"), EasyValue);
						(*Count)->TryGetNumberField(TEXT("normal"), NormalValue);
						EasyCount = FMath::Max(0, FMath::RoundToInt(EasyValue));
						NormalCount = FMath::Max(0, FMath::RoundToInt(NormalValue));
					}
					double Leash = 0.0;
					double Aggro = 0.0;
					Row->TryGetNumberField(TEXT("leash_radius"), Leash);
					Row->TryGetNumberField(TEXT("aggro_range"), Aggro);
					const int32 UseCount = bEasy ? EasyCount : NormalCount;
					if (UnitId == TEXT("SCN_FIGHTER"))
					{
						Script.GuardFighters = UseCount;
						Script.FighterLeashUU = CellUU(Leash);
						Script.FighterAggroUU = CellUU(Aggro);
					}
					else if (UnitId == TEXT("SCN_TWINBLADE"))
					{
						Script.GuardBlades = UseCount;
						Script.BladeLeashUU = CellUU(Leash);
						Script.BladeAggroUU = CellUU(Aggro);
					}
					else if (UnitId == TEXT("SCN_WORKER"))
					{
						Script.GuardWorkers = UseCount;
					}
				}
			}
			const TSharedPtr<FJsonObject>* Wave = nullptr;
			if ((*Ai)->TryGetObjectField(TEXT("counter_wave"), Wave) && Wave && Wave->IsValid())
			{
				const TSharedPtr<FJsonObject>* Delay = nullptr;
				if ((*Wave)->TryGetObjectField(TEXT("trigger_delay_s"), Delay) && Delay && Delay->IsValid())
				{
					double EasyDelay = 60.0;
					double NormalDelay = 45.0;
					(*Delay)->TryGetNumberField(TEXT("easy"), EasyDelay);
					(*Delay)->TryGetNumberField(TEXT("normal"), NormalDelay);
					Script.DelayTicks = FMath::Max(0, FMath::RoundToInt((bEasy ? EasyDelay : NormalDelay) * 16.0));
				}
				const TCHAR* Column = bEasy ? TEXT("easy") : TEXT("normal");
				const TSharedPtr<FJsonObject>* Composition = nullptr;
				if ((*Wave)->TryGetObjectField(TEXT("composition"), Composition) && Composition && Composition->IsValid())
				{
					const TArray<TSharedPtr<FJsonValue>>* ColumnRows = nullptr;
					if ((*Composition)->TryGetArrayField(Column, ColumnRows) && ColumnRows)
					{
						for (const TSharedPtr<FJsonValue>& Value : *ColumnRows)
						{
							const TSharedPtr<FJsonObject> Row = Value.IsValid() ? Value->AsObject() : nullptr;
							if (!Row.IsValid())
							{
								continue;
							}
							FString UnitId;
							Row->TryGetStringField(TEXT("unit"), UnitId);
							double Count = 0.0;
							Row->TryGetNumberField(TEXT("count"), Count);
							if (UnitId == TEXT("SCN_FIGHTER"))
							{
								Script.WaveFighters = FMath::Max(0, FMath::RoundToInt(Count));
							}
							else if (UnitId == TEXT("SCN_TWINBLADE"))
							{
								Script.WaveBlades = FMath::Max(0, FMath::RoundToInt(Count));
							}
						}
					}
				}
				const TSharedPtr<FJsonObject>* Scalar = nullptr;
				if ((*Wave)->TryGetObjectField(TEXT("difficulty_scalar"), Scalar) && Scalar && Scalar->IsValid())
				{
					double EasyScale = 0.75;
					double NormalScale = 1.0;
					(*Scalar)->TryGetNumberField(TEXT("easy"), EasyScale);
					(*Scalar)->TryGetNumberField(TEXT("normal"), NormalScale);
					const double UseScale = bEasy ? EasyScale : NormalScale;
					Script.HpPermille = FMath::Clamp(FMath::RoundToInt(UseScale * 1000.0), 1, 1000);
				}
			}
		}
		UE_LOG(LogTemp, Log, TEXT("KodFaultline Balance Profile=%s DelayTicks=%d Wave=%d+%d Guards=%d+%d Permille=%d"),
			bEasy ? TEXT("easy") : TEXT("normal"),
			Script.DelayTicks,
			Script.WaveFighters,
			Script.WaveBlades,
			Script.GuardFighters,
			Script.GuardBlades,
			Script.HpPermille);
	}

	FString BuildingKey(const FString& FeatureId)
	{
		if (FeatureId == TEXT("ENEMY_HQ"))
		{
			return TEXT("SCN_HQ");
		}
		if (FeatureId == TEXT("ENEMY_PROD_A"))
		{
			return TEXT("SCN_BARRACKS");
		}
		if (FeatureId == TEXT("ENEMY_PROD_B"))
		{
			return TEXT("SCN_FACTORY");
		}
		if (FeatureId.Contains(TEXT("OUTPOST")) || FeatureId.Contains(TEXT("TOWER")))
		{
			return TEXT("SCN_OUTPOST");
		}
		return TEXT("SCN_BARRACKS");
	}

	bool FindMarker(UWorld* World, const TCHAR* MarkerId, FVector& OutPoint)
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (Actor && (Actor->ActorHasTag(FName(MarkerId)) || Actor->GetName().Contains(MarkerId)))
			{
				OutPoint = Actor->GetActorLocation();
				return true;
			}
		}
		TSharedPtr<FJsonObject> Root;
		if (!LoadJson(TEXT("map_features.json"), Root))
		{
			return false;
		}
		const TArray<TSharedPtr<FJsonValue>>* Features = nullptr;
		if (!Root->TryGetArrayField(TEXT("features"), Features) || !Features)
		{
			return false;
		}
		for (const TSharedPtr<FJsonValue>& Value : *Features)
		{
			const TSharedPtr<FJsonObject> Feature = Value.IsValid() ? Value->AsObject() : nullptr;
			if (!Feature.IsValid())
			{
				continue;
			}
			FString Id;
			Feature->TryGetStringField(TEXT("id"), Id);
			if (!Id.Equals(MarkerId))
			{
				continue;
			}
			const TArray<TSharedPtr<FJsonValue>>* Centre = nullptr;
			if (Feature->TryGetArrayField(TEXT("centre_uu"), Centre) && Centre && Centre->Num() >= 2)
			{
				OutPoint = FVector(
					static_cast<float>((*Centre)[0]->AsNumber()),
					static_cast<float>((*Centre)[1]->AsNumber()),
					80.f);
				return true;
			}
			const TSharedPtr<FJsonObject>* Cells = nullptr;
			Feature->TryGetObjectField(TEXT("cells"), Cells);
			const TArray<TSharedPtr<FJsonValue>>* Rect = nullptr;
			if (Cells && Cells->IsValid())
			{
				(*Cells)->TryGetArrayField(TEXT("rect_xywh"), Rect);
			}
			OutPoint = RectCenter(Rect, 80.f);
			return true;
		}
		return false;
	}

	void SpawnTaggedPoint(UWorld* World, const FVector& At, const FString& Id, const TCHAR* Tag)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* Marker = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(At), Params);
		if (!Marker)
		{
			return;
		}
		Marker->Tags.Add(FName(Tag));
		Marker->Tags.Add(FName(*Id));
		USceneComponent* MarkerRoot = NewObject<USceneComponent>(Marker, TEXT("Root"));
		Marker->SetRootComponent(MarkerRoot);
		MarkerRoot->RegisterComponent();
		Marker->SetActorLocation(At);
	}

	void ReadCounterWaveMarkers(UWorld* World, FVector& SpawnAt, bool& bSpawn, TArray<FVector>& Lane)
	{
		bSpawn = false;
		Lane.Reset();
		TSharedPtr<FJsonObject> Root;
		if (!LoadJson(TEXT("map_features.json"), Root))
		{
			return;
		}
		const TArray<TSharedPtr<FJsonValue>>* Features = nullptr;
		if (!Root->TryGetArrayField(TEXT("features"), Features) || !Features)
		{
			return;
		}
		struct FLanePoint
		{
			int32 Order = 0;
			FVector At = FVector::ZeroVector;
			FString Id;
		};
		TArray<FLanePoint> Points;
		for (const TSharedPtr<FJsonValue>& Value : *Features)
		{
			const TSharedPtr<FJsonObject> Feature = Value.IsValid() ? Value->AsObject() : nullptr;
			if (!Feature.IsValid())
			{
				continue;
			}
			FString Type;
			FString Id;
			Feature->TryGetStringField(TEXT("type"), Type);
			Feature->TryGetStringField(TEXT("id"), Id);
			const TArray<TSharedPtr<FJsonValue>>* Centre = nullptr;
			if (!Feature->TryGetArrayField(TEXT("centre_uu"), Centre) || !Centre || Centre->Num() < 2)
			{
				continue;
			}
			const FVector At(
				static_cast<float>((*Centre)[0]->AsNumber()),
				static_cast<float>((*Centre)[1]->AsNumber()),
				0.f);
			if (Type == TEXT("spawn-point") && Id == TEXT("SPAWN_CW_01"))
			{
				SpawnAt = At;
				bSpawn = true;
				SpawnTaggedPoint(World, At, Id, TEXT("KodSpawnPoint"));
			}
			else if (Type == TEXT("waypoint"))
			{
				FString LaneId;
				Feature->TryGetStringField(TEXT("lane"), LaneId);
				if (LaneId != TEXT("QRF_LANE_01"))
				{
					continue;
				}
				double Order = 0.0;
				Feature->TryGetNumberField(TEXT("order"), Order);
				FLanePoint Point;
				Point.Order = FMath::RoundToInt(Order);
				Point.At = At;
				Point.Id = Id;
				Points.Add(Point);
			}
		}
		Points.Sort([](const FLanePoint& A, const FLanePoint& B) { return A.Order < B.Order; });
		for (const FLanePoint& Point : Points)
		{
			Lane.Add(Point.At);
			SpawnTaggedPoint(World, Point.At, Point.Id, TEXT("KodWaypoint"));
		}
	}

	int32 SpawnScinder(UWorld* World, UKodSimSubsystem* Sim, FVector& WaveOrigin, const TMap<FString, FScnBuilding>& Buildings)
	{
		TSharedPtr<FJsonObject> Root;
		int32 Outposts = 0;
		if (!LoadJson(TEXT("map_features.json"), Root))
		{
			return 0;
		}
		const TArray<TSharedPtr<FJsonValue>>* Features = nullptr;
		if (!Root->TryGetArrayField(TEXT("features"), Features) || !Features)
		{
			return 0;
		}
		UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		for (const TSharedPtr<FJsonValue>& Value : *Features)
		{
			const TSharedPtr<FJsonObject> Feature = Value.IsValid() ? Value->AsObject() : nullptr;
			if (!Feature.IsValid())
			{
				continue;
			}
			FString Type;
			FString Id;
			FString Tag;
			Feature->TryGetStringField(TEXT("type"), Type);
			Feature->TryGetStringField(TEXT("id"), Id);
			Feature->TryGetStringField(TEXT("tag"), Tag);
			const TSharedPtr<FJsonObject>* Cells = nullptr;
			Feature->TryGetObjectField(TEXT("cells"), Cells);
			const TArray<TSharedPtr<FJsonValue>>* Rect = nullptr;
			if (Cells && Cells->IsValid())
			{
				(*Cells)->TryGetArrayField(TEXT("rect_xywh"), Rect);
			}
			if (Type == TEXT("objective-trigger"))
			{
				const FVector At = RectCenter(Rect, 200.f);
				FActorSpawnParameters Params;
				Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				AActor* Trigger = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(At), Params);
				if (!Trigger)
				{
					continue;
				}
				Trigger->Tags.Add(FName(TEXT("KodObjectiveTrigger")));
				Trigger->Tags.Add(FName(*Id));
				UBoxComponent* Box = NewObject<UBoxComponent>(Trigger);
				Trigger->SetRootComponent(Box);
				Box->RegisterComponent();
				int32 W = 1;
				int32 H = 1;
				if (Rect && Rect->Num() >= 4)
				{
					W = FMath::Max(1, FMath::RoundToInt((*Rect)[2]->AsNumber()));
					H = FMath::Max(1, FMath::RoundToInt((*Rect)[3]->AsNumber()));
				}
				Box->SetBoxExtent(FVector(W * 100.f, H * 100.f, 450.f));
				Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
				Box->SetCollisionResponseToAllChannels(ECR_Overlap);
				continue;
			}
			if (Type != TEXT("enemy-structure"))
			{
				continue;
			}
			const FVector At = RectCenter(Rect, 80.f);
			const bool bOutpost = Tag == TEXT("KodEnemyOutpost");
			const bool bHq = Id == TEXT("ENEMY_HQ");
			if (bOutpost && Id == TEXT("OUTPOST_SPIRE"))
			{
				WaveOrigin = At;
			}
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AKodBuilding* Building = World->SpawnActor<AKodBuilding>(AKodBuilding::StaticClass(), FTransform(At), Params);
			if (!Building)
			{
				continue;
			}
			Building->Tags.Add(FName(*Tag));
			Building->TeamId = 1;
			if (UStaticMeshComponent* Mesh = Building->FindComponentByClass<UStaticMeshComponent>())
			{
				if (Cube)
				{
					Mesh->SetStaticMesh(Cube);
				}
				const float Span = bHq ? 5.f : (bOutpost && Id == TEXT("OUTPOST_SPIRE") ? 3.f : 2.f);
				Mesh->SetWorldScale3D(FVector(Span, Span, 1.5f));
				Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			}
			const FString Key = BuildingKey(Id);
			const FScnBuilding* Row = Buildings.Find(Key);
			const int32 Hp = Row ? Row->Hp : 1000;
			const int32 Armor = Row ? Row->Armor : 1;
			const int32 RegenMilli = Row ? Row->RegenMilli : 2000;
			FKodStructureSpec Spec;
			Spec.DefinitionId = FName(*Tag);
			Spec.DisplayId = FName(*Id);
			Spec.TeamId = 1;
			Spec.MaxHealth = static_cast<float>(Hp);
			Spec.FootprintX = bHq ? 5 : (Id == TEXT("OUTPOST_SPIRE") ? 3 : 2);
			Spec.FootprintY = Spec.FootprintX;
			const FKodEntityId Built = Sim->RegisterStructure(Building, Spec);
			Building->EntityId = Built;
			if (Built.IsValid() && bOutpost)
			{
				++Outposts;
			}
			if (Built.IsValid())
			{
				const bool bGun = bOutpost || Id.Contains(TEXT("TOWER"));
				Sim->TuneStructureCombat(Built, Armor, bGun ? 12.f : 0.f, bGun ? 700.f : 0.f);
				Sim->SetStructureVitals(Built, Hp, Armor, RegenMilli);
			}
			UE_LOG(LogTemp, Log, TEXT("KodFaultline Scinder %s Balance=%s HP=%d Armor=%d RegenMilli=%d Loc=%.0f,%.0f"),
				*Id, *Key, Hp, Armor, RegenMilli, At.X, At.Y);
		}
		return Outposts;
	}

	bool MapLeafMatches(const FString& MapName, const FString& Entry)
	{
		const FString Leaf = FPaths::GetBaseFilename(Entry);
		return !Leaf.IsEmpty() && MapName.EndsWith(Leaf);
	}
}

bool KodFaultline::IsTrainingMap(const UWorld* World)
{
	if (!World)
	{
		return false;
	}
	if (const AKodWorldSettings* Settings = Cast<AKodWorldSettings>(World->GetWorldSettings()))
	{
		if (Settings->bFaultlineTraining)
		{
			return true;
		}
	}
	UWorld* MutableWorld = const_cast<UWorld*>(World);
	const AKodSlice0GameMode* Mode = Cast<AKodSlice0GameMode>(MutableWorld->GetAuthGameMode());
	if (!Mode)
	{
		return false;
	}
	const FString MapName = FPaths::GetBaseFilename(World->GetMapName());
	for (const FString& Entry : Mode->FaultlineMaps)
	{
		if (MapLeafMatches(MapName, Entry))
		{
			return true;
		}
	}
	return false;
}

void KodFaultline::Begin(UWorld* World)
{
	if (!IsTrainingMap(World))
	{
		return;
	}
	UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>();
	if (!Sim)
	{
		return;
	}
	Sim->SetMatchMode(EKodMatchMode::Campaign);
	Sim->SetAiEnabled(false);
	int32 CmdDiff = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("kod.AIDifficulty="), CmdDiff))
	{
		Sim->SetAiDifficulty(CmdDiff);
	}
	else if (Sim->GetAiDifficulty() <= 1)
	{
		Sim->SetAiDifficulty(2);
	}
	if (APlayerController* Controller = World->GetFirstPlayerController())
	{
		if (const AKodHUD* HUD = Cast<AKodHUD>(Controller->GetHUD()))
		{
			Sim->SetPlayerFaction(HUD->GetHudSkin());
		}
	}
	PlaceResources(World, Sim);
	SeatOctagon(World);
	const bool bEasy = Sim->GetAiDifficulty() <= 1;
	FKodFaultlineScript Script;
	TMap<FString, FScnBuilding> Buildings;
	LoadBalance(Sim, bEasy, Script, Buildings);
	FVector OutpostAt(1500.f, -300.f, 80.f);
	const int32 Outposts = SpawnScinder(World, Sim, OutpostAt, Buildings);
	FVector SpawnAt = OutpostAt;
	TArray<FVector> Lane;
	bool bSpawnMarker = false;
	ReadCounterWaveMarkers(World, SpawnAt, bSpawnMarker, Lane);
	if (!bSpawnMarker)
	{
		SpawnAt = OutpostAt;
		UE_LOG(LogTemp, Log, TEXT("KodFaultline Spawn marker miss SPAWN_CW_01; using the outpost"));
	}
	Script.OutpostCount = Outposts;
	Script.OutpostX = FMath::RoundToInt(OutpostAt.X);
	Script.OutpostY = FMath::RoundToInt(OutpostAt.Y);
	Script.OutpostZ = FMath::RoundToInt(OutpostAt.Z);
	Script.SpawnX = FMath::RoundToInt(SpawnAt.X);
	Script.SpawnY = FMath::RoundToInt(SpawnAt.Y);
	Script.SpawnZ = FMath::RoundToInt(SpawnAt.Z);
	Script.HomeX = -7500;
	Script.HomeY = -7500;
	Script.HomeZ = 340;
	Script.bHasLane = Lane.Num() > 0 ? 1 : 0;
	if (Lane.Num() > 0)
	{
		Script.LaneX = FMath::RoundToInt(Lane[0].X);
		Script.LaneY = FMath::RoundToInt(Lane[0].Y);
		Script.LaneZ = 0;
		for (const FVector& Point : Lane)
		{
			Script.LanePointX.Add(FMath::RoundToInt(Point.X));
			Script.LanePointY.Add(FMath::RoundToInt(Point.Y));
		}
	}
	UE_LOG(LogTemp, Log, TEXT("KodFaultline LanePoints=%d"), Lane.Num());
	Sim->ArmFaultlineScript(Script);
	UE_LOG(LogTemp, Log, TEXT("KodFaultline Begin Map=%s Outposts=%d Spawn=%.0f,%.0f Lane=%d"),
		*World->GetMapName(), Outposts, SpawnAt.X, SpawnAt.Y, Lane.Num());
}
