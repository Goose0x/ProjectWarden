#include "Slice0/KodBuildGhost.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Sim/KodBuildTypes.h"
#include "UObject/ConstructorHelpers.h"

AKodBuildGhost::AKodBuildGhost()
{
	PrimaryActorTick.bCanEverTick = false;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GhostMesh"));
	SetRootComponent(Mesh);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded())
	{
		Mesh->SetStaticMesh(CubeFinder.Object);
	}
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
}

void AKodBuildGhost::SetFootprint(int32 CellsX, int32 CellsY)
{
	if (!Mesh)
	{
		return;
	}
	const float SpanX = static_cast<float>(FMath::Max(1, CellsX)) * static_cast<float>(KodBuildGrid::CellUU);
	const float SpanY = static_cast<float>(FMath::Max(1, CellsY)) * static_cast<float>(KodBuildGrid::CellUU);
	Mesh->SetRelativeScale3D(FVector(SpanX / 100.f, SpanY / 100.f, 0.4f));
}

void AKodBuildGhost::SetValidTint(bool bValid)
{
	if (!Mesh)
	{
		return;
	}
	if (!Tint)
	{
		UMaterialInterface* Source = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		if (!Source)
		{
			Source = Mesh->GetMaterial(0);
		}
		if (!Source)
		{
			return;
		}
		Tint = UMaterialInstanceDynamic::Create(Source, this);
		Mesh->SetMaterial(0, Tint);
	}
	const FLinearColor Color = bValid
		? FLinearColor(FColor(0x3D, 0xFF, 0x6A))
		: FLinearColor(FColor(0xFF, 0x3D, 0x3D));
	Tint->SetVectorParameterValue(TEXT("Color"), Color);
	Tint->SetVectorParameterValue(TEXT("BaseColor"), Color);
}
