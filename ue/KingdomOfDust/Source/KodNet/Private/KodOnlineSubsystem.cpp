#include "KodOnlineSubsystem.h"

#include "Game/KodGameInstance.h"

void UKodOnlineSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Service = MakeUnique<FKodNullOnlineService>();
	if (UKodGameInstance* Instance = Cast<UKodGameInstance>(GetGameInstance()))
	{
		Instance->BindRemoteSessionQuery(FKodRemoteSessionQuery::CreateWeakLambda(this, [this]()
		{
			return QueryRemoteSession();
		}));
	}
	UE_LOG(LogTemp, Log, TEXT("KodOnline Subsystem=%s Remote=%d"),
		*Service->GetSubsystemName().ToString(),
		Service->IsRemoteBackend() ? 1 : 0);
}

void UKodOnlineSubsystem::Deinitialize()
{
	if (UKodGameInstance* Instance = Cast<UKodGameInstance>(GetGameInstance()))
	{
		Instance->ClearRemoteSessionQuery();
	}
	Service.Reset();
	Super::Deinitialize();
}

bool UKodOnlineSubsystem::QueryRemoteSession() const
{
	return Service.IsValid() && Service->IsRemoteBackend();
}

IKodOnlineService& UKodOnlineSubsystem::GetService()
{
	if (!Service)
	{
		Service = MakeUnique<FKodNullOnlineService>();
	}
	return *Service;
}
