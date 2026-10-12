#include "KodLandscapeBuilder.h"

#include "Editor.h"
#include "Landscape.h"
#include "LandscapeLayerInfoObject.h"
#include "LandscapeProxy.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Paths.h"

namespace
{
	constexpr int32 SectionQuads = 63;
	constexpr int32 SectionsPerComponent = 2;
	constexpr int32 ComponentCount = 4;
	constexpr int32 QuadsPerComponent = SectionQuads * SectionsPerComponent;
	constexpr int32 Verts = ComponentCount * QuadsPerComponent + 1;
	constexpr uint16 FlatHeight = 32768;

	FString MakeObjectPath(const FString& AssetPath)
	{
		const FString Trimmed = AssetPath.TrimStartAndEnd();
		if (Trimmed.IsEmpty() || Trimmed.Contains(TEXT(".")))
		{
			return Trimmed;
		}
		return Trimmed + TEXT(".") + FPaths::GetCleanFilename(Trimmed);
	}
}

ALandscape* UKodLandscapeBuilder::CreateFaultlineLandscape(const FString& MaterialInstancePath, const TArray<FString>& LayerInfoPaths)
{
	if (!GEditor)
	{
		UE_LOG(LogTemp, Error, TEXT("KodLandscape CreateFaultlineLandscape requires the editor"));
		return nullptr;
	}
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World || World->WorldType != EWorldType::Editor)
	{
		UE_LOG(LogTemp, Error, TEXT("KodLandscape CreateFaultlineLandscape requires the editor world"));
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags = RF_Transactional;
	const FVector Origin(-11200.f, -11200.f, 0.f);
	const FVector Scale(44.444f, 44.444f, 100.f);
	ALandscape* Landscape = World->SpawnActor<ALandscape>(Origin, FRotator::ZeroRotator, Params);
	if (!Landscape)
	{
		UE_LOG(LogTemp, Error, TEXT("KodLandscape SpawnActor ALandscape failed"));
		return nullptr;
	}

	Landscape->ComponentSizeQuads = QuadsPerComponent;
	Landscape->SubsectionSizeQuads = SectionQuads;
	Landscape->NumSubsections = SectionsPerComponent;
	Landscape->SetLandscapeGuid(FGuid::NewGuid());
	Landscape->CreateLandscapeInfo();
	Landscape->SetActorTransform(FTransform(FQuat::Identity, Origin, Scale));

	const FString MaterialPath = MakeObjectPath(MaterialInstancePath);
	if (!MaterialPath.IsEmpty())
	{
		if (UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *MaterialPath))
		{
			Landscape->LandscapeMaterial = Material;
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("KodLandscape Material miss %s"), *MaterialPath);
		}
	}

	TArray<uint16> Height;
	Height.Init(FlatHeight, Verts * Verts);

	// Base edit layer. Paint layers live inside this GUID, not as extra map keys.
	const FGuid BaseLayerGuid;
	TMap<FGuid, TArray<uint16>> HeightPerLayer;
	HeightPerLayer.Add(BaseLayerGuid, MoveTemp(Height));

	TArray<FLandscapeImportLayerInfo> ImportLayers;
	ImportLayers.Reserve(LayerInfoPaths.Num());
	for (const FString& LayerInfoPath : LayerInfoPaths)
	{
		const FString LayerPath = MakeObjectPath(LayerInfoPath);
		ULandscapeLayerInfoObject* LayerObject = LayerPath.IsEmpty()
			? nullptr
			: LoadObject<ULandscapeLayerInfoObject>(nullptr, *LayerPath);
		if (!LayerObject)
		{
			UE_LOG(LogTemp, Error, TEXT("KodLandscape LayerInfo miss %s"), *LayerPath);
			continue;
		}
		FName LayerName = LayerObject->LayerName;
		if (LayerName.IsNone())
		{
			LayerName = LayerObject->GetFName();
		}
		FLandscapeImportLayerInfo LayerInfo(LayerName);
		LayerInfo.LayerInfo = LayerObject;
		const bool bBasePaint = ImportLayers.Num() == 0;
		LayerInfo.LayerData.Init(bBasePaint ? 255 : 0, Verts * Verts);
		ImportLayers.Add(MoveTemp(LayerInfo));
	}

	const int32 AssignedLayers = ImportLayers.Num();
	TMap<FGuid, TArray<FLandscapeImportLayerInfo>> MaterialPerLayer;
	MaterialPerLayer.Add(BaseLayerGuid, MoveTemp(ImportLayers));

	// UE 5.8 ALandscapeProxy::Import. The edit-layer view has no default argument.
	Landscape->Import(
		Landscape->GetLandscapeGuid(),
		0,
		0,
		Verts - 1,
		Verts - 1,
		SectionsPerComponent,
		SectionQuads,
		HeightPerLayer,
		TEXT("NONE"),
		MaterialPerLayer,
		ELandscapeImportAlphamapType::Additive,
		TArrayView<const FLandscapeLayer>());

	Landscape->ReregisterAllComponents();
	Landscape->CreateLandscapeInfo();
	Landscape->RecreateCollisionComponents();
	Landscape->SetActorLabel(TEXT("KodFaultlineLandscape"));
	Landscape->MarkPackageDirty();

	UE_LOG(LogTemp, Log, TEXT("KodLandscape Actor=%s Loc=%s Scale=%s Verts=%dx%d Layers=%d BaseGuid=%s"),
		*Landscape->GetName(),
		*Landscape->GetActorLocation().ToString(),
		*Landscape->GetActorScale3D().ToString(),
		Verts,
		Verts,
		AssignedLayers,
		*BaseLayerGuid.ToString());
	return Landscape;
}
