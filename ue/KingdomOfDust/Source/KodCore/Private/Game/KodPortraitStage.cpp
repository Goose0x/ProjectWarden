#include "Game/KodPortraitStage.h"

#include "Components/PointLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

AKodPortraitStage::AKodPortraitStage()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	SkelMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Skel"));
	SkelMesh->SetupAttachment(Root);
	SkelMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StaticMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Static"));
	StaticMesh->SetupAttachment(Root);
	StaticMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Backdrop = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Backdrop"));
	Backdrop->SetupAttachment(Root);
	Backdrop->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Capture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Capture"));
	Capture->SetupAttachment(Root);
	KeyLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("Key"));
	KeyLight->SetupAttachment(Root);
	RimLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("Rim"));
	RimLight->SetupAttachment(Root);
	FillLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("Fill"));
	FillLight->SetupAttachment(Root);
	SetActorLocation(FVector(0.f, 0.f, -200000.f));
	SetActorEnableCollision(false);
}

void AKodPortraitStage::EnsureCapture()
{
	if (RenderTarget || !Capture)
	{
		return;
	}
	RenderTarget = NewObject<UTextureRenderTarget2D>(this);
	RenderTarget->ClearColor = FLinearColor(0.f, 0.f, 0.f, 0.f);
	RenderTarget->InitCustomFormat(256, 256, PF_B8G8R8A8, false);
	RenderTarget->UpdateResourceImmediate(true);
	Capture->TextureTarget = RenderTarget;
	Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	Capture->bCaptureEveryFrame = false;
	Capture->bCaptureOnMovement = false;
	Capture->FOVAngle = 28.f;
	Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	Capture->ShowOnlyActors.Add(this);
	Capture->SetRelativeLocation(FVector(-140.f, 28.f, 150.f));
	Capture->SetRelativeRotation(FRotator(-8.f, 0.f, 0.f));
	KeyLight->SetRelativeLocation(FVector(-80.f, -70.f, 180.f));
	KeyLight->SetIntensity(8000.f);
	KeyLight->SetLightColor(FLinearColor(1.f, 0.95f, 0.85f));
	RimLight->SetRelativeLocation(FVector(40.f, 90.f, 160.f));
	RimLight->SetIntensity(5000.f);
	RimLight->SetLightColor(FLinearColor(0.6f, 0.75f, 1.f));
	FillLight->SetRelativeLocation(FVector(-40.f, 10.f, 80.f));
	FillLight->SetIntensity(2500.f);
	if (UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
	{
		Backdrop->SetStaticMesh(Cube);
	}
	Backdrop->SetRelativeLocation(FVector(80.f, 0.f, 120.f));
	Backdrop->SetRelativeScale3D(FVector(0.4f, 4.f, 4.f));
	if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		BackdropMid = UMaterialInstanceDynamic::Create(Base, this);
		Backdrop->SetMaterial(0, BackdropMid);
	}
}

void AKodPortraitStage::ShowSubject(AActor* Subject, int32 TeamId)
{
	EnsureCapture();
	if (!Subject)
	{
		return;
	}
	SetActorLocation(FVector(0.f, 0.f, -200000.f));
	USkeletalMesh* Portrait = nullptr;
	USkeletalMesh* Body = nullptr;
	UStaticMesh* Solid = nullptr;
	TInlineComponentArray<USkeletalMeshComponent*> Skels;
	Subject->GetComponents(Skels);
	for (USkeletalMeshComponent* Skel : Skels)
	{
		if (!Skel)
		{
			continue;
		}
		USkeletalMesh* Mesh = Skel->GetSkeletalMeshAsset();
		if (!Mesh)
		{
			continue;
		}
		if (Skel->ComponentHasTag(FName(TEXT("PortraitMesh"))))
		{
			Portrait = Mesh;
		}
		else if (!Body)
		{
			Body = Mesh;
		}
	}
	if (!Portrait && !Body)
	{
		TInlineComponentArray<UStaticMeshComponent*> Statics;
		Subject->GetComponents(Statics);
		for (UStaticMeshComponent* MeshComp : Statics)
		{
			if (MeshComp && MeshComp->GetStaticMesh() && !MeshComp->ComponentHasTag(FName(TEXT("KodFx"))))
			{
				Solid = MeshComp->GetStaticMesh();
				break;
			}
		}
	}
	USkeletalMesh* UseSkel = Portrait ? Portrait : Body;
	bSway = UseSkel != nullptr;
	if (SkelMesh)
	{
		SkelMesh->SetSkeletalMeshAsset(UseSkel);
		SkelMesh->SetVisibility(UseSkel != nullptr);
		SkelMesh->SetRelativeLocation(FVector(0.f, 0.f, -40.f));
		SkelMesh->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
	}
	if (StaticMesh)
	{
		StaticMesh->SetStaticMesh(UseSkel ? nullptr : Solid);
		StaticMesh->SetVisibility(UseSkel == nullptr && Solid != nullptr);
		StaticMesh->SetRelativeLocation(FVector(0.f, 0.f, 40.f));
		StaticMesh->SetRelativeScale3D(FVector(1.2f));
	}
	ShownTeam = TeamId;
	IdleSeq = nullptr;
	TalkSeq = nullptr;
	bTalking = false;
	TalkLeft = 0.f;
	if (UKodPortraitHooks* Hooks = Subject->FindComponentByClass<UKodPortraitHooks>())
	{
		IdleSeq = Hooks->GetIdle();
		TalkSeq = Hooks->GetTalk();
	}
	if (SkelMesh && IdleSeq)
	{
		SkelMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		SkelMesh->PlayAnimation(IdleSeq, true);
	}
	else if (SkelMesh)
	{
		SkelMesh->Stop();
	}
	if (BackdropMid)
	{
		const FLinearColor Ink = TeamId == 0
			? FLinearColor(0.15f, 0.28f, 0.18f, 1.f)
			: FLinearColor(0.28f, 0.1f, 0.08f, 1.f);
		BackdropMid->SetVectorParameterValue(TEXT("Color"), Ink);
		BackdropMid->SetVectorParameterValue(TEXT("BaseColor"), Ink);
	}
}

void AKodPortraitStage::CapturePortrait()
{
	EnsureCapture();
	if (Capture)
	{
		Capture->CaptureScene();
	}
}

void AKodPortraitStage::AppendHiddenPrimitives(TSet<FPrimitiveComponentId>& Out) const
{
	TInlineComponentArray<UPrimitiveComponent*> Prims;
	GetComponents(Prims);
	for (UPrimitiveComponent* Prim : Prims)
	{
		if (Prim)
		{
			Out.Add(Prim->GetPrimitiveSceneId());
		}
	}
}

void AKodPortraitStage::NotifyVoice(float Seconds)
{
	if (!TalkSeq || !SkelMesh || Seconds <= 0.f)
	{
		return;
	}
	SkelMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	SkelMesh->PlayAnimation(TalkSeq, true);
	TalkLeft = Seconds;
	bTalking = true;
}

void AKodPortraitStage::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bTalking)
	{
		TalkLeft -= DeltaSeconds;
		if (TalkLeft <= 0.f)
		{
			bTalking = false;
			if (IdleSeq && SkelMesh)
			{
				SkelMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
				SkelMesh->PlayAnimation(IdleSeq, true);
			}
		}
	}
	if (!bSway || !SkelMesh)
	{
		return;
	}
	Sway += DeltaSeconds;
	const float Yaw = 180.f + 6.f * FMath::Sin(Sway * 1.4f);
	SkelMesh->SetRelativeRotation(FRotator(0.f, Yaw, 0.f));
}
