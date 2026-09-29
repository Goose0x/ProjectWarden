#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Sim/KodBuildTicks.h"
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Economy")
	int32 BuildCostDust = 50;

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
