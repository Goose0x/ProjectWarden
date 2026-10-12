#include "Slice0/KodMapLife.h"

#include "Slice0/KodDressList.h"

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
#include "Actors/KodUnit.h"
#include "Data/KodUnitDefinition.h"
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
	ScatterDressing();
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
	KodDress::Apply(this, Dressing);
	if (!bAmbient)
	{
		for (USceneComponent* Component : Dressing)
		{
			if (Component)
			{
				Component->SetVisibility(false, true);
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("KodAmbient Scatter=%d"), Dressing.Num());
}


void AKodMapLife::TickSparks(float Now)
{
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	TArray<FKodStructureState> Structures;
	TArray<FKodSimEntityState> States;
	if (Sim)
	{
		Sim->CopyStructures(Structures);
		TArray<AActor*> Actors;
		Sim->CopyEntitySnapshot(States, Actors);
	}
	TInlineComponentArray<UNiagaraComponent*> Niagaras;
	GetComponents(Niagaras);
	for (UNiagaraComponent* Comp : Niagaras)
	{
		if (!Comp)
		{
			continue;
		}
		if (Comp->ComponentHasTag(FName(TEXT("KodFobSpark"))))
		{
			float* Next = NextSparkTime.Find(Comp->GetUniqueID());
			if (!Next)
			{
				NextSparkTime.Add(Comp->GetUniqueID(), Now + 4.f + static_cast<float>(Comp->GetUniqueID() % 5));
				continue;
			}
			if (Now >= *Next)
			{
				Comp->Activate(true);
				*Next = Now + 4.f + static_cast<float>(Comp->GetUniqueID() % 5);
			}
			continue;
		}
		if (!Comp->ComponentHasTag(FName(TEXT("KodDamageSpark"))))
		{
			continue;
		}
		bool bShow = false;
		const FVector At = Comp->GetComponentLocation();
		float BestDist = MAX_flt;
		FKodEntityId NearestId;
		for (const FKodStructureState& Structure : Structures)
		{
			if (Structure.TeamId != 1)
			{
				continue;
			}
			const float Dist = FVector::DistSquared2D(Structure.Position, At);
			if (Dist < BestDist)
			{
				BestDist = Dist;
				NearestId = Structure.Id;
			}
		}
		if (NearestId.IsValid() && BestDist <= FMath::Square(900.f))
		{
			for (const FKodSimEntityState& State : States)
			{
				if (State.Id != NearestId || State.MaxHealth <= 0.f || State.Health <= 0.f)
				{
					continue;
				}
				bShow = State.Health <= State.MaxHealth * 0.5f;
				break;
			}
		}
		Comp->SetVisibility(bShow);
		if (bShow && !Comp->IsActive())
		{
			Comp->Activate(true);
		}
		else if (!bShow && Comp->IsActive())
		{
			Comp->Deactivate();
		}
	}
	TInlineComponentArray<UParticleSystemComponent*> Particles;
	GetComponents(Particles);
	for (UParticleSystemComponent* Comp : Particles)
	{
		if (!Comp || !Comp->ComponentHasTag(FName(TEXT("KodFobSpark"))))
		{
			continue;
		}
		float* Next = NextSparkTime.Find(Comp->GetUniqueID());
		if (!Next)
		{
			NextSparkTime.Add(Comp->GetUniqueID(), Now + 4.f + static_cast<float>(Comp->GetUniqueID() % 5));
			continue;
		}
		if (Now >= *Next)
		{
			Comp->Activate(true);
			*Next = Now + 4.f + static_cast<float>(Comp->GetUniqueID() % 5);
		}
	}
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
	if (bAmbient)
	{
		TickSparks(World->GetTimeSeconds());
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
	if (const AKodUnit* Unit = Cast<AKodUnit>(Target))
	{
		const UKodUnitDefinition* Def = Unit->GetDefinition();
		if (!Def || Def->UsesOrganicDeath())
		{
			return;
		}
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
