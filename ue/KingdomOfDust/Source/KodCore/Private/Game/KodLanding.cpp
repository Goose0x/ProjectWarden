#include "Game/KodLanding.h"

#include "Game/KodBuildingYaw.h"
#include "Game/KodMeshSockets.h"
#include "GameFramework/Actor.h"
#include "Sim/KodBuildTypes.h"

namespace
{
	constexpr float YawTolerance = 0.01f;
	constexpr float GridTolerance = 1.f;
	constexpr float SocketTolerance = 8.f;

	const FName CheckedSockets[] = {
		KodMeshSockets::Door,
		KodMeshSockets::UnitExit,
		KodMeshSockets::WorkerExit,
		KodMeshSockets::BayRamp,
		KodMeshSockets::Rally
	};

	void CollectSockets(const AActor* Actor, TArray<FVector>& OutLocal, TArray<FVector>& OutWorld)
	{
		OutLocal.Reset();
		OutWorld.Reset();
		if (!Actor)
		{
			return;
		}
		const FTransform ActorXf = Actor->GetActorTransform();
		for (const FName SocketName : CheckedSockets)
		{
			FVector World = FVector::ZeroVector;
			if (!KodMeshSockets::TryGet(Actor, SocketName, World))
			{
				continue;
			}
			OutWorld.Add(World);
			OutLocal.Add(ActorXf.InverseTransformPosition(World));
		}
	}
}

float KodLanding::CanonicalYaw()
{
	return KodBuildingYaw::OffsetDegrees();
}

FRotator KodLanding::CanonicalRotation()
{
	return FRotator(0.f, CanonicalYaw(), 0.f);
}

FVector KodLanding::FootprintCenter(int32 CellX, int32 CellY, int32 FootX, int32 FootY, float GroundZ)
{
	const float SpanX = static_cast<float>(FMath::Max(1, FootX)) * static_cast<float>(KodBuildGrid::CellUU);
	const float SpanY = static_cast<float>(FMath::Max(1, FootY)) * static_cast<float>(KodBuildGrid::CellUU);
	const float MinX = static_cast<float>(CellX) * static_cast<float>(KodBuildGrid::CellUU);
	const float MinY = static_cast<float>(CellY) * static_cast<float>(KodBuildGrid::CellUU);
	return FVector(MinX + SpanX * 0.5f, MinY + SpanY * 0.5f, GroundZ);
}

FKodLandingPose KodLanding::MakePose(int32 CellX, int32 CellY, int32 FootX, int32 FootY, float GroundZ)
{
	FKodLandingPose Pose;
	Pose.CellX = CellX;
	Pose.CellY = CellY;
	Pose.FootX = FMath::Max(1, FootX);
	Pose.FootY = FMath::Max(1, FootY);
	Pose.Yaw = CanonicalYaw();
	Pose.Location = FootprintCenter(Pose.CellX, Pose.CellY, Pose.FootX, Pose.FootY, GroundZ);
	return Pose;
}

FKodLandingFault KodLanding::Evaluate(
	const FRotator& Rotation,
	const FVector& Location,
	const FKodLandingPose& Pose,
	const TArray<FVector>& SocketActorSpace,
	const TArray<FVector>& SocketWorld)
{
	FKodLandingFault Fault;
	Fault.bYaw = FMath::Abs(FMath::FindDeltaAngleDegrees(Rotation.Yaw, Pose.Yaw)) > YawTolerance;
	Fault.bLevel = FMath::Abs(Rotation.Pitch) > YawTolerance || FMath::Abs(Rotation.Roll) > YawTolerance;
	Fault.bGrid = FVector::Dist2D(Location, Pose.Location) > GridTolerance
		|| FMath::Abs(Location.Z - Pose.Location.Z) > GridTolerance;
	const int32 Count = FMath::Min(SocketActorSpace.Num(), SocketWorld.Num());
	const FRotator Canonical(0.f, Pose.Yaw, 0.f);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FVector Expected = Pose.Location + Canonical.RotateVector(SocketActorSpace[Index]);
		if (FVector::DistSquared(Expected, SocketWorld[Index]) > FMath::Square(SocketTolerance))
		{
			Fault.bSocket = true;
			break;
		}
	}
	return Fault;
}

FRotator KodLanding::EaseTowardCanonical(const FRotator& Current, float DeltaSeconds)
{
	return FMath::RInterpTo(Current, CanonicalRotation(), FMath::Max(0.f, DeltaSeconds), 12.f);
}

bool KodLanding::ValidateLandingOrientation(AActor* Actor, const FKodLandingPose& Pose)
{
	if (!Actor)
	{
		return true;
	}
	TArray<FVector> Local;
	TArray<FVector> World;
	CollectSockets(Actor, Local, World);
	const FKodLandingFault Fault = Evaluate(Actor->GetActorRotation(), Actor->GetActorLocation(), Pose, Local, World);
	if (!Fault.Any())
	{
		return true;
	}
	UE_LOG(LogTemp, Warning, TEXT("KodLanding Yaw=%d Level=%d Grid=%d Socket=%d Cell=%d,%d"),
		Fault.bYaw ? 1 : 0,
		Fault.bLevel ? 1 : 0,
		Fault.bGrid ? 1 : 0,
		Fault.bSocket ? 1 : 0,
		Pose.CellX,
		Pose.CellY);
	Actor->SetActorLocation(Pose.Location, false, nullptr, ETeleportType::TeleportPhysics);
	Actor->SetActorRotation(FRotator(0.f, Pose.Yaw, 0.f), ETeleportType::TeleportPhysics);
	return false;
}

bool KodLanding::SameTransform(const FVector& LocationA, float YawA, const FVector& LocationB, float YawB)
{
	return FVector::DistSquared(LocationA, LocationB) <= FMath::Square(GridTolerance)
		&& FMath::Abs(FMath::FindDeltaAngleDegrees(YawA, YawB)) <= YawTolerance;
}
