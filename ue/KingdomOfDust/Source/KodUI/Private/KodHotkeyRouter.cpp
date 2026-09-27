#include "KodHotkeyRouter.h"
#include "Game/KodPlayerController.h"
#include "KodSelectionManager.h"
#include "KodControlGroupStore.h"
#include "Engine/LocalPlayer.h"

void UKodHotkeyRouter::BindToController(AKodPlayerController* PC)
{
	(void)PC;
	// Wire Enhanced Input callbacks → selection / command enqueue / control groups
}

void UKodHotkeyRouter::OnControlGroupAssign(int32 GroupIndex)
{
	// Fetch selection entity ids → ControlGroupStore::AssignGroup
	(void)GroupIndex;
}

void UKodHotkeyRouter::OnControlGroupRecall(int32 GroupIndex)
{
	(void)GroupIndex;
}
