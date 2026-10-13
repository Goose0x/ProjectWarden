#include "Game/KodUiArt.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Warden/KodWardenPaths.h"

namespace
{
	struct FArtEntry
	{
		FSoftObjectPath Wire160;
		FSoftObjectPath Wire48;
		FSoftObjectPath Mask;
		FSoftObjectPath Icon;
	};

	struct FKodUiArtState
	{
		bool bManifestLoaded = false;
		TMap<FName, FSoftObjectPath> RegisteredWire;
		TMap<FName, FSoftObjectPath> RegisteredIcon;
		TMap<FName, FArtEntry> Entries;
	};

	FKodUiArtState& ArtState()
	{
		static FKodUiArtState State;
		return State;
	}

	FSoftObjectPath FromStem(const FString& Stem, const TCHAR* Root)
	{
		if (Stem.IsEmpty())
		{
			return FSoftObjectPath();
		}
		if (Stem.Contains(TEXT("/")))
		{
			return FSoftObjectPath(Stem);
		}
		return FSoftObjectPath(FString::Printf(TEXT("%s/%s.%s"), Root, *Stem, *Stem));
	}

	FSoftObjectPath FieldStem(const TSharedPtr<FJsonObject>& Row, const TCHAR* Field, const TCHAR* Root)
	{
		FString Stem;
		if (!Row.IsValid() || !Row->TryGetStringField(Field, Stem))
		{
			return FSoftObjectPath();
		}
		return FromStem(Stem, Root);
	}

	void ReadEntries(const TSharedPtr<FJsonObject>& Root, TMap<FName, FArtEntry>& Out)
	{
		const TSharedPtr<FJsonObject>* Object = nullptr;
		if (!Root.IsValid() || !Root->TryGetObjectField(TEXT("entries"), Object) || !Object || !Object->IsValid())
		{
			return;
		}
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Object)->Values)
		{
			const TSharedPtr<FJsonObject> Row = Pair.Value.IsValid() ? Pair.Value->AsObject() : nullptr;
			if (!Row.IsValid())
			{
				continue;
			}
			FArtEntry Entry;
			Entry.Wire160 = FieldStem(Row, TEXT("wire160"), KodWardenPaths::UiWireframeRoot);
			Entry.Wire48 = FieldStem(Row, TEXT("wire48"), KodWardenPaths::UiWireframeRoot);
			Entry.Mask = FieldStem(Row, TEXT("mask"), KodWardenPaths::UiWireframeRoot);
			Entry.Icon = FieldStem(Row, TEXT("icon"), KodWardenPaths::UiIconRoot);
			if (Entry.Wire160.IsValid() || Entry.Wire48.IsValid() || Entry.Mask.IsValid() || Entry.Icon.IsValid())
			{
				Out.Add(FName(*Pair.Key), Entry);
			}
		}
	}

	FSoftObjectPath Convention(FName Id, FKodUiArt::EKind Kind)
	{
		if (Id.IsNone())
		{
			return FSoftObjectPath();
		}
		const FString Name = Id.ToString();
		const TCHAR* Root = Kind == FKodUiArt::EKind::Icon ? KodWardenPaths::UiIconRoot : KodWardenPaths::UiWireframeRoot;
		const TCHAR* Suffix = TEXT("");
		switch (Kind)
		{
		case FKodUiArt::EKind::Wire160:
			Suffix = TEXT("_160");
			break;
		case FKodUiArt::EKind::Wire48:
			Suffix = TEXT("_48");
			break;
		case FKodUiArt::EKind::ZoneMask:
			Suffix = TEXT("_Mask");
			break;
		case FKodUiArt::EKind::Icon:
			Suffix = TEXT("");
			break;
		default:
			break;
		}
		return FSoftObjectPath(FString::Printf(TEXT("%s/%s%s.%s%s"), Root, *Name, Suffix, *Name, Suffix));
	}

	const FSoftObjectPath* Mapped(const FArtEntry* Entry, FKodUiArt::EKind Kind)
	{
		if (!Entry)
		{
			return nullptr;
		}
		switch (Kind)
		{
		case FKodUiArt::EKind::Wire160:
			return Entry->Wire160.IsValid() ? &Entry->Wire160 : nullptr;
		case FKodUiArt::EKind::Wire48:
			return Entry->Wire48.IsValid() ? &Entry->Wire48 : nullptr;
		case FKodUiArt::EKind::ZoneMask:
			return Entry->Mask.IsValid() ? &Entry->Mask : nullptr;
		case FKodUiArt::EKind::Icon:
			return Entry->Icon.IsValid() ? &Entry->Icon : nullptr;
		default:
			return nullptr;
		}
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
	ReadEntries(Root, State.Entries);
	UE_LOG(LogTemp, Log, TEXT("KodUiArt Manifest Entries=%d"), State.Entries.Num());
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

FSoftObjectPath FKodUiArt::Resolve(FName Id, EKind Kind)
{
	LoadManifest();
	const FKodUiArtState& State = ArtState();
	if (const FArtEntry* Entry = State.Entries.Find(Id))
	{
		if (const FSoftObjectPath* MappedPath = Mapped(Entry, Kind))
		{
			return *MappedPath;
		}
	}
	if (Kind == EKind::Icon)
	{
		if (const FSoftObjectPath* Registered = State.RegisteredIcon.Find(Id))
		{
			if (Registered->IsValid())
			{
				return *Registered;
			}
		}
	}
	else if (Kind == EKind::Wire160 || Kind == EKind::Wire48)
	{
		if (const FSoftObjectPath* Registered = State.RegisteredWire.Find(Id))
		{
			if (Registered->IsValid())
			{
				return *Registered;
			}
		}
	}
	return Convention(Id, Kind);
}

FSoftObjectPath FKodUiArt::Resolve(FName Id, KodWireTint::ESlot Slot)
{
	return Resolve(Id, Slot == KodWireTint::ESlot::Group48 ? EKind::Wire48 : EKind::Wire160);
}
