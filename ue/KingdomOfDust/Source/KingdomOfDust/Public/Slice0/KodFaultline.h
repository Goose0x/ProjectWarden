#pragma once

#include "CoreMinimal.h"

class UWorld;

/** Faultline Training. JSON lives in Config/KodDress/faultline_training/. */
namespace KodFaultline
{
	bool IsTrainingMap(const UWorld* World);
	/** Seat the octagon CC, place the jade arc, and spawn Scinder proxies. */
	void Begin(UWorld* World);

	/**
	 * World XY of one feature in map_features.json text.
	 * centre_uu, then cell via the Faultline cell transform, then cells.rect_xywh.
	 */
	bool ReadMarkerPoint(const FString& JsonText, const FString& MarkerId, float& OutX, float& OutY);
}
