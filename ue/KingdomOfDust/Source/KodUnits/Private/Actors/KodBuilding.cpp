#include "Actors/KodBuilding.h"
#include "Game/KodCollision.h"
#include "Actors/KodJadeiteVisual.h"
#include "Game/KodJadeitePalette.h"
#include "Data/KodBuildingDefinition.h"
#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StreamableManager.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
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
	BuildingMesh->SetCollisionResponseToChannel(KodCollision::Select, ECR_Block);
	BuildingMesh->SetCollisionResponseToChannel(KodCollision::Ground, ECR_Ignore);
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
	BindJadeitePalette();
	RefreshJadeiteEmissive();
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

int32 AKodBuilding::GetScaffoldStage() const
{
	const UWorld* World = GetWorld();
	const UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	FKodStructureState Structure;
	if (!Sim || !Sim->TryGetStructure(EntityId, Structure) || Structure.bComplete || Structure.BuildTimeTotal <= 0)
	{
		return 3;
	}
	if (Structure.BuildTicksDone >= Structure.BuildTimeTotal)
	{
		return 3;
	}
	return FMath::Clamp((Structure.BuildTicksDone * 3) / Structure.BuildTimeTotal, 0, 2);
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
	const int32 Stage = GetScaffoldStage();
	FVector Scale = BaseScale;
	float Rise = 0.f;
	if (Sim->TryGetStructure(EntityId, Structure))
	{
		if (Structure.bCanLower)
		{
			if (!bDepotVisualInit)
			{
				bDepotVisualInit = true;
				DepotLowerAlpha = Structure.bLowered ? 1.f : 0.f;
			}
			const float Target = Structure.bLowered ? 1.f : 0.f;
			DepotLowerAlpha = FMath::FInterpConstantTo(DepotLowerAlpha, Target, DeltaSeconds, 2.f);
		}
		if (Structure.bAirborne)
		{
			Rise = 220.f;
		}
		else if (Structure.bCanLower && DepotLowerAlpha > 0.f)
		{
			const float Plate = FMath::Lerp(1.f, 0.12f, DepotLowerAlpha);
			Scale.Z *= Plate;
		}
		else if (!Structure.bComplete && Structure.BuildTimeTotal > 0)
		{
			const float StageScale[4] = {0.2f, 0.45f, 0.75f, 1.f};
			Scale *= StageScale[FMath::Clamp(Stage, 0, 3)];
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
		Scale *= 1.f + 0.22f * FMath::Max(0.f, FlashLeft / 0.35f);
		if (FlashLeft <= 0.f)
		{
			bCompletionFlash = false;
		}
	}
	BuildingMesh->SetRelativeScale3D(Scale);
	BuildingMesh->SetRelativeLocation(FVector(0.f, 0.f, Rise));
	FKodSimEntityState Combat;
	if (Sim->TryGetState(EntityId, Combat))
	{
		UpdateLowHealthFx(Combat.Health, Combat.MaxHealth);
	}
}

void AKodBuilding::UpdateLowHealthFx(float Health, float MaxHealth)
{
	if (!CachedDef || MaxHealth <= 0.f || !BuildingMesh)
	{
		return;
	}
	const float Ratio = Health / FMath::Max(1.f, MaxHealth);
	auto Ensure = [this](const TSoftObjectPtr<UNiagaraSystem>& Soft, TObjectPtr<UNiagaraComponent>& Comp, bool bWant)
	{
		if (!bWant || Soft.IsNull())
		{
			if (Comp)
			{
				Comp->Deactivate();
				Comp = nullptr;
			}
			return;
		}
		if (Comp)
		{
			if (!Comp->IsActive())
			{
				Comp->Activate(true);
			}
			return;
		}
		UNiagaraSystem* System = Soft.Get();
		if (!System)
		{
			return;
		}
		Comp = UNiagaraFunctionLibrary::SpawnSystemAttached(
			System,
			BuildingMesh,
			NAME_None,
			FVector(0.f, 0.f, 80.f),
			FRotator::ZeroRotator,
			EAttachLocation::KeepRelativeOffset,
			false);
	};
	Ensure(CachedDef->LowHealthSmoke, SmokeFx, Ratio < 0.35f);
	Ensure(CachedDef->LowHealthFire, FireFx, Ratio < 0.15f);
	if (!bLoggedLowHealth && Ratio < 0.35f)
	{
		bLoggedLowHealth = true;
		UE_LOG(LogTemp, Log, TEXT("KodFx LowHealth Id=%d Smoke=%d Fire=%d"),
			EntityId.Value,
			CachedDef->LowHealthSmoke.IsNull() ? 0 : 1,
			CachedDef->LowHealthFire.IsNull() ? 0 : 1);
	}
}

void AKodBuilding::PreloadLowHealthFx()
{
	if (LowHealthHandle.IsValid())
	{
		LowHealthHandle->CancelHandle();
		LowHealthHandle.Reset();
	}
	if (!CachedDef)
	{
		return;
	}
	TArray<FSoftObjectPath> Paths;
	if (!CachedDef->LowHealthSmoke.IsNull())
	{
		Paths.Add(CachedDef->LowHealthSmoke.ToSoftObjectPath());
	}
	if (!CachedDef->LowHealthFire.IsNull())
	{
		Paths.Add(CachedDef->LowHealthFire.ToSoftObjectPath());
	}
	if (Paths.Num() == 0)
	{
		return;
	}
	LowHealthHandle = LowHealthStream.RequestAsyncLoad(
		Paths,
		FStreamableDelegate(),
		0,
		false,
		false,
		TEXT("KodLowHealthFx"));
}

void AKodBuilding::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindJadeitePalette();
	if (LowHealthHandle.IsValid())
	{
		LowHealthHandle->CancelHandle();
		LowHealthHandle.Reset();
	}
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
	CachedDef = Def;
	PreloadLowHealthFx();

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
	RefreshJadeiteEmissive();
}

void AKodBuilding::BindJadeitePalette()
{
	if (JadeitePaletteHandle.IsValid())
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		if (UKodJadeitePaletteSubsystem* Palette = World->GetSubsystem<UKodJadeitePaletteSubsystem>())
		{
			JadeitePaletteHandle = Palette->OnPaletteChanged.AddUObject(this, &AKodBuilding::RefreshJadeiteEmissive);
		}
	}
}

void AKodBuilding::UnbindJadeitePalette()
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

void AKodBuilding::RefreshJadeiteEmissive()
{
	FName DefId = NAME_None;
	if (CachedDef)
	{
		DefId = CachedDef->DefinitionId.IsNone() ? CachedDef->GetFName() : CachedDef->DefinitionId;
	}
	if (!KodJadeiteVisual::IsScinderId(DefId) && !KodJadeiteVisual::IsScinderActor(this))
	{
		return;
	}
	TArray<UMeshComponent*> Meshes;
	GetComponents<UMeshComponent>(Meshes);
	for (UMeshComponent* Mesh : Meshes)
	{
		KodJadeiteVisual::ApplyTunedEmissive(Mesh);
	}
}
