// Copyright (c) 2026 Nelaric Contributors

import * as UE from "ue";
import { clone, FCompanyMission, FCompanyPlan, FObjectiveResult, RuleInputs } from "./CommandContracts";
import { ObjectiveRules } from "./ObjectiveRules";

/** World-owned rule adapter. It neither allocates platoons nor publishes orders. */
export class ObjectiveAuthority {
    private mission?: FCompanyMission;
    private readonly rules: ObjectiveRules;
    constructor(private readonly sensors: UE.DemoObjectiveWorldSubsystem, private readonly teamId: number, maxAge: number) {
        this.rules = new ObjectiveRules(teamId, maxAge);
    }
    configure(mission: FCompanyMission): boolean {
        if (!this.rules.configure(mission)) return false;
        this.mission = clone(mission); return true;
    }
    poll(now: number, plan: FCompanyPlan | undefined, sources: Map<string, UE.Actor>): {
        results: Record<string, FObjectiveResult>; outcome: "Running" | "Succeeded" | "Failed" } {
        const participants = UE.NewArray(UE.BuiltinString);
        for (const o of this.mission?.objectives ?? []) {
            for (const id of o.rule.participants) participants.Add(id);
            if (["Arrive", "Regroup"].includes(o.rule.type) && !o.rule.participants.length) {
                const assigned = Object.values(plan?.assignments ?? {}).filter(a => a.objectiveId === o.id);
                if (assigned.length >= o.minimumPlatoons) {
                    const ids: string[] = [];
                    for (const a of assigned) {
                        const source = sources.get(a.platoonId);
                        if (source) ids.push(...Array.from(this.sensors.GetPlatoonParticipants(source, this.teamId)));
                    }
                    this.rules.bindParticipants(o.id, [...new Set(ids)]);
                }
            }
        }
        const inputs = JSON.parse(this.sensors.GetRuleInputs(this.teamId, participants)) as RuleInputs;
        // An arrival cannot be confirmed using unrelated friendly soldiers before its force group exists.
        for (const o of this.mission?.objectives ?? []) if (["Arrive", "Regroup"].includes(o.rule.type) &&
            !o.rule.participants.length && !Object.values(plan?.assignments ?? {}).some(a => a.objectiveId === o.id)) {
            if (inputs.areas[o.rule.areaId]) inputs.areas[o.rule.areaId].known = false;
        }
        return { results: this.rules.evaluate(now, inputs), outcome: this.rules.outcome(now) };
    }
    save(): ReturnType<ObjectiveRules["save"]> { return this.rules.save(); }
    restore(value: ReturnType<ObjectiveRules["save"]>): boolean { return this.rules.restore(value); }
    validateRestore(mission: FCompanyMission, value: ReturnType<ObjectiveRules["save"]>): boolean {
        const probe = new ObjectiveRules(this.teamId); return probe.configure(mission) && probe.restore(value);
    }
}

const worlds = new WeakMap<UE.DemoObjectiveWorldSubsystem, Map<number, ObjectiveAuthority>>();
export function getObjectiveAuthority(sensors: UE.DemoObjectiveWorldSubsystem, teamId: number, maxAge: number): ObjectiveAuthority {
    let teams = worlds.get(sensors);
    if (!teams) { teams = new Map(); worlds.set(sensors, teams); }
    let authority = teams.get(teamId);
    if (!authority) { authority = new ObjectiveAuthority(sensors, teamId, maxAge); teams.set(teamId, authority); }
    return authority;
}
