// Copyright (c) 2026 Nelaric Contributors

/** @file DemoAnimationInstance.h
 * Declares prepared animation inputs, transitions, and weapon blend weights.
 */

#pragma once

#include "Animation/DemoAnimationDataInstance.h"
#include "GameplayTagContainer.h"
#include "Math/Rotator.h"
#include "Math/Vector.h"
#include "Misc/Guid.h"

#include "DemoAnimationInstance.generated.h"

/// Four movement directions, matching the animation direction groups.
UENUM(BlueprintType)
enum class EDemoCardinalDirection : uint8
{
	Forward,  ///< Forward movement.
	Backward, ///< Backward movement.
	Left,     ///< Left movement.
	Right,    ///< Right movement.
};

namespace Nelaric::UnitAnimation
{
/** @brief Value inputs supplied by animation layers and gameplay providers.
 * @details Pass on the game thread through SetAnimationContext. The instance
 * copies these values before calculation; providers retain no shared data.
 */
struct FLocomotionAnimationContext
{
	/// Actual root yaw offset from the animation layer, in degrees.
	float RootYawOffset = 0.0f;
	/// ADS state used when no ADS gameplay tag is configured.
	bool bAimingDownSights = false;
	/// Melee state used when no melee gameplay tag is configured.
	bool bMelee = false;
};

/// Game-thread values consumed exclusively by the scheduled calculation.
struct FLocomotionInput
{
	/// World velocity in centimeters per second.
	FVector Velocity = FVector::ZeroVector;
	/// World acceleration in centimeters per second squared.
	FVector Acceleration = FVector::ZeroVector;
	/// Actor orientation used to calculate relative movement direction.
	FRotator Rotation = FRotator::ZeroRotator;
	/// World aim rotation sampled from the pawn, including remote view pitch.
	FRotator AimRotation = FRotator::ZeroRotator;
	/// Vertical gravity in centimeters per second squared.
	float GravityZ = 0.0f;
	/// Current local velocity direction, sampled with the root yaw offset.
	EDemoCardinalDirection VelocityDirection = EDemoCardinalDirection::Forward;
	/// Distance below the capsule, in centimeters; -1 means unavailable.
	float GroundDistance = -1.0f;
	/// Value inputs from animation and gameplay providers.
	FLocomotionAnimationContext Context;
	/// Retained crouch state revision, including skipped calculations.
	uint64 CrouchChangeSerial = 0;
	/// Retained ADS state revision, including skipped calculations.
	uint64 ADSChangeSerial = 0;
	/// Monotonic revision retained across scheduled update skips.
	uint64 LayerChangeSerial = 0;
	/// Active weapon identity; invalid when no weapon is selected.
	FGuid ActiveWeaponId;
	/// Seconds since the active weapon fired; -1 means no accepted shot.
	float TimeSinceFiredWeapon = -1.0f;
	/// Evaluated hip-fire override curve sampled before worker dispatch.
	float HipFireOverridePoseCurveValue = 0.0f;
	/// Evaluated left-hand suppression curve sampled before worker dispatch.
	float DisableLeftHandPoseOverrideCurveValue = 0.0f;
	/// Whether the sampled instance enables the left-hand pose override.
	bool bEnableLeftHandPoseOverride = false;
	/// Whether the main animation instance currently has a playing montage.
	bool bMontagePlaying = false;
	/// Whether the character currently uses its crouched capsule.
	bool bCrouching = false;
	/// Whether the movement component reports ground movement.
	bool bGrounded = false;
	/// Whether the movement component reports falling.
	bool bInAir = false;
};

} // namespace Nelaric::UnitAnimation

/** @brief Calculates movement, airborne transitions, and weapon weights.
 * @details Unreal owns the instance. Derive an animation blueprint from this
 * class. Idle to Move uses bHasVelocity; Move to Idle uses its inverse.
 * Airborne transitions use their matching results. The subsystem
 * captures game-thread inputs and schedules calculation-only worker updates.
 * @note Automatic transitions remain in the animation blueprint.
 */
UCLASS(MinimalAPI, Blueprintable)
class UDemoAnimationInstance : public UDemoAnimationDataInstance
{
	GENERATED_BODY()

public:
	/** @brief Supplies actual animation-layer and gameplay input values.
	 * @details Game thread only. Values are copied before scheduled workers.
	 * @param Context Prepared root yaw, ADS, and melee values.
	 * @note Configured gameplay tags take precedence over the ADS/melee values.
	 * @note Non-finite numeric values are rejected without replacing inputs.
	 */
	DEMOGAME_API void SetAnimationContext(const Nelaric::UnitAnimation::FLocomotionAnimationContext& Context);

	/** @brief Enables procedural rig evaluation while movement is not airborne.
	 * @details Reads completed animation data without accessing engine objects.
	 * Call on the game thread or during synchronized animation evaluation.
	 * @return False while in the air; true for all other movement states.
	 */
	UFUNCTION(BlueprintPure, Category = "Demo|Animation|Layers", meta = (BlueprintThreadSafe))
	bool ShouldEnableControlRig() const;

	/// Interpolated horizontal speed in centimeters per second.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Demo|Animation|Movement")
	float GroundSpeed = 0.0f;

	/// Actual vertical velocity in centimeters per second.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Demo|Animation|Movement")
	float VerticalSpeed = 0.0f;

	/// Velocity direction relative to actor yaw, in degrees.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Demo|Animation|Movement")
	float Direction = 0.0f;

	/// Horizontal acceleration magnitude in centimeters per second squared.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Demo|Animation|Movement")
	float AccelerationAmount = 0.0f;

	/// Estimated apex time in seconds; zero when not ascending or unavailable.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Demo|Animation|Movement")
	float TimeToJumpApex = 0.0f;

	/// Actor-local horizontal velocity in centimeters per second.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Demo|Animation|Movement")
	FVector LocalVelocity2D = FVector::ZeroVector;

	/// Actor-local horizontal acceleration in centimeters per second squared.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Demo|Animation|Movement")
	FVector LocalAcceleration2D = FVector::ZeroVector;

	/// Current four-way velocity direction, including root yaw offset.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Demo|Animation|Movement")
	EDemoCardinalDirection LocalVelocityDirection = EDemoCardinalDirection::Forward;

	/// Distance below the capsule, in centimeters; -1 means unavailable.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Demo|Animation|Movement")
	float GroundDistance = -1.0f;

	/// Actual root yaw offset supplied by the animation layer, in degrees.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Demo|Animation|Layers")
	float RootYawOffset = 0.0f;

	/// Aim yaw relative to the actor and root yaw offset, in [-180, 180] degrees.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Demo|Animation|Weapon")
	float AimYaw = 0.0f;

	/// Aim pitch relative to the actor, normalized to [-180, 180] degrees.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Demo|Animation|Weapon")
	float AimPitch = 0.0f;

	/// Dynamic upper-body additive weight, between zero and one.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Demo|Animation|Layers")
	float UpperbodyDynamicAdditiveWeight = 0.0f;

	/// Upper-body hip-fire override weight, between zero and one.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Demo|Animation|Weapon")
	float HipFireUpperBodyOverrideWeight = 0.0f;

	/// Aim offset blend weight, between zero and one.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Demo|Animation|Weapon")
	float AimOffsetBlendWeight = 1.0f;

	/// Left-hand pose override weight, clamped between zero and one.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Demo|Animation|Weapon")
	float LeftHandPoseOverrideWeight = 0.0f;

	/// Seconds since the active weapon fired; -1 means no accepted shot.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Demo|Animation|Weapon")
	float TimeSinceFiredWeapon = -1.0f;

	/// Whether the prepared character state reports crouching.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Demo|Animation|Movement", meta = (DisplayName = "Is Crouching"))
	bool bIsCrouching = false;

	/// Whether the prepared gameplay state reports aiming down sights.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Demo|Animation|Weapon")
	bool bGameplayTagIsADS = false;

	/** @brief Whether actual horizontal velocity exceeds its tolerance.
	 * @details Use directly for Idle to Move and invert for Move to Idle.
	 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Demo|Animation|Movement", meta = (DisplayName = "Has Velocity"))
	bool bHasVelocity = false;

	/// Whether the movement component reports ground movement.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Demo|Animation|Movement")
	bool bIsOnGround = false;

	/// Whether a retained crouch change was processed in this calculation.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Demo|Animation|Movement")
	bool bCrouchStateChange = false;

	/// Whether a retained ADS change was processed in this calculation.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Demo|Animation|Movement")
	bool bADSStateChanged = false;

	/// Whether the prepared gameplay state reports an active melee action.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Demo|Animation|Movement")
	bool bGameplayTagIsMelee = false;

	/// Whether the actual horizontal velocity exceeds the movement threshold.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Demo|Animation|Movement")
	bool bIsMoving = false;

	/// Whether horizontal acceleration indicates movement intent.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Demo|Animation|Movement")
	bool bHasAcceleration = false;

	/// Whether the movement component reports airborne falling movement.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Demo|Animation|Movement")
	bool bIsInAir = false;

	/// Whether airborne vertical velocity is positive.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Demo|Animation|Movement")
	bool bIsJumping = false;

	/// Whether airborne vertical velocity is non-positive.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Demo|Animation|Movement")
	bool bIsFalling = false;

	/// Whether a retained layer change was processed in this update.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Demo|Animation|Layers")
	bool bAnimationLayerChanged = false;

	/// Selects JumpStart while moving upward in the air.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Demo|Animation|Transitions|Air")
	bool bJumpSelectorToJumpStart = false;

	/// Selects JumpApex while falling, including zero vertical velocity.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Demo|Animation|Transitions|Air")
	bool bJumpSelectorToJumpApex = false;

	/// Enters JumpApex when time to the apex is below the configured lead.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Demo|Animation|Transitions|Air")
	bool bJumpStartLoopToJumpApex = false;

	/// Enters FallLand when the ground is closer than the landing threshold.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Demo|Animation|Transitions|Air")
	bool bFallLoopToFallLand = false;

	/// Ends landing recovery when the movement component reports ground.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Demo|Animation|Transitions|Air")
	bool bFallLandToEndInAir = false;

	/// Interrupts airborne recovery when movement reports ground.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Demo|Animation|Transitions|Air")
	bool bJumpFallInterruptSourcesToEndInAir = false;

	/// Horizontal speed threshold in centimeters per second.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Demo|Animation|Settings", meta = (ClampMin = "0.0"))
	float MovingSpeedThreshold = 3.0f;

	/// Movement intent threshold in centimeters per second squared.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Demo|Animation|Settings", meta = (ClampMin = "0.0"))
	float AccelerationThreshold = 0.001f;

	/// Horizontal speed interpolation rate; zero uses the target directly.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Demo|Animation|Settings", meta = (ClampMin = "0.0"))
	float SpeedInterpRate = 10.0f;

	/// Squared horizontal velocity tolerance for HasVelocity, in cm/s squared.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Demo|Animation|Settings", meta = (ClampMin = "0.0"))
	float VelocitySquaredTolerance = 0.000001f;

	/// Hysteresis added to the previous forward/backward direction, in degrees.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Demo|Animation|Settings",
	          meta = (ClampMin = "0.0", ClampMax = "45.0"))
	float CardinalDirectionDeadZone = 0.0f;

	/// Estimated time before the jump apex at which its state begins.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Demo|Animation|Settings", meta = (ClampMin = "0.0"))
	float JumpApexLeadSeconds = 0.40f;

	/// Ground distance below which FallLand begins, in centimeters.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Demo|Animation|Settings", meta = (ClampMin = "0.0"))
	float LandingGroundDistance = 200.0f;

	/// Maximum distance used for airborne ground traces, in centimeters.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Demo|Animation|Settings", meta = (ClampMin = "1.0"))
	float GroundTraceDistance = 100000.0f;

	/// Dynamic additive fade-out rate; zero uses the target directly.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Demo|Animation|Settings|Layers",
	          meta = (ClampMin = "0.0"))
	float UpperbodyDynamicAdditiveBlendOutInterpRate = 6.0f;

	/// Seconds to hold the hip-fire pose after an accepted shot; zero disables.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Demo|Animation|Settings|Weapon",
	          meta = (ClampMin = "0.0"))
	float RaiseWeaponAfterFiringDuration = 0.5f;

	/// Whether crouched characters may use the hip-fire upper-body override.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Demo|Animation|Settings|Weapon")
	bool bRaiseWeaponAfterFiringWhenCrouched = true;

	/// Hip-fire lowering interpolation rate; zero uses the target directly.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Demo|Animation|Settings|Weapon",
	          meta = (ClampMin = "0.0"))
	float HipFireBlendOutInterpRate = 1.0f;

	/// Aim offset interpolation rate; zero uses the target directly.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Demo|Animation|Settings|Weapon",
	          meta = (ClampMin = "0.0"))
	float AimOffsetBlendInterpRate = 10.0f;

	/// Root yaw magnitude below which acceleration uses the relaxed aim pose.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Demo|Animation|Settings|Weapon",
	          meta = (ClampMin = "0.0"))
	float AimOffsetRootYawThreshold = 10.0f;

	/// Override curve sampled from the active weapon layer, then the main BP.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Demo|Animation|Settings|Weapon")
	FName HipFireOverridePoseCurveName;

	/// Whether the left-hand pose override is enabled before curve suppression.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Demo|Animation|Settings|Weapon",
	          meta = (DisplayName = "Enable Left Hand Pose Override"))
	bool bEnableLeftHandPoseOverride = false;

	/// Suppression curve sampled from the active weapon layer, then the main BP.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Demo|Animation|Settings|Weapon")
	FName DisableLeftHandPoseOverrideCurveName;

	/// Optional ASC tag for ADS; absent tags use the supplied context value.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Demo|Animation|Settings")
	FGameplayTag ADSTag;

	/// Optional ASC tag for melee; absent tags use the supplied context value.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Demo|Animation|Settings")
	FGameplayTag MeleeTag;

public:
	UDemoAnimationInstance();
	virtual void UpdateAnimationData(float DeltaSeconds) override;
	virtual void PrepareAnimationData(const ADemoCharacter& Character) override;
	virtual void NativeInitializeAnimation() override;
	virtual void NativePostEvaluateAnimation() override;

private:
	void CaptureAnimationInputs(const ADemoCharacter& Character);
	void CaptureGameplayState(const ADemoCharacter& Character);
	void CaptureWeaponInputs(const ADemoCharacter& Character);
	void UpdateBlendWeightData(float DeltaSeconds);
	float CaptureGroundDistance(const ADemoCharacter& Character) const;
	void ResetCalculatedData();

	Nelaric::UnitAnimation::FLocomotionInput Input;
	Nelaric::UnitAnimation::FLocomotionAnimationContext AnimationContext;
	FGuid LastCalculatedWeaponId;
	uint64 LastCalculatedLayerSerial = 0;
	uint64 CrouchChangeSerial = 0;
	uint64 ADSChangeSerial = 0;
	uint64 LastCalculatedCrouchSerial = 0;
	uint64 LastCalculatedADSSerial = 0;
	EDemoCardinalDirection LastSampledDirection = EDemoCardinalDirection::Forward;
	bool bHasGameplaySample = false;
	bool bWasCrouching = false;
	bool bWasADS = false;
	bool bWasMovingAtSample = false;
};
