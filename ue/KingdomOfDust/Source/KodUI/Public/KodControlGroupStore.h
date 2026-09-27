#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Sim/KodEntityId.h"
#include "KodControlGroupStore.generated.h"

/**
 * Control groups 0–9 (SC2-like). Ctrl+# assign; # recall.
 */
UCLASS()
class KODUI_API UKodControlGroupStore : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	static constexpr int32 NumGroups = 10;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable, Category = "Kod|ControlGroups")
	void AssignGroup(int32 GroupIndex, const TArray<FKodEntityId>& Entities);

	UFUNCTION(BlueprintCallable, Category = "Kod|ControlGroups")
	void AddToGroup(int32 GroupIndex, const TArray<FKodEntityId>& Entities);

	UFUNCTION(BlueprintCallable, Category = "Kod|ControlGroups")
	TArray<FKodEntityId> GetGroup(int32 GroupIndex) const;

	UFUNCTION(BlueprintCallable, Category = "Kod|ControlGroups")
	void ClearGroup(int32 GroupIndex);

	UFUNCTION(BlueprintCallable, Category = "Kod|ControlGroups")
	void RecallGroup(int32 GroupIndex, bool bAddToSelection);

protected:
	TArray<FKodEntityId> Groups[NumGroups];
};
