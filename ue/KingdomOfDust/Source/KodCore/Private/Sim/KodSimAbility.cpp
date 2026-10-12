#include "Sim/KodSimSubsystem.h"

float UKodSimSubsystem::EffectiveMoveSpeed(const FKodSimEntityState& State) const
{
	const int32 Permille = FMath::Clamp(State.AbilityMovePermille, 1, 10000);
	return State.MoveSpeed * (static_cast<float>(Permille) / 1000.f);
}

void UKodSimSubsystem::RegisterUnitAbility(FName UnitDefinitionId, const FKodAbilitySpec& Spec)
{
	if (UnitDefinitionId.IsNone() || Spec.AbilityId.IsNone())
	{
		return;
	}
	AbilitySpecs.Add(Spec.AbilityId, Spec);
	TArray<FName>& List = UnitAbilities.FindOrAdd(UnitDefinitionId);
	List.AddUnique(Spec.AbilityId);
}

bool UKodSimSubsystem::TryGetAbilitySpec(FName AbilityId, FKodAbilitySpec& OutSpec) const
{
	if (const FKodAbilitySpec* Found = AbilitySpecs.Find(AbilityId))
	{
		OutSpec = *Found;
		return true;
	}
	return false;
}

bool UKodSimSubsystem::UnitHasAbility(FName UnitDefinitionId, FName AbilityId) const
{
	const TArray<FName>* List = UnitAbilities.Find(UnitDefinitionId);
	return List && List->Contains(AbilityId);
}

bool UKodSimSubsystem::UseAbility(FKodEntityId UnitId, FName AbilityId)
{
	FKodSimEntityState* State = States.Find(UnitId.Value);
	const FKodAbilitySpec* Spec = AbilitySpecs.Find(AbilityId);
	if (!State || State->Health <= 0.f || !Spec)
	{
		return false;
	}
	if (!UnitHasAbility(State->DefinitionId, AbilityId))
	{
		return false;
	}
	if (State->AbilityCooldownTicks > 0)
	{
		return false;
	}
	const int32 HealthCost = FMath::Max(0, Spec->HealthCost);
	if (HealthCost > 0 && State->Health <= static_cast<float>(HealthCost))
	{
		return false;
	}
	FKodResourceCost Price;
	Price.Jadeite = FMath::Max(0, Spec->Credits);
	Price.Luminene = FMath::Max(0, Spec->Luminene);
	if ((Price.Jadeite > 0 || Price.Luminene > 0) && !Spend(State->TeamId, Price))
	{
		return false;
	}
	if (HealthCost > 0)
	{
		State->Health = FMath::Max(1.f, State->Health - static_cast<float>(HealthCost));
	}
	State->ActiveAbilityId = Spec->AbilityId;
	State->AbilityTicksRemaining = FMath::Max(0, Spec->DurationTicks);
	State->AbilityCooldownTicks = FMath::Max(0, Spec->CooldownTicks);
	State->AbilityMovePermille = FMath::Clamp(Spec->MovePermille, 1, 10000);
	State->AbilityAttackPermille = FMath::Clamp(Spec->AttackPermille, 1, 10000);
	UE_LOG(LogTemp, Log, TEXT("KodAbility Use Id=%s Unit=%d"), *Spec->AbilityId.ToString(), State->Id.Value);
	return true;
}

void UKodSimSubsystem::StepAbilities()
{
	TArray<int32> Keys;
	States.GetKeys(Keys);
	Keys.Sort();
	for (int32 Key : Keys)
	{
		FKodSimEntityState* State = States.Find(Key);
		if (!State)
		{
			continue;
		}
		if (State->AbilityCooldownTicks > 0)
		{
			--State->AbilityCooldownTicks;
		}
		if (State->AbilityTicksRemaining <= 0)
		{
			continue;
		}
		--State->AbilityTicksRemaining;
		if (State->AbilityTicksRemaining <= 0)
		{
			State->AbilityTicksRemaining = 0;
			State->ActiveAbilityId = NAME_None;
			State->AbilityMovePermille = 1000;
			State->AbilityAttackPermille = 1000;
		}
	}
}
