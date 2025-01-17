// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Interactibles/VRDialComponent.h"
#include "Net/UnrealNetwork.h"

  //=============================================================================
UVRDialComponent::UVRDialComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	this->SetGenerateOverlapEvents(true);
	this->PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.bCanEverTick = true;

	bRepGameplayTags = false;

	// Defaulting these true so that they work by default in networked environments
	bReplicateMovement = true;

	DialRotationAxis = EVRInteractibleAxis::Axis_Z;
	InteractorRotationAxis = EVRInteractibleAxis::Axis_X;

	bDialUsesAngleSnap = false;
	SnapAngleThreshold = 0.0f;
	SnapAngleIncrement = 45.0f;
	LastSnapAngle = 0.0f;
	RotationScaler = 1.0f;

	ClockwiseMaximumDialAngle = 180.0f;
	CClockwiseMaximumDialAngle = 180.0f;
	bDenyGripping = false;

	GripPriority = 1;

	MovementReplicationSetting = EGripMovementReplicationSettings::ForceClientSideMovement;
	BreakDistance = 100.0f;

	bLerpBackOnRelease = false;
	bSendDialEventsDuringLerp = false;
	DialReturnSpeed = 90.0f;
	bIsLerping = false;

	bDialUseDirectHandRotation = false;
	LastGripRot = 0.0f;
}

//=============================================================================
UVRDialComponent::~UVRDialComponent()
{
}


void UVRDialComponent::GetLifetimeReplicatedProps(TArray< class FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UVRDialComponent, bRepGameplayTags);
	DOREPLIFETIME(UVRDialComponent, bReplicateMovement);
	DOREPLIFETIME_CONDITION(UVRDialComponent, GameplayTags, COND_Custom);
}

void UVRDialComponent::PreReplication(IRepChangedPropertyTracker & ChangedPropertyTracker)
{
	Super::PreReplication(ChangedPropertyTracker);

	// Don't replicate if set to not do it
	DOREPLIFETIME_ACTIVE_OVERRIDE(UVRDialComponent, GameplayTags, bRepGameplayTags);

	DOREPLIFETIME_ACTIVE_OVERRIDE(USceneComponent, RelativeLocation, bReplicateMovement);
	DOREPLIFETIME_ACTIVE_OVERRIDE(USceneComponent, RelativeRotation, bReplicateMovement);
	DOREPLIFETIME_ACTIVE_OVERRIDE(USceneComponent, RelativeScale3D, bReplicateMovement);
}

void UVRDialComponent::OnRegister()
{
	Super::OnRegister();
	ResetInitialDialLocation(); // Load the original dial location
}

void UVRDialComponent::BeginPlay()
{
	// Call the base class 
	Super::BeginPlay();

	bOriginalReplicatesMovement = bReplicateMovement;
}

void UVRDialComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	if (bIsLerping)
	{
		// Flip lerp direction if we are on the other side
		if (CurrentDialAngle > ClockwiseMaximumDialAngle)
			this->SetDialAngle(FMath::FInterpConstantTo(CurRotBackEnd, 360.f, DeltaTime, DialReturnSpeed), bSendDialEventsDuringLerp);
		else
			this->SetDialAngle(FMath::FInterpConstantTo(CurRotBackEnd, 0.f, DeltaTime, DialReturnSpeed), bSendDialEventsDuringLerp);

		if (CurRotBackEnd == 0.f)
		{
			this->SetComponentTickEnabled(false);
			bIsLerping = false;
			OnDialFinishedLerping.Broadcast();
			ReceiveDialFinishedLerping();
		}
	}
	else
	{
		this->SetComponentTickEnabled(false); 
	}
}

void UVRDialComponent::TickGrip_Implementation(UGripMotionControllerComponent * GrippingController, const FBPActorGripInformation & GripInformation, float DeltaTime) 
{

	// #TODO: Should this use a pivot rotation? it wouldn't make that much sense to me?
	float DeltaRot = 0.0f;

	if (!bDialUseDirectHandRotation)
	{
		FTransform CurrentRelativeTransform = InitialRelativeTransform * UVRInteractibleFunctionLibrary::Interactible_GetCurrentParentTransform(this);
		FVector CurInteractorLocation = CurrentRelativeTransform.InverseTransformPosition(GrippingController->GetPivotLocation());
		
		float NewRot = FRotator::ClampAxis(UVRInteractibleFunctionLibrary::GetAtan2Angle(DialRotationAxis, CurInteractorLocation));
		DeltaRot = RotationScaler * (NewRot - LastGripRot);
		LastGripRot = NewRot;
	}
	else
	{
		FRotator curRotation = GrippingController->GetComponentRotation();
		DeltaRot = RotationScaler * UVRInteractibleFunctionLibrary::GetAxisValue(InteractorRotationAxis, (curRotation - LastRotation).GetNormalized());
		LastRotation = curRotation;
	}

	AddDialAngle(DeltaRot, true);

	// Handle the auto drop
	if (BreakDistance > 0.f && GrippingController->HasGripAuthority(GripInformation) && FVector::DistSquared(InitialDropLocation, this->GetComponentTransform().InverseTransformPosition(GrippingController->GetPivotLocation())) >= FMath::Square(BreakDistance))
	{
		GrippingController->DropObjectByInterface(this);
		return;
	}
}

void UVRDialComponent::OnGrip_Implementation(UGripMotionControllerComponent * GrippingController, const FBPActorGripInformation & GripInformation) 
{
	FTransform CurrentRelativeTransform = InitialRelativeTransform * UVRInteractibleFunctionLibrary::Interactible_GetCurrentParentTransform(this);

	// This lets me use the correct original location over the network without changes
	FTransform ReversedRelativeTransform = FTransform(GripInformation.RelativeTransform.ToInverseMatrixWithScale());
	FTransform CurrentTransform = this->GetComponentTransform();
	FTransform RelativeToGripTransform = ReversedRelativeTransform * CurrentTransform;

	//FTransform InitialTrans = RelativeToGripTransform.GetRelativeTransform(CurrentRelativeTransform);

	InitialInteractorLocation = CurrentRelativeTransform.InverseTransformPosition(RelativeToGripTransform.GetTranslation());
	InitialDropLocation = ReversedRelativeTransform.GetTranslation();

	if (!bDialUseDirectHandRotation)
	{
		LastGripRot = FRotator::ClampAxis(UVRInteractibleFunctionLibrary::GetAtan2Angle(DialRotationAxis, InitialInteractorLocation));
	}
	else
	{
		LastRotation = RelativeToGripTransform.GetRotation().Rotator(); // Forcing into world space now so that initial can be correct over the network
	}

	bIsLerping = false;
}

void UVRDialComponent::OnGripRelease_Implementation(UGripMotionControllerComponent * ReleasingController, const FBPActorGripInformation & GripInformation, bool bWasSocketed) 
{
	if (bDialUsesAngleSnap && FMath::Abs(FMath::Fmod(CurRotBackEnd, SnapAngleIncrement)) <= FMath::Min(SnapAngleIncrement, SnapAngleThreshold))
	{
		this->SetRelativeRotation((FTransform(UVRInteractibleFunctionLibrary::SetAxisValueRot(DialRotationAxis, FMath::GridSnap(CurRotBackEnd, SnapAngleIncrement), FRotator::ZeroRotator)) * InitialRelativeTransform).Rotator());		
		CurRotBackEnd = FMath::GridSnap(CurRotBackEnd, SnapAngleIncrement);
		CurrentDialAngle = FRotator::ClampAxis(FMath::RoundToFloat(CurRotBackEnd));
	}

	if (bLerpBackOnRelease)
	{
		bIsLerping = true;
		this->SetComponentTickEnabled(true);
	}
	else
		this->SetComponentTickEnabled(false);
}

void UVRDialComponent::OnChildGrip_Implementation(UGripMotionControllerComponent * GrippingController, const FBPActorGripInformation & GripInformation) {}
void UVRDialComponent::OnChildGripRelease_Implementation(UGripMotionControllerComponent * ReleasingController, const FBPActorGripInformation & GripInformation, bool bWasSocketed) {}
void UVRDialComponent::OnSecondaryGrip_Implementation(USceneComponent * SecondaryGripComponent, const FBPActorGripInformation & GripInformation) {}
void UVRDialComponent::OnSecondaryGripRelease_Implementation(USceneComponent * ReleasingSecondaryGripComponent, const FBPActorGripInformation & GripInformation) {}
void UVRDialComponent::OnUsed_Implementation() {}
void UVRDialComponent::OnEndUsed_Implementation() {}
void UVRDialComponent::OnSecondaryUsed_Implementation() {}
void UVRDialComponent::OnEndSecondaryUsed_Implementation() {}
void UVRDialComponent::OnInput_Implementation(FKey Key, EInputEvent KeyEvent) {}
bool UVRDialComponent::RequestsSocketing_Implementation(USceneComponent *& ParentToSocketTo, FName & OptionalSocketName, FTransform_NetQuantize & RelativeTransform) { return false; }

bool UVRDialComponent::DenyGripping_Implementation()
{
	return bDenyGripping;
}

EGripInterfaceTeleportBehavior UVRDialComponent::TeleportBehavior_Implementation()
{
	return EGripInterfaceTeleportBehavior::DropOnTeleport;
}

bool UVRDialComponent::SimulateOnDrop_Implementation()
{
	return false;
}

/*EGripCollisionType UVRDialComponent::SlotGripType_Implementation()
{
	return EGripCollisionType::CustomGrip;
}

EGripCollisionType UVRDialComponent::FreeGripType_Implementation()
{
	return EGripCollisionType::CustomGrip;
}*/

EGripCollisionType UVRDialComponent::GetPrimaryGripType_Implementation(bool bIsSlot)
{
	return EGripCollisionType::CustomGrip;
}

ESecondaryGripType UVRDialComponent::SecondaryGripType_Implementation()
{
	return ESecondaryGripType::SG_None;
}


EGripMovementReplicationSettings UVRDialComponent::GripMovementReplicationType_Implementation()
{
	return MovementReplicationSetting;
}

EGripLateUpdateSettings UVRDialComponent::GripLateUpdateSetting_Implementation()
{
	return EGripLateUpdateSettings::LateUpdatesAlwaysOff;
}

/*float UVRDialComponent::GripStiffness_Implementation()
{
	return 1500.0f;
}

float UVRDialComponent::GripDamping_Implementation()
{
	return 200.0f;
}*/

void UVRDialComponent::GetGripStiffnessAndDamping_Implementation(float &GripStiffnessOut, float &GripDampingOut)
{
	GripStiffnessOut = 0.0f;
	GripDampingOut = 0.0f;
}

FBPAdvGripSettings UVRDialComponent::AdvancedGripSettings_Implementation()
{
	return FBPAdvGripSettings(GripPriority);
}

float UVRDialComponent::GripBreakDistance_Implementation()
{
	return BreakDistance;
}

/*void UVRDialComponent::ClosestSecondarySlotInRange_Implementation(FVector WorldLocation, bool & bHadSlotInRange, FTransform & SlotWorldTransform, UGripMotionControllerComponent * CallingController, FName OverridePrefix)
{
	bHadSlotInRange = false;
}

void UVRDialComponent::ClosestPrimarySlotInRange_Implementation(FVector WorldLocation, bool & bHadSlotInRange, FTransform & SlotWorldTransform, UGripMotionControllerComponent * CallingController, FName OverridePrefix)
{
	bHadSlotInRange = false;
}*/

void UVRDialComponent::ClosestGripSlotInRange_Implementation(FVector WorldLocation, bool bSecondarySlot, bool & bHadSlotInRange, FTransform & SlotWorldTransform, UGripMotionControllerComponent * CallingController, FName OverridePrefix)
{
	bHadSlotInRange = false;
}

/*bool UVRDialComponent::IsInteractible_Implementation()
{
	return false;
}*/

void UVRDialComponent::IsHeld_Implementation(UGripMotionControllerComponent *& CurHoldingController, bool & bCurIsHeld)
{
	CurHoldingController = HoldingController;
	bCurIsHeld = bIsHeld;
}

void UVRDialComponent::SetHeld_Implementation(UGripMotionControllerComponent * NewHoldingController, bool bNewIsHeld)
{
	if (bNewIsHeld)
	{
		HoldingController = NewHoldingController;
		if (MovementReplicationSetting != EGripMovementReplicationSettings::ForceServerSideMovement)
		{
			if(!bIsHeld)
				bOriginalReplicatesMovement = bReplicateMovement;
			bReplicateMovement = false;
		}
	}
	else
	{
		HoldingController = nullptr;
		if (MovementReplicationSetting != EGripMovementReplicationSettings::ForceServerSideMovement)
		{
			bReplicateMovement = bOriginalReplicatesMovement;
		}
	}

	bIsHeld = bNewIsHeld;
}

/*FBPInteractionSettings UVRDialComponent::GetInteractionSettings_Implementation()
{
	return FBPInteractionSettings();
}*/

bool UVRDialComponent::GetGripScripts_Implementation(TArray<UVRGripScriptBase*> & ArrayReference)
{
	return false;
}

void UVRDialComponent::SetDialAngle(float DialAngle, bool bCallEvents)
{
	CurRotBackEnd = DialAngle;
	AddDialAngle(0.0f);
}

void UVRDialComponent::AddDialAngle(float DialAngleDelta, bool bCallEvents)
{
	float MaxCheckValue = 360.0f - CClockwiseMaximumDialAngle;

	float DeltaRot = DialAngleDelta;
	float tempCheck = FRotator::ClampAxis(CurRotBackEnd + DeltaRot);

	// Clamp it to the boundaries
	if (FMath::IsNearlyZero(CClockwiseMaximumDialAngle))
	{
		CurRotBackEnd = FMath::Clamp(CurRotBackEnd + DeltaRot, 0.0f, ClockwiseMaximumDialAngle);
	}
	else if (FMath::IsNearlyZero(ClockwiseMaximumDialAngle))
	{
		if (CurRotBackEnd < MaxCheckValue)
			CurRotBackEnd = FMath::Clamp(360.0f + DeltaRot, MaxCheckValue, 360.0f);
		else
			CurRotBackEnd = FMath::Clamp(CurRotBackEnd + DeltaRot, MaxCheckValue, 360.0f);
	}
	else if (tempCheck > ClockwiseMaximumDialAngle && tempCheck < MaxCheckValue)
	{
		if (CurRotBackEnd < MaxCheckValue)
		{
			CurRotBackEnd = ClockwiseMaximumDialAngle;
		}
		else
		{
			CurRotBackEnd = MaxCheckValue;
		}
	}
	else
		CurRotBackEnd = tempCheck;

	if (bDialUsesAngleSnap && FMath::Abs(FMath::Fmod(CurRotBackEnd, SnapAngleIncrement)) <= FMath::Min(SnapAngleIncrement, SnapAngleThreshold))
	{
		this->SetRelativeRotation((FTransform(UVRInteractibleFunctionLibrary::SetAxisValueRot(DialRotationAxis, FMath::GridSnap(CurRotBackEnd, SnapAngleIncrement), FRotator::ZeroRotator)) * InitialRelativeTransform).Rotator());
		CurrentDialAngle = FMath::RoundToFloat(FMath::GridSnap(CurRotBackEnd, SnapAngleIncrement));

		if (bCallEvents && !FMath::IsNearlyEqual(LastSnapAngle, CurrentDialAngle))
		{
			ReceiveDialHitSnapAngle(CurrentDialAngle);
			OnDialHitSnapAngle.Broadcast(CurrentDialAngle);
		}

		LastSnapAngle = CurrentDialAngle;
	}
	else
	{
		this->SetRelativeRotation((FTransform(UVRInteractibleFunctionLibrary::SetAxisValueRot(DialRotationAxis, CurRotBackEnd, FRotator::ZeroRotator)) * InitialRelativeTransform).Rotator());
		CurrentDialAngle = FMath::RoundToFloat(CurRotBackEnd);
	}

}

void UVRDialComponent::ResetInitialDialLocation()
{
	// Get our initial relative transform to our parent (or not if un-parented).
	InitialRelativeTransform = this->GetRelativeTransform();
	CurRotBackEnd = 0.0f;
}