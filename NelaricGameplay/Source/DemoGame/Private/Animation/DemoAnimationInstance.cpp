// Copyright (c) 2026 Nelaric Contributors

#include "Animation/DemoAnimationInstance.h"

#include "AbilitySystemComponent.h"
#include "Character/DemoCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Equipment/DemoEquipmentDefinition.h"
#include "Equipment/DemoEquipmentInstance.h"
#include "Equipment/DemoEquipmentManagerComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Math/RotationMatrix.h"

namespace Nelaric::UnitAnimation
{
static EDemoCardinalDirection SelectCardinalDirection(float Angle, float DeadZone, EDemoCardinalDirection Previous,
                                                      bool bUsePrevious)
{
	const float ForwardZone = bUsePrevious && Previous == EDemoCardinalDirection::Forward ? DeadZone * 2.0f : DeadZone;
	const float BackwardZone =
	    bUsePrevious && Previous == EDemoCardinalDirection::Backward ? DeadZone * 2.0f : DeadZone;
	const float AbsoluteAngle = FMath::Abs(FRotator::NormalizeAxis(Angle));
	if (AbsoluteAngle <= 45.0f + ForwardZone)
	{
		return EDemoCardinalDirection::Forward;
	}
	if (AbsoluteAngle >= 135.0f - BackwardZone)
	{
		return EDemoCardinalDirection::Backward;
	}
	return FRotator::NormalizeAxis(Angle) > 0.0f ? EDemoCardinalDirection::Right : EDemoCardinalDirection::Left;
}

} // namespace Nelaric::UnitAnimation

UDemoAnimationInstance::UDemoAnimationInstance()
    : HipFireOverridePoseCurveName(TEXT("applyHipfireOverridePose")),
      DisableLeftHandPoseOverrideCurveName(TEXT("DisableLeftHandPoseOverride"))
{
}

bool UDemoAnimationInstance::ShouldEnableControlRig() const
{
	return !bIsInAir;
}

void UDemoAnimationInstance::SetAnimationContext(const Nelaric::UnitAnimation::FLocomotionAnimationContext& Context)
{
	check(IsInGameThread());
	if (FMath::IsFinite(Context.RootYawOffset))
	{
		AnimationContext = Context;
	}
}

void UDemoAnimationInstance::PrepareAnimationData(const ADemoCharacter& Character)
{
	check(IsInGameThread());
	const UCharacterMovementComponent* Movement = Character.GetCharacterMovement();
	Input.bMontagePlaying = IsAnyMontagePlaying();
	Input.Velocity = Character.GetVelocity();
	Input.Acceleration = Movement ? Movement->GetCurrentAcceleration() : FVector::ZeroVector;
	Input.Rotation = Character.GetActorRotation();
	Input.GravityZ = Movement ? Movement->GetGravityZ() : 0.0f;
	Input.bGrounded = Movement && Movement->IsMovingOnGround();
	Input.bInAir = Movement && Movement->IsFalling();
	if (Input.Velocity.ContainsNaN() || Input.Acceleration.ContainsNaN() || Input.Rotation.ContainsNaN() ||
	    !FMath::IsFinite(Input.GravityZ))
	{
		Input.Velocity = FVector::ZeroVector;
		Input.Acceleration = FVector::ZeroVector;
		Input.Rotation = FRotator::ZeroRotator;
		Input.GravityZ = 0.0f;
		Input.bGrounded = false;
		Input.bInAir = false;
	}
	Input.AimRotation = Character.GetBaseAimRotation();
	if (Input.AimRotation.ContainsNaN())
	{
		Input.AimRotation = Input.Rotation;
	}
	Input.GroundDistance = CaptureGroundDistance(Character);
	CaptureAnimationInputs(Character);
	CaptureWeaponInputs(Character);
}

void UDemoAnimationInstance::CaptureGameplayState(const ADemoCharacter& Character)
{
	Input.Context = AnimationContext;
	if (const UAbilitySystemComponent* ASC = Character.GetAbilitySystemComponent())
	{
		if (ADSTag.IsValid())
		{
			Input.Context.bAimingDownSights = ASC->HasMatchingGameplayTag(ADSTag);
		}
		if (MeleeTag.IsValid())
		{
			Input.Context.bMelee = ASC->HasMatchingGameplayTag(MeleeTag);
		}
	}
	else
	{
		Input.Context.bAimingDownSights &= !ADSTag.IsValid();
		Input.Context.bMelee &= !MeleeTag.IsValid();
	}
	Input.bCrouching = Character.bIsCrouched;
	const bool bCrouching = Input.bCrouching;
	const bool bADS = Input.Context.bAimingDownSights;
	if (bHasGameplaySample)
	{
		CrouchChangeSerial += bWasCrouching != bCrouching;
		ADSChangeSerial += bWasADS != bADS;
	}
	bWasCrouching = bCrouching;
	bWasADS = bADS;
	bHasGameplaySample = true;
	Input.CrouchChangeSerial = CrouchChangeSerial;
	Input.ADSChangeSerial = ADSChangeSerial;
}

float UDemoAnimationInstance::CaptureGroundDistance(const ADemoCharacter& Character) const
{
	const UCharacterMovementComponent* Movement = Character.GetCharacterMovement();
	if (!Movement)
	{
		return -1.0f;
	}
	if (Movement->IsMovingOnGround())
	{
		return 0.0f;
	}
	const UCapsuleComponent* Capsule = Character.GetCapsuleComponent();
	UWorld* World = Character.GetWorld();
	const FVector Origin = Character.GetActorLocation();
	if (!Capsule || !World || Origin.ContainsNaN())
	{
		return -1.0f;
	}
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FVector End = Origin - FVector(0.0, 0.0, GroundTraceDistance + HalfHeight);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(DemoAnimationGroundDistance), false, &Character);
	FCollisionResponseParams Response;
	Movement->InitCollisionParams(Query, Response);
	const ECollisionChannel Channel =
	    Movement->UpdatedComponent ? Movement->UpdatedComponent->GetCollisionObjectType() : ECC_Pawn;
	FHitResult Hit;
	World->LineTraceSingleByChannel(Hit, Origin, End, Channel, Query, Response);
	return Hit.bBlockingHit ? FMath::Max(0.0f, Hit.Distance - HalfHeight) : GroundTraceDistance;
}

void UDemoAnimationInstance::CaptureAnimationInputs(const ADemoCharacter& Character)
{
	using namespace Nelaric::UnitAnimation;
	check(IsInGameThread());
	CaptureGameplayState(Character);
	Input.LayerChangeSerial = GetAnimationLayerChangeSerial();

	const FVector Velocity = Character.GetVelocity();
	const FVector LocalVelocity = Character.GetActorRotation().UnrotateVector(Velocity);
	if (!LocalVelocity.ContainsNaN())
	{
		const float Angle = FMath::RadiansToDegrees(
		    FMath::Atan2(static_cast<float>(LocalVelocity.Y), static_cast<float>(LocalVelocity.X)));
		LastSampledDirection = SelectCardinalDirection(Angle - Input.Context.RootYawOffset, CardinalDirectionDeadZone,
		                                               LastSampledDirection, bWasMovingAtSample);
		bWasMovingAtSample = Velocity.SizeSquared2D() > VelocitySquaredTolerance;
	}
	Input.VelocityDirection = LastSampledDirection;
}

void UDemoAnimationInstance::CaptureWeaponInputs(const ADemoCharacter& Character)
{
	check(IsInGameThread());
	Input.ActiveWeaponId.Invalidate();
	Input.TimeSinceFiredWeapon = -1.0f;
	Input.HipFireOverridePoseCurveValue = GetCurveValue(HipFireOverridePoseCurveName);
	Input.DisableLeftHandPoseOverrideCurveValue = GetCurveValue(DisableLeftHandPoseOverrideCurveName);
	Input.bEnableLeftHandPoseOverride = bEnableLeftHandPoseOverride;
	const UDemoEquipmentManagerComponent* Manager = Character.FindComponentByClass<UDemoEquipmentManagerComponent>();
	const UDemoWeaponInstance* Weapon = Manager ? Manager->GetActiveWeapon() : nullptr;
	if (IsValid(Weapon))
	{
		Input.ActiveWeaponId = Weapon->GetEquipmentId();
		Input.TimeSinceFiredWeapon = Weapon->GetTimeSinceFiredWeapon();
		const UDemoWeaponDefinition* Definition = Weapon->GetWeaponDefinition();
		const UAnimInstance* Layer = Definition && Definition->ActiveAnimationLayer
		                                 ? GetLinkedAnimLayerInstanceByClass(Definition->ActiveAnimationLayer)
		                                 : nullptr;
		if (Layer)
		{
			float CurveValue = 0.0f;
			if (Layer->GetCurveValue(HipFireOverridePoseCurveName, CurveValue))
			{
				Input.HipFireOverridePoseCurveValue = CurveValue;
			}
			if (Layer->GetCurveValue(DisableLeftHandPoseOverrideCurveName, CurveValue))
			{
				Input.DisableLeftHandPoseOverrideCurveValue = CurveValue;
			}
		}
	}
	if (!FMath::IsFinite(Input.HipFireOverridePoseCurveValue))
	{
		Input.HipFireOverridePoseCurveValue = 0.0f;
	}
	if (!FMath::IsFinite(Input.DisableLeftHandPoseOverrideCurveValue))
	{
		Input.DisableLeftHandPoseOverrideCurveValue = 0.0f;
	}
}

void UDemoAnimationInstance::UpdateBlendWeightData(float DeltaSeconds)
{
	LeftHandPoseOverrideWeight = FMath::Clamp(
	    (Input.bEnableLeftHandPoseOverride ? 1.0f : 0.0f) - Input.DisableLeftHandPoseOverrideCurveValue, 0.0f, 1.0f);
	UpperbodyDynamicAdditiveWeight = Input.bMontagePlaying && bIsOnGround
	                                     ? 1.0f
	                                     : FMath::FInterpTo(UpperbodyDynamicAdditiveWeight, 0.0f, DeltaSeconds,
	                                                        UpperbodyDynamicAdditiveBlendOutInterpRate);
	if (LastCalculatedWeaponId != Input.ActiveWeaponId || bAnimationLayerChanged ||
	    (TimeSinceFiredWeapon >= 0.0f && Input.TimeSinceFiredWeapon < 0.0f))
	{
		HipFireUpperBodyOverrideWeight = 0.0f;
		AimOffsetBlendWeight = 1.0f;
	}
	LastCalculatedWeaponId = Input.ActiveWeaponId;
	TimeSinceFiredWeapon = Input.TimeSinceFiredWeapon;

	const bool bDisableHipFireOverride =
	    (!bRaiseWeaponAfterFiringWhenCrouched && bIsCrouching) || (!bIsCrouching && bGameplayTagIsADS && bIsOnGround);
	if (bDisableHipFireOverride)
	{
		HipFireUpperBodyOverrideWeight = 0.0f;
		AimOffsetBlendWeight = 1.0f;
		return;
	}

	const bool bRecentlyFired = TimeSinceFiredWeapon >= 0.0f && TimeSinceFiredWeapon < RaiseWeaponAfterFiringDuration;
	if (bRecentlyFired || (bGameplayTagIsADS && (bIsCrouching || !bIsOnGround)) ||
	    Input.HipFireOverridePoseCurveValue > 0.0f)
	{
		constexpr float RaiseBlendDuration = 0.1f;
		constexpr float RaiseBlendSpeed = 1.0f / RaiseBlendDuration;
		HipFireUpperBodyOverrideWeight =
		    FMath::FInterpConstantTo(HipFireUpperBodyOverrideWeight, 1.0f, DeltaSeconds, RaiseBlendSpeed);
		AimOffsetBlendWeight = FMath::FInterpConstantTo(AimOffsetBlendWeight, 1.0f, DeltaSeconds, RaiseBlendSpeed);
		return;
	}

	HipFireUpperBodyOverrideWeight =
	    FMath::FInterpTo(HipFireUpperBodyOverrideWeight, 0.0f, DeltaSeconds, HipFireBlendOutInterpRate);
	const float AimOffsetTarget = FMath::Abs(RootYawOffset) < AimOffsetRootYawThreshold && bHasAcceleration
	                                  ? HipFireUpperBodyOverrideWeight
	                                  : 1.0f;
	AimOffsetBlendWeight =
	    FMath::FInterpTo(AimOffsetBlendWeight, AimOffsetTarget, DeltaSeconds, AimOffsetBlendInterpRate);
}

void UDemoAnimationInstance::UpdateAnimationData(float DeltaSeconds)
{
	const float ActualSpeed = static_cast<float>(Input.Velocity.Size2D());
	VerticalSpeed = static_cast<float>(Input.Velocity.Z);
	AccelerationAmount = static_cast<float>(Input.Acceleration.Size2D());
	GroundSpeed = FMath::FInterpTo(GroundSpeed, ActualSpeed, DeltaSeconds, SpeedInterpRate);
	LocalVelocity2D = Input.Rotation.UnrotateVector(Input.Velocity);
	LocalVelocity2D.Z = 0.0;
	LocalAcceleration2D = Input.Rotation.UnrotateVector(Input.Acceleration);
	LocalAcceleration2D.Z = 0.0;
	LocalVelocityDirection = Input.VelocityDirection;
	GroundDistance = Input.GroundDistance;
	RootYawOffset = Input.Context.RootYawOffset;
	const FRotator RelativeAimRotation = (Input.AimRotation - Input.Rotation).GetNormalized();
	AimYaw = static_cast<float>(FRotator::NormalizeAxis(RelativeAimRotation.Yaw - RootYawOffset));
	AimPitch = static_cast<float>(RelativeAimRotation.Pitch);
	bIsMoving = ActualSpeed > MovingSpeedThreshold;
	bHasVelocity = Input.Velocity.SizeSquared2D() > VelocitySquaredTolerance;
	bHasAcceleration = AccelerationAmount > AccelerationThreshold;
	bIsOnGround = Input.bGrounded;
	bIsInAir = Input.bInAir;
	bIsJumping = bIsInAir && VerticalSpeed > 0.0f;
	bIsFalling = bIsInAir && !bIsJumping;
	bAnimationLayerChanged = Input.LayerChangeSerial != LastCalculatedLayerSerial;
	bCrouchStateChange = Input.CrouchChangeSerial != LastCalculatedCrouchSerial;
	bADSStateChanged = Input.ADSChangeSerial != LastCalculatedADSSerial;
	bGameplayTagIsMelee = Input.Context.bMelee;
	bIsCrouching = Input.bCrouching;
	bGameplayTagIsADS = Input.Context.bAimingDownSights;

	const FVector VelocityDirection = Input.Velocity.GetSafeNormal2D();
	const FRotationMatrix ActorAxes(FRotator(0.0f, Input.Rotation.Yaw, 0.0f));
	Direction = bIsMoving
	                ? FMath::RadiansToDegrees(FMath::Atan2(
	                      static_cast<float>(FVector::DotProduct(VelocityDirection, ActorAxes.GetUnitAxis(EAxis::Y))),
	                      static_cast<float>(FVector::DotProduct(VelocityDirection, ActorAxes.GetUnitAxis(EAxis::X)))))
	                : 0.0f;
	TimeToJumpApex = bIsJumping && Input.GravityZ < -UE_SMALL_NUMBER ? VerticalSpeed / -Input.GravityZ : 0.0f;

	bJumpSelectorToJumpStart = bIsJumping;
	bJumpSelectorToJumpApex = bIsFalling;
	bJumpStartLoopToJumpApex = TimeToJumpApex < JumpApexLeadSeconds;
	bFallLoopToFallLand = GroundDistance >= 0.0f && GroundDistance < LandingGroundDistance;
	bFallLandToEndInAir = bIsOnGround;
	bJumpFallInterruptSourcesToEndInAir = bIsOnGround;

	UpdateBlendWeightData(DeltaSeconds);
	LastCalculatedLayerSerial = Input.LayerChangeSerial;
	LastCalculatedCrouchSerial = Input.CrouchChangeSerial;
	LastCalculatedADSSerial = Input.ADSChangeSerial;
}

void UDemoAnimationInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	Input = {};
	AnimationContext = {};
	LastCalculatedWeaponId.Invalidate();
	LastCalculatedLayerSerial = GetAnimationLayerChangeSerial();
	CrouchChangeSerial = 0;
	ADSChangeSerial = 0;
	LastCalculatedCrouchSerial = 0;
	LastCalculatedADSSerial = 0;
	LastSampledDirection = EDemoCardinalDirection::Forward;
	bHasGameplaySample = false;
	bWasCrouching = false;
	bWasADS = false;
	bWasMovingAtSample = false;
	ResetCalculatedData();
}

void UDemoAnimationInstance::NativePostEvaluateAnimation()
{
	Super::NativePostEvaluateAnimation();
	check(IsInGameThread());
	if (const ADemoCharacter* Character = Cast<ADemoCharacter>(TryGetPawnOwner()))
	{
		CaptureAnimationInputs(*Character);
	}
}

void UDemoAnimationInstance::ResetCalculatedData()
{
	GroundSpeed = 0.0f;
	VerticalSpeed = 0.0f;
	Direction = 0.0f;
	AccelerationAmount = 0.0f;
	TimeToJumpApex = 0.0f;
	LocalVelocity2D = FVector::ZeroVector;
	LocalAcceleration2D = FVector::ZeroVector;
	LocalVelocityDirection = EDemoCardinalDirection::Forward;
	GroundDistance = -1.0f;
	RootYawOffset = 0.0f;
	AimYaw = 0.0f;
	AimPitch = 0.0f;
	UpperbodyDynamicAdditiveWeight = 0.0f;
	LeftHandPoseOverrideWeight = 0.0f;
	HipFireUpperBodyOverrideWeight = 0.0f;
	AimOffsetBlendWeight = 1.0f;
	TimeSinceFiredWeapon = -1.0f;
	bIsCrouching = false;
	bGameplayTagIsADS = false;
	bHasVelocity = false;
	bIsOnGround = false;
	bCrouchStateChange = false;
	bADSStateChanged = false;
	bGameplayTagIsMelee = false;
	bIsMoving = false;
	bHasAcceleration = false;
	bIsInAir = false;
	bIsJumping = false;
	bIsFalling = false;
	bAnimationLayerChanged = false;
	bJumpSelectorToJumpStart = false;
	bJumpSelectorToJumpApex = false;
	bJumpStartLoopToJumpApex = false;
	bFallLoopToFallLand = false;
	bFallLandToEndInAir = false;
	bJumpFallInterruptSourcesToEndInAir = false;
}
