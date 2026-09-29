#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Sim/KodEntityId.h"
#include "KodSelectionManager.generated.h"

class AActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FKodSelectionChangedSignature);

/**
 * Local player selection set. Box-select / click-select feed this;
 * command card and control groups read from it.
 */
UCLASS()
class KODUI_API UKodSelectionManager : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable, Category = "Kod|Selection")
	void ClearSelection();

	UFUNCTION(BlueprintCallable, Category = "Kod|Selection")
	void SelectActor(AActor* Actor, bool bAddToSelection = false);

	UFUNCTION(BlueprintCallable, Category = "Kod|Selection")
	void SelectActors(const TArray<AActor*>& Actors, bool bAddToSelection = false);

	UFUNCTION(BlueprintCallable, Category = "Kod|Selection")
	void DeselectActor(AActor* Actor);

	/** Resolved selection. Stale weak refs are omitted; SelectedActors stays weak. */
	UFUNCTION(BlueprintPure, Category = "Kod|Selection")
	TArray<AActor*> GetSelectedActors() const;

	UFUNCTION(BlueprintPure, Category = "Kod|Selection")
	TArray<FKodEntityId> GetSelectedEntityIds() const;

	UFUNCTION(BlueprintPure, Category = "Kod|Selection")
	int32 GetSelectionCount() const { return SelectedActors.Num(); }

	UPROPERTY(BlueprintAssignable, Category = "Kod|Selection")
	FKodSelectionChangedSignature OnSelectionChanged;

	/** Begin / update / end screen-space box select (pixels). */
	UFUNCTION(BlueprintCallable, Category = "Kod|Selection")
	void BeginBoxSelect(FVector2D ScreenPos);

	UFUNCTION(BlueprintCallable, Category = "Kod|Selection")
	void UpdateBoxSelect(FVector2D ScreenPos);

	UFUNCTION(BlueprintCallable, Category = "Kod|Selection")
	void EndBoxSelect(bool bAddToSelection);

protected:
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> SelectedActors;

	bool bBoxSelecting = false;
	FVector2D BoxStart = FVector2D::ZeroVector;
	FVector2D BoxEnd = FVector2D::ZeroVector;
};
