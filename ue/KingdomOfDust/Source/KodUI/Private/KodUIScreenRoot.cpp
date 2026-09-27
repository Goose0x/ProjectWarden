#include "KodUIScreenRoot.h"
#include "KodUI.h"
#include "Style/KodUIStyle.h"

void UKodUIScreenRoot::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (!UKodUIStyleLibrary::LanguageMatchesScreen(MaterialLanguage, Screen))
	{
		UE_LOG(LogKodUI, Error, TEXT("Screen material language does not match its stamp. HUD is Ironstock. Menus are cyan glass."));
	}
}
