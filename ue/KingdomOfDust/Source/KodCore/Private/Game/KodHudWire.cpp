#include "Game/KodHUD.h"

#include "Engine/Texture2D.h"
#include "Game/KodHudLayout.h"
#include "Game/KodUiArt.h"
#include "Game/KodWireTint.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Warden/KodWardenPaths.h"

namespace
{
	UTexture2D* MakeFlatIcon()
	{
		constexpr int32 Texels = KodHudLayout::IconSourceTexels;
		UTexture2D* Tex = UTexture2D::CreateTransient(Texels, Texels, PF_B8G8R8A8);
		if (!Tex)
		{
			return nullptr;
		}
		Tex->SRGB = true;
		Tex->Filter = TF_Bilinear;
		Tex->AddressX = TA_Clamp;
		Tex->AddressY = TA_Clamp;
		Tex->NeverStream = true;
		FTexturePlatformData* Platform = Tex->GetPlatformData();
		if (!Platform || Platform->Mips.Num() == 0)
		{
			return nullptr;
		}
		uint8* Bits = static_cast<uint8*>(Platform->Mips[0].BulkData.Lock(LOCK_READ_WRITE));
		for (int32 PixelY = 0; PixelY < Texels; ++PixelY)
		{
			for (int32 PixelX = 0; PixelX < Texels; ++PixelX)
			{
				const bool bEdge = PixelX < 4 || PixelY < 4 || PixelX >= Texels - 4 || PixelY >= Texels - 4;
				const int32 Offset = (PixelY * Texels + PixelX) * 4;
				Bits[Offset + 0] = bEdge ? 0x3A : 0x5C;
				Bits[Offset + 1] = bEdge ? 0x3F : 0x64;
				Bits[Offset + 2] = bEdge ? 0x45 : 0x6C;
				Bits[Offset + 3] = 255;
			}
		}
		Platform->Mips[0].BulkData.Unlock();
		Tex->UpdateResource();
		return Tex;
	}
}

UTexture2D* AKodHUD::ResolveArtTexture(FName Id, bool bWireframe)
{
	return ResolveArtTexture(Id, bWireframe ? FKodUiArt::EKind::Wire160 : FKodUiArt::EKind::Icon);
}

UTexture2D* AKodHUD::ResolveArtTexture(FName Id, FKodUiArt::EKind Kind)
{
	if (Id.IsNone())
	{
		return nullptr;
	}
	const bool bIcon = Kind == FKodUiArt::EKind::Icon;
	TMap<FName, TObjectPtr<UTexture2D>>& Hits = bIcon ? IconHits : WireHits;
	TSet<FName>& Miss = bIcon ? IconMiss : WireMiss;
	const FSoftObjectPath Path = FKodUiArt::Resolve(Id, Kind);
	const FName PathKey(*Path.ToString());
	if (PathKey.IsNone() || Miss.Contains(PathKey))
	{
		return nullptr;
	}
	if (const TObjectPtr<UTexture2D>* Found = Hits.Find(PathKey))
	{
		return Found->Get();
	}
	UTexture2D* Texture = Cast<UTexture2D>(Path.TryLoad());
	if (!Texture)
	{
		Miss.Add(PathKey);
		return nullptr;
	}
	Hits.Add(PathKey, Texture);
	return Texture;
}

UMaterialInstanceDynamic* AKodHUD::LeaseWireMaterial()
{
	if (bWireMaterialMiss)
	{
		return nullptr;
	}
	if (!WireMaterial)
	{
		WireMaterial = Cast<UMaterialInterface>(FSoftObjectPath(KodWardenPaths::WireSilhouetteMaterial).TryLoad());
		if (!WireMaterial)
		{
			bWireMaterialMiss = true;
			return nullptr;
		}
	}
	if (WireMidCursor >= WireMidPool.Num())
	{
		if (WireMidPool.Num() >= 32)
		{
			return nullptr;
		}
		UMaterialInstanceDynamic* Created = UMaterialInstanceDynamic::Create(WireMaterial, this);
		if (!Created)
		{
			return nullptr;
		}
		WireMidPool.Add(Created);
	}
	UMaterialInstanceDynamic* Mid = WireMidPool[WireMidCursor];
	++WireMidCursor;
	return Mid;
}

void AKodHUD::DrawIconSlot(FName IconId, float X, float Y, float W, float H, float Opacity)
{
	if (!Canvas || W < 1.f || H < 1.f)
	{
		return;
	}
	const float Alpha = FMath::Clamp(Opacity, 0.f, 1.f);
	if (UTexture2D* Texture = ResolveArtTexture(IconId, false))
	{
		DrawTexture(Texture, X, Y, W, H, 0.f, 0.f, 1.f, 1.f, FLinearColor(1.f, 1.f, 1.f, Alpha));
		return;
	}
	if (!FlatIconPlaceholder)
	{
		FlatIconPlaceholder = MakeFlatIcon();
	}
	if (FlatIconPlaceholder)
	{
		DrawTexture(FlatIconPlaceholder, X, Y, W, H, 0.f, 0.f, 1.f, 1.f, FLinearColor(1.f, 1.f, 1.f, Alpha));
		return;
	}
	DrawRect(FLinearColor(0.36f, 0.39f, 0.42f, Alpha), X, Y, W, H);
}

void AKodHUD::DrawWireSilhouette(float X, float Y, float W, float H, FName DefinitionId, int32 Health, int32 MaxHealth, bool bStructure, bool bFlash, KodWireTint::ESlot Slot, int32 Shield, int32 ShieldMax, const TArray<float>* ZoneRemain)
{
	(void)Slot;
	if (!Canvas || W < 1.f || H < 1.f)
	{
		return;
	}
	TArray<KodWireTint::FZone> Zones;
	KodWireTint::BuildZones(bStructure, Zones);
	const bool bPerZone = ZoneRemain && KodWireTint::ApplyPerZone(Zones, *ZoneRemain);
	if (!bPerZone)
	{
		KodWireTint::ApplyOverallHp(Zones, Health, MaxHealth);
	}

	const float Rim = KodWireTint::ShieldRimStrength(Shield, ShieldMax);
	const FLinearColor RimColor = KodWireTint::ShieldRimColor();
	const UKodHudProposed& Style = HudStyle();
	const int32 WireHealthy = Style.WireHealthyPermille;
	const int32 WireWarning = Style.WireWarningPermille;
	const FLinearColor OverallTint = KodWireTint::FromHitPointsPermille(Health, MaxHealth, WireHealthy, WireWarning, false);
	UTexture2D* IconTexture = ResolveArtTexture(DefinitionId, FKodUiArt::EKind::Icon);
	UTexture2D* ZoneMask = bPerZone ? ResolveArtTexture(DefinitionId, FKodUiArt::EKind::ZoneMask) : nullptr;
	bool bDrewMaterial = false;
	if (UMaterialInstanceDynamic* Mid = IconTexture ? LeaseWireMaterial() : nullptr)
	{
		static const FName ZoneRemainName[KodWireTint::MaterialZoneCount] = {
			TEXT("ZoneRemain0"), TEXT("ZoneRemain1"), TEXT("ZoneRemain2"), TEXT("ZoneRemain3")
		};
		static const FName ZoneTintName[KodWireTint::MaterialZoneCount] = {
			TEXT("ZoneTint0"), TEXT("ZoneTint1"), TEXT("ZoneTint2"), TEXT("ZoneTint3")
		};
		Mid->SetTextureParameterValue(TEXT("Silhouette"), IconTexture);
		if (ZoneMask)
		{
			Mid->SetTextureParameterValue(TEXT("ZoneMask"), ZoneMask);
		}
		for (int32 Index = 0; Index < KodWireTint::MaterialZoneCount; ++Index)
		{
			const bool bLive = bPerZone && ZoneMask && Zones.IsValidIndex(Index);
			const float Remain = bLive ? Zones[Index].Remain : 1.f;
			const FLinearColor Tint = bLive ? KodWireTint::FromRemainPermille(Zones[Index].Remain, WireHealthy, WireWarning, false) : OverallTint;
			Mid->SetScalarParameterValue(ZoneRemainName[Index], Remain);
			Mid->SetVectorParameterValue(ZoneTintName[Index], Tint);
		}
		Mid->SetScalarParameterValue(TEXT("ShieldRim"), Rim);
		Mid->SetVectorParameterValue(TEXT("ShieldRimColor"), RimColor);
		DrawMaterial(Mid, X, Y, W, H, 0.f, 0.f, 1.f, 1.f);
		bDrewMaterial = true;
	}
	else if (IconTexture)
	{
		DrawTexture(IconTexture, X, Y, W, H, 0.f, 0.f, 1.f, 1.f, OverallTint);
	}
	else
	{
		for (const KodWireTint::FZone& Zone : Zones)
		{
			DrawRect(
				KodWireTint::FromRemainPermille(Zone.Remain, WireHealthy, WireWarning, false),
				X + Zone.U0 * W,
				Y + Zone.V0 * H,
				(Zone.U1 - Zone.U0) * W,
				(Zone.V1 - Zone.V0) * H);
		}
	}
	if (Rim > 0.f && !bDrewMaterial)
	{
		const float Thickness = FMath::Max(1.f, 2.f * Rim);
		const FLinearColor Glow = RimColor.CopyWithNewOpacity(Rim);
		DrawRect(Glow, X, Y, W, Thickness);
		DrawRect(Glow, X, Y + H - Thickness, W, Thickness);
		DrawRect(Glow, X, Y, Thickness, H);
		DrawRect(Glow, X + W - Thickness, Y, Thickness, H);
	}
	if (bFlash)
	{
		DrawRect(KodWireTint::Critical().CopyWithNewOpacity(0.45f), X, Y, W, H);
	}
}
