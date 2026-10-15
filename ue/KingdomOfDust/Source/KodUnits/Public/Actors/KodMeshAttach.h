#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "KodMeshAttach.generated.h"

class USceneComponent;
class USkeletalMeshComponent;

/**
 * Visual pieces hung on a building body from [KodMeshes] +Attach rows.
 * The bay ramp's open and close are presentation only. They do not write sim state.
 */
UCLASS()
class KODUNITS_API UKodMeshAttachComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UKodMeshAttachComponent();

	void SetRamp(USkeletalMeshComponent* InRamp, const FString& IdleClosed, const FString& Open, const FString& Close, const FString& IdleOpen);
	void ClearRamp();
	void NotifyTrainExit();
	bool TryGetBayPoint(FVector& OutWorld) const;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	bool PlayClip(const FString& Path, bool bLoop, float& OutLength);

	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> RampMesh;

	FString IdleClosedPath;
	FString OpenPath;
	FString ClosePath;
	FString IdleOpenPath;
	float Age = -1.f;
	float CloseLength = 1.2f;
	float CloseStarted = 0.f;
	bool bHeldOpen = false;
	bool bClosing = false;
};

namespace KodMeshAttach
{
	/** Primary bodies only. A fallback body drops any previous pieces. */
	KODUNITS_API void Apply(AActor* Actor, USceneComponent* Body, FName OwnerId, bool bPrimaryBody);
}
