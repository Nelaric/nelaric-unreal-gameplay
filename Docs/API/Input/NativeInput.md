<!-- Copyright (c) 2026 Nelaric Contributors -->

English | [简体中文](NativeInput.zh-CN.md)

# Native input infrastructure

GameplayRuntime provides native input configuration, tag bindings, mapping management, and settings-driven modifiers. It uses Enhanced Input and Gameplay Tags without GameplayAbilities or ability input dispatch.

## Responsibility

The framework provides action lookup, binding/removal helpers, mapping activation/removal, and input preferences. The consuming game defines action tags, action value types, trigger events, input callbacks, movement rules, camera controls, crouch, jump, and automatic movement. Framework Pawn and Character bases contain no concrete input handlers and do not automatically create or bind the optional input lifecycle component.

## Pawn input lifecycle component

`UPlayerInputComponent` derives from `UPawnInitStateComponent` and provides the Hero-style input lifecycle without gameplay handlers. Add it to a pawn's `UPawnInitializationConfig` as a component entry, with creation on both authority and clients when the pawn may be locally controlled on either side. Assign `InputConfig` on the component or call `SetInputConfig` to replace it during play. Configure it as required for pawn Ready only when input availability should gate that pawn's readiness.

The participant advances on remote pawns and AI without local input work. When a locally controlled player's configuration and Enhanced Input objects are available, it waits for the owning local player, `UNelaricInputComponent`, and `UEnhancedInputLocalPlayerSubsystem`. After its initialization group commits Ready, it activates mapping contexts and calls `BindInputActions`. Derive a game-specific C++ component and override this hook to bind game-defined actions with the protected `BindNativeAction` helper. Override `UnbindInputActions` to release game state. The component removes tracked action handles and owned mappings when its initialization generation is invalidated, including controller replacement and teardown. `OnPlayerInputReady()` and `OnPlayerInputRevoked()` expose native C++ multicast notifications for lifecycle observers; they do not define gameplay callbacks or Blueprint-assignable delegates.

A null `InputConfig` leaves the participant Ready without installing mappings or announcing local input readiness. `SetInputConfig` invalidates the current generation so the coordinator can retry. A game that uses the component should avoid separately calling `AddInputMappings` for the same input component, since that method replaces its previous mappings. The game still owns action tags, callback logic, and content assets.

## Configure and bind

1. Create Input Action and Input Mapping Context assets in the consuming game. Select action value types and triggers according to game behavior.
2. Define the game's action tags and mapping tags under `InputMapping.*`. Create a `UNelaricInputConfig` data asset with `NativeInputActions` and `MappingContexts`. Duplicate action tags resolve to the first non-null action; by-tag mapping lookup uses the first non-null matching context. Null entries are skipped. No concrete tags are predefined by the framework.
3. In the game's input setup code, obtain the pawn's `UNelaricInputComponent` and the owning local player's `UEnhancedInputLocalPlayerSubsystem`. Do not use a global player index for this lookup.
4. Call `AddInputMappings` with the configuration and local subsystem, then `BindNativeAction` for each game callback and trigger event. Store the returned binding handles in the consuming game's owner.
5. Without `UPlayerInputComponent`, remove bindings on the creating component and call `RemoveInputMappings` during input replacement or teardown. With the lifecycle component, its init-state invalidation performs that cleanup and reinitialization.

All APIs and callbacks run on the game thread. The game authors the configuration and callbacks; the input component retains its installed config until mapping removal or replacement. `BindNativeAction` returns false for missing config, action, or callback target without adding a handle. UObject callback targets are weakly bound. Only the game's specified actions are bound; the framework never calls movement, camera, crouch, or jump APIs.

`DefaultInput.ini` selects `UEnhancedPlayerInput` and `UNelaricInputComponent`, enables Enhanced Input user settings, and selects `UNelaricInputUserSettings`. Other consuming projects must apply these settings and enable the `NelaricGameplay` plugin. No custom local player class is required. Action, mapping, and configuration assets are authored by the game.

## Mapping lifetime

Each mapping entry has an optional `MappingTag` and a `bActivateOnStart` flag, which defaults to true for existing assets. Give an entry a valid mapping tag if the game needs to control it later. `AddInputMappings` replaces this input component's previous mapping configuration and activates entries marked for startup. Passing null config or subsystem releases old mappings and returns false. Registration with user settings is optional and independent of activation, including for mappings that start inactive. Registered remapping rows persist for the local player across pawn replacement.

After installing a configuration, call `AddInputMappingByTag` to activate a configured context at its priority, or `RemoveInputMappingByTag` to remove one activated by this component. Both use exact tags; an invalid or missing tag returns false. Adding an already active context succeeds without changing its priority. Removing a borrowed context returns false and leaves it active. The component retains the installed config until `RemoveInputMappings` or replacement. For independent toggling, use a distinct context for each tag.

Contexts already active are borrowed with their existing priority. The component records only contexts it newly activates and removes them on `RemoveInputMappings` or `OnUnregister`. Duplicate context entries use the first active priority. By-tag operations on duplicate mapping tags select the first non-null context; startup activation still processes every marked entry. Untagged entries may activate at startup but cannot be toggled by tag. No global `ClearAllMappings` or `ClearActionBindings` is used.

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
