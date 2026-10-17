#include "Actors/KodBuilding.h"

#include "Animation/AnimEnums.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/Skeleton.h"
#include "UObject/TopLevelAssetPath.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Data/KodMeshCatalog.h"
#include "Engine/SkeletalMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "Warden/KodWardenPaths.h"

namespace
{
	FString WithAssetSuffix(const FString& Path)
	{
		FString Full = Path.TrimStartAndEnd();
		if (Full.IsEmpty() || Full.Contains(TEXT(".")))
		{
			return Full;
		}
		return Full + TEXT(".") + FPaths::GetBaseFilename(Full);
	}

	UObject* LoadByAssetName(FName AssetName, FTopLevelAssetPath ClassPath)
	{
		if (AssetName.IsNone() || !FModuleManager::Get().ModuleExists(TEXT("AssetRegistry")))
		{
			return nullptr;
		}
		FAssetRegistryModule& Assets = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		FARFilter Filter;
		Filter.ClassPaths.Add(ClassPath);
		Filter.bRecursivePaths = true;
		Filter.bRecursiveClasses = true;
		TArray<FAssetData> Found;
		Assets.Get().GetAssets(Filter, Found);
		for (const FAssetData& Asset : Found)
		{
			if (Asset.AssetName == AssetName)
			{
				return Asset.GetAsset();
			}
		}
		return nullptr;
	}

	bool IsTranslucentBlend(EBlendMode Blend)
	{
		return Blend == BLEND_Translucent
			|| Blend == BLEND_Additive
			|| Blend == BLEND_Modulate
			|| Blend == BLEND_AlphaComposite
			|| Blend == BLEND_AlphaHoldout;
	}
}

FString AKodBuilding::DepotClipToken(const FKodMeshRow& Row, FName ClipId) const
{
	if (ClipId == FName(KodDepotClipId::Idle) && !Row.IdleAnim.IsEmpty())
	{
		return Row.IdleAnim;
	}
	if (ClipId == FName(KodDepotClipId::Lower) && !Row.LowerAnim.IsEmpty())
	{
		return Row.LowerAnim;
	}
	if (ClipId == FName(KodDepotClipId::LoweredIdle) && !Row.LoweredIdleAnim.IsEmpty())
	{
		return Row.LoweredIdleAnim;
	}
	if (ClipId == FName(KodDepotClipId::Raise) && !Row.RaiseAnim.IsEmpty())
	{
		return Row.RaiseAnim;
	}
	if (ClipId == FName(KodDepotClipId::Death) && !Row.DeathAnim.IsEmpty())
	{
		return Row.DeathAnim;
	}
	if (ClipId == FName(KodDepotClipId::DeathLowered) && !Row.DeathLoweredAnim.IsEmpty())
	{
		return Row.DeathLoweredAnim;
	}
	return ClipId.ToString();
}

USkeletalMesh* AKodBuilding::ResolveDepotSkeleton(const FKodMeshRow& Row) const
{
	if (!Row.Skel.IsEmpty())
	{
		if (USkeletalMesh* FromRow = LoadObject<USkeletalMesh>(nullptr, *WithAssetSuffix(Row.Skel)))
		{
			return FromRow;
		}
	}
	return Cast<USkeletalMesh>(LoadByAssetName(FName(KodDepotClipId::Skeleton), USkeletalMesh::StaticClass()->GetClassPathName()));
}

UAnimSequence* AKodBuilding::ResolveDepotSequence(const FKodMeshRow& Row, FName ClipId) const
{
	const FString Token = DepotClipToken(Row, ClipId);
	if (Token.IsEmpty())
	{
		return nullptr;
	}
	if (Token.Contains(TEXT("/")))
	{
		if (UAnimSequence* FromPath = LoadObject<UAnimSequence>(nullptr, *WithAssetSuffix(Token)))
		{
			return FromPath;
		}
	}
	if (!Row.AnimFolder.IsEmpty())
	{
		FString Folder = Row.AnimFolder;
		if (!Folder.EndsWith(TEXT("/")))
		{
			Folder += TEXT("/");
		}
		const FString FolderPath = Folder + Token + TEXT(".") + Token;
		if (UAnimSequence* FromFolder = LoadObject<UAnimSequence>(nullptr, *FolderPath))
		{
			return FromFolder;
		}
	}
	if (!Row.Skel.IsEmpty())
	{
		const FString SkelFolder = FPaths::GetPath(Row.Skel);
		if (!SkelFolder.IsEmpty())
		{
			const FString Beside = SkelFolder / Token + TEXT(".") + Token;
			if (UAnimSequence* FromSkel = LoadObject<UAnimSequence>(nullptr, *Beside))
			{
				return FromSkel;
			}
		}
	}
	return Cast<UAnimSequence>(LoadByAssetName(FName(*Token), UAnimSequence::StaticClass()->GetClassPathName()));
}

void AKodBuilding::TryUseDepotSkeleton()
{
	FKodMeshRow Row;
	const FName MeshId = ActiveMeshId.IsNone() ? FName(KodWardenPaths::Id_SupplyDepot) : ActiveMeshId;
	KodMeshCatalogFindRow(MeshId, Row);
	USkeletalMesh* Skel = ResolveDepotSkeleton(Row);
	if (!Skel)
	{
		UE_LOG(LogTemp, Log, TEXT("KodDepot SkelMiss Id=%s Name=%s"), *MeshId.ToString(), KodDepotClipId::Skeleton);
		return;
	}
	if (BuildingSkel && BuildingSkel->GetSkeletalMeshAsset() == Skel && !BuildingSkel->bHiddenInGame)
	{
		return;
	}
	if (!BuildingSkel)
	{
		BuildingSkel = NewObject<USkeletalMeshComponent>(this, TEXT("BuildingSkel"));
		BuildingSkel->SetupAttachment(GetRootComponent());
		BuildingSkel->SetMobility(EComponentMobility::Movable);
		BuildingSkel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		BuildingSkel->RegisterComponent();
	}
	BuildingSkel->SetSkeletalMeshAsset(Skel);
	BuildingSkel->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	BuildingSkel->SetAnimInstanceClass(nullptr);
	BuildingSkel->bPauseAnims = false;
	BuildingSkel->SetHiddenInGame(false);
	BuildingSkel->SetVisibility(true);
	BuildingSkel->SetRelativeScale3D(BaseScale);
	BuildingSkel->SetRelativeRotation(FRotator::ZeroRotator);
	BuildingSkel->SetRelativeLocation(FVector(0.f, 0.f, MeshZ));
	if (BuildingMesh)
	{
		BuildingMesh->SetHiddenInGame(true);
		BuildingMesh->SetVisibility(false);
	}
	bDepotPlateApplied = false;
	ActiveDepotClip = NAME_None;
	UE_LOG(LogTemp, Log, TEXT("KodDepot Skel Id=%s Mesh=%s"), *MeshId.ToString(), *Skel->GetName());
}

void AKodBuilding::ApplyDepotPlateClip(UMeshComponent* Mesh)
{
	if (!Mesh)
	{
		return;
	}
	Mesh->SetCustomPrimitiveDataFloat(0, KodDepotClipId::ClipLocalZCm);
	const int32 MaterialCount = Mesh->GetNumMaterials();
	for (int32 Index = 0; Index < MaterialCount; ++Index)
	{
		UMaterialInterface* Material = Mesh->GetMaterial(Index);
		if (!Material)
		{
			continue;
		}
		if (IsTranslucentBlend(Material->GetBlendMode()))
		{
			UE_LOG(LogTemp, Log, TEXT("KodDepot PlateClip SkipTranslucent Index=%d"), Index);
			continue;
		}
		UMaterialInstanceDynamic* Mid = Mesh->CreateDynamicMaterialInstance(Index);
		if (!Mid)
		{
			continue;
		}
		Mid->SetScalarParameterValue(FName(KodDepotClipId::ClipLocalZ), KodDepotClipId::ClipLocalZCm);
	}
}

void AKodBuilding::ApplyDepotPawnCollision(bool bWalkable)
{
	if (bDepotCollisionInit && bDepotPawnOpen == bWalkable)
	{
		return;
	}
	bDepotCollisionInit = true;
	bDepotPawnOpen = bWalkable;
	if (!BuildingMesh)
	{
		return;
	}
	BuildingMesh->SetCollisionResponseToChannel(ECC_Pawn, bWalkable ? ECR_Ignore : ECR_Block);
	BuildingMesh->SetCanEverAffectNavigation(!bWalkable);
	UE_LOG(LogTemp, Log, TEXT("KodDepot Collision Walkable=%d PawnBlock=%d NavObstacle=%d"),
		bWalkable ? 1 : 0,
		bWalkable ? 0 : 1,
		bWalkable ? 0 : 1);
}

void AKodBuilding::SyncDepotClipPosition(const FKodStructureState& Structure)
{
	if (!DepotSequence || !BuildingSkel || BuildingSkel->bHiddenInGame)
	{
		return;
	}
	UAnimSingleNodeInstance* Node = BuildingSkel->GetSingleNodeInstance();
	if (!Node)
	{
		return;
	}
	const float ShownTicks = static_cast<float>(FMath::Max(0, Structure.DepotPoseTicks));
	float Seconds = ShownTicks * KodBuildTicks::SimDt;
	const float Length = DepotSequence->GetPlayLength();
	if (bDepotSequenceLoop && Length > KINDA_SMALL_NUMBER)
	{
		Seconds = FMath::Fmod(Seconds, Length);
	}
	else if (Length > 0.f)
	{
		Seconds = FMath::Min(Seconds, Length);
	}
	Node->SetPosition(Seconds, false);
}

void AKodBuilding::PlayDepotClip(const FKodMeshRow& Row, FName ClipId, bool bLoop)
{
	bDepotClipDriven = false;
	DepotSequence = nullptr;
	bDepotSequenceLoop = bLoop;
	if (!BuildingSkel || !BuildingSkel->GetSkeletalMeshAsset() || BuildingSkel->bHiddenInGame)
	{
		return;
	}
	UAnimSequence* Sequence = ResolveDepotSequence(Row, ClipId);
	USkeleton* MeshSkeleton = BuildingSkel->GetSkeletalMeshAsset()->GetSkeleton();
	USkeleton* ClipSkeleton = Sequence ? Sequence->GetSkeleton() : nullptr;
	if (!Sequence || (MeshSkeleton && ClipSkeleton && ClipSkeleton != MeshSkeleton))
	{
		UE_LOG(LogTemp, Log, TEXT("KodDepot ClipMiss Id=%s Clip=%s"), *ActiveMeshId.ToString(), *ClipId.ToString());
		return;
	}
	BuildingSkel->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	BuildingSkel->bPauseAnims = false;
	BuildingSkel->PlayAnimation(Sequence, bLoop);
	DepotSequence = Sequence;
	bDepotClipDriven = true;
	UE_LOG(LogTemp, Log, TEXT("KodDepot Clip Id=%s Clip=%s Loop=%d"), *ActiveMeshId.ToString(), *ClipId.ToString(), bLoop ? 1 : 0);
}

void AKodBuilding::SetDepotAmber(float Value)
{
	UMeshComponent* Body = BuildingMesh;
	if (BuildingSkel && !BuildingSkel->bHiddenInGame)
	{
		Body = BuildingSkel;
	}
	if (!Body)
	{
		return;
	}
	Body->SetScalarParameterValueOnMaterials(FName(KodDepotClipId::Amber), Value);
}

void AKodBuilding::PulseDepotAmber()
{
	AmberPhasesLeft = 2;
	bAmberLit = true;
	AmberPhaseLeft = 0.1f;
	SetDepotAmber(1.f);
	UE_LOG(LogTemp, Log, TEXT("KodDepot Amber Blinks=2"));
}

void AKodBuilding::TickDepotAmber(float DeltaSeconds)
{
	if (AmberPhasesLeft <= 0)
	{
		return;
	}
	AmberPhaseLeft -= DeltaSeconds;
	if (AmberPhaseLeft > 0.f)
	{
		return;
	}
	if (bAmberLit)
	{
		bAmberLit = false;
		SetDepotAmber(0.f);
		--AmberPhasesLeft;
		AmberPhaseLeft = 0.08f;
		return;
	}
	bAmberLit = true;
	SetDepotAmber(1.f);
	AmberPhaseLeft = 0.1f;
}

FVector AKodBuilding::DepotSocketLocation(FName SocketName) const
{
	USceneComponent* Body = BuildingMesh;
	if (BuildingSkel && !BuildingSkel->bHiddenInGame)
	{
		Body = BuildingSkel;
	}
	if (Body && !SocketName.IsNone() && Body->DoesSocketExist(SocketName))
	{
		return Body->GetSocketLocation(SocketName);
	}
	return GetActorLocation();
}

void AKodBuilding::HandleDepotNotify(const FKodDepotNotify& Notify, int32 AtTick)
{
	// Every notify name on the clip is eligible. AN_Depot_Slam and the rest need no special case.
	if (Notify.Chance < 0.999f)
	{
		const int32 Gate = FMath::Clamp(FMath::RoundToInt(Notify.Chance * 100.f), 0, 100);
		const int32 Roll = FMath::Abs((EntityId.Value * 17 + AtTick * 13) % 100);
		if (Roll >= Gate)
		{
			return;
		}
	}
	const bool bAmber = Notify.Name == FName(KodDepotClipId::LowerStart) || Notify.Name == FName(KodDepotClipId::RaiseStart);
	if (bAmber)
	{
		PulseDepotAmber();
	}
	const FKodDepotFxSlot* Slot = FKodDepotClipTable::Get().FindFx(Notify.Name);
	if (!Slot || Slot->IsEmpty())
	{
		return;
	}
	UNiagaraSystem* System = nullptr;
	USoundBase* Sound = nullptr;
	if (!Slot->Niagara.IsEmpty())
	{
		System = LoadObject<UNiagaraSystem>(nullptr, *WithAssetSuffix(Slot->Niagara));
	}
	if (!Slot->Sound.IsEmpty())
	{
		Sound = LoadObject<USoundBase>(nullptr, *WithAssetSuffix(Slot->Sound));
	}
	if (!System && !Sound)
	{
		static TSet<FName> Missed;
		if (!Missed.Contains(Notify.Name))
		{
			Missed.Add(Notify.Name);
			UE_LOG(LogTemp, Log, TEXT("KodDepot FxMiss Notify=%s"), *Notify.Name.ToString());
		}
		return;
	}
	TArray<FName> Sockets = Notify.Sockets;
	if (Sockets.Num() == 0)
	{
		Sockets.Add(NAME_None);
	}
	for (const FName SocketName : Sockets)
	{
		const FVector At = DepotSocketLocation(SocketName);
		if (System)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, System, At, FRotator::ZeroRotator, FVector(1.f), true, true);
		}
		if (Sound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, Sound, At);
		}
	}
}

void AKodBuilding::PumpDepotNotifies(const FKodStructureState& Structure)
{
	const FName ClipId = KodDepotActiveClip(Structure);
	const FKodDepotClip* Clip = FKodDepotClipTable::Get().FindClip(ClipId);
	if (!Clip)
	{
		return;
	}
	if (DepotFxPose != Structure.DepotPose)
	{
		DepotFxPose = Structure.DepotPose;
		DepotFxTicks = -1;
	}
	const int32 Now = FMath::Max(0, Structure.DepotPoseTicks);
	if (Now == DepotFxTicks)
	{
		return;
	}
	const int32 From = DepotFxTicks;
	const bool bWrapped = Now < From;
	DepotFxTicks = Now;
	for (const FKodDepotNotify& Notify : Clip->Notifies)
	{
		const int32 At = Notify.Tick();
		const bool bHit = bWrapped ? (At > From || At <= Now) : (At > From && At <= Now);
		if (bHit)
		{
			HandleDepotNotify(Notify, At);
		}
	}
}

void AKodBuilding::SyncDepotPresentation(const FKodStructureState& Structure, float DeltaSeconds)
{
	FKodMeshRow Row;
	const FName MeshId = ActiveMeshId.IsNone() ? FName(KodWardenPaths::Id_SupplyDepot) : ActiveMeshId;
	KodMeshCatalogFindRow(MeshId, Row);
	if (!bDepotSkelTried)
	{
		bDepotSkelTried = true;
		TryUseDepotSkeleton();
	}
	if (!bDepotPlateApplied)
	{
		if (BuildingSkel && !BuildingSkel->bHiddenInGame)
		{
			ApplyDepotPlateClip(BuildingSkel);
		}
		if (BuildingMesh)
		{
			ApplyDepotPlateClip(BuildingMesh);
		}
		bDepotPlateApplied = true;
		UE_LOG(LogTemp, Log, TEXT("KodDepot PlateClip Param=%s Threshold=%.0f CustomData0=%.0f TranslucentPass=0"),
			KodDepotClipId::ClipLocalZ,
			KodDepotClipId::ClipLocalZCm,
			KodDepotClipId::ClipLocalZCm);
	}
	const FName ClipId = KodDepotActiveClip(Structure);
	bool bLoop = false;
	if (const FKodDepotClip* Clip = FKodDepotClipTable::Get().FindClip(ClipId))
	{
		bLoop = Clip->bLoop;
	}
	if (ClipId != ActiveDepotClip)
	{
		ActiveDepotClip = ClipId;
		PlayDepotClip(Row, ClipId, bLoop);
		SyncDepotClipPosition(Structure);
	}
	else
	{
		SyncDepotClipPosition(Structure);
	}
	PumpDepotNotifies(Structure);
	ApplyDepotPawnCollision(Structure.bDepotWalkable);
	TickDepotAmber(DeltaSeconds);
}
