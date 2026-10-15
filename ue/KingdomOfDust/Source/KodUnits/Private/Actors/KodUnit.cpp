#include "Actors/KodUnit.h"
#include "Game/KodCameraSettings.h"
#include "Game/KodCollision.h"
#include "Actors/KodJadeiteVisual.h"
#include "Game/KodJadeitePalette.h"
#include "Animation/KodUnitAnimInstance.h"
#include "Data/KodFactionDefinition.h"
#include "Data/KodMeshCatalog.h"
#include "Data/KodUnitDefinition.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Data/KodWeaponDefinition.h"
#include "Components/KodAbilitySystemComponent.h"
#include "Components/KodMoveComponent.h"
#include "Components/KodAttackComponent.h"
#include "Attributes/KodCombatAttributeSet.h"
#include "Sim/KodSimSubsystem.h"
#include "Slice0/KodSlice0Bootstrap.h"
#include "Warden/KodWardenPaths.h"
#include "Game/KodPlayerController.h"
#include "Game/KodPortraitStage.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/HitResult.h"
#include "Components/PointLightComponent.h"
#include "Animation/AnimEnums.h"
#include "Animation/AnimInstance.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Math/RotationMatrix.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Paths.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr const TCHAR* EngineCubePath = TEXT("/Engine/BasicShapes/Cube.Cube");
	constexpr const TCHAR* EngineSpherePath = TEXT("/Engine/BasicShapes/Sphere.Sphere");
	constexpr const TCHAR* EngineConePath = TEXT("/Engine/BasicShapes/Cone.Cone");
	constexpr const TCHAR* EngineCylinderPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
	constexpr const TCHAR* BasicShapeMaterialPath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");
	const FVector EngineCubeScale(0.8f, 0.8f, KodCameraSettings::WorkerBodyUU / 100.f);
	constexpr double LifeReferenceHeightCm = 170.0;
	// Real move speed is 450. A 1 uu idle quantize snap is ~60-150 uu/s depending on frame
	// time, so the threshold sits above that and still sees a walk.
	constexpr float LifeMoveSpeedThreshold = 200.f;
	constexpr float MuzzleFlashSeconds = 0.06f;
	constexpr float TracerSeconds = 0.08f;
	constexpr float HitReactSeconds = 0.2f;
	constexpr float DeathSinkSeconds = 1.5f;
	constexpr float DeathSinkDepthCm = 220.f;
	constexpr float MuzzleForwardCm = 62.f;
	constexpr float MuzzleUpCm = 72.f;
	constexpr float SkeletalCapsuleRadius = 34.f;
	constexpr float SkeletalCapsuleHalfHeight = 90.f;
	constexpr float DefaultCapsuleRadius = 34.f;
	constexpr float DefaultCapsuleHalfHeight = 88.f;
	constexpr float ShotNotifyFallbackSeconds = 0.5f;
	constexpr float HitJiggleCm = 16.f;
	constexpr float TracerRadiusScale = 0.28f;

	bool IsEngineCubeMesh(const UStaticMesh* BodyStaticMesh)
	{
		return BodyStaticMesh && BodyStaticMesh->GetPathName().Contains(TEXT("/Engine/BasicShapes/Cube"));
	}

	bool ExpectsRangerRifle(const UKodUnitDefinition* Def)
	{
		if (!Def)
		{
			return false;
		}
		const FName MarineId(KodWardenPaths::Id_Marine);
		const FName RifleId(KodWardenPaths::Id_RangerRifle);
		if (Def->DefinitionId == MarineId || Def->GetFName() == MarineId)
		{
			return true;
		}
		if (Def->PrimaryWeapon.IsNull())
		{
			return false;
		}
		const FSoftObjectPath Path = Def->PrimaryWeapon.ToSoftObjectPath();
		if (Path.GetAssetFName() == RifleId)
		{
			return true;
		}
		if (Path.GetSubPathString().Equals(KodWardenPaths::Id_RangerRifle, ESearchCase::CaseSensitive))
		{
			return true;
		}
		if (const UKodWeaponDefinition* Live = Def->PrimaryWeapon.Get())
		{
			return Live->DefinitionId == RifleId || Live->GetFName() == RifleId;
		}
		return false;
	}

	void ConfigureFxPrimitive(UPrimitiveComponent* Primitive)
	{
		if (!Primitive)
		{
			return;
		}
		Primitive->SetMobility(EComponentMobility::Movable);
		Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Primitive->SetGenerateOverlapEvents(false);
		Primitive->SetCanEverAffectNavigation(false);
		Primitive->SetCastShadow(false);
		Primitive->ComponentTags.AddUnique(FName(TEXT("KodFx")));
	}

	UMaterialInstanceDynamic* MakeShapeMID(UObject* Outer, const FLinearColor& Tint)
	{
		UMaterialInterface* Source = LoadObject<UMaterialInterface>(nullptr, BasicShapeMaterialPath);
		if (!Source || !Outer)
		{
			return nullptr;
		}
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Source, Outer);
		if (!MID)
		{
			return nullptr;
		}
		MID->SetVectorParameterValue(TEXT("Color"), Tint);
		MID->SetVectorParameterValue(TEXT("BaseColor"), Tint);
		return MID;
	}
}

AKodUnit::AKodUnit()
{
	PrimaryActorTick.bCanEverTick = true;
	// Best-effort late tick. Bob and facing are UnitMesh relative offsets, so they
	// still show if this runs before the sim sync (one frame late) or after it.
	// Neither write touches FKodSimEntityState or ComputeIdleHash.
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;

	FriendlyTeamColor = FLinearColor(FColor(0xD2, 0xC4, 0xB1));
	HostileTeamColor = FLinearColor(FColor(0xB3, 0x26, 0x1E));

	AbilitySystemComponent = CreateDefaultSubobject<UKodAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystemComponent->SetIsReplicated(true);

	CombatAttributes = CreateDefaultSubobject<UKodCombatAttributeSet>(TEXT("CombatAttributes"));

	MoveComponent = CreateDefaultSubobject<UKodMoveComponent>(TEXT("KodMove"));
	AttackComponent = CreateDefaultSubobject<UKodAttackComponent>(TEXT("KodAttack"));

	// Engine cube so smoke-spawned units are visible until ApplyDefinition resolves a mesh.
	// Capsule stays the movement body and is not retuned here.
	// Query-only Pawn: ECC_Pawn click-select hits the visible shape; Visibility is ignored
	// (same as the pawn capsule) so ground traces still pass through.
	UnitMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("UnitMesh"));
	UnitMesh->SetupAttachment(GetCapsuleComponent());
	UnitMesh->SetCanEverAffectNavigation(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(EngineCubePath);
	if (CubeFinder.Succeeded())
	{
		UnitMesh->SetStaticMesh(CubeFinder.Object);
	}
	UnitMesh->SetRelativeScale3D(EngineCubeScale);
	KeepSelectCollision();

	// Character skeletal mesh stays hidden until a definition skeletal mesh actually loads.
	// Its collision profile is left at the Character default so it does not join select traces.
	if (USkeletalMeshComponent* Body = GetMesh())
	{
		Body->SetCanEverAffectNavigation(false);
		Body->SetHiddenInGame(true);
		Body->SetVisibility(false);
	}

	// CharacterMovement may exist for animation/capsule but is NOT match-state truth (sim owns pose).
	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		CMC->bOrientRotationToMovement = false;
		CMC->MaxWalkSpeed = 0.f;
		CMC->BrakingDecelerationWalking = 0.f;
		CMC->SetMovementMode(MOVE_None);
	}
}

void AKodUnit::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		if (UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>())
		{
			EntityId = Sim->RegisterEntity(this);
			BindSimEvents(Sim);
		}
	}

	// Prefer assigned soft ref; else try Warden bare id from soft path asset name.
	UKodUnitDefinition* Def = GetDefinition();
	if (!Def && !Definition.IsNull())
	{
		Def = Cast<UKodUnitDefinition>(UKodSlice0Bootstrap::ResolveDefinition(Definition.ToSoftObjectPath().GetAssetFName(), this));
	}
	if (Def)
	{
		ApplyDefinition(Def);
	}

	if (!GetDefinition())
	{
		SnapToGround();
	}
	VisualYaw = GetActorRotation().Yaw;
	LastPresentationLocation = GetActorLocation();
	BodyRestRelativeLocation = UnitMesh ? UnitMesh->GetRelativeLocation() : FVector::ZeroVector;
	BindJadeitePalette();
	RefreshJadeiteEmissive();
	UE_LOG(LogTemp, Log, TEXT("KodUnitLife %s Bob=%.1f Turn=%.0f"),
		*GetName(),
		BobAmplitudeCm,
		TurnRateDegreesPerSecond);
}

void AKodUnit::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HitStopUntil > 0.f)
	{
		const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
		if (Now < HitStopUntil)
		{
			return;
		}
		CustomTimeDilation = 1.f;
		HitStopUntil = -1.f;
	}
	UpdateCargoPresentation();
	const float Dt = FMath::Max(0.f, DeltaSeconds);
	if (AdvanceDeathSink(Dt))
	{
		return;
	}
	if (bAwaitShotNotify)
	{
		ShotNotifyWait += Dt;
		if (ShotNotifyWait >= ShotNotifyFallbackSeconds)
		{
			bAwaitShotNotify = false;
			ShowMuzzleAndTracer(PendingMuzzleTarget.Get(), false, FVector::ZeroVector);
		}
	}
	AdvanceAttackPresentation(Dt);
	UpdateLifePresentation(Dt);
	UpdateStimCue();
}

void AKodUnit::SetForceCubeBody(bool bInForce)
{
	bForceCubeBody = bInForce;
}

void AKodUnit::ApplyHostileCubeTint()
{
	bForceCubeBody = true;
	if (!UnitMesh || !IsEngineCubeMesh(UnitMesh->GetStaticMesh()))
	{
		MountCubePlaceholder();
	}
	if (!UnitMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("KodUnitHostile %s red tint failed (no UnitMesh)"), *GetName());
		return;
	}

	UMaterialInterface* Source = LoadObject<UMaterialInterface>(nullptr, BasicShapeMaterialPath);
	if (!Source)
	{
		Source = UnitMesh->GetMaterial(0);
	}
	if (!Source)
	{
		UE_LOG(LogTemp, Warning, TEXT("KodUnitHostile %s red tint failed (BasicShapeMaterial missing)"), *GetName());
		return;
	}

	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Source, UnitMesh);
	if (!MID)
	{
		UE_LOG(LogTemp, Warning, TEXT("KodUnitHostile %s red tint failed (MID)"), *GetName());
		return;
	}

	// BasicShapeMaterial uses Color. BaseColor covers a parent that uses the other name.
	// Slot 0 only — no SetOverlayMaterial, so the selection rim (stencil 1) stays free.
	const FLinearColor HostileRed = HostileTeamColor;
	MID->SetVectorParameterValue(TEXT("Color"), HostileRed);
	MID->SetVectorParameterValue(TEXT("BaseColor"), HostileRed);
	UnitMesh->SetMaterial(0, MID);
	BodyTintMID = MID;
	BodyRestColor = HostileRed;
	bTeamColorApplied = false;
}

namespace
{
	bool MaterialHasVectorParam(const UMaterialInterface* Source, FName ParamName)
	{
		if (!Source || ParamName.IsNone())
		{
			return false;
		}
		TArray<FMaterialParameterInfo> Infos;
		TArray<FGuid> Ids;
		Source->GetAllParameterInfoOfType(EMaterialParameterType::Vector, Infos, Ids);
		for (const FMaterialParameterInfo& Info : Infos)
		{
			if (Info.Name == ParamName)
			{
				return true;
			}
		}
		return false;
	}
}

const TCHAR* AKodUnit::GetMountedBodyName() const
{
	if (bSkeletalBody)
	{
		return TEXT("Skeletal");
	}
	if (UnitMesh && IsEngineCubeMesh(UnitMesh->GetStaticMesh()))
	{
		return TEXT("Cube");
	}
	return TEXT("StaticMesh");
}

void AKodUnit::WriteBodyTint(const FLinearColor& Tint)
{
	if (!BodyTintMID)
	{
		return;
	}
	if (bTeamColorApplied)
	{
		BodyTintMID->SetVectorParameterValue(TEXT("TeamColor"), Tint);
	}
	BodyTintMID->SetVectorParameterValue(TEXT("Color"), Tint);
	BodyTintMID->SetVectorParameterValue(TEXT("BaseColor"), Tint);
}

void AKodUnit::ApplyColorTintFallback(UMeshComponent* VisualBody, const FLinearColor& Tint)
{
	if (!VisualBody)
	{
		return;
	}

	UMaterialInterface* Source = nullptr;
	if (!bSkeletalBody && UnitMesh && IsEngineCubeMesh(UnitMesh->GetStaticMesh()))
	{
		Source = LoadObject<UMaterialInterface>(nullptr, BasicShapeMaterialPath);
	}
	if (!Source)
	{
		Source = VisualBody->GetMaterial(0);
	}
	if (!Source)
	{
		return;
	}

	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Source, this);
	if (!MID)
	{
		return;
	}
	MID->SetVectorParameterValue(TEXT("Color"), Tint);
	MID->SetVectorParameterValue(TEXT("BaseColor"), Tint);
	VisualBody->SetMaterial(0, MID);
	BodyTintMID = MID;
	BodyRestColor = Tint;
	bTeamColorApplied = false;
}

void AKodUnit::UpdateStimCue()
{
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	FKodSimEntityState State;
	const bool bStim = Sim && Sim->TryGetState(EntityId, State) && State.AbilityTicksRemaining > 0;
	if (!bStim)
	{
		if (bStimCue && BodyTintMID)
		{
			if (bTeamColorApplied)
			{
				BodyTintMID->SetVectorParameterValue(TEXT("TeamColor"), BodyRestColor);
			}
			else
			{
				BodyTintMID->SetVectorParameterValue(TEXT("Color"), BodyRestColor);
				BodyTintMID->SetVectorParameterValue(TEXT("BaseColor"), BodyRestColor);
			}
		}
		bStimCue = false;
		return;
	}
	// Team 0 keeps its authored material when no dynamic tint exists. Never create one here.
	if (!BodyTintMID || !World)
	{
		return;
	}
	bStimCue = true;
	const float Pulse = 0.5f + 0.5f * FMath::Sin(World->GetTimeSeconds() * 10.f);
	const FLinearColor Stim(1.f, 0.42f, 0.08f);
	const FLinearColor Mix = FMath::Lerp(BodyRestColor, Stim, 0.35f + 0.5f * Pulse);
	if (bTeamColorApplied)
	{
		BodyTintMID->SetVectorParameterValue(TEXT("TeamColor"), Mix);
	}
	else
	{
		BodyTintMID->SetVectorParameterValue(TEXT("Color"), Mix);
		BodyTintMID->SetVectorParameterValue(TEXT("BaseColor"), Mix);
	}
}

void AKodUnit::ApplyTeamColor()
{
	const FLinearColor Tint = (TeamId == 0) ? FriendlyTeamColor : HostileTeamColor;
	UMeshComponent* VisualBody = nullptr;
	if (bSkeletalBody)
	{
		if (USkeletalMeshComponent* SkelBody = GetMesh())
		{
			if (!SkelBody->bHiddenInGame)
			{
				VisualBody = SkelBody;
			}
		}
	}
	if (!VisualBody)
	{
		VisualBody = UnitMesh;
	}
	if (!VisualBody)
	{
		return;
	}

	bTeamColorApplied = false;
	BodyTintMID = nullptr;
	const FName TeamParam(TEXT("TeamColor"));
	const int32 SlotCount = VisualBody->GetNumMaterials();
	for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
	{
		UMaterialInterface* Source = VisualBody->GetMaterial(SlotIndex);
		if (!MaterialHasVectorParam(Source, TeamParam))
		{
			continue;
		}
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Source, this);
		if (!MID)
		{
			continue;
		}
		MID->SetVectorParameterValue(TeamParam, Tint);
		MID->SetVectorParameterValue(TEXT("EmissiveColor"), Tint * 0.35f);
		MID->SetScalarParameterValue(TEXT("RimStrength"), 0.22f);
		VisualBody->SetMaterial(SlotIndex, MID);
		bTeamColorApplied = true;
		if (!BodyTintMID)
		{
			BodyTintMID = MID;
			BodyRestColor = Tint;
		}
	}
	if (!bTeamColorApplied && TeamId != 0)
	{
		ApplyColorTintFallback(VisualBody, Tint);
	}
	if (!WeaponMeshComp)
	{
		return;
	}
	const int32 WeaponSlots = WeaponMeshComp->GetNumMaterials();
	for (int32 SlotIndex = 0; SlotIndex < WeaponSlots; ++SlotIndex)
	{
		UMaterialInterface* Source = WeaponMeshComp->GetMaterial(SlotIndex);
		if (!MaterialHasVectorParam(Source, TeamParam))
		{
			continue;
		}
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Source, this);
		if (!MID)
		{
			continue;
		}
		MID->SetVectorParameterValue(TeamParam, Tint);
		WeaponMeshComp->SetMaterial(SlotIndex, MID);
	}
}

float AKodUnit::GetScaledBobAmplitudeCm() const
{
	const float Base = FMath::Max(0.f, BobAmplitudeCm);
	if (!UnitMesh)
	{
		return Base;
	}
	const UStaticMesh* BodyStaticMesh = UnitMesh->GetStaticMesh();
	if (!BodyStaticMesh)
	{
		return Base;
	}

	// Parent-space offset, on top of RelativeScale3D. Scale the cm amplitude by
	// world height / 170 so a non-human body bobs in proportion. The cube (100 cm
	// mesh at 1.7) and the auto-scaled Ranger (~170 cm) both stay near BobAmplitudeCm.
	const double MeshHeight = BodyStaticMesh->GetBounds().BoxExtent.Z * 2.0;
	const double WorldHeight = MeshHeight * UnitMesh->GetRelativeScale3D().Z;
	if (!FMath::IsFinite(WorldHeight) || WorldHeight <= static_cast<double>(KINDA_SMALL_NUMBER))
	{
		return Base;
	}
	const double Scale = WorldHeight / LifeReferenceHeightCm;
	if (!FMath::IsFinite(Scale) || Scale <= 0.0)
	{
		return Base;
	}
	return Base * static_cast<float>(Scale);
}

void AKodUnit::UpdateCargoPresentation()
{
	int32 Jadeite = 0;
	int32 Luminene = 0;
	if (const UWorld* World = GetWorld())
	{
		if (const UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>())
		{
			FKodSimEntityState State;
			if (EntityId.IsValid() && Sim->TryGetState(EntityId, State))
			{
				Jadeite = State.CargoJadeite;
				Luminene = State.CargoLuminene;
			}
		}
	}

	if (Jadeite == ShownCargoJadeite && Luminene == ShownCargoLuminene)
	{
		return;
	}
	ShownCargoJadeite = Jadeite;
	ShownCargoLuminene = Luminene;

	const bool bShow = Jadeite > 0 || Luminene > 0;
	if (!bShow)
	{
		if (CargoMesh)
		{
			CargoMesh->SetVisibility(false, true);
			CargoMesh->SetHiddenInGame(true);
		}
		return;
	}

	if (!CargoMesh)
	{
		CargoMesh = NewObject<UStaticMeshComponent>(this, TEXT("CargoMesh"));
		if (!CargoMesh)
		{
			return;
		}
		CargoMesh->ComponentTags.Add(FName(TEXT("KodFx")));
		CargoMesh->SetMobility(EComponentMobility::Movable);
		USceneComponent* AttachParent = GetCapsuleComponent() ? static_cast<USceneComponent*>(GetCapsuleComponent()) : GetRootComponent();
		CargoMesh->SetupAttachment(AttachParent);
		CargoMesh->SetRelativeLocation(FVector(-20.f, 0.f, 80.f));
		CargoMesh->SetRelativeScale3D(FVector(0.18f));
		CargoMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		CargoMesh->SetGenerateOverlapEvents(false);
		CargoMesh->SetCanEverAffectNavigation(false);
		CargoMesh->RegisterComponent();
	}

	const bool bGas = Luminene > 0 && Jadeite <= 0;
	UStaticMesh* Payload = nullptr;
	if (const UKodFactionDefinition* Faction = UKodSlice0Bootstrap::ResolveFaction(FName(KodWardenPaths::Id_USA), const_cast<AKodUnit*>(this)))
	{
		const TSoftObjectPtr<UStaticMesh>& Soft = bGas ? Faction->CarryMeshGas : Faction->CarryMeshCrystal;
		if (!Soft.IsNull())
		{
			Payload = Soft.LoadSynchronous();
		}
	}
	if (!Payload)
	{
		Payload = LoadObject<UStaticMesh>(nullptr, bGas
			? TEXT("/Engine/BasicShapes/Cylinder.Cylinder")
			: TEXT("/Engine/BasicShapes/Cube.Cube"));
	}
	if (Payload && CargoMesh)
	{
		CargoMesh->SetStaticMesh(Payload);
		CargoMesh->SetRelativeScale3D(bGas ? FVector(0.12f, 0.12f, 0.28f) : FVector(0.18f));
		if (USkeletalMeshComponent* Body = GetMesh())
		{
			if (Body->DoesSocketExist(FName(TEXT("hand_r"))))
			{
				CargoMesh->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, FName(TEXT("hand_r")));
			}
			else if (Body->DoesSocketExist(FName(TEXT("back"))))
			{
				CargoMesh->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, FName(TEXT("back")));
			}
		}
	}
	const FLinearColor Tint = bGas
		? KodResourceColors::LumineneCore()
		: KodJadeiteVisual::TunedJadeBase(GetWorld());
	if (!CargoMID)
	{
		if (UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr, BasicShapeMaterialPath))
		{
			CargoMID = UMaterialInstanceDynamic::Create(Parent, this);
		}
	}
	if (CargoMID && CargoMesh)
	{
		CargoMID->SetVectorParameterValue(TEXT("Color"), Tint);
		CargoMID->SetVectorParameterValue(TEXT("BaseColor"), Tint);
		CargoMesh->SetMaterial(0, CargoMID);
	}
	CargoMesh->SetVisibility(true, true);
	CargoMesh->SetHiddenInGame(false);
}

void AKodUnit::UpdateLifePresentation(float DeltaSeconds)
{
	const float Dt = FMath::Max(0.f, DeltaSeconds);
	const FVector Loc = GetActorLocation();
	const float Speed = (Dt > KINDA_SMALL_NUMBER) ? (FVector::Dist2D(Loc, LastPresentationLocation) / Dt) : 0.f;
	LastPresentationLocation = Loc;
	const bool bMoving = Speed >= LifeMoveSpeedThreshold;

	float DesiredYaw = VisualYaw;
	if (UWorld* World = GetWorld())
	{
		if (UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>())
		{
			FKodSimEntityState State;
			if (EntityId.IsValid() && Sim->TryGetState(EntityId, State))
			{
				// Sim already aims YawDegrees along the move, and at the attack target in range.
				DesiredYaw = State.YawDegrees;
			}
		}
	}

	if (TurnRateDegreesPerSecond <= 0.f)
	{
		VisualYaw = DesiredYaw;
	}
	else
	{
		const FRotator Smoothed = FMath::RInterpConstantTo(
			FRotator(0.f, VisualYaw, 0.f),
			FRotator(0.f, DesiredYaw, 0.f),
			Dt,
			TurnRateDegreesPerSecond);
		VisualYaw = Smoothed.Yaw;
	}

	if (bSkeletalBody)
	{
		// Facing is the offset plus the fast visual yaw. A held mesh (no matching anim)
		// still bobs. A live AnimBP keeps its own pose.
		if (USkeletalMeshComponent* SkelBody = GetMesh())
		{
			FRotator SkelRel = SkelBody->GetRelativeRotation();
			SkelRel.Yaw = MountedMeshYawOffset + FMath::UnwindDegrees(VisualYaw - GetActorRotation().Yaw);
			SkelBody->SetRelativeRotation(SkelRel, false, nullptr, ETeleportType::None);
			if (bMeshAnimHeld && !bDeathSinking)
			{
				const float TargetWeight = bMoving ? 1.f : 0.f;
				const float Ease = FMath::Max(0.f, BobEaseSpeed);
				BobWeight = (Ease <= 0.f || Dt <= 0.f) ? TargetWeight : FMath::FInterpTo(BobWeight, TargetWeight, Dt, Ease);
				float BobZ = 0.f;
				if (bMoving || BobWeight > KINDA_SMALL_NUMBER)
				{
					BobTime += Dt;
					const float Frequency = FMath::Max(0.f, BobFrequencyHz);
					const float Angle = BobTime * Frequency * 2.f * static_cast<float>(UE_PI);
					BobZ = GetScaledBobAmplitudeCm() * BobWeight * FMath::Sin(Angle);
				}
				SkelBody->SetRelativeLocation(BodyRestRelativeLocation + FVector(0.f, 0.f, BobZ));
			}
		}
		return;
	}

	if (!UnitMesh)
	{
		return;
	}

	// Sim sync writes actor yaw. Keep the eased facing on the body mesh so it
	// survives whichever tick runs last. If we run first, the offset is one
	// frame behind the sim yaw the sync is about to write.
	FRotator BodyRelRotation = UnitMesh->GetRelativeRotation();
	BodyRelRotation.Yaw = FMath::UnwindDegrees(VisualYaw - GetActorRotation().Yaw);
	UnitMesh->SetRelativeRotation(BodyRelRotation, false, nullptr, ETeleportType::None);

	if (bDeathSinking)
	{
		SetBodyRelativeLocation(BodyRestRelativeLocation);
		return;
	}

	const float TargetWeight = bMoving ? 1.f : 0.f;
	const float Ease = FMath::Max(0.f, BobEaseSpeed);
	BobWeight = (Ease <= 0.f || Dt <= 0.f) ? TargetWeight : FMath::FInterpTo(BobWeight, TargetWeight, Dt, Ease);

	if (!bMoving && BobWeight <= KINDA_SMALL_NUMBER)
	{
		BobWeight = 0.f;
		BobTime = 0.f;
		SetBodyRelativeLocation(BodyRestRelativeLocation);
		bBobOffsetApplied = false;
		return;
	}

	BobTime += Dt;
	const float Frequency = FMath::Max(0.f, BobFrequencyHz);
	const float Angle = BobTime * Frequency * 2.f * static_cast<float>(UE_PI);
	const float Z = GetScaledBobAmplitudeCm() * BobWeight * FMath::Sin(Angle);
	SetBodyRelativeLocation(BodyRestRelativeLocation + FVector(0.f, 0.f, Z));
	bBobOffsetApplied = true;
}

UAbilitySystemComponent* AKodUnit::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

UKodUnitDefinition* AKodUnit::GetDefinition() const
{
	if (UKodUnitDefinition* Loaded = Definition.LoadSynchronous())
	{
		return Loaded;
	}
	if (!Definition.IsNull())
	{
		return UKodSlice0Bootstrap::ResolveUnit(Definition.ToSoftObjectPath().GetAssetFName(), const_cast<AKodUnit*>(this));
	}
	return nullptr;
}

void AKodUnit::ApplyDefinition(UKodUnitDefinition* Def)
{
	if (!Def)
	{
		return;
	}

	Definition = Def;
	if (!Def->IdleAnim.IsNull() || !Def->TalkAnim.IsNull())
	{
		UAnimSequence* Idle = Def->IdleAnim.IsNull() ? nullptr : Def->IdleAnim.LoadSynchronous();
		UAnimSequence* Talk = Def->TalkAnim.IsNull() ? nullptr : Def->TalkAnim.LoadSynchronous();
		if (Idle || Talk)
		{
			UKodPortraitHooks* Hooks = FindComponentByClass<UKodPortraitHooks>();
			if (!Hooks)
			{
				Hooks = NewObject<UKodPortraitHooks>(this, TEXT("PortraitHooks"));
				Hooks->RegisterComponent();
			}
			Hooks->SetSequences(Idle, Talk);
		}
	}
	if (!Def->PortraitMesh.IsNull())
	{
		if (USkeletalMesh* Bust = Def->PortraitMesh.LoadSynchronous())
		{
			USkeletalMeshComponent* PortraitComp = FindComponentByTag<USkeletalMeshComponent>(FName(TEXT("PortraitMesh")));
			if (!PortraitComp)
			{
				PortraitComp = NewObject<USkeletalMeshComponent>(this, TEXT("PortraitBust"));
				PortraitComp->ComponentTags.Add(FName(TEXT("PortraitMesh")));
				PortraitComp->SetSkeletalMeshAsset(Bust);
				PortraitComp->SetVisibility(false);
				PortraitComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				PortraitComp->SetupAttachment(GetRootComponent());
				PortraitComp->RegisterComponent();
			}
			else
			{
				PortraitComp->SetSkeletalMeshAsset(Bust);
			}
		}
	}

	float WeaponDamage = 0.f;
	float WeaponRange = 0.f;
	float WeaponCooldown = 0.f;
	if (ExpectsRangerRifle(Def))
	{
		// Disk asset when it loads. A miss used to leave damage and range at 0, so
		// StepEntityAttack's hitscan check never ran and no KodSim Hit was logged.
		UKodSlice0Bootstrap::ResolveRangerRifleCombatStats(WeaponDamage, WeaponRange, WeaponCooldown, GetWorld());
	}
	else if (!Def->PrimaryWeapon.IsNull())
	{
		UKodWeaponDefinition* Weapon = Def->PrimaryWeapon.Get();
		if (!Weapon)
		{
			Weapon = Def->PrimaryWeapon.LoadSynchronous();
		}
		if (!Weapon)
		{
			const FSoftObjectPath Path = Def->PrimaryWeapon.ToSoftObjectPath();
			Weapon = UKodSlice0Bootstrap::ResolveWeapon(Path.GetAssetFName(), this);
			if (!Weapon && !Path.GetSubPathString().IsEmpty())
			{
				Weapon = UKodSlice0Bootstrap::ResolveWeapon(FName(*Path.GetSubPathString()), this);
			}
		}
		if (Weapon)
		{
			WeaponDamage = Weapon->Damage;
			WeaponRange = Weapon->Range;
			WeaponCooldown = Weapon->CooldownSeconds;
		}
	}

	// HP from DataAsset only — never hardcode in Tick / BeginPlay without reading DA.
	if (CombatAttributes)
	{
		CombatAttributes->SetMaxHealth(Def->MaxHealth);
		CombatAttributes->SetHealth(Def->MaxHealth);
	}

	if (UWorld* World = GetWorld())
	{
		if (UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>())
		{
			if (!EntityId.IsValid())
			{
				EntityId = Sim->RegisterEntity(this);
			}
			const float Accept = MoveComponent ? MoveComponent->AcceptanceRadius : 50.f;
			const FName DefId = Def->DefinitionId.IsNone() ? Def->GetFName() : Def->DefinitionId;
			// A cooked Dozer uasset from before this step still gathers. The flag is the authored switch.
			const bool bGather = Def->bCanGather || DefId == FName(KodWardenPaths::Id_Dozer);
			Sim->ConfigureEntity(
				EntityId,
				DefId,
				Def->MaxHealth,
				Def->MoveSpeed,
				Accept,
				true,
				WeaponDamage,
				WeaponRange,
				WeaponCooldown,
				TeamId,
				bGather,
				FMath::Max(8, FMath::RoundToInt(Def->CollisionRadius)),
				FMath::Max(0, FMath::RoundToInt(Def->SightRadius)));
			Sim->AccountUnitSupply(EntityId, Def->Cost.Supply, bSupplyPrepaid);
			const int32 ShieldPool = FMath::Max(0, Def->ShieldMax);
			Sim->SetPresentationShield(EntityId, ShieldPool, ShieldPool);
		}
	}

	// Keep CMC disabled as truth; optional visual speed hint only.
	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		CMC->MaxWalkSpeed = 0.f;
		CMC->SetMovementMode(MOVE_None);
	}

	ApplyBodyMesh(Def);
	SnapToGround();
	if (!bForceCubeBody)
	{
		ApplyTeamColor();
	}
	const FName GatherId = Def->DefinitionId.IsNone() ? Def->GetFName() : Def->DefinitionId;
	const bool bWorkerBody = Def->bCanGather || GatherId == FName(KodWardenPaths::Id_Dozer);
	if (bWorkerBody && !bSkeletalBody && UnitMesh && IsEngineCubeMesh(UnitMesh->GetStaticMesh()))
	{
		// Shorter than the Marine cube so three workers read as a crew, not a second squad.
		UnitMesh->SetRelativeScale3D(FVector(0.7f, 0.7f, 1.15f));
		ApplyColorTintFallback(UnitMesh, FLinearColor(FColor(0xC6, 0xA1, 0x5B)));
	}
	ApplyAuthorMesh(Def);
	RefreshJadeiteEmissive();
}

void AKodUnit::ApplyAuthorMesh(const UKodUnitDefinition* Def)
{
	if (!Def)
	{
		return;
	}
	UMaterialInterface* AuthorMaterial = nullptr;
	if (!Def->BodyMaterial.IsNull())
	{
		AuthorMaterial = Def->BodyMaterial.LoadSynchronous();
	}
	const bool bScale = !Def->MeshScale.Equals(FVector::OneVector, 0.01f);
	if (bSkeletalBody)
	{
		if (USkeletalMeshComponent* SkelBody = GetMesh())
		{
			if (AuthorMaterial)
			{
				SkelBody->SetMaterial(0, AuthorMaterial);
			}
			if (bScale)
			{
				SkelBody->SetRelativeScale3D(SkelBody->GetRelativeScale3D() * Def->MeshScale);
			}
		}
		return;
	}
	if (!UnitMesh)
	{
		return;
	}
	if (AuthorMaterial)
	{
		UnitMesh->SetMaterial(0, AuthorMaterial);
	}
	if (bScale)
	{
		UnitMesh->SetRelativeScale3D(UnitMesh->GetRelativeScale3D() * Def->MeshScale);
	}
}

void AKodUnit::KeepSelectCollision()
{
	if (!UnitMesh)
	{
		return;
	}

	// Same two calls as construction. Do not retune Visibility, and do not touch the capsule.
	UnitMesh->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	UnitMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	UnitMesh->SetCollisionResponseToChannel(KodCollision::Select, ECR_Block);
	UnitMesh->SetCollisionResponseToChannel(KodCollision::Ground, ECR_Ignore);
}

float AKodUnit::MirrorBodyHeightUU() const
{
	FName Id = FName();
	if (const UKodUnitDefinition* Def = GetDefinition())
	{
		Id = Def->DefinitionId.IsNone() ? Def->GetFName() : Def->DefinitionId;
	}
	if (Id == FName(KodWardenPaths::Id_Marine))
	{
		return KodCameraSettings::MarineBodyUU;
	}
	return KodCameraSettings::WorkerBodyUU;
}

void AKodUnit::SnapToGround()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	const float HalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 88.f;
	const FVector Loc = GetActorLocation();
	FCollisionQueryParams Params(TEXT("KodUnitGround"), false, this);
	FHitResult Hit;
	const FVector High(Loc.X, Loc.Y, FMath::Max(Loc.Z + 4000.f, 20000.f));
	const FVector Low(Loc.X, Loc.Y, -20000.f);
	if (!World->LineTraceSingleByChannel(Hit, High, Low, KodCollision::Ground, Params) || !Hit.bBlockingHit)
	{
		return;
	}
	const FVector Snapped(Loc.X, Loc.Y, Hit.ImpactPoint.Z + HalfHeight);
	SetActorLocation(Snapped, false, nullptr, ETeleportType::TeleportPhysics);
	if (UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>())
	{
		if (EntityId.IsValid())
		{
			Sim->SetEntityGroundPosition(EntityId, Snapped);
		}
	}
}

void AKodUnit::ApplyBodyMesh(const UKodUnitDefinition* Def)
{
	// Hostile marker. Do not mount SM_Ranger_Body; the red cube tint is the tell.
	if (bForceCubeBody || !Def)
	{
		bMeshAnimHeld = false;
		MountCubePlaceholder();
		return;
	}

	const FName MeshId = Def->DefinitionId.IsNone() ? GetFName() : Def->DefinitionId;
	const FKodMeshChoice Choice = FKodMeshCatalog::Resolve(MeshId);
	if (Choice.bHasRow && Choice.bLoaded && Choice.bSkeletal && Choice.SkeletalMesh)
	{
		MountResolvedSkeletalMesh(Choice.SkeletalMesh, &Choice);
		return;
	}
	if (Choice.bHasRow && Choice.bLoaded && Choice.StaticMesh)
	{
		bMeshAnimHeld = false;
		MountResolvedStaticMesh(Choice.StaticMesh);
		if (UnitMesh)
		{
			UnitMesh->SetRelativeScale3D(FVector(FMath::Max(0.01f, Choice.Scale)));
			FRotator BodyYaw = UnitMesh->GetRelativeRotation();
			BodyYaw.Yaw += Choice.Yaw;
			UnitMesh->SetRelativeRotation(BodyYaw);
			UnitMesh->AddRelativeLocation(FVector(0.f, 0.f, Choice.Z));
			BodyRestRelativeLocation = UnitMesh->GetRelativeLocation();
		}
		return;
	}

	bMeshAnimHeld = false;
	// Skeletal wins when the asset actually loads. A set-but-missing path falls through
	// so PIE never drops the visible body. Slice 0 leaves SkeletalMesh null.
	if (!Def->SkeletalMesh.IsNull())
	{
		if (USkeletalMesh* SkelMesh = Def->SkeletalMesh.LoadSynchronous())
		{
			MountResolvedSkeletalMesh(SkelMesh);
			return;
		}
	}

	if (!Def->StaticMesh.IsNull())
	{
		if (UStaticMesh* StaticBody = Def->StaticMesh.LoadSynchronous())
		{
			MountResolvedStaticMesh(StaticBody);
			return;
		}
	}

	MountCubePlaceholder();
}

void AKodUnit::ApplySkeletalCapsule()
{
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCapsuleSize(SkeletalCapsuleRadius, SkeletalCapsuleHalfHeight);
	}
	bSkeletalCapsule = true;
}

void AKodUnit::RestoreDefaultCapsule()
{
	if (!bSkeletalCapsule)
	{
		return;
	}
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCapsuleSize(DefaultCapsuleRadius, DefaultCapsuleHalfHeight);
	}
	bSkeletalCapsule = false;
}

bool AKodUnit::DefinitionHasAnimSequence(const UKodUnitDefinition* BodyDef) const
{
	if (!BodyDef)
	{
		return false;
	}
	return !BodyDef->IdleAnim.IsNull()
		|| !BodyDef->WalkAnim.IsNull()
		|| !BodyDef->RunAnim.IsNull()
		|| !BodyDef->AimIdleAnim.IsNull()
		|| !BodyDef->FireAnim.IsNull()
		|| !BodyDef->HitReactAnim.IsNull()
		|| !BodyDef->DeathAnim.IsNull()
		|| !BodyDef->DeathAnimB.IsNull();
}

void AKodUnit::LogAnimMode(const TCHAR* ModeName) const
{
	UE_LOG(LogTemp, Log, TEXT("KodUnit AnimMode=%s"), ModeName ? ModeName : TEXT("None"));
}

void AKodUnit::ClearWeaponMesh()
{
	if (!WeaponMeshComp)
	{
		return;
	}
	WeaponMeshComp->SetStaticMesh(nullptr);
	WeaponMeshComp->SetHiddenInGame(true);
	WeaponMeshComp->SetVisibility(false);
}

void AKodUnit::AttachWeaponMesh(USkeletalMeshComponent* SkelBody, const UKodUnitDefinition* BodyDef)
{
	const FName SocketName = (BodyDef && !BodyDef->WeaponSocket.IsNone())
		? BodyDef->WeaponSocket
		: FName(TEXT("weapon_r"));
	UStaticMesh* LoadedWeapon = nullptr;
	if (BodyDef && !BodyDef->WeaponMesh.IsNull())
	{
		LoadedWeapon = BodyDef->WeaponMesh.LoadSynchronous();
	}
	if (!SkelBody || !LoadedWeapon)
	{
		ClearWeaponMesh();
		UE_LOG(LogTemp, Log, TEXT("KodUnit Weapon attached=None socket=%s"), *SocketName.ToString());
		return;
	}

	if (!WeaponMeshComp)
	{
		WeaponMeshComp = NewObject<UStaticMeshComponent>(this, FName(TEXT("KodWeaponMesh")));
		WeaponMeshComp->SetMobility(EComponentMobility::Movable);
		WeaponMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		WeaponMeshComp->SetGenerateOverlapEvents(false);
		WeaponMeshComp->SetCanEverAffectNavigation(false);
		WeaponMeshComp->RegisterComponent();
	}
	WeaponMeshComp->SetStaticMesh(LoadedWeapon);
	WeaponMeshComp->AttachToComponent(SkelBody, FAttachmentTransformRules::SnapToTargetIncludingScale, SocketName);
	WeaponMeshComp->SetRelativeTransform(FTransform::Identity);
	WeaponMeshComp->SetHiddenInGame(false);
	WeaponMeshComp->SetVisibility(true);
	UE_LOG(LogTemp, Log, TEXT("KodUnit Weapon attached=%s socket=%s"),
		*LoadedWeapon->GetName(),
		*SocketName.ToString());
}

void AKodUnit::InitializeAnimFromDefinition(UKodUnitAnimInstance* Anim)
{
	if (!Anim)
	{
		return;
	}
	const UKodUnitDefinition* BodyDef = GetDefinition();
	const bool bNative = bSkeletalBody
		&& Anim->GetClass() == UKodUnitAnimInstance::StaticClass()
		&& DefinitionHasAnimSequence(BodyDef);
	Anim->SetupFromDefinition(BodyDef, bNative);
}

void AKodUnit::MountResolvedSkeletalMesh(USkeletalMesh* SkelMesh, const FKodMeshChoice* Choice)
{
	// Delivered mesh is 178.9 cm at scale 1, root at the soles. No 170 cm fit.
	// Half-height ~90; offset by -half-height so the soles sit on the ground.
	// Yaw comes from the definition (Marine default -90). Do not retarget this
	// skeleton onto another asset.
	ApplySkeletalCapsule();
	bSkeletalBody = true;
	bMeshAnimHeld = false;

	float YawOffset = -90.f;
	const UKodUnitDefinition* BodyDef = GetDefinition();
	if (BodyDef)
	{
		YawOffset = BodyDef->SkeletalMeshYawOffset;
	}
	if (Choice && Choice->bHasRow)
	{
		YawOffset = Choice->Yaw;
	}
	MountedMeshYawOffset = YawOffset;
	const float MountZ = -SkeletalCapsuleHalfHeight + (Choice ? Choice->Z : 0.f);
	const float MountScale = (Choice && Choice->Scale > 0.f) ? Choice->Scale : 1.f;

	if (USkeletalMeshComponent* Body = GetMesh())
	{
		Body->SetSkeletalMeshAsset(SkelMesh);
		Body->SetRelativeLocation(FVector(0.f, 0.f, MountZ));
		Body->SetRelativeRotation(FRotator(0.f, YawOffset, 0.f));
		Body->SetRelativeScale3D(FVector(MountScale));
		Body->SetCanEverAffectNavigation(false);
		Body->SetHiddenInGame(false);
		Body->SetVisibility(true);
		BodyRestRelativeLocation = Body->GetRelativeLocation();

		USkeleton* MeshSkeleton = SkelMesh ? SkelMesh->GetSkeleton() : nullptr;
		auto SequenceMatches = [MeshSkeleton](const TSoftObjectPtr<UAnimSequence>& Soft) -> bool
		{
			if (Soft.IsNull() || !MeshSkeleton)
			{
				return Soft.IsNull();
			}
			const UAnimSequence* Sequence = Soft.LoadSynchronous();
			return !Sequence || Sequence->GetSkeleton() == MeshSkeleton;
		};
		const bool bClipsMatch = !BodyDef
			|| (SequenceMatches(BodyDef->IdleAnim)
				&& SequenceMatches(BodyDef->WalkAnim)
				&& SequenceMatches(BodyDef->RunAnim)
				&& SequenceMatches(BodyDef->AimIdleAnim)
				&& SequenceMatches(BodyDef->FireAnim)
				&& SequenceMatches(BodyDef->HitReactAnim)
				&& SequenceMatches(BodyDef->DeathAnim)
				&& SequenceMatches(BodyDef->DeathAnimB));

		const bool bHasAnims = DefinitionHasAnimSequence(BodyDef);
		UClass* ResolvedAnimClass = nullptr;
		bool bNativeMode = false;
		const TCHAR* AnimModeName = TEXT("None");
		if (Choice && !Choice->Anim.IsEmpty())
		{
			const FString AnimPath = Choice->Anim.Contains(TEXT("."))
				? Choice->Anim
				: Choice->Anim + TEXT(".") + FPaths::GetBaseFilename(Choice->Anim) + TEXT("_C");
			if (UClass* LoadedAnim = LoadClass<UAnimInstance>(nullptr, *AnimPath))
			{
				if (UAnimBlueprintGeneratedClass* Generated = Cast<UAnimBlueprintGeneratedClass>(LoadedAnim))
				{
					if (Generated->TargetSkeleton == MeshSkeleton)
					{
						ResolvedAnimClass = LoadedAnim;
						AnimModeName = TEXT("AnimBP");
					}
					else if (Generated->TargetSkeleton && MeshSkeleton)
					{
						// v7b uses Mannequin bone names on its own skeleton asset.
						// Compatible-skeleton registration edits the shared AnimBP asset, and a
						// runtime retargeter is not available here, so the pose stays held.
						static const TCHAR* BoneNames[] = {
							TEXT("pelvis"), TEXT("spine_01"), TEXT("spine_02"), TEXT("spine_03"),
							TEXT("clavicle_l"), TEXT("upperarm_l"), TEXT("lowerarm_l"), TEXT("hand_l"),
							TEXT("thigh_l"), TEXT("calf_l"), TEXT("foot_l"), TEXT("neck_01"), TEXT("head")
						};
						int32 SharedBones = 0;
						const FReferenceSkeleton& MeshBones = MeshSkeleton->GetReferenceSkeleton();
						const FReferenceSkeleton& AnimBones = Generated->TargetSkeleton->GetReferenceSkeleton();
						for (const TCHAR* BoneName : BoneNames)
						{
							const FName Bone(BoneName);
							if (MeshBones.FindBoneIndex(Bone) != INDEX_NONE && AnimBones.FindBoneIndex(Bone) != INDEX_NONE)
							{
								++SharedBones;
							}
						}
						UE_LOG(LogTemp, Log, TEXT("KodMesh AnimRetarget Id=%s SharedBones=%d Feasible=0"),
							BodyDef ? *BodyDef->DefinitionId.ToString() : *GetName(),
							SharedBones);
					}
				}
			}
		}
		if (!ResolvedAnimClass && BodyDef && !BodyDef->AnimClass.IsNull() && bClipsMatch)
		{
			if (UClass* LoadedAnim = BodyDef->AnimClass.LoadSynchronous())
			{
				if (LoadedAnim->IsChildOf(UKodUnitAnimInstance::StaticClass()))
				{
					ResolvedAnimClass = LoadedAnim;
					bNativeMode = LoadedAnim == UKodUnitAnimInstance::StaticClass() && bHasAnims;
					AnimModeName = bNativeMode ? TEXT("Native") : TEXT("AnimBP");
				}
			}
		}
		if (!ResolvedAnimClass && bHasAnims && bClipsMatch)
		{
			ResolvedAnimClass = UKodUnitAnimInstance::StaticClass();
			bNativeMode = true;
			AnimModeName = TEXT("Native");
		}
		const bool bWantedAnim = (Choice && !Choice->Anim.IsEmpty()) || bHasAnims || (BodyDef && !BodyDef->AnimClass.IsNull());
		if (bWantedAnim && !ResolvedAnimClass)
		{
			bMeshAnimHeld = true;
			Body->SetAnimationMode(EAnimationMode::AnimationCustomMode);
			Body->SetAnimInstanceClass(nullptr);
			Body->bPauseAnims = true;
			UE_LOG(LogTemp, Log, TEXT("KodMesh AnimMismatch Id=%s"), BodyDef ? *BodyDef->DefinitionId.ToString() : *GetName());
			AnimModeName = TEXT("None");
		}
		else if (ResolvedAnimClass)
		{
			Body->bPauseAnims = false;
			Body->SetAnimationMode(EAnimationMode::AnimationBlueprint);
			Body->SetAnimInstanceClass(ResolvedAnimClass);
			if (UAnimInstance* MountedAnim = Body->GetAnimInstance())
			{
				// Pose sampling strips root translation. Do not also apply it to the actor.
				MountedAnim->SetRootMotionMode(ERootMotionMode::NoRootMotionExtraction);
			}
			if (UKodUnitAnimInstance* Anim = Cast<UKodUnitAnimInstance>(Body->GetAnimInstance()))
			{
				Anim->SetupFromDefinition(BodyDef, bNativeMode);
			}
		}
		else
		{
			Body->SetAnimationMode(EAnimationMode::AnimationCustomMode);
			Body->SetAnimInstanceClass(nullptr);
			Body->bPauseAnims = true;
		}
		LogAnimMode(AnimModeName);
		AttachWeaponMesh(Body, BodyDef);
	}

	// Hide the cube draw. Keep its QueryOnly Pawn collision so click-select does not move
	// onto the Character mesh profile or the capsule.
	if (UnitMesh)
	{
		UnitMesh->SetHiddenInGame(true);
		UnitMesh->SetVisibility(false);
		KeepSelectCollision();
	}

	UE_LOG(LogTemp, Log, TEXT("KodUnitBody %s Source=SkeletalMesh Yaw=%.0f Scale=1 CapsuleHH=%.0f Radius=%.0f"),
		*GetName(),
		YawOffset,
		SkeletalCapsuleHalfHeight,
		SkeletalCapsuleRadius);
}

void AKodUnit::MountResolvedStaticMesh(UStaticMesh* StaticBody)
{
	bSkeletalBody = false;
	bAwaitShotNotify = false;
	ClearWeaponMesh();
	RestoreDefaultCapsule();
	LogAnimMode(TEXT("None"));

	if (USkeletalMeshComponent* Body = GetMesh())
	{
		Body->SetSkeletalMeshAsset(nullptr);
		Body->SetHiddenInGame(true);
		Body->SetVisibility(false);
	}

	if (UnitMesh && StaticBody)
	{
		UnitMesh->SetStaticMesh(StaticBody);
		// Slice 0 presentation band-aid. SM_Ranger_Body (and any other static body on this
		// path) was imported in meters; Unreal is centimeters, so bounds are ~2 cm tall at
		// scale 1 and the unit looks missing next to the Engine cube. Fit a uniform scale
		// from local bounds so height lands near a human (~170 cm). Cube placeholder does
		// not use this. MountCubePlaceholder still applies EngineCubeScale. Drop this once
		// Art reimports SM_Ranger_Body at cm scale, or once a skeletal mesh lands.
		// MeshHeight is BoxExtent.Z * 2 (full local Z). Invalid bounds stay at scale 1.
		// BoxExtent is double (UE5 large-world FVector); keep the scale in double too.
		const double TargetHeightCm = static_cast<double>(MirrorBodyHeightUU());
		const double MeshHeight = StaticBody->GetBounds().BoxExtent.Z * 2.0;
		const bool bSaneHeight = FMath::IsFinite(MeshHeight) && MeshHeight > static_cast<double>(KINDA_SMALL_NUMBER);
		const double UniformScale = bSaneHeight ? (TargetHeightCm / MeshHeight) : 1.0;
		const double AppliedScale = FMath::IsFinite(UniformScale) ? UniformScale : 1.0;
		UnitMesh->SetRelativeScale3D(FVector(AppliedScale, AppliedScale, AppliedScale));
		UnitMesh->SetHiddenInGame(false);
		UnitMesh->SetVisibility(true);
		KeepSelectCollision();
	}

	UE_LOG(LogTemp, Log, TEXT("KodUnitBody %s Source=StaticMesh Mesh=%s"),
		*GetName(),
		StaticBody ? *StaticBody->GetName() : TEXT("None"));
}

void AKodUnit::MountCubePlaceholder()
{
	bSkeletalBody = false;
	bAwaitShotNotify = false;
	ClearWeaponMesh();
	RestoreDefaultCapsule();
	LogAnimMode(TEXT("None"));

	if (USkeletalMeshComponent* Body = GetMesh())
	{
		Body->SetSkeletalMeshAsset(nullptr);
		Body->SetHiddenInGame(true);
		Body->SetVisibility(false);
	}

	if (!UnitMesh)
	{
		return;
	}

	if (UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, EngineCubePath))
	{
		UnitMesh->SetStaticMesh(Cube);
	}
	const float BodyUU = MirrorBodyHeightUU();
	UnitMesh->SetRelativeScale3D(FVector(0.8f, 0.8f, BodyUU / 100.f));
	UnitMesh->SetHiddenInGame(false);
	UnitMesh->SetVisibility(true);
	KeepSelectCollision();

	const UKodUnitDefinition* CubeDef = GetDefinition();
	const FName CubeId = CubeDef ? (CubeDef->DefinitionId.IsNone() ? CubeDef->GetFName() : CubeDef->DefinitionId) : GetFName();
	UE_LOG(LogTemp, Log, TEXT("KodMesh Placeholder Id=%s Path=%s"), *CubeId.ToString(), EngineCubePath);
	UE_LOG(LogTemp, Log, TEXT("KodUnitBody %s Source=Cube"), *GetName());
}

void AKodUnit::BindSimEvents(UKodSimSubsystem* Sim)
{
	if (!Sim || BoundSim.Get() == Sim)
	{
		return;
	}
	UnbindSimEvents();
	Sim->OnUnitFired.AddDynamic(this, &AKodUnit::HandleUnitFired);
	Sim->OnUnitHit.AddDynamic(this, &AKodUnit::HandleUnitHit);
	Sim->OnUnitKilled.AddDynamic(this, &AKodUnit::HandleUnitKilled);
	BoundSim = Sim;
}

void AKodUnit::UnbindSimEvents()
{
	if (UKodSimSubsystem* Sim = BoundSim.Get())
	{
		Sim->OnUnitFired.RemoveDynamic(this, &AKodUnit::HandleUnitFired);
		Sim->OnUnitHit.RemoveDynamic(this, &AKodUnit::HandleUnitHit);
		Sim->OnUnitKilled.RemoveDynamic(this, &AKodUnit::HandleUnitKilled);
	}
	BoundSim = nullptr;
}

void AKodUnit::SetBodyRelativeLocation(const FVector& Bobbed)
{
	if (!UnitMesh)
	{
		return;
	}
	const float Alpha = (HitReactRemaining > 0.f)
		? FMath::Clamp(HitReactRemaining / HitReactSeconds, 0.f, 1.f)
		: 0.f;
	UnitMesh->SetRelativeLocation(Bobbed + HitJiggleLocal * Alpha);
}

void AKodUnit::BindJadeitePalette()
{
	if (JadeitePaletteHandle.IsValid())
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		if (UKodJadeitePaletteSubsystem* Palette = World->GetSubsystem<UKodJadeitePaletteSubsystem>())
		{
			JadeitePaletteHandle = Palette->OnPaletteChanged.AddUObject(this, &AKodUnit::RefreshJadeiteEmissive);
		}
	}
}

void AKodUnit::UnbindJadeitePalette()
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

void AKodUnit::RefreshJadeiteEmissive()
{
	if (CargoMID && CargoMesh && ShownCargoJadeite > 0 && ShownCargoLuminene <= 0)
	{
		const FLinearColor Tint = KodJadeiteVisual::TunedJadeBase(GetWorld());
		CargoMID->SetVectorParameterValue(TEXT("Color"), Tint);
		CargoMID->SetVectorParameterValue(TEXT("BaseColor"), Tint);
	}
	FName DefId = NAME_None;
	if (const UKodUnitDefinition* Def = GetDefinition())
	{
		DefId = Def->DefinitionId.IsNone() ? Def->GetFName() : Def->DefinitionId;
	}
	if (!KodJadeiteVisual::IsScinderId(DefId) && !KodJadeiteVisual::IsScinderActor(this))
	{
		return;
	}
	TArray<UMeshComponent*> Meshes;
	GetComponents<UMeshComponent>(Meshes);
	for (UMeshComponent* TintMesh : Meshes)
	{
		KodJadeiteVisual::ApplyTunedEmissive(TintMesh);
	}
}

void AKodUnit::EnsureBodyTint()
{
	if (BodyTintMID || !UnitMesh)
	{
		return;
	}
	if (UMaterialInstanceDynamic* Existing = Cast<UMaterialInstanceDynamic>(UnitMesh->GetMaterial(0)))
	{
		BodyTintMID = Existing;
		return;
	}
	UMaterialInterface* Source = UnitMesh->GetMaterial(0);
	if (!Source)
	{
		Source = LoadObject<UMaterialInterface>(nullptr, BasicShapeMaterialPath);
	}
	if (!Source)
	{
		return;
	}
	BodyTintMID = UMaterialInstanceDynamic::Create(Source, this);
	if (!BodyTintMID)
	{
		return;
	}
	BodyTintMID->SetVectorParameterValue(TEXT("Color"), BodyRestColor);
	BodyTintMID->SetVectorParameterValue(TEXT("BaseColor"), BodyRestColor);
	UnitMesh->SetMaterial(0, BodyTintMID);
}

void AKodUnit::EnsureAttackPresentation()
{
	if (MuzzleFlashLight)
	{
		return;
	}

	USceneComponent* AttachParent = GetRootComponent();
	if (!AttachParent)
	{
		return;
	}

	const FLinearColor WarmOrange(1.f, 0.45f, 0.08f, 1.f);
	const FLinearColor TracerYellow(1.f, 0.92f, 0.1f, 1.f);

	MuzzleFlashLight = NewObject<UPointLightComponent>(this, FName(TEXT("KodFxMuzzleLight")));
	MuzzleFlashLight->SetupAttachment(AttachParent);
	MuzzleFlashLight->SetMobility(EComponentMobility::Movable);
	MuzzleFlashLight->SetIntensity(60000.f);
	MuzzleFlashLight->SetLightColor(WarmOrange);
	MuzzleFlashLight->SetAttenuationRadius(550.f);
	MuzzleFlashLight->SetCastShadows(false);
	MuzzleFlashLight->SetVisibility(false);
	MuzzleFlashLight->SetHiddenInGame(true);
	MuzzleFlashLight->RegisterComponent();

	MuzzleMID = MakeShapeMID(this, WarmOrange);
	TracerMID = MakeShapeMID(this, TracerYellow);

	auto MakeFxMesh = [this, AttachParent](FName FxName, const TCHAR* MeshPath, UMaterialInstanceDynamic* MID) -> UStaticMeshComponent*
	{
		UStaticMesh* Shape = LoadObject<UStaticMesh>(nullptr, MeshPath);
		if (!Shape)
		{
			return nullptr;
		}
		UStaticMeshComponent* FxMesh = NewObject<UStaticMeshComponent>(this, FxName);
		FxMesh->SetupAttachment(AttachParent);
		FxMesh->SetStaticMesh(Shape);
		ConfigureFxPrimitive(FxMesh);
		if (MID)
		{
			FxMesh->SetMaterial(0, MID);
		}
		FxMesh->SetVisibility(false);
		FxMesh->SetHiddenInGame(true);
		FxMesh->RegisterComponent();
		return FxMesh;
	};

	MuzzleFlashMesh = MakeFxMesh(FName(TEXT("KodFxMuzzleSphere")), EngineSpherePath, MuzzleMID);
	MuzzleConeMesh = MakeFxMesh(FName(TEXT("KodFxMuzzleCone")), EngineConePath, MuzzleMID);
	ShotTracerMesh = MakeFxMesh(FName(TEXT("KodFxTracer")), EngineCylinderPath, TracerMID);
}

bool AKodUnit::TryResolveMuzzleSocket(FVector& OutWorld) const
{
	const FName MuzzleSocket(TEXT("SOCKET_Muzzle"));
	const FName GripSocket(TEXT("weapon_r"));

	if (WeaponMeshComp && WeaponMeshComp->DoesSocketExist(MuzzleSocket))
	{
		OutWorld = WeaponMeshComp->GetSocketLocation(MuzzleSocket);
		return true;
	}

	USkeletalMeshComponent* SkelBody = GetMesh();
	if (SkelBody && !SkelBody->bHiddenInGame && SkelBody->DoesSocketExist(MuzzleSocket))
	{
		OutWorld = SkelBody->GetSocketLocation(MuzzleSocket);
		return true;
	}

	if (SkelBody && !SkelBody->bHiddenInGame && SkelBody->DoesSocketExist(GripSocket))
	{
		OutWorld = SkelBody->GetSocketLocation(GripSocket);
		return true;
	}
	if (WeaponMeshComp && WeaponMeshComp->DoesSocketExist(GripSocket))
	{
		OutWorld = WeaponMeshComp->GetSocketLocation(GripSocket);
		return true;
	}
	return false;
}

void AKodUnit::ShowMuzzleAndTracer(AActor* Target, bool bUseMuzzleWorld, FVector MuzzleWorld)
{
	EnsureAttackPresentation();

	FVector ShotDir = GetActorForwardVector();
	ShotDir.Z = 0.f;
	if (Target)
	{
		FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
		ToTarget.Z = 0.f;
		if (ToTarget.SizeSquared() > KINDA_SMALL_NUMBER)
		{
			ShotDir = ToTarget.GetSafeNormal();
		}
	}
	if (ShotDir.IsNearlyZero())
	{
		ShotDir = FVector::ForwardVector;
	}

	const FVector Muzzle = bUseMuzzleWorld
		? MuzzleWorld
		: (GetActorLocation() + ShotDir * MuzzleForwardCm + FVector(0.f, 0.f, MuzzleUpCm));
	const FVector LightAt = Muzzle + ShotDir * 24.f;

	auto ShowAbsolute = [](USceneComponent* Component, const FVector& Location, const FRotator& Rotation, const FVector& Scale)
	{
		if (!Component)
		{
			return;
		}
		Component->SetAbsolute(true, true, true);
		Component->SetWorldLocation(Location);
		Component->SetWorldRotation(Rotation);
		Component->SetWorldScale3D(Scale);
		Component->SetHiddenInGame(false);
		Component->SetVisibility(true);
	};

	if (MuzzleFlashLight)
	{
		MuzzleFlashLight->SetAbsolute(true, true, true);
		MuzzleFlashLight->SetWorldLocation(LightAt);
		MuzzleFlashLight->SetIntensity(60000.f);
		MuzzleFlashLight->SetHiddenInGame(false);
		MuzzleFlashLight->SetVisibility(true);
	}

	ShowAbsolute(MuzzleFlashMesh, Muzzle, FRotator::ZeroRotator, FVector(0.4f));
	const FRotator ConeRotation = FRotationMatrix::MakeFromZ(ShotDir).Rotator();
	ShowAbsolute(MuzzleConeMesh, Muzzle, ConeRotation, FVector(0.18f, 0.18f, 0.45f));

	if (ShotTracerMesh && Target)
	{
		const FVector Chest = Target->GetActorLocation() + FVector(0.f, 0.f, 55.f);
		const FVector Delta = Chest - Muzzle;
		const float Dist = Delta.Size();
		if (Dist > KINDA_SMALL_NUMBER)
		{
			const FVector Mid = (Muzzle + Chest) * 0.5f;
			const FRotator Align = FRotationMatrix::MakeFromZ(Delta).Rotator();
			const float CylinderHeightCm = 100.f;
			ShowAbsolute(
				ShotTracerMesh,
				Mid,
				Align,
				FVector(TracerRadiusScale, TracerRadiusScale, Dist / CylinderHeightCm));
			TracerRemaining = TracerSeconds;
		}
	}

	MuzzleFlashRemaining = MuzzleFlashSeconds;
}

void AKodUnit::AdvanceAttackPresentation(float DeltaSeconds)
{
	const float Dt = FMath::Max(0.f, DeltaSeconds);
	if (MuzzleFlashRemaining > 0.f)
	{
		MuzzleFlashRemaining = FMath::Max(0.f, MuzzleFlashRemaining - Dt);
		if (MuzzleFlashRemaining <= 0.f)
		{
			auto HideFx = [](USceneComponent* Component)
			{
				if (!Component)
				{
					return;
				}
				Component->SetVisibility(false);
				Component->SetHiddenInGame(true);
			};
			HideFx(MuzzleFlashLight);
			HideFx(MuzzleFlashMesh);
			HideFx(MuzzleConeMesh);
			if (MuzzleFlashLight)
			{
				MuzzleFlashLight->SetIntensity(0.f);
			}
		}
	}

	if (TracerRemaining > 0.f)
	{
		TracerRemaining = FMath::Max(0.f, TracerRemaining - Dt);
		if (TracerRemaining <= 0.f && ShotTracerMesh)
		{
			ShotTracerMesh->SetVisibility(false);
			ShotTracerMesh->SetHiddenInGame(true);
		}
	}

	if (HitReactRemaining > 0.f)
	{
		HitReactRemaining = FMath::Max(0.f, HitReactRemaining - Dt);
		if (HitReactRemaining <= 0.f)
		{
			HitJiggleLocal = FVector::ZeroVector;
			WriteBodyTint(BodyRestColor);
		}
	}
}

void AKodUnit::PushAnimSimEvent(bool bFired, bool bHit, bool bDead)
{
	if (!bSkeletalBody)
	{
		return;
	}
	USkeletalMeshComponent* Body = GetMesh();
	UKodUnitAnimInstance* Anim = Body ? Cast<UKodUnitAnimInstance>(Body->GetAnimInstance()) : nullptr;
	if (!Anim)
	{
		return;
	}
	if (bFired)
	{
		Anim->HandleSimFired();
	}
	if (bHit)
	{
		Anim->HandleSimHit();
	}
	if (bDead)
	{
		Anim->HandleSimDeath();
	}
}

float AKodUnit::GetSkeletalDeathHoldSeconds() const
{
	if (!bSkeletalBody)
	{
		return 0.f;
	}
	const USkeletalMeshComponent* Body = GetMesh();
	const UKodUnitAnimInstance* Anim = Body ? Cast<UKodUnitAnimInstance>(Body->GetAnimInstance()) : nullptr;
	return Anim ? Anim->GetDeathSinkDelaySeconds() : 0.f;
}

void AKodUnit::PlayMuzzleFromShotNotify()
{
	if (!bAwaitShotNotify)
	{
		return;
	}
	bAwaitShotNotify = false;
	FVector SocketWorld = FVector::ZeroVector;
	if (TryResolveMuzzleSocket(SocketWorld))
	{
		ShowMuzzleAndTracer(PendingMuzzleTarget.Get(), true, SocketWorld);
	}
	else
	{
		ShowMuzzleAndTracer(PendingMuzzleTarget.Get(), false, FVector::ZeroVector);
	}
}

void AKodUnit::HandleUnitFired(AActor* Attacker, AActor* Target, int32 /*SimTick*/)
{
	if (Attacker != this || bDeathSinking)
	{
		return;
	}

	PendingMuzzleTarget = Target;
	PushAnimSimEvent(true, false, false);

	USkeletalMeshComponent* Body = GetMesh();
	UKodUnitAnimInstance* Anim = (bSkeletalBody && Body) ? Cast<UKodUnitAnimInstance>(Body->GetAnimInstance()) : nullptr;
	const bool bAnimBp = Anim && Anim->GetClass() != UKodUnitAnimInstance::StaticClass();
	const bool bNativeShot = Anim && Anim->IsNativePoseDriver() && Anim->HasFireSequence();
	const bool bWaitForShotNotify = bAnimBp || bNativeShot;
	if (bWaitForShotNotify)
	{
		bAwaitShotNotify = true;
		ShotNotifyWait = 0.f;
	}
	else
	{
		bAwaitShotNotify = false;
		ShowMuzzleAndTracer(Target, false, FVector::ZeroVector);
	}
}

void AKodUnit::HandleUnitHit(AActor* Attacker, AActor* Target, float /*Health*/, int32 /*TargetId*/)
{
	if (Target != this || bDeathSinking)
	{
		return;
	}

	EnsureBodyTint();
	const bool bRestIsRed = BodyRestColor.R > BodyRestColor.G * 2.f
		&& BodyRestColor.R > BodyRestColor.B * 2.f
		&& BodyRestColor.R > 0.15f;
	const FLinearColor FlashColor = bRestIsRed
		? FLinearColor::White
		: FLinearColor(1.f, 0.15f, 0.12f, 1.f);
	WriteBodyTint(FlashColor);

	FVector Away = FVector::ForwardVector;
	if (Attacker && Attacker != this)
	{
		Away = GetActorLocation() - Attacker->GetActorLocation();
		Away.Z = 0.f;
	}
	if (!Away.Normalize())
	{
		Away = -GetActorForwardVector();
		Away.Z = 0.f;
		if (!Away.Normalize())
		{
			Away = FVector::ForwardVector;
		}
	}
	HitJiggleLocal = GetActorTransform().InverseTransformVectorNoScale(Away * HitJiggleCm);
	HitJiggleLocal.Z += 8.f;
	HitReactRemaining = HitReactSeconds;
	PushAnimSimEvent(false, true, false);
}

void AKodUnit::HandleUnitKilled(AActor* Target, int32 /*TargetId*/)
{
	if (Target != this || bDeathSinking)
	{
		return;
	}
	BeginDeathPresentation();
}

void AKodUnit::BeginDeathPresentation()
{
	if (bDeathSinking)
	{
		return;
	}
	bDeathSinking = true;
	bAwaitShotNotify = false;
	DeathSinkElapsed = 0.f;
	if (UWorld* World = GetWorld())
	{
		CustomTimeDilation = 0.f;
		HitStopUntil = World->GetTimeSeconds() + 0.07f;
	}
	DeathSinkStart = GetActorLocation();
	PushAnimSimEvent(false, false, true);
	DeathSinkDelayRemaining = GetSkeletalDeathHoldSeconds();
	SetActorEnableCollision(false);
	UE_LOG(LogTemp, Log, TEXT("KodUnit Death %s"), *GetName());

	if (UWorld* World = GetWorld())
	{
		if (AKodPlayerController* KodPC = Cast<AKodPlayerController>(World->GetFirstPlayerController()))
		{
			KodPC->RemoveFromLocalSelection(this);
		}
	}
}

bool AKodUnit::AdvanceDeathSink(float DeltaSeconds)
{
	if (!bDeathSinking)
	{
		return false;
	}
	const float Dt = FMath::Max(0.f, DeltaSeconds);
	if (DeathSinkDelayRemaining > 0.f)
	{
		DeathSinkDelayRemaining -= Dt;
		if (DeathSinkDelayRemaining > 0.f)
		{
			return false;
		}
		DeathSinkStart = GetActorLocation();
		DeathSinkElapsed = 0.f;
		return false;
	}
	DeathSinkElapsed += Dt;
	const float Alpha = FMath::Clamp(DeathSinkElapsed / DeathSinkSeconds, 0.f, 1.f);
	const float Eased = Alpha * Alpha;
	const FVector Sunk = DeathSinkStart - FVector(0.f, 0.f, DeathSinkDepthCm * Eased);
	SetActorLocation(Sunk, false, nullptr, ETeleportType::TeleportPhysics);
	if (Alpha >= 1.f)
	{
		Destroy();
		return true;
	}
	return false;
}

void AKodUnit::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindJadeitePalette();
	UnbindSimEvents();
	if (UWorld* World = GetWorld())
	{
		if (UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>())
		{
			Sim->UnregisterEntityIfActor(EntityId, this);
		}
	}
	Super::EndPlay(EndPlayReason);
}
