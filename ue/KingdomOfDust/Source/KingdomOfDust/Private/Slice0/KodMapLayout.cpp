#include "Slice0/KodMapLayout.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sim/KodBuildTypes.h"
#include "Sim/KodSimSubsystem.h"

namespace
{
	int64 PackCell(int32 X, int32 Y)
	{
		return (static_cast<int64>(X) << 32) | static_cast<uint32>(Y);
	}

	bool LoadRoot(const TCHAR* Folder, const TCHAR* FileName, TSharedPtr<FJsonObject>& OutRoot)
	{
		const FString Path = FPaths::Combine(FPaths::ProjectConfigDir(), Folder, FileName);
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Path))
		{
			UE_LOG(LogTemp, Warning, TEXT("KodMap File Miss %s"), *Path);
			return false;
		}
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		return FJsonSerializer::Deserialize(Reader, OutRoot) && OutRoot.IsValid();
	}

	void ReadRect(const TSharedPtr<FJsonObject>& Cells, int32& X, int32& Y, int32& W, int32& H)
	{
		X = Y = 0;
		W = H = 1;
		const TArray<TSharedPtr<FJsonValue>>* Rect = nullptr;
		if (!Cells.IsValid())
		{
			return;
		}
		if (Cells->TryGetArrayField(TEXT("rect_xywh"), Rect) && Rect && Rect->Num() >= 4)
		{
			X = FMath::RoundToInt((*Rect)[0]->AsNumber());
			Y = FMath::RoundToInt((*Rect)[1]->AsNumber());
			W = FMath::Max(1, FMath::RoundToInt((*Rect)[2]->AsNumber()));
			H = FMath::Max(1, FMath::RoundToInt((*Rect)[3]->AsNumber()));
		}
	}

	FVector BoxCenter(const TSharedPtr<FJsonObject>& Feature, float Z)
	{
		const TSharedPtr<FJsonObject>* Box = nullptr;
		if (Feature->TryGetObjectField(TEXT("box_uu"), Box) && Box && Box->IsValid())
		{
			const TArray<TSharedPtr<FJsonValue>>* Min = nullptr;
			const TArray<TSharedPtr<FJsonValue>>* Max = nullptr;
			if ((*Box)->TryGetArrayField(TEXT("min"), Min) && (*Box)->TryGetArrayField(TEXT("max"), Max)
				&& Min && Max && Min->Num() >= 2 && Max->Num() >= 2)
			{
				return FVector(
					static_cast<float>(((*Min)[0]->AsNumber() + (*Max)[0]->AsNumber()) * 0.5),
					static_cast<float>(((*Min)[1]->AsNumber() + (*Max)[1]->AsNumber()) * 0.5),
					Z);
			}
		}
		const TArray<TSharedPtr<FJsonValue>>* Centre = nullptr;
		if (Feature->TryGetArrayField(TEXT("centre_uu"), Centre) && Centre && Centre->Num() >= 2)
		{
			return FVector(static_cast<float>((*Centre)[0]->AsNumber()), static_cast<float>((*Centre)[1]->AsNumber()), Z);
		}
		return FVector(0.f, 0.f, Z);
	}

	void TagActor(AActor* Actor, const FString& FeatureTag)
	{
		if (!Actor || FeatureTag.IsEmpty())
		{
			return;
		}
		Actor->Tags.AddUnique(FName(*FeatureTag));
	}

	void SpawnMarker(UWorld* World, const FVector& Location, const FVector& Scale, const FString& FeatureTag, UStaticMesh* Mesh)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(Location), Params);
		if (!Actor)
		{
			return;
		}
		TagActor(Actor, FeatureTag);
		if (FeatureTag != TEXT("KodBaseSocket") && FeatureTag != TEXT("KodStartSpot_0") && FeatureTag != TEXT("KodStartSpot_1"))
		{
			Actor->Tags.AddUnique(FName(*FeatureTag));
		}
		if (FeatureTag.StartsWith(TEXT("KodStartSpot")) || FeatureTag == TEXT("KodBaseSocket"))
		{
			Actor->Tags.AddUnique(FName(TEXT("KodBaseSocket")));
		}
		UStaticMeshComponent* Comp = Actor->GetStaticMeshComponent();
		if (!Comp || !Mesh)
		{
			return;
		}
		Comp->SetMobility(EComponentMobility::Movable);
		Comp->SetStaticMesh(Mesh);
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Comp->SetCanEverAffectNavigation(false);
		Actor->SetActorScale3D(Scale);
		Comp->SetMobility(EComponentMobility::Static);
	}
}

void KodMapLayout::Load(UWorld* World)
{
	if (!World)
	{
		return;
	}
	UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>();
	if (!Sim)
	{
		return;
	}
	const bool bFaultline = World->GetMapName().Contains(TEXT("Faultline"));
	const TCHAR* Folder = bFaultline ? TEXT("KodDress/faultline_training") : TEXT("KodDress");
	const int32 CellBias = bFaultline ? -56 : 0;
	TSharedPtr<FJsonObject> Root;
	if (!LoadRoot(Folder, TEXT("map_features.json"), Root))
	{
		return;
	}
	const TArray<TSharedPtr<FJsonValue>>* Features = nullptr;
	if (!Root->TryGetArrayField(TEXT("features"), Features) || !Features)
	{
		return;
	}

	TArray<int64> Cliffs;
	TArray<int64> Vision;
	TArray<int64> HighGround;
	TArray<FVector2D> Sockets;
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	int32 Destructibles = 0;

	for (const TSharedPtr<FJsonValue>& Value : *Features)
	{
		const TSharedPtr<FJsonObject> Feature = Value.IsValid() ? Value->AsObject() : nullptr;
		if (!Feature.IsValid())
		{
			continue;
		}
		FString Type;
		FString Id;
		FString Tag;
		Feature->TryGetStringField(TEXT("type"), Type);
		Feature->TryGetStringField(TEXT("id"), Id);
		Feature->TryGetStringField(TEXT("tag"), Tag);
		const TSharedPtr<FJsonObject>* Cells = nullptr;
		Feature->TryGetObjectField(TEXT("cells"), Cells);
		int32 CellX = 0;
		int32 CellY = 0;
		int32 CellW = 1;
		int32 CellH = 1;
		ReadRect(Cells ? *Cells : nullptr, CellX, CellY, CellW, CellH);

		if (Type == TEXT("cliff-block"))
		{
			for (int32 Y = CellY; Y < CellY + CellH; ++Y)
			{
				for (int32 X = CellX; X < CellX + CellW; ++X)
				{
					Cliffs.Add(PackCell(X + CellBias, Y + CellBias));
				}
			}
			const FVector Center = BoxCenter(Feature, 250.f);
			const TSharedPtr<FJsonObject>* Box = nullptr;
			float SpanX = 200.f;
			float SpanY = 200.f;
			if (Feature->TryGetObjectField(TEXT("box_uu"), Box) && Box && Box->IsValid())
			{
				const TArray<TSharedPtr<FJsonValue>>* Min = nullptr;
				const TArray<TSharedPtr<FJsonValue>>* Max = nullptr;
				if ((*Box)->TryGetArrayField(TEXT("min"), Min) && (*Box)->TryGetArrayField(TEXT("max"), Max)
					&& Min && Max && Min->Num() >= 2 && Max->Num() >= 2)
				{
					SpanX = FMath::Max(100.f, static_cast<float>((*Max)[0]->AsNumber() - (*Min)[0]->AsNumber()));
					SpanY = FMath::Max(100.f, static_cast<float>((*Max)[1]->AsNumber() - (*Min)[1]->AsNumber()));
				}
			}
			SpawnMarker(World, Center, FVector(SpanX / 100.f, SpanY / 100.f, 5.f), Tag.IsEmpty() ? FString(TEXT("KodCliffBlock")) : Tag, Cube);
		}
		else if (Type == TEXT("LOS"))
		{
			for (int32 Y = CellY; Y < CellY + CellH; ++Y)
			{
				for (int32 X = CellX; X < CellX + CellW; ++X)
				{
					Vision.Add(PackCell(X + CellBias, Y + CellBias));
				}
			}
			SpawnMarker(World, BoxCenter(Feature, 0.f), FVector(CellW * 2.f, CellH * 2.f, 1.f), Tag.IsEmpty() ? FString(TEXT("KodLOS")) : Tag, nullptr);
		}
		else if (Type == TEXT("destructible"))
		{
			double HP = 2000.0;
			double Armor = 8.0;
			Feature->TryGetNumberField(TEXT("hp"), HP);
			Feature->TryGetNumberField(TEXT("armor"), Armor);
			const FVector Center = BoxCenter(Feature, 0.f);
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(Center), Params);
			if (Actor)
			{
				TagActor(Actor, Tag.IsEmpty() ? FString(TEXT("KodDestructible")) : Tag);
				USceneComponent* DestructRoot = NewObject<USceneComponent>(Actor, TEXT("Root"));
				Actor->SetRootComponent(DestructRoot);
				DestructRoot->RegisterComponent();
				Actor->SetActorLocation(Center);
				Sim->RegisterMapDestructible(Actor, CellX, CellY, CellX + CellW, CellY + CellH, static_cast<float>(HP), FMath::RoundToInt(Armor));
				++Destructibles;
			}
		}
		else if (Type == TEXT("high-ground"))
		{
			for (int32 Y = CellY; Y < CellY + CellH; ++Y)
			{
				for (int32 X = CellX; X < CellX + CellW; ++X)
				{
					HighGround.Add(PackCell(X + CellBias, Y + CellBias));
				}
			}
			SpawnMarker(World, BoxCenter(Feature, 500.f), FVector::OneVector, Tag.IsEmpty() ? FString(TEXT("KodHighGround")) : Tag, nullptr);
		}
		else if (Type == TEXT("ramp") || Type == TEXT("choke") || Type == TEXT("route"))
		{
			FVector At = BoxCenter(Feature, 0.f);
			if (Type == TEXT("route"))
			{
				const TArray<TSharedPtr<FJsonValue>>* Line = nullptr;
				if (Feature->TryGetArrayField(TEXT("centreline_uu"), Line) && Line && Line->Num() > 0)
				{
					const int32 Mid = Line->Num() / 2;
					if ((*Line)[Mid].IsValid() && (*Line)[Mid]->Type == EJson::Array)
					{
						const TArray<TSharedPtr<FJsonValue>>& Point = (*Line)[Mid]->AsArray();
						if (Point.Num() >= 2)
						{
							At = FVector(static_cast<float>(Point[0]->AsNumber()), static_cast<float>(Point[1]->AsNumber()), 0.f);
						}
					}
				}
			}
			const FString UseTag = !Tag.IsEmpty() ? Tag : (Type == TEXT("ramp") ? TEXT("KodRamp") : (Type == TEXT("choke") ? TEXT("KodChoke") : TEXT("KodRoute")));
			SpawnMarker(World, At, FVector(CellW * 2.f, CellH * 2.f, 1.f), UseTag, nullptr);
		}
		else if (Type == TEXT("base-keepclear"))
		{
			const TArray<TSharedPtr<FJsonValue>>* Centre = nullptr;
			if (Feature->TryGetArrayField(TEXT("centre_uu"), Centre) && Centre && Centre->Num() >= 2)
			{
				const FVector2D Socket(static_cast<float>((*Centre)[0]->AsNumber()), static_cast<float>((*Centre)[1]->AsNumber()));
				Sockets.Add(Socket);
				SpawnMarker(World, FVector(Socket.X, Socket.Y, 0.f), FVector::OneVector, Tag, nullptr);
			}
		}
	}

	const int32 CliffCount = Cliffs.Num();
	const int32 VisionCount = Vision.Num();
	const int32 HighCount = HighGround.Num();
	Sim->SetCliffBlockCells(MoveTemp(Cliffs));
	Sim->SetVisionBlockCells(MoveTemp(Vision));
	Sim->SetHighGroundCells(MoveTemp(HighGround));
	Sim->SetLayoutSockets(Sockets);
	UE_LOG(LogTemp, Log, TEXT("KodMap Features Cliffs=%d Vision=%d High=%d Destructibles=%d Sockets=%d Match=%d"),
		CliffCount,
		VisionCount,
		HighCount,
		Destructibles,
		Sockets.Num(),
		Sim->LayoutSocketsMatch() ? 1 : 0);
}
