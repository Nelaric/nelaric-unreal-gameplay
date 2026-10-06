// Copyright (c) 2026 Nelaric Contributors

/** @file DemoSquadDefinition.h Declares authored squad and tactic settings. */
#pragma once

#include "AI/DemoSquadTypes.h"
#include "Engine/DataAsset.h"
#include "DemoSquadDefinition.generated.h"

/// Shared squad sizing and authority recovery policy; not a performance claim.
UCLASS(MinimalAPI, BlueprintType)
class UDemoSquadDefinition : public UDataAsset
{
	GENERATED_BODY()
public:
	/// Maximum registered roster size; default eight, runtime bound sixty-four.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Squad", meta = (ClampMin = "1", ClampMax = "64"))
	int32 MemberLimit = 8;
	/// Desired support group size when sufficient capabilities exist.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Squad", meta = (ClampMin = "1", ClampMax = "32"))
	int32 SupportGroupSize = 3;
	/// Minimum confirmed ready support before advancing.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Squad", meta = (ClampMin = "1", ClampMax = "32"))
	int32 MinimumReadySupport = 2;
	/// World seconds before eligible succession is confirmed.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Squad", meta = (ClampMin = "0.1"))
	float LeadershipRecoverySeconds = 1.0f;
	/// World seconds for limited retained orders after loss of authority.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Squad", meta = (ClampMin = "0.1"))
	float RetainedOrderSeconds = 3.0f;
	/// Whether a player-controlled member may hold command authority.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Squad")
	bool bAllowPlayerCommander = true;

public:
};

/// Bounded tactical tuning available to authored StateTree bindings.
UCLASS(MinimalAPI, BlueprintType)
class UDemoSquadTactics : public UDataAsset
{
	GENERATED_BODY()
public:
	/// Member status age beyond which it cannot guarantee phase completion.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Squad", meta = (ClampMin = "1"))
	float FeedbackTimeoutSeconds = 3.0f;
	/// Formation slot spacing, in centimeters.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Squad", meta = (ClampMin = "100"))
	float SlotSpacing = 200.0f;
	/// Initial contact confidence lost per world second.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Squad", meta = (ClampMin = "0.01"))
	float ContactDecay = 0.08f;
	/// Contact uncertainty growth, in centimeters per world second.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Squad", meta = (ClampMin = "0"))
	float ContactUncertaintyGrowth = 100.0f;

public:
};
