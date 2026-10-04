// Copyright (c) 2026 Nelaric Contributors

import * as UE from "ue";
import { blueprint } from "puerts";

const cueRoot = "/Game/Demo/Demo1_GrandWarfront/GAS/GameplayCues/";
const particleRoot = "/Game/ThirdParty/LyraStarterGame/LyraStarterGame/Effects/Particles/";
const muzzleLifetime = 0.08;

function loadCue(name: string): typeof UE.GameplayCueNotify_Static {
    const cueClass = UE.Class.Load(cueRoot + name + "." + name + "_C");
    if (!UE.KismetSystemLibrary.IsValid(cueClass)) {
        throw new Error("Weapon cue Blueprint is unavailable: " + name);
    }
    return blueprint.tojs<typeof UE.GameplayCueNotify_Static>(cueClass);
}

// Load once when the entry starts, rather than during each accepted shot.
const fireCue = loadCue("GCN_Weapon_Rifle_Fire");
const muzzleFlash = UE.NiagaraSystem.Load(
    particleRoot + "Weapons/NS_WeaponFire_MuzzleFlash_Rifle.NS_WeaponFire_MuzzleFlash_Rifle"
);

function vector(value: UE.Vector): UE.Vector {
    return new UE.Vector(value.X, value.Y, value.Z);
}

function spawnAtLocation(
    target: UE.Actor, system: UE.NiagaraSystem, location: UE.Vector
): UE.NiagaraComponent {
    return UE.NiagaraFunctionLibrary.SpawnSystemAtLocation(
        target, system, location, new UE.Rotator(0, 0, 0), new UE.Vector(1, 1, 1),
        true, false, UE.ENCPoolMethod.None, true
    );
}

function expireBurst(component: UE.NiagaraComponent, seconds: number): void {
    // A world-owned weak-object timer also handles systems authored to loop.
    UE.KismetSystemLibrary.K2_SetTimer(component, "Deactivate", seconds, false);
}

/** Overrides only this Blueprint's instant cue; all state remains invocation-local. */
class GCN_Weapon_Rifle_Fire_C extends fireCue {
    OnExecute(target: UE.Actor | null, parameters: UE.GameplayCueParameters): boolean {
        if (!target || !UE.KismetSystemLibrary.IsValid(target) ||
            UE.KismetSystemLibrary.IsDedicatedServer(target) ||
            !UE.KismetSystemLibrary.IsValid(muzzleFlash)) {
            return false;
        }

        const source = parameters.SourceObject;
        const socket = UE.KismetSystemLibrary.IsValid(source) &&
            source.IsA(UE.DemoWeaponDefinition.StaticClass())
            ? (source as UE.DemoWeaponDefinition).MuzzleSocketName : "Muzzle";
        const attach = parameters.TargetAttachComponent;
        const flash = UE.KismetSystemLibrary.IsValid(attach) && attach.DoesSocketExist(socket)
            ? UE.NiagaraFunctionLibrary.SpawnSystemAttached(
                muzzleFlash, attach, socket, new UE.Vector(0, 0, 0), new UE.Rotator(0, 0, 0),
                UE.EAttachLocation.SnapToTargetIncludingScale, true, false, UE.ENCPoolMethod.None, true
            )
            : spawnAtLocation(target, muzzleFlash, vector(parameters.Location));
        if (!UE.KismetSystemLibrary.IsValid(flash)) {
            return false;
        }
        flash.SetVariableVec3("User.Direction", vector(parameters.Normal));
        flash.SetVariableBool("User.Trigger", true);
        flash.Activate(false);
        expireBurst(flash, muzzleLifetime);
        return true;
    }
}

// Replace the existing class in place.
blueprint.mixin(fireCue, GCN_Weapon_Rifle_Fire_C, { objectTakeByNative: true });
console.log("[WeaponCues] Registered rifle Fire mixin.");
