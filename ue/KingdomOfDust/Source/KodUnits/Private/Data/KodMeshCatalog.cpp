#include "Data/KodMeshCatalog.h"

#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Game/KodCameraSettings.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Warden/KodWardenPaths.h"

namespace
{
	TAutoConsoleVariable<int32> CVarKodNewMeshes(
		TEXT("kod.NewMeshes"),
		1,
		TEXT("1 loads [KodMeshes] primary paths. 0 uses each row's fallback mesh."),
		ECVF_Default);

	TArray<FKodMeshRow> GRows;
	bool bRowsLoaded = false;

	FString ObjectPath(const FString& Path)
	{
		FString Trimmed = Path.TrimStartAndEnd();
		if (Trimmed.IsEmpty() || Trimmed.Contains(TEXT(".")))
		{
			return Trimmed;
		}
		const FString Name = FPaths::GetBaseFilename(Trimmed);
		return Trimmed + TEXT(".") + Name;
	}

	float HorizontalSpanCm(const FBoxSphereBounds& Bounds)
	{
		return FMath::Max(Bounds.BoxExtent.X, Bounds.BoxExtent.Y) * 2.f;
	}

	float VerticalSpanCm(const FBoxSphereBounds& Bounds)
	{
		return Bounds.BoxExtent.Z * 2.f;
	}

	bool IsScinderId(FName Id)
	{
		const FString Text = Id.ToString();
		return Text.Contains(TEXT("Scinder"))
			|| Text.StartsWith(TEXT("SCN_"))
			|| Id == FName(TEXT("Faultburst"))
			|| Id == FName(TEXT("Shardbitten"));
	}

	void ParseRow(const FString& Text, FKodMeshRow& Row)
	{
		TArray<FString> Parts;
		Text.ParseIntoArray(Parts, TEXT(","), false);
		for (const FString& Part : Parts)
		{
			FString Key;
			FString Value;
			if (!Part.Split(TEXT("="), &Key, &Value))
			{
				continue;
			}
			Key = Key.TrimStartAndEnd();
			Value = Value.TrimStartAndEnd();
			if (Key.Equals(TEXT("Id"), ESearchCase::IgnoreCase))
			{
				Row.Id = FName(*Value);
			}
			else if (Key.Equals(TEXT("Primary"), ESearchCase::IgnoreCase))
			{
				Row.Primary = Value;
			}
			else if (Key.Equals(TEXT("Fallback"), ESearchCase::IgnoreCase))
			{
				Row.Fallback = Value;
			}
			else if (Key.Equals(TEXT("Fallback2"), ESearchCase::IgnoreCase))
			{
				Row.Fallback2 = Value;
			}
			else if (Key.Equals(TEXT("Scale"), ESearchCase::IgnoreCase))
			{
				Row.Scale = FCString::Atof(*Value);
			}
			else if (Key.Equals(TEXT("Yaw"), ESearchCase::IgnoreCase))
			{
				Row.Yaw = FCString::Atof(*Value);
			}
			else if (Key.Equals(TEXT("Z"), ESearchCase::IgnoreCase))
			{
				Row.Z = FCString::Atof(*Value);
			}
			else if (Key.Equals(TEXT("Anim"), ESearchCase::IgnoreCase))
			{
				Row.Anim = Value;
			}
		}
		if (Row.Scale <= 0.f)
		{
			Row.Scale = 1.f;
		}
	}

	void LoadRows()
	{
		if (bRowsLoaded)
		{
			return;
		}
		bRowsLoaded = true;
		TArray<FString> Lines;
		GConfig->GetArray(TEXT("KodMeshes"), TEXT("Mesh"), Lines, GGameIni);
		for (const FString& Line : Lines)
		{
			FKodMeshRow Row;
			ParseRow(Line, Row);
			if (!Row.Id.IsNone())
			{
				GRows.Add(Row);
			}
		}
	}

	const FKodMeshRow* FindRow(FName Id)
	{
		LoadRows();
		for (const FKodMeshRow& Row : GRows)
		{
			if (Row.Id == Id)
			{
				return &Row;
			}
		}
		return nullptr;
	}

	bool TryLoadMesh(const FString& Path, UStaticMesh*& OutStatic, USkeletalMesh*& OutSkeletal)
	{
		OutStatic = nullptr;
		OutSkeletal = nullptr;
		if (Path.IsEmpty())
		{
			return false;
		}
		OutStatic = LoadObject<UStaticMesh>(nullptr, *Path);
		if (OutStatic)
		{
			return true;
		}
		OutSkeletal = LoadObject<USkeletalMesh>(nullptr, *Path);
		return OutSkeletal != nullptr;
	}

	float MeshHeightCm(UStaticMesh* StaticMesh, USkeletalMesh* SkeletalMesh)
	{
		if (StaticMesh)
		{
			return VerticalSpanCm(StaticMesh->GetBounds());
		}
		if (SkeletalMesh)
		{
			return VerticalSpanCm(SkeletalMesh->GetBounds());
		}
		return 0.f;
	}

	float UnitTargetHeightCm(FName Id)
	{
		if (Id == FName(KodWardenPaths::Id_Dozer) || Id == FName(KodWardenPaths::Id_ScnWorker))
		{
			return KodCameraSettings::WorkerBodyUU;
		}
		if (Id == FName(KodWardenPaths::Id_Marine))
		{
			return KodCameraSettings::MarineBodyUU;
		}
		return 0.f;
	}

	float FactionWorkerScale(bool bScinder)
	{
		static float VanguardScale = -1.f;
		static float ScinderScale = -1.f;
		float& Slot = bScinder ? ScinderScale : VanguardScale;
		if (Slot > 0.f)
		{
			return Slot;
		}
		const FName WorkerId = bScinder ? FName(KodWardenPaths::Id_ScnWorker) : FName(KodWardenPaths::Id_Dozer);
		const FKodMeshRow* WorkerRow = FindRow(WorkerId);
		float Height = 0.f;
		if (WorkerRow)
		{
			UStaticMesh* StaticMesh = nullptr;
			USkeletalMesh* SkeletalMesh = nullptr;
			const bool bNewMeshes = CVarKodNewMeshes.GetValueOnGameThread() != 0;
			bool bLoaded = bNewMeshes && TryLoadMesh(ObjectPath(WorkerRow->Primary), StaticMesh, SkeletalMesh);
			if (!bLoaded)
			{
				bLoaded = TryLoadMesh(ObjectPath(WorkerRow->Fallback), StaticMesh, SkeletalMesh);
			}
			if (!bLoaded)
			{
				bLoaded = TryLoadMesh(ObjectPath(WorkerRow->Fallback2), StaticMesh, SkeletalMesh);
			}
			if (bLoaded)
			{
				Height = MeshHeightCm(StaticMesh, SkeletalMesh);
			}
		}
		Slot = Height > KINDA_SMALL_NUMBER ? (KodCameraSettings::WorkerBodyUU / Height) : 1.f;
		return Slot;
	}
}

const TArray<FKodMeshRow>& FKodMeshCatalog::Rows()
{
	LoadRows();
	return GRows;
}

float FKodMeshCatalog::FootprintCm(FName Id)
{
	if (Id == FName(KodWardenPaths::Id_CommandCenter) || Id == FName(TEXT("ScinderHQ")))
	{
		return 1000.f;
	}
	if (Id == FName(KodWardenPaths::Id_Barracks))
	{
		return 600.f;
	}
	if (Id == FName(KodWardenPaths::Id_SupplyDepot))
	{
		return 400.f;
	}
	return 0.f;
}

bool FKodMeshCatalog::SpanWithin(float BoundsCm, float ExpectedCm, float Fraction)
{
	if (ExpectedCm <= 0.f)
	{
		return false;
	}
	return FMath::Abs(BoundsCm - ExpectedCm) <= ExpectedCm * Fraction;
}

float FKodMeshCatalog::FittedSpanCm(float RawSpanCm, float RowScale, float FootprintCm)
{
	const float Scale = RowScale > 0.f ? RowScale : 1.f;
	if (FootprintCm <= 0.f || RawSpanCm <= KINDA_SMALL_NUMBER)
	{
		return RawSpanCm * Scale;
	}
	return FootprintCm * Scale;
}

FKodMeshChoice FKodMeshCatalog::Resolve(FName Id)
{
	FKodMeshChoice Choice;
	Choice.Id = Id;
	const FKodMeshRow* Row = FindRow(Id);
	if (!Row)
	{
		return Choice;
	}
	Choice.bHasRow = true;
	Choice.Yaw = Row->Yaw;
	Choice.Z = Row->Z;
	Choice.Anim = Row->Anim;
	const bool bNewMeshes = CVarKodNewMeshes.GetValueOnGameThread() != 0;
	const FString Primary = ObjectPath(Row->Primary);
	const FString Fallback = ObjectPath(Row->Fallback);
	auto TryLoad = [](const FString& Path, UStaticMesh*& OutStatic, USkeletalMesh*& OutSkeletal) -> bool
	{
		OutStatic = nullptr;
		OutSkeletal = nullptr;
		if (Path.IsEmpty())
		{
			return false;
		}
		OutStatic = LoadObject<UStaticMesh>(nullptr, *Path);
		if (OutStatic)
		{
			return true;
		}
		OutSkeletal = LoadObject<USkeletalMesh>(nullptr, *Path);
		return OutSkeletal != nullptr;
	};

	UStaticMesh* StaticMesh = nullptr;
	USkeletalMesh* SkeletalMesh = nullptr;
	FString Chosen = bNewMeshes ? Primary : FString();
	bool bLoaded = bNewMeshes && TryLoad(Primary, StaticMesh, SkeletalMesh);
	bool bFallback = false;
	if (!bLoaded)
	{
		bFallback = true;
		Chosen = Fallback;
		bLoaded = TryLoad(Fallback, StaticMesh, SkeletalMesh);
	}
	if (!bLoaded && !Row->Fallback2.IsEmpty())
	{
		bFallback = true;
		Chosen = ObjectPath(Row->Fallback2);
		bLoaded = TryLoad(Chosen, StaticMesh, SkeletalMesh);
	}
	Choice.Path = Chosen;
	Choice.bLoaded = bLoaded;
	Choice.bFallback = bFallback;
	Choice.StaticMesh = StaticMesh;
	Choice.SkeletalMesh = SkeletalMesh;
	Choice.bSkeletal = SkeletalMesh != nullptr;
	const FBoxSphereBounds RawBounds = StaticMesh
		? StaticMesh->GetBounds()
		: (SkeletalMesh ? SkeletalMesh->GetBounds() : FBoxSphereBounds(FVector::ZeroVector, FVector::ZeroVector, 0.f));
	const float RawSpan = (StaticMesh || SkeletalMesh) ? HorizontalSpanCm(RawBounds) : 0.f;
	const float RawHeight = (StaticMesh || SkeletalMesh) ? VerticalSpanCm(RawBounds) : 0.f;
	const float Footprint = FootprintCm(Id);
	const float TargetHeight = UnitTargetHeightCm(Id);
	const float RowScale = Row->Scale > 0.f ? Row->Scale : 1.f;
	if (Footprint > 0.f && RawSpan > KINDA_SMALL_NUMBER)
	{
		Choice.Scale = (Footprint / RawSpan) * RowScale;
		Choice.BoundsCm = RawSpan * Choice.Scale;
	}
	else if (TargetHeight > 0.f && RawHeight > KINDA_SMALL_NUMBER)
	{
		Choice.Scale = (TargetHeight / RawHeight) * RowScale;
		Choice.BoundsCm = RawHeight * Choice.Scale;
	}
	else if (bLoaded && RawHeight > KINDA_SMALL_NUMBER && TargetHeight <= 0.f && Footprint <= 0.f)
	{
		Choice.Scale = FactionWorkerScale(IsScinderId(Id)) * RowScale;
		Choice.BoundsCm = RawHeight * Choice.Scale;
	}
	else
	{
		Choice.Scale = RowScale;
		Choice.BoundsCm = RawSpan * Choice.Scale;
	}
	UE_LOG(LogTemp, Log, TEXT("KodMesh Id=%s Path=%s Loaded=%d Fallback=%d Bounds=%.0f Scale=%.3f"),
		*Id.ToString(),
		*Choice.Path,
		Choice.bLoaded ? 1 : 0,
		Choice.bFallback ? 1 : 0,
		Choice.BoundsCm,
		Choice.Scale);
	return Choice;
}
