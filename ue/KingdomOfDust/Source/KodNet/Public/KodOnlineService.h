#pragma once

#include "CoreMinimal.h"

/**
 * Session seam. Null today. A Steam or EOS implementation can replace it
 * without the menu knowing the SDK. No Steam config ships with this.
 */
class KODNET_API IKodOnlineService
{
public:
	virtual ~IKodOnlineService() = default;
	virtual FName GetSubsystemName() const = 0;
	virtual bool IsRemoteBackend() const = 0;
	virtual bool HostOfflineLobby() = 0;
};

class KODNET_API FKodNullOnlineService : public IKodOnlineService
{
public:
	virtual FName GetSubsystemName() const override;
	virtual bool IsRemoteBackend() const override;
	virtual bool HostOfflineLobby() override;
};
