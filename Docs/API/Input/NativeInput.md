<!-- Copyright (c) 2026 Nelaric -->

English | [简体中文](NativeInput.zh-CN.md)

# Native input infrastructure

GameplayRuntime provides native input configuration, tag bindings, mapping management, and settings-driven modifiers. It uses Enhanced Input and Gameplay Tags without GameplayAbilities or ability input dispatch.

## Responsibility

The framework provides action lookup, binding/removal helpers, mapping activation/removal, and input preferences. The consuming game defines action tags, action value types, trigger events, input callbacks, movement rules, camera controls, crouch, jump, and automatic movement. Framework Pawn and Character bases contain no concrete input handlers and do not automatically create an input gameplay component or bind actions.

## Configure and bind

1. Create Input Action and Input Mapping Context assets in the consuming game. Select action value types and triggers according to game behavior.
2. Define the game's action tags. Create a `UNelaricInputConfig` data asset with `NativeInputActions` and `MappingContexts`. Duplicate action tags resolve to the first non-null action; null entries are skipped. No concrete action tags are predefined by the framework.
3. In the game's input setup code, obtain the pawn's `UNelaricInputComponent` and the owning local player's `UEnhancedInputLocalPlayerSubsystem`. Do not use a global player index for this lookup.
4. Call `AddInputMappings` with the configuration and local subsystem, then `BindNativeAction` for each game callback and trigger event. Store the returned binding handles in the consuming game's owner.
5. During input replacement or teardown, call `RemoveBinds` on the component that created those handles and `RemoveInputMappings`. Repeat input setup when the game's required context is ready. Existing `UPawnInitializationComponent` readiness/revocation notifications are available for the game to coordinate this lifecycle.

All APIs and callbacks run on the game thread. The game owns configuration lifetime and callback behavior. `BindNativeAction` returns false for missing config, action, or callback target without adding a handle. UObject callback targets are weakly bound. Only the game's specified actions are bound; the framework never calls movement, camera, crouch, or jump APIs.

`DefaultInput.ini` selects `UEnhancedPlayerInput` and `UNelaricInputComponent`, enables Enhanced Input user settings, and selects `UNelaricInputUserSettings`. Other consuming projects must apply these settings and enable the `NelaricGameplay` plugin. No custom local player class is required. Action, mapping, and configuration assets are authored by the game.

## Mapping lifetime

`AddInputMappings` replaces this input component's previous mapping configuration. Passing null config or subsystem releases old mappings and returns false. Registration with user settings is optional and independent of activation. Registered remapping rows persist for the local player across pawn replacement.

Contexts already active are borrowed with their existing priority. The component records only contexts it newly activates and removes them on `RemoveInputMappings` or `OnUnregister`. Duplicate context entries use the first active priority. No global `ClearAllMappings` or `ClearActionBindings` is used.

Use distinct mapping contexts for independently managed systems. Another system must not concurrently claim a context owned by this component, because Enhanced Input does not provide reference-counted activation ownership. Mapping removal does not remove game callback bindings; `RemoveBinds` handles those separately.

## Preferences and modifiers

Enhanced Input owns one `UNelaricInputUserSettings` per local player and supplies key profiles and key remapping. Mark action or context keys as player mappable and enable `bRegisterWithSettings` in the config. Use inherited `MapPlayerKey`, `ApplySettings`, and `AsyncSaveSettings` for key changes and persistence. Native preference fields also use SaveGame serialization; call `AsyncSaveSettings` after changing them.

| Modifier | Preferences and behavior |
| --- | --- |
| `UNelaricInputModifierMouseSensitivity` | Per-axis mouse multipliers, clamped to 0–10. |
| `UNelaricInputModifierGamepadSensitivity` | Normal or targeting multiplier selected by the modifier, clamped to 0–10. |
| `UNelaricInputModifierDeadZone` | Move or look lower threshold, radial or axial remapping, and upper threshold. |
| `UNelaricInputModifierAimInversion` | Horizontal and vertical axis inversion. |

The game selects which action or mapping uses each modifier. Apply dead zone before sensitivity and keep mouse and gamepad scaling on their appropriate device paths. Modifiers retain the input value type; Boolean values or missing settings pass through unchanged. An upper dead zone threshold at or below the lower threshold produces zero. Non-finite preference scalars are treated as zero. Modifiers transform values without invoking gameplay actions.

Gamepad sensitivity uses editable scalar preferences.
