// Copyright (c) 2026 Nelaric Contributors

#include "Input/DemoPlayerInputComponent.h"

#include "NativeGameplayTags.h"

// Keep tags referenced by existing input assets independent of bindings.
namespace Nelaric::DemoInputTags
{
UE_DEFINE_GAMEPLAY_TAG_STATIC(InputTag_Move, "InputTag.Move");
UE_DEFINE_GAMEPLAY_TAG_STATIC(InputTag_Look_Mouse, "InputTag.Look.Mouse");
UE_DEFINE_GAMEPLAY_TAG_STATIC(InputTag_Look_Stick, "InputTag.Look.Stick");
UE_DEFINE_GAMEPLAY_TAG_STATIC(InputTag_Jump, "InputTag.Jump");
} // namespace Nelaric::DemoInputTags

UDemoPlayerInputComponent::UDemoPlayerInputComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}
