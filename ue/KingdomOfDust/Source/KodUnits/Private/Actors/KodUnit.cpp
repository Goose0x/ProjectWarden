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

AKodUnit::AKodUnit()
{
	PrimaryActorTick.bCanEverTick = true;

	AbilitySystemComponent = CreateDefaultSubobject<UKodAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystemComponent->SetIsReplicated(true);

	CombatAttributes = CreateDefaultSubobject<UKodCombatAttributeSet>(TEXT("CombatAttributes"));

	MoveComponent = CreateDefaultSubobject<UKodMoveComponent>(TEXT("KodMove"));
	AttackComponent = CreateDefaultSubobject<UKodAttackComponent>(TEXT("KodAttack"));

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
		Def = Cast<UKodUnitDefinition>(UKodSlice0Bootstrap::ResolveDefinition(Definition.GetAssetFName(), this));
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
		return UKodSlice0Bootstrap::ResolveUnit(Definition.GetAssetFName(), const_cast<AKodUnit*>(this));
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
		if (UKodWeaponDefinition* BootWeapon = UKodSlice0Bootstrap::ResolveWeapon(Def->PrimaryWeapon.GetAssetFName(), this))
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
