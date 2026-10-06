// Copyright (c) 2026 Nelaric Contributors

import * as UE from "ue";
import { $ref, $unref, blueprint } from "puerts";
import TS_SquadBootstrap from "./TS_SquadBootstrap";

const commandClass = UE.Class.Load(
    "/Game/Demo/Demo1_GrandWarfront/AI/Squad/BP_DemoSquadCommand.BP_DemoSquadCommand_C"
);
const bootstrapClass = UE.Class.Load(
    "/Game/Demo/Demo1_GrandWarfront/AI/Squad/BP_SquadBootstrapRuntime.BP_SquadBootstrapRuntime_C"
);
if (!UE.KismetSystemLibrary.IsValid(commandClass) || !UE.KismetSystemLibrary.IsValid(bootstrapClass)) {
    throw new Error("Configured squad command or bootstrap Blueprint is unavailable.");
}
const commandBlueprint = blueprint.tojs<typeof UE.DemoSquadCommandActor>(commandClass);
const bindings = new WeakMap<UE.DemoSquadCommandActor, TS_SquadBootstrap[]>();

function startBindings(command: UE.DemoSquadCommandActor): void {
    if (!UE.KismetSystemLibrary.IsValid(command) || !command.HasAuthority()) return;
    const output = $ref(UE.NewArray(UE.Actor));
    UE.GameplayStatics.GetAllActorsOfClass(command, bootstrapClass, output);
    const matches: TS_SquadBootstrap[] = [];
    const actors = $unref(output);
    console.log("[SquadSetup] Bootstrap actors=" + actors.Num());
    for (let index = 0; index < actors.Num(); ++index) {
        const actor = actors.Get(index) as TS_SquadBootstrap;
        if (UE.KismetMathLibrary.EqualEqual_ObjectObject(actor.Squad, command)) matches.push(actor);
    }
    if (matches.length !== 1) {
        console.error("[SquadSetup] Expected one configured bootstrap for " + command.GetName() +
            "; found " + matches.length + ".");
        return;
    }
    bindings.set(command, matches);
    // Call the proxy's normal UFunction after serialized instance fields exist.
    matches[0].StartSquad();
}

class BP_DemoSquadCommand_Squad extends commandBlueprint {
    ReceiveBeginPlay(): void {
        console.log("[SquadSetup] Command BeginPlay: " + this.GetName() + " authority=" + this.HasAuthority());
        if (!this.HasAuthority()) return;
        startBindings(this);
    }

    ReceiveEndPlay(_reason: UE.EEndPlayReason): void {
        (bindings.get(this) || []).forEach(actor => {
            if (UE.KismetSystemLibrary.IsValid(actor)) actor.StopSquad();
        });
        bindings.delete(this);
    }
}

blueprint.mixin(commandBlueprint, BP_DemoSquadCommand_Squad, {
    objectTakeByNative: true,
    inherit: false,
});

console.log("[SquadSetup] Command lifecycle mixin bound: " + commandBlueprint.StaticClass().GetName());
