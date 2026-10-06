// Copyright (c) 2026 Nelaric Contributors

import * as UE from "ue";
import { blueprint } from "puerts";
import TS_SquadBootstrap from "./TS_SquadBootstrap";

const runtimeClass = UE.Class.Load(
    "/Game/Demo/Demo1_GrandWarfront/AI/Squad/BP_SquadBootstrapRuntime.BP_SquadBootstrapRuntime_C"
);
if (!UE.KismetSystemLibrary.IsValid(runtimeClass)) {
    throw new Error("Squad initialization Blueprint is unavailable.");
}
const runtimeBlueprint = blueprint.tojs<typeof UE.Actor>(runtimeClass);

class BP_SquadBootstrapRuntime extends runtimeBlueprint {
    StartSquad(): void {
        TS_SquadBootstrap.prototype.StartSquad.call(this as unknown as TS_SquadBootstrap);
    }
    PollMembers(): void {
        TS_SquadBootstrap.prototype.PollMembers.call(this as unknown as TS_SquadBootstrap);
    }
    StopSquad(): void {
        TS_SquadBootstrap.prototype.StopSquad.call(this as unknown as TS_SquadBootstrap);
    }
}

blueprint.mixin(runtimeBlueprint, BP_SquadBootstrapRuntime, {
    objectTakeByNative: true,
    inherit: false,
});
