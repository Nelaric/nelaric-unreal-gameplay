// Copyright (c) 2026 Nelaric Contributors

import * as UE from "ue";
import { $ref, $unref } from "puerts";

// Distances are centimeters. Existing input tuning is retained at 18 m.
const referenceHeight = 1800;
const minimumHeight = 300;
// The battlefront deploys the overview at 850 m; allow a wider strategic view.
const maximumHeight = 200000;
const keyboardHeightRate = 1.25;
const wheelHeightStep = 0.18;
const heightSmoothing = 12;
const lowerKey = new UE.Key("Q");
const raiseKey = new UE.Key("E");
const wheelKey = new UE.Key("MouseWheelAxis");
const groundObjects = UE.NewArray(UE.BuiltinByte);
groundObjects.Add(UE.EObjectTypeQuery.WorldStatic);

interface OverviewState {
    pawn: UE.DemoOverviewPawn;
    input: UE.DemoPlayerInputComponent;
    moveSpeed: number;
    dragSensitivity: number;
    groundZ: number;
    targetCameraZ: number;
    lastCameraZ: number;
    ignoredActors: UE.TArray<UE.Actor>;
}

const states = new WeakMap<UE.DemoPlayerController, OverviewState>();
const valid = (object: UE.Object | undefined | null): boolean =>
    !!object && UE.KismetSystemLibrary.IsValid(object);
const clampHeight = (height: number): number =>
    Math.max(minimumHeight, Math.min(maximumHeight, height));

/** Restore live component tuning; teardown only drops state because the pawn may be gone. */
export function releaseOverviewCamera(controller: UE.DemoPlayerController, restoreTuning = true): void {
    const state = states.get(controller);
    if (state && restoreTuning) {
        const pawn = controller.GetOverviewPawn();
        const input = valid(pawn) && pawn === state.pawn
            ? pawn.GetComponentByClass(UE.DemoPlayerInputComponent.StaticClass()) as UE.DemoPlayerInputComponent
            : undefined;
        if (valid(input) && input === state.input) {
            input.OverviewMoveSpeed = state.moveSpeed;
            input.OverviewDragSensitivity = state.dragSensitivity;
        }
    }
    states.delete(controller);
}

/** Called by the controller's UE tick; only the locally possessed overview moves. */
export function updateOverviewCamera(controller: UE.DemoPlayerController, deltaSeconds: number): void {
    if (!controller.IsLocalPlayerController()) return;

    const pawn = controller.GetOverviewPawn();
    if (!valid(pawn) || pawn.IsActorBeingDestroyed() || controller.K2_GetPawn() !== pawn ||
        controller.GetDemoControlMode() !== UE.EDemoControlMode.Overview) {
        releaseOverviewCamera(controller);
        return;
    }

    const input = pawn.GetComponentByClass(UE.DemoPlayerInputComponent.StaticClass()) as UE.DemoPlayerInputComponent;
    let state = states.get(controller);
    if (state && (state.pawn !== pawn || state.input !== input)) {
        releaseOverviewCamera(controller);
        state = undefined;
    }
    if (!valid(input) || !valid(input.GetBoundInputComponent()) ||
        input.GetBoundInputComponent() !== pawn.InputComponent) {
        releaseOverviewCamera(controller);
        return;
    }
    const camera = pawn.GetCameraComponent();
    if (!valid(camera) || !camera.IsActive()) return;
    const cameraLocation = camera.K2_GetComponentLocation();
    if (![cameraLocation.X, cameraLocation.Y, cameraLocation.Z].every(Number.isFinite)) return;

    if (!state) {
        const ignoredActors = UE.NewArray(UE.Actor);
        ignoredActors.Add(pawn);
        state = {
            pawn, input, ignoredActors,
            moveSpeed: input.OverviewMoveSpeed,
            dragSensitivity: input.OverviewDragSensitivity,
            groundZ: cameraLocation.Z - referenceHeight,
            targetCameraZ: cameraLocation.Z,
            lastCameraZ: cameraLocation.Z,
        };
        states.set(controller, state);
    }

    // Use the surface below the camera, not world Z; ignore units and other dynamic objects.
    const groundHit = $ref(new UE.HitResult());
    if (UE.KismetSystemLibrary.LineTraceSingleForObjects(pawn,
        new UE.Vector(cameraLocation.X, cameraLocation.Y, cameraLocation.Z + minimumHeight),
        new UE.Vector(cameraLocation.X, cameraLocation.Y, cameraLocation.Z - 1000000),
        groundObjects, false, state.ignoredActors, UE.EDrawDebugTrace.None, groundHit, true)) {
        const groundZ = $unref(groundHit).ImpactPoint.Z;
        if (Number.isFinite(groundZ)) state.groundZ = groundZ;
    }

    // Deployment and possession may relocate the pawn independently of camera input.
    if (Math.abs(cameraLocation.Z - state.lastCameraZ) > 0.5) {
        state.targetCameraZ = cameraLocation.Z;
    }
    const suspended = controller.IsMoveInputIgnored() || UE.GameplayStatics.IsGamePaused(controller) ||
        !Number.isFinite(deltaSeconds) || deltaSeconds <= 0;
    if (suspended) {
        state.targetCameraZ = cameraLocation.Z;
        state.lastCameraZ = cameraLocation.Z;
        input.OverviewMoveSpeed = 0;
        input.OverviewDragSensitivity = 0;
        return;
    }

    const dt = deltaSeconds;
    const vertical = Number(controller.IsInputKeyDown(raiseKey)) - Number(controller.IsInputKeyDown(lowerKey));
    const wheelValue = controller.GetInputAnalogKeyState(wheelKey);
    const wheel = Number.isFinite(wheelValue) ? wheelValue : 0;
    const targetHeight = clampHeight(state.targetCameraZ - state.groundZ);
    // Wheel input is an impulse, so it is not multiplied by frame duration.
    state.targetCameraZ = state.groundZ + clampHeight(targetHeight *
        Math.exp(vertical * keyboardHeightRate * dt - wheel * wheelHeightStep));
    const smoothedZ = cameraLocation.Z + (state.targetCameraZ - cameraLocation.Z) *
        (1 - Math.exp(-heightSmoothing * dt));
    const cameraZ = state.groundZ + clampHeight(smoothedZ - state.groundZ);
    if (Math.abs(cameraZ - cameraLocation.Z) > 0.001) {
        const location = pawn.K2_GetActorLocation();
        location.Z += cameraZ - cameraLocation.Z;
        pawn.K2_SetActorLocation(location, false, $ref(new UE.HitResult()), false);
    }
    state.lastCameraZ = camera.K2_GetComponentLocation().Z;

    const heightScale = clampHeight(state.lastCameraZ - state.groundZ) / referenceHeight;
    input.OverviewMoveSpeed = Math.max(0, state.moveSpeed) * heightScale;
    input.OverviewDragSensitivity = Math.max(0, state.dragSensitivity) * heightScale;
}
