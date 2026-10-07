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
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr const TCHAR* EngineCubePath = TEXT("/Engine/BasicShapes/Cube.Cube");
	const FVector EngineCubeScale(0.8f, 0.8f, 1.7f);
}

AKodUnit::AKodUnit()
{
	PrimaryActorTick.bCanEverTick = true;

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
				WeaponCooldown);
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
	if (!Def)
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
		// Cube scale would squash an imported body. Real mesh stays at 1.
		UnitMesh->SetRelativeScale3D(FVector::OneVector);
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
