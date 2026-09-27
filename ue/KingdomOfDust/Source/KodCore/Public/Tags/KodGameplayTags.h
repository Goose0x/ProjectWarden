#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

/**
 * Native gameplay tag helpers for Kingdom of Dust.
 * Config tags also live in Config/DefaultGameplayTags.ini.
 */
namespace KodGameplayTags
{
	KODCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Kod_Unit);
	KODCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Kod_Unit_Soldier);
	KODCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Kod_Unit_Ranger);
	KODCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Kod_Unit_Dozer);
	KODCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Kod_Building);
	KODCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Kod_Building_Barracks);
	KODCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Kod_Building_CommandCenter);
	KODCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Kod_Power);
	KODCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Kod_Power_DustStorm);
	KODCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Kod_Resource_DustCrystal);
}
