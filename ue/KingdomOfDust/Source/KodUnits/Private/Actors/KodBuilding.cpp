#include "Actors/KodBuilding.h"
#include "Data/KodBuildingDefinition.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Sim/KodBuildTypes.h"
#include "Sim/KodSimSubsystem.h"
#include "Slice0/KodSlice0Bootstrap.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/SoftObjectPath.h"

AKodBuilding::AKodBuilding()
{
	PrimaryActorTick.bCanEverTick = true;
	BuildingMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BuildingMesh"));
	SetRootComponent(BuildingMesh);

	// Default StaticMeshComponent profile is BlockAllDynamic, which blocks ECC_Pawn
	// (click-select) and Visibility. Engine cube until a real building mesh exists.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded())
	{
		BuildingMesh->SetStaticMesh(CubeFinder.Object);
	}
	BuildingMesh->SetRelativeScale3D(FVector(3.f, 3.f, 2.f));
	BuildingMesh->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	BuildingMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BaseScale = BuildingMesh->GetRelativeScale3D();
}

void AKodBuilding::BeginPlay()
{
	Super::BeginPlay();
	if (UWorld* World = GetWorld())
	{
		if (UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>())
		{
			if (EntityId.IsValid())
			{
				Sim->BindStructureActor(EntityId, this);
				bBoundStructure = true;
			}
			else
			{
				EntityId = Sim->RegisterEntity(this);
			}
		}
	}
	if (UKodBuildingDefinition* Def = GetDefinition())
	{
		ApplyDefinition(Def);
	}
	if (bBoundStructure)
	{
		if (UWorld* World = GetWorld())
		{
			if (UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>())
			{
				Sim->BindStructureActor(EntityId, this);
			}
		}
	}
}

void AKodBuilding::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Sim || !BuildingMesh)
	{
		return;
	}
	if (bBoundStructure && !Sim->IsStructure(EntityId))
	{
		Destroy();
		return;
	}
	FKodStructureState Structure;
	float Alpha = 1.f;
	if (Sim->TryGetStructure(EntityId, Structure) && !Structure.bComplete && Structure.BuildTimeTotal > 0)
	{
		Alpha = 0.1f + 0.9f * (static_cast<float>(Structure.BuildTicksDone) / static_cast<float>(Structure.BuildTimeTotal));
	}
	FVector Scale = BaseScale;
	float Rise = 0.f;
	if (Sim->TryGetStructure(EntityId, Structure))
	{
		if (Structure.bLowered)
		{
			Scale.Z *= 0.25f;
			Rise = -80.f;
		}
		else if (!Structure.bComplete && Structure.BuildTimeTotal > 0)
		{
			Alpha = 0.1f + 0.9f * (static_cast<float>(Structure.BuildTicksDone) / static_cast<float>(Structure.BuildTimeTotal));
			Scale.Z *= FMath::Clamp(Alpha, 0.15f, 1.f);
			const float Height = BaseScale.Z * 100.f;
			Rise = -Height * (1.f - Alpha);
			bSawIncomplete = true;
		}
		else if (!bSawComplete)
		{
			bSawComplete = true;
			if (bSawIncomplete)
			{
				bCompletionFlash = true;
				FlashLeft = 0.35f;
			}
		}
	}
	if (bCompletionFlash)
	{
		FlashLeft -= DeltaSeconds;
		Scale *= 1.f + 0.08f * FMath::Max(0.f, FlashLeft / 0.35f);
		if (FlashLeft <= 0.f)
		{
			bCompletionFlash = false;
		}
	}
	BuildingMesh->SetRelativeScale3D(Scale);
	BuildingMesh->SetRelativeLocation(FVector(0.f, 0.f, Rise));
}

void AKodBuilding::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>())
		{
			Sim->UnregisterEntityIfActor(EntityId, this);
		}
	}
	Super::EndPlay(EndPlayReason);
}

UKodBuildingDefinition* AKodBuilding::GetDefinition() const
{
	if (UKodBuildingDefinition* Loaded = Definition.LoadSynchronous())
	{
		return Loaded;
	}
	if (!Definition.IsNull())
	{
		return UKodSlice0Bootstrap::ResolveBuilding(Definition.ToSoftObjectPath().GetAssetFName(), const_cast<AKodBuilding*>(this));
	}
	return nullptr;
}

void AKodBuilding::ApplyDefinition(UKodBuildingDefinition* Def)
{
	if (!Def)
	{
		return;
	}
	Definition = Def;

	if (UWorld* World = GetWorld())
	{
		if (UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>())
		{
			if (!EntityId.IsValid())
			{
				EntityId = Sim->RegisterEntity(this);
			}
			Sim->ConfigureEntity(
				EntityId,
				Def->DefinitionId.IsNone() ? Def->GetFName() : Def->DefinitionId,
				Def->MaxHealth,
				0.f,
				0.f,
				false,
				0.f,
				0.f,
				0.f,
				TeamId,
				false,
				34,
				FMath::Max(0, FMath::RoundToInt(Def->SightRadius)));
		}
	}

	UStaticMesh* LoadedMesh = nullptr;
	if (!Def->Mesh.IsNull())
	{
		LoadedMesh = Def->Mesh.LoadSynchronous();
	}
	if (LoadedMesh && BuildingMesh)
	{
		BuildingMesh->SetStaticMesh(LoadedMesh);
		const float SpanX = static_cast<float>(FMath::Max(1, Def->FootprintX)) * static_cast<float>(KodBuildGrid::CellUU);
		const float SpanY = static_cast<float>(FMath::Max(1, Def->FootprintY)) * static_cast<float>(KodBuildGrid::CellUU);
		const FVector Extent = LoadedMesh->GetBounds().BoxExtent * 2.f;
		const float SafeX = FMath::Max(1.f, Extent.X);
		const float SafeY = FMath::Max(1.f, Extent.Y);
		const float SafeZ = FMath::Max(1.f, Extent.Z);
		BaseScale = FVector(SpanX / SafeX, SpanY / SafeY, (SpanX * 0.45f) / SafeZ);
		if (!Def->MeshScale.Equals(FVector::OneVector, 0.01f))
		{
			BaseScale *= Def->MeshScale;
		}
		BuildingMesh->SetRelativeScale3D(BaseScale);
	}
	else if (BuildingMesh)
	{
		const float Span = static_cast<float>(FMath::Max(Def->FootprintX, Def->FootprintY)) * 2.f;
		BaseScale = FVector(Span, Span, Span * 0.6f);
		BuildingMesh->SetRelativeScale3D(BaseScale);
	}
}
