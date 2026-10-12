#include "Slice0/KodMapLife.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Modules/ModuleManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "Sim/KodSimSubsystem.h"

namespace
{
	IAssetRegistry* MapRegistry()
	{
		if (!FModuleManager::Get().ModuleExists(TEXT("AssetRegistry")))
		{
			return nullptr;
		}
		FAssetRegistryModule& Module = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		IAssetRegistry& Registry = Module.Get();
		Registry.WaitForCompletion();
		return &Registry;
	}

	TArray<FAssetData> AssetsUnder(const TCHAR* Root)
	{
		TArray<FAssetData> Assets;
		IAssetRegistry* Registry = MapRegistry();
		if (!Registry)
		{
			return Assets;
		}
		FARFilter Filter;
		Filter.PackagePaths.Add(FName(Root));
		Filter.bRecursivePaths = true;
		Registry->GetAssets(Filter, Assets);
		Assets.Sort([](const FAssetData& A, const FAssetData& B)
		{
			return A.AssetName.ToString() < B.AssetName.ToString();
		});
		return Assets;
	}

	UObject* LoadAsset(const FAssetData& Data)
	{
		if (!Data.IsValid())
		{
			return nullptr;
		}
		return Data.GetSoftObjectPath().TryLoad();
	}

	FAssetData FindExact(const TCHAR* Root, const TCHAR* Name)
	{
		const FString Wanted(Name);
		for (const FAssetData& Asset : AssetsUnder(Root))
		{
			if (Asset.AssetName.ToString().Equals(Wanted, ESearchCase::IgnoreCase))
			{
				return Asset;
			}
		}
		return FAssetData();
	}

	FAssetData FindPrefix(const TCHAR* Root, const TCHAR* Prefix, const TCHAR* PreferA, const TCHAR* PreferB)
	{
		const FString Start(Prefix);
		FAssetData Fallback;
		for (const FAssetData& Asset : AssetsUnder(Root))
		{
			const FString Name = Asset.AssetName.ToString();
			if (!Name.StartsWith(Start))
			{
				continue;
			}
			if ((PreferA && Name.Contains(PreferA)) || (PreferB && Name.Contains(PreferB)))
			{
				return Asset;
			}
			if (!Fallback.IsValid())
			{
				Fallback = Asset;
			}
		}
		return Fallback;
	}

	bool BlockedDressing(const FVector& Point)
	{
		const FVector Bases[4] = {
			FVector(-8400.f, -8400.f, 0.f),
			FVector(8400.f, 8400.f, 0.f),
			FVector(-8400.f, -3000.f, 0.f),
			FVector(8400.f, 3000.f, 0.f)
		};
		const float Radii[4] = { 2200.f, 2200.f, 1800.f, 1800.f };
		for (int32 Index = 0; Index < 4; ++Index)
		{
			if (FVector::DistSquared2D(Point, Bases[Index]) < FMath::Square(Radii[Index]))
			{
				return true;
			}
		}
		auto NearLane = [](const FVector& P, const FVector& A, const FVector& B, float Width)
		{
			const FVector AB = FVector(B.X - A.X, B.Y - A.Y, 0.f);
			const float LenSq = AB.SizeSquared();
			const float T = LenSq > 1.f
				? FMath::Clamp(FVector::DotProduct(FVector(P.X - A.X, P.Y - A.Y, 0.f), AB) / LenSq, 0.f, 1.f)
				: 0.f;
			const FVector Closest(A.X + AB.X * T, A.Y + AB.Y * T, 0.f);
			return FVector::DistSquared2D(P, Closest) < FMath::Square(Width);
		};
		const FVector Player(-8400.f, -8400.f, 0.f);
		const FVector Enemy(8400.f, 8400.f, 0.f);
		const FVector West(-8400.f, -3000.f, 0.f);
		const FVector East(8400.f, 3000.f, 0.f);
		return NearLane(Point, Player, Enemy, 900.f)
			|| NearLane(Point, Player, West, 700.f)
			|| NearLane(Point, Enemy, East, 700.f);
	}

	void KodAmbient(const TArray<FString>& Args, UWorld* World)
	{
		const int32 Enabled = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 1;
		if (!World)
		{
			return;
		}
		for (TActorIterator<AKodMapLife> It(World); It; ++It)
		{
			It->SetAmbientEnabled(Enabled != 0);
		}
		UE_LOG(LogTemp, Log, TEXT("KodAmbient %d"), Enabled != 0 ? 1 : 0);
	}

	FAutoConsoleCommandWithWorldAndArgs GKodAmbient(
		TEXT("kod.Ambient"),
		TEXT("Presentation dressing and combat flourishes. Usage: kod.Ambient 0 or kod.Ambient 1"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&KodAmbient));
}

AKodMapLife::AKodMapLife()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AKodMapLife::BeginPlay()
{
	Super::BeginPlay();
	DiscoverAssets();
	if (bAmbient)
	{
		ScatterDressing();
	}
}

void AKodMapLife::SetAmbientEnabled(bool bEnabled)
{
	bAmbient = bEnabled;
	for (USceneComponent* Component : Dressing)
	{
		if (Component)
		{
			Component->SetVisibility(bEnabled, true);
		}
	}
	if (bEnabled && Dressing.Num() == 0)
	{
		ScatterDressing();
	}
}

void AKodMapLife::DiscoverAssets()
{
	Muzzle = Cast<UNiagaraSystem>(LoadAsset(FindPrefix(TEXT("/Game/MuzzleFlashesVol1"), TEXT("NS_MuzzleFlash_v"), TEXT("Rifle"), TEXT("Gun"))));
	Hit = Cast<UNiagaraSystem>(LoadAsset(FindExact(TEXT("/Game/HitsImpactsVol1"), TEXT("NS_HitImpact_v08"))));
	ExplosionMain = Cast<UNiagaraSystem>(LoadAsset(FindExact(TEXT("/Game/StylizedExplosionV1"), TEXT("NS_StylizedExplosion1_M"))));
	ExplosionGround = Cast<UNiagaraSystem>(LoadAsset(FindExact(TEXT("/Game/StylizedExplosionV1"), TEXT("NS_StylizedGroundCircularExplosion_L"))));
	ExplosionSmoke = Cast<UNiagaraSystem>(LoadAsset(FindExact(TEXT("/Game/StylizedExplosionV1"), TEXT("NS_StylizedSmokeExplosion_S"))));

	FAssetData Foot = FindPrefix(TEXT("/Game/A_Surface_Footstep"), TEXT("NS_"), TEXT("Dust"), TEXT("Foot"));
	if (!Foot.IsValid())
	{
		Foot = FindPrefix(TEXT("/Game/A_Surface_Footstep"), TEXT("P_"), TEXT("Dust"), TEXT("Foot"));
	}
	UObject* FootObj = LoadAsset(Foot);
	FootstepNiagara = Cast<UNiagaraSystem>(FootObj);
	FootstepCascade = Cast<UParticleSystem>(FootObj);

	UObject* FogObj = LoadAsset(FindExact(TEXT("/Game/StylizedRocks_Etherion"), TEXT("P_FogWind")));
	FogNiagara = Cast<UNiagaraSystem>(FogObj);
	FogCascade = Cast<UParticleSystem>(FogObj);
	UObject* SteamObj = LoadAsset(FindExact(TEXT("/Game/StylizedRocks_Etherion"), TEXT("P_Steam")));
	SteamNiagara = Cast<UNiagaraSystem>(SteamObj);
	SteamCascade = Cast<UParticleSystem>(SteamObj);
}

void AKodMapLife::ScatterDressing()
{
	TArray<UStaticMesh*> Cliffs;
	TArray<UStaticMesh*> Rocks;
	TArray<UMaterialInterface*> Tints;
	for (const FAssetData& Asset : AssetsUnder(TEXT("/Game/StylizedRocks_Etherion")))
	{
		const FString Name = Asset.AssetName.ToString();
		UObject* Loaded = LoadAsset(Asset);
		if (Name.StartsWith(TEXT("SM_Cliffside_")))
		{
			if (UStaticMesh* Mesh = Cast<UStaticMesh>(Loaded))
			{
				Cliffs.Add(Mesh);
			}
		}
		else if (Name.StartsWith(TEXT("SM_GroundRock_")))
		{
			if (UStaticMesh* Mesh = Cast<UStaticMesh>(Loaded))
			{
				Rocks.Add(Mesh);
			}
		}
		else if (Name.Contains(TEXT("Dusty")) || Name.Contains(TEXT("Sandstone")))
		{
			if (UMaterialInterface* Mat = Cast<UMaterialInterface>(Loaded))
			{
				Tints.Add(Mat);
			}
		}
	}
	if (UStaticMesh* Boulder = Cast<UStaticMesh>(LoadAsset(FindExact(TEXT("/Game/Advanced_Auto_Material"), TEXT("SM_boulder_01_1k")))))
	{
		Rocks.Add(Boulder);
	}
	TArray<UStaticMesh*> Shrubs;
	if (UStaticMesh* Shrub = Cast<UStaticMesh>(LoadAsset(FindExact(TEXT("/Game/Advanced_Auto_Material"), TEXT("SM_shrub_02_a")))))
	{
		Shrubs.Add(Shrub);
	}
	if (Cliffs.Num() == 0 && Rocks.Num() == 0 && Shrubs.Num() == 0 && !FogNiagara && !FogCascade && !SteamNiagara && !SteamCascade)
	{
		return;
	}

	FRandomStream Stream(24024);
	auto PlaceMesh = [this, &Stream, &Tints](UStaticMesh* Mesh, const FVector& Point)
	{
		if (!Mesh)
		{
			return;
		}
		UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this);
		Comp->SetMobility(EComponentMobility::Movable);
		Comp->SetStaticMesh(Mesh);
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Comp->SetCanEverAffectNavigation(false);
		if (Tints.Num() > 0)
		{
			Comp->SetMaterial(0, Tints[Stream.RandRange(0, Tints.Num() - 1)]);
		}
		Comp->SetupAttachment(GetRootComponent());
		Comp->RegisterComponent();
		const float Yaw = static_cast<float>(Stream.RandRange(0, 23) * 15);
		const float Scale = Stream.FRandRange(0.65f, 1.25f);
		Comp->SetWorldLocationAndRotation(Point, FRotator(0.f, Yaw, 0.f));
		Comp->SetWorldScale3D(FVector(Scale));
		Comp->SetMobility(EComponentMobility::Static);
		Dressing.Add(Comp);
	};

	int32 Placed = 0;
	for (int32 Attempt = 0; Attempt < 80 && Placed < 42; ++Attempt)
	{
		const FVector Point(Stream.FRandRange(-12000.f, 12000.f), Stream.FRandRange(-12000.f, 12000.f), -2.f);
		if (BlockedDressing(Point))
		{
			continue;
		}
		UStaticMesh* Mesh = nullptr;
		const int32 Kind = Stream.RandRange(0, 3);
		if (Kind == 0 && Cliffs.Num() > 0)
		{
			Mesh = Cliffs[Stream.RandRange(0, Cliffs.Num() - 1)];
		}
		else if (Kind == 1 && Shrubs.Num() > 0)
		{
			Mesh = Shrubs[0];
		}
		else if (Rocks.Num() > 0)
		{
			Mesh = Rocks[Stream.RandRange(0, Rocks.Num() - 1)];
		}
		else if (Cliffs.Num() > 0)
		{
			Mesh = Cliffs[Stream.RandRange(0, Cliffs.Num() - 1)];
		}
		if (!Mesh)
		{
			continue;
		}
		const int32 Before = Dressing.Num();
		PlaceMesh(Mesh, Point);
		if (Dressing.Num() > Before)
		{
			++Placed;
		}
	}

	auto PlaceFx = [this](UNiagaraSystem* Niagara, UParticleSystem* Cascade, const FVector& Point)
	{
		if (!Niagara && !Cascade)
		{
			return;
		}
		USceneComponent* Comp = nullptr;
		if (Niagara)
		{
			UNiagaraComponent* NiagaraComp = NewObject<UNiagaraComponent>(this);
			NiagaraComp->SetAsset(Niagara);
			NiagaraComp->SetAutoActivate(true);
			NiagaraComp->SetupAttachment(GetRootComponent());
			NiagaraComp->RegisterComponent();
			NiagaraComp->SetWorldLocation(Point);
			Comp = NiagaraComp;
		}
		else if (Cascade)
		{
			UParticleSystemComponent* Particle = NewObject<UParticleSystemComponent>(this);
			Particle->SetTemplate(Cascade);
			Particle->SetupAttachment(GetRootComponent());
			Particle->RegisterComponent();
			Particle->SetWorldLocation(Point);
			Comp = Particle;
		}
		if (Comp)
		{
			if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Comp))
			{
				Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			}
			Dressing.Add(Comp);
		}
	};

	FRandomStream FxStream(24025);
	for (int32 Index = 0; Index < 5; ++Index)
	{
		const FVector Point(FxStream.FRandRange(-11000.f, 11000.f), FxStream.FRandRange(-11000.f, 11000.f), 40.f);
		if (!BlockedDressing(Point))
		{
			PlaceFx(FogNiagara, FogCascade, Point);
		}
	}
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const FVector Point(FxStream.FRandRange(-11000.f, 11000.f), FxStream.FRandRange(-11000.f, 11000.f), 0.f);
		if (!BlockedDressing(Point))
		{
			PlaceFx(SteamNiagara, SteamCascade, Point);
		}
	}
	UE_LOG(LogTemp, Log, TEXT("KodAmbient Scatter=%d"), Dressing.Num());
}

void AKodMapLife::SpawnBurst(UNiagaraSystem* Niagara, UParticleSystem* Cascade, const FVector& Location, const FRotator& Rotation, float Scale) const
{
	if (!bAmbient)
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (Niagara)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Niagara, Location, Rotation, FVector(Scale), true, true, ENCPoolMethod::AutoRelease);
	}
	else if (Cascade)
	{
		UGameplayStatics::SpawnEmitterAtLocation(World, Cascade, Location, Rotation, FVector(Scale), true, EPSCPoolMethod::AutoRelease);
	}
}

void AKodMapLife::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Sim)
	{
		return;
	}
	if (!bBound)
	{
		Sim->OnUnitFired.AddDynamic(this, &AKodMapLife::HandleFired);
		Sim->OnUnitHit.AddDynamic(this, &AKodMapLife::HandleHit);
		Sim->OnUnitKilled.AddDynamic(this, &AKodMapLife::HandleKilled);
		Sim->OnCue.AddDynamic(this, &AKodMapLife::HandleCue);
		bBound = true;
	}
	if (!bAmbient || (!FootstepNiagara && !FootstepCascade))
	{
		return;
	}
	TArray<FKodSimEntityState> States;
	TArray<AActor*> Actors;
	Sim->CopyEntitySnapshot(States, Actors);
	const int32 Count = FMath::Min(States.Num(), Actors.Num());
	const float Now = World->GetTimeSeconds();
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FKodSimEntityState& State = States[Index];
		if (!State.bMobile || State.Health <= 0.f)
		{
			continue;
		}
		const FVector NowPos = State.Position;
		FVector* Prev = LastStep.Find(State.Id.Value);
		if (!Prev)
		{
			LastStep.Add(State.Id.Value, NowPos);
			continue;
		}
		if (FVector::DistSquared2D(*Prev, NowPos) < FMath::Square(140.f))
		{
			continue;
		}
		float* Next = NextStepTime.Find(State.Id.Value);
		if (Next && Now < *Next)
		{
			continue;
		}
		*Prev = NowPos;
		NextStepTime.Add(State.Id.Value, Now + 0.35f);
		SpawnBurst(FootstepNiagara, FootstepCascade, NowPos + FVector(0.f, 0.f, 8.f), FRotator::ZeroRotator, 0.6f);
	}
}

void AKodMapLife::HandleFired(AActor* Attacker, AActor* Target, int32 SimTick)
{
	(void)SimTick;
	if (!Attacker)
	{
		return;
	}
	FVector Dir = Attacker->GetActorForwardVector();
	if (Target)
	{
		Dir = Target->GetActorLocation() - Attacker->GetActorLocation();
	}
	Dir.Z = 0.f;
	if (!Dir.Normalize())
	{
		Dir = FVector::ForwardVector;
	}
	const FVector MuzzleAt = Attacker->GetActorLocation() + Dir * 70.f + FVector(0.f, 0.f, 72.f);
	SpawnBurst(Muzzle, nullptr, MuzzleAt, Dir.Rotation(), 0.45f);
}

void AKodMapLife::HandleHit(AActor* Attacker, AActor* Target, float Health, int32 TargetId)
{
	(void)Attacker;
	(void)Health;
	(void)TargetId;
	if (!Target)
	{
		return;
	}
	SpawnBurst(Hit, nullptr, Target->GetActorLocation() + FVector(0.f, 0.f, 60.f), FRotator::ZeroRotator, 0.7f);
}

void AKodMapLife::HandleKilled(AActor* Target, int32 TargetId)
{
	if (!Target)
	{
		return;
	}
	const FVector At = Target->GetActorLocation();
	UNiagaraSystem* Burst = ExplosionGround ? ExplosionGround : ExplosionMain;
	if ((TargetId & 1) && ExplosionSmoke)
	{
		Burst = ExplosionSmoke;
	}
	SpawnBurst(Burst, nullptr, At, FRotator::ZeroRotator, 1.f);
}

void AKodMapLife::HandleCue(FName CueId, FVector Location, bool bAtLocation)
{
	(void)bAtLocation;
	if (CueId != FName(TEXT("artillery_strike")))
	{
		return;
	}
	UNiagaraSystem* Burst = ExplosionMain ? ExplosionMain : ExplosionGround;
	SpawnBurst(Burst, nullptr, Location, FRotator::ZeroRotator, 1.4f);
	if (ExplosionSmoke)
	{
		SpawnBurst(ExplosionSmoke, nullptr, Location + FVector(0.f, 0.f, 40.f), FRotator::ZeroRotator, 1.f);
	}
}
