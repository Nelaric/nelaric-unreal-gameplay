// Copyright (c) 2026 Nelaric Contributors

import * as UE from "ue";
import { $ref, blueprint, toDelegate } from "puerts";

const characterPath =
    "/Game/Demo/Demo1_GrandWarfront/Characters/BP_DemoCharacter.BP_DemoCharacter_C";
// A small velocity change in cm/s, independent of the imported body masses.
const deathImpulseSpeed = 80;
const deathSettleDelay = 1;
const corpseFriction = 0.9;

const characterClass = UE.Class.Load(characterPath);
if (!UE.KismetSystemLibrary.IsValid(characterClass)) {
    throw new Error("Demo character Blueprint is unavailable: " + characterPath);
}
const characterBlueprint = blueprint.tojs<typeof UE.DemoCharacter>(characterClass);

interface RagdollState {
    parent: UE.SceneComponent;
    socket: string;
    relativeTransform: UE.Transform;
    meshProfile: string;
    meshCollision: UE.ECollisionEnabled;
    meshObjectType: UE.ECollisionChannel;
    meshResponses: UE.ECollisionResponse[];
    capsuleCollision: UE.ECollisionEnabled;
    pauseAnims: boolean;
    gravity: boolean;
    meshTickEnabled: boolean;
    physicalMaterial: UE.PhysicalMaterial | null;
    bodyNames: string[];
    settleTimer?: UE.TimerHandle;
}

// The class is shared across worlds; each character owns its own saved pose.
const ragdolls = new WeakMap<UE.DemoCharacter, RagdollState>();

function snapshot(character: UE.DemoCharacter, physicsAsset: UE.PhysicsAsset): RagdollState {
    const mesh = character.Mesh;
    const responses: UE.ECollisionResponse[] = [];
    for (let channel = 0; channel < UE.ECollisionChannel.ECC_OverlapAll_Deprecated; ++channel) {
        responses.push(mesh.GetCollisionResponseToChannel(channel));
    }
    return {
        parent: mesh.GetAttachParent(),
        socket: mesh.GetAttachSocketName(),
        relativeTransform: mesh.GetRelativeTransform(),
        meshProfile: mesh.GetCollisionProfileName(),
        meshCollision: mesh.GetCollisionEnabled(),
        meshObjectType: mesh.GetCollisionObjectType(),
        meshResponses: responses,
        capsuleCollision: character.CapsuleComponent.GetCollisionEnabled(),
        pauseAnims: mesh.bPauseAnims,
        gravity: mesh.IsGravityEnabled(),
        meshTickEnabled: mesh.IsComponentTickEnabled(),
        physicalMaterial: mesh.BodyInstance.PhysMaterialOverride,
        bodyNames: Array.from(physicsAsset.SkeletalBodySetups, body => body.BoneName),
    };
}

function clearVelocity(mesh: UE.SkeletalMeshComponent): void {
    mesh.SetAllPhysicsLinearVelocity(new UE.Vector(0, 0, 0));
    mesh.SetAllPhysicsAngularVelocityInDegrees(new UE.Vector(0, 0, 0));
}

function configureCorpseMaterial(character: UE.DemoCharacter): void {
    // Override this mesh instance only; shared Physics Assets remain untouched.
    const material = new UE.PhysicalMaterial(character);
    material.Friction = corpseFriction;
    material.StaticFriction = corpseFriction;
    material.FrictionCombineMode = UE.EFrictionCombineMode.Max;
    material.bOverrideFrictionCombineMode = true;
    material.Restitution = 0;
    material.RestitutionCombineMode = UE.EFrictionCombineMode.Min;
    material.bOverrideRestitutionCombineMode = true;
    material.SleepLinearVelocityThreshold = 5;
    material.SleepAngularVelocityThreshold = 0.2;
    material.SleepCounterThreshold = 10;
    character.Mesh.SetPhysMaterialOverride(material);
}

function settleCorpse(character: UE.DemoCharacter, state: RagdollState): void {
    if (!UE.KismetSystemLibrary.IsValid(character) || ragdolls.get(character) !== state ||
        character.IsAlive()) {
        return;
    }
    state.settleTimer = undefined;
    const mesh = character.Mesh;
    if (!UE.KismetSystemLibrary.IsValid(mesh)) {
        return;
    }
    clearVelocity(mesh);
    mesh.SetEnableGravity(false);
    // The reflected sleep method operates on one bone, not the whole ragdoll.
    state.bodyNames.forEach(bone => mesh.PutRigidBodyToSleep(bone));
    // Disable simulation while retaining the last blended corpse pose.
    mesh.SetAllBodiesBelowPhysicsDisabled("None", true, true);
    mesh.SetComponentTickEnabled(false);
    console.log("[SoldierDeath] Corpse settled: " + character.GetName());
}

/** Replaces the existing Blueprint's death presentation through native events. */
class BP_DemoCharacter_C extends characterBlueprint {
    OnDeath(_damageInstigator: UE.Actor | null): void {
        if (this.bHidden || UE.KismetSystemLibrary.IsDedicatedServer(this) || ragdolls.has(this)) {
            return;
        }
        const mesh = this.Mesh;
        if (!UE.KismetSystemLibrary.IsValid(mesh)) {
            return;
        }
        const skeletalMesh = mesh.GetSkeletalMeshAsset();
        const physicsAsset = UE.KismetSystemLibrary.IsValid(mesh.PhysicsAssetOverride)
            ? mesh.PhysicsAssetOverride
            : UE.KismetSystemLibrary.IsValid(skeletalMesh) ? skeletalMesh.GetPhysicsAsset() : null;
        if (!physicsAsset || !UE.KismetSystemLibrary.IsValid(physicsAsset)) {
            console.warn("[SoldierDeath] Character mesh needs a Physics Asset: " + this.GetName());
            return;
        }
        const state = snapshot(this, physicsAsset);
        ragdolls.set(this, state);

        this.CharacterMovement.StopMovementImmediately();
        this.CharacterMovement.DisableMovement();
        this.CapsuleComponent.SetCollisionEnabled(UE.ECollisionEnabled.NoCollision);
        mesh.K2_DetachFromComponent(
            UE.EDetachmentRule.KeepWorld, UE.EDetachmentRule.KeepWorld,
            UE.EDetachmentRule.KeepWorld, false
        );
        mesh.bPauseAnims = true;
        mesh.SetCollisionProfileName("Ragdoll", false);
        mesh.SetEnableGravity(true);
        mesh.SetSimulatePhysics(true);
        mesh.SetAllBodiesSimulatePhysics(true);
        mesh.SetAllBodiesPhysicsBlendWeight(1, false);
        clearVelocity(mesh);
        configureCorpseMaterial(this);
        mesh.WakeAllRigidBodies();

        // Native death state freezes and replicates the last attack direction.
        const direction = this.GetDeathImpulseDirection();
        if (Number.isFinite(direction.X) && Number.isFinite(direction.Y) &&
            Number.isFinite(direction.Z)) {
            mesh.AddImpulseToAllBodiesBelow(
                new UE.Vector(
                    direction.X * deathImpulseSpeed,
                    direction.Y * deathImpulseSpeed,
                    direction.Z * deathImpulseSpeed
                ), "None", true, true
            );
        }
        // A single world-owned callback; no Tick or repeated settle checks.
        state.settleTimer = UE.KismetSystemLibrary.K2_SetTimerDelegate(
            toDelegate(this, () => settleCorpse(this, state)), deathSettleDelay, false
        );
    }

    OnDeathPresentationReset(): void {
        const state = ragdolls.get(this);
        if (!state) {
            return;
        }
        ragdolls.delete(this);
        if (state.settleTimer) {
            UE.KismetSystemLibrary.K2_ClearTimerHandle(this, state.settleTimer);
        }
        const mesh = this.Mesh;
        if (!UE.KismetSystemLibrary.IsValid(mesh)) {
            return;
        }
        clearVelocity(mesh);
        mesh.SetAllBodiesSimulatePhysics(false);
        mesh.SetAllBodiesBelowPhysicsDisabled("None", false, true);
        mesh.SetSimulatePhysics(false);
        mesh.SetAllBodiesPhysicsBlendWeight(0, false);
        mesh.SetEnableGravity(state.gravity);
        mesh.SetPhysMaterialOverride(state.physicalMaterial);
        if (state.meshProfile !== "Custom") {
            mesh.SetCollisionProfileName(state.meshProfile, false);
        }
        // Custom profiles require restoring their actual channel responses.
        mesh.SetCollisionObjectType(state.meshObjectType);
        state.meshResponses.forEach((response, channel) => {
            mesh.SetCollisionResponseToChannel(channel, response);
        });
        mesh.SetCollisionEnabled(state.meshCollision);
        if (UE.KismetSystemLibrary.IsValid(state.parent)) {
            mesh.K2_AttachToComponent(
                state.parent, state.socket, UE.EAttachmentRule.KeepRelative,
                UE.EAttachmentRule.KeepRelative, UE.EAttachmentRule.KeepRelative, false
            );
        }
        mesh.K2_SetRelativeTransform(state.relativeTransform, false, $ref(new UE.HitResult()), true);
        mesh.bPauseAnims = state.pauseAnims;
        mesh.SetComponentTickEnabled(state.meshTickEnabled);
        this.CapsuleComponent.SetCollisionEnabled(state.capsuleCollision);
    }
}

blueprint.mixin(characterBlueprint, BP_DemoCharacter_C, { objectTakeByNative: true });
console.log("[SoldierDeath] Registered DemoCharacter death mixin.");
