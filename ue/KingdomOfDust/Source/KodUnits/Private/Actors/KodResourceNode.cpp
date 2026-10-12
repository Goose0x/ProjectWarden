#include "Actors/KodResourceNode.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Data/KodResourceNodeDefinition.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Game/KodPlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
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

	const FLinearColor JadeiteTint = KodResourceColors::Jadeite();
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
	const FLinearColor CoreTint = bLuminene ? LumineneTint : JadeiteTint;
	for (UStaticMeshComponent* Mesh : CoreMeshes)
	{
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
	ApplyJadeiteStandIn();
}

void AKodResourceNode::ApplyJadeiteStandIn()
{
	if (ResourceType != EKodResourceType::Jadeite || !NodeMesh)
	{
		return;
	}
	UStaticMesh* Monolith = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/StylizedRocks_Etherion/RocksVol1/Meshes/Monoliths/SM_Monolith_A.SM_Monolith_A"));
	if (!Monolith)
	{
		return;
	}
	ClearClusterMeshes();
	NodeMesh->SetStaticMesh(Monolith);
	const int32 Salt = EntityId.IsValid() ? EntityId.Value : FMath::RoundToInt(GetActorLocation().X);
	const float Alpha = static_cast<float>(Salt % 9) / 8.f;
	const float Scale = FMath::Lerp(0.12f, 0.2f, Alpha);
	const float Yaw = static_cast<float>((Salt * 15) % 360);
	NodeMesh->SetRelativeScale3D(FVector(Scale));
	NodeMesh->SetRelativeRotation(FRotator(0.f, Yaw, 0.f));
	NodeMesh->SetRelativeLocation(FVector(0.f, 0.f, Monolith->GetBounds().BoxExtent.Z * Scale));
	UMaterialInterface* Parent = Monolith->GetMaterial(0);
	if (!Parent)
	{
		Parent = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/StylizedRocks_Etherion/RocksVol1/Meshes/Monoliths/MI_T_Monolith_Obsidian.MI_T_Monolith_Obsidian"));
	}
	if (!Parent)
	{
		return;
	}
	UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Parent, this);
	const FLinearColor Tint(FColor(0x00, 0xFF, 0x73));
	Mid->SetVectorParameterValue(TEXT("Color"), Tint);
	Mid->SetVectorParameterValue(TEXT("BaseColor"), Tint);
	Mid->SetVectorParameterValue(TEXT("Tint"), Tint);
	NodeMesh->SetMaterial(0, Mid);
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
