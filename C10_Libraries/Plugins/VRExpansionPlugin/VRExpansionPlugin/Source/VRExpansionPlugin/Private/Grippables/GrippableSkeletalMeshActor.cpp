// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Grippables/GrippableSkeletalMeshActor.h"
#include "Net/UnrealNetwork.h"

UOptionalRepSkeletalMeshComponent::UOptionalRepSkeletalMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bReplicateMovement = true;
}

void UOptionalRepSkeletalMeshComponent::PreReplication(IRepChangedPropertyTracker & ChangedPropertyTracker)
{
	Super::PreReplication(ChangedPropertyTracker);

	// Don't replicate if set to not do it
	DOREPLIFETIME_ACTIVE_OVERRIDE(USceneComponent, RelativeLocation, bReplicateMovement);
	DOREPLIFETIME_ACTIVE_OVERRIDE(USceneComponent, RelativeRotation, bReplicateMovement);
	DOREPLIFETIME_ACTIVE_OVERRIDE(USceneComponent, RelativeScale3D, bReplicateMovement);
}

void UOptionalRepSkeletalMeshComponent::GetLifetimeReplicatedProps(TArray< class FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UOptionalRepSkeletalMeshComponent, bReplicateMovement);
}

  //=============================================================================
AGrippableSkeletalMeshActor::AGrippableSkeletalMeshActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UOptionalRepSkeletalMeshComponent>(TEXT("SkeletalMeshComponent0")))
{
	VRGripInterfaceSettings.bDenyGripping = false;
	VRGripInterfaceSettings.OnTeleportBehavior = EGripInterfaceTeleportBehavior::TeleportAllComponents;
	VRGripInterfaceSettings.bSimulateOnDrop = true;
	VRGripInterfaceSettings.SlotDefaultGripType = EGripCollisionType::InteractiveCollisionWithPhysics;
	VRGripInterfaceSettings.FreeDefaultGripType = EGripCollisionType::InteractiveCollisionWithPhysics;
	//VRGripInterfaceSettings.bCanHaveDoubleGrip = false;
	VRGripInterfaceSettings.SecondaryGripType = ESecondaryGripType::SG_None;
	//VRGripInterfaceSettings.GripTarget = EGripTargetType::ActorGrip;
	VRGripInterfaceSettings.MovementReplicationType = EGripMovementReplicationSettings::ForceClientSideMovement;
	VRGripInterfaceSettings.LateUpdateSetting = EGripLateUpdateSettings::NotWhenCollidingOrDoubleGripping;
	VRGripInterfaceSettings.ConstraintStiffness = 1500.0f;
	VRGripInterfaceSettings.ConstraintDamping = 200.0f;
	VRGripInterfaceSettings.ConstraintBreakDistance = 100.0f;
	VRGripInterfaceSettings.SecondarySlotRange = 20.0f;
	VRGripInterfaceSettings.PrimarySlotRange = 20.0f;
	//VRGripInterfaceSettings.bIsInteractible = false;

	VRGripInterfaceSettings.bIsHeld = false;
	VRGripInterfaceSettings.HoldingController = nullptr;

	// Default replication on for multiplayer
	//this->bNetLoadOnClient = false;
	this->bReplicateMovement = true;
	this->bReplicates = true;

	bRepGripSettingsAndGameplayTags = true;
	bAllowIgnoringAttachOnOwner = true;

	// Setting a minimum of every 3rd frame (VR 90fps) for replication consideration
	// Otherwise we will get some massive slow downs if the replication is allowed to hit the 2 per second minimum default
	MinNetUpdateFrequency = 30.0f;
}

void AGrippableSkeletalMeshActor::GetLifetimeReplicatedProps(TArray< class FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME/*_CONDITION*/(AGrippableSkeletalMeshActor, GripLogicScripts);// , COND_Custom);
	DOREPLIFETIME(AGrippableSkeletalMeshActor, bRepGripSettingsAndGameplayTags);
	DOREPLIFETIME(AGrippableSkeletalMeshActor, bAllowIgnoringAttachOnOwner);
	DOREPLIFETIME(AGrippableSkeletalMeshActor, ClientAuthReplicationData);
	DOREPLIFETIME_CONDITION(AGrippableSkeletalMeshActor, VRGripInterfaceSettings, COND_Custom);
	DOREPLIFETIME_CONDITION(AGrippableSkeletalMeshActor, GameplayTags, COND_Custom);
}

void AGrippableSkeletalMeshActor::PreReplication(IRepChangedPropertyTracker & ChangedPropertyTracker)
{
	Super::PreReplication(ChangedPropertyTracker);

	// Don't replicate if set to not do it
	DOREPLIFETIME_ACTIVE_OVERRIDE(AGrippableSkeletalMeshActor, VRGripInterfaceSettings, bRepGripSettingsAndGameplayTags);
	DOREPLIFETIME_ACTIVE_OVERRIDE(AGrippableSkeletalMeshActor, GameplayTags, bRepGripSettingsAndGameplayTags);
}

bool AGrippableSkeletalMeshActor::ReplicateSubobjects(UActorChannel* Channel, class FOutBunch *Bunch, FReplicationFlags *RepFlags)
{
	bool WroteSomething = Super::ReplicateSubobjects(Channel, Bunch, RepFlags);

	for (UVRGripScriptBase* Script : GripLogicScripts)
	{
		if (Script && !Script->IsPendingKill())
		{
			WroteSomething |= Channel->ReplicateSubobject(Script, *Bunch, *RepFlags);
		}
	}

	return WroteSomething;
}

/*void AGrippableSkeletalMeshActor::GetLifetimeReplicatedProps(TArray< class FLifetimeProperty > & OutLifetimeProps) const
{
	DOREPLIFETIME(AGrippableSkeletalMeshActor, VRGripInterfaceSettings);
}*/

//=============================================================================
AGrippableSkeletalMeshActor::~AGrippableSkeletalMeshActor()
{
}

void AGrippableSkeletalMeshActor::BeginPlay()
{
	// Call the base class 
	Super::BeginPlay();

	// Call all grip scripts begin play events so they can perform any needed logic
	for (UVRGripScriptBase* Script : GripLogicScripts)
	{
		if (Script)
		{
			Script->BeginPlay(this);
		}
	}
}

void AGrippableSkeletalMeshActor::SetDenyGripping(bool bDenyGripping)
{
	VRGripInterfaceSettings.bDenyGripping = bDenyGripping;
}

void AGrippableSkeletalMeshActor::TickGrip_Implementation(UGripMotionControllerComponent * GrippingController, const FBPActorGripInformation & GripInformation, float DeltaTime) {}
void AGrippableSkeletalMeshActor::OnGrip_Implementation(UGripMotionControllerComponent * GrippingController, const FBPActorGripInformation & GripInformation) {}
void AGrippableSkeletalMeshActor::OnGripRelease_Implementation(UGripMotionControllerComponent * ReleasingController, const FBPActorGripInformation & GripInformation, bool bWasSocketed) {}
void AGrippableSkeletalMeshActor::OnChildGrip_Implementation(UGripMotionControllerComponent * GrippingController, const FBPActorGripInformation & GripInformation) {}
void AGrippableSkeletalMeshActor::OnChildGripRelease_Implementation(UGripMotionControllerComponent * ReleasingController, const FBPActorGripInformation & GripInformation, bool bWasSocketed) {}
void AGrippableSkeletalMeshActor::OnSecondaryGrip_Implementation(USceneComponent * SecondaryGripComponent, const FBPActorGripInformation & GripInformation) {}
void AGrippableSkeletalMeshActor::OnSecondaryGripRelease_Implementation(USceneComponent * ReleasingSecondaryGripComponent, const FBPActorGripInformation & GripInformation) {}
void AGrippableSkeletalMeshActor::OnUsed_Implementation() {}
void AGrippableSkeletalMeshActor::OnEndUsed_Implementation() {}
void AGrippableSkeletalMeshActor::OnSecondaryUsed_Implementation() {}
void AGrippableSkeletalMeshActor::OnEndSecondaryUsed_Implementation() {}
void AGrippableSkeletalMeshActor::OnInput_Implementation(FKey Key, EInputEvent KeyEvent) {}
bool AGrippableSkeletalMeshActor::RequestsSocketing_Implementation(USceneComponent *& ParentToSocketTo, FName & OptionalSocketName, FTransform_NetQuantize & RelativeTransform) { return false; }

bool AGrippableSkeletalMeshActor::DenyGripping_Implementation()
{
	return VRGripInterfaceSettings.bDenyGripping;
}


EGripInterfaceTeleportBehavior AGrippableSkeletalMeshActor::TeleportBehavior_Implementation()
{
	return VRGripInterfaceSettings.OnTeleportBehavior;
}

bool AGrippableSkeletalMeshActor::SimulateOnDrop_Implementation()
{
	return VRGripInterfaceSettings.bSimulateOnDrop;
}

EGripCollisionType AGrippableSkeletalMeshActor::GetPrimaryGripType_Implementation(bool bIsSlot)
{
	return bIsSlot ? VRGripInterfaceSettings.SlotDefaultGripType : VRGripInterfaceSettings.FreeDefaultGripType;
}

ESecondaryGripType AGrippableSkeletalMeshActor::SecondaryGripType_Implementation()
{
	return VRGripInterfaceSettings.SecondaryGripType;
}

EGripMovementReplicationSettings AGrippableSkeletalMeshActor::GripMovementReplicationType_Implementation()
{
	return VRGripInterfaceSettings.MovementReplicationType;
}

EGripLateUpdateSettings AGrippableSkeletalMeshActor::GripLateUpdateSetting_Implementation()
{
	return VRGripInterfaceSettings.LateUpdateSetting;
}

void AGrippableSkeletalMeshActor::GetGripStiffnessAndDamping_Implementation(float &GripStiffnessOut, float &GripDampingOut)
{
	GripStiffnessOut = VRGripInterfaceSettings.ConstraintStiffness;
	GripDampingOut = VRGripInterfaceSettings.ConstraintDamping;
}

FBPAdvGripSettings AGrippableSkeletalMeshActor::AdvancedGripSettings_Implementation()
{
	return VRGripInterfaceSettings.AdvancedGripSettings;
}

float AGrippableSkeletalMeshActor::GripBreakDistance_Implementation()
{
	return VRGripInterfaceSettings.ConstraintBreakDistance;
}

void AGrippableSkeletalMeshActor::ClosestGripSlotInRange_Implementation(FVector WorldLocation, bool bSecondarySlot, bool & bHadSlotInRange, FTransform & SlotWorldTransform, UGripMotionControllerComponent * CallingController, FName OverridePrefix)
{
	if (OverridePrefix.IsNone())
		bSecondarySlot ? OverridePrefix = "VRGripS" : OverridePrefix = "VRGripP";

	UVRExpansionFunctionLibrary::GetGripSlotInRangeByTypeName(OverridePrefix, this, WorldLocation, bSecondarySlot ? VRGripInterfaceSettings.SecondarySlotRange : VRGripInterfaceSettings.PrimarySlotRange, bHadSlotInRange, SlotWorldTransform);
}

/*bool AGrippableSkeletalMeshActor::IsInteractible_Implementation()
{
	return VRGripInterfaceSettings.bIsInteractible;
}*/

void AGrippableSkeletalMeshActor::IsHeld_Implementation(UGripMotionControllerComponent *& HoldingController, bool & bIsHeld)
{
	HoldingController = VRGripInterfaceSettings.HoldingController;
	bIsHeld = VRGripInterfaceSettings.bIsHeld;
}

void AGrippableSkeletalMeshActor::SetHeld_Implementation(UGripMotionControllerComponent * HoldingController, bool bIsHeld)
{
	if (bIsHeld)
	{
		VRGripInterfaceSettings.HoldingController = HoldingController;

		if (ClientAuthReplicationData.bIsCurrentlyClientAuth)
		{
			IVRReplicationInterface::RemoveObjectFromReplicationManager(this);
			CeaseReplicationBlocking();
		}
	}
	else
	{
		VRGripInterfaceSettings.HoldingController = nullptr;

		if (ClientAuthReplicationData.bUseClientAuthThrowing && ShouldWeSkipAttachmentReplication())
		{
			if (UPrimitiveComponent * PrimComp = Cast<UPrimitiveComponent>(GetRootComponent()))
			{
				if (PrimComp->IsSimulatingPhysics())
				{
					IVRReplicationInterface::AddObjectToReplicationManager(ClientAuthReplicationData.UpdateRate, this);
					ClientAuthReplicationData.bIsCurrentlyClientAuth = true;

					if (UWorld * World = GetWorld())
						ClientAuthReplicationData.TimeAtInitialThrow = World->GetTimeSeconds();
				}
			}
		}
	}

	VRGripInterfaceSettings.bIsHeld = bIsHeld;
}

/*FBPInteractionSettings AGrippableSkeletalMeshActor::GetInteractionSettings_Implementation()
{
	return VRGripInterfaceSettings.InteractionSettings;
}*/

bool AGrippableSkeletalMeshActor::GetGripScripts_Implementation(TArray<UVRGripScriptBase*> & ArrayReference)
{
	ArrayReference = GripLogicScripts;
	return GripLogicScripts.Num() > 0;
}

bool AGrippableSkeletalMeshActor::PollReplicationEvent(float DeltaTime)
{
	if (!ClientAuthReplicationData.bIsCurrentlyClientAuth)
		return false;

	UWorld *OurWorld = GetWorld();
	if (!OurWorld)
		return false;

	if ((OurWorld->GetTimeSeconds() - ClientAuthReplicationData.TimeAtInitialThrow) > 10.0f)
	{
		// Lets time out sending, its been 10 seconds since we threw the object and its likely that it is conflicting with some server
		// Authed movement that is forcing it to keep momentum.
		return false;
	}

	// Store current transform for resting check
	FTransform CurTransform = this->GetActorTransform();

	if (!CurTransform.GetRotation().Equals(ClientAuthReplicationData.LastActorTransform.GetRotation()) || !CurTransform.GetLocation().Equals(ClientAuthReplicationData.LastActorTransform.GetLocation()))
	{
		ClientAuthReplicationData.LastActorTransform = CurTransform;

		if (UPrimitiveComponent * PrimComp = Cast<UPrimitiveComponent>(RootComponent))
		{
			// Need to clamp to a max time since start, to handle cases with conflicting collisions
			if (PrimComp->IsSimulatingPhysics() && ShouldWeSkipAttachmentReplication())
			{
				FRepMovementVR ClientAuthMovementRep;
				if (ClientAuthMovementRep.GatherActorsMovement(this))
				{
					Server_GetClientAuthRepliction(ClientAuthMovementRep);

					if (PrimComp->RigidBodyIsAwake())
						return true;
				}
			}
		}
		else
		{
			return false;
		}
	}
	else
	{
		// Difference is too small, lets end sending location
		ClientAuthReplicationData.LastActorTransform = FTransform::Identity;
	}

	AActor* TopOwner = GetOwner();

	if (TopOwner != nullptr)
	{
		AActor * tempOwner = TopOwner->GetOwner();

		// I have an owner so search that for the top owner
		while (tempOwner)
		{
			TopOwner = tempOwner;
			tempOwner = TopOwner->GetOwner();
		}

		if (APlayerController* PlayerController = Cast<APlayerController>(TopOwner))
		{
			if (APlayerState* PlayerState = PlayerController->PlayerState)
			{
				if (ClientAuthReplicationData.ResetReplicationHandle.IsValid())
				{
					OurWorld->GetTimerManager().ClearTimer(ClientAuthReplicationData.ResetReplicationHandle);
				}

				// Lets clamp the ping to a min / max value just in case
				float clampedPing = FMath::Clamp(PlayerState->ExactPing, 0.0f, 1000.0f);
				OurWorld->GetTimerManager().SetTimer(ClientAuthReplicationData.ResetReplicationHandle, this, &AGrippableSkeletalMeshActor::CeaseReplicationBlocking, clampedPing, false);
			}
		}
	}

	return false;
}

void AGrippableSkeletalMeshActor::CeaseReplicationBlocking()
{
	ClientAuthReplicationData.bIsCurrentlyClientAuth = false;
	if (UWorld * OurWorld = GetWorld())
	{
		OurWorld->GetTimerManager().ClearTimer(ClientAuthReplicationData.ResetReplicationHandle);
	}
}

void AGrippableSkeletalMeshActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CeaseReplicationBlocking();

	// Call all grip scripts begin play events so they can perform any needed logic
	for (UVRGripScriptBase* Script : GripLogicScripts)
	{
		if (Script)
		{
			Script->EndPlay(EndPlayReason);
		}
	}

	Super::EndPlay(EndPlayReason);
}


bool AGrippableSkeletalMeshActor::Server_GetClientAuthRepliction_Validate(const FRepMovementVR & newMovement)
{
	return true;
}

void AGrippableSkeletalMeshActor::Server_GetClientAuthRepliction_Implementation(const FRepMovementVR & newMovement)
{
	newMovement.CopyTo(ReplicatedMovement);
	OnRep_ReplicatedMovement();
}

void AGrippableSkeletalMeshActor::OnRep_AttachmentReplication()
{
	if (bAllowIgnoringAttachOnOwner && ShouldWeSkipAttachmentReplication())
	{
		return;
	}

	// None of our overrides are required, lets just pass it on now
	Super::OnRep_AttachmentReplication();
}

void AGrippableSkeletalMeshActor::OnRep_ReplicateMovement()
{
	if (bAllowIgnoringAttachOnOwner && ShouldWeSkipAttachmentReplication())
	{
		return;
	}

	if (RootComponent)
	{
		const FRepAttachment ReplicationAttachment = GetAttachmentReplication();
		if (!ReplicationAttachment.AttachParent)
		{
			// This "fix" corrects the simulation state not replicating over correctly
			// If you turn off movement replication, simulate an object, turn movement replication back on and un-simulate, it never knows the difference
			// This change ensures that it is checking against the current state
			if (RootComponent->IsSimulatingPhysics() != ReplicatedMovement.bRepPhysics)//SavedbRepPhysics != ReplicatedMovement.bRepPhysics)
			{
				// Turn on/off physics sim to match server.
				SyncReplicatedPhysicsSimulation();

				// It doesn't really hurt to run it here, the super can call it again but it will fail out as they already match
			}
		}
	}

	Super::OnRep_ReplicateMovement();
}

void AGrippableSkeletalMeshActor::OnRep_ReplicatedMovement()
{
	if (ClientAuthReplicationData.bIsCurrentlyClientAuth && ShouldWeSkipAttachmentReplication())
	{
		return;
	}

	Super::OnRep_ReplicatedMovement();
}

void AGrippableSkeletalMeshActor::PostNetReceivePhysicState()
{
	if (VRGripInterfaceSettings.bIsHeld && bAllowIgnoringAttachOnOwner && ShouldWeSkipAttachmentReplication())
	{
		return;
	}

	Super::PostNetReceivePhysicState();
}

void AGrippableSkeletalMeshActor::MarkComponentsAsPendingKill()
{
	Super::MarkComponentsAsPendingKill();

	for (int32 i = 0; i < GripLogicScripts.Num(); ++i)
	{
		if (UObject *SubObject = GripLogicScripts[i])
		{
			SubObject->MarkPendingKill();
		}
	}

	GripLogicScripts.Empty();
}

void AGrippableSkeletalMeshActor::PreDestroyFromReplication()
{
	Super::PreDestroyFromReplication();

	// Destroy any sub-objects we created
	for (int32 i = 0; i < GripLogicScripts.Num(); ++i)
	{
		if (UObject *SubObject = GripLogicScripts[i])
		{
			OnSubobjectDestroyFromReplication(SubObject); //-V595
			SubObject->PreDestroyFromReplication();
			SubObject->MarkPendingKill();
		}
	}

	for (UActorComponent * ActorComp : GetComponents())
	{
		// Pending kill components should have already had this called as they were network spawned and are being killed
		if (ActorComp && !ActorComp->IsPendingKill() && ActorComp->GetClass()->ImplementsInterface(UVRGripInterface::StaticClass()))
			ActorComp->PreDestroyFromReplication();
	}

	GripLogicScripts.Empty();
}

void AGrippableSkeletalMeshActor::BeginDestroy()
{
	Super::BeginDestroy();

	for (int32 i = 0; i < GripLogicScripts.Num(); i++)
	{
		if (UObject *SubObject = GripLogicScripts[i])
		{
			SubObject->MarkPendingKill();
		}
	}

	GripLogicScripts.Empty();
}