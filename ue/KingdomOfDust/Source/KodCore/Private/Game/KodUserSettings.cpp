#include "Game/KodUserSettings.h"

#include "AudioDevice.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/KodHUD.h"
#include "Game/KodRTSCameraPawn.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"

UKodUserSettings* UKodUserSettings::Get()
{
	static UKodUserSettings* Settings = nullptr;
	if (!Settings)
	{
		Settings = NewObject<UKodUserSettings>(GetTransientPackage(), TEXT("KodUserSettings"));
		Settings->AddToRoot();
		Settings->LoadConfig();
	}
	return Settings;
}

void UKodUserSettings::SaveNow()
{
	SaveConfig();
	if (UGameUserSettings* User = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		User->SetOverallScalabilityLevel(FMath::Clamp(GraphicsQuality, 0, 3));
		User->ApplyNonResolutionSettings();
		User->SaveSettings();
	}
}

void UKodUserSettings::Apply(UWorld* World) const
{
	if (!World)
	{
		return;
	}
	if (FAudioDeviceHandle Audio = World->GetAudioDevice())
	{
		Audio->SetTransientPrimaryVolume(FMath::Clamp(MasterVolume, 0.f, 1.f));
	}
	USoundMix* Mix = NewObject<USoundMix>(GetTransientPackage());
	auto PushClass = [Mix](const TCHAR* Name, float Volume)
	{
		USoundClass* Class = NewObject<USoundClass>(GetTransientPackage(), FName(Name));
		FSoundClassAdjuster Adjust;
		Adjust.SoundClassObject = Class;
		Adjust.VolumeAdjuster = FMath::Clamp(Volume, 0.f, 1.f);
		Adjust.bApplyToChildren = true;
		Mix->SoundClassEffects.Add(Adjust);
	};
	PushClass(TEXT("KodMusic"), MusicVolume);
	PushClass(TEXT("KodSFX"), SfxVolume);
	PushClass(TEXT("KodVoice"), VoiceVolume);
	UGameplayStatics::PushSoundMixModifier(World, Mix);
	if (UGameUserSettings* User = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		User->SetOverallScalabilityLevel(FMath::Clamp(GraphicsQuality, 0, 3));
		User->ApplyNonResolutionSettings();
	}
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC)
		{
			continue;
		}
		if (AKodRTSCameraPawn* CameraPawn = Cast<AKodRTSCameraPawn>(PC->GetPawn()))
		{
			CameraPawn->EdgeScrollSpeed = EdgeScrollSpeed;
			CameraPawn->KeyboardScrollSpeed = KeyboardScrollSpeed;
			CameraPawn->MouseSensitivity = MouseSensitivity;
		}
		if (AKodHUD* HUD = Cast<AKodHUD>(PC->GetHUD()))
		{
			HUD->SetHealthBarMode(HealthBarMode);
			HUD->SetHotkeyGrid(bShowHotkeyGrid);
		}
	}
}
