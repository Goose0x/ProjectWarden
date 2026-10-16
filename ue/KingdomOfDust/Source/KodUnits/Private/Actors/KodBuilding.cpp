#include "Actors/KodBuilding.h"
#include "Actors/KodMeshAttach.h"
#include "Game/KodBuildVisual.h"
#include "Game/KodLanding.h"
#include "Game/KodCollision.h"
#include "Actors/KodJadeiteVisual.h"
#include "Game/KodJadeitePalette.h"
#include "Data/KodBuildingDefinition.h"
#include "Data/KodMeshCatalog.h"
#include "Warden/KodWardenPaths.h"
#include "Components/MeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Game/KodMeshSockets.h"
#include "Animation/AnimEnums.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/CollisionProfile.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Misc/Paths.h"
#include "UObject/SoftObjectPath.h"
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
	return KodBuildVisual::StageIndex(KodBuildVisual::Progress(Structure.BuildTicksDone, Structure.BuildTimeTotal));
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
			if (!bDepotLowerKnown)
			{
				bDepotLowerKnown = true;
				bDepotLowerLatched = Structure.bLowered;
			}
			else if (bDepotLowerLatched != Structure.bLowered)
			{
				bDepotLowerLatched = Structure.bLowered;
				PlayDepotCommandAnim(Structure.bLowered);
			}
			const float Target = Structure.bLowered ? 1.f : 0.f;
			DepotLowerAlpha = FMath::FInterpConstantTo(DepotLowerAlpha, Target, DeltaSeconds, 2.f);
		}
		if (Structure.bAirborne)
		{
			Rise = 220.f;
		}
		else if (Structure.bCanLower && DepotLowerAlpha > 0.f && !bDepotClipDriven)
		{
			const float Plate = FMath::Lerp(1.f, 0.12f, DepotLowerAlpha);
			Scale.Z *= Plate;
		}
		else if (!Structure.bComplete && Structure.BuildTimeTotal > 0)
		{
			ApplyConstructionVisual(Sim, Structure, Scale);
			bSawIncomplete = true;
		}
		else if (!bSawComplete)
		{
			RestoreFinalPresentation(Sim);
			bSawComplete = true;
			const FKodLandingPose DonePose = KodLanding::MakePose(
				Structure.CellX,
				Structure.CellY,
				Structure.FootprintX,
				Structure.FootprintY,
				Structure.Position.Z);
			KodLanding::ValidateLandingOrientation(this, DonePose);
			if (bSawIncomplete)
			{
				bCompletionFlash = true;
				FlashLeft = 0.35f;
			}
		}
		const float GhostProgress = (Structure.bComplete || Structure.BuildTimeTotal <= 0)
			? 1.f
			: KodBuildVisual::Progress(Structure.BuildTicksDone, Structure.BuildTimeTotal);
		UpdateDynastyGhost(GhostProgress);
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
	const bool bAir = Sim->TryGetStructure(EntityId, Structure) && Structure.bAirborne;
	const FRotator Facing = bAir
		? KodLanding::EaseTowardCanonical(GetActorRotation(), DeltaSeconds)
		: KodLanding::CanonicalRotation();
	BuildingMesh->SetRelativeScale3D(Scale);
	BuildingMesh->SetRelativeRotation(Facing);
	BuildingMesh->SetRelativeLocation(FVector(0.f, 0.f, MeshZ + Rise));
	if (BuildingSkel && !BuildingSkel->bHiddenInGame)
	{
		BuildingSkel->SetRelativeScale3D(BaseScale);
		BuildingSkel->SetRelativeRotation(FRotator::ZeroRotator);
		BuildingSkel->SetRelativeLocation(FVector(0.f, 0.f, MeshZ + Rise));
	}
	if (bWasAirborne && !bAir)
	{
		const FKodLandingPose Pose = KodLanding::MakePose(
			Structure.CellX,
			Structure.CellY,
			Structure.FootprintX,
			Structure.FootprintY,
			Structure.Position.Z);
		KodLanding::ValidateLandingOrientation(this, Pose);
	}
	bWasAirborne = bAir;
	FKodSimEntityState Combat;
	if (Sim->TryGetState(EntityId, Combat))
	{
		UpdateLowHealthFx(Combat.Health, Combat.MaxHealth);
	}
	SyncBunkerTurret(Sim, DeltaSeconds);
}

void AKodBuilding::FlashBunkerMuzzle()
{
	const FName Ports[] = {
		KodMeshSockets::Port0,
		KodMeshSockets::Port1,
		KodMeshSockets::Port2,
		KodMeshSockets::Port3
	};
	FVector At = GetActorLocation() + FVector(0.f, 0.f, 120.f);
	bool bSocket = false;
	FName Used = KodMeshSockets::Port0;
	for (int32 Step = 0; Step < 4; ++Step)
	{
		const int32 Index = (NextPort + Step) % 4;
		FVector SocketAt = FVector::ZeroVector;
		if (KodMeshSockets::TryGet(this, Ports[Index], SocketAt))
		{
			At = SocketAt;
			Used = Ports[Index];
			NextPort = (Index + 1) % 4;
			bSocket = true;
			break;
		}
	}
	if (!bSocket && !bLoggedPortSocket)
	{
		bLoggedPortSocket = true;
		UE_LOG(LogTemp, Warning, TEXT("KodMesh SocketMissing Id=%s Socket=Port_0"), *FName(KodWardenPaths::Id_Bunker).ToString());
	}
	if (!BunkerMuzzle)
	{
		UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
		BunkerMuzzle = NewObject<UStaticMeshComponent>(this, TEXT("BunkerMuzzle"));
		BunkerMuzzle->SetupAttachment(GetRootComponent());
		BunkerMuzzle->SetStaticMesh(Sphere);
		BunkerMuzzle->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		BunkerMuzzle->SetCastShadow(false);
		BunkerMuzzle->ComponentTags.Add(FName(TEXT("KodFx")));
		BunkerMuzzle->RegisterComponent();
	}
	if (!BunkerMuzzleLight)
	{
		BunkerMuzzleLight = NewObject<UPointLightComponent>(this, TEXT("BunkerMuzzleLight"));
		BunkerMuzzleLight->SetupAttachment(GetRootComponent());
		BunkerMuzzleLight->SetIntensity(40000.f);
		BunkerMuzzleLight->SetAttenuationRadius(400.f);
		BunkerMuzzleLight->SetCastShadows(false);
		BunkerMuzzleLight->SetLightColor(FLinearColor(1.f, 0.72f, 0.35f));
		BunkerMuzzleLight->ComponentTags.Add(FName(TEXT("KodFx")));
		BunkerMuzzleLight->RegisterComponent();
	}
	BunkerMuzzle->SetWorldLocation(At);
	BunkerMuzzle->SetWorldScale3D(FVector(0.15f));
	BunkerMuzzle->SetVisibility(true);
	BunkerMuzzle->SetHiddenInGame(false);
	BunkerMuzzleLight->SetWorldLocation(At);
	BunkerMuzzleLight->SetVisibility(true);
	BunkerMuzzleLight->SetHiddenInGame(false);
	MuzzleFlashLeft = 0.06f;
	UE_LOG(LogTemp, Log, TEXT("KodBunker Muzzle Id=%d Port=%s"), EntityId.Value, *Used.ToString());
}

void AKodBuilding::SyncBunkerTurret(UKodSimSubsystem* Sim, float DeltaSeconds)
{
	if (MuzzleFlashLeft > 0.f)
	{
		MuzzleFlashLeft -= DeltaSeconds;
		if (MuzzleFlashLeft <= 0.f)
		{
			if (BunkerMuzzle)
			{
				BunkerMuzzle->SetVisibility(false);
				BunkerMuzzle->SetHiddenInGame(true);
			}
			if (BunkerMuzzleLight)
			{
				BunkerMuzzleLight->SetVisibility(false);
				BunkerMuzzleLight->SetHiddenInGame(true);
			}
		}
	}
	FKodStructureState Structure;
	if (!Sim || !Sim->TryGetStructure(EntityId, Structure) || Structure.DefinitionId != FName(KodWardenPaths::Id_Bunker))
	{
		return;
	}
	if (Structure.MuzzleSerial != SeenMuzzle)
	{
		SeenMuzzle = Structure.MuzzleSerial;
		FlashBunkerMuzzle();
	}
	if (!Structure.bTurret || TurretPieces.Num() > 0)
	{
		return;
	}
	const FKodMeshChoice Choice = FKodMeshCatalog::Resolve(FName(KodWardenPaths::Id_BunkerTurret));
	UStaticMesh* Mesh = (Choice.bLoaded && Choice.StaticMesh) ? Choice.StaticMesh : nullptr;
	if (!Mesh)
	{
		Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	}
	if (Mesh && Mesh->GetPathName().Contains(TEXT("/Engine/BasicShapes/Cube")))
	{
		UE_LOG(LogTemp, Log, TEXT("KodMesh Placeholder Id=%s Path=%s"), *FName(KodWardenPaths::Id_BunkerTurret).ToString(), *Mesh->GetPathName());
	}
	USceneComponent* Body = BuildingSkel && !BuildingSkel->bHiddenInGame ? static_cast<USceneComponent*>(BuildingSkel) : static_cast<USceneComponent*>(BuildingMesh);
	const FName Mounts[] = {
		KodMeshSockets::TurretMount,
		FName(TEXT("TurretMount_0")),
		FName(TEXT("TurretMount_1")),
		FName(TEXT("TurretMount_2")),
		FName(TEXT("TurretMount_3"))
	};
	TArray<FName> Found;
	if (Body)
	{
		for (const FName& Mount : Mounts)
		{
			if (Body->DoesSocketExist(Mount))
			{
				Found.Add(Mount);
				continue;
			}
			const FName Prefixed(*FString::Printf(TEXT("SOCKET_%s"), *Mount.ToString()));
			if (Body->DoesSocketExist(Prefixed))
			{
				Found.Add(Prefixed);
			}
		}
	}
	if (Found.Num() == 0)
	{
		Found.Add(NAME_None);
		if (!bLoggedTurretSocket)
		{
			bLoggedTurretSocket = true;
			UE_LOG(LogTemp, Warning, TEXT("KodMesh SocketMissing Id=%s Socket=TurretMount"), *FName(KodWardenPaths::Id_Bunker).ToString());
		}
	}
	const float Scale = (Choice.bLoaded && Choice.Scale > 0.01f) ? Choice.Scale : 0.4f;
	for (int32 Index = 0; Index < Found.Num(); ++Index)
	{
		UStaticMeshComponent* Piece = NewObject<UStaticMeshComponent>(this);
		Piece->SetupAttachment(Body ? Body : GetRootComponent(), Found[Index]);
		Piece->SetStaticMesh(Mesh);
		Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Piece->SetCastShadow(false);
		Piece->ComponentTags.Add(FName(TEXT("KodBunkerTurret")));
		Piece->ComponentTags.Add(FName(TEXT("KodFx")));
		Piece->SetRelativeScale3D(FVector(Scale));
		if (Found[Index].IsNone())
		{
			Piece->SetRelativeLocation(FVector(0.f, 0.f, 80.f));
		}
		Piece->RegisterComponent();
		TurretPieces.Add(Piece);
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

void AKodBuilding::PlayDepotCommandAnim(bool bLower)
{
	bDepotClipDriven = false;
	const UKodBuildingDefinition* Def = GetDefinition();
	const FName MeshId = Def && !Def->DefinitionId.IsNone() ? Def->DefinitionId : FName(TEXT("SupplyDepot"));
	FKodMeshRow Row;
	if (!KodMeshCatalogFindRow(MeshId, Row) || !BuildingSkel || !BuildingSkel->GetSkeletalMeshAsset())
	{
		return;
	}
	const FString Clip = bLower ? Row.LowerAnim : Row.RaiseAnim;
	if (Row.AnimFolder.IsEmpty() || Clip.IsEmpty())
	{
		return;
	}
	FString Folder = Row.AnimFolder;
	if (!Folder.EndsWith(TEXT("/")))
	{
		Folder += TEXT("/");
	}
	const FString Path = Folder + Clip + TEXT(".") + Clip;
	UAnimSequence* Sequence = LoadObject<UAnimSequence>(nullptr, *Path);
	USkeleton* MeshSkeleton = BuildingSkel->GetSkeletalMeshAsset()->GetSkeleton();
	if (!Sequence || (MeshSkeleton && Sequence->GetSkeleton() != MeshSkeleton))
	{
		UE_LOG(LogTemp, Log, TEXT("KodMesh AnimMismatch Id=%s Clip=%s"), *MeshId.ToString(), *Clip);
		return;
	}
	BuildingSkel->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	BuildingSkel->bPauseAnims = false;
	BuildingSkel->PlayAnimation(Sequence, false);
	bDepotClipDriven = true;
	UE_LOG(LogTemp, Log, TEXT("KodMesh DepotAnim Id=%s Clip=%s Lower=%d"), *MeshId.ToString(), *Clip, bLower ? 1 : 0);
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

	FName MeshId = Def->DefinitionId.IsNone() ? Def->GetFName() : Def->DefinitionId;
	if (MeshId == FName(KodWardenPaths::Id_CommandCenter) && TeamId == 0)
	{
		if (const UWorld* MeshWorld = GetWorld())
		{
			if (const UKodSimSubsystem* MeshSim = MeshWorld->GetSubsystem<UKodSimSubsystem>())
			{
				if (MeshSim->GetPlayerFaction() == 1)
				{
					MeshId = FName(TEXT("ScinderHQ"));
				}
			}
		}
	}
	const FKodMeshChoice Choice = FKodMeshCatalog::Resolve(MeshId);
	if (Choice.bLoaded && Choice.bSkeletal && Choice.SkeletalMesh)
	{
		if (!BuildingSkel)
		{
			BuildingSkel = NewObject<USkeletalMeshComponent>(this, TEXT("BuildingSkel"));
			BuildingSkel->SetupAttachment(GetRootComponent());
			BuildingSkel->SetMobility(EComponentMobility::Movable);
			BuildingSkel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			BuildingSkel->RegisterComponent();
		}
		BuildingSkel->SetSkeletalMeshAsset(Choice.SkeletalMesh);
		BuildingSkel->SetAnimationMode(EAnimationMode::AnimationCustomMode);
		BuildingSkel->SetAnimInstanceClass(nullptr);
		BuildingSkel->bPauseAnims = true;
		MeshYaw = Choice.Yaw;
		MeshZ = Choice.Z;
		BuildingSkel->SetRelativeScale3D(FVector(Choice.Scale));
		BuildingSkel->SetRelativeRotation(FRotator::ZeroRotator);
		BuildingMesh->SetRelativeRotation(KodLanding::CanonicalRotation());
		BuildingSkel->SetRelativeLocation(FVector(0.f, 0.f, MeshZ));
		BuildingSkel->SetHiddenInGame(false);
		BuildingSkel->SetVisibility(true);
		if (BuildingMesh)
		{
			BuildingMesh->SetHiddenInGame(true);
			BuildingMesh->SetVisibility(false);
		}
		BaseScale = FVector(Choice.Scale);
	}
	else
	{
		UStaticMesh* LoadedMesh = Choice.bLoaded ? Choice.StaticMesh : nullptr;
		if (!LoadedMesh && !Def->Mesh.IsNull())
		{
			LoadedMesh = Def->Mesh.LoadSynchronous();
		}
		if (BuildingSkel)
		{
			BuildingSkel->SetHiddenInGame(true);
			BuildingSkel->SetVisibility(false);
		}
		if (LoadedMesh && BuildingMesh)
		{
			BuildingMesh->SetStaticMesh(LoadedMesh);
			BuildingMesh->SetHiddenInGame(false);
			BuildingMesh->SetVisibility(true);
			if (Choice.bLoaded)
			{
				BaseScale = FVector(Choice.Scale);
				MeshYaw = Choice.Yaw;
				MeshZ = Choice.Z;
				BuildingMesh->SetRelativeRotation(KodLanding::CanonicalRotation());
				BuildingMesh->SetRelativeLocation(FVector(0.f, 0.f, MeshZ));
				BuildingMesh->SetRelativeScale3D(BaseScale);
			}
			else
			{
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
				MeshYaw = 0.f;
			}
			BuildingMesh->SetRelativeRotation(KodLanding::CanonicalRotation());
			BuildingMesh->SetRelativeScale3D(BaseScale);
		}
		else if (BuildingMesh)
		{
			const float Span = static_cast<float>(FMath::Max(Def->FootprintX, Def->FootprintY)) * 2.f;
			BaseScale = FVector(Span, Span, Span * 0.6f);
			MeshYaw = 0.f;
			BuildingMesh->SetRelativeRotation(KodLanding::CanonicalRotation());
			BuildingMesh->SetRelativeScale3D(BaseScale);
		}
	}
	const bool bSkelVisible = BuildingSkel && !BuildingSkel->bHiddenInGame && BuildingSkel->GetSkeletalMeshAsset();
	if (!bSkelVisible && BuildingMesh && BuildingMesh->GetStaticMesh())
	{
		const FString MeshPath = BuildingMesh->GetStaticMesh()->GetPathName();
		if (MeshPath.Contains(TEXT("/Engine/BasicShapes/Cube")))
		{
			UE_LOG(LogTemp, Log, TEXT("KodMesh Placeholder Id=%s Path=%s"), *MeshId.ToString(), *MeshPath);
		}
	}
	USceneComponent* AttachBody = nullptr;
	if (bSkelVisible)
	{
		AttachBody = BuildingSkel;
	}
	else if (BuildingMesh && BuildingMesh->GetStaticMesh())
	{
		AttachBody = BuildingMesh;
	}
	ActiveMeshId = MeshId;
	bFinalPrimaryBody = Choice.bLoaded && !Choice.bFallback;
	RememberFinalMesh();
	bool bDeferAttach = false;
	if (UWorld* DeferWorld = GetWorld())
	{
		if (UKodSimSubsystem* DeferSim = DeferWorld->GetSubsystem<UKodSimSubsystem>())
		{
			FKodStructureState DeferStructure;
			if (DeferSim->TryGetStructure(EntityId, DeferStructure) && !DeferStructure.bComplete && DeferStructure.BuildTimeTotal > 0)
			{
				FKodMeshRow DeferRow;
				const bool bDeferRow = KodMeshCatalogFindRow(MeshId, DeferRow);
				const bool bHasStages = bDeferRow && !DeferRow.Stage1.IsEmpty() && !DeferRow.Stage2.IsEmpty() && !DeferRow.Stage3.IsEmpty();
				const EKodBuildVisual Requested = bDeferRow ? KodBuildVisual::ParseMode(DeferRow.BuildVisual) : EKodBuildVisual::Scaffold;
				const EKodBuildVisual Mode = KodBuildVisual::Resolve(MeshId, Requested, bHasStages);
				bDeferAttach = Mode == EKodBuildVisual::Stages || Mode == EKodBuildVisual::Vfx;
			}
		}
	}
	if (bDeferAttach)
	{
		bDeferredAttach = true;
	}
	else
	{
		KodMeshAttach::Apply(this, AttachBody, MeshId, bFinalPrimaryBody);
	}
	RefreshJadeiteEmissive();
}

namespace
{
	UStaticMesh* LoadStageMesh(const FString& Path)
	{
		if (Path.IsEmpty())
		{
			return nullptr;
		}
		FString Full = Path;
		if (!Full.Contains(TEXT(".")))
		{
			Full += TEXT(".") + FPaths::GetBaseFilename(Full);
		}
		return Cast<UStaticMesh>(FSoftObjectPath(Full).TryLoad());
	}

	float StageScaleAt(const FKodMeshRow& Row, int32 Stage)
	{
		const float Defaults[3] = {0.45f, 0.70f, 0.92f};
		TArray<FString> Parts;
		Row.StageScales.ParseIntoArray(Parts, TEXT("|"), true);
		const int32 Index = FMath::Clamp(Stage, 0, 2);
		if (Parts.IsValidIndex(Index))
		{
			const float Parsed = FCString::Atof(*Parts[Index]);
			if (Parsed > 0.f && Parsed <= 1.f)
			{
				return Parsed;
			}
		}
		return Defaults[Index];
	}

	const FString& StagePath(const FKodMeshRow& Row, int32 Stage)
	{
		if (Stage <= 0)
		{
			return Row.Stage1;
		}
		if (Stage == 1)
		{
			return Row.Stage2;
		}
		return Row.Stage3;
	}
}

void AKodBuilding::RememberFinalMesh()
{
	bFinalSkeletal = BuildingSkel && !BuildingSkel->bHiddenInGame && BuildingSkel->GetSkeletalMeshAsset() != nullptr;
	bFinalSkelHidden = !BuildingSkel || BuildingSkel->bHiddenInGame;
	bFinalStaticHidden = !BuildingMesh || BuildingMesh->bHiddenInGame;
	FinalStaticMesh = BuildingMesh ? BuildingMesh->GetStaticMesh() : nullptr;
}

void AKodBuilding::SetConstructionAttachesVisible(bool bVisible)
{
	TInlineComponentArray<USceneComponent*> Pieces;
	GetComponents(Pieces);
	for (USceneComponent* Piece : Pieces)
	{
		if (Piece && Piece->ComponentHasTag(FName(TEXT("KodMeshAttach"))))
		{
			Piece->SetHiddenInGame(!bVisible);
			Piece->SetVisibility(bVisible);
		}
	}
}

void AKodBuilding::UpdateDynastyGhost(float Progress)
{
	if (!KodBuildVisual::DynastyGhostEnabled())
	{
		if (DynastyGhost)
		{
			DynastyGhost->SetHiddenInGame(true);
			DynastyGhost->SetVisibility(false);
		}
		return;
	}
	if (!bLoggedDynastyGhost)
	{
		bLoggedDynastyGhost = true;
		UE_LOG(LogTemp, Log, TEXT("KodBuild DynastyGhost hook Id=%s"), *ActiveMeshId.ToString());
	}
	if (!DynastyGhost && FinalStaticMesh)
	{
		DynastyGhost = NewObject<UStaticMeshComponent>(this, TEXT("DynastyGhost"));
		DynastyGhost->SetupAttachment(GetRootComponent());
		DynastyGhost->SetStaticMesh(FinalStaticMesh);
		DynastyGhost->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		DynastyGhost->SetRelativeScale3D(BaseScale);
		DynastyGhost->RegisterComponent();
	}
	if (DynastyGhost)
	{
		const bool bShow = Progress < 0.999f;
		DynastyGhost->SetHiddenInGame(!bShow);
		DynastyGhost->SetVisibility(bShow);
	}
}

void AKodBuilding::RestoreFinalPresentation(UKodSimSubsystem* Sim)
{
	(void)Sim;
	if (bConstructionMeshSwap || bConstructionHidden)
	{
		if (BuildingMesh)
		{
			if (FinalStaticMesh)
			{
				BuildingMesh->SetStaticMesh(FinalStaticMesh);
			}
			BuildingMesh->SetHiddenInGame(bFinalStaticHidden);
			BuildingMesh->SetVisibility(!bFinalStaticHidden);
		}
		if (BuildingSkel)
		{
			BuildingSkel->SetHiddenInGame(bFinalSkelHidden);
			BuildingSkel->SetVisibility(!bFinalSkelHidden);
		}
		bConstructionMeshSwap = false;
		bConstructionHidden = false;
	}
	ShownStage = 3;
	SetConstructionAttachesVisible(true);
	if (bDeferredAttach)
	{
		USceneComponent* AttachBody = nullptr;
		if (bFinalSkeletal && BuildingSkel)
		{
			AttachBody = BuildingSkel;
		}
		else if (BuildingMesh && BuildingMesh->GetStaticMesh())
		{
			AttachBody = BuildingMesh;
		}
		KodMeshAttach::Apply(this, AttachBody, ActiveMeshId, bFinalPrimaryBody && AttachBody != nullptr);
		bDeferredAttach = false;
	}
}

void AKodBuilding::ApplyConstructionVisual(UKodSimSubsystem* Sim, FKodStructureState& Structure, FVector& Scale)
{
	if (ActiveMeshId.IsNone())
	{
		ActiveMeshId = Structure.DefinitionId;
	}
	const float BuildProgress = KodBuildVisual::Progress(Structure.BuildTicksDone, Structure.BuildTimeTotal);
	FKodMeshRow Row;
	const bool bRow = KodMeshCatalogFindRow(ActiveMeshId, Row);
	const bool bHasStages = bRow && !Row.Stage1.IsEmpty() && !Row.Stage2.IsEmpty() && !Row.Stage3.IsEmpty();
	const EKodBuildVisual Requested = bRow ? KodBuildVisual::ParseMode(Row.BuildVisual) : EKodBuildVisual::Scaffold;
	const EKodBuildVisual Mode = KodBuildVisual::Resolve(ActiveMeshId, Requested, bHasStages);
	if (Requested == EKodBuildVisual::Stages && Mode != EKodBuildVisual::Stages && !bLoggedStageFallback)
	{
		bLoggedStageFallback = true;
		UE_LOG(LogTemp, Log, TEXT("KodBuild ScaffoldFallback Id=%s"), *ActiveMeshId.ToString());
	}
	const int32 Stage = KodBuildVisual::StageIndex(BuildProgress);
	SetConstructionAttachesVisible(false);
	if (Mode == EKodBuildVisual::Vfx)
	{
		if (BuildingMesh)
		{
			BuildingMesh->SetHiddenInGame(true);
		}
		if (BuildingSkel)
		{
			BuildingSkel->SetHiddenInGame(true);
		}
		bConstructionHidden = true;
		if (ShownStage != Stage)
		{
			ShownStage = Stage;
			Sim->RaiseCue(FName(TEXT("build_dust")), GetActorLocation(), true);
			UE_LOG(LogTemp, Log, TEXT("KodVfx DustPuff Id=%s Stage=%d"), *ActiveMeshId.ToString(), Stage);
		}
		return;
	}
	if (Mode == EKodBuildVisual::Stages)
	{
		if (ShownStage != Stage)
		{
			if (UStaticMesh* StageMesh = LoadStageMesh(StagePath(Row, Stage)))
			{
				if (BuildingMesh)
				{
					BuildingMesh->SetStaticMesh(StageMesh);
					BuildingMesh->SetHiddenInGame(false);
					BuildingMesh->SetVisibility(true);
				}
				if (BuildingSkel)
				{
					BuildingSkel->SetHiddenInGame(true);
				}
				bConstructionMeshSwap = true;
			}
			ShownStage = Stage;
			Sim->RaiseCue(FName(TEXT("build_dust")), GetActorLocation(), true);
			UE_LOG(LogTemp, Log, TEXT("KodVfx DustPuff Id=%s Stage=%d"), *ActiveMeshId.ToString(), Stage);
		}
		Scale *= StageScaleAt(Row, Stage);
		return;
	}
	const float ScaffoldScale[3] = {0.2f, 0.45f, 0.75f};
	Scale *= ScaffoldScale[FMath::Clamp(Stage, 0, 2)];
	if (ShownStage != Stage)
	{
		ShownStage = Stage;
		Sim->RaiseCue(FName(TEXT("build_dust")), GetActorLocation(), true);
		UE_LOG(LogTemp, Log, TEXT("KodVfx DustPuff Id=%s Stage=%d"), *ActiveMeshId.ToString(), Stage);
	}
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
