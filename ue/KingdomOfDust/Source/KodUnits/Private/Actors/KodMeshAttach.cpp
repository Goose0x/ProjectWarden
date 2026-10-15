#include "Actors/KodMeshAttach.h"

#include "Animation/AnimSequence.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Data/KodMeshCatalog.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Misc/Paths.h"

namespace
{
	const FName AttachTag(TEXT("KodMeshAttach"));
	constexpr float RampOpenSeconds = 1.2f;
	constexpr float RampCloseDelay = 2.f;

	FString ObjectPath(const FString& Path)
	{
		const FString Trimmed = Path.TrimStartAndEnd();
		if (Trimmed.IsEmpty() || Trimmed.Contains(TEXT(".")))
		{
			return Trimmed;
		}
		const FString Name = FPaths::GetBaseFilename(Trimmed);
		return Name.IsEmpty() ? Trimmed : Trimmed + TEXT(".") + Name;
	}

	void SilencePiece(UPrimitiveComponent* Piece)
	{
		if (!Piece)
		{
			return;
		}
		Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Piece->SetGenerateOverlapEvents(false);
		Piece->SetCanEverAffectNavigation(false);
	}

	enum class EAttachMount : uint8
	{
		Socket,
		Origin,
		Skip
	};

	bool BodyHasSocket(const USceneComponent* Body, FName Name, FName& OutResolved)
	{
		if (!Body || Name.IsNone())
		{
			return false;
		}
		if (Body->DoesSocketExist(Name))
		{
			OutResolved = Name;
			return true;
		}
		const FName Prefixed(*FString::Printf(TEXT("SOCKET_%s"), *Name.ToString()));
		if (Body->DoesSocketExist(Prefixed))
		{
			OutResolved = Prefixed;
			return true;
		}
		return false;
	}

	EAttachMount ResolveMount(const USceneComponent* Body, const FKodMeshAttach& Row, FName& OutSocket)
	{
		OutSocket = NAME_None;
		if (Row.SocketChain.Num() == 0)
		{
			return EAttachMount::Origin;
		}
		for (const FName& Candidate : Row.SocketChain)
		{
			if (Candidate.IsNone())
			{
				return EAttachMount::Origin;
			}
			FName Resolved = NAME_None;
			if (BodyHasSocket(Body, Candidate, Resolved))
			{
				OutSocket = Resolved;
				return EAttachMount::Socket;
			}
		}
		return EAttachMount::Skip;
	}

	FString ChainLabel(const TArray<FName>& Chain)
	{
		FString Label;
		for (const FName& Name : Chain)
		{
			if (!Label.IsEmpty())
			{
				Label += TEXT("|");
			}
			Label += Name.IsNone() ? TEXT("None") : Name.ToString();
		}
		return Label.IsEmpty() ? FString(TEXT("None")) : Label;
	}
}

UKodMeshAttachComponent::UKodMeshAttachComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UKodMeshAttachComponent::SetRamp(
	USkeletalMeshComponent* InRamp,
	const FString& IdleClosed,
	const FString& Open,
	const FString& Close,
	const FString& IdleOpen)
{
	RampMesh = InRamp;
	IdleClosedPath = IdleClosed;
	OpenPath = Open;
	ClosePath = Close;
	IdleOpenPath = IdleOpen;
	Age = -1.f;
	bHeldOpen = false;
	bClosing = false;
	float IdleLength = 0.f;
	PlayClip(IdleClosedPath, true, IdleLength);
}

void UKodMeshAttachComponent::ClearRamp()
{
	RampMesh = nullptr;
	Age = -1.f;
	bHeldOpen = false;
	bClosing = false;
}

bool UKodMeshAttachComponent::PlayClip(const FString& Path, bool bLoop, float& OutLength)
{
	OutLength = 0.f;
	if (!RampMesh || Path.IsEmpty())
	{
		return false;
	}
	UAnimSequence* Sequence = LoadObject<UAnimSequence>(nullptr, *ObjectPath(Path), nullptr, LOAD_NoWarn | LOAD_Quiet);
	USkeletalMesh* Body = RampMesh->GetSkeletalMeshAsset();
	USkeleton* MeshSkeleton = Body ? Body->GetSkeleton() : nullptr;
	if (!Sequence || (MeshSkeleton && Sequence->GetSkeleton() && Sequence->GetSkeleton() != MeshSkeleton))
	{
		UE_LOG(LogTemp, Log, TEXT("KodMesh AttachAnimMiss Path=%s"), *Path);
		return false;
	}
	OutLength = Sequence->GetPlayLength();
	RampMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	RampMesh->bPauseAnims = false;
	RampMesh->PlayAnimation(Sequence, bLoop);
	return true;
}

void UKodMeshAttachComponent::NotifyTrainExit()
{
	if (!RampMesh)
	{
		return;
	}
	Age = 0.f;
	bHeldOpen = false;
	bClosing = false;
	CloseLength = RampOpenSeconds;
	float OpenLength = 0.f;
	if (PlayClip(OpenPath, false, OpenLength))
	{
		UE_LOG(LogTemp, Log, TEXT("KodMesh Ramp Phase=Open"));
	}
}

bool UKodMeshAttachComponent::TryGetExitPoint(FVector& OutWorld) const
{
	if (!RampMesh)
	{
		return false;
	}
	OutWorld = RampMesh->GetComponentLocation();
	return true;
}

void UKodMeshAttachComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	(void)TickType;
	(void)ThisTickFunction;
	if (Age < 0.f || !RampMesh)
	{
		return;
	}
	Age += FMath::Max(0.f, DeltaTime);
	if (!bHeldOpen && Age >= RampOpenSeconds)
	{
		bHeldOpen = true;
		float HeldLength = 0.f;
		PlayClip(IdleOpenPath, true, HeldLength);
	}
	if (!bClosing && Age >= RampCloseDelay)
	{
		bClosing = true;
		CloseStarted = Age;
		if (!PlayClip(ClosePath, false, CloseLength))
		{
			CloseLength = RampOpenSeconds;
		}
		UE_LOG(LogTemp, Log, TEXT("KodMesh Ramp Phase=Close"));
	}
	if (bClosing && Age >= CloseStarted + FMath::Max(0.05f, CloseLength))
	{
		float IdleLength = 0.f;
		PlayClip(IdleClosedPath, true, IdleLength);
		Age = -1.f;
		bHeldOpen = false;
		bClosing = false;
		UE_LOG(LogTemp, Log, TEXT("KodMesh Ramp Phase=IdleClosed"));
	}
}

void KodMeshAttach::Apply(AActor* Actor, USceneComponent* Body, FName OwnerId, bool bPrimaryBody)
{
	if (!Actor)
	{
		return;
	}
	TInlineComponentArray<USceneComponent*> Pieces;
	Actor->GetComponents(Pieces);
	for (USceneComponent* Piece : Pieces)
	{
		if (Piece && Piece->ComponentHasTag(AttachTag))
		{
			Piece->DestroyComponent();
		}
	}
	UKodMeshAttachComponent* Visual = Actor->FindComponentByClass<UKodMeshAttachComponent>();
	if (!bPrimaryBody || !Body)
	{
		if (Visual)
		{
			Visual->ClearRamp();
		}
		return;
	}
	TArray<FKodMeshAttach> Rows;
	KodMeshCatalogCopyAttaches(OwnerId, Rows);
	if (Rows.Num() == 0)
	{
		if (Visual)
		{
			Visual->ClearRamp();
		}
		return;
	}
	if (!Visual)
	{
		Visual = NewObject<UKodMeshAttachComponent>(Actor, TEXT("KodMeshAttach"));
		if (!Visual)
		{
			return;
		}
		Visual->RegisterComponent();
	}
	Visual->ClearRamp();
	for (const FKodMeshAttach& Row : Rows)
	{
		FName Socket = NAME_None;
		const EAttachMount Mount = ResolveMount(Body, Row, Socket);
		if (Mount == EAttachMount::Skip)
		{
			UE_LOG(LogTemp, Log, TEXT("KodMesh AttachMiss Owner=%s Socket=%s"), *OwnerId.ToString(), *ChainLabel(Row.SocketChain));
			continue;
		}
		const FString Path = ObjectPath(Row.Mesh);
		UStaticMesh* StaticMesh = LoadObject<UStaticMesh>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
		USkeletalMesh* SkeletalMesh = StaticMesh
			? nullptr
			: LoadObject<USkeletalMesh>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!StaticMesh && !SkeletalMesh)
		{
			UE_LOG(LogTemp, Log, TEXT("KodMesh AttachMiss Owner=%s Mesh=%s"), *OwnerId.ToString(), *Row.Mesh);
			continue;
		}
		USceneComponent* Mounted = nullptr;
		if (StaticMesh)
		{
			UStaticMeshComponent* Piece = NewObject<UStaticMeshComponent>(Actor);
			if (!Piece)
			{
				continue;
			}
			Piece->ComponentTags.Add(AttachTag);
			Piece->SetMobility(EComponentMobility::Movable);
			Piece->SetStaticMesh(StaticMesh);
			SilencePiece(Piece);
			if (Socket.IsNone())
			{
				Piece->SetupAttachment(Body);
			}
			else
			{
				Piece->SetupAttachment(Body, Socket);
			}
			Piece->SetRelativeTransform(FTransform::Identity);
			Piece->RegisterComponent();
			Mounted = Piece;
		}
		else if (USkeletalMeshComponent* Piece = NewObject<USkeletalMeshComponent>(Actor))
		{
			Piece->ComponentTags.Add(AttachTag);
			Piece->SetMobility(EComponentMobility::Movable);
			Piece->SetSkeletalMeshAsset(SkeletalMesh);
			Piece->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
			SilencePiece(Piece);
			if (Socket.IsNone())
			{
				Piece->SetupAttachment(Body);
			}
			else
			{
				Piece->SetupAttachment(Body, Socket);
			}
			Piece->SetRelativeTransform(FTransform::Identity);
			Piece->RegisterComponent();
			Mounted = Piece;
			if (!Row.Anim.IsEmpty())
			{
				Visual->SetRamp(Piece, Row.Anim, Row.OpenAnim, Row.CloseAnim, Row.IdleOpenAnim);
			}
		}
		if (Mounted)
		{
			const FString SocketName = Socket.IsNone() ? FString(TEXT("None")) : Socket.ToString();
			UE_LOG(LogTemp, Log, TEXT("KodMesh Attach Owner=%s Mesh=%s Socket=%s"),
				*OwnerId.ToString(),
				*FPaths::GetBaseFilename(Row.Mesh),
				*SocketName);
		}
	}
}
