#include "Game/KodHudProposed.h"

const TCHAR* UKodHudProposed::AssetPath = TEXT("/Game/Warden/UI/DA_KodHudProposed.DA_KodHudProposed");

UKodHudProposed::UKodHudProposed()
{
	ShieldColor = FLinearColor(FColor(0x3A, 0x8D, 0xFF));
	ConstructionColor = FLinearColor(FColor(0x4F, 0xD8, 0xFF));
	ConstructionLabelColor = FLinearColor(FColor(0xBF, 0xF1, 0xFF));
	SupplyLatticeColor = FLinearColor(FColor(0x2F, 0x7B, 0xFF, 69));
	SupplyRingColor = FLinearColor(FColor(0x2F, 0x7B, 0xFF, 150));
	ToxicBlotchColor = FLinearColor(FColor(0x1B, 0x43, 0x32, 184));
}
