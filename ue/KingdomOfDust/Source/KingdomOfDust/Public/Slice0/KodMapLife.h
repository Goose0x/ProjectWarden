#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KodMapLife.generated.h"

class UNiagaraSystem;
class UParticleSystem;

/** Presentation dressing. Missing soft assets are skipped. Never written into the sim. */
UCLASS()
class KINGDOMOFDUST_API AKodMapLife : public AActor
{
	GENERATED_BODY()

public:
	AKodMapLife();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	void SetAmbientEnabled(bool bEnabled);
	bool IsAmbientEnabled() const { return bAmbient; }

private:
	UFUNCTION()
	void HandleFired(AActor* Attacker, AActor* Target, int32 SimTick);

	UFUNCTION()
	void HandleHit(AActor* Attacker, AActor* Target, float Health, int32 TargetId);

	UFUNCTION()
	void HandleKilled(AActor* Target, int32 TargetId);

	UFUNCTION()
	void HandleCue(FName CueId, FVector Location, bool bAtLocation);

	void DiscoverAssets();
	void ScatterDressing();
	void TickSparks(float Now);
	void SpawnBurst(UNiagaraSystem* Niagara, UParticleSystem* Cascade, const FVector& Location, const FRotator& Rotation, float Scale) const;

	bool bAmbient = true;
	bool bBound = false;

	UPROPERTY()
	TObjectPtr<UNiagaraSystem> Muzzle;

	UPROPERTY()
	TObjectPtr<UNiagaraSystem> Hit;

	UPROPERTY()
	TObjectPtr<UNiagaraSystem> ExplosionMain;

	UPROPERTY()
	TObjectPtr<UNiagaraSystem> ExplosionGround;

	UPROPERTY()
	TObjectPtr<UNiagaraSystem> ExplosionSmoke;

	UPROPERTY()
	TObjectPtr<UNiagaraSystem> FootstepNiagara;

	UPROPERTY()
	TObjectPtr<UParticleSystem> FootstepCascade;

	UPROPERTY()
	TObjectPtr<UNiagaraSystem> FogNiagara;

	UPROPERTY()
	TObjectPtr<UParticleSystem> FogCascade;

	UPROPERTY()
	TObjectPtr<UNiagaraSystem> SteamNiagara;

	UPROPERTY()
	TObjectPtr<UParticleSystem> SteamCascade;

	TMap<int32, FVector> LastStep;
	TMap<int32, float> NextStepTime;
	TMap<int32, float> NextSparkTime;
	TArray<TObjectPtr<USceneComponent>> Dressing;
};
