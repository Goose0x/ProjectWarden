#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "KodLandscapeBuilder.generated.h"

class ALandscape;

/**
 * Editor-only Faultline landscape import. Python:
 * unreal.KodLandscapeBuilder.create_faultline_landscape(material_path, layer_info_paths)
 */
UCLASS()
class KODEDITOR_API UKodLandscapeBuilder : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Spawns a flat ALandscape in the open editor world.
	 * 505x505 heightmap at 32768, 63-quad sections, 2x2 sections per component,
	 * 4x4 components, scale (44.444, 44.444, 100), origin (-11200, -11200, 0).
	 * MaterialInstancePath and LayerInfoPaths are soft asset paths. A missing dot
	 * is completed to ObjectName.ObjectName. Layer weights are additive: the first
	 * loaded layer is 255 and the rest are 0. The height and weight maps use the
	 * zero edit-layer GUID.
	 */
	UFUNCTION(BlueprintCallable, Category = "Kod|Landscape")
	static ALandscape* CreateFaultlineLandscape(const FString& MaterialInstancePath, const TArray<FString>& LayerInfoPaths);
};
