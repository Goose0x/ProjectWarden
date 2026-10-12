#include "Slice0/KodDressList.h"

#include "Components/DecalComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/LocalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
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
#include "Slice0/KodFaultline.h"

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

	struct FDressSuppress
	{
		FString Row;
		float X = 0.f;
		float Y = 0.f;
	};

	struct FCampTrigger
	{
		float MinX = 0.f;
		float MinY = 0.f;
		float MaxX = 0.f;
		float MaxY = 0.f;
		int32 Cap = 0;
		int32 Count = 0;
	};

	bool IsSuppressed(const TArray<FDressSuppress>& Suppress, const FString& RowId, float X, float Y)
	{
		for (const FDressSuppress& Item : Suppress)
		{
			if (Item.Row == RowId && FMath::Abs(Item.X - X) < 2.f && FMath::Abs(Item.Y - Y) < 2.f)
			{
				return true;
			}
		}
		return false;
	}

	bool AllowCampProp(TArray<FCampTrigger>& Triggers, float X, float Y)
	{
		for (FCampTrigger& Trigger : Triggers)
		{
			if (X < Trigger.MinX || X > Trigger.MaxX || Y < Trigger.MinY || Y > Trigger.MaxY)
			{
				continue;
			}
			if (Trigger.Count >= Trigger.Cap)
			{
				return false;
			}
			++Trigger.Count;
			return true;
		}
		return true;
	}

	FLinearColor DressHex(const FString& Hex, bool bVanguard)
	{
		FString Use = Hex;
		if (bVanguard && (Use.Equals(TEXT("#00FF73"), ESearchCase::IgnoreCase) || Use.Equals(TEXT("00FF73"), ESearchCase::IgnoreCase)))
		{
			Use = TEXT("33E5FF");
		}
		Use.RemoveFromStart(TEXT("#"));
		return FLinearColor::FromSRGBColor(FColor::FromHex(Use));
	}

	void LoadTriggerCaps(TArray<FCampTrigger>& Triggers)
	{
		const FString Path = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("KodDress/faultline_training/map_features.json"));
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Path))
		{
			return;
		}
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			return;
		}
		const TArray<TSharedPtr<FJsonValue>>* Features = nullptr;
		if (!Root->TryGetArrayField(TEXT("features"), Features) || !Features)
		{
			return;
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
			if (Id != TEXT("TRIG_05_OUTPOST") && Id != TEXT("TRIG_07_ENEMY_BASE"))
			{
				continue;
			}
			const TSharedPtr<FJsonObject>* Box = nullptr;
			if (!Feature->TryGetObjectField(TEXT("box_uu"), Box) || !Box || !Box->IsValid())
			{
				continue;
			}
			const TArray<TSharedPtr<FJsonValue>>* Min = nullptr;
			const TArray<TSharedPtr<FJsonValue>>* Max = nullptr;
			if (!(*Box)->TryGetArrayField(TEXT("min"), Min) || !(*Box)->TryGetArrayField(TEXT("max"), Max)
				|| !Min || !Max || Min->Num() < 2 || Max->Num() < 2)
			{
				continue;
			}
			FCampTrigger Trigger;
			Trigger.MinX = static_cast<float>((*Min)[0]->AsNumber());
			Trigger.MinY = static_cast<float>((*Min)[1]->AsNumber());
			Trigger.MaxX = static_cast<float>((*Max)[0]->AsNumber());
			Trigger.MaxY = static_cast<float>((*Max)[1]->AsNumber());
			const float Area = FMath::Max(0.f, (Trigger.MaxX - Trigger.MinX) * (Trigger.MaxY - Trigger.MinY));
			Trigger.Cap = FMath::FloorToInt(Area / 1000000.f * 0.6f);
			Triggers.Add(Trigger);
		}
	}

	void SpawnFaultlineLights(AActor* Owner, const TSharedPtr<FJsonObject>& Delta, TArray<TObjectPtr<USceneComponent>>& OutDressing)
	{
		UWorld* World = Owner->GetWorld();
		const TSharedPtr<FJsonObject>* Lighting = nullptr;
		if (!Delta.IsValid() || !World || !Delta->TryGetObjectField(TEXT("lighting"), Lighting) || !Lighting || !Lighting->IsValid())
		{
			return;
		}
		const TArray<TSharedPtr<FJsonValue>>* Lights = nullptr;
		if (!(*Lighting)->TryGetArrayField(TEXT("accent_lights"), Lights) || !Lights)
		{
			return;
		}
		int32 Spawned = 0;
		int32 Shadows = 0;
		for (const TSharedPtr<FJsonValue>& Value : *Lights)
		{
			const TSharedPtr<FJsonObject> Light = Value.IsValid() ? Value->AsObject() : nullptr;
			if (!Light.IsValid())
			{
				continue;
			}
			FString Id;
			FString Side;
			FString Type;
			FString Hex;
			Light->TryGetStringField(TEXT("id"), Id);
			Light->TryGetStringField(TEXT("side"), Side);
			Light->TryGetStringField(TEXT("type"), Type);
			Light->TryGetStringField(TEXT("color_hex"), Hex);
			double X = 0.0;
			double Y = 0.0;
			double Z = 0.0;
			double Intensity = 1.0;
			double Radius = 500.0;
			double Inner = 20.0;
			double Outer = 40.0;
			Light->TryGetNumberField(TEXT("x"), X);
			Light->TryGetNumberField(TEXT("y"), Y);
			Light->TryGetNumberField(TEXT("z"), Z);
			Light->TryGetNumberField(TEXT("intensity_cd"), Intensity);
			Light->TryGetNumberField(TEXT("attenuation_radius_uu"), Radius);
			Light->TryGetNumberField(TEXT("inner_cone_deg"), Inner);
			Light->TryGetNumberField(TEXT("outer_cone_deg"), Outer);
			bool bCast = false;
			Light->TryGetBoolField(TEXT("cast_shadows"), bCast);
			const bool bVanguard = Side.Equals(TEXT("Vanguard"), ESearchCase::IgnoreCase);
			const FLinearColor Color = DressHex(Hex, bVanguard);
			const FVector At(static_cast<float>(X), static_cast<float>(Y), static_cast<float>(Z));
			const bool bSpot = Type.Equals(TEXT("spot"), ESearchCase::IgnoreCase);
			FActorSpawnParameters Params;
			Params.Owner = Owner;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ULocalLightComponent* Component = nullptr;
			if (bSpot)
			{
				ASpotLight* Spot = World->SpawnActor<ASpotLight>(ASpotLight::StaticClass(), FTransform(FRotator(-90.f, 0.f, 0.f), At), Params);
				if (Spot)
				{
					Spot->Tags.Add(FName(TEXT("KodAccentLight")));
					Spot->Tags.Add(FName(*Id));
					if (USpotLightComponent* SpotComp = Cast<USpotLightComponent>(Spot->GetLightComponent()))
					{
						SpotComp->SetInnerConeAngle(static_cast<float>(Inner));
						SpotComp->SetOuterConeAngle(static_cast<float>(Outer));
						Component = SpotComp;
					}
				}
			}
			else
			{
				APointLight* Point = World->SpawnActor<APointLight>(APointLight::StaticClass(), FTransform(At), Params);
				if (Point)
				{
					Point->Tags.Add(FName(TEXT("KodAccentLight")));
					Point->Tags.Add(FName(*Id));
					Component = Cast<ULocalLightComponent>(Point->GetLightComponent());
				}
			}
			if (!Component)
			{
				continue;
			}
			Component->SetMobility(EComponentMobility::Movable);
			Component->SetIntensityUnits(ELightUnits::Candelas);
			Component->SetIntensity(static_cast<float>(Intensity));
			Component->SetLightColor(Color);
			Component->SetAttenuationRadius(static_cast<float>(Radius));
			const bool bShadow = bCast && Shadows < 2;
			Component->SetCastShadows(bShadow);
			if (bShadow)
			{
				++Shadows;
			}
			OutDressing.Add(Component);
			++Spawned;
		}
		UE_LOG(LogTemp, Log, TEXT("KodDress Lights=%d Shadows=%d"), Spawned, Shadows);
	}

	void SpawnFaultlineVfx(AActor* Owner, const TSharedPtr<FJsonObject>& Delta, TArray<TObjectPtr<USceneComponent>>& OutDressing)
	{
		const TArray<TSharedPtr<FJsonValue>>* Vfx = nullptr;
		if (!Delta.IsValid() || !Delta->TryGetArrayField(TEXT("vfx"), Vfx) || !Vfx)
		{
			return;
		}
		int32 Spawned = 0;
		for (const TSharedPtr<FJsonValue>& Value : *Vfx)
		{
			const TSharedPtr<FJsonObject> Row = Value.IsValid() ? Value->AsObject() : nullptr;
			if (!Row.IsValid())
			{
				continue;
			}
			FString Id;
			FString AssetPath;
			FString Role;
			Row->TryGetStringField(TEXT("id"), Id);
			Row->TryGetStringField(TEXT("asset"), AssetPath);
			Row->TryGetStringField(TEXT("role"), Role);
			const TArray<TSharedPtr<FJsonValue>>* Instances = nullptr;
			if (!Row->TryGetArrayField(TEXT("instances"), Instances) || !Instances)
			{
				continue;
			}
			const FString Object = ObjectPath(AssetPath, TEXT(""));
			UNiagaraSystem* Niagara = LoadObject<UNiagaraSystem>(nullptr, *Object);
			UParticleSystem* Cascade = Niagara ? nullptr : LoadObject<UParticleSystem>(nullptr, *Object);
			if (!Niagara && !Cascade)
			{
				continue;
			}
			const bool bDamage = Role == TEXT("damage");
			const bool bFob = Role == TEXT("fob");
			for (const TSharedPtr<FJsonValue>& InstanceValue : *Instances)
			{
				const TSharedPtr<FJsonObject> Instance = InstanceValue.IsValid() ? InstanceValue->AsObject() : nullptr;
				if (!Instance.IsValid())
				{
					continue;
				}
				double X = 0.0;
				double Y = 0.0;
				double Z = 0.0;
				Instance->TryGetNumberField(TEXT("x"), X);
				Instance->TryGetNumberField(TEXT("y"), Y);
				Instance->TryGetNumberField(TEXT("z"), Z);
				const FVector At(static_cast<float>(X), static_cast<float>(Y), static_cast<float>(Z));
				USceneComponent* Added = nullptr;
				if (Niagara)
				{
					UNiagaraComponent* Comp = NewObject<UNiagaraComponent>(Owner);
					Comp->SetAsset(Niagara);
					Comp->SetAutoActivate(!bDamage);
					Comp->SetupAttachment(Owner->GetRootComponent());
					Comp->RegisterComponent();
					Comp->SetWorldLocation(At);
					if (bDamage)
					{
						Comp->ComponentTags.Add(FName(TEXT("KodDamageSpark")));
						Comp->Deactivate();
					}
					else if (bFob)
					{
						Comp->ComponentTags.Add(FName(TEXT("KodFobSpark")));
						Comp->Activate(true);
					}
					DisableCollision(Comp);
					Added = Comp;
				}
				else if (Cascade)
				{
					UParticleSystemComponent* Comp = NewObject<UParticleSystemComponent>(Owner);
					Comp->SetTemplate(Cascade);
					Comp->SetupAttachment(Owner->GetRootComponent());
					Comp->RegisterComponent();
					Comp->SetWorldLocation(At);
					if (bDamage)
					{
						Comp->ComponentTags.Add(FName(TEXT("KodDamageSpark")));
						Comp->DeactivateSystem();
					}
					else if (bFob)
					{
						Comp->ComponentTags.Add(FName(TEXT("KodFobSpark")));
					}
					DisableCollision(Comp);
					Added = Comp;
				}
				if (Added)
				{
					OutDressing.Add(Added);
					++Spawned;
				}
			}
		}
		UE_LOG(LogTemp, Log, TEXT("KodDress Vfx=%d"), Spawned);
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
	const bool bFaultline = KodFaultline::IsTrainingMap(World);
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
	TArray<TSharedPtr<FJsonValue>> RowValues = *Rows;
	TSharedPtr<FJsonObject> Delta;
	TArray<FDressSuppress> Suppress;
	TArray<FCampTrigger> Triggers;
	if (bFaultline)
	{
		LoadTriggerCaps(Triggers);
		const FString DeltaPath = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("KodDress/faultline_training/dress_list_v2_delta.json"));
		FString DeltaText;
		if (FFileHelper::LoadFileToString(DeltaText, *DeltaPath))
		{
			const TSharedRef<TJsonReader<>> DeltaReader = TJsonReaderFactory<>::Create(DeltaText);
			if (FJsonSerializer::Deserialize(DeltaReader, Delta) && Delta.IsValid())
			{
				const TArray<TSharedPtr<FJsonValue>>* DeltaRows = nullptr;
				if (Delta->TryGetArrayField(TEXT("rows"), DeltaRows) && DeltaRows)
				{
					RowValues.Append(*DeltaRows);
				}
				const TSharedPtr<FJsonObject>* SuppressRoot = nullptr;
				if (Delta->TryGetObjectField(TEXT("v1_suppress"), SuppressRoot) && SuppressRoot && SuppressRoot->IsValid())
				{
					const TArray<TSharedPtr<FJsonValue>>* Skip = nullptr;
					if ((*SuppressRoot)->TryGetArrayField(TEXT("instances"), Skip) && Skip)
					{
						for (const TSharedPtr<FJsonValue>& SkipValue : *Skip)
						{
							const TSharedPtr<FJsonObject> SkipRow = SkipValue.IsValid() ? SkipValue->AsObject() : nullptr;
							if (!SkipRow.IsValid())
							{
								continue;
							}
							FDressSuppress Item;
							SkipRow->TryGetStringField(TEXT("row"), Item.Row);
							double SkipX = 0.0;
							double SkipY = 0.0;
							SkipRow->TryGetNumberField(TEXT("x"), SkipX);
							SkipRow->TryGetNumberField(TEXT("y"), SkipY);
							Item.X = static_cast<float>(SkipX);
							Item.Y = static_cast<float>(SkipY);
							Suppress.Add(Item);
						}
					}
				}
			}
		}
	}

	int32 Placed = 0;
	int32 BlockRows = 0;
	int32 SkippedOffCliff = 0;
	int32 SkippedSuppress = 0;
	int32 SkippedDensity = 0;
	for (const TSharedPtr<FJsonValue>& RowValue : RowValues)
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
		bool bCamp = false;
		Row->TryGetBoolField(TEXT("camp"), bCamp);
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
			if (IsSuppressed(Suppress, Id, static_cast<float>(X), static_cast<float>(Y)))
			{
				++SkippedSuppress;
				++Index;
				continue;
			}
			if (bCamp && !AllowCampProp(Triggers, static_cast<float>(X), static_cast<float>(Y)))
			{
				++SkippedDensity;
				++Index;
				continue;
			}
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
			double ZField = 0.0;
			const bool bHasZ = Instance->TryGetNumberField(TEXT("z"), ZField);
			const float PropZ = bHasZ
				? static_cast<float>(ZField)
				: ((bBlocks && Sim && Sim->IsDressBlockCell(CellX, CellY)) ? 500.f : 0.f);
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
				else if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *ObjectPath(AssetPath, TEXT(""))))
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

	if (bFaultline && Delta.IsValid())
	{
		SpawnFaultlineLights(Owner, Delta, OutDressing);
		SpawnFaultlineVfx(Owner, Delta, OutDressing);
	}
	UE_LOG(LogTemp, Log, TEXT("KodDress Placed=%d BlockRows=%d OffCliff=%d Suppress=%d Density=%d"),
		Placed, BlockRows, SkippedOffCliff, SkippedSuppress, SkippedDensity);
}
