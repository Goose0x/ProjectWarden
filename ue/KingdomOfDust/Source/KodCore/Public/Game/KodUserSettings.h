#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "KodUserSettings.generated.h"

/** Persisted under GameUserSettings.ini. Volumes are 0 to 1. */
UCLASS(Config = GameUserSettings)
class KODCORE_API UKodUserSettings : public UObject
{
	GENERATED_BODY()

public:
	static UKodUserSettings* Get();

	void Apply(UWorld* World) const;
	void SaveNow();

	UPROPERTY(Config)
	float MasterVolume = 1.f;

	UPROPERTY(Config)
	float MusicVolume = 1.f;

	UPROPERTY(Config)
	float SfxVolume = 1.f;

	UPROPERTY(Config)
	float VoiceVolume = 1.f;

	UPROPERTY(Config)
	float EdgeScrollSpeed = 4800.f;

	UPROPERTY(Config)
	float KeyboardScrollSpeed = 4800.f;

	UPROPERTY(Config)
	float MouseSensitivity = 1.f;

	/** 0 selected, 1 damaged, 2 always. */
	UPROPERTY(Config)
	int32 HealthBarMode = 1;

	UPROPERTY(Config)
	bool bShowHotkeyGrid = false;

	/** Scalability 0–3. */
	UPROPERTY(Config)
	int32 GraphicsQuality = 2;

	/** Light shake on artillery and building deaths. */
	UPROPERTY(Config)
	bool bCameraShake = true;

	/** Floating damage numbers. Off until the player turns them on. */
	UPROPERTY(Config)
	bool bShowDamageNumbers = false;
};
