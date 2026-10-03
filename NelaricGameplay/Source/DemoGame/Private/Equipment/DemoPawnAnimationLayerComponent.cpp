// Copyright (c) 2026 Nelaric Contributors

#include "Equipment/DemoPawnAnimationLayerComponent.h"

#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimLayerInterface.h"
#include "Animation/DemoAnimationDataInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Equipment/DemoEquipmentInstance.h"
#include "GameFramework/Pawn.h"

DEFINE_LOG_CATEGORY_STATIC(LogDemoEquipmentAnimation, Log, All);

UDemoPawnAnimationLayerComponent::UDemoPawnAnimationLayerComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	MeshComponentName = TEXT("CharacterMesh0");
}

FGuid UDemoPawnAnimationLayerComponent::AcquireLayer(UDemoEquipmentInstance* Source,
                                                     TSubclassOf<UAnimInstance> LayerClass)
{
	check(IsInGameThread());
	if (bEnding || !IsValid(Source) || !GetPawn() || Source->GetPawn() != GetPawn() || !CanUseLayer(LayerClass))
	{
		return FGuid();
	}
	const FGuid Handle = FGuid::NewGuid();
	Requests.Add(Handle, {Source, LayerClass.Get(), ++NextSequence});
	RefreshLayer();
	return Handle;
}

void UDemoPawnAnimationLayerComponent::ReleaseLayer(FGuid Handle)
{
	check(IsInGameThread());
	if (Requests.Remove(Handle) != 0)
	{
		RefreshLayer();
	}
}

bool UDemoPawnAnimationLayerComponent::HasLayerRequest(FGuid Handle) const
{
	check(IsInGameThread());
	const Nelaric::DemoEquipment::FAnimationRequest* Request = Requests.Find(Handle);
	return !bEnding && Request && Request->Source.IsValid() && Request->LayerClass.IsValid();
}

bool UDemoPawnAnimationLayerComponent::CanUseLayer(TSubclassOf<UAnimInstance> LayerClass) const
{
	check(IsInGameThread());
	if (bEnding || MeshComponentName.IsNone() || !LayerClass || LayerClass->HasAnyClassFlags(CLASS_Abstract))
	{
		return false;
	}
	const UAnimBlueprintGeneratedClass* AnimClass = Cast<UAnimBlueprintGeneratedClass>(LayerClass.Get());
	if (!AnimClass || !AnimClass->GetTargetSkeleton())
	{
		return false;
	}
	USkeletalMeshComponent* Mesh = ResolveMesh();
	const USkeletalMesh* MeshAsset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	// This demo deliberately requires a layer authored for the same skeleton.
	if (MeshAsset && MeshAsset->GetSkeleton() != AnimClass->GetTargetSkeleton())
	{
		return false;
	}
	UClass* MainClass = Mesh ? Mesh->GetAnimClass() : nullptr;
	for (const UClass* Class = LayerClass.Get(); Class; Class = Class->GetSuperClass())
	{
		for (const FImplementedInterface& Interface : Class->Interfaces)
		{
			if (Interface.Class && Interface.Class->IsChildOf(UAnimLayerInterface::StaticClass()) &&
			    (!MainClass || MainClass->ImplementsInterface(Interface.Class)))
			{
				return true;
			}
		}
	}
	return false;
}

USkeletalMeshComponent* UDemoPawnAnimationLayerComponent::ResolveMesh() const
{
	const APawn* Pawn = GetPawn();
	if (!Pawn || MeshComponentName.IsNone())
	{
		return nullptr;
	}
	TInlineComponentArray<USkeletalMeshComponent*> Meshes;
	Pawn->GetComponents(Meshes);
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (IsValid(Mesh) && Mesh->GetFName() == MeshComponentName)
		{
			return Mesh;
		}
	}
	return nullptr;
}

void UDemoPawnAnimationLayerComponent::RefreshLayer()
{
	if (bEnding || !HasBegunPlay() || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	TSubclassOf<UAnimInstance> DesiredClass = DefaultAnimationLayer;
	uint64 WinningSequence = 0;
	for (auto It = Requests.CreateIterator(); It; ++It)
	{
		const Nelaric::DemoEquipment::FAnimationRequest& Request = It.Value();
		UDemoEquipmentInstance* Source = Request.Source.Get();
		if (!Source || Source->GetPawn() != GetPawn() || !Request.LayerClass.IsValid())
		{
			It.RemoveCurrent();
			continue;
		}
		if (Request.Sequence > WinningSequence)
		{
			WinningSequence = Request.Sequence;
			DesiredClass = Request.LayerClass.Get();
		}
	}
	USkeletalMeshComponent* Mesh = ResolveMesh();
	UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (DesiredClass && !CanUseLayer(DesiredClass))
	{
		DesiredClass = nullptr;
	}
	const bool bLinkMissing =
	    bAppliedLinkWasPresent && Mesh && DesiredClass && !Mesh->GetLinkedAnimLayerInstanceByClass(DesiredClass);
	if (AppliedMesh == Mesh && AppliedAnimInstance == AnimInstance && AppliedClass == DesiredClass && !bLinkMissing)
	{
		return;
	}
	ClearAppliedLayer();
	if (IsValid(Mesh) && IsValid(AnimInstance))
	{
		if (DesiredClass)
		{
			Mesh->LinkAnimClassLayers(DesiredClass);
			bAppliedLinkWasPresent = Mesh->GetLinkedAnimLayerInstanceByClass(DesiredClass) != nullptr;
			if (bAppliedLinkWasPresent)
			{
				if (UDemoAnimationDataInstance* Data = Cast<UDemoAnimationDataInstance>(AnimInstance))
				{
					Data->NotifyAnimationLayerChanged();
				}
			}
			if (!bAppliedLinkWasPresent)
			{
				UE_LOG(LogDemoEquipmentAnimation, Warning,
				       TEXT("Layer '%s' could not link on '%s'; check the main animation blueprint's layer interface."),
				       *GetNameSafe(DesiredClass.Get()), *GetNameSafe(Mesh));
			}
		}
		AppliedMesh = Mesh;
		AppliedAnimInstance = AnimInstance;
		AppliedClass = DesiredClass;
	}
}

void UDemoPawnAnimationLayerComponent::ClearAppliedLayer()
{
	USkeletalMeshComponent* Mesh = AppliedMesh.Get();
	if (Mesh && AppliedClass && Mesh->GetAnimInstance() == AppliedAnimInstance.Get())
	{
		Mesh->UnlinkAnimClassLayers(AppliedClass);
		if (bAppliedLinkWasPresent)
		{
			if (UDemoAnimationDataInstance* Data = Cast<UDemoAnimationDataInstance>(Mesh->GetAnimInstance()))
			{
				Data->NotifyAnimationLayerChanged();
			}
		}
	}
	AppliedMesh.Reset();
	AppliedAnimInstance.Reset();
	AppliedClass = nullptr;
	bAppliedLinkWasPresent = false;
}

void UDemoPawnAnimationLayerComponent::BeginPlay()
{
	Super::BeginPlay();
	SetComponentTickEnabled(GetNetMode() != NM_DedicatedServer);
	RefreshLayer();
}

void UDemoPawnAnimationLayerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEnding = true;
	Requests.Empty();
	ClearAppliedLayer();
	Super::EndPlay(EndPlayReason);
}

void UDemoPawnAnimationLayerComponent::OnUnregister()
{
	Requests.Empty();
	ClearAppliedLayer();
	Super::OnUnregister();
}

void UDemoPawnAnimationLayerComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                                     FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	RefreshLayer();
}
