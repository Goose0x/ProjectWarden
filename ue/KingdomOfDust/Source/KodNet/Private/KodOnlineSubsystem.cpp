#include "KodOnlineSubsystem.h"

void UKodOnlineSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Service = MakeUnique<FKodNullOnlineService>();
	UE_LOG(LogTemp, Log, TEXT("KodOnline Subsystem=%s Remote=%d"),
		*Service->GetSubsystemName().ToString(),
		Service->IsRemoteBackend() ? 1 : 0);
}

void UKodOnlineSubsystem::Deinitialize()
{
	Service.Reset();
	Super::Deinitialize();
}

IKodOnlineService& UKodOnlineSubsystem::GetService()
{
	if (!Service)
	{
		Service = MakeUnique<FKodNullOnlineService>();
	}
	return *Service;
}
