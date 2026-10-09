#include "Actors/KodResourceNode.h"

#include "Components/StaticMeshComponent.h"
#include "Data/KodResourceNodeDefinition.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Game/KodPlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Sim/KodSimSubsystem.h"

#if WITH_EDITOR
#include "Materials/Material.h"
#include "Materials/MaterialEditorOnlyData.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#endif

namespace
{
	constexpr const TCHAR* BasicShapeMaterialPath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");
	constexpr const TCHAR* ConeMeshPath = TEXT("/Engine/BasicShapes/Cone.Cone");
	constexpr const TCHAR* CylinderMeshPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
	const FName ClusterTag(TEXT("KodResourceCluster"));
	const FName ColorParamName(TEXT("Color"));

	const FLinearColor JadeiteTint(0.05f, 0.95f, 0.72f, 1.f);
	// #1C1E22, sRGB to linear. Dark metal with a highlight, not a flat black.
	const FLinearColor OilTint(FColor(0x1C, 0x1E, 0x22));

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

#if WITH_EDITOR
	UMaterial* BuildPlaceholderMaterial(FName Name, const FLinearColor& Tint, float Roughness, float Metallic, bool bUnlit)
	{
		UMaterial* Material = NewObject<UMaterial>(GetTransientPackage(), Name, RF_Transient);
		if (!Material)
		{
			return nullptr;
		}
		Material->bUsedWithNanite = true;
		Material->SetShadingModel(bUnlit ? MSM_Unlit : MSM_DefaultLit);
		Material->BlendMode = BLEND_Opaque;
		Material->TwoSided = false;

		UMaterialEditorOnlyData* EditorData = Material->GetEditorOnlyData();
		if (!EditorData)
		{
			return nullptr;
		}

		UMaterialExpressionVectorParameter* ColorExpr = NewObject<UMaterialExpressionVectorParameter>(Material);
		ColorExpr->ParameterName = ColorParamName;
		ColorExpr->DefaultValue = Tint;
		ColorExpr->Material = Material;
		EditorData->ExpressionCollection.AddExpression(ColorExpr);
		EditorData->BaseColor.Connect(0, ColorExpr);
		if (bUnlit)
		{
			// Unlit reads emissive. A saturated color reads as a glow without a scene light.
			EditorData->EmissiveColor.Connect(0, ColorExpr);
		}

		UMaterialExpressionScalarParameter* RoughExpr = NewObject<UMaterialExpressionScalarParameter>(Material);
		RoughExpr->ParameterName = TEXT("Roughness");
		RoughExpr->DefaultValue = Roughness;
		RoughExpr->Material = Material;
		EditorData->ExpressionCollection.AddExpression(RoughExpr);
		EditorData->Roughness.Connect(0, RoughExpr);

		UMaterialExpressionScalarParameter* MetalExpr = NewObject<UMaterialExpressionScalarParameter>(Material);
		MetalExpr->ParameterName = TEXT("Metallic");
		MetalExpr->DefaultValue = Metallic;
		MetalExpr->Material = Material;
		EditorData->ExpressionCollection.AddExpression(MetalExpr);
		EditorData->Metallic.Connect(0, MetalExpr);

		Material->PreEditChange(nullptr);
		Material->PostEditChange();
		return Material;
	}

	UMaterialInterface* GetEditorPlaceholderParent(bool bJadeite)
	{
		static TWeakObjectPtr<UMaterial> JadeiteParent;
		static TWeakObjectPtr<UMaterial> OilParent;
		TWeakObjectPtr<UMaterial>& Slot = bJadeite ? JadeiteParent : OilParent;
		if (UMaterial* Existing = Slot.Get())
		{
			return Existing;
		}
		UMaterial* Built = bJadeite
			? BuildPlaceholderMaterial(NAME_None, JadeiteTint, 0.35f, 0.05f, true)
			: BuildPlaceholderMaterial(NAME_None, OilTint, 0.22f, 0.4f, false);
		Slot = Built;
		return Built;
	}
#endif

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

	NodeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NodeMesh"));
	SetRootComponent(NodeMesh);
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
	if (UStaticMesh* Loaded = Def->Mesh.LoadSynchronous())
	{
		if (NodeMesh)
		{
			NodeMesh->SetStaticMesh(Loaded);
			NodeMesh->SetRelativeScale3D(FVector::OneVector);
			const float HalfHeight = Loaded->GetBounds().BoxExtent.Z;
			NodeMesh->SetRelativeLocation(FVector(0.f, 0.f, HalfHeight));
		}
		return;
	}
	ApplyPlaceholder(ResourceType);
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
	Mesh->SetupAttachment(NodeMesh);
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

	const bool bOil = Type == EKodResourceType::Oil;
	const TCHAR* MeshPath = bOil ? CylinderMeshPath : ConeMeshPath;
	UStaticMesh* Shape = LoadObject<UStaticMesh>(nullptr, MeshPath);
	if (!Shape)
	{
		UE_LOG(LogTemp, Warning, TEXT("KodEcon placeholder mesh missing %s"), MeshPath);
		LogNodeTint(false, NAME_None);
		return;
	}

	TArray<UStaticMeshComponent*, TInlineAllocator<4>> TintMeshes;
	if (bOil)
	{
		// Derrick base: ~250 cm across and low. Pivot is the mesh center, so lift by half the height.
		const FVector Scale = ScaleForTargetSize(Shape, 250.f, 46.f);
		NodeMesh->SetStaticMesh(Shape);
		NodeMesh->SetRelativeScale3D(Scale);
		NodeMesh->SetRelativeRotation(FRotator::ZeroRotator);
		NodeMesh->SetRelativeLocation(FVector(0.f, 0.f, 23.f));
		TintMeshes.Add(NodeMesh);
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
			TintMeshes.Add(Left);
		}
		if (UStaticMeshComponent* Right = AddClusterMesh(Shape, FVector(18.f, -16.f, 42.f), FRotator(0.f, -22.f, -6.f), RightScale))
		{
			TintMeshes.Add(Right);
		}
		TintMeshes.Add(NodeMesh);
	}

	FName UsedParam = NAME_None;
	for (UStaticMeshComponent* Mesh : TintMeshes)
	{
		const FName Param = ApplyTint(Mesh, bOil ? OilTint : JadeiteTint, !bOil);
		if (!Param.IsNone())
		{
			UsedParam = Param;
		}
	}
	LogNodeTint(!UsedParam.IsNone(), UsedParam);
}

FName AKodResourceNode::ApplyTint(UStaticMeshComponent* Mesh, const FLinearColor& Tint, bool bEmissive)
{
	if (!Mesh)
	{
		return NAME_None;
	}

	UMaterialInterface* Parent = nullptr;
	FName Param = NAME_None;

#if WITH_EDITOR
	Parent = GetEditorPlaceholderParent(bEmissive);
	if (Parent)
	{
		Param = ColorParamName;
	}
#endif

	if (!Parent)
	{
		Parent = LoadBasicShapeParent();
		Param = FindColorParam(Parent);
		if (Param.IsNone() && Mesh->GetMaterial(0))
		{
			Parent = Mesh->GetMaterial(0);
			Param = FindColorParam(Parent);
		}
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
#if WITH_EDITOR
		else if (Parent->IsA<UMaterial>())
		{
			// Color is the material default. Assign the parent if the MID readback is not ready yet.
			Mesh->SetMaterial(Slot, Parent);
			Mesh->MarkRenderStateDirty();
			bApplied = true;
		}
#endif
	}

	return bApplied ? Param : NAME_None;
}
