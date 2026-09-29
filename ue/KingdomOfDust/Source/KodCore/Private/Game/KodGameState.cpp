#include "Game/KodGameState.h"

AKodGameState::AKodGameState()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AKodGameState::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	MatchTimeSeconds += DeltaSeconds;
}
