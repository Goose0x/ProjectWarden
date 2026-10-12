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
	TArray<uint8> Open;
	Open.Init(1, FMath::Max(1, CellsX) * FMath::Max(1, CellsY));
	SetCells(CellsX, CellsY, Open);
}

void AKodBuildGhost::SetCells(int32 CellsX, int32 CellsY, const TArray<uint8>& CellOk)
{
	if (!Mesh)
	{
		return;
	}
	Mesh->SetVisibility(false);
	const int32 SizeX = FMath::Max(1, CellsX);
	const int32 SizeY = FMath::Max(1, CellsY);
	const int32 Count = SizeX * SizeY;
	UStaticMesh* Cube = Mesh->GetStaticMesh();
	UMaterialInterface* Source = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	while (CellMeshes.Num() < Count)
	{
		UStaticMeshComponent* Cell = NewObject<UStaticMeshComponent>(this);
		Cell->SetupAttachment(Mesh);
		Cell->SetStaticMesh(Cube);
		Cell->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Cell->SetCastShadow(false);
		Cell->RegisterComponent();
		CellMeshes.Add(Cell);
		CellTints.Add(nullptr);
	}
	for (int32 Index = 0; Index < CellMeshes.Num(); ++Index)
	{
		UStaticMeshComponent* Cell = CellMeshes[Index];
		if (!Cell)
		{
			continue;
		}
		if (Index >= Count)
		{
			Cell->SetVisibility(false);
			continue;
		}
		const int32 X = Index % SizeX;
		const int32 Y = Index / SizeX;
		const float LocalX = (static_cast<float>(X) + 0.5f - static_cast<float>(SizeX) * 0.5f) * static_cast<float>(KodBuildGrid::CellUU);
		const float LocalY = (static_cast<float>(Y) + 0.5f - static_cast<float>(SizeY) * 0.5f) * static_cast<float>(KodBuildGrid::CellUU);
		Cell->SetVisibility(true);
		Cell->SetRelativeLocation(FVector(LocalX, LocalY, 20.f));
		Cell->SetRelativeScale3D(FVector(1.8f, 1.8f, 0.15f));
		if (!CellTints[Index] && Source)
		{
			CellTints[Index] = UMaterialInstanceDynamic::Create(Source, this);
			Cell->SetMaterial(0, CellTints[Index]);
		}
		const bool bOpen = !CellOk.IsValidIndex(Index) || CellOk[Index] != 0;
		const FLinearColor Color = bOpen
			? FLinearColor(FColor(0x3D, 0xFF, 0x6A))
			: FLinearColor(FColor(0xFF, 0x3D, 0x3D));
		if (CellTints[Index])
		{
			CellTints[Index]->SetVectorParameterValue(TEXT("Color"), Color);
			CellTints[Index]->SetVectorParameterValue(TEXT("BaseColor"), Color);
		}
	}
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
