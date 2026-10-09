// Copyright (c) 2026 Nelaric Contributors

/** @file DemoGasPlayerState.h Declares common demo participant state. */
#pragma once
#include "NelaricGasPlayerState.h"
#include "DemoGasPlayerState.generated.h"

/// Installs demo character attributes for both human and bot participants.
UCLASS(MinimalAPI, Blueprintable)
class ADemoGasPlayerState : public ANelaricGasPlayerState
{
	GENERATED_BODY()
public:
	/// Stable match faction independent of the currently controlled body.
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Demo|Team")
	uint8 BattlefrontTeamId = 255;

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void CopyProperties(APlayerState* PlayerState) override;
	DEMOGAME_API ADemoGasPlayerState();
};
