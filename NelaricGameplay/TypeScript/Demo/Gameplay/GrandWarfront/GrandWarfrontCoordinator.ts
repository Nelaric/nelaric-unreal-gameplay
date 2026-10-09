// Copyright (c) 2026 Nelaric Contributors

import * as UE from "ue";
import { Counts, GrandWarfrontRules, MatchDefinition, parseDefinition } from "./GrandWarfrontRules";
import { GrandWarfrontMovement } from "./GrandWarfrontMovement";
import TS_PlatoonCommand from "../../AI/Platoon/TS_PlatoonCommand";
import TS_CompanyCommand from "../../AI/Company/TS_CompanyCommand";

const path = "/Game/Demo/Demo1_GrandWarfront/AI/";
const valid = (o: UE.Object | null | undefined): boolean => !!o && UE.KismetSystemLibrary.IsValid(o);
interface Seat {
    id: string; team: number; squad: UE.DemoSquadCommandActor; everSpawned: boolean; alive: boolean;
    remaining: number; nextAttempt: number; deployedRevision: number; waited: number; deathOrder: number;
}
/** Owns one authority match, with bounded roster and synchronous native leases. */
export class GrandWarfrontCoordinator {
    private movement: GrandWarfrontMovement;
    private definition?: MatchDefinition;
    private rules?: GrandWarfrontRules;
    private seats: Seat[] = [];
    private squads: UE.DemoSquadCommandActor[] = [];
    private platoons: TS_PlatoonCommand[] = [];
    private companies: TS_CompanyCommand[] = [];
    private actors: UE.Actor[] = [];
    private createdAt: number;
    private lastAt: number;
    private publishedAt = -Infinity;
    private started = false;
    private stopped = false;
    private ended = false;
    private ready = false;
    private deathSequence = 0;
    private reinforcementWait = new Map<number, { waited: number; nextAttempt: number }>();
    constructor(private readonly mode: UE.DemoGrandWarfrontGameMode) {
        this.movement = new GrandWarfrontMovement(mode);
        this.createdAt = this.lastAt = UE.GameplayStatics.GetTimeSeconds(mode);
        console.log("[Warfront] Authority entry reached at " + this.createdAt);
    }
    private spawn<T extends UE.Actor>(asset: string, configure: (a: T) => void): T {
        const cls = UE.Class.Load(asset), transform = new UE.Transform(new UE.Rotator(), new UE.Vector(), new UE.Vector(1, 1, 1));
        if (!valid(cls)) throw new Error("Missing command asset: " + asset);
        const actor = UE.GameplayStatics.BeginDeferredActorSpawnFromClass(this.mode, cls, transform,
            UE.ESpawnActorCollisionHandlingMethod.AlwaysSpawn, this.mode) as T;
        if (!valid(actor)) throw new Error("Cannot create command actor");
        this.actors.push(actor); configure(actor); UE.GameplayStatics.FinishSpawningActor(actor, transform); return actor;
    }
    private initialize(now: number): void {
        const d = this.definition = parseDefinition(this.mode.ReadDefinition());
        this.mode.ConfigureTeams(d.attackerTeamId, d.defenderTeamId);
        this.rules = new GrandWarfrontRules(d, UE.KismetGuidLibrary.Conv_GuidToString(UE.KismetGuidLibrary.NewGuid()).replace(/[^A-Za-z0-9]/g, ""), now);
        const initial = d.initialAttackerSpawn;
        if (!this.mode.CreateArea(initial.id, new UE.Vector(initial.center.x, initial.center.y, initial.center.z),
            initial.radius, initial.halfHeight, d.attackerTeamId)) throw new Error("Invalid initial spawn navigation");
        for (const r of d.regions) for (const a of r.points) {
            if (!this.mode.CreateArea(a.id, new UE.Vector(a.center.x, a.center.y, a.center.z), a.radius, a.halfHeight, -1) ||
                !this.mode.CreatePointSpawn(a.id)) throw new Error("Invalid point navigation or spawn: " + a.id);
        }
        for (const r of d.regions) {
            const a = r.defenderFallbackSpawn;
            if (!this.mode.CreateArea(a.id, new UE.Vector(a.center.x, a.center.y, a.center.z),
                a.radius, a.halfHeight, d.defenderTeamId)) throw new Error("Invalid defender fallback navigation: " + a.id);
        }
        const passages = new Set<string>();
        for (let i = 0; i < d.regions.length; ++i) {
            const areas = [...d.regions[i].points, d.regions[i].defenderFallbackSpawn, ...(i === 0 ? [initial] : d.regions[i - 1].points)];
            for (const a of areas) for (const b of areas) if (a !== b) {
                const key = a.id + ">" + b.id;
                if (passages.has(key)) continue;
                if (!this.mode.ConnectAreas(a.id, b.id)) throw new Error("Invalid passage");
                passages.add(key);
            }
        }
        this.mode.SetActiveSpawnAreas(this.rules.state.attackerSpawnAreaId, this.rules.state.defenderSpawnAreaId);
        for (const team of [d.attackerTeamId, d.defenderTeamId]) {
            const companyPlatoons: TS_PlatoonCommand[] = [];
            const teamCount = team === d.attackerTeamId ? d.attackerCount : d.defenderCount;
            const platoonCount = Math.ceil(teamCount / (d.squadsPerPlatoon * d.membersPerSquad));
            for (let pi = 0; pi < platoonCount; ++pi) {
                const platoonSquads: UE.DemoSquadCommandActor[] = [];
                for (let si = 0; si < d.squadsPerPlatoon; ++si) {
                    const firstSeat = (pi * d.squadsPerPlatoon + si) * d.membersPerSquad;
                    const squadCount = Math.min(d.membersPerSquad, teamCount - firstSeat);
                    if (squadCount <= 0) break;
                    const squad = this.spawn<UE.DemoSquadCommandActor>(path + "Squad/BP_DemoSquadCommand.BP_DemoSquadCommand_C", a => {
                        a.bStartOnBeginPlay = false; a.InitialMembers.Empty();
                        const definition = new UE.DemoSquadDefinition(a);
                        definition.MemberLimit = d.membersPerSquad; definition.MinimumReadySupport = 1;
                        definition.SupportGroupSize = Math.min(2, d.membersPerSquad);
                        a.GetSquadContext().Definition = definition;
                    });
                    squad.GetSquadContext().MemberOrderHandler.Bind((body, order) =>
                        body ? this.movement.resolve(body, order, this.rules!) : order);
                    this.squads.push(squad); platoonSquads.push(squad);
                    for (let mi = 0; mi < squadCount; ++mi) this.seats.push({ id: `T${team}_P${pi}_S${si}_M${mi}`,
                        team, squad, everSpawned: false, alive: false, remaining: 0, nextAttempt: 0, deployedRevision: 0, waited: 0, deathOrder: 0 });
                }
                const platoon = this.spawn<TS_PlatoonCommand>(path + "Platoon/BP_DemoPlatoonCommand.BP_DemoPlatoonCommand_C", a => {
                    a.PlatoonId = `T${team}_P${pi}`; a.TeamId = team; a.Squads.Empty();
                    for (const s of platoonSquads) a.Squads.Add(s);
                    a.MinimumMobileMembers = 1; a.MinimumAmmoReadiness = 0.1; a.MaxReportAge = 3;
                });
                companyPlatoons.push(platoon); this.platoons.push(platoon);
            }
            const company = this.spawn<TS_CompanyCommand>(path + "Company/BP_DemoCompanyCommand.BP_DemoCompanyCommand_C", a => {
                a.bStartOnBeginPlay = false;
                const definition = new UE.DemoCompanyDefinition(a); definition.CompanyId = "Team_" + team; definition.TeamId = team;
                for (const p of companyPlatoons) definition.ExpectedPlatoonIds.Add(p.PlatoonId);
                a.Definition = definition; a.Policy = new UE.DemoCompanyPolicy(a); a.InitialMission = undefined as unknown as UE.DemoCompanyMissionAsset;
                a.Platoons.Empty(); for (const p of companyPlatoons) a.Platoons.Add(p);
            });
            this.companies.push(company);
        }
        this.started = true;
        console.log("[Warfront] Created four regions and " + this.seats.length + " roster seats.");
    }
    private chooseSpawn(team: number, attempt: (areaId: string) => boolean): string | undefined {
        const candidates = this.rules!.getSpawnCandidates(team);
        // Uniform random order, with bounded fallback when a candidate is temporarily crowded.
        for (let i = candidates.length - 1; i > 0; --i) {
            const j = Math.floor(Math.random() * (i + 1));
            [candidates[i], candidates[j]] = [candidates[j], candidates[i]];
        }
        return candidates.find(attempt);
    }
    private updateSeats(now: number, delta: number): boolean {
        const rules = this.rules!, d = this.definition!, s = rules.state;
        let ready = true, budget = 8;
        for (const seat of this.seats) {
            let body = this.mode.GetSeatBody(seat.id);
            const alive = valid(body) && body.IsAlive();
            if (seat.alive && !alive) { seat.remaining = d.respawnSeconds; seat.waited = 0; seat.deployedRevision = 0; seat.deathOrder = ++this.deathSequence; }
            seat.alive = alive;
            if (!alive) {
                if (s.phase === "RegionActive") seat.remaining = Math.max(0, seat.remaining - delta);
                // Existing respawn delays remain frozen while advancing to another region.
                // Returning soldiers only enter through a complete team reinforcement batch.
                if (seat.everSpawned) continue;
                if (!seat.everSpawned) ready = false;
                if (seat.remaining > 0) continue;
                seat.waited += delta;
                if (now < seat.nextAttempt || budget <= 0) continue;
                --budget; seat.nextAttempt = now + 1;
                if (!this.mode.ReleaseDeadSeat(seat.id) || !this.chooseSpawn(seat.team, spawn => this.mode.SpawnSeat(seat.id, spawn, seat.team, seat.squad))) {
                    if (seat.waited > d.deploymentTimeout) {
                        rules.abort("SpawnSeatTimeout:" + seat.id, now); return false;
                    }
                    continue;
                }
                seat.everSpawned = seat.alive = true; seat.deployedRevision = s.regionRevision; body = this.mode.GetSeatBody(seat.id);
            }
            if (valid(body) && !body.GetPawnInitializationComponent().IsPawnInitialized()) { ready = false; continue; }
            if (seat.deployedRevision !== s.regionRevision) {
                ready = false;
                if (budget > 0 && now >= seat.nextAttempt) {
                    --budget; seat.nextAttempt = now + 0.5;
                    if (this.chooseSpawn(seat.team, spawn => this.mode.DeploySeat(seat.id, spawn))) seat.deployedRevision = s.regionRevision;
                }
            }
        }
        if (s.phase === "RegionActive") {
            // Births and region deployment retain their own budget. At most one batch per team per pulse.
            for (const team of [d.attackerTeamId, d.defenderTeamId]) {
                const eligible = this.seats.filter(seat => seat.team === team && seat.everSpawned && !seat.alive && seat.remaining <= 0)
                    .sort((a, b) => a.deathOrder - b.deathOrder);
                if (eligible.length < d.membersPerSquad) { this.reinforcementWait.delete(team); continue; }
                let wave = this.reinforcementWait.get(team);
                if (!wave) { wave = { waited: 0, nextAttempt: now }; this.reinforcementWait.set(team, wave); }
                wave.waited += delta;
                if (now < wave.nextAttempt) continue;
                wave.nextAttempt = now + 1;
                const batch = eligible.slice(0, d.membersPerSquad);
                const ids = UE.NewArray(UE.BuiltinString); for (const seat of batch) ids.Add(seat.id);
                const spawn = batch.every(seat => this.mode.ReleaseDeadSeat(seat.id))
                    ? this.chooseSpawn(team, area => this.mode.SpawnReinforcements(ids, area, team)) : undefined;
                if (!spawn) {
                    if (wave.waited > d.deploymentTimeout) { rules.abort("ReinforcementTimeout:T" + team, now); return false; }
                    continue;
                }
                for (const seat of batch) {
                    seat.alive = true; seat.remaining = 0; seat.waited = 0; seat.deployedRevision = s.regionRevision;
                }
                this.reinforcementWait.delete(team);
                console.log(`[Warfront] Reinforcements team=${team} spawn=${spawn} seats=${batch.map(seat => seat.id).join(",")}`);
            }
        }
        return ready;
    }
    tick(): void {
        if (this.stopped || this.ended) return;
        const now = UE.GameplayStatics.GetTimeSeconds(this.mode), delta = Math.max(0, now - this.lastAt); this.lastAt = now;
        try {
            // Let initial pool, components and navigation enter the play world first.
            if (!this.started) {
                if (now - this.createdAt < 2) return;
                const state = UE.GameplayStatics.GetGameState(this.mode) as UE.DemoGrandWarfrontState;
                if (!state.IsBattlefieldLoaded() || UE.NavigationSystemV1.IsNavigationBeingBuiltOrLocked(this.mode)) {
                    if (now - this.createdAt > 32) throw new Error("BattlefieldStartupTimeout");
                    return;
                }
                this.initialize(now);
            }
            const rules = this.rules!, previous = rules.state.phase, revision = rules.state.regionRevision;
            const deploymentReady = this.updateSeats(now, delta);
            if (deploymentReady && !this.ready && !rules.terminal) {
                if (this.squads.every(s => s.StartCommander()) && this.platoons.every(p => p.StartCommander()) &&
                    this.companies.every(c => c.StartCommander())) this.ready = true;
            }
            let counts: Counts | undefined;
            if (rules.state.phase === "RegionActive" && now + 1e-6 >= rules.sampleDue) {
                const ids = UE.NewArray(UE.BuiltinString); for (const p of rules.state.points) ids.Add(p.areaId);
                const sample = this.mode.SamplePoints(ids, rules.state.attackerTeamId, rules.state.defenderTeamId);
                if (!sample) rules.abort("SensorUnavailable", now); else counts = JSON.parse(sample);
            }
            rules.tick(now, counts, deploymentReady && this.ready);
            if (now - this.publishedAt >= 0.5 || previous !== rules.state.phase || rules.terminal)
                this.movement.update(this.seats, rules);
            if (previous !== rules.state.phase || revision !== rules.state.regionRevision) {
                this.mode.FreezeSeats(rules.state.phase !== "RegionActive");
                if (previous === "Preparing" || revision !== rules.state.regionRevision)
                    this.mode.SetActiveSpawnAreas(rules.state.attackerSpawnAreaId, rules.state.defenderSpawnAreaId);
                console.log(`[Warfront] ${rules.state.phase} ${rules.state.regionId} ${rules.state.reason}`);
            }
            if (now - this.publishedAt >= 0.5 || previous !== rules.state.phase || rules.terminal) {
                this.publishedAt = now;
                const snapshot = JSON.stringify(rules.state); this.mode.PublishSnapshot(snapshot);
                if (this.ready) for (const company of this.companies) if (!company.UpdateBattlefrontState(this.mode, snapshot))
                    throw new Error("Company rejected shared battlefront state");
            }
            if (rules.terminal) { this.mode.FreezeSeats(true); this.ended = true; }
        } catch (error) {
            const reason = String(error); console.error("[Warfront] " + reason);
            this.mode.FreezeSeats(true);
            this.rules?.abort(reason, now);
            const snapshot = JSON.stringify(this.rules?.state ?? { phase: "Aborted", reason, winnerTeamId: 255 });
            this.mode.PublishSnapshot(snapshot);
            for (const c of this.companies) if (valid(c)) { c.UpdateBattlefrontState(this.mode, snapshot); c.StopCommander(); }
            this.ended = true;
        }
    }
    stop(): void {
        this.stopped = true;
        for (const c of this.companies) if (valid(c)) c.StopCommander();
        for (const p of this.platoons) if (valid(p)) p.StopCommander();
        for (const s of this.squads) if (valid(s)) { s.StopCommander(); s.GetSquadContext().MemberOrderHandler.Unbind(); }
        this.movement.clear();
        this.actors = []; this.seats = []; this.reinforcementWait.clear();
    }
}
