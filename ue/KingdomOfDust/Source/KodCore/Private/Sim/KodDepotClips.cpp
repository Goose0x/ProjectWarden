#include "Sim/KodDepotClips.h"

#include "Dom/JsonObject.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	FKodDepotNotify MakeNotify(const TCHAR* Name, int32 Frame, float TimeSeconds, float Chance)
	{
		FKodDepotNotify Notify;
		Notify.Name = FName(Name);
		Notify.Frame = Frame;
		Notify.TimeSeconds = TimeSeconds;
		Notify.Chance = Chance;
		return Notify;
	}

	void AddSocket(FKodDepotNotify& Notify, const TCHAR* Socket)
	{
		Notify.Sockets.Add(FName(Socket));
	}

	void ReadSockets(const TSharedPtr<FJsonObject>& NotifyObj, TArray<FName>& OutSockets)
	{
		const TArray<TSharedPtr<FJsonValue>>* SocketValues = nullptr;
		if (!NotifyObj->TryGetArrayField(TEXT("sockets"), SocketValues) || !SocketValues)
		{
			return;
		}
		for (const TSharedPtr<FJsonValue>& SocketValue : *SocketValues)
		{
			FString SocketName;
			if (SocketValue.IsValid() && SocketValue->TryGetString(SocketName) && !SocketName.IsEmpty())
			{
				OutSockets.Add(FName(*SocketName));
			}
		}
	}

	bool ReadNotify(const TSharedPtr<FJsonObject>& NotifyObj, FKodDepotNotify& OutNotify, int32 Fps)
	{
		if (!NotifyObj.IsValid())
		{
			return false;
		}
		FString Name;
		if (!NotifyObj->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty())
		{
			return false;
		}
		OutNotify.Name = FName(*Name);
		double Frame = 0.0;
		double TimeSeconds = 0.0;
		double Chance = 1.0;
		const float Rate = static_cast<float>(FMath::Max(1, Fps));
		if (NotifyObj->TryGetNumberField(TEXT("frame"), Frame))
		{
			OutNotify.Frame = FMath::RoundToInt(Frame);
		}
		if (NotifyObj->TryGetNumberField(TEXT("time_s"), TimeSeconds))
		{
			OutNotify.TimeSeconds = static_cast<float>(TimeSeconds);
		}
		else
		{
			OutNotify.TimeSeconds = static_cast<float>(OutNotify.Frame) / Rate;
		}
		if (NotifyObj->TryGetNumberField(TEXT("chance"), Chance))
		{
			OutNotify.Chance = FMath::Clamp(static_cast<float>(Chance), 0.f, 1.f);
		}
		ReadSockets(NotifyObj, OutNotify.Sockets);
		return true;
	}
}

void FKodDepotClipTable::AddDefaults()
{
	FKodDepotClip Idle;
	Idle.Id = FName(KodDepotClipId::Idle);
	Idle.Frames = 60;
	Idle.bLoop = true;
	FKodDepotNotify VentTick = MakeNotify(KodDepotClipId::VentTick, 44, 44.f / 30.f, 0.35f);
	AddSocket(VentTick, TEXT("SOCKET_Vent"));
	Idle.Notifies.Add(VentTick);
	UpsertClip(MoveTemp(Idle));

	FKodDepotClip Lower;
	Lower.Id = FName(KodDepotClipId::Lower);
	Lower.Frames = 30;
	Lower.bLoop = false;
	Lower.Notifies.Add(MakeNotify(KodDepotClipId::LowerStart, 0, 0.f, 1.f));
	FKodDepotNotify LowerHiss = MakeNotify(KodDepotClipId::VentHiss, 0, 0.f, 1.f);
	AddSocket(LowerHiss, TEXT("SOCKET_Vent_FL"));
	AddSocket(LowerHiss, TEXT("SOCKET_Vent_FR"));
	AddSocket(LowerHiss, TEXT("SOCKET_Vent_BL"));
	AddSocket(LowerHiss, TEXT("SOCKET_Vent_BR"));
	Lower.Notifies.Add(LowerHiss);
	FKodDepotNotify LowerDust = MakeNotify(KodDepotClipId::Dust, 8, 8.f / 30.f, 1.f);
	AddSocket(LowerDust, TEXT("FX_Dust_Base_0"));
	AddSocket(LowerDust, TEXT("FX_Dust_Base_1"));
	AddSocket(LowerDust, TEXT("FX_Dust_Base_2"));
	AddSocket(LowerDust, TEXT("FX_Dust_Base_3"));
	Lower.Notifies.Add(LowerDust);
	FKodDepotNotify Land = MakeNotify(KodDepotClipId::Land, 22, 22.f / 30.f, 1.f);
	AddSocket(Land, TEXT("SOCKET_Dust_Ground"));
	Lower.Notifies.Add(Land);
	Lower.Notifies.Add(MakeNotify(KodDepotClipId::NavWalkable, 30, 1.f, 1.f));
	UpsertClip(MoveTemp(Lower));

	FKodDepotClip LoweredIdle;
	LoweredIdle.Id = FName(KodDepotClipId::LoweredIdle);
	LoweredIdle.Frames = 60;
	LoweredIdle.bLoop = true;
	UpsertClip(MoveTemp(LoweredIdle));

	FKodDepotClip Raise;
	Raise.Id = FName(KodDepotClipId::Raise);
	Raise.Frames = 30;
	Raise.bLoop = false;
	Raise.Notifies.Add(MakeNotify(KodDepotClipId::RaiseStart, 0, 0.f, 1.f));
	FKodDepotNotify RaiseHiss = MakeNotify(KodDepotClipId::VentHiss, 0, 0.f, 1.f);
	AddSocket(RaiseHiss, TEXT("SOCKET_Vent_FL"));
	AddSocket(RaiseHiss, TEXT("SOCKET_Vent_FR"));
	AddSocket(RaiseHiss, TEXT("SOCKET_Vent_BL"));
	AddSocket(RaiseHiss, TEXT("SOCKET_Vent_BR"));
	Raise.Notifies.Add(RaiseHiss);
	FKodDepotNotify RaiseDust = MakeNotify(KodDepotClipId::Dust, 3, 3.f / 30.f, 1.f);
	AddSocket(RaiseDust, TEXT("FX_Dust_Base_0"));
	AddSocket(RaiseDust, TEXT("FX_Dust_Base_1"));
	AddSocket(RaiseDust, TEXT("FX_Dust_Base_2"));
	AddSocket(RaiseDust, TEXT("FX_Dust_Base_3"));
	Raise.Notifies.Add(RaiseDust);
	FKodDepotNotify Clunk = MakeNotify(KodDepotClipId::Clunk, 25, 25.f / 30.f, 1.f);
	AddSocket(Clunk, TEXT("SOCKET_Dust_Ground"));
	Raise.Notifies.Add(Clunk);
	UpsertClip(MoveTemp(Raise));

	auto AddDeath = [this](const TCHAR* ClipName)
	{
		FKodDepotClip Death;
		Death.Id = FName(ClipName);
		Death.Frames = 45;
		Death.bLoop = false;
		FKodDepotNotify Explosion = MakeNotify(KodDepotClipId::DeathExplosion, 0, 0.f, 1.f);
		AddSocket(Explosion, TEXT("SOCKET_Vent"));
		AddSocket(Explosion, TEXT("SOCKET_Door"));
		Death.Notifies.Add(Explosion);
		FKodDepotNotify Secondary = MakeNotify(KodDepotClipId::DeathSecondary, 13, 13.f / 30.f, 1.f);
		AddSocket(Secondary, TEXT("SOCKET_DecalFront"));
		Death.Notifies.Add(Secondary);
		UpsertClip(MoveTemp(Death));
	};
	AddDeath(KodDepotClipId::Death);
	AddDeath(KodDepotClipId::DeathLowered);
}

void FKodDepotClipTable::UpsertClip(FKodDepotClip&& Clip)
{
	for (FKodDepotClip& Existing : ClipList)
	{
		if (Existing.Id == Clip.Id)
		{
			Existing = MoveTemp(Clip);
			return;
		}
	}
	ClipList.Add(MoveTemp(Clip));
}

FKodDepotFxSlot& FKodDepotClipTable::FxSlot(FName NotifyName)
{
	for (FKodDepotFxSlot& Slot : FxSlots)
	{
		if (Slot.Notify == NotifyName)
		{
			return Slot;
		}
	}
	FKodDepotFxSlot& Added = FxSlots.AddDefaulted_GetRef();
	Added.Notify = NotifyName;
	return Added;
}

void FKodDepotClipTable::EnsureNotifySlots()
{
	for (const FKodDepotClip& Clip : ClipList)
	{
		for (const FKodDepotNotify& Notify : Clip.Notifies)
		{
			FxSlot(Notify.Name);
		}
	}
}

bool FKodDepotClipTable::ApplyJson(const FString& Text)
{
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return false;
	}
	double RootFps = 30.0;
	const bool bRootFps = Root->TryGetNumberField(TEXT("fps"), RootFps);
	const TSharedPtr<FJsonObject>* Clips = nullptr;
	if (Root->TryGetObjectField(TEXT("clips"), Clips) && Clips && Clips->IsValid())
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Clips)->Values)
		{
			const TSharedPtr<FJsonObject> ClipObj = Pair.Value.IsValid() ? Pair.Value->AsObject() : nullptr;
			if (!ClipObj.IsValid() || Pair.Key.IsEmpty())
			{
				continue;
			}
			FKodDepotClip Clip;
			Clip.Id = FName(*Pair.Key);
			Clip.Fps = bRootFps ? FMath::Max(1, FMath::RoundToInt(RootFps)) : 30;
			double Frames = static_cast<double>(Clip.Frames);
			double ClipFps = static_cast<double>(Clip.Fps);
			if (ClipObj->TryGetNumberField(TEXT("frames"), Frames))
			{
				Clip.Frames = FMath::Max(1, FMath::RoundToInt(Frames));
			}
			if (ClipObj->TryGetNumberField(TEXT("fps"), ClipFps))
			{
				Clip.Fps = FMath::Max(1, FMath::RoundToInt(ClipFps));
			}
			ClipObj->TryGetBoolField(TEXT("loop"), Clip.bLoop);
			const TArray<TSharedPtr<FJsonValue>>* NotifyValues = nullptr;
			if (ClipObj->TryGetArrayField(TEXT("notifies"), NotifyValues) && NotifyValues)
			{
				for (const TSharedPtr<FJsonValue>& NotifyValue : *NotifyValues)
				{
					FKodDepotNotify Notify;
					if (ReadNotify(NotifyValue.IsValid() ? NotifyValue->AsObject() : nullptr, Notify, Clip.Fps))
					{
						Clip.Notifies.Add(Notify);
					}
				}
			}
			UpsertClip(MoveTemp(Clip));
		}
	}
	const TSharedPtr<FJsonObject>* Fx = nullptr;
	if (Root->TryGetObjectField(TEXT("fx"), Fx) && Fx && Fx->IsValid())
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Fx)->Values)
		{
			const TSharedPtr<FJsonObject> SlotObj = Pair.Value.IsValid() ? Pair.Value->AsObject() : nullptr;
			if (!SlotObj.IsValid() || Pair.Key.IsEmpty())
			{
				continue;
			}
			FKodDepotFxSlot& Slot = FxSlot(FName(*Pair.Key));
			FString Niagara;
			FString Sound;
			if (SlotObj->TryGetStringField(TEXT("niagara"), Niagara))
			{
				Slot.Niagara = Niagara;
			}
			if (SlotObj->TryGetStringField(TEXT("sound"), Sound))
			{
				Slot.Sound = Sound;
			}
		}
	}
	return true;
}

void FKodDepotClipTable::LoadJson()
{
	const FString Path = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("SupplyDepotClips.json"));
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		UE_LOG(LogTemp, Log, TEXT("KodDepot Clips Source=builtin Path=%s"), *Path);
		return;
	}
	if (!ApplyJson(Text))
	{
		UE_LOG(LogTemp, Warning, TEXT("KodDepot Clips parse failed %s"), *Path);
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("KodDepot Clips Source=json Path=%s Count=%d"), *Path, ClipList.Num());
}

void FKodDepotClipTable::LoadFxIni()
{
	TArray<FString> Lines;
	if (!GConfig || !GConfig->GetArray(TEXT("KodDepotFx"), TEXT("Slot"), Lines, GGameIni))
	{
		return;
	}
	for (const FString& Line : Lines)
	{
		FString NotifyName;
		FString Niagara;
		FString Sound;
		bool bNiagara = false;
		bool bSound = false;
		TArray<FString> Parts;
		Line.ParseIntoArray(Parts, TEXT(","), false);
		for (const FString& Part : Parts)
		{
			FString Key;
			FString Value;
			if (!Part.Split(TEXT("="), &Key, &Value))
			{
				continue;
			}
			Key.TrimStartAndEndInline();
			Value.TrimStartAndEndInline();
			if (Key.Equals(TEXT("Notify"), ESearchCase::IgnoreCase))
			{
				NotifyName = Value;
			}
			else if (Key.Equals(TEXT("Niagara"), ESearchCase::IgnoreCase))
			{
				Niagara = Value;
				bNiagara = true;
			}
			else if (Key.Equals(TEXT("Sound"), ESearchCase::IgnoreCase))
			{
				Sound = Value;
				bSound = true;
			}
		}
		if (NotifyName.IsEmpty())
		{
			continue;
		}
		FKodDepotFxSlot& Slot = FxSlot(FName(*NotifyName));
		if (bNiagara && !Niagara.IsEmpty())
		{
			Slot.Niagara = Niagara;
		}
		if (bSound && !Sound.IsEmpty())
		{
			Slot.Sound = Sound;
		}
	}
}

FKodDepotClipTable::FKodDepotClipTable()
{
	AddDefaults();
	EnsureNotifySlots();
	LoadJson();
	EnsureNotifySlots();
	LoadFxIni();
}

FKodDepotClipTable FKodDepotClipTable::Parse(const FString& JsonText)
{
	FKodDepotClipTable Table;
	Table.AddDefaults();
	Table.ApplyJson(JsonText);
	Table.EnsureNotifySlots();
	return Table;
}

const FKodDepotClipTable& FKodDepotClipTable::Get()
{
	static const FKodDepotClipTable Table;
	return Table;
}

const FKodDepotClip* FKodDepotClipTable::FindClip(FName ClipId) const
{
	for (const FKodDepotClip& Clip : ClipList)
	{
		if (Clip.Id == ClipId)
		{
			return &Clip;
		}
	}
	return nullptr;
}

const FKodDepotNotify* FKodDepotClipTable::FindNotify(FName ClipId, FName NotifyName) const
{
	const FKodDepotClip* Clip = FindClip(ClipId);
	if (!Clip)
	{
		return nullptr;
	}
	for (const FKodDepotNotify& Notify : Clip->Notifies)
	{
		if (Notify.Name == NotifyName)
		{
			return &Notify;
		}
	}
	return nullptr;
}

const FKodDepotFxSlot* FKodDepotClipTable::FindFx(FName NotifyName) const
{
	for (const FKodDepotFxSlot& Slot : FxSlots)
	{
		if (Slot.Notify == NotifyName)
		{
			return &Slot;
		}
	}
	return nullptr;
}

int32 FKodDepotClipTable::ClipTicks(FName ClipId) const
{
	if (const FKodDepotClip* Clip = FindClip(ClipId))
	{
		return Clip->Ticks();
	}
	return KodBuildTicks::SecondsToBuildTicks(1.f);
}

int32 FKodDepotClipTable::WalkableTick() const
{
	if (const FKodDepotNotify* Notify = FindNotify(FName(KodDepotClipId::Lower), FName(KodDepotClipId::NavWalkable)))
	{
		return Notify->Tick();
	}
	if (const FKodDepotClip* LowerClip = FindClip(FName(KodDepotClipId::Lower)))
	{
		return LowerClip->Ticks();
	}
	return 0;
}

int32 FKodDepotClipTable::RaiseBlockTick() const
{
	if (const FKodDepotNotify* Notify = FindNotify(FName(KodDepotClipId::Raise), FName(KodDepotClipId::RaiseStart)))
	{
		return Notify->Tick();
	}
	return 0;
}
