#include "Slice0/KodDressList.h"

#include "Components/DecalComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sim/KodBuildTypes.h"
#include "Sim/KodSimSubsystem.h"

namespace
{
	FString ObjectPath(const FString& AssetPath, const TCHAR* Suffix)
	{
		const FString Base = FPaths::GetBaseFilename(AssetPath);
		return AssetPath + TEXT(".") + Base + Suffix;
	}

	bool IsCloth(const FString& AssetPath)
	{
		return AssetPath.Contains(TEXT("ClothSim")) || AssetPath.Contains(TEXT("BP_Banner")) || AssetPath.Contains(TEXT("BannerACloth"));
	}

	uint32 DressHash(const FString& RowId, int32 Index)
	{
		return HashCombine(HashCombine(24024u, FCrc::StrCrc32(*RowId)), GetTypeHash(Index));
	}

	void DisableCollision(UPrimitiveComponent* Primitive)
	{
		if (!Primitive)
		{
			return;
		}
		Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Primitive->SetGenerateOverlapEvents(false);
		Primitive->SetCanEverAffectNavigation(false);
	}
}

void KodDress::Apply(AActor* Owner, TArray<TObjectPtr<USceneComponent>>& OutDressing)
{
	if (!Owner || !Owner->GetWorld())
	{
		return;
	}
	UWorld* World = Owner->GetWorld();
	UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>();
	const bool bFaultline = World->GetMapName().Contains(TEXT("Faultline"));
	const TCHAR* Folder = bFaultline ? TEXT("KodDress/faultline_training") : TEXT("KodDress");
	const FString Path = FPaths::Combine(FPaths::ProjectConfigDir(), Folder, TEXT("dress_list.json"));
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		UE_LOG(LogTemp, Warning, TEXT("KodDress File Miss %s"), *Path);
		return;
	}
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("KodDress Parse Fail"));
		return;
	}
	const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
	if (!Root->TryGetArrayField(TEXT("rows"), Rows) || !Rows)
	{
		return;
	}

	int32 Placed = 0;
	int32 BlockRows = 0;
	int32 SkippedOffCliff = 0;
	for (const TSharedPtr<FJsonValue>& RowValue : *Rows)
	{
		const TSharedPtr<FJsonObject> Row = RowValue.IsValid() ? RowValue->AsObject() : nullptr;
		if (!Row.IsValid())
		{
			continue;
		}
		FString Id;
		FString Kind;
		Row->TryGetStringField(TEXT("id"), Id);
		Row->TryGetStringField(TEXT("kind"), Kind);
		if (Id == TEXT("D18_DESTRUCTIBLE_ROCKS"))
		{
			continue;
		}
		bool bBlocks = false;
		Row->TryGetBoolField(TEXT("blocks"), bBlocks);
		if (bBlocks)
		{
			++BlockRows;
		}
		const TArray<TSharedPtr<FJsonValue>>* Assets = nullptr;
		Row->TryGetArrayField(TEXT("assets"), Assets);
		const TArray<TSharedPtr<FJsonValue>>* Instances = nullptr;
		Row->TryGetArrayField(TEXT("instances"), Instances);
		float ScaleMin = 1.f;
		float ScaleMax = 1.f;
		const TArray<TSharedPtr<FJsonValue>>* ScaleField = nullptr;
		if (Row->TryGetArrayField(TEXT("scale"), ScaleField) && ScaleField && ScaleField->Num() >= 2)
		{
			ScaleMin = static_cast<float>((*ScaleField)[0]->AsNumber());
			ScaleMax = static_cast<float>((*ScaleField)[1]->AsNumber());
		}
		bool bRandomYaw = false;
		int32 YawStep = 15;
		int32 YawA = 0;
		int32 YawB = 0;
		bool bFixedPair = false;
		const TSharedPtr<FJsonObject>* YawField = nullptr;
		if (Row->TryGetObjectField(TEXT("random_yaw"), YawField) && YawField && YawField->IsValid())
		{
			(*YawField)->TryGetBoolField(TEXT("random"), bRandomYaw);
			double Step = 15.0;
			if ((*YawField)->TryGetNumberField(TEXT("step_deg"), Step))
			{
				YawStep = FMath::Max(1, FMath::RoundToInt(Step));
			}
			const TArray<TSharedPtr<FJsonValue>>* YawDeg = nullptr;
			if ((*YawField)->TryGetArrayField(TEXT("yaw_deg"), YawDeg) && YawDeg && YawDeg->Num() >= 2)
			{
				bFixedPair = true;
				YawA = FMath::RoundToInt((*YawDeg)[0]->AsNumber());
				YawB = FMath::RoundToInt((*YawDeg)[1]->AsNumber());
			}
		}

		if (Kind.Contains(TEXT("material")) && !Kind.Contains(TEXT("decal")))
		{
			const FString AssetPath = (Assets && Assets->Num() > 0) ? (*Assets)[0]->AsString() : FString();
			UMaterialInterface* Grid = AssetPath.IsEmpty() ? nullptr : LoadObject<UMaterialInterface>(nullptr, *ObjectPath(AssetPath, TEXT("")));
			if (!Grid)
			{
				Grid = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/FancyDev/Materials/MI_fancy_grid.MI_fancy_grid"));
			}
			if (Grid)
			{
				for (TActorIterator<AActor> It(World); It; ++It)
				{
					AActor* Actor = *It;
					if (!Actor || !Actor->ActorHasTag(FName(TEXT("KodGround"))))
					{
						continue;
					}
					TInlineComponentArray<UStaticMeshComponent*> Meshes;
					Actor->GetComponents(Meshes);
					for (UStaticMeshComponent* Mesh : Meshes)
					{
						if (Mesh)
						{
							Mesh->SetMaterial(0, Grid);
						}
					}
				}
			}
			continue;
		}

		if (!Instances || !Assets)
		{
			continue;
		}
		const bool bHism = Kind.Contains(TEXT("HISM"));
		const bool bBlueprint = Kind.Contains(TEXT("blueprint"));
		const bool bDecal = Kind.Contains(TEXT("decal"));
		const bool bNiagara = Kind.Contains(TEXT("niagara"));
		const bool bParticle = Kind.Contains(TEXT("particle")) || Kind.Contains(TEXT("Cascade"));
		TMap<FString, UHierarchicalInstancedStaticMeshComponent*> Hisms;
		int32 Index = 0;
		for (const TSharedPtr<FJsonValue>& InstanceValue : *Instances)
		{
			const TSharedPtr<FJsonObject> Instance = InstanceValue.IsValid() ? InstanceValue->AsObject() : nullptr;
			if (!Instance.IsValid())
			{
				continue;
			}
			double X = 0.0;
			double Y = 0.0;
			Instance->TryGetNumberField(TEXT("x"), X);
			Instance->TryGetNumberField(TEXT("y"), Y);
			const int32 CellX = KodBuildGrid::CellOf(static_cast<float>(X));
			const int32 CellY = KodBuildGrid::CellOf(static_cast<float>(Y));
			if (bBlocks && Sim && !Sim->IsCellBlocked(CellX, CellY))
			{
				++SkippedOffCliff;
				++Index;
				continue;
			}
			const uint32 Hash = DressHash(Id, Index);
			const int32 AssetIndex = Assets->Num() > 0 ? static_cast<int32>(Hash % static_cast<uint32>(Assets->Num())) : 0;
			FString AssetPath = (*Assets)[AssetIndex]->AsString();
			if (IsCloth(AssetPath))
			{
				++Index;
				continue;
			}
			const float Alpha = static_cast<float>(Hash % 1000) / 999.f;
			const float Scale = FMath::Lerp(ScaleMin, ScaleMax, Alpha);
			float Yaw = 0.f;
			if (bFixedPair)
			{
				Yaw = (Index % 2 == 0) ? static_cast<float>(YawA) : static_cast<float>(YawB);
			}
			else if (bRandomYaw)
			{
				Yaw = static_cast<float>((Hash / YawStep) * YawStep % 360);
			}
			const float PropZ = (bBlocks && Sim && Sim->IsDressBlockCell(CellX, CellY)) ? 500.f : 0.f;
			const FVector Point(static_cast<float>(X), static_cast<float>(Y), PropZ);
			const FRotator Rotation(0.f, Yaw, 0.f);
			USceneComponent* Added = nullptr;
			if (bBlueprint)
			{
				UClass* Class = LoadObject<UClass>(nullptr, *ObjectPath(AssetPath, TEXT("_C")));
				if (Class)
				{
					FActorSpawnParameters Params;
					Params.Owner = Owner;
					Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
					AActor* Prop = World->SpawnActor<AActor>(Class, FTransform(Rotation, Point), Params);
					if (Prop)
					{
						Prop->SetActorScale3D(FVector(Scale));
						TInlineComponentArray<UPrimitiveComponent*> Primitives;
						Prop->GetComponents(Primitives);
						for (UPrimitiveComponent* Primitive : Primitives)
						{
							DisableCollision(Primitive);
						}
						++Placed;
					}
				}
			}
			else if (bDecal)
			{
				UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *ObjectPath(AssetPath, TEXT("")));
				if (Material)
				{
					UDecalComponent* Decal = NewObject<UDecalComponent>(Owner);
					Decal->SetDecalMaterial(Material);
					Decal->DecalSize = FVector(40.f, 180.f * Scale, 180.f * Scale);
					Decal->SetupAttachment(Owner->GetRootComponent());
					Decal->RegisterComponent();
					Decal->SetWorldLocationAndRotation(Point + FVector(0.f, 0.f, 20.f), FRotator(-90.f, Yaw, 0.f));
					Added = Decal;
				}
			}
			else if (bNiagara)
			{
				UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *ObjectPath(AssetPath, TEXT("")));
				if (System)
				{
					UNiagaraComponent* Niagara = NewObject<UNiagaraComponent>(Owner);
					Niagara->SetAsset(System);
					Niagara->SetAutoActivate(true);
					Niagara->SetupAttachment(Owner->GetRootComponent());
					Niagara->RegisterComponent();
					Niagara->SetWorldLocation(Point);
					DisableCollision(Niagara);
					Added = Niagara;
				}
			}
			else if (bParticle)
			{
				UParticleSystem* System = LoadObject<UParticleSystem>(nullptr, *ObjectPath(AssetPath, TEXT("")));
				if (System)
				{
					UParticleSystemComponent* Particle = NewObject<UParticleSystemComponent>(Owner);
					Particle->SetTemplate(System);
					Particle->SetupAttachment(Owner->GetRootComponent());
					Particle->RegisterComponent();
					Particle->SetWorldLocation(Point);
					DisableCollision(Particle);
					Added = Particle;
				}
			}
			else if (bHism)
			{
				UHierarchicalInstancedStaticMeshComponent*& Hism = Hisms.FindOrAdd(AssetPath);
				if (!Hism)
				{
					UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *ObjectPath(AssetPath, TEXT("")));
					if (Mesh)
					{
						Hism = NewObject<UHierarchicalInstancedStaticMeshComponent>(Owner);
						Hism->SetMobility(EComponentMobility::Movable);
						Hism->SetStaticMesh(Mesh);
						DisableCollision(Hism);
						Hism->SetupAttachment(Owner->GetRootComponent());
						Hism->RegisterComponent();
						OutDressing.Add(Hism);
					}
				}
				if (Hism)
				{
					Hism->AddInstance(FTransform(Rotation, Point, FVector(Scale)));
					++Placed;
				}
			}
			else
			{
				UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *ObjectPath(AssetPath, TEXT("")));
				if (Mesh)
				{
					UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(Owner);
					Comp->SetMobility(EComponentMobility::Movable);
					Comp->SetStaticMesh(Mesh);
					DisableCollision(Comp);
					Comp->SetupAttachment(Owner->GetRootComponent());
					Comp->RegisterComponent();
					Comp->SetWorldLocationAndRotation(Point, Rotation);
					Comp->SetWorldScale3D(FVector(Scale));
					Added = Comp;
				}
			}
			if (Added)
			{
				OutDressing.Add(Added);
				++Placed;
			}
			++Index;
		}
	}

	// Rock meshes belong to the destructible actor, not the dress scatter.
	TSharedPtr<FJsonObject> D18;
	for (const TSharedPtr<FJsonValue>& RowValue : *Rows)
	{
		const TSharedPtr<FJsonObject> Row = RowValue.IsValid() ? RowValue->AsObject() : nullptr;
		FString Id;
		if (Row.IsValid() && Row->TryGetStringField(TEXT("id"), Id) && Id == TEXT("D18_DESTRUCTIBLE_ROCKS"))
		{
			D18 = Row;
			break;
		}
	}
	if (D18.IsValid())
	{
		const TArray<TSharedPtr<FJsonValue>>* Assets = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Instances = nullptr;
		D18->TryGetArrayField(TEXT("assets"), Assets);
		D18->TryGetArrayField(TEXT("instances"), Instances);
		if (Assets && Instances)
		{
			int32 RockIndex = 0;
			for (const TSharedPtr<FJsonValue>& InstanceValue : *Instances)
			{
				const TSharedPtr<FJsonObject> Instance = InstanceValue.IsValid() ? InstanceValue->AsObject() : nullptr;
				if (!Instance.IsValid())
				{
					continue;
				}
				double X = 0.0;
				double Y = 0.0;
				Instance->TryGetNumberField(TEXT("x"), X);
				Instance->TryGetNumberField(TEXT("y"), Y);
				AActor* Host = nullptr;
				float Best = MAX_flt;
				for (TActorIterator<AActor> It(World); It; ++It)
				{
					AActor* Actor = *It;
					if (!Actor || !Actor->ActorHasTag(FName(TEXT("KodDestructible"))))
					{
						continue;
					}
					const float Dist = FVector::DistSquared2D(Actor->GetActorLocation(), FVector(static_cast<float>(X), static_cast<float>(Y), 0.f));
					if (Dist < Best)
					{
						Best = Dist;
						Host = Actor;
					}
				}
				const int32 AssetIndex = static_cast<int32>(DressHash(TEXT("D18"), RockIndex) % static_cast<uint32>(Assets->Num()));
				UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *ObjectPath((*Assets)[AssetIndex]->AsString(), TEXT("")));
				if (Host && Mesh)
				{
					UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(Host);
					Comp->SetMobility(EComponentMobility::Movable);
					Comp->SetStaticMesh(Mesh);
					DisableCollision(Comp);
					if (Host->GetRootComponent())
					{
						Comp->SetupAttachment(Host->GetRootComponent());
					}
					Comp->RegisterComponent();
					Comp->SetWorldLocation(FVector(static_cast<float>(X), static_cast<float>(Y), 0.f));
					++Placed;
				}
				++RockIndex;
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("KodDress Placed=%d BlockRows=%d OffCliff=%d"), Placed, BlockRows, SkippedOffCliff);
}
