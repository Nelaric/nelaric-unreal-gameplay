// Copyright (c) 2026 Nelaric Contributors

import { BattlefrontSnapshot } from "../../AI/Company/BattlefrontObjectives";
export interface AreaDefinition { id: string; center: { x: number; y: number; z: number }; radius: number; halfHeight: number; }
export interface RegionDefinition {
    id: string; seconds: number; points: AreaDefinition[]; defenderFallbackSpawn: AreaDefinition;
}
export interface MatchDefinition {
    version: 1; attackerTeamId: number; defenderTeamId: number; preparingSeconds: number; transitionSeconds: number;
    respawnSeconds: number; deploymentTimeout: number; attackerCount: number; defenderCount: number; squadsPerPlatoon: number; membersPerSquad: number;
    initialAttackerSpawn: AreaDefinition; regions: RegionDefinition[];
}
export type Counts = Record<string, { attack: number; defend: number }>;
export interface MatchSnapshot extends BattlefrontSnapshot { reason: string; stageEndsAt: number; observationGaps: number; }
const id = (s: unknown): s is string => typeof s === "string" && /^[A-Za-z0-9_-]{1,48}$/.test(s);
const positive = (n: number): boolean => Number.isFinite(n) && n > 0;
export function parseDefinition(text: string): MatchDefinition {
    const d: MatchDefinition = JSON.parse(text);
    if (!d || d.version !== 1 || ![d.attackerTeamId, d.defenderTeamId].every(t => Number.isInteger(t) && t >= 0 && t < 255) ||
        d.attackerTeamId === d.defenderTeamId || ![d.preparingSeconds, d.transitionSeconds, d.respawnSeconds, d.deploymentTimeout].every(positive) ||
        ![d.attackerCount, d.defenderCount, d.squadsPerPlatoon, d.membersPerSquad].every(n => Number.isInteger(n) && n > 0) ||
        Math.ceil(Math.max(d.attackerCount, d.defenderCount) / (d.squadsPerPlatoon * d.membersPerSquad)) > 8 || d.squadsPerPlatoon > 4 || d.membersPerSquad > 16 ||
        d.attackerCount + d.defenderCount > 512 ||
        !Array.isArray(d.regions) || d.regions.length !== 4) throw new Error("Invalid match definition");
    const names = new Set<string>(), regions = new Set<string>(), captureAreas: AreaDefinition[] = [];
    for (const r of d.regions) {
        if (!r || !id(r.id) || regions.has(r.id) || !positive(r.seconds) || r.seconds > 86400 ||
            !Array.isArray(r.points) || !r.points.length || r.points.length > 64) throw new Error("Invalid region");
        regions.add(r.id);
        for (const a of [...r.points, r.defenderFallbackSpawn, ...(r === d.regions[0] ? [d.initialAttackerSpawn] : [])]) {
            if (!a || !id(a.id) || names.has(a.id) || !a.center || !Object.values(a.center).every(Number.isFinite) ||
                ![a.center.x, a.center.y, a.center.z].every(Number.isFinite) || !positive(a.radius) || a.radius < 100 ||
                !positive(a.halfHeight) || a.halfHeight < 100) throw new Error("Invalid or duplicate area");
            if (r.points.includes(a)) {
                if (captureAreas.some(b => Math.hypot(a.center.x-b.center.x,a.center.y-b.center.y) < a.radius+b.radius &&
                    Math.abs(a.center.z-b.center.z) < a.halfHeight+b.halfHeight)) throw new Error("Overlapping capture areas");
                captureAreas.push(a);
            }
            names.add(a.id);
        }
    }
    return d;
}
/** Deterministic authority rules. Counts are a single current spatial observation. */
export class GrandWarfrontRules {
    readonly state: MatchSnapshot;
    private startedAt = 0;
    private scoredSecond = 0;
    constructor(readonly definition: MatchDefinition, matchId: string, now: number) {
        const r = definition.regions[0];
        this.state = { matchId, sequence: 1, observedAt: now, phase: "Preparing", regionId: r.id, regionIndex: 0, regionRevision: 1,
            attackerTeamId: definition.attackerTeamId, defenderTeamId: definition.defenderTeamId, deadline: 0,
            attackerSpawnAreaId: definition.initialAttackerSpawn.id, defenderSpawnAreaId: r.points[0].id, points: [], winnerTeamId: 255,
            reason: "", stageEndsAt: now + definition.preparingSeconds, observationGaps: 0 };
        this.resetPoints();
    }
    get terminal(): boolean { return this.state.phase === "Finished" || this.state.phase === "Aborted"; }
    get region(): RegionDefinition { return this.definition.regions[this.state.regionIndex]; }
    get sampleDue(): number { return this.startedAt + this.scoredSecond + 1; }
    /** Legal spawn areas from the committed scores; callers may shuffle this fresh array. */
    getSpawnCandidates(team: number): string[] {
        const s = this.state, d = this.definition;
        if (team === s.attackerTeamId) {
            const rear = s.regionIndex === 0 ? [d.initialAttackerSpawn.id]
                : d.regions[s.regionIndex - 1].points.map(p => p.id);
            const controlled = s.points.filter(p => p.ownerTeamId === s.attackerTeamId && p.attackScore === 60).map(p => p.areaId);
            return [...rear, ...controlled];
        }
        if (team !== s.defenderTeamId) return [];
        const controlled = s.points.filter(p => p.ownerTeamId === s.defenderTeamId && p.attackScore === 0).map(p => p.areaId);
        return controlled.length ? controlled : [this.region.defenderFallbackSpawn.id];
    }
    private resetPoints(): void {
        const s = this.state;
        s.points = this.region.points.map(a => ({ id: a.id, areaId: a.id, attackScore: 0, ownerTeamId: s.defenderTeamId, pressureTeamId: 255, attackers: 0, defenders: 0 }));
    }
    abort(reason: string, now: number): void {
        if (this.terminal) return;
        this.state.phase = "Aborted"; this.state.reason = reason; this.state.winnerTeamId = 255; this.touch(now);
    }
    private touch(now: number): void { this.state.observedAt = now; ++this.state.sequence; }
    tick(now: number, counts: Counts | undefined, deploymentReady: boolean): void {
        const s = this.state;
        if (this.terminal || !Number.isFinite(now) || now < s.observedAt) return;
        if (s.phase !== "RegionActive") {
            if (now >= s.stageEndsAt && deploymentReady) {
                s.phase = "RegionActive"; this.startedAt = now; this.scoredSecond = 0; s.deadline = now + this.region.seconds;
            } else if (now >= s.stageEndsAt + this.definition.deploymentTimeout) this.abort("DeploymentTimeout", now);
            this.touch(now); return;
        }
        const last = Math.floor(Math.min(now, s.deadline) - this.startedAt + 1e-6);
        if (last > this.scoredSecond) {
            const skipped = last - this.scoredSecond - 1;
            s.observationGaps += skipped;
            this.scoredSecond = last;
            // Do not manufacture historical observations after a stalled game thread.
            if (now - (this.startedAt + last) <= 0.25 && counts) {
                if (s.points.some(p => !counts[p.areaId] || ![counts[p.areaId].attack, counts[p.areaId].defend]
                    .every(n => Number.isSafeInteger(n) && n >= 0 && n <= 512))) { this.abort("InvalidSensorSample", now); return; }
                for (const p of s.points) {
                    const c = counts[p.areaId], direction = Math.sign(c.attack-c.defend);
                    p.attackers = c.attack; p.defenders = c.defend;
                    p.pressureTeamId = direction > 0 ? s.attackerTeamId : direction < 0 ? s.defenderTeamId : 255;
                    p.attackScore = Math.max(0, Math.min(60, p.attackScore + direction));
                    if (p.attackScore === 60) p.ownerTeamId = s.attackerTeamId;
                    else if (p.attackScore === 0) p.ownerTeamId = s.defenderTeamId;
                }
            } else ++s.observationGaps;
        }
        s.defenderSpawnAreaId = this.getSpawnCandidates(s.defenderTeamId)[0];
        // Commit every point before evaluating the all-owned condition or the clock.
        if (s.points.every(p => p.ownerTeamId === s.attackerTeamId)) {
            if (s.regionIndex === this.definition.regions.length - 1) { s.phase = "Finished"; s.winnerTeamId = s.attackerTeamId; s.reason = "AllRegionsCaptured"; }
            else {
                ++s.regionIndex; ++s.regionRevision; s.regionId = this.region.id; s.phase = "RegionTransition";
                s.attackerSpawnAreaId = this.definition.regions[s.regionIndex - 1].points[0].id; s.defenderSpawnAreaId = this.region.points[0].id;
                s.deadline = 0; s.stageEndsAt = now + this.definition.transitionSeconds; this.resetPoints();
            }
        } else if (now >= s.deadline) { s.phase = "Finished"; s.winnerTeamId = s.defenderTeamId; s.reason = "RegionTimeout"; }
        this.touch(now);
    }
}
