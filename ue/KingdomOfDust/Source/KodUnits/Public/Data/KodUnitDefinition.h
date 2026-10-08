#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameplayTagContainer.h"
#include "Sim/KodBuildTicks.h"
#include "Animation/KodUnitAnimInstance.h"
#include "KodUnitDefinition.generated.h"

class UKodWeaponDefinition;

/**
 * Primary data asset for a unit archetype.
 * Runtime MUST load MaxHealth / MoveSpeed / BuildTicks from this DA —
 * UPROPERTY defaults are editor-only conveniences, never match-state truth.
 * Slice 0 id law: Ranger, Dozer under /Game/Warden/Data/.
 */
UCLASS(BlueprintType)
class KODUNITS_API UKodUnitDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Must match asset name exactly (e.g. Ranger). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Identity")
	FName DefinitionId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Identity")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Identity")
	FGameplayTag UnitTag;

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

	/** Cash spent to train. Not a gather-node field. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Economy")
	int32 BuildCostCash = 50;

	/**
	 * Authoritative build duration in sim ticks (SimHz=16).
	 * Ranger=80, Dozer=128 per Slice 0 sheet.
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

	/**
	 * AnimBP parented to UKodUnitAnimInstance. Empty uses that C++ class directly
	 * (no Fire notify; the timed muzzle flash stays on).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Mesh")
	TSoftClassPtr<UKodUnitAnimInstance> AnimClass;

	/**
	 * Slice 0 visible body on AKodUnit::UnitMesh (the static mesh child).
	 * Local import soft path: /Game/Warden/Characters/USA/Ranger/SM_Ranger_Body.SM_Ranger_Body
	 * (KodWardenPaths::RangerBodyStaticMesh). Null keeps the Engine cube.
	 * Do not point this at /Game/Units/Meshes/.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Mesh")
	TSoftObjectPtr<UStaticMesh> StaticMesh;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		const FName Id = !DefinitionId.IsNone() ? DefinitionId : GetFName();
		return FPrimaryAssetId(TEXT("KodUnitDefinition"), Id);
	}

	UFUNCTION(BlueprintPure, Category = "Kod|Economy")
	int32 GetResolvedBuildTicks() const
	{
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
