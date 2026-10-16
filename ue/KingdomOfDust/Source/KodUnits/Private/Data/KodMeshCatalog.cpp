#include "Data/KodMeshCatalog.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "RHIFeatureLevel.h"
#include "Game/KodCameraSettings.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Warden/KodWardenPaths.h"

namespace
{
	TAutoConsoleVariable<int32> CVarKodNewMeshes(
		TEXT("kod.NewMeshes"),
		1,
		TEXT("1 loads [KodMeshes] primary paths. 0 uses each row's fallback mesh."),
		ECVF_Default);

	TArray<FKodMeshRow> GRows;
	TArray<FKodMeshAttach> GAttaches;
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
			|| Id == FName(TEXT("Shardbitten"))
			|| Id == FName(TEXT("SkyReaver"))
			|| Id == FName(TEXT("SkySentinel"));
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
			else if (Key.Equals(TEXT("FitMode"), ESearchCase::IgnoreCase))
			{
				if (Value.Equals(TEXT("Length"), ESearchCase::IgnoreCase))
				{
					Row.Fit = EKodMeshFit::Length;
				}
				else if (Value.Equals(TEXT("Footprint"), ESearchCase::IgnoreCase))
				{
					Row.Fit = EKodMeshFit::Footprint;
				}
				else if (Value.Equals(TEXT("Height"), ESearchCase::IgnoreCase))
				{
					Row.Fit = EKodMeshFit::Height;
				}
			}
			else if (Key.Equals(TEXT("FitCm"), ESearchCase::IgnoreCase))
			{
				Row.FitCm = FCString::Atof(*Value);
			}
			else if (Key.Equals(TEXT("MinHeight"), ESearchCase::IgnoreCase))
			{
				Row.MinHeightCm = FCString::Atof(*Value);
			}
			else if (Key.Equals(TEXT("Anim"), ESearchCase::IgnoreCase))
			{
				Row.Anim = Value;
			}
			else if (Key.Equals(TEXT("AnimFolder"), ESearchCase::IgnoreCase))
			{
				Row.AnimFolder = Value;
			}
			else if (Key.Equals(TEXT("Raise"), ESearchCase::IgnoreCase))
			{
				Row.RaiseAnim = Value;
			}
			else if (Key.Equals(TEXT("Lower"), ESearchCase::IgnoreCase))
			{
				Row.LowerAnim = Value;
			}
			else if (Key.Equals(TEXT("DeckExtend"), ESearchCase::IgnoreCase))
			{
				Row.DeckExtendAnim = Value;
			}
			else if (Key.Equals(TEXT("DeckRetract"), ESearchCase::IgnoreCase))
			{
				Row.DeckRetractAnim = Value;
			}
		}
		if (Row.Scale <= 0.f)
		{
			Row.Scale = 1.f;
		}
	}

	FString SiblingClip(const FString& IdlePath, const TCHAR* Token)
	{
		if (IdlePath.IsEmpty() || !IdlePath.Contains(TEXT("IdleClosed")))
		{
			return FString();
		}
		FString Copy = IdlePath;
		Copy.ReplaceInline(TEXT("IdleClosed"), Token);
		return Copy;
	}

	void ParseAttach(const FString& Text, FKodMeshAttach& Attach)
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
			if (Key.Equals(TEXT("Owner"), ESearchCase::IgnoreCase))
			{
				Attach.OwnerId = FName(*Value);
			}
			else if (Key.Equals(TEXT("Mesh"), ESearchCase::IgnoreCase))
			{
				Attach.Mesh = Value;
			}
			else if (Key.Equals(TEXT("Socket"), ESearchCase::IgnoreCase))
			{
				Attach.SocketChain.Reset();
				TArray<FString> Tokens;
				Value.ParseIntoArray(Tokens, TEXT("|"), true);
				for (const FString& Token : Tokens)
				{
					const FString Trimmed = Token.TrimStartAndEnd();
					const FName Name = Trimmed.IsEmpty() || Trimmed.Equals(TEXT("None"), ESearchCase::IgnoreCase)
						? NAME_None
						: FName(*Trimmed);
					Attach.SocketChain.Add(Name);
				}
				Attach.Socket = Attach.SocketChain.Num() > 0 ? Attach.SocketChain[0] : NAME_None;
			}
			else if (Key.Equals(TEXT("Anim"), ESearchCase::IgnoreCase))
			{
				Attach.Anim = Value;
			}
			else if (Key.Equals(TEXT("Open"), ESearchCase::IgnoreCase))
			{
				Attach.OpenAnim = Value;
			}
			else if (Key.Equals(TEXT("Close"), ESearchCase::IgnoreCase))
			{
				Attach.CloseAnim = Value;
			}
			else if (Key.Equals(TEXT("IdleOpen"), ESearchCase::IgnoreCase))
			{
				Attach.IdleOpenAnim = Value;
			}
		}
		if (Attach.OpenAnim.IsEmpty())
		{
			Attach.OpenAnim = SiblingClip(Attach.Anim, TEXT("Open"));
		}
		if (Attach.CloseAnim.IsEmpty())
		{
			Attach.CloseAnim = SiblingClip(Attach.Anim, TEXT("Close"));
		}
		if (Attach.IdleOpenAnim.IsEmpty())
		{
			Attach.IdleOpenAnim = SiblingClip(Attach.Anim, TEXT("IdleOpen"));
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
		TArray<FString> AttachLines;
		GConfig->GetArray(TEXT("KodMeshes"), TEXT("Attach"), AttachLines, GGameIni);
		for (const FString& Line : AttachLines)
		{
			FKodMeshAttach Attach;
			ParseAttach(Line, Attach);
			if (!Attach.OwnerId.IsNone() && !Attach.Mesh.IsEmpty())
			{
				GAttaches.Add(Attach);
			}
		}
	}

	const FKodMeshRow* FindRow(FName Id)
	{
		LoadRows();
		if (Id == FName(TEXT("Bunker")))
		{
			Id = FName(KodWardenPaths::Id_Bunker);
		}
		for (const FKodMeshRow& Row : GRows)
		{
			if (Row.Id == Id)
			{
				return &Row;
			}
		}
		return nullptr;
	}

	bool TryLoadMesh(const FString& Path, UStaticMesh*& OutStatic, USkeletalMesh*& OutSkeletal);

	bool LoadFirstMeshInFolder(const FString& Folder, FName Id, UStaticMesh*& OutStatic, USkeletalMesh*& OutSkeletal, FString& OutPath)
	{
		OutStatic = nullptr;
		OutSkeletal = nullptr;
		OutPath.Reset();
		FString PackagePath = Folder;
		while (PackagePath.EndsWith(TEXT("/")))
		{
			PackagePath.LeftChopInline(1);
		}
		if (PackagePath.IsEmpty() || !FModuleManager::Get().ModuleExists(TEXT("AssetRegistry")))
		{
			return false;
		}
		FAssetRegistryModule& Assets = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		IAssetRegistry& Registry = Assets.Get();
		TArray<FString> Scan;
		Scan.Add(PackagePath);
		Registry.ScanPathsSynchronous(Scan, false);
		FARFilter Filter;
		Filter.PackagePaths.Add(*PackagePath);
		Filter.ClassPaths.Add(UStaticMesh::StaticClass()->GetClassPathName());
		Filter.ClassPaths.Add(USkeletalMesh::StaticClass()->GetClassPathName());
		Filter.bRecursivePaths = false;
		TArray<FAssetData> Found;
		Registry.GetAssets(Filter, Found);
		Found.Sort([](const FAssetData& A, const FAssetData& B)
		{
			return A.AssetName.LexicalLess(B.AssetName);
		});
		for (const FAssetData& Asset : Found)
		{
			const FString Candidate = Asset.GetSoftObjectPath().ToString();
			if (TryLoadMesh(Candidate, OutStatic, OutSkeletal))
			{
				OutPath = Candidate;
				UE_LOG(LogTemp, Log, TEXT("KodMesh FolderPick Id=%s Path=%s"), *Id.ToString(), *Candidate);
				return true;
			}
		}
		return false;
	}

	bool LoadMeshReference(const FString& Raw, FName Id, UStaticMesh*& OutStatic, USkeletalMesh*& OutSkeletal, FString& OutPath)
	{
		OutStatic = nullptr;
		OutSkeletal = nullptr;
		OutPath.Reset();
		const FString Trimmed = Raw.TrimStartAndEnd();
		if (Trimmed.IsEmpty())
		{
			return false;
		}
		if (Trimmed.EndsWith(TEXT("/")))
		{
			return LoadFirstMeshInFolder(Trimmed, Id, OutStatic, OutSkeletal, OutPath);
		}
		if (Trimmed.Contains(TEXT(".")))
		{
			if (TryLoadMesh(Trimmed, OutStatic, OutSkeletal))
			{
				OutPath = Trimmed;
				return true;
			}
			return LoadFirstMeshInFolder(FPaths::GetPath(Trimmed), Id, OutStatic, OutSkeletal, OutPath);
		}
		const FString Implied = ObjectPath(Trimmed);
		if (TryLoadMesh(Implied, OutStatic, OutSkeletal))
		{
			OutPath = Implied;
			return true;
		}
		if (LoadFirstMeshInFolder(Trimmed, Id, OutStatic, OutSkeletal, OutPath))
		{
			return true;
		}
		return LoadFirstMeshInFolder(FPaths::GetPath(Trimmed), Id, OutStatic, OutSkeletal, OutPath);
	}

	void CapImportedTextures(UStaticMesh* StaticMesh, USkeletalMesh* SkeletalMesh, int32 MaxSize)
	{
		TArray<UMaterialInterface*> Materials;
		if (StaticMesh)
		{
			for (const FStaticMaterial& Slot : StaticMesh->GetStaticMaterials())
			{
				Materials.Add(Slot.MaterialInterface);
			}
		}
		if (SkeletalMesh)
		{
			for (const FSkeletalMaterial& Slot : SkeletalMesh->GetMaterials())
			{
				Materials.Add(Slot.MaterialInterface);
			}
		}
		static TSet<FString> Capped;
		for (UMaterialInterface* Material : Materials)
		{
			if (!Material)
			{
				continue;
			}
			TArray<UTexture*> Used;
			Material->GetUsedTextures(Used, EMaterialQualityLevel::High, true, ERHIFeatureLevel::SM5, true);
			for (UTexture* Texture : Used)
			{
				UTexture2D* Image = Cast<UTexture2D>(Texture);
				if (!Image)
				{
					continue;
				}
				const FString Path = Image->GetPathName();
				if (!Path.Contains(TEXT("/Game/Warden/")))
				{
					continue;
				}
				if (Capped.Contains(Path))
				{
					continue;
				}
				Capped.Add(Path);
				const bool bBuilding = MaxSize >= 4096;
				Image->LODGroup = bBuilding ? TEXTUREGROUP_World : TEXTUREGROUP_Character;
				Image->MaxTextureSize = MaxSize;
				Image->UpdateResource();
				UE_LOG(LogTemp, Log, TEXT("KodMesh TexCap Path=%s Max=%d"), *Path, MaxSize);
			}
		}
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

	bool SkeletalVertexBounds(const USkeletalMesh* Mesh, FBoxSphereBounds& OutBounds)
	{
		OutBounds = FBoxSphereBounds(FVector::ZeroVector, FVector::ZeroVector, 0.f);
		if (!Mesh)
		{
			return false;
		}
		const FSkeletalMeshRenderData* RenderData = Mesh->GetResourceForRendering();
		if (!RenderData || RenderData->LODRenderData.Num() <= 0)
		{
			return false;
		}
		const FPositionVertexBuffer& Positions = RenderData->LODRenderData[0].StaticVertexBuffers.PositionVertexBuffer;
		const uint32 Count = Positions.GetNumVertices();
		if (Count == 0)
		{
			return false;
		}
		FBox Box(ForceInit);
		for (uint32 Index = 0; Index < Count; ++Index)
		{
			Box += FVector(Positions.VertexPosition(Index));
		}
		OutBounds = FBoxSphereBounds(Box);
		return VerticalSpanCm(OutBounds) > 1.f || HorizontalSpanCm(OutBounds) > 1.f;
	}

	FBoxSphereBounds MeasureMesh(UStaticMesh* StaticMesh, USkeletalMesh* SkeletalMesh, FName Id)
	{
		if (StaticMesh)
		{
			return StaticMesh->GetBounds();
		}
		if (!SkeletalMesh)
		{
			return FBoxSphereBounds(FVector::ZeroVector, FVector::ZeroVector, 0.f);
		}
		const FBoxSphereBounds AssetBounds = SkeletalMesh->GetBounds();
		const FBoxSphereBounds ImportedBounds = SkeletalMesh->GetImportedBounds();
		FBoxSphereBounds VertexBounds(FVector::ZeroVector, FVector::ZeroVector, 0.f);
		const bool bVertex = SkeletalVertexBounds(SkeletalMesh, VertexBounds);
		const float AssetHeight = VerticalSpanCm(AssetBounds);
		const float ImportedHeight = VerticalSpanCm(ImportedBounds);
		const float VertexHeight = bVertex ? VerticalSpanCm(VertexBounds) : 0.f;
		FBoxSphereBounds Chosen = AssetBounds;
		const TCHAR* Source = TEXT("Asset");
		if (bVertex && VertexHeight > 1.f && (AssetHeight <= 1.f || VertexHeight < AssetHeight))
		{
			Chosen = VertexBounds;
			Source = TEXT("Vertex");
		}
		else if (ImportedHeight > 1.f && (AssetHeight <= 1.f || ImportedHeight < AssetHeight))
		{
			Chosen = ImportedBounds;
			Source = TEXT("Imported");
		}
		if (AssetHeight > 1.f && VerticalSpanCm(Chosen) < AssetHeight * 0.5f)
		{
			UE_LOG(LogTemp, Log, TEXT("KodMesh SkelBounds Id=%s AssetH=%.0f ImportedH=%.0f VertexH=%.0f Use=%s"),
				*Id.ToString(),
				AssetHeight,
				ImportedHeight,
				VertexHeight,
				Source);
		}
		return Chosen;
	}

	float MeshHeightCm(UStaticMesh* StaticMesh, USkeletalMesh* SkeletalMesh)
	{
		return VerticalSpanCm(MeasureMesh(StaticMesh, SkeletalMesh, NAME_None));
	}

	void ClampDegenerateScale(FName Id, float RawCm, float& InOutScale, float& InOutBounds)
	{
		constexpr float MinScale = 0.05f;
		constexpr float MaxScale = 20.f;
		if (InOutScale >= MinScale && InOutScale <= MaxScale)
		{
			return;
		}
		UE_LOG(LogTemp, Warning, TEXT("KodMesh DegenerateScale Id=%s Scale=%.3f Clamped=1 Raw=%.0f"),
			*Id.ToString(),
			InOutScale,
			RawCm);
		InOutScale = 1.f;
		InOutBounds = RawCm;
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
			FString LoadedPath;
			bool bLoaded = bNewMeshes && LoadMeshReference(WorkerRow->Primary, WorkerId, StaticMesh, SkeletalMesh, LoadedPath);
			if (!bLoaded)
			{
				bLoaded = LoadMeshReference(WorkerRow->Fallback, WorkerId, StaticMesh, SkeletalMesh, LoadedPath);
			}
			if (!bLoaded)
			{
				bLoaded = LoadMeshReference(WorkerRow->Fallback2, WorkerId, StaticMesh, SkeletalMesh, LoadedPath);
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
		return 800.f;
	}
	if (Id == FName(KodWardenPaths::Id_SupplyDepot) || Id == FName(TEXT("RiteWell")))
	{
		return 450.f;
	}
	if (Id == FName(KodWardenPaths::Id_Bunker) || Id == FName(TEXT("Bunker")))
	{
		return 600.f;
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
	UStaticMesh* StaticMesh = nullptr;
	USkeletalMesh* SkeletalMesh = nullptr;
	FString Chosen;
	bool bLoaded = bNewMeshes && LoadMeshReference(Row->Primary, Id, StaticMesh, SkeletalMesh, Chosen);
	bool bFallback = false;
	if (!bLoaded)
	{
		bFallback = true;
		bLoaded = LoadMeshReference(Row->Fallback, Id, StaticMesh, SkeletalMesh, Chosen);
	}
	if (!bLoaded && !Row->Fallback2.IsEmpty())
	{
		bFallback = true;
		bLoaded = LoadMeshReference(Row->Fallback2, Id, StaticMesh, SkeletalMesh, Chosen);
	}
	Choice.Path = Chosen;
	Choice.bLoaded = bLoaded;
	Choice.bFallback = bFallback;
	Choice.StaticMesh = StaticMesh;
	Choice.SkeletalMesh = SkeletalMesh;
	Choice.bSkeletal = SkeletalMesh != nullptr;
	const FBoxSphereBounds RawBounds = MeasureMesh(StaticMesh, SkeletalMesh, Id);
	const float RawSpan = (StaticMesh || SkeletalMesh) ? HorizontalSpanCm(RawBounds) : 0.f;
	const float RawHeight = (StaticMesh || SkeletalMesh) ? VerticalSpanCm(RawBounds) : 0.f;
	const float RowScale = Row->Scale > 0.f ? Row->Scale : 1.f;
	EKodMeshFit Fit = Row->Fit;
	if (Fit == EKodMeshFit::Unset)
	{
		if (FootprintCm(Id) > 0.f)
		{
			Fit = EKodMeshFit::Footprint;
		}
		else if (Id == FName(KodWardenPaths::Id_Dozer) || Id == FName(TEXT("ScinderHost")))
		{
			Fit = EKodMeshFit::Length;
		}
		else
		{
			Fit = EKodMeshFit::Height;
		}
	}
	float FitCm = Row->FitCm;
	if (Fit == EKodMeshFit::Length && FitCm <= KINDA_SMALL_NUMBER)
	{
		FitCm = 300.f;
	}
	else if (Fit == EKodMeshFit::Footprint && FitCm <= KINDA_SMALL_NUMBER)
	{
		FitCm = FootprintCm(Id);
	}
	else if (Fit == EKodMeshFit::Height && FitCm <= KINDA_SMALL_NUMBER)
	{
		FitCm = UnitTargetHeightCm(Id);
	}
	if (Fit == EKodMeshFit::Footprint && FitCm > 0.f && RawSpan > KINDA_SMALL_NUMBER)
	{
		Choice.Scale = (FitCm / RawSpan) * RowScale;
		Choice.BoundsCm = RawSpan * Choice.Scale;
	}
	else if (Fit == EKodMeshFit::Length && RawSpan > KINDA_SMALL_NUMBER)
	{
		// Horizontal max extent sets the length. MinHeight only lifts it when that length is shorter.
		const float LengthTarget = (Row->MinHeightCm > FitCm) ? Row->MinHeightCm : FitCm;
		Choice.Scale = (LengthTarget / RawSpan) * RowScale;
		Choice.BoundsCm = RawSpan * Choice.Scale;
	}
	else if (Fit == EKodMeshFit::Height && FitCm > 0.f && RawHeight > KINDA_SMALL_NUMBER)
	{
		Choice.Scale = (FitCm / RawHeight) * RowScale;
		Choice.BoundsCm = RawHeight * Choice.Scale;
	}
	else if (Fit == EKodMeshFit::Height && bLoaded && RawHeight > KINDA_SMALL_NUMBER)
	{
		Choice.Scale = FactionWorkerScale(IsScinderId(Id)) * RowScale;
		Choice.BoundsCm = RawHeight * Choice.Scale;
	}
	else
	{
		Choice.Scale = RowScale;
		Choice.BoundsCm = RawSpan * Choice.Scale;
	}
	const float RawForClamp = (Fit == EKodMeshFit::Length || Fit == EKodMeshFit::Footprint) ? RawSpan : RawHeight;
	ClampDegenerateScale(Id, RawForClamp, Choice.Scale, Choice.BoundsCm);
	UE_LOG(LogTemp, Log, TEXT("KodMesh Id=%s Path=%s Loaded=%d Fallback=%d Bounds=%.0f Scale=%.3f"),
		*Id.ToString(),
		*Choice.Path,
		Choice.bLoaded ? 1 : 0,
		Choice.bFallback ? 1 : 0,
		Choice.BoundsCm,
		Choice.Scale);
	if (Choice.Path.Contains(TEXT("/Engine/BasicShapes/Cube")))
	{
		UE_LOG(LogTemp, Log, TEXT("KodMesh Placeholder Id=%s Path=%s"), *Id.ToString(), *Choice.Path);
	}
	if (Choice.bLoaded)
	{
		const int32 TexCap = Fit == EKodMeshFit::Footprint ? 4096 : 2048;
		CapImportedTextures(StaticMesh, SkeletalMesh, TexCap);
	}
	return Choice;
}

bool KodMeshCatalogFindRow(FName Id, FKodMeshRow& OutRow)
{
	if (const FKodMeshRow* Row = FindRow(Id))
	{
		OutRow = *Row;
		return true;
	}
	return false;
}

void KodMeshCatalogCopyAttaches(FName OwnerId, TArray<FKodMeshAttach>& OutAttaches)
{
	LoadRows();
	OutAttaches.Reset();
	for (const FKodMeshAttach& Attach : GAttaches)
	{
		if (Attach.OwnerId == OwnerId)
		{
			OutAttaches.Add(Attach);
		}
	}
}
