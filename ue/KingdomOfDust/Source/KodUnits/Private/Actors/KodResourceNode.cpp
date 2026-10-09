#include "Actors/KodResourceNode.h"

#include "Components/StaticMeshComponent.h"
#include "Data/KodResourceNodeDefinition.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Game/KodPlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Sim/KodSimSubsystem.h"

namespace
{
	constexpr const TCHAR* BasicShapeMaterialPath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");
	constexpr const TCHAR* ConeMeshPath = TEXT("/Engine/BasicShapes/Cone.Cone");
	constexpr const TCHAR* CylinderMeshPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");

	const FLinearColor JadeiteTint(0.05f, 1.15f, 0.62f, 1.f);
	const FLinearColor OilTint(0.03f, 0.025f, 0.02f, 1.f);
}

AKodResourceNode::AKodResourceNode()
{
	PrimaryActorTick.bCanEverTick = false;

	NodeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NodeMesh"));
	SetRootComponent(NodeMesh);
	// Same click path as AKodUnit::UnitMesh. QueryOnly so the camera pawn is not blocked.
	// Keep the Pawn query profile when ApplyPlaceholder calls SetStaticMesh.
	NodeMesh->bUseDefaultCollision = false;
	NodeMesh->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	NodeMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
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
			SitMeshOnGround();
		}
		return;
	}
	ApplyPlaceholder(ResourceType);
}

void AKodResourceNode::ApplyPlaceholder(EKodResourceType Type)
{
	ResourceType = Type;
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
		return;
	}

	NodeMesh->SetStaticMesh(Shape);
	// Cone stands in for a crystal. Cylinder is a squat derrick / well.
	NodeMesh->SetRelativeScale3D(bOil ? FVector(1.5f, 1.5f, 0.35f) : FVector(0.55f, 0.55f, 1.7f));
	SitMeshOnGround();
	ApplyTint(bOil ? OilTint : JadeiteTint);
}

void AKodResourceNode::SitMeshOnGround()
{
	if (!NodeMesh || !NodeMesh->GetStaticMesh())
	{
		return;
	}
	const float HalfHeight = NodeMesh->GetStaticMesh()->GetBounds().BoxExtent.Z * NodeMesh->GetRelativeScale3D().Z;
	NodeMesh->SetRelativeLocation(FVector(0.f, 0.f, HalfHeight));
}

void AKodResourceNode::ApplyTint(const FLinearColor& Tint)
{
	if (!NodeMesh)
	{
		return;
	}

	UMaterialInterface* Source = LoadObject<UMaterialInterface>(nullptr, BasicShapeMaterialPath);
	if (!Source)
	{
		Source = NodeMesh->GetMaterial(0);
	}
	if (!Source)
	{
		UE_LOG(LogTemp, Warning, TEXT("KodEcon tint failed (BasicShapeMaterial missing) %s"), *GetName());
		return;
	}

	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Source, NodeMesh);
	if (!MID)
	{
		return;
	}
	// BasicShapeMaterial uses Color. EmissiveColor is set for a parent that has that pin.
	MID->SetVectorParameterValue(TEXT("Color"), Tint);
	MID->SetVectorParameterValue(TEXT("BaseColor"), Tint);
	MID->SetVectorParameterValue(TEXT("EmissiveColor"), Tint);
	NodeMesh->SetMaterial(0, MID);
}
