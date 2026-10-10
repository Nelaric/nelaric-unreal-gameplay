// Copyright (c) 2026 Nelaric Contributors

import { clone, equivalent, finite, fresh, FCompanyMission, FCompanyObjective, FObjectiveResult, TacticalArea } from "./CommandContracts";
import { CompanyCoordinator } from "./CompanyCoordinator";

export interface BattlefrontPoint {
    id: string; areaId: string; ownerTeamId: number; attackScore: number;
    pressureTeamId: number; attackers?: number; defenders?: number;
}
/** Public rule facts, never hostile positions or a second capture evaluator. */
export interface BattlefrontSnapshot {
    matchId: string; sequence: number; observedAt: number;
    phase: "Preparing" | "RegionActive" | "RegionTransition" | "Finished" | "Aborted";
    regionId: string; regionIndex: number; regionRevision: number;
    attackerTeamId: number; defenderTeamId: number; deadline: number;
    attackerSpawnAreaId: string; defenderSpawnAreaId: string;
    points: BattlefrontPoint[]; winnerTeamId: number;
}
const identity = (value: unknown): value is string => typeof value === "string" && /^[A-Za-z0-9_-]{1,48}$/.test(value);
const areaId = (value: unknown): value is string => typeof value === "string" && value.length > 0 && value.length <= 128;
const team = (value: unknown): value is number => Number.isInteger(value) && Number(value) >= 0 && Number(value) < 255;
const terminal = (s: BattlefrontSnapshot): boolean => s.phase === "Finished" || s.phase === "Aborted";

/** One session per authority world, shared by both company adapters. */
export class BattlefrontSession {
    private state?: BattlefrontSnapshot;
    read(): BattlefrontSnapshot | undefined { return this.state && clone(this.state); }
    publish(text: string, now: number, knownAreas: ReadonlySet<string>): boolean {
        if (text.length > 65536 || !finite(now)) return false;
        try {
            const s: BattlefrontSnapshot = JSON.parse(text);
            if (!s || !identity(s.matchId) || !identity(s.regionId) || !Number.isSafeInteger(s.sequence) || s.sequence < 1 ||
                !Number.isSafeInteger(s.regionRevision) || s.regionRevision < 1 || !Number.isInteger(s.regionIndex) ||
                s.regionIndex < 0 || s.regionIndex > 3 || !fresh(s.observedAt, now, 3) ||
                !team(s.attackerTeamId) || !team(s.defenderTeamId) || s.attackerTeamId === s.defenderTeamId ||
                !["Preparing", "RegionActive", "RegionTransition", "Finished", "Aborted"].includes(s.phase) ||
                !finite(s.deadline) || s.deadline < 0 || (s.phase === "RegionActive" && s.deadline <= 0) ||
                !areaId(s.attackerSpawnAreaId) || !areaId(s.defenderSpawnAreaId) || s.attackerSpawnAreaId === s.defenderSpawnAreaId ||
                !Array.isArray(s.points) || s.points.length < 1 || s.points.length > 64 ||
                new Set(s.points.map(p => p?.id)).size !== s.points.length ||
                new Set(s.points.map(p => p?.areaId)).size !== s.points.length ||
                s.points.some(p => !p || !identity(p.id) || !areaId(p.areaId) ||
                    ![s.attackerTeamId, s.defenderTeamId].includes(p.ownerTeamId) ||
                    ![s.attackerTeamId, s.defenderTeamId, 255].includes(p.pressureTeamId) ||
                    !Number.isInteger(p.attackScore) || p.attackScore < 0 || p.attackScore > 60 ||
                    (p.attackScore === 60 && p.ownerTeamId !== s.attackerTeamId) ||
                    (p.attackScore === 0 && p.ownerTeamId !== s.defenderTeamId)) ||
                (s.phase === "Finished" ? ![s.attackerTeamId, s.defenderTeamId].includes(s.winnerTeamId) : s.winnerTeamId !== 255)) return false;
            if (![s.attackerSpawnAreaId, s.defenderSpawnAreaId, ...s.points.map(p => p.areaId)].every(id => knownAreas.has(id))) return false;
            s.points.sort((a, b) => a.id.localeCompare(b.id));
            const previous = this.state;
            if (previous) {
                if (s.matchId !== previous.matchId) {
                    if (!terminal(previous) || s.phase !== "Preparing" || s.regionIndex !== 0 || s.sequence !== 1) return false;
                } else {
                    if (s.sequence <= previous.sequence) return equivalent(s, previous);
                    if (terminal(previous) || s.observedAt < previous.observedAt ||
                        s.attackerTeamId !== previous.attackerTeamId || s.defenderTeamId !== previous.defenderTeamId ||
                        s.regionIndex < previous.regionIndex || s.regionIndex > previous.regionIndex + 1 ||
                        s.regionRevision < previous.regionRevision ||
                        (s.regionIndex !== previous.regionIndex && s.regionRevision <= previous.regionRevision) ||
                        (s.regionIndex === previous.regionIndex && s.regionId !== previous.regionId) ||
                        (s.phase === "Preparing" && previous.phase !== "Preparing")) return false;
                    if (s.regionRevision === previous.regionRevision &&
                        (s.regionId !== previous.regionId || s.attackerSpawnAreaId !== previous.attackerSpawnAreaId ||
                        !equivalent(s.points.map(p => [p.id, p.areaId]), previous.points.map(p => [p.id, p.areaId])))) return false;
                }
            }
            this.state = clone(s); return true;
        } catch { return false; }
    }
}

/** Projects rule-owned current control into persistent company responsibilities. */
export class BattlefrontCompanyAdapter {
    private scope = "";
    private paused = false;
    private history = new Set<string>();
    private conditions = new Map<string, { owned: boolean; version: number }>();
    constructor(private readonly core: CompanyCoordinator) {}

    update(s: BattlefrontSnapshot, now: number): void {
        const c = this.core, participant = [s.attackerTeamId, s.defenderTeamId].includes(c.teamId);
        const scope = `${s.matchId}/${s.regionRevision}/${s.phase}`;
        const unavailable = !participant || (!terminal(s) && !fresh(s.observedAt, now, 3)) ||
            (s.phase === "RegionActive" && now >= s.deadline) ||
            !this.availableAreas(s);
        if (scope !== this.scope || unavailable !== this.paused) {
            c.endObjectiveScope(); this.history.clear(); this.conditions.clear();
            this.scope = scope; this.paused = unavailable;
        }
        if (terminal(s)) {
            c.endObjectiveScope(s.phase === "Aborted" ? "Cancelled" : s.winnerTeamId === c.teamId ? "Succeeded" : "Failed");
            c.context.message = `Battlefront ${s.phase}: ${s.matchId}`;
            return;
        }
        if (unavailable || s.phase === "RegionTransition") {
            c.context.message = unavailable ? "Waiting for fresh valid battlefront objectives." : "Waiting for region deployment.";
            return;
        }
        const objectives = this.objectives(s);
        const mission: FCompanyMission = { id: `Front:${s.matchId}:${s.regionRevision}:${c.teamId}`,
            revision: 1, objectives, deadline: 0, strategicPolicy: c.teamId === s.attackerTeamId ? "Advance" : "Maintain",
            fallbackPolicy: "Hold" };
        // The rule authority owns timeouts and victory. AI mission completion never advances a region.
        if (!c.context.mission) { if (!c.submit(mission)) return; }
        else if (!c.updateObjectives(objectives)) return;
        if (s.phase === "Preparing") return;
        const results: Record<string, FObjectiveResult> = {};
        for (const p of s.points) {
            const id = "Point:" + p.id, owned = p.ownerTeamId === c.teamId;
            if (owned) this.history.add(id);
            const previous = this.conditions.get(id);
            const version = (previous?.version ?? 0) + (!previous || previous.owned !== owned ? 1 : 0);
            this.conditions.set(id, { owned, version });
            results[id] = { id, completionRecorded: this.history.has(id), currentConditionValid: owned,
                evaluationState: "Valid", state: owned ? "Succeeded" : "Running",
                progress: (c.teamId === s.attackerTeamId ? p.attackScore : 60 - p.attackScore) / 60,
                resultSequence: s.sequence, evaluatedAt: s.observedAt, conditionVersion: version };
        }
        c.publishRuleResults(results, "Running");
    }

    private availableAreas(s: BattlefrontSnapshot): boolean {
        return [s.attackerSpawnAreaId, s.defenderSpawnAreaId, ...s.points.map(p => p.areaId)]
            .every(id => this.core.areas.has(id));
    }
    private objectives(s: BattlefrontSnapshot): FCompanyObjective[] {
        const teamId = this.core.teamId;
        const make = (id: string, area: TacticalArea, type: "Regroup" | "SecureArea" | "Defend", priority: number,
            mandatory: boolean): FCompanyObjective => ({ id, priority, mandatory, minimumPlatoons: 1, minimumMobile: 1,
            dependencies: [], mission: { id: "", revision: 1, type, areaId: area.id, goal: clone(area.goal),
                preparationAreaId: "", deadline: 0, engagement: type === "Regroup" ? "HoldFire" : "FireAtWill",
                hasBoundary: false, boundary: clone(area.goal), minimumMobile: 1, minimumAmmo: type === "Regroup" ? 0 : 0.1 },
            rule: { type: type === "Regroup" ? "Regroup" : "Maintain", areaId: area.id, minimumPresent: 1,
                requiredFraction: 1, participants: [], holdSeconds: 0, graceSeconds: 0, children: [], checkpoints: [],
                continuous: true, publicResult: true } });
        if (s.phase === "Preparing") {
            const spawn = teamId === s.attackerTeamId ? s.attackerSpawnAreaId : s.defenderSpawnAreaId;
            const objective = make("Deployment", this.core.areas.get(spawn)!, "Regroup", 100, true);
            objective.minimumPlatoons = Math.max(1, this.core.ports.size);
            return [objective];
        }
        // Keep one defending anchor; other garrisons may reinforce more valuable objectives.
        const anchor = teamId === s.defenderTeamId ? s.points.find(p => p.ownerTeamId === teamId)?.id : undefined;
        const targets = s.points.map(p => {
            const owned = p.ownerTeamId === teamId, underPressure = owned && p.pressureTeamId !== 255 && p.pressureTeamId !== teamId;
            return { point: p, owned, underPressure, weight: owned ? (underPressure ? 2 : 1) : 4, platoons: 1 };
        }).sort((a, b) => a.point.id.localeCompare(b.point.id));
        const budget = Math.max(targets.length, this.core.ports.size), weight = targets.reduce((sum, t) => sum + t.weight, 0);
        // Preserve a garrison per point, then round weighted shares without leaving spare platoons idle.
        for (let assigned = targets.length; assigned < budget; ++assigned) {
            const next = targets.reduce((best, t) => budget * t.weight / weight - t.platoons >
                budget * best.weight / weight - best.platoons ? t : best);
            ++next.platoons;
        }
        return targets.map(t => {
            const priority = !t.owned ? 220 : t.underPressure ? 180 : teamId === s.defenderTeamId ? 100 : 30;
            const objective = make("Point:" + t.point.id, this.core.areas.get(t.point.areaId)!,
                t.owned ? "Defend" : "SecureArea", priority, t.point.id === anchor);
            objective.minimumPlatoons = t.platoons;
            return objective;
        });
    }
}
