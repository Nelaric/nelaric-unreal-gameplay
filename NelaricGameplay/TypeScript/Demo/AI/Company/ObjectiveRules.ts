// Copyright (c) 2026 Nelaric Contributors

import { clone, fresh, FCompanyMission, FCompanyObjective, FObjectiveResult, RuleInputs, validateMission } from "./CommandContracts";

interface Runtime { held: number; lostAt: number; lastAt: number; checkpoint: number; completion: boolean; failed: boolean;
    participants?: string[]; covered?: string[]; conditionVersion?: number }
/** Rule authority. It evaluates world predicates and never submits a command. */
export class ObjectiveRules {
    private runtime: Record<string, Runtime> = {};
    private results: Record<string, FObjectiveResult> = {};
    private mission?: FCompanyMission;
    private sequence = 0;
    constructor(private readonly teamId: number, private readonly maxAge = 3) {}

    configure(mission: FCompanyMission): boolean {
        if (validateMission(mission)) return false;
        if (this.mission?.id !== mission.id || this.mission.revision !== mission.revision) {
            this.runtime = {}; this.results = {};
        }
        this.mission = clone(mission);
        for (const o of mission.objectives) this.runtime[o.id] ??= {
            held: 0, lostAt: -1, lastAt: -1, checkpoint: 0, completion: false, failed: false };
        return true;
    }
    /** Freezes the required participant manifest; casualties never shrink it. */
    bindParticipants(id: string, identities: string[]): boolean {
        const o = this.mission?.objectives.find(o => o.id === id), r = this.runtime[id];
        if (!o || !r || !identities.length || identities.length > 512 || new Set(identities).size !== identities.length) return false;
        if (r.participants) return true;
        r.participants = [...identities].sort(); return true;
    }

    evaluate(now: number, inputs: RuleInputs): Record<string, FObjectiveResult> {
        const mission = this.mission;
        if (!mission) return {};
        const evaluated: Record<string, FObjectiveResult> = {};
        const objectives = new Map(mission.objectives.map(o => [o.id, o]));
        const evaluate = (o: FCompanyObjective): FObjectiveResult => {
            if (evaluated[o.id]) return evaluated[o.id];
            const r = o.rule, state = this.runtime[o.id];
            let known = true, condition = false, progress = 0;
            if (r.type === "All" || r.type === "Any") {
                const children = r.children.map(id => evaluate(objectives.get(id)!));
                const passes = (v: FObjectiveResult) => v.evaluationState === "Valid" &&
                    (objectives.get(v.id)!.rule.continuous ? v.currentConditionValid : v.completionRecorded);
                condition = r.type === "All" ? children.every(passes) : children.some(passes);
                known = condition || children.every(v => v.evaluationState === "Valid");
                progress = children.filter(passes).length / children.length;
            } else if (r.type === "Escort") {
                const units = r.participants.map(id => inputs.units[id]);
                known = units.every(u => !!u && u.known && fresh(u.observedAt, now, this.maxAge));
                if (units.some(u => u?.known && (!u.alive || u.teamId !== this.teamId))) state.failed = true;
                const checkpoint = r.checkpoints[state.checkpoint];
                const area = checkpoint ? inputs.areas[checkpoint] : undefined;
                const required = Math.ceil(r.participants.length * r.requiredFraction);
                if (known && area?.known && fresh(area.observedAt, now, this.maxAge) &&
                    r.participants.filter(id => area.presentIds.includes(id)).length >= required) ++state.checkpoint;
                condition = known && !state.failed && state.checkpoint >= r.checkpoints.length;
                progress = state.checkpoint / r.checkpoints.length;
            } else if (r.type === "Search") {
                const areas = (r.checkpoints.length ? r.checkpoints : [r.areaId]).map(id => inputs.areas[id]);
                known = areas.every(a => !!a && a.known && fresh(a.observedAt, now, this.maxAge));
                const scope = mission.id + "/" + mission.revision + "/" + o.id;
                const ids = r.checkpoints.length ? r.checkpoints : [r.areaId];
                state.covered ??= [];
                for (let i = 0; i < areas.length; ++i) {
                    const a = areas[i];
                    const evidence = a?.searchCounts ? (a.searchCounts[scope] ?? 0) >= o.minimumPlatoons :
                        a?.searchScopes ? a.searchScopes.includes(scope) : a?.searched;
                    if (a?.known && fresh(a.observedAt, now, this.maxAge) && evidence && !state.covered.includes(ids[i])) state.covered.push(ids[i]);
                }
                condition = known && ids.every(id => state.covered!.includes(id));
                progress = ids.filter(id => state.covered!.includes(id)).length / ids.length;
            } else {
                const area = inputs.areas[r.areaId];
                known = !!area && area.known && fresh(area.observedAt, now, this.maxAge);
                if (known) {
                    const participants = r.participants.length ? r.participants : state.participants ?? [];
                    const count = participants.length ? participants.filter(id => area.presentIds.includes(id)).length : area.presentIds.length;
                    const required = Math.max(r.minimumPresent, Math.ceil(participants.length * r.requiredFraction));
                    condition = count >= required && (r.type !== "Regroup" || area.gathered) &&
                        ((r.type !== "Capture" && r.type !== "Maintain") || !area.contested);
                    progress = Math.min(1, count / required);
                }
            }
            const delta = state.lastAt < 0 ? 0 : Math.max(0, Math.min(now - state.lastAt, this.maxAge));
            // Unknown intervals never accrue hold time and break the continuous evidence chain.
            if (known && condition) {
                if (state.lostAt >= 0 && now - state.lostAt > r.graceSeconds) state.held = 0;
                if (state.lastAt >= 0 && this.results[o.id]?.currentConditionValid) state.held += delta;
                state.lostAt = -1;
                if (state.held >= r.holdSeconds && !state.failed) state.completion = true;
            } else {
                if (state.lostAt < 0) state.lostAt = now;
                if (!known || now - state.lostAt >= r.graceSeconds) state.held = 0;
            }
            state.lastAt = now;
            const expired = o.mission.deadline > 0 && now >= o.mission.deadline && !state.completion;
            if (expired) state.failed = true;
            const result: FObjectiveResult = { id: o.id, completionRecorded: state.completion,
                currentConditionValid: known && condition && !state.failed,
                evaluationState: known ? "Valid" : "Unknown", state: state.failed ? "Failed" : state.completion ? "Succeeded" : "Running",
                progress: state.completion ? 1 : r.holdSeconds > 0 ? Math.min(progress, state.held / r.holdSeconds) : progress,
                resultSequence: ++this.sequence, evaluatedAt: now };
            const previous = this.results[o.id];
            if (!previous || previous.completionRecorded !== result.completionRecorded ||
                previous.currentConditionValid !== result.currentConditionValid || previous.evaluationState !== result.evaluationState ||
                previous.state !== result.state) state.conditionVersion = (state.conditionVersion ?? 0) + 1;
            result.conditionVersion = state.conditionVersion ?? 1;
            evaluated[o.id] = result;
            return result;
        };
        for (const objective of mission.objectives) evaluate(objective);
        this.results = evaluated;
        const published: Record<string, FObjectiveResult> = {};
        for (const o of mission.objectives) if (o.rule.publicResult) published[o.id] = clone(evaluated[o.id]);
        return published;
    }

    /** Mission settlement is also decided here, including private rule predicates. */
    outcome(now: number): "Running" | "Succeeded" | "Failed" {
        if (!this.mission) return "Running";
        if (this.mission.failureCriteria?.some(id => this.results[id]?.evaluationState === "Valid" &&
            this.results[id].currentConditionValid)) return "Failed";
        const required = this.mission.objectives.filter(o => this.mission!.successCriteria?.length ?
            this.mission!.successCriteria.includes(o.id) : o.mandatory);
        if (required.some(o => this.results[o.id]?.state === "Failed")) return "Failed";
        if (required.length && required.every(o => {
            const r = this.results[o.id];
            return r?.completionRecorded && (!o.rule.continuous || (r.evaluationState === "Valid" && r.currentConditionValid));
        })) return "Succeeded";
        return this.mission.deadline > 0 && now >= this.mission.deadline ? "Failed" : "Running";
    }

    save(): { runtime: Record<string, Runtime>; sequence: number } { return clone({ runtime: this.runtime, sequence: this.sequence }); }
    restore(saved: ReturnType<ObjectiveRules["save"]>): boolean {
        if (!this.mission || !saved || !Number.isInteger(saved.sequence) || saved.sequence < 0) return false;
        for (const o of this.mission.objectives) {
            const s = saved.runtime?.[o.id];
            if (!s || !Number.isFinite(s.held) || s.held < 0 || !Number.isInteger(s.checkpoint) ||
                s.checkpoint < 0 || s.checkpoint > o.rule.checkpoints.length || typeof s.completion !== "boolean" ||
                typeof s.failed !== "boolean" || (s.participants && (!Array.isArray(s.participants) || s.participants.length > 512)) ||
                (s.covered && (!Array.isArray(s.covered) || s.covered.some(id => ![o.rule.areaId, ...o.rule.checkpoints].includes(id))))) return false;
        }
        this.runtime = clone(saved.runtime); this.sequence = saved.sequence; this.results = {};
        // Resume accrued progress only after fresh evidence; offline time never contributes.
        for (const s of Object.values(this.runtime)) { s.lastAt = -1; s.lostAt = -1; }
        return true;
    }
}
