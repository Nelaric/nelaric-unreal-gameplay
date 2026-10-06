// Copyright (c) 2026 Nelaric Contributors

import * as UE from "ue";
import { blueprint } from "puerts";
import { issueMission, selectedSquad } from "./SquadMissions";

type PlayerBlueprint = UE.Game.Demo.Demo1_GrandWarfront.Player.BP_DemoPlayerController.BP_DemoPlayerController_C;
const controllerClass = UE.Class.Load(
    "/Game/Demo/Demo1_GrandWarfront/Player/BP_DemoPlayerController.BP_DemoPlayerController_C"
);
if (!UE.KismetSystemLibrary.IsValid(controllerClass)) {
    throw new Error("Squad command player-controller Blueprint is unavailable.");
}
const controllerBlueprint = blueprint.tojs<typeof UE.Game.Demo.Demo1_GrandWarfront.Player.BP_DemoPlayerController.BP_DemoPlayerController_C>(controllerClass);

function report(controller: PlayerBlueprint, accepted: boolean, operation: string): void {
    controller.SquadCommandAccepted = accepted;
    controller.SquadCommandMessage = accepted ? operation + " accepted." : operation + " rejected.";
    if (accepted) {
        console.log("[SquadCommand] " + controller.SquadCommandMessage);
    } else {
        console.warn("[SquadCommand] " + controller.SquadCommandMessage);
    }
}

function submit(controller: PlayerBlueprint, type: UE.EDemoSquadMissionType): void {
    const squad = selectedSquad(controller);
    if (!squad || !squad.HasAuthority()) {
        report(controller, false, "Squad mission (select or control a squad member)");
        return;
    }
    squad.GetSquadContext().SetCommandMode(UE.EDemoSquadCommandMode.PlayerAssisted);
    const engagement = type === UE.EDemoSquadMissionType.Withdraw || type === UE.EDemoSquadMissionType.Regroup
        ? UE.EDemoSquadEngagement.SelfDefense : UE.EDemoSquadEngagement.FireAtWill;
    report(controller, issueMission(squad, type, controller.SquadTargetLocation,
        controller.SquadTargetRadius, controller.SquadTargetFacing, engagement), "Squad mission");
}

function changeMode(controller: PlayerBlueprint, mode: UE.EDemoSquadCommandMode): void {
    const squad = selectedSquad(controller);
    if (!squad || !squad.HasAuthority()) {
        report(controller, false, "Command mode");
        return;
    }
    squad.GetSquadContext().SetCommandMode(mode);
    report(controller, true, "Command mode");
}

/** The existing controller retains camera, input and possession behavior. */
class BP_DemoPlayerController_Squad extends controllerBlueprint {
    SquadMove(): void { submit(this, UE.EDemoSquadMissionType.Move); }
    SquadDefend(): void { submit(this, UE.EDemoSquadMissionType.Defend); }
    SquadControl(): void { submit(this, UE.EDemoSquadMissionType.Control); }
    SquadWithdraw(): void { submit(this, UE.EDemoSquadMissionType.Withdraw); }
    SquadRegroup(): void { submit(this, UE.EDemoSquadMissionType.Regroup); }
    SquadCancel(): void { submit(this, UE.EDemoSquadMissionType.None); }
    SquadUseAutonomous(): void { changeMode(this, UE.EDemoSquadCommandMode.Autonomous); }
    SquadUseAssisted(): void { changeMode(this, UE.EDemoSquadCommandMode.PlayerAssisted); }
    SquadUseManual(): void { changeMode(this, UE.EDemoSquadCommandMode.PlayerManual); }

    SquadManualEngage(): void {
        const squad = selectedSquad(this);
        report(this, !!squad && squad.HasAuthority() &&
            squad.GetSquadContext().SetManualTactic(UE.EDemoSquadTactic.Engage), "Manual engage");
    }

    SquadManualDefend(): void {
        const squad = selectedSquad(this);
        report(this, !!squad && squad.HasAuthority() &&
            squad.GetSquadContext().SetManualTactic(UE.EDemoSquadTactic.Defend), "Manual defend");
    }
}

blueprint.mixin(controllerBlueprint, BP_DemoPlayerController_Squad, {
    objectTakeByNative: true,
    inherit: false,
});
