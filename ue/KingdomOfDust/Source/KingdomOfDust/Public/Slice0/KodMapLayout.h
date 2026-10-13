#pragma once

#include "CoreMinimal.h"

class UWorld;

/** Loads Config/KodDress/map_features.json into the sim and tags the world. */
namespace KodMapLayout
{
	void Load(UWorld* World);

	/**
	 * Cliff cells with ramp and choke footprints removed.
	 * CellBias is added to every layout cell (Faultline uses -56).
	 */
	void BlockedCellsFromJson(const FString& JsonText, int32 CellBias, TArray<int64>& OutBlocked);
}
