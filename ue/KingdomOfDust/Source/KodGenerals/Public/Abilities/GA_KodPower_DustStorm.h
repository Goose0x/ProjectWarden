#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_KodPower_DustStorm.generated.h"

/**
 * M1 sample general power — Dust Storm.
 * Activate → brief area effect at target location (Niagara cue later).
 * Tag: Kod.Power.DustStorm
 */
UCLASS()
class KODGENERALS_API UGA_KodPower_DustStorm : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_KodPower_DustStorm();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	UPROPERTY(EditDefaultsOnly, Category = "Kod|Power")
	float Radius = 600.f;

	UPROPERTY(EditDefaultsOnly, Category = "Kod|Power")
	float DurationSeconds = 3.f;

	UPROPERTY(EditDefaultsOnly, Category = "Kod|Power")
	float CooldownSeconds = 30.f;
};
