// Copyright (c) 2026 Nelaric Contributors

import * as UE from "ue";

/** Submit an objective; the native squad executor owns its plan and orders. */
export function issueMission(
    squad: UE.DemoSquadCommandActor,
    type: UE.EDemoSquadMissionType,
    goal: UE.Vector,
    radius: number,
    facing: UE.Vector,
    engagement: UE.EDemoSquadEngagement,
    duration = 0
): boolean {
    if (!UE.KismetSystemLibrary.IsValid(squad) || !squad.HasAuthority() ||
        !Number.isFinite(radius) || radius <= 0 || !Number.isFinite(duration) || duration < 0 ||
        ![goal.X, goal.Y, goal.Z, facing.X, facing.Y, facing.Z].every(Number.isFinite)) {
        return false;
    }
    const mission = new UE.DemoSquadMission();
    mission.Type = type;
    mission.Goal = new UE.DemoSquadArea(goal, radius);
    mission.Facing = facing;
    mission.Engagement = engagement;
    mission.Deadline = duration > 0 ? UE.GameplayStatics.GetTimeSeconds(squad) + duration : 0;
    return squad.GetSquadContext().SetMission(mission);
}

/** Resolve command authority from the controlled or selected member body. */
export function selectedSquad(controller: UE.DemoPlayerController): UE.DemoSquadCommandActor | null {
    const possessed = controller.K2_GetPawn();
    const target = possessed instanceof UE.DemoCharacter ? possessed : controller.GetSelectedBot();
    if (!UE.KismetSystemLibrary.IsValid(target) || !(target instanceof UE.DemoCharacter)) {
        return null;
    }
    const member = target.GetComponentByClass(
        UE.DemoSquadMemberComponent.StaticClass()
    ) as UE.DemoSquadMemberComponent;
    if (!UE.KismetSystemLibrary.IsValid(member)) {
        return null;
    }
    const squad = member.GetSquad();
    return UE.KismetSystemLibrary.IsValid(squad) ? squad : null;
}

/** Body control uses the existing transactional player-controller API. */
export function takeControl(controller: UE.DemoPlayerController, character: UE.DemoCharacter): boolean {
    return UE.KismetSystemLibrary.IsValid(controller) &&
        UE.KismetSystemLibrary.IsValid(character) && controller.TakeControlOfBot(character);
}

export function returnControl(controller: UE.DemoPlayerController): boolean {
    return UE.KismetSystemLibrary.IsValid(controller) && controller.ReturnToOverview();
}
