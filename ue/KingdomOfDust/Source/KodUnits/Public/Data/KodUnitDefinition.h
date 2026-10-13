#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameplayTagContainer.h"
#include "Sim/KodBuildTicks.h"
#include "Sim/KodAbilityTypes.h"
#include "Sim/KodResourceTypes.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "Animation/AnimSequence.h"
#include "Animation/KodUnitAnimInstance.h"
#include "KodUnitDefinition.generated.h"

class UKodWeaponDefinition;

/** Auto: infantry, biological units, and workers. Mechanical units and buildings explode. */
UENUM(BlueprintType)
enum class EKodDeathStyle : uint8
{
	Auto,
	Organic,
	Mechanical
};

/**
 * Primary data asset for a unit archetype.
 * Runtime MUST load MaxHealth / MoveSpeed / BuildTicks from this DA —
 * UPROPERTY defaults are editor-only conveniences, never match-state truth.
 * Slice 0 id law: Marine and Dozer under /Game/Warden/Data/. Ranger is the legacy id for Marine.
 * Dozer is the AMP worker engine id. DisplayName is AMP.
 */
UCLASS(BlueprintType)
class KODUNITS_API UKodUnitDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Must match asset name exactly (e.g. Marine). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Identity")
	FName DefinitionId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Identity")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Identity")
	FGameplayTag UnitTag;

	/** Silhouette for the info panel and the 48px group cell. 128 source, scaled by the HUD. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|UI")
	TSoftObjectPtr<UTexture2D> Wireframe;

	/** Command card, queue, and group-cell icon. 128 source, scaled to the slot. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|UI")
	TSoftObjectPtr<UTexture2D> CommandIcon;

	/** HP ceiling — applied at spawn from DA only. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Combat")
	float MaxHealth = 100.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Combat")
	float Armor = 0.f;

	/** Sim units per second (seek speed). Not CharacterMovement MaxWalkSpeed as truth. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Movement")
	float MoveSpeed = 400.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Combat")
	TSoftObjectPtr<UKodWeaponDefinition> PrimaryWeapon;

	/** Credits are Jadeite. BuildTime is sim ticks. Replaces BuildCostCash. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Economy")
	FKodTrainCost Cost;

	/** Dozer (AMP). Copied onto the sim entity. Marines stay false. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Economy")
	bool bCanGather = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Tech")
	bool bBiological = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Tech")
	bool bInfantry = false;

	/**
	 * Auto treats infantry, biological units, and workers as organic.
	 * Organic deaths sink or play a body clip. Mechanical deaths explode.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Combat")
	EKodDeathStyle DeathStyle = EKodDeathStyle::Auto;

	bool UsesOrganicDeath() const
	{
		if (DeathStyle == EKodDeathStyle::Organic)
		{
			return true;
		}
		if (DeathStyle == EKodDeathStyle::Mechanical)
		{
			return false;
		}
		return bInfantry || bBiological || bCanGather;
	}

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Tech")
	bool bFlyer = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Tech")
	bool bHealer = false;

	/** Bunker space. 0 cannot load. Marine and Medic are 1. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Tech")
	int32 CargoSize = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Tech")
	int32 EnergyMax = 0;

	/** Spawn shield. 0 hides the wireframe rim. Presentation only, not combat. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Tech")
	int32 ShieldMax = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Tech")
	int32 EnergyStart = 0;

	/** Completed structure required before this unit can be trained. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Tech")
	FName RequiresId;

	/** Sim abilities. Combat Stim lives on the Marine. Not GAS. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Ability")
	TArray<FKodAbilitySpec> Abilities;

	/**
	 * Authoritative build duration in sim ticks (SimHz=16).
	 * Marine=80, Dozer=128 per Slice 0 sheet.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Economy")
	int32 BuildTicks = 80;

	/**
	 * Editor convenience / legacy. Synced to BuildTicks via SecondsToBuildTicks when edited.
	 * Runtime enqueue prefers BuildTicks.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Economy")
	float BuildTimeSeconds = 5.f;

	/**
	 * Collision / footprint radius (cm). Slice 0 stop distance still uses MoveComponent default (50);
	 * later: AcceptanceRadius from this field so seek-stop is data-driven.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Movement")
	float CollisionRadius = 50.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Vision")
	float SightRadius = 1500.f;

	/**
	 * Preferred body once anims land. Slice 0 leaves this empty.
	 * When the soft path loads, AKodUnit shows it on the Character skeletal mesh and hides the static placeholder.
	 * Presentation only — not a Warden Data id, and not select-collision truth.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Mesh")
	TSoftObjectPtr<USkeletalMesh> SkeletalMesh;

	/** Optional bust. Empty keeps a copy of the body mesh in the portrait capture. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Mesh")
	TSoftObjectPtr<USkeletalMesh> PortraitMesh;

	/**
	 * Relative yaw on the skeletal mesh component. The Marine bind pose faces +Y,
	 * so -90 lines the mesh up with actor forward (+X). The skeleton is SK_Marine_Skeleton.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Mesh")
	float SkeletalMeshYawOffset = -90.f;

	/**
	 * AnimBP parented to UKodUnitAnimInstance. Empty, with sequences set, uses the
	 * C++ pose driver (AnimMode=Native) instead of an AnimBP asset.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Mesh")
	TSoftClassPtr<UKodUnitAnimInstance> AnimClass;

	/**
	 * When AnimClass is empty and any of these sequences load, UKodUnitAnimInstance
	 * drives the pose in C++. A set AnimClass still wins (future AnimBP).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TSoftObjectPtr<UAnimSequence> IdleAnim;

	/** Portrait talk. Empty until a VO line is assigned. Not hashed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TSoftObjectPtr<UAnimSequence> TalkAnim;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TSoftObjectPtr<UAnimSequence> WalkAnim;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TSoftObjectPtr<UAnimSequence> RunAnim;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TSoftObjectPtr<UAnimSequence> AimIdleAnim;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TSoftObjectPtr<UAnimSequence> FireAnim;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TSoftObjectPtr<UAnimSequence> HitReactAnim;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TSoftObjectPtr<UAnimSequence> DeathAnim;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TSoftObjectPtr<UAnimSequence> DeathAnimB;

	/** Death holds from this frame at 30 fps. Even entity ids. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	int32 DeathHoldFrame = 26;

	/** Death_B holds from this frame at 30 fps. Odd entity ids. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	int32 DeathHoldFrameB = 30;

	/** Carbine (or other weapon) static mesh. Attached on the skeletal path. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TSoftObjectPtr<UStaticMesh> WeaponMesh;

	/** Socket for WeaponMesh. Default weapon_r. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	FName WeaponSocket = TEXT("weapon_r");

	/**
	 * Slice 0 visible body on AKodUnit::UnitMesh (the static mesh child).
	 * Local import soft path: /Game/Warden/Characters/USA/Ranger/SM_Ranger_Body.SM_Ranger_Body
	 * (KodWardenPaths::RangerBodyStaticMesh). Null keeps the Engine cube.
	 * Do not point this at /Game/Units/Meshes/.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Mesh")
	TSoftObjectPtr<UStaticMesh> StaticMesh;

	/** Optional. Empty keeps the mesh material or the cube tint. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Mesh")
	TSoftObjectPtr<UMaterialInterface> BodyMaterial;

	/** Multiplies the resolved body scale. (1,1,1) leaves the placeholder. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Mesh")
	FVector MeshScale = FVector(1.f, 1.f, 1.f);

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		const FName Id = !DefinitionId.IsNone() ? DefinitionId : GetFName();
		return FPrimaryAssetId(TEXT("KodUnitDefinition"), Id);
	}

	UFUNCTION(BlueprintPure, Category = "Kod|Economy")
	int32 GetResolvedBuildTicks() const
	{
		if (Cost.BuildTime > 0)
		{
			return Cost.BuildTime;
		}
		return KodBuildTicks::ResolveBuildTicks(BuildTicks, BuildTimeSeconds);
	}

	/** Keep BuildTicks and BuildTimeSeconds consistent after authoring edits. */
	UFUNCTION(BlueprintCallable, Category = "Kod|Economy")
	void SyncBuildTimingFromSeconds()
	{
		BuildTicks = KodBuildTicks::SecondsToBuildTicks(BuildTimeSeconds);
	}

	UFUNCTION(BlueprintCallable, Category = "Kod|Economy")
	void SyncBuildTimingFromTicks()
	{
		BuildTimeSeconds = KodBuildTicks::BuildTicksToSeconds(BuildTicks);
	}

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
