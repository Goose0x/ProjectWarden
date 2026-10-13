#include "Game/KodUserSettings.h"

#include "AudioDevice.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/KodHUD.h"
#include "Game/KodRTSCameraPawn.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
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

bool UKodUserSettings::EnsureVolumeMix()
{
	if (!MusicClass)
	{
		MusicClass = LoadObject<USoundClass>(nullptr, TEXT("/Game/Audio/Classes/KodMusic.KodMusic"), nullptr, LOAD_NoWarn);
	}
	if (!SfxClass)
	{
		SfxClass = LoadObject<USoundClass>(nullptr, TEXT("/Game/Audio/Classes/KodSFX.KodSFX"), nullptr, LOAD_NoWarn);
	}
	if (!VoiceClass)
	{
		VoiceClass = LoadObject<USoundClass>(nullptr, TEXT("/Game/Audio/Classes/KodVoice.KodVoice"), nullptr, LOAD_NoWarn);
	}
	if (!MusicClass || !SfxClass || !VoiceClass)
	{
		if (!bLoggedMissingBuses)
		{
			bLoggedMissingBuses = true;
			UE_LOG(LogTemp, Log, TEXT("KodAudio buses missing; volume mix not pushed"));
		}
		return false;
	}
	if (VolumeMix)
	{
		return true;
	}
	VolumeMix = NewObject<USoundMix>(this, TEXT("KodVolumeMix"));
	auto AddBus = [this](USoundClass* Class, float Volume)
	{
		FSoundClassAdjuster Adjust;
		Adjust.SoundClassObject = Class;
		Adjust.VolumeAdjuster = FMath::Clamp(Volume, 0.f, 1.f);
		Adjust.PitchAdjuster = 1.f;
		Adjust.bApplyToChildren = true;
		VolumeMix->SoundClassEffects.Add(Adjust);
	};
	AddBus(MusicClass, MusicVolume);
	AddBus(SfxClass, SfxVolume);
	AddBus(VoiceClass, VoiceVolume);
	return true;
}

void UKodUserSettings::ApplyVolumes(UWorld* World)
{
	if (FParse::Param(FCommandLine::Get(), TEXT("nosound")))
	{
		return;
	}
	FAudioDeviceHandle Audio = World->GetAudioDevice();
	if (!Audio.IsValid())
	{
		return;
	}
	Audio->SetTransientPrimaryVolume(FMath::Clamp(MasterVolume, 0.f, 1.f));
	if (!EnsureVolumeMix())
	{
		return;
	}
	if (VolumeMixWorld.Get() != World)
	{
		UGameplayStatics::PushSoundMixModifier(World, VolumeMix);
		VolumeMixWorld = World;
	}
	UGameplayStatics::SetSoundMixClassOverride(World, VolumeMix, MusicClass, FMath::Clamp(MusicVolume, 0.f, 1.f), 1.f, 0.f, true);
	UGameplayStatics::SetSoundMixClassOverride(World, VolumeMix, SfxClass, FMath::Clamp(SfxVolume, 0.f, 1.f), 1.f, 0.f, true);
	UGameplayStatics::SetSoundMixClassOverride(World, VolumeMix, VoiceClass, FMath::Clamp(VoiceVolume, 0.f, 1.f), 1.f, 0.f, true);
}

void UKodUserSettings::Apply(UWorld* World)
{
	if (!World)
	{
		return;
	}
	ApplyVolumes(World);
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
