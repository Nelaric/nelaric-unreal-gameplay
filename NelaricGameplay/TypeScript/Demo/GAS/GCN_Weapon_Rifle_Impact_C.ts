// Copyright (c) 2026 Nelaric Contributors

import * as UE from "ue";
import { blueprint } from "puerts";

const cueRoot = "/Game/Demo/Demo1_GrandWarfront/GAS/GameplayCues/";
const particleRoot = "/Game/ThirdParty/LyraStarterGame/LyraStarterGame/Effects/Particles/";
const impactLifetime = 1.5;

function loadCue(name: string): typeof UE.GameplayCueNotify_Static {
    const cueClass = UE.Class.Load(cueRoot + name + "." + name + "_C");
    if (!UE.KismetSystemLibrary.IsValid(cueClass)) {
        throw new Error("Weapon cue Blueprint is unavailable: " + name);
    }
    return blueprint.tojs<typeof UE.GameplayCueNotify_Static>(cueClass);
}

// Load once when the entry starts, rather than during each accepted shot.
const impactCue = loadCue("GCN_Weapon_Rifle_Impact");
const concreteImpact = UE.NiagaraSystem.Load(
    particleRoot + "Impacts/NS_ImpactConcrete.NS_ImpactConcrete"
);
const characterImpact = UE.NiagaraSystem.Load(
    particleRoot + "Impacts/NS_ImactSparksCharacter.NS_ImactSparksCharacter"
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

/** Uses the blocking hit in the context, including impacts that cause no damage. */
class GCN_Weapon_Rifle_Impact_C extends impactCue {
    OnExecute(target: UE.Actor | null, parameters: UE.GameplayCueParameters): boolean {
        if (!target || !UE.KismetSystemLibrary.IsValid(target) ||
            UE.KismetSystemLibrary.IsDedicatedServer(target)) {
            return false;
        }
        const hit = UE.AbilitySystemBlueprintLibrary.GetHitResult(parameters);
        if (!hit.bBlockingHit) {
            return false;
        }
        const hitActor = hit.GetActor();
        const isCharacter = UE.KismetSystemLibrary.IsValid(hitActor) &&
            hitActor.IsA(UE.Character.StaticClass());
        const system = isCharacter ? characterImpact : concreteImpact;
        if (!UE.KismetSystemLibrary.IsValid(system)) {
            return false;
        }
        const location = vector(parameters.Location);
        const impact = spawnAtLocation(target, system, location);
        if (!UE.KismetSystemLibrary.IsValid(impact)) {
            return false;
        }

        const positions = UE.NewArray(UE.Vector);
        positions.Add(location);
        UE.NiagaraDataInterfaceArrayFunctionLibrary.SetNiagaraArrayPosition(
            impact, "User.ImpactPositions", positions
        );
        const normals = UE.NewArray(UE.Vector);
        normals.Add(vector(parameters.Normal));
        UE.NiagaraDataInterfaceArrayFunctionLibrary.SetNiagaraArrayVector(
            impact, "User.ImpactNormals", normals
        );
        impact.SetVariableInt("User.NumberOfHits", 1);
        if (!isCharacter) {
            impact.SetVariableInt("User.StartOffset", 0);
            impact.SetVariablePosition("User.MuzzlePosition",
                UE.AbilitySystemBlueprintLibrary.EffectContextGetOrigin(parameters.EffectContext));
        }
        impact.Activate(false);
        expireBurst(impact, impactLifetime);
        return true;
    }
}

// Replace the existing class in place.
blueprint.mixin(impactCue, GCN_Weapon_Rifle_Impact_C, { objectTakeByNative: true });
console.log("[WeaponCues] Registered rifle Impact mixin.");
