#include "Game/KodUiArt.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Warden/KodWardenPaths.h"

namespace
{
	struct FKodUiArtState
	{
		bool bManifestLoaded = false;
		TMap<FName, FSoftObjectPath> RegisteredWire;
		TMap<FName, FSoftObjectPath> RegisteredIcon;
		TMap<FName, FSoftObjectPath> ManifestWire;
		TMap<FName, FSoftObjectPath> ManifestIcon;
	};

	FKodUiArtState& ArtState()
	{
		static FKodUiArtState State;
		return State;
	}

	void ReadMap(const TSharedPtr<FJsonObject>& Root, const TCHAR* Field, TMap<FName, FSoftObjectPath>& Out)
	{
		const TSharedPtr<FJsonObject>* Object = nullptr;
		if (!Root.IsValid() || !Root->TryGetObjectField(FString(Field), Object) || !Object || !Object->IsValid())
		{
			return;
		}
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Object)->Values)
		{
			FString Path;
			if (!Pair.Value.IsValid() || !Pair.Value->TryGetString(Path) || Path.IsEmpty())
			{
				continue;
			}
			FSoftObjectPath Soft(Path);
			if (Soft.IsValid())
			{
				Out.Add(FName(*Pair.Key), Soft);
			}
		}
	}

	FSoftObjectPath Convention(const TCHAR* Root, FName Id)
	{
		if (Id.IsNone())
		{
			return FSoftObjectPath();
		}
		const FString Name = Id.ToString();
		return FSoftObjectPath(FString::Printf(TEXT("%s/%s.%s"), Root, *Name, *Name));
	}

	FSoftObjectPath Pick(FName Id, const TMap<FName, FSoftObjectPath>& Registered, const TMap<FName, FSoftObjectPath>& Manifest, const TCHAR* Root)
	{
		FKodUiArt::LoadManifest();
		if (const FSoftObjectPath* Found = Registered.Find(Id))
		{
			if (Found->IsValid())
			{
				return *Found;
			}
		}
		if (const FSoftObjectPath* Found = Manifest.Find(Id))
		{
			if (Found->IsValid())
			{
				return *Found;
			}
		}
		return Convention(Root, Id);
	}
}

void FKodUiArt::LoadManifest()
{
	FKodUiArtState& State = ArtState();
	if (State.bManifestLoaded)
	{
		return;
	}
	State.bManifestLoaded = true;
	const FString Path = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("KodUI/ui_art_manifest.json"));
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		UE_LOG(LogTemp, Log, TEXT("KodUiArt Manifest miss %s"), *Path);
		return;
	}
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("KodUiArt Manifest parse %s"), *Path);
		return;
	}
	ReadMap(Root, TEXT("wireframes"), State.ManifestWire);
	ReadMap(Root, TEXT("icons"), State.ManifestIcon);
	UE_LOG(LogTemp, Log, TEXT("KodUiArt Manifest Wire=%d Icon=%d"), State.ManifestWire.Num(), State.ManifestIcon.Num());
}

void FKodUiArt::RegisterWireframe(FName Id, const FSoftObjectPath& Path)
{
	if (!Id.IsNone() && Path.IsValid())
	{
		ArtState().RegisteredWire.Add(Id, Path);
	}
}

void FKodUiArt::RegisterIcon(FName Id, const FSoftObjectPath& Path)
{
	if (!Id.IsNone() && Path.IsValid())
	{
		ArtState().RegisteredIcon.Add(Id, Path);
	}
}

FSoftObjectPath FKodUiArt::ResolveWireframe(FName Id)
{
	const FKodUiArtState& State = ArtState();
	return Pick(Id, State.RegisteredWire, State.ManifestWire, KodWardenPaths::UiWireframeRoot);
}

FSoftObjectPath FKodUiArt::ResolveIcon(FName Id)
{
	const FKodUiArtState& State = ArtState();
	return Pick(Id, State.RegisteredIcon, State.ManifestIcon, KodWardenPaths::UiIconRoot);
}
