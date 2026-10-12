#include "Actors/KodJadeiteVisual.h"

#include "Components/MeshComponent.h"
#include "Engine/World.h"
#include "Game/KodJadeitePalette.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Sim/KodResourceTypes.h"

namespace
{
	const TCHAR* JadeiteMaterialPath = TEXT("/Game/Warden/Materials/M_KodJadeite.M_KodJadeite");
	const TCHAR* BasicShapeMaterialPath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");

	bool NameHasJade(const FString& Name)
	{
		return Name.Contains(TEXT("Jade"), ESearchCase::IgnoreCase);
	}

	bool IsEyeOrRegen(const FString& Name)
	{
		return Name.Equals(TEXT("EyeEmissive"), ESearchCase::IgnoreCase)
			|| Name.Equals(TEXT("EyeGlow"), ESearchCase::IgnoreCase)
			|| Name.Equals(TEXT("EyeColor"), ESearchCase::IgnoreCase)
			|| Name.Equals(TEXT("RegenGlow"), ESearchCase::IgnoreCase)
			|| Name.Equals(TEXT("RegenEmissive"), ESearchCase::IgnoreCase);
	}

	bool IsTunerScalar(const FString& Name)
	{
		return Name.Equals(TEXT("JadeHueShift"), ESearchCase::IgnoreCase)
			|| Name.Equals(TEXT("JadeSaturation"), ESearchCase::IgnoreCase)
			|| Name.Equals(TEXT("JadeBrightness"), ESearchCase::IgnoreCase)
			|| Name.Equals(TEXT("JadeEmissive"), ESearchCase::IgnoreCase);
	}

	FLinearColor ColorForParameter(const UKodJadeitePaletteSubsystem* Palette, const FString& Name)
	{
		if (Name.Contains(TEXT("Shadow"), ESearchCase::IgnoreCase))
		{
			return Palette->TunedShadow();
		}
		if (Name.Contains(TEXT("Lit"), ESearchCase::IgnoreCase))
		{
			return Palette->TunedLit();
		}
		if (Name.Contains(TEXT("Rim"), ESearchCase::IgnoreCase))
		{
			return Palette->TunedRim();
		}
		if (Name.Contains(TEXT("Core"), ESearchCase::IgnoreCase) || IsEyeOrRegen(Name) || Name.Contains(TEXT("Emissive"), ESearchCase::IgnoreCase) || Name.Contains(TEXT("Glow"), ESearchCase::IgnoreCase))
		{
			return Palette->TunedEmissive();
		}
		return Palette->TunedBase();
	}

	bool SourceHasJadeParameter(const UMaterialInterface* Source)
	{
		if (!Source)
		{
			return false;
		}
		TArray<FMaterialParameterInfo> Infos;
		TArray<FGuid> Ids;
		Source->GetAllParameterInfoOfType(EMaterialParameterType::Vector, Infos, Ids);
		for (const FMaterialParameterInfo& Info : Infos)
		{
			const FString Name = Info.Name.ToString();
			if (NameHasJade(Name) || IsEyeOrRegen(Name))
			{
				return true;
			}
		}
		Infos.Reset();
		Ids.Reset();
		Source->GetAllParameterInfoOfType(EMaterialParameterType::Scalar, Infos, Ids);
		for (const FMaterialParameterInfo& Info : Infos)
		{
			const FString Name = Info.Name.ToString();
			if (NameHasJade(Name) || IsEyeOrRegen(Name) || IsTunerScalar(Name))
			{
				return true;
			}
		}
		return false;
	}

	void WriteTunedParameters(UMaterialInstanceDynamic* Mid, const UKodJadeitePaletteSubsystem* Palette)
	{
		TArray<FMaterialParameterInfo> Infos;
		TArray<FGuid> Ids;
		Mid->GetAllParameterInfoOfType(EMaterialParameterType::Vector, Infos, Ids);
		for (const FMaterialParameterInfo& Info : Infos)
		{
			const FString Name = Info.Name.ToString();
			if (NameHasJade(Name) || IsEyeOrRegen(Name))
			{
				Mid->SetVectorParameterValue(Info.Name, ColorForParameter(Palette, Name));
			}
		}
		Infos.Reset();
		Ids.Reset();
		Mid->GetAllParameterInfoOfType(EMaterialParameterType::Scalar, Infos, Ids);
		for (const FMaterialParameterInfo& Info : Infos)
		{
			const FString Name = Info.Name.ToString();
			if (Name.Equals(TEXT("JadeHueShift"), ESearchCase::IgnoreCase))
			{
				Mid->SetScalarParameterValue(Info.Name, Palette->GetHueShift());
			}
			else if (Name.Equals(TEXT("JadeSaturation"), ESearchCase::IgnoreCase))
			{
				Mid->SetScalarParameterValue(Info.Name, Palette->GetSaturation());
			}
			else if (Name.Equals(TEXT("JadeBrightness"), ESearchCase::IgnoreCase))
			{
				Mid->SetScalarParameterValue(Info.Name, Palette->GetBrightness());
			}
			else if (Name.Equals(TEXT("JadeEmissive"), ESearchCase::IgnoreCase) || IsEyeOrRegen(Name))
			{
				Mid->SetScalarParameterValue(Info.Name, Palette->GetEmissive());
			}
		}
	}

	UMaterialInterface* JadeiteMaterial()
	{
		static bool bLoggedMiss = false;
		UMaterialInterface* Found = LoadObject<UMaterialInterface>(nullptr, JadeiteMaterialPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Found && !bLoggedMiss)
		{
			bLoggedMiss = true;
			UE_LOG(LogTemp, Log, TEXT("KodJadeite M_KodJadeite missing; node stand-in uses a dynamic tint. Run Tools/kod_jadeite_mpc_setup.py"));
		}
		return Found;
	}
}

bool MaterialIsJadeiteNode(const UMaterialInterface* Material)
{
	const UMaterialInterface* Cursor = Material;
	while (Cursor)
	{
		const FString Path = Cursor->GetPathName();
		if (Path.Contains(TEXT("MI_Jadeite_Node")))
		{
			return true;
		}
		const UMaterialInstance* Instance = Cast<UMaterialInstance>(Cursor);
		if (!Instance)
		{
			break;
		}
		Cursor = Instance->Parent;
	}
	return false;
}

bool KodJadeiteVisual::IsScinderId(FName DefinitionId)
{
	const FString Id = DefinitionId.ToString();
	return Id.Contains(TEXT("Scinder")) || Id.StartsWith(TEXT("SCN_"));
}

bool KodJadeiteVisual::IsScinderActor(const AActor* Actor)
{
	if (!Actor)
	{
		return false;
	}
	for (const FName& Tag : Actor->Tags)
	{
		const FString Text = Tag.ToString();
		if (Text.Contains(TEXT("Scinder")) || Text == TEXT("KodEnemyOutpost") || Text == TEXT("KodEnemyBase"))
		{
			return true;
		}
	}
	return false;
}

FLinearColor KodJadeiteVisual::TunedJadeBase(const UWorld* World)
{
	if (const UKodJadeitePaletteSubsystem* Palette = World ? World->GetSubsystem<UKodJadeitePaletteSubsystem>() : nullptr)
	{
		return Palette->TunedBase();
	}
	return KodResourceColors::Jadeite();
}

bool KodJadeiteVisual::UsesJadeiteNodeMaterial(const UMaterialInterface* Material)
{
	return MaterialIsJadeiteNode(Material);
}

bool KodJadeiteVisual::UsesJadeiteNodeMaterial(const UMeshComponent* Mesh)
{
	if (!Mesh)
	{
		return false;
	}
	const int32 Slots = FMath::Max(1, Mesh->GetNumMaterials());
	for (int32 Slot = 0; Slot < Slots; ++Slot)
	{
		if (MaterialIsJadeiteNode(Mesh->GetMaterial(Slot)))
		{
			return true;
		}
	}
	return false;
}

void KodJadeiteVisual::ApplyJadeiteNode(UMeshComponent* Mesh)
{
	if (!Mesh || UsesJadeiteNodeMaterial(Mesh))
	{
		return;
	}
	const UWorld* World = Mesh->GetWorld();
	const UKodJadeitePaletteSubsystem* Palette = World ? World->GetSubsystem<UKodJadeitePaletteSubsystem>() : nullptr;
	if (UMaterialInterface* Jade = JadeiteMaterial())
	{
		const int32 Slots = FMath::Max(1, Mesh->GetNumMaterials());
		for (int32 Slot = 0; Slot < Slots; ++Slot)
		{
			Mesh->SetMaterial(Slot, Jade);
		}
		return;
	}
	if (!Palette)
	{
		return;
	}
	UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr, BasicShapeMaterialPath);
	if (!Parent)
	{
		Parent = Mesh->GetMaterial(0);
	}
	if (!Parent)
	{
		return;
	}
	UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Parent, Mesh);
	if (!Mid)
	{
		return;
	}
	const FLinearColor Base = Palette->TunedBase();
	const FLinearColor Glow = Palette->TunedEmissive();
	Mid->SetVectorParameterValue(TEXT("Color"), Base);
	Mid->SetVectorParameterValue(TEXT("BaseColor"), Base);
	Mid->SetVectorParameterValue(TEXT("Tint"), Base);
	Mid->SetVectorParameterValue(TEXT("EmissiveColor"), Glow);
	const int32 Slots = FMath::Max(1, Mesh->GetNumMaterials());
	for (int32 Slot = 0; Slot < Slots; ++Slot)
	{
		Mesh->SetMaterial(Slot, Mid);
	}
}

void KodJadeiteVisual::ApplyTunedEmissive(UMeshComponent* Mesh)
{
	if (!Mesh)
	{
		return;
	}
	const UWorld* World = Mesh->GetWorld();
	const UKodJadeitePaletteSubsystem* Palette = World ? World->GetSubsystem<UKodJadeitePaletteSubsystem>() : nullptr;
	if (!Palette)
	{
		return;
	}
	const int32 Slots = FMath::Max(1, Mesh->GetNumMaterials());
	for (int32 Slot = 0; Slot < Slots; ++Slot)
	{
		UMaterialInterface* Current = Mesh->GetMaterial(Slot);
		if (!Current)
		{
			continue;
		}
		UMaterialInstanceDynamic* Mid = Cast<UMaterialInstanceDynamic>(Current);
		const UMaterialInterface* Source = Current;
		if (Mid)
		{
			Source = Mid->Parent;
		}
		if (!SourceHasJadeParameter(Source))
		{
			continue;
		}
		if (!Mid)
		{
			Mid = UMaterialInstanceDynamic::Create(Current, Mesh);
			if (!Mid)
			{
				continue;
			}
			Mesh->SetMaterial(Slot, Mid);
		}
		WriteTunedParameters(Mid, Palette);
	}
}
