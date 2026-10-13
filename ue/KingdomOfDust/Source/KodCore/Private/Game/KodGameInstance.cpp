#include "Game/KodGameInstance.h"

UKodGameInstance::UKodGameInstance()
{
}

void UKodGameInstance::Init()
{
	Super::Init();
}

void UKodGameInstance::Shutdown()
{
	RemoteSessionQuery.Unbind();
	Super::Shutdown();
}

void UKodGameInstance::BindRemoteSessionQuery(FKodRemoteSessionQuery Query)
{
	RemoteSessionQuery = MoveTemp(Query);
}

void UKodGameInstance::ClearRemoteSessionQuery()
{
	RemoteSessionQuery.Unbind();
}

bool UKodGameInstance::IsRemoteOnlineSession() const
{
	return RemoteSessionQuery.IsBound() && RemoteSessionQuery.Execute();
}
