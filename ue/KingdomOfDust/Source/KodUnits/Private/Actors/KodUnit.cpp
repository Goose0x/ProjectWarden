#include "Actors/KodUnit.h"
#include "Data/KodUnitDefinition.h"
#include "Data/KodWeaponDefinition.h"
#include "Components/KodAbilitySystemComponent.h"
#include "Components/KodMoveComponent.h"
#include "Components/KodAttackComponent.h"
#include "Attributes/KodCombatAttributeSet.h"
#include "Sim/KodSimSubsystem.h"
#include "Slice0/KodSlice0Bootstrap.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr const TCHAR* EngineCubePath = TEXT("/Engine/BasicShapes/Cube.Cube");
	constexpr const TCHAR* BasicShapeMaterialPath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");
	const FVector EngineCubeScale(0.8f, 0.8f, 1.7f);
	constexpr double LifeReferenceHeightCm = 170.0;
	// Real move speed is 450. A 1 uu idle quantize snap is ~60-150 uu/s depending on frame
	// time, so the threshold sits above that and still sees a walk.
	constexpr float LifeMoveSpeedThreshold = 200.f;

	bool IsEngineCubeMesh(const UStaticMesh* BodyStaticMesh)
	{
		return BodyStaticMesh && BodyStaticMesh->GetPathName().Contains(TEXT("/Engine/BasicShapes/Cube"));
	}
}

AKodUnit::AKodUnit()
{
	PrimaryActorTick.bCanEverTick = true;
	// Best-effort late tick. Bob and facing are UnitMesh relative offsets, so they
	// still show if this runs before the sim sync (one frame late) or after it.
	// Neither write touches FKodSimEntityState or ComputeIdleHash.
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;

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

	VisualYaw = GetActorRotation().Yaw;
	LastPresentationLocation = GetActorLocation();
	BodyRestRelativeLocation = UnitMesh ? UnitMesh->GetRelativeLocation() : FVector::ZeroVector;
	UE_LOG(LogTemp, Log, TEXT("KodUnitLife %s Bob=%.1f Turn=%.0f"),
		*GetName(),
		BobAmplitudeCm,
		TurnRateDegreesPerSecond);
}

void AKodUnit::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateLifePresentation(DeltaSeconds);
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
	const FLinearColor HostileRed(0.75f, 0.02f, 0.02f, 1.f);
	MID->SetVectorParameterValue(TEXT("Color"), HostileRed);
	MID->SetVectorParameterValue(TEXT("BaseColor"), HostileRed);
	UnitMesh->SetMaterial(0, MID);
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

	if (!UnitMesh)
	{
		return;
	}

	// Sim sync writes actor yaw. Keep the eased facing on the body mesh so it
	// survives whichever tick runs last. If we run first, the offset is one
	// frame behind the sim yaw the sync is about to write.
	FRotator BodyRelRotation = UnitMesh->GetRelativeRotation();
	BodyRelRotation.Yaw = FMath::UnwindDegrees(VisualYaw - GetActorRotation().Yaw);
	UnitMesh->SetRelativeRotation(BodyRelRotation);

	const float TargetWeight = bMoving ? 1.f : 0.f;
	const float Ease = FMath::Max(0.f, BobEaseSpeed);
	BobWeight = (Ease <= 0.f || Dt <= 0.f) ? TargetWeight : FMath::FInterpTo(BobWeight, TargetWeight, Dt, Ease);

	if (!bMoving && BobWeight <= KINDA_SMALL_NUMBER)
	{
		BobWeight = 0.f;
		BobTime = 0.f;
		if (bBobOffsetApplied)
		{
			UnitMesh->SetRelativeLocation(BodyRestRelativeLocation);
			bBobOffsetApplied = false;
		}
		return;
	}

	BobTime += Dt;
	const float Frequency = FMath::Max(0.f, BobFrequencyHz);
	const float Angle = BobTime * Frequency * 2.f * static_cast<float>(UE_PI);
	const float Z = GetScaledBobAmplitudeCm() * BobWeight * FMath::Sin(Angle);
	UnitMesh->SetRelativeLocation(BodyRestRelativeLocation + FVector(0.f, 0.f, Z));
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

	float WeaponDamage = 0.f;
	float WeaponRange = 0.f;
	float WeaponCooldown = 0.f;
	if (UKodWeaponDefinition* Weapon = Def->PrimaryWeapon.LoadSynchronous())
	{
		WeaponDamage = Weapon->Damage;
		WeaponRange = Weapon->Range;
		WeaponCooldown = Weapon->CooldownSeconds;
	}
	else if (!Def->PrimaryWeapon.IsNull())
	{
		if (UKodWeaponDefinition* BootWeapon = UKodSlice0Bootstrap::ResolveWeapon(Def->PrimaryWeapon.ToSoftObjectPath().GetAssetFName(), this))
		{
			WeaponDamage = BootWeapon->Damage;
			WeaponRange = BootWeapon->Range;
			WeaponCooldown = BootWeapon->CooldownSeconds;
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
			Sim->ConfigureEntity(
				EntityId,
				Def->DefinitionId.IsNone() ? Def->GetFName() : Def->DefinitionId,
				Def->MaxHealth,
				Def->MoveSpeed,
				Accept,
				true,
				WeaponDamage,
				WeaponRange,
				WeaponCooldown,
				TeamId);
		}
	}

	// Keep CMC disabled as truth; optional visual speed hint only.
	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		CMC->MaxWalkSpeed = 0.f;
		CMC->SetMovementMode(MOVE_None);
	}

	ApplyBodyMesh(Def);
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
}

void AKodUnit::ApplyBodyMesh(const UKodUnitDefinition* Def)
{
	// Hostile marker. Do not mount SM_Ranger_Body; the red cube tint is the tell.
	if (bForceCubeBody || !Def)
	{
		MountCubePlaceholder();
		return;
	}

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

void AKodUnit::MountResolvedSkeletalMesh(USkeletalMesh* SkelMesh)
{
	if (USkeletalMeshComponent* Body = GetMesh())
	{
		Body->SetSkeletalMeshAsset(SkelMesh);
		Body->SetCanEverAffectNavigation(false);
		Body->SetHiddenInGame(false);
		Body->SetVisibility(true);
	}

	// Hide the cube draw. Keep its QueryOnly Pawn collision so click-select does not move
	// onto the Character mesh profile or the capsule.
	if (UnitMesh)
	{
		UnitMesh->SetHiddenInGame(true);
		UnitMesh->SetVisibility(false);
		KeepSelectCollision();
	}

	UE_LOG(LogTemp, Log, TEXT("KodUnitBody %s Source=SkeletalMesh"), *GetName());
}

void AKodUnit::MountResolvedStaticMesh(UStaticMesh* StaticBody)
{
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
		constexpr double TargetHeightCm = 170.0;
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
	UnitMesh->SetRelativeScale3D(EngineCubeScale);
	UnitMesh->SetHiddenInGame(false);
	UnitMesh->SetVisibility(true);
	KeepSelectCollision();

	UE_LOG(LogTemp, Log, TEXT("KodUnitBody %s Source=Cube"), *GetName());
}

void AKodUnit::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>())
		{
			Sim->UnregisterEntity(EntityId);
		}
	}
	Super::EndPlay(EndPlayReason);
}
