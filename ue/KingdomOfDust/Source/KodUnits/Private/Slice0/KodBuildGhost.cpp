#include "Slice0/KodBuildGhost.h"

#include "Components/MeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Data/KodMeshCatalog.h"
#include "Engine/EngineTypes.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Sim/KodBuildTypes.h"
#include "UObject/ConstructorHelpers.h"

AKodBuildGhost::AKodBuildGhost()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("GhostRoot"));
	SetRootComponent(SceneRoot);
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GhostMesh"));
	Mesh->SetupAttachment(SceneRoot);
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
	const int32 SizeX = FMath::Max(1, CellsX);
	const int32 SizeY = FMath::Max(1, CellsY);
	const int32 Count = SizeX * SizeY;
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UMaterialInterface* Source = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	while (CellMeshes.Num() < Count)
	{
		UStaticMeshComponent* Cell = NewObject<UStaticMeshComponent>(this);
		Cell->SetupAttachment(SceneRoot);
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
		const float LocalX = KodBuildGrid::CellLocalOffset(X, SizeX);
		const float LocalY = KodBuildGrid::CellLocalOffset(Y, SizeY);
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
			? FLinearColor(FColor(0x3D, 0xFF, 0x6A, static_cast<uint8>(0.27f * 255.f)))
			: FLinearColor(FColor(0xFF, 0x3D, 0x3D, static_cast<uint8>(0.27f * 255.f)));
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
		? FLinearColor(0.15f, 1.f, 0.35f, 0.45f)
		: FLinearColor(1.f, 0.2f, 0.15f, 0.45f);
	Tint->SetVectorParameterValue(TEXT("Color"), Color);
	Tint->SetVectorParameterValue(TEXT("BaseColor"), Color);
	auto Paint = [Color](UMeshComponent* Visual)
	{
		if (!Visual || !Visual->IsVisible())
		{
			return;
		}
		const int32 Slots = FMath::Max(1, Visual->GetNumMaterials());
		for (int32 Slot = 0; Slot < Slots; ++Slot)
		{
			UMaterialInterface* Parent = Visual->GetMaterial(Slot);
			UMaterialInstanceDynamic* Mid = Cast<UMaterialInstanceDynamic>(Parent);
			if (!Mid && Parent)
			{
				Mid = UMaterialInstanceDynamic::Create(Parent, Visual);
				if (Mid)
				{
					Visual->SetMaterial(Slot, Mid);
				}
			}
			if (!Mid)
			{
				continue;
			}
			Mid->SetVectorParameterValue(TEXT("Color"), Color);
			Mid->SetVectorParameterValue(TEXT("BaseColor"), Color);
			Mid->SetScalarParameterValue(TEXT("Opacity"), 0.45f);
		}
	};
	Paint(Mesh);
	Paint(PreviewSkel);
}

void AKodBuildGhost::SetPreview(FName TypeId)
{
	if (!Mesh)
	{
		return;
	}
	const FKodMeshChoice Choice = FKodMeshCatalog::Resolve(TypeId);
	// Catalog fit sits slightly inside the footprint. That gap is visual only.
	// Pathing uses the full cells, so do not scale the mesh up to close it.
	const FVector Scale(FMath::Max(0.01f, Choice.Scale));
	if (Choice.bLoaded && Choice.StaticMesh)
	{
		if (PreviewSkel)
		{
			PreviewSkel->SetVisibility(false);
		}
		Mesh->SetStaticMesh(Choice.StaticMesh);
		Mesh->SetVisibility(true);
		Mesh->SetRelativeRotation(FRotator::ZeroRotator);
		Mesh->SetRelativeScale3D(Scale);
		Mesh->SetRelativeLocation(FVector(0.f, 0.f, Choice.Z));
		return;
	}
	if (Choice.bLoaded && Choice.SkeletalMesh)
	{
		Mesh->SetVisibility(false);
		if (!PreviewSkel)
		{
			PreviewSkel = NewObject<USkeletalMeshComponent>(this, TEXT("GhostSkel"));
			PreviewSkel->SetupAttachment(SceneRoot);
			PreviewSkel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			PreviewSkel->SetCastShadow(false);
			PreviewSkel->RegisterComponent();
		}
		PreviewSkel->SetSkeletalMeshAsset(Choice.SkeletalMesh);
		PreviewSkel->SetVisibility(true);
		PreviewSkel->SetRelativeRotation(FRotator::ZeroRotator);
		PreviewSkel->SetRelativeScale3D(Scale);
		PreviewSkel->SetRelativeLocation(FVector(0.f, 0.f, Choice.Z));
		return;
	}
	Mesh->SetVisibility(false);
	if (PreviewSkel)
	{
		PreviewSkel->SetVisibility(false);
	}
}

void AKodBuildGhost::Show(FName TypeId, const FKodLandingPose& Pose, const TArray<uint8>& CellOk, bool bValid)
{
	SetActorLocation(Pose.Location, false, nullptr, ETeleportType::TeleportPhysics);
	SetActorRotation(FRotator(0.f, Pose.Yaw, 0.f), ETeleportType::TeleportPhysics);
	SetPreview(TypeId);
	SetCells(Pose.FootX, Pose.FootY, CellOk);
	SetValidTint(bValid);
}
