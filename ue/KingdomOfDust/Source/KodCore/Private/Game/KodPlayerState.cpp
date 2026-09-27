#include "Game/KodPlayerState.h"
#include "Net/UnrealNetwork.h"

AKodPlayerState::AKodPlayerState()
{
}

void AKodPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKodPlayerState, TeamId);
}
