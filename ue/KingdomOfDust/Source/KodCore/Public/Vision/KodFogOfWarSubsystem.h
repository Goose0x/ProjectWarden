#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "KodFogOfWarSubsystem.generated.h"

/**
 * Grid-based fog of war stub (M1).
 * Reveal cells near friendly vision sources; query visibility for actors / locations.
 * Presentation (last-seen ghosts) is a later layer.
 */
UCLASS()
class KODCORE_API UKodFogOfWarSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** Cell size in UU. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|FOW")
	float CellSize = 200.f;

	/** Grid extent from world origin (± half cells). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|FOW")
	int32 GridHalfExtent = 128;

	UFUNCTION(BlueprintCallable, Category = "Kod|FOW")
	void ConfigureGrid(float InCellSize, int32 InHalfExtent);

	/** Mark cells within Radius of WorldLocation as revealed for TeamId. */
	UFUNCTION(BlueprintCallable, Category = "Kod|FOW")
	void RevealAt(FVector WorldLocation, float Radius, int32 TeamId);

	UFUNCTION(BlueprintCallable, Category = "Kod|FOW")
	bool IsLocationVisible(FVector WorldLocation, int32 TeamId) const;

	UFUNCTION(BlueprintCallable, Category = "Kod|FOW")
	bool IsActorVisible(AActor* Actor, int32 ObserverTeamId) const;

	/** Clear all reveal for a team (e.g. match reset). */
	UFUNCTION(BlueprintCallable, Category = "Kod|FOW")
	void ResetVision(int32 TeamId);

protected:
	int32 CoordToIndex(int32 X, int32 Y) const;
	void WorldToCell(FVector WorldLocation, int32& OutX, int32& OutY) const;

	/** Per-team reveal bitsets (packed bools). Index = Y * Width + X. */
	TMap<int32, TArray<uint8>> TeamReveal;

	int32 GridWidth = 0;
};
