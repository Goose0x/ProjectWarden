#include "Actors/KodResourceNode.h"
#include "Game/KodCollision.h"
#include "Actors/KodJadeiteVisual.h"
#include "Game/KodJadeitePalette.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Data/KodResourceNodeDefinition.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Game/KodPlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sim/KodSimSubsystem.h"
#include "UObject/SoftObjectPath.h"
#include "Warden/KodWardenPaths.h"

namespace
{
	constexpr const TCHAR* BasicShapeMaterialPath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");
	constexpr const TCHAR* ConeMeshPath = TEXT("/Engine/BasicShapes/Cone.Cone");
	constexpr const TCHAR* CylinderMeshPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
	const FName ClusterTag(TEXT("KodResourceCluster"));
	const FName ColorParamName(TEXT("Color"));

	const FLinearColor LumineneTint = KodResourceColors::LumineneCore();
	const FLinearColor LumineneHighlightTint = KodResourceColors::LumineneHighlight();

	const FName CandidateColorParams[] = {
		FName(TEXT("Color")),
		FName(TEXT("BaseColor")),
		FName(TEXT("BaseColorTint")),
		FName(TEXT("Tint")),
		FName(TEXT("DiffuseColor")),
		FName(TEXT("Albedo")),
		FName(TEXT("EmissiveColor"))
	};

	bool MaterialHasVectorParam(UMaterialInterface* Material, FName Param)
	{
		if (!Material || Param.IsNone())
		{
			return false;
		}
		FLinearColor Value;
		return Material->GetVectorParameterValue(FHashedMaterialParameterInfo(Param), Value);
	}

	FName FindColorParam(UMaterialInterface* Material)
	{
		if (!Material)
		{
			return NAME_None;
		}
		for (const FName& Candidate : CandidateColorParams)
		{
			if (MaterialHasVectorParam(Material, Candidate))
			{
				return Candidate;
			}
		}
		TArray<FMaterialParameterInfo> Infos;
		TArray<FGuid> Ids;
		Material->GetAllVectorParameterInfo(Infos, Ids);
		for (const FMaterialParameterInfo& Info : Infos)
		{
			const FString Name = Info.Name.ToString();
			if (Name.Contains(TEXT("Color")) || Name.Contains(TEXT("Tint")) || Name.Contains(TEXT("Albedo")))
			{
				return Info.Name;
			}
		}
		return NAME_None;
	}

	void LogNodeTint(bool bApplied, const FName& Param)
	{
		UE_LOG(LogTemp, Log, TEXT("KodEcon NodeTint Applied=%d Param=%s"),
			bApplied ? 1 : 0,
			Param.IsNone() ? TEXT("None") : *Param.ToString());
	}

	UMaterialInterface* LoadBasicShapeParent()
	{
		return LoadObject<UMaterialInterface>(nullptr, BasicShapeMaterialPath);
	}

	FVector ScaleForTargetSize(const UStaticMesh* Shape, float TargetWidthCm, float TargetHeightCm)
	{
		const float Width = (Shape && Shape->GetBounds().BoxExtent.X > KINDA_SMALL_NUMBER)
			? Shape->GetBounds().BoxExtent.X * 2.f
			: 100.f;
		const float Height = (Shape && Shape->GetBounds().BoxExtent.Z > KINDA_SMALL_NUMBER)
			? Shape->GetBounds().BoxExtent.Z * 2.f
			: 100.f;
		return FVector(TargetWidthCm / Width, TargetWidthCm / Width, TargetHeightCm / Height);
	}

	const TCHAR* JadeiteNodeMaterialPath = TEXT("/Game/Kod/Proxy/Materials/MI_Jadeite_Node.MI_Jadeite_Node");
	const TCHAR* MonolithFallbackPath = TEXT("/Game/StylizedRocks_Etherion/RocksVol1/Meshes/Monoliths/SM_Monolith_A");

	struct FJadeiteStandInSpec
	{
		bool bLoaded = false;
		FString Mesh;
		TArray<FString> MeshPool;
		TMap<FString, FVector> PerMeshScale;
		float ScaleMin = 0.12f;
		float ScaleMax = 0.2f;
		float YawStep = 15.f;
	};

	FString ToObjectPath(const FString& AssetPath)
	{
		const FString Path = AssetPath.TrimStartAndEnd();
		if (Path.IsEmpty() || Path.Contains(TEXT(".")))
		{
			return Path;
		}
		const FString Base = FPaths::GetBaseFilename(Path);
		if (Base.IsEmpty())
		{
			return Path;
		}
		return Path + TEXT(".") + Base;
	}

	bool ReadJsonNumber(const TSharedPtr<FJsonValue>& Value, double& OutNumber)
	{
		if (!Value.IsValid() || Value->Type != EJson::Number)
		{
			return false;
		}
		OutNumber = Value->AsNumber();
		return true;
	}

	bool ReadScaleValue(const TSharedPtr<FJsonValue>& Value, FVector& OutScale)
	{
		double Uniform = 0.0;
		if (ReadJsonNumber(Value, Uniform))
		{
			OutScale = FVector(static_cast<float>(Uniform));
			return true;
		}
		if (!Value.IsValid() || Value->Type != EJson::Array)
		{
			return false;
		}
		const TArray<TSharedPtr<FJsonValue>>& Axes = Value->AsArray();
		double X = 1.0;
		double Y = 1.0;
		double Z = 1.0;
		if (Axes.Num() >= 3 && ReadJsonNumber(Axes[0], X) && ReadJsonNumber(Axes[1], Y) && ReadJsonNumber(Axes[2], Z))
		{
			OutScale = FVector(static_cast<float>(X), static_cast<float>(Y), static_cast<float>(Z));
			return true;
		}
		if (Axes.Num() == 1 && ReadJsonNumber(Axes[0], X))
		{
			OutScale = FVector(static_cast<float>(X));
			return true;
		}
		return false;
	}

	void ReadStandInObject(const TSharedPtr<FJsonObject>& StandIn, FJadeiteStandInSpec& Spec)
	{
		if (!StandIn.IsValid())
		{
			return;
		}
		FString MeshPath;
		if (StandIn->TryGetStringField(TEXT("mesh"), MeshPath) && !MeshPath.IsEmpty())
		{
			Spec.Mesh = MeshPath;
		}
		const TArray<TSharedPtr<FJsonValue>>* Pool = nullptr;
		if (StandIn->TryGetArrayField(TEXT("mesh_pool"), Pool) && Pool)
		{
			Spec.MeshPool.Reset();
			for (const TSharedPtr<FJsonValue>& Entry : *Pool)
			{
				if (!Entry.IsValid() || Entry->Type != EJson::String)
				{
					continue;
				}
				const FString Item = Entry->AsString().TrimStartAndEnd();
				if (!Item.IsEmpty())
				{
					Spec.MeshPool.Add(Item);
				}
			}
		}
		const TSharedPtr<FJsonObject>* ScaleMap = nullptr;
		if (StandIn->TryGetObjectField(TEXT("per_mesh_scale"), ScaleMap) && ScaleMap && ScaleMap->IsValid())
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*ScaleMap)->Values)
			{
				FVector Scale = FVector::OneVector;
				if (ReadScaleValue(Pair.Value, Scale))
				{
					Spec.PerMeshScale.Add(Pair.Key, Scale);
				}
			}
		}
		const TArray<TSharedPtr<FJsonValue>>* ScaleField = nullptr;
		if (StandIn->TryGetArrayField(TEXT("scale"), ScaleField) && ScaleField && ScaleField->Num() >= 2)
		{
			double MinScale = Spec.ScaleMin;
			double MaxScale = Spec.ScaleMax;
			if (ReadJsonNumber((*ScaleField)[0], MinScale) && ReadJsonNumber((*ScaleField)[1], MaxScale))
			{
				Spec.ScaleMin = static_cast<float>(MinScale);
				Spec.ScaleMax = static_cast<float>(MaxScale);
			}
		}
		else
		{
			double ScaleNumber = 0.0;
			if (StandIn->TryGetNumberField(TEXT("scale"), ScaleNumber))
			{
				Spec.ScaleMin = static_cast<float>(ScaleNumber);
				Spec.ScaleMax = Spec.ScaleMin;
			}
		}
		const TSharedPtr<FJsonObject>* Yaw = nullptr;
		if (StandIn->TryGetObjectField(TEXT("random_yaw"), Yaw) && Yaw && Yaw->IsValid())
		{
			double Step = Spec.YawStep;
			if ((*Yaw)->TryGetNumberField(TEXT("step_deg"), Step) && Step > 0.0)
			{
				Spec.YawStep = static_cast<float>(Step);
			}
		}
		Spec.bLoaded = true;
	}

	bool LoadDressRoot(const TCHAR* Relative, TSharedPtr<FJsonObject>& OutRoot)
	{
		const FString Path = FPaths::Combine(FPaths::ProjectConfigDir(), Relative);
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Path))
		{
			return false;
		}
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		return FJsonSerializer::Deserialize(Reader, OutRoot) && OutRoot.IsValid();
	}

	const FJadeiteStandInSpec& StandInFor(const UWorld* World)
	{
		const bool bFaultline = World && World->GetMapName().Contains(TEXT("Faultline"));
		static FJadeiteStandInSpec CachedFaultline;
		static FJadeiteStandInSpec CachedDefault;
		static bool bFaultlineReady = false;
		static bool bDefaultReady = false;
		FJadeiteStandInSpec& Cached = bFaultline ? CachedFaultline : CachedDefault;
		bool& bReady = bFaultline ? bFaultlineReady : bDefaultReady;
		if (bReady)
		{
			return Cached;
		}
		bReady = true;
		const TCHAR* File = bFaultline
			? TEXT("KodDress/faultline_training/dress_list.json")
			: TEXT("KodDress/dress_list.json");
		TSharedPtr<FJsonObject> Root;
		if (LoadDressRoot(File, Root))
		{
			const TSharedPtr<FJsonObject>* StandIn = nullptr;
			if (Root->TryGetObjectField(TEXT("jadeite_standin"), StandIn) && StandIn && StandIn->IsValid())
			{
				ReadStandInObject(*StandIn, Cached);
			}
		}
		if (bFaultline)
		{
			TSharedPtr<FJsonObject> Delta;
			if (LoadDressRoot(TEXT("KodDress/faultline_training/dress_list_v2_delta.json"), Delta))
			{
				const TSharedPtr<FJsonObject>* StandIn = nullptr;
				if (Delta->TryGetObjectField(TEXT("jadeite_standin"), StandIn) && StandIn && StandIn->IsValid())
				{
					ReadStandInObject(*StandIn, Cached);
				}
			}
		}
		UE_LOG(LogTemp, Log, TEXT("KodJadeite StandIn Pool=%d Fallback=%s File=%s"),
			Cached.MeshPool.Num(),
			Cached.Mesh.IsEmpty() ? TEXT("monolith") : *Cached.Mesh,
			File);
		return Cached;
	}

	bool LookupMeshScale(const FJadeiteStandInSpec& Spec, const FString& AssetPath, FVector& OutScale)
	{
		if (const FVector* Exact = Spec.PerMeshScale.Find(AssetPath))
		{
			OutScale = *Exact;
			return true;
		}
		const FString ObjectPath = ToObjectPath(AssetPath);
		if (ObjectPath != AssetPath)
		{
			if (const FVector* ExactObject = Spec.PerMeshScale.Find(ObjectPath))
			{
				OutScale = *ExactObject;
				return true;
			}
		}
		TArray<FString> Keys;
		Spec.PerMeshScale.GetKeys(Keys);
		Keys.Sort();
		const FString Base = FPaths::GetBaseFilename(AssetPath);
		for (const FString& Key : Keys)
		{
			if (FPaths::GetBaseFilename(Key) == Base)
			{
				OutScale = Spec.PerMeshScale.FindChecked(Key);
				return true;
			}
		}
		return false;
	}

	uint32 JadeiteVisualHash(int32 EntityValue, const FVector& Location)
	{
		uint32 Mixed = 0;
		if (EntityValue != 0)
		{
			Mixed = static_cast<uint32>(EntityValue);
		}
		else
		{
			const int32 X = FMath::RoundToInt(Location.X);
			const int32 Y = FMath::RoundToInt(Location.Y);
			const int32 Z = FMath::RoundToInt(Location.Z);
			Mixed = HashCombine(HashCombine(GetTypeHash(X), GetTypeHash(Y)), GetTypeHash(Z));
		}
		Mixed ^= Mixed >> 16;
		Mixed *= 0x7feb352du;
		Mixed ^= Mixed >> 15;
		Mixed *= 0x846ca68bu;
		Mixed ^= Mixed >> 16;
		return Mixed;
	}

	UMaterialInterface* LoadJadeiteNodeMaterial()
	{
		static bool bLoggedMiss = false;
		UMaterialInterface* Found = LoadObject<UMaterialInterface>(nullptr, JadeiteNodeMaterialPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Found && !bLoggedMiss)
		{
			bLoggedMiss = true;
			UE_LOG(LogTemp, Log, TEXT("KodJadeite MI_Jadeite_Node missing; node keeps the current material"));
		}
		return Found;
	}
}

AKodResourceNode::AKodResourceNode()
{
	PrimaryActorTick.bCanEverTick = false;

	// Scene root keeps the spawn transform. SetRelativeLocation on a root mesh
	// is a world move, which was sending every node to (0, 0, half-height).
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	NodeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NodeMesh"));
	NodeMesh->SetupAttachment(SceneRoot);
	// Movable so a runtime MID is not dropped the way a static lightmap mesh can ignore it.
	NodeMesh->SetMobility(EComponentMobility::Movable);
	ConfigurePlaceholderCollision(NodeMesh);
	NodeMesh->SetGenerateOverlapEvents(false);
	NodeMesh->SetCanEverAffectNavigation(false);
}

void AKodResourceNode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>())
		{
			if (Sim->IsResourceNode(EntityId))
			{
				Sim->UnregisterResourceNode(EntityId);
			}
		}
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			if (AKodPlayerController* KodPC = Cast<AKodPlayerController>(It->Get()))
			{
				KodPC->RemoveFromLocalSelection(this);
			}
		}
	}
	UnbindJadeitePalette();
	Super::EndPlay(EndPlayReason);
}

void AKodResourceNode::ApplyDefinition(const UKodResourceNodeDefinition* Def)
{
	ClearClusterMeshes();
	if (!Def)
	{
		ApplyPlaceholder(ResourceType);
		return;
	}

	ResourceType = Def->ResourceType;
	DefinitionId = Def->DefinitionId;
	TSoftObjectPtr<UStaticMesh> MeshRef = Def->Mesh;
	if (MeshRef.IsNull() && Def->ResourceType == EKodResourceType::Jadeite)
	{
		MeshRef = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(KodWardenPaths::ProtoJadeiteMesh));
	}
	if (UStaticMesh* Loaded = MeshRef.LoadSynchronous())
	{
		if (NodeMesh)
		{
			NodeMesh->SetStaticMesh(Loaded);
			const FVector Scale = Def->MeshScale.Equals(FVector::OneVector, 0.01f) ? FVector::OneVector : Def->MeshScale;
			NodeMesh->SetRelativeScale3D(Scale);
			const float HalfHeight = Loaded->GetBounds().BoxExtent.Z * Scale.Z;
			NodeMesh->SetRelativeLocation(FVector(0.f, 0.f, HalfHeight));
			if (!Def->Material.IsNull())
			{
				if (UMaterialInterface* AuthorMaterial = Def->Material.LoadSynchronous())
				{
					NodeMesh->SetMaterial(0, AuthorMaterial);
				}
			}
		}
		ApplyJadeiteStandIn();
		return;
	}
	ApplyPlaceholder(ResourceType);
	if (!Def->Material.IsNull())
	{
		if (UMaterialInterface* AuthorMaterial = Def->Material.LoadSynchronous())
		{
			TInlineComponentArray<UStaticMeshComponent*> Meshes;
			GetComponents(Meshes);
			for (UStaticMeshComponent* Mesh : Meshes)
			{
				if (Mesh)
				{
					Mesh->SetMaterial(0, AuthorMaterial);
				}
			}
		}
	}
	if (!Def->MeshScale.Equals(FVector::OneVector, 0.01f) && NodeMesh)
	{
		NodeMesh->SetRelativeScale3D(NodeMesh->GetRelativeScale3D() * Def->MeshScale);
	}
	ApplyJadeiteStandIn();
}

void AKodResourceNode::ClearClusterMeshes()
{
	TInlineComponentArray<UStaticMeshComponent*> Meshes;
	GetComponents(Meshes);
	for (UStaticMeshComponent* Mesh : Meshes)
	{
		if (Mesh && Mesh != NodeMesh && Mesh->ComponentHasTag(ClusterTag))
		{
			Mesh->DestroyComponent();
		}
	}
}

void AKodResourceNode::ConfigurePlaceholderCollision(UStaticMeshComponent* Mesh) const
{
	if (!Mesh)
	{
		return;
	}
	Mesh->bUseDefaultCollision = false;
	Mesh->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh->SetCollisionResponseToChannel(KodCollision::Select, ECR_Block);
	Mesh->SetCollisionResponseToChannel(KodCollision::Ground, ECR_Ignore);
}

UStaticMeshComponent* AKodResourceNode::AddClusterMesh(
	UStaticMesh* Shape,
	const FVector& RelativeLocation,
	const FRotator& RelativeRotation,
	const FVector& Scale)
{
	if (!NodeMesh || !Shape)
	{
		return nullptr;
	}
	UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(this);
	if (!Mesh)
	{
		return nullptr;
	}
	Mesh->ComponentTags.Add(ClusterTag);
	Mesh->SetMobility(EComponentMobility::Movable);
	// Attach to the scene root, not NodeMesh. Offsets are ground-relative.
	Mesh->SetupAttachment(SceneRoot);
	Mesh->SetStaticMesh(Shape);
	Mesh->SetRelativeLocation(RelativeLocation);
	Mesh->SetRelativeRotation(RelativeRotation);
	Mesh->SetRelativeScale3D(Scale);
	ConfigurePlaceholderCollision(Mesh);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCanEverAffectNavigation(false);
	Mesh->RegisterComponent();
	return Mesh;
}

void AKodResourceNode::ApplyPlaceholder(EKodResourceType Type)
{
	ResourceType = Type;
	ClearClusterMeshes();
	if (!NodeMesh)
	{
		return;
	}

	const bool bLuminene = Type == EKodResourceType::Luminene;
	const TCHAR* MeshPath = bLuminene ? CylinderMeshPath : ConeMeshPath;
	UStaticMesh* Shape = LoadObject<UStaticMesh>(nullptr, MeshPath);
	if (!Shape)
	{
		UE_LOG(LogTemp, Warning, TEXT("KodEcon placeholder mesh missing %s"), MeshPath);
		LogNodeTint(false, NAME_None);
		return;
	}

	TArray<UStaticMeshComponent*, TInlineAllocator<4>> CoreMeshes;
	TArray<UStaticMeshComponent*, TInlineAllocator<2>> HighlightMeshes;
	if (bLuminene)
	{
		// Fissure mouth: ~250 cm across and low. Pivot is the mesh center, so lift by half the height.
		const FVector Scale = ScaleForTargetSize(Shape, 250.f, 46.f);
		NodeMesh->SetStaticMesh(Shape);
		NodeMesh->SetRelativeScale3D(Scale);
		NodeMesh->SetRelativeRotation(FRotator::ZeroRotator);
		NodeMesh->SetRelativeLocation(FVector(0.f, 0.f, 23.f));
		CoreMeshes.Add(NodeMesh);

		if (UStaticMesh* Cone = LoadObject<UStaticMesh>(nullptr, ConeMeshPath))
		{
			const FVector PlumeScale = ScaleForTargetSize(Cone, 36.f, 110.f);
			if (UStaticMeshComponent* Plume = AddClusterMesh(Cone, FVector(0.f, 0.f, 70.f), FRotator::ZeroRotator, PlumeScale))
			{
				HighlightMeshes.Add(Plume);
			}
		}
	}
	else
	{
		// Three cones, tallest about 135 cm, so the cluster reads as an SC2 mineral chunk.
		const FVector PrimaryScale = ScaleForTargetSize(Shape, 58.f, 135.f);
		NodeMesh->SetStaticMesh(Shape);
		NodeMesh->SetRelativeScale3D(PrimaryScale);
		NodeMesh->SetRelativeRotation(FRotator(0.f, 12.f, 0.f));
		NodeMesh->SetRelativeLocation(FVector(0.f, 0.f, 67.5f));

		const FVector LeftScale = ScaleForTargetSize(Shape, 40.f, 96.f);
		const FVector RightScale = ScaleForTargetSize(Shape, 36.f, 84.f);
		if (UStaticMeshComponent* Left = AddClusterMesh(Shape, FVector(-22.f, 14.f, 48.f), FRotator(0.f, 28.f, 8.f), LeftScale))
		{
			CoreMeshes.Add(Left);
		}
		if (UStaticMeshComponent* Right = AddClusterMesh(Shape, FVector(18.f, -16.f, 42.f), FRotator(0.f, -22.f, -6.f), RightScale))
		{
			CoreMeshes.Add(Right);
		}
		CoreMeshes.Add(NodeMesh);
	}

	FName UsedParam = NAME_None;
	const FLinearColor CoreTint = bLuminene ? LumineneTint : KodJadeiteVisual::TunedJadeBase(GetWorld());
	for (UStaticMeshComponent* Mesh : CoreMeshes)
	{
		if (!bLuminene && KodJadeiteVisual::UsesJadeiteNodeMaterial(Mesh))
		{
			continue;
		}
		const FName Param = ApplyTint(Mesh, CoreTint, true);
		if (!Param.IsNone())
		{
			UsedParam = Param;
		}
	}
	for (UStaticMeshComponent* Mesh : HighlightMeshes)
	{
		const FName Param = ApplyTint(Mesh, LumineneHighlightTint, true);
		if (!Param.IsNone())
		{
			UsedParam = Param;
		}
	}
	LogNodeTint(!UsedParam.IsNone(), UsedParam);
	if (!bLuminene)
	{
		BindJadeitePalette();
		RefreshJadeitePresentation();
	}
	ApplyJadeiteStandIn();
}

void AKodResourceNode::ApplyJadeiteStandIn()
{
	if (ResourceType != EKodResourceType::Jadeite || !NodeMesh)
	{
		return;
	}
	const FJadeiteStandInSpec& Spec = StandInFor(GetWorld());
	const FVector Location = GetActorLocation();
	const int32 Salt = EntityId.IsValid() ? EntityId.Value : FMath::RoundToInt(Location.X);
	const uint32 Pick = JadeiteVisualHash(EntityId.IsValid() ? EntityId.Value : 0, Location);

	FString ChosenPath;
	UStaticMesh* ChosenMesh = nullptr;
	bool bFromPool = false;
	const int32 PoolCount = Spec.MeshPool.Num();
	if (PoolCount > 0)
	{
		const int32 Start = static_cast<int32>(Pick % static_cast<uint32>(PoolCount));
		for (int32 Step = 0; Step < PoolCount; ++Step)
		{
			const FString& Candidate = Spec.MeshPool[(Start + Step) % PoolCount];
			UStaticMesh* Loaded = LoadObject<UStaticMesh>(nullptr, *ToObjectPath(Candidate), nullptr, LOAD_NoWarn | LOAD_Quiet);
			if (!Loaded)
			{
				continue;
			}
			ChosenMesh = Loaded;
			ChosenPath = Candidate;
			bFromPool = true;
			break;
		}
	}
	if (!ChosenMesh)
	{
		ChosenPath = Spec.Mesh.IsEmpty() ? FString(MonolithFallbackPath) : Spec.Mesh;
		ChosenMesh = LoadObject<UStaticMesh>(nullptr, *ToObjectPath(ChosenPath), nullptr, LOAD_NoWarn | LOAD_Quiet);
		bFromPool = false;
	}
	if (!ChosenMesh)
	{
		return;
	}

	FVector ChosenScale = FVector::OneVector;
	if (!LookupMeshScale(Spec, ChosenPath, ChosenScale))
	{
		if (bFromPool)
		{
			ChosenScale = FVector::OneVector;
		}
		else
		{
			const float Alpha = static_cast<float>(Salt % 9) / 8.f;
			const float Uniform = FMath::Lerp(Spec.ScaleMin, Spec.ScaleMax, Alpha);
			ChosenScale = FVector(Uniform);
		}
	}

	ClearClusterMeshes();
	NodeMesh->SetStaticMesh(ChosenMesh);
	const int32 YawStep = FMath::Max(1, FMath::RoundToInt(Spec.YawStep));
	int32 Yaw = (Salt * YawStep) % 360;
	if (Yaw < 0)
	{
		Yaw += 360;
	}
	NodeMesh->SetRelativeScale3D(ChosenScale);
	NodeMesh->SetRelativeRotation(FRotator(0.f, static_cast<float>(Yaw), 0.f));
	NodeMesh->SetRelativeLocation(FVector(0.f, 0.f, ChosenMesh->GetBounds().BoxExtent.Z * ChosenScale.Z));
	NodeMesh->SetVisibility(true);
	NodeMesh->SetHiddenInGame(false);
	if (UMaterialInterface* JadeNode = LoadJadeiteNodeMaterial())
	{
		const int32 Slots = FMath::Max(1, NodeMesh->GetNumMaterials());
		for (int32 Slot = 0; Slot < Slots; ++Slot)
		{
			NodeMesh->SetMaterial(Slot, JadeNode);
		}
	}
	BindJadeitePalette();
	RefreshJadeitePresentation();
	UE_LOG(LogTemp, Log, TEXT("KodJadeite StandIn Mesh=%s Scale=%.3f,%.3f,%.3f Pool=%d"),
		*ChosenMesh->GetName(),
		ChosenScale.X,
		ChosenScale.Y,
		ChosenScale.Z,
		bFromPool ? 1 : 0);
}

void AKodResourceNode::BindJadeitePalette()
{
	if (JadeitePaletteHandle.IsValid())
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		if (UKodJadeitePaletteSubsystem* Palette = World->GetSubsystem<UKodJadeitePaletteSubsystem>())
		{
			JadeitePaletteHandle = Palette->OnPaletteChanged.AddUObject(this, &AKodResourceNode::RefreshJadeitePresentation);
		}
	}
}

void AKodResourceNode::UnbindJadeitePalette()
{
	if (!JadeitePaletteHandle.IsValid())
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		if (UKodJadeitePaletteSubsystem* Palette = World->GetSubsystem<UKodJadeitePaletteSubsystem>())
		{
			Palette->OnPaletteChanged.Remove(JadeitePaletteHandle);
		}
	}
	JadeitePaletteHandle.Reset();
}

void AKodResourceNode::RefreshJadeitePresentation()
{
	if (ResourceType != EKodResourceType::Jadeite)
	{
		return;
	}
	TArray<UStaticMeshComponent*> Meshes;
	GetComponents<UStaticMeshComponent>(Meshes);
	for (UStaticMeshComponent* Mesh : Meshes)
	{
		KodJadeiteVisual::ApplyJadeiteNode(Mesh);
	}
}

FName AKodResourceNode::ApplyTint(UStaticMeshComponent* Mesh, const FLinearColor& Tint, bool bEmissive)
{
	if (!Mesh)
	{
		return NAME_None;
	}

	UMaterialInterface* Parent = LoadBasicShapeParent();
	FName Param = FindColorParam(Parent);
	if (Param.IsNone() && Mesh->GetMaterial(0))
	{
		Parent = Mesh->GetMaterial(0);
		Param = FindColorParam(Parent);
	}

	if (!Parent || Param.IsNone())
	{
		return NAME_None;
	}

	const int32 SlotCount = FMath::Max(1, Mesh->GetNumMaterials());
	bool bApplied = false;
	for (int32 Slot = 0; Slot < SlotCount; ++Slot)
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Parent, Mesh);
		if (!MID)
		{
			continue;
		}
		MID->SetVectorParameterValue(Param, Tint);
		if (bEmissive && Param != FName(TEXT("EmissiveColor")) && MaterialHasVectorParam(MID, FName(TEXT("EmissiveColor"))))
		{
			MID->SetVectorParameterValue(TEXT("EmissiveColor"), Tint * 1.6f);
		}
		if (MaterialHasVectorParam(MID, FName(TEXT("Roughness"))))
		{
			MID->SetScalarParameterValue(TEXT("Roughness"), bEmissive ? 0.35f : 0.22f);
		}
		FLinearColor Readback = Tint;
		const bool bReadable = MID->GetVectorParameterValue(FHashedMaterialParameterInfo(Param), Readback);
		if (bReadable)
		{
			Mesh->SetMaterial(Slot, MID);
			Mesh->MarkRenderStateDirty();
			bApplied = true;
		}
	}

	return bApplied ? Param : FName();
}
