#include "KodOnlineService.h"

#include "OnlineSubsystem.h"

FName FKodNullOnlineService::GetSubsystemName() const
{
	if (const IOnlineSubsystem* Online = IOnlineSubsystem::Get())
	{
		return Online->GetSubsystemName();
	}
	return FName(TEXT("NULL"));
}

bool FKodNullOnlineService::IsRemoteBackend() const
{
	const FName Name = GetSubsystemName();
	return !Name.IsNone() && Name != FName(TEXT("NULL"));
}

bool FKodNullOnlineService::HostOfflineLobby()
{
	UE_LOG(LogTemp, Log, TEXT("KodOnline Host offline subsystem=%s"), *GetSubsystemName().ToString());
	return true;
}
