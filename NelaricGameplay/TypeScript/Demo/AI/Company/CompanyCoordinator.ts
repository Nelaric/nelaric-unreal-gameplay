// Copyright (c) 2026 Nelaric Contributors

import { Area, Claim, clone, continuous, defaultPolicy, Dependency, equivalent, finite, fresh, FCompanyAssignment,
    FCompanyKnowledge, FCompanyMission, FCompanyObjective, FCompanyPlan, FExecutionPermit,
    FPlatoonMission, FPlatoonSituationReport, inside, Phase, PlatoonPort, Policy, TacticalArea, validateMission, validPlatoonMission } from "./CommandContracts";
import { ResourceLedger } from "./ResourceLedger";

interface CandidateGroup {
    objectiveId: string; assignments: FCompanyAssignment[]; retire: FCompanyAssignment[];
    accepted: string[]; activated: string[]; deadline: number;
    playerAuthorized?: boolean;
    objective?: FCompanyObjective;
}
/** Persistent facts, including active tasks while the tree changes workflow. */
export class CompanyContext {
    mission?: FCompanyMission;
    plan?: FCompanyPlan;
    phase: Phase = "Bootstrap";
    nextPhase: Phase = "Bootstrap";
    mode: "Autonomous" | "PlayerAssisted" | "PlayerManual" | "Suspended" = "Autonomous";
    commandEpoch = 1;
    registrationComplete = false;
    reports = new Map<string, FPlatoonSituationReport>();
    knowledge: FCompanyKnowledge = { objectiveResults: {} };
    locks = new Set<string>();
    candidates: CandidateGroup[] = [];
    repairs: Record<string, number> = {};
    affected = new Set<string>();
    outcome: "Running" | "Succeeded" | "Failed" | "Cancelled" = "Running";
    settled = "";
    message = "Waiting for explicit platoon registration completion.";
    changedAssignments = 0;
    cancelledAssignments = 0;
}

/** Sole force allocator. Rule results and platoon reports are its only inputs. */
export class CompanyCoordinator {
    readonly context = new CompanyContext();
    readonly ledger: ResourceLedger;
    readonly ports = new Map<string, PlatoonPort>();
    private readonly expected: Set<string>;
    private startedAt: number;
    private nextPoll = 0;
    private nextAssessment = 0;
    private nextHoldRetry = 0;
    private publicationTime = -Infinity;
    private publishedThisFrame = 0;
    private lastOrdinaryPlan = -Infinity;
    private pendingReports = new Map<string, FPlatoonSituationReport>();
    private terminalReports: FPlatoonSituationReport[] = [];
    private authorityLost = false;
    private requestedPhase?: Phase;
    private queuedTerminal = new Map<string, string>();
    private ruleOutcome: "Running" | "Succeeded" | "Failed" = "Running";
    private stopped = false;
    readonly policy: Policy;

    constructor(readonly companyId: string, readonly runId: string, readonly teamId: number,
        expectedIds: string[], readonly areas: Map<string, TacticalArea>, policy: Partial<Policy>,
        private readonly newId: () => string, private readonly now: () => number,
        private readonly authorized: () => boolean) {
        this.policy = { ...defaultPolicy, ...policy };
        for (const value of Object.values(this.policy)) if (!finite(value) || value <= 0) throw new Error("InvalidCompanyPolicy");
        if (!Number.isInteger(this.policy.publishBudget) || !Number.isInteger(this.policy.maximumRepairs) ||
            !companyId || !runId || teamId < 0 || teamId >= 255 || expectedIds.length > 64 ||
            new Set(expectedIds).size !== expectedIds.length) throw new Error("InvalidCompanyDefinition");
        this.expected = new Set(expectedIds);
        this.startedAt = now();
        this.ledger = new ResourceLedger(areas);
    }

    register(port: PlatoonPort): boolean {
        if (this.stopped || !this.authorized() || !this.expected.has(port.id) || port.teamId !== this.teamId ||
            port.membershipRevision < 1 || (this.ports.has(port.id) && this.ports.get(port.id) !== port)) return false;
        this.ports.set(port.id, port);
        return true;
    }
    unregister(id: string, membershipRevision: number): boolean {
        const port = this.ports.get(id);
        if (!port || port.membershipRevision !== membershipRevision) return false;
        for (const g of [...this.context.candidates]) if (g.assignments.some(a => a.platoonId === id)) this.discardGroup(g);
        const a = this.context.plan?.assignments[id];
        if (a) { this.cancelAssignment(a); this.markAffected(a.objectiveId); }
        this.ports.delete(id); this.context.reports.delete(id); this.pendingReports.delete(id); this.queuedTerminal.delete(id);
        this.terminalReports = this.terminalReports.filter(r => r.platoonId !== id);
        this.context.registrationComplete = false; this.requestedPhase = "Bootstrap";
        return true;
    }
    completeRegistration(): boolean {
        if (!this.expected.size || this.ports.size !== this.expected.size || !this.authorized()) return false;
        this.context.registrationComplete = true;
        this.context.message = "Virtual command hierarchy registered.";
        return true;
    }
    submit(mission: FCompanyMission): boolean {
        const error = validateMission(mission), current = this.context.mission;
        if (this.stopped || !this.authorized() || error ||
            (current?.id === mission.id && mission.revision <= current.revision)) {
            this.context.message = error || "StaleMissionOrAuthority"; return false;
        }
        this.discardCandidates();
        this.context.mission = clone(mission);
        for (const o of this.context.mission.objectives) {
            if (mission.rulesOfEngagement) o.mission.engagement = mission.rulesOfEngagement;
            if (mission.hasOperatingBoundary && mission.operatingBoundary) {
                o.mission.hasBoundary = true; o.mission.boundary = clone(mission.operatingBoundary);
            }
        }
        this.context.repairs = {};
        this.context.settled = "";
        this.context.outcome = "Running";
        this.ruleOutcome = "Running";
        this.context.knowledge.objectiveResults = {};
        this.context.affected = new Set(mission.objectives.map(o => o.id));
        this.context.nextPhase = "AssessSituation";
        this.requestedPhase = "AssessSituation";
        this.nextAssessment = 0;
        return true;
    }
    /** Reconciles changed objectives without discarding unrelated live tasks. */
    updateObjectives(objectives: FCompanyObjective[]): boolean {
        const current = this.context.mission;
        if (!current || this.stopped || !this.authorized()) return false;
        if (equivalent(current.objectives, objectives)) return true;
        const next = { ...clone(current), revision: current.revision + 1, objectives: clone(objectives) };
        if (validateMission(next)) return false;
        const changed = new Set([...current.objectives, ...objectives].filter(o =>
            !equivalent(current.objectives.find(p => p.id === o.id), objectives.find(p => p.id === o.id))).map(o => o.id));
        for (const group of [...this.context.candidates]) if (changed.has(group.objectiveId)) this.discardGroup(group);
        this.context.mission = next;
        for (const id of changed) { delete this.context.repairs[id]; this.markAffected(id); }
        this.requestedPhase = "AssessSituation"; this.context.nextPhase = "AssessSituation";
        this.nextAssessment = 0;
        return true;
    }
    /** Region boundaries revoke even player-locked orders, retaining the lock policy. */
    endObjectiveScope(outcome: CompanyContext["outcome"] = "Cancelled"): void {
        this.discardCandidates();
        for (const a of Object.values(this.context.plan?.assignments ?? {})) this.cancelAssignment(a);
        const c = this.context;
        c.mission = undefined; c.plan = undefined; c.knowledge.objectiveResults = {};
        c.repairs = {}; c.affected.clear(); c.outcome = outcome; c.settled = "";
        this.ruleOutcome = "Running";
        this.pendingReports.clear(); this.terminalReports = []; this.queuedTerminal.clear();
        this.requestedPhase = "AwaitMission"; c.nextPhase = "AwaitMission";
    }
    cancelMission(): void {
        this.discardCandidates();
        const c = this.context;
        if (c.plan) for (const a of Object.values(c.plan.assignments)) {
            // A cancellation withdraws this mission, while unrelated player intent stays protected.
            if (!c.locks.has(a.platoonId)) this.cancelAssignment(a);
        }
        c.mission = undefined; c.outcome = "Cancelled"; c.settled = "";
        c.nextPhase = "AwaitMission";
        this.requestedPhase = "AwaitMission";
    }
    setMode(mode: CompanyContext["mode"]): boolean {
        if (!["Autonomous", "PlayerAssisted", "PlayerManual", "Suspended"].includes(mode) || !this.authorized()) return false;
        if (mode === this.context.mode) return true;
        this.discardCandidates();
        this.context.mode = mode;
        ++this.context.commandEpoch;
        this.context.reports.clear();
        for (const a of Object.values(this.context.plan?.assignments ?? {})) {
            a.commandEpoch = this.context.commandEpoch; a.ready = false;
            a.permit.commandEpoch = this.context.commandEpoch; ++a.permit.gateVersion;
            this.ports.get(a.platoonId)?.reconfirm(clone(a));
        }
        this.authorityLost = true;
        this.context.nextPhase = "AuthorityRecovery";
        this.requestedPhase = "AuthorityRecovery";
        return true;
    }
    lockPlatoon(id: string, locked: boolean): boolean {
        if (!this.ports.has(id) || !this.authorized()) return false;
        if (locked) this.context.locks.add(id); else this.context.locks.delete(id);
        // Scope changes affect only this recipient, never global command generation.
        for (const group of [...this.context.candidates]) if (group.assignments.some(a => a.platoonId === id)) this.discardGroup(group);
        const a = this.context.plan?.assignments[id];
        if (a) this.markAffected(a.objectiveId);
        this.context.nextPhase = "AssessSituation";
        this.requestedPhase = "AssessSituation";
        return true;
    }
    submitManual(id: string, mission: FPlatoonMission): boolean {
        const c = this.context, port = this.ports.get(id), report = c.reports.get(id);
        if (!mission) return false;
        mission = clone(mission);
        if (c.mission?.rulesOfEngagement) mission.engagement = c.mission.rulesOfEngagement;
        if (c.mission?.hasOperatingBoundary && c.mission.operatingBoundary) {
            mission.hasBoundary = true; mission.boundary = clone(c.mission.operatingBoundary);
        }
        if (!port || !report || !this.authorized() || c.mode === "Suspended" || !c.locks.has(id) ||
            !fresh(report.observedAt, this.now(), this.policy.maxReportAge) || !report.commandAvailable ||
            report.knownRisk > (c.mission?.maximumKnownRisk ?? 1) ||
            !mission || !this.areas.has(mission.areaId) || report.mobile < mission.minimumMobile || report.ammo < mission.minimumAmmo ||
            (mission.hasBoundary && !inside(mission.goal.center, mission.boundary))) return false;
        const rule = { type: "Arrive" as const, areaId: mission.areaId, minimumPresent: 1, requiredFraction: 1,
            participants: [], holdSeconds: 0, graceSeconds: 0, children: [], checkpoints: [], continuous: false, publicResult: true };
        const o: FCompanyObjective = { id: "Player:" + id, priority: 0, mandatory: false,
            minimumPlatoons: 1, minimumMobile: mission.minimumMobile, mission, rule, dependencies: [] };
        if (validateMission({ id: "Manual", revision: 1, objectives: [o], deadline: 0 })) return false;
        const previous = c.plan?.assignments[id];
        const protectedObjective = c.mission?.objectives.find(o => o.id === previous?.objectiveId);
        if (protectedObjective?.mandatory && continuous(protectedObjective.mission.type) &&
            (previous?.mission.areaId !== mission.areaId || !continuous(mission.type)) && !Object.values(c.plan!.assignments).some(a =>
                a.platoonId !== id && a.objectiveId === protectedObjective.id && a.ready)) return false;
        c.plan ??= { id: this.newId(), revision: 1, sourceMissionId: c.mission?.id ?? "", sourceMissionRevision: c.mission?.revision ?? 1,
            assignments: {}, reservePlatoonIds: [], startedAt: this.now(), lastReplannedAt: this.now() };
        const a = this.create(id, o), owner = a.id + "/" + a.revision;
        const destination = this.areas.get(mission.areaId)!;
        let route = inside(report.location, destination.goal) ? [] : this.route(this.nearest(report.location), destination.id);
        if (mission.preparationAreaId) {
            const preparation = this.areas.get(mission.preparationAreaId);
            const before = preparation && inside(report.location, preparation.goal) ? [] : this.route(this.nearest(report.location), mission.preparationAreaId);
            const after = this.route(mission.preparationAreaId, mission.areaId);
            route = preparation && before && after ? [...before, ...after] : undefined;
        }
        if (!route || !this.ledger.reserve([...new Set([mission.areaId, mission.preparationAreaId, ...route].filter(Boolean))]
            .map(key => ({ key, owner, amount: 1, expiresAt: this.now() + this.policy.acceptanceSeconds, provisional: true,
                replaces: previous ? previous.id + "/" + previous.revision : undefined })), this.now())) return false;
        c.candidates.push({ objectiveId: o.id, objective: o, playerAuthorized: true, assignments: [a],
            retire: [], accepted: [], activated: [], deadline: this.now() + this.policy.acceptanceSeconds });
        this.requestedPhase = "CommitChanges"; c.nextPhase = "CommitChanges";
        return true;
    }
    publishRuleResults(results: FCompanyKnowledge["objectiveResults"], outcome: typeof this.ruleOutcome): void {
        for (const [id, result] of Object.entries(results)) {
            const old = this.context.knowledge.objectiveResults[id];
            if (old && result.resultSequence <= old.resultSequence) continue;
            this.context.knowledge.objectiveResults[id] = clone(result);
            if (!old || old.currentConditionValid !== result.currentConditionValid || old.evaluationState !== result.evaluationState ||
                old.completionRecorded !== result.completionRecorded || old.state !== result.state) this.markAffected(id);
        }
        this.ruleOutcome = outcome;
    }
    enqueue(report: FPlatoonSituationReport): boolean {
        if (!this.validReport(report)) return false;
        const terminalKey = report.assignmentId + "/" + report.assignmentRevision + "/" + report.taskSequence + "/" + report.state;
        if (["Succeeded", "Failed", "Cancelled"].includes(report.state) && this.queuedTerminal.get(report.platoonId) !== terminalKey) {
            // Terminal history is never silently coalesced; producer receives backpressure.
            if (this.terminalReports.length >= 256) return false;
            this.terminalReports.push(clone(report));
            this.queuedTerminal.set(report.platoonId, terminalKey);
        } else {
            const old = this.pendingReports.get(report.platoonId);
            if (!old || report.reportSequence > old.reportSequence || report.taskSequence > old.taskSequence)
                this.pendingReports.set(report.platoonId, clone(report));
        }
        return true;
    }
    private validReport(r: FPlatoonSituationReport): boolean {
        const p = r && this.ports.get(r.platoonId);
        return !!p && r.membershipRevision === p.membershipRevision && r.teamId === this.teamId &&
            r.runId === this.runId && r.commandEpoch === this.context.commandEpoch &&
            Number.isInteger(r.reportSequence) && r.reportSequence >= 0 && Number.isInteger(r.taskSequence) && r.taskSequence >= 0 &&
            [r.observedAt, r.taskObservedAt, r.effective, r.mobile, r.ammo, r.integrity, r.recovery,
            r.knownRisk, r.location?.x, r.location?.y, r.location?.z].every(finite) &&
            r.effective >= 0 && r.mobile >= 0 && r.mobile <= r.effective && r.ammo >= 0 && r.ammo <= 1 &&
            typeof r.commandAvailable === "boolean" && typeof r.ready === "boolean" &&
            Number.isInteger(r.gateVersion) && r.gateVersion >= 0 && r.taskObservedAt <= this.now() + 0.1 &&
            ["Accepted", "Preparing", "Executing", "Succeeded", "Failed", "Cancelled"].includes(r.state);
    }
    private ingest(): void {
        const now = this.now();
        if (now >= this.nextPoll) {
            this.nextPoll = now + 1;
            for (const port of this.ports.values()) this.enqueue(port.read());
        }
        const reports = this.terminalReports.splice(0, 64);
        for (const r of this.pendingReports.values()) reports.push(r);
        this.pendingReports.clear();
        for (const incoming of reports) {
            const old = this.context.reports.get(incoming.platoonId);
            const next = old ? clone(old) : clone(incoming);
            const expected = this.context.plan?.assignments[incoming.platoonId];
            const matchingTask = !expected || (expected.id === incoming.assignmentId && expected.revision === incoming.assignmentRevision);
            if (!old || (incoming.reportSequence > old.reportSequence && incoming.observedAt >= old.observedAt)) {
                Object.assign(next, incoming);
                if (old && (!matchingTask || incoming.taskSequence < old.taskSequence)) {
                    next.assignmentId = old.assignmentId; next.assignmentRevision = old.assignmentRevision;
                    next.taskSequence = old.taskSequence; next.taskObservedAt = old.taskObservedAt;
                    next.state = old.state; next.ready = old.ready;
                    next.gateVersion = old.gateVersion;
                }
            } else if (matchingTask && incoming.taskSequence > next.taskSequence && incoming.taskObservedAt >= next.taskObservedAt) {
                next.assignmentId = incoming.assignmentId; next.assignmentRevision = incoming.assignmentRevision;
                next.taskSequence = incoming.taskSequence; next.taskObservedAt = incoming.taskObservedAt;
                next.state = incoming.state; next.ready = incoming.ready; next.failure = incoming.failure;
                next.gateVersion = incoming.gateVersion;
                next.preparationReady = incoming.preparationReady;
            }
            this.context.reports.set(incoming.platoonId, next);
            const assignment = this.context.plan?.assignments[next.platoonId];
            if (!assignment || next.assignmentId !== assignment.id || next.assignmentRevision !== assignment.revision) continue;
            const wasReady = assignment.ready, wasState = assignment.state;
            assignment.state = next.state;
            assignment.ready = next.ready && assignment.permit.allowed && next.gateVersion === assignment.permit.gateVersion &&
                fresh(next.observedAt, now, this.policy.maxReportAge) &&
                fresh(next.taskObservedAt, now, this.policy.maxReportAge);
            if (wasState !== "Failed" && assignment.state === "Failed") {
                this.context.repairs[assignment.objectiveId] = (this.context.repairs[assignment.objectiveId] ?? 0) + 1;
                this.context.message = "AssignmentFailed:" + assignment.objectiveId + ":" + next.failure;
            }
            if (wasReady !== assignment.ready || wasState !== assignment.state) this.markAffected(assignment.objectiveId);
        }
    }
    private markAffected(id: string): void {
        this.context.affected.add(id);
        const visit = (source: string) => {
            for (const o of this.context.mission?.objectives ?? []) if (o.dependencies.some(d => d.id === source) &&
                !this.context.affected.has(o.id)) { this.context.affected.add(o.id); visit(o.id); }
        };
        visit(id);
    }
    private dependency(d: Dependency): boolean {
        const r = this.context.knowledge.objectiveResults[d.id];
        return !!r && (d.completion ? r.completionRecorded :
            r.evaluationState === "Valid" && r.currentConditionValid && fresh(r.evaluatedAt, this.now(), this.policy.maxReportAge));
    }
    private dependencies(o: FCompanyObjective, assignment?: FCompanyAssignment): boolean {
        return o.dependencies.every(d => (d.mode === "BeforeStartOnly" && assignment?.latchedDependencies.includes(d.id)) || this.dependency(d));
    }
    private capable(id: string, o: FCompanyObjective): boolean {
        const r = this.context.reports.get(id), p = this.ports.get(id);
        return !!r && !!p && r.commandAvailable && r.membershipRevision === p.membershipRevision &&
            r.commandEpoch === this.context.commandEpoch && fresh(r.observedAt, this.now(), this.policy.maxReportAge) &&
            r.knownRisk <= (this.context.mission?.maximumKnownRisk ?? 1) &&
            (!o.mission.hasBoundary || inside(o.mission.goal.center, o.mission.boundary)) &&
            r.mobile >= o.mission.minimumMobile && r.ammo >= o.mission.minimumAmmo;
    }
    private assignmentObjective(a: FCompanyAssignment): FCompanyObjective | undefined {
        const mission = this.context.mission;
        const configured = mission?.objectives.find(o => o.id === a.objectiveId);
        if (configured) return configured;
        if (mission && a.objectiveId === "Fallback:" + a.platoonId && mission.fallbackPolicy !== "Hold" &&
            a.mission.areaId === mission.fallbackAreaId && a.mission.type === (mission.fallbackPolicy === "Recover" ? "Regroup" : "Withdraw")) {
            return { ...clone(mission.objectives[0]), id: a.objectiveId, mandatory: false, minimumPlatoons: 1,
                minimumMobile: 1, mission: clone(a.mission), dependencies: [] };
        }
        return undefined;
    }
    private current(a: FCompanyAssignment, o: FCompanyObjective): boolean {
        const scope = this.context.mission!.id + "/" + this.context.mission!.revision + "/" + o.id;
        const expected = { ...o.mission, id: a.mission.id, revision: a.mission.revision,
            objectiveScope: o.mission.type === "Search" ? scope : a.mission.objectiveScope,
            searchAreaIds: o.mission.type === "Search" && o.rule.checkpoints.length ? o.rule.checkpoints : o.mission.searchAreaIds };
        return equivalent(expected, a.mission) && equivalent(a.dependencies, o.dependencies) && this.capable(a.platoonId, o) &&
            a.state !== "Failed" && a.state !== "Cancelled";
    }
    private route(start: string, target: string): string[] | undefined {
        if (!target) return [];
        if (!this.areas.has(target)) return undefined;
        if (!start || start === target) return [];
        const seen = new Set([start]), queue: { id: string; route: string[] }[] = [{ id: start, route: [] }];
        for (let i = 0; i < queue.length && i < 256; ++i) {
            const entry = queue[i];
            for (const p of this.areas.get(entry.id)?.passages ?? []) {
                if (!p.available || this.now() < p.opensAt || (p.closesAt > 0 && this.now() >= p.closesAt) || seen.has(p.to)) continue;
                if (p.to === target) return [...entry.route, p.id];
                seen.add(p.to); queue.push({ id: p.to, route: [...entry.route, p.id] });
            }
        }
        return undefined;
    }
    private nearest(location: FPlatoonSituationReport["location"]): string {
        let best = "", distance = Infinity;
        for (const area of this.areas.values()) {
            const d = (location.x - area.goal.center.x) ** 2 + (location.y - area.goal.center.y) ** 2;
            if (d <= area.goal.radius ** 2 && d < distance) { best = area.id; distance = d; }
        }
        return best;
    }
    private create(id: string, o: FCompanyObjective): FCompanyAssignment {
        const old = this.context.plan?.assignments[id];
        const same = old?.objectiveId === o.id;
        const assignmentId = same ? old!.id : this.newId(), revision = same ? old!.revision + 1 : 1;
        const identity = { companyId: this.companyId, runId: this.runId, commandEpoch: this.context.commandEpoch,
            membershipRevision: this.ports.get(id)!.membershipRevision, assignmentId, assignmentRevision: revision };
        return { id: assignmentId, revision, ...identity, platoonId: id, companyPlanId: this.context.plan!.id,
            objectiveId: o.id, role: o.mission.type === "Defend" ? "AreaSecurity" : "MainObjective",
            mission: { ...clone(o.mission), id: assignmentId, revision,
                searchAreaIds: o.mission.type === "Search" && o.rule.checkpoints.length ? clone(o.rule.checkpoints) : o.mission.searchAreaIds,
                objectiveScope: this.context.mission ? this.context.mission.id + "/" + this.context.mission.revision + "/" + o.id : "" }, dependencies: clone(o.dependencies),
            permit: { ...identity, gateVersion: 1, phaseId: "Execute", dependencyVersions: {}, allowed: false }, state: "Accepted", ready: false,
            startedAt: this.now(), latchedDependencies: [] };
    }
    private utility(o: FCompanyObjective): number {
        const mission = this.context.mission!, p = this.policy;
        const deadline = o.mission.deadline || mission.deadline;
        const urgency = deadline > 0 ? Math.max(0, 1 - (deadline - this.now()) / 60) : 0;
        const unlocked = mission.objectives.filter(target => target.dependencies.some(d => d.id === o.id)).length;
        const reports = [...this.context.reports.values()].filter(r => fresh(r.observedAt, this.now(), p.maxReportAge));
        const risk = reports.length ? reports.reduce((sum, r) => sum + r.knownRisk, 0) / reports.length : 1;
        const opportunity = o.minimumPlatoons / Math.max(1, reports.length);
        const strategy = mission.strategicPolicy ?? "Maintain";
        const preference = strategy === "Maintain" && continuous(o.mission.type) ? 1 :
            strategy === "Recover" && o.mission.type === "Regroup" ? 2 : strategy === "Disengage" && o.mission.type === "Withdraw" ? 2 : 0;
        return o.priority * p.valueWeight + urgency * p.urgencyWeight + unlocked * p.unlockWeight *
            (strategy === "Advance" ? 1.5 : 1) + preference - risk * p.riskWeight - opportunity * p.opportunityWeight;
    }
    private build(repair: boolean): void {
        const c = this.context, mission = c.mission;
        if (!mission || c.mode === "Suspended" || c.mode === "PlayerManual") { c.nextPhase = "Hold"; return; }
        if (!c.plan) c.plan = { id: this.newId(), revision: 1, sourceMissionId: mission.id, sourceMissionRevision: mission.revision,
            assignments: {}, reservePlatoonIds: [], startedAt: this.now(), lastReplannedAt: this.now() };
        const plan = c.plan;
        const used = new Set<string>([...c.locks, ...c.candidates.flatMap(g => g.assignments.map(a => a.platoonId))]);
        for (const a of Object.values(plan.assignments)) {
            const o = this.assignmentObjective(a);
            if (o && this.current(a, o) && (continuous(a.mission.type) || a.state !== "Succeeded")) used.add(a.platoonId);
            if (o && a.objectiveId.startsWith("Fallback:") && a.state === "Succeeded") used.add(a.platoonId);
        }
        const objectives = [...mission.objectives].sort((a, b) => Number(b.mandatory) - Number(a.mandatory) ||
            this.utility(b) - this.utility(a) || a.id.localeCompare(b.id));
        for (const o of objectives) {
            if (mission.failureCriteria?.includes(o.id)) continue;
            if (o.rule.type === "All" || o.rule.type === "Any") continue;
            if ((repair && !c.affected.has(o.id)) || c.candidates.some(g => g.objectiveId === o.id) ||
                (c.repairs[o.id] ?? 0) >= this.policy.maximumRepairs || !this.dependencies(o)) continue;
            const result = c.knowledge.objectiveResults[o.id];
            if (result?.completionRecorded && !continuous(o.mission.type)) continue;
            const old = Object.values(plan.assignments).filter(a => a.objectiveId === o.id);
            const kept = old.filter(a => this.current(a, o));
            let mobile = kept.reduce((n, a) => n + c.reports.get(a.platoonId)!.mobile, 0);
            if (kept.length >= o.minimumPlatoons && mobile >= o.minimumMobile) continue;
            const committedCandidates = new Set(c.candidates.flatMap(g => g.assignments.map(a => a.platoonId)));
            const ranked: { id: string; cost: number; route: string[] | undefined }[] = [...this.ports.keys()].filter(id => {
                if (c.locks.has(id) || committedCandidates.has(id) || !this.capable(id, o)) return false;
                if (!used.has(id)) return true;
                const old = plan.assignments[id];
                const previous: FCompanyObjective | undefined = mission.objectives.find(v => v.id === old?.objectiveId);
                return !!previous && !previous.mandatory && !mission.objectives.some(v => v.dependencies.some(d =>
                    d.id === previous.id && d.mode === "MaintainDuringExecution")) &&
                    this.utility(o) > this.utility(previous) + this.policy.reassignmentGain &&
                    (repair || this.now() - this.lastOrdinaryPlan >= this.policy.reassignmentSeconds);
            }).map(id => {
                const r = c.reports.get(id)!;
                const travel = Math.hypot(r.location.x - o.mission.goal.center.x, r.location.y - o.mission.goal.center.y) / 1000;
                const cost = travel + r.recovery * 10 + r.knownRisk * 5 + (plan.assignments[id] ? 10 : 0);
                const target = this.areas.get(o.mission.areaId);
                let route = target && inside(r.location, target.goal) ? [] : this.route(this.nearest(r.location), o.mission.areaId);
                if (o.mission.preparationAreaId) {
                    const preparation = this.areas.get(o.mission.preparationAreaId);
                    const before = preparation && inside(r.location, preparation.goal) ? [] : this.route(this.nearest(r.location), o.mission.preparationAreaId);
                    const after = this.route(o.mission.preparationAreaId, o.mission.areaId);
                    route = preparation && before && after ? [...before, ...after] : undefined;
                }
                return { id, cost, route };
            }).filter(p => p.route !== undefined).sort((a, b) => a.cost - b.cost || a.id.localeCompare(b.id));
            const selected: { id: string; cost: number; route: string[] | undefined }[] = [];
            for (const p of ranked) {
                if (kept.length + selected.length >= o.minimumPlatoons && mobile >= o.minimumMobile) break;
                selected.push(p); mobile += c.reports.get(p.id)!.mobile;
            }
            if (kept.length + selected.length < o.minimumPlatoons || mobile < o.minimumMobile) {
                c.message = "No complete feasible force group for " + o.id; continue;
            }
            const assignments = selected.map(p => this.create(p.id, o));
            const claims: Claim[] = assignments.flatMap((a, i) => [...new Set([a.mission.areaId, a.mission.preparationAreaId,
                ...(a.mission.searchAreaIds ?? []), ...selected[i].route!].filter(Boolean))]
                .map(key => ({ key, owner: a.id + "/" + a.revision, amount: 1,
                    replaces: plan.assignments[a.platoonId] ? plan.assignments[a.platoonId].id + "/" + plan.assignments[a.platoonId].revision : undefined,
                    expiresAt: this.now() + this.policy.acceptanceSeconds, provisional: true })));
            if (!this.ledger.reserve(claims, this.now())) { c.message = "Shared resource group unavailable: " + o.id; continue; }
            c.candidates.push({ objectiveId: o.id, assignments, retire: old.filter(a => !kept.includes(a)),
                accepted: [], activated: [], deadline: this.now() + this.policy.acceptanceSeconds });
            for (const p of selected) used.add(p.id);
        }
        if (!c.candidates.length && (mission.fallbackPolicy === "Recover" || mission.fallbackPolicy === "Withdraw") &&
            mission.fallbackAreaId && this.areas.has(mission.fallbackAreaId) && !Object.values(plan.assignments).some(a =>
                !c.locks.has(a.platoonId) && continuous(a.mission.type) && a.state !== "Failed")) {
            const area = this.areas.get(mission.fallbackAreaId)!;
            for (const id of [...this.ports.keys()].sort()) {
                if (c.locks.has(id) || used.has(id)) continue;
                const reference = mission.objectives[0];
                const fallback: FCompanyObjective = { ...clone(reference), id: "Fallback:" + id, mandatory: false,
                    minimumPlatoons: 1, minimumMobile: 1, dependencies: [], mission: { ...clone(reference.mission),
                        type: mission.fallbackPolicy === "Recover" ? "Regroup" : "Withdraw", areaId: area.id, goal: clone(area.goal),
                        preparationAreaId: "", searchAreaIds: [] } };
                if (!this.capable(id, fallback)) continue;
                const location = c.reports.get(id)!.location;
                const a = this.create(id, fallback), route = inside(location, area.goal) ? [] : this.route(this.nearest(location), area.id);
                if (!route || !this.ledger.reserve([area.id, ...route].map(key => ({ key, owner: a.id + "/" + a.revision,
                    amount: 1, expiresAt: this.now() + this.policy.acceptanceSeconds, provisional: true })), this.now())) continue;
                c.candidates.push({ objectiveId: fallback.id, objective: fallback, assignments: [a], retire: [],
                    accepted: [], activated: [], deadline: this.now() + this.policy.acceptanceSeconds }); used.add(id);
            }
        }
        plan.reservePlatoonIds = [...this.ports.keys()].filter(id => !used.has(id) && !plan.assignments[id]).sort();
        plan.lastReplannedAt = this.now(); plan.sourceMissionId = mission.id; plan.sourceMissionRevision = mission.revision;
        c.affected.clear();
        c.nextPhase = c.candidates.length ? "CommitChanges" : "Hold";
        if (c.nextPhase === "Hold") this.nextHoldRetry = this.now() + this.policy.holdSeconds;
        if (!c.candidates.length && Object.values(plan.assignments).some(a =>
            a.state !== "Failed" && a.state !== "Cancelled" && a.permit.allowed)) c.nextPhase = "Supervise";
        if (!repair) this.lastOrdinaryPlan = this.now();
    }
    private commit(): void {
        const c = this.context, plan = c.plan!;
        if (this.publicationTime !== this.now()) { this.publicationTime = this.now(); this.publishedThisFrame = 0; }
        let budget = Math.max(0, this.policy.publishBudget - this.publishedThisFrame);
        for (const group of [...c.candidates]) {
            const o = group.objective ?? c.mission?.objectives.find(o => o.id === group.objectiveId);
            if (!o || this.now() >= group.deadline || !this.dependencies(o) || group.assignments.some(a =>
                (!group.playerAuthorized && c.locks.has(a.platoonId)) || !this.capable(a.platoonId, o))) {
                this.failGroup(group, "CandidateExpiredOrConstraintLost"); continue;
            }
            for (const a of group.assignments) {
                if (budget <= 0) break;
                const p = this.ports.get(a.platoonId)!;
                if (!group.accepted.includes(a.id)) {
                    --budget;
                    ++this.publishedThisFrame;
                    if (!p.stage(clone(a), group.playerAuthorized)) { this.failGroup(group, "CandidateRejected"); break; }
                    group.accepted.push(a.id);
                }
            }
            if (!c.candidates.includes(group) || group.accepted.length !== group.assignments.length ||
                !group.assignments.every(a => this.ports.get(a.platoonId)!.preparationReady(a))) continue;
            for (const a of group.assignments) {
                if (budget <= 0 || group.activated.includes(a.id)) continue;
                if (!this.authorized() || c.mode === "Suspended") { this.failGroup(group, "AuthorityLost"); break; }
                --budget; a.permit.allowed = true;
                a.permit.dependencyVersions = this.dependencyVersions(a);
                ++this.publishedThisFrame;
                a.latchedDependencies = o.dependencies.filter(d => d.mode === "BeforeStartOnly").map(d => d.id);
                const previous = plan.assignments[a.platoonId];
                if (!this.ports.get(a.platoonId)!.activate(clone(a))) { this.failGroup(group, "ActivationRejected"); break; }
                if (previous && (previous.id !== a.id || previous.revision !== a.revision)) {
                    // The receiver has already atomically replaced this recipient's old intent.
                    this.ledger.release(previous.id + "/" + previous.revision);
                    ++c.cancelledAssignments;
                }
                plan.assignments[a.platoonId] = a;
                this.ledger.commit(a.id + "/" + a.revision, this.now(), 10);
                a.state = "Executing"; group.activated.push(a.id); ++c.changedAssignments; ++plan.revision;
            }
            if (!c.candidates.includes(group) || group.activated.length !== group.assignments.length) continue;
            group.retire = group.retire.filter(old => !group.assignments.some(a => a.platoonId === old.platoonId));
            if (!group.retire.length || group.assignments.every(a => a.ready && this.capable(a.platoonId, o))) {
                for (const old of group.retire) if (!c.locks.has(old.platoonId) && plan.assignments[old.platoonId] === old) this.cancelAssignment(old);
                c.candidates.splice(c.candidates.indexOf(group), 1);
            }
        }
        c.nextPhase = c.candidates.length ? "CommitChanges" : "Supervise";
    }
    private cancelAssignment(a: FCompanyAssignment): void {
        this.ports.get(a.platoonId)?.cancel(a.id, a.revision);
        this.ledger.release(a.id + "/" + a.revision);
        if (this.context.plan?.assignments[a.platoonId] === a) delete this.context.plan.assignments[a.platoonId];
        ++this.context.cancelledAssignments;
    }
    private discardGroup(group: CandidateGroup): void {
        for (const a of group.assignments) if (!group.activated.includes(a.id)) {
            this.ports.get(a.platoonId)?.discardCandidate(a.id, a.revision);
            this.ledger.release(a.id + "/" + a.revision);
        }
        const index = this.context.candidates.indexOf(group);
        if (index >= 0) this.context.candidates.splice(index, 1);
    }
    private failGroup(group: CandidateGroup, reason: string): void {
        this.context.repairs[group.objectiveId] = (this.context.repairs[group.objectiveId] ?? 0) + 1;
        this.context.message = reason + ":" + group.objectiveId;
        this.discardGroup(group); this.markAffected(group.objectiveId);
    }
    private discardCandidates(): void { for (const g of [...this.context.candidates]) this.discardGroup(g); }
    private supervise(): void {
        const c = this.context, mission = c.mission;
        for (const a of Object.values(c.plan?.assignments ?? {})) {
            const o = this.assignmentObjective(a);
            const manual = c.locks.has(a.platoonId) && a.objectiveId.startsWith("Player:");
            const report = c.reports.get(a.platoonId);
            const manualAllowed = manual && !!report && fresh(report.observedAt, this.now(), this.policy.maxReportAge) &&
                report.mobile >= a.mission.minimumMobile && report.ammo >= a.mission.minimumAmmo &&
                report.knownRisk <= (mission?.maximumKnownRisk ?? 1) &&
                (!mission?.hasOperatingBoundary || (!!mission.operatingBoundary && inside(a.mission.goal.center, mission.operatingBoundary)));
            const allowed = (manualAllowed || (!!o && this.capable(a.platoonId, o) && this.dependencies(o, a))) &&
                a.state !== "Failed" && a.state !== "Cancelled";
            const versions = this.dependencyVersions(a);
            if (!this.ledger.renew(a.id + "/" + a.revision, this.now(), 10)) this.markAffected(a.objectiveId);
            if (a.permit.allowed !== allowed || !equivalent(a.permit.dependencyVersions ?? {}, versions)) {
                a.permit.allowed = allowed; ++a.permit.gateVersion; a.ready = false;
                a.permit.dependencyVersions = versions;
                this.ports.get(a.platoonId)?.permit(clone(a.permit));
                this.markAffected(a.objectiveId);
            }
        }
        if (!mission) { c.nextPhase = "AwaitMission"; return; }
        const key = mission.id + "/" + mission.revision;
        if (this.ruleOutcome !== "Running" && c.settled !== key) { c.nextPhase = "FinalizeMission"; return; }
        if (c.affected.size) { c.nextPhase = "RepairPlan"; return; }
        if (this.now() >= this.nextAssessment) {
            this.nextAssessment = this.now() + this.policy.reassessmentSeconds;
            c.nextPhase = this.now() - this.lastOrdinaryPlan >= this.policy.reassignmentSeconds ? "AssessSituation" : "Supervise";
        } else c.nextPhase = "Supervise";
    }
    private dependencyVersions(a: FCompanyAssignment): Record<string, number> {
        return Object.fromEntries(a.dependencies.filter(d => d.mode === "MaintainDuringExecution" ||
            !a.latchedDependencies.includes(d.id)).map(d => [d.id, this.context.knowledge.objectiveResults[d.id]?.conditionVersion ?? 0]));
    }

    /** Called only by the active StateTree task; undefined means Running. */
    step(phase: Phase): boolean | undefined {
        const c = this.context;
        if (this.stopped && phase !== "Inactive") return false;
        this.ingest(); this.ledger.expire(this.now()); c.phase = phase; c.nextPhase = phase;
        if (this.requestedPhase) {
            const target = this.requestedPhase;
            if (phase !== target) { c.nextPhase = target; return true; }
            this.requestedPhase = undefined;
        }
        if (!this.authorized() && phase !== "Inactive") {
            if (!this.authorityLost) { ++c.commandEpoch; this.authorityLost = true; this.discardCandidates(); }
            c.nextPhase = "AuthorityRecovery";
            return phase === "AuthorityRecovery" ? undefined : true;
        }
        if (c.mode === "Suspended" && phase !== "Inactive" && phase !== "AuthorityRecovery") {
            c.nextPhase = "AuthorityRecovery"; return true;
        }
        switch (phase) {
            case "Bootstrap":
                if (!c.registrationComplete) {
                    if (this.now() - this.startedAt >= this.policy.registrationSeconds) {
                        c.message = "RegistrationTimeout"; c.nextPhase = "Hold"; return true;
                    }
                    return undefined;
                }
                c.nextPhase = c.mission ? "AssessSituation" : "AwaitMission"; break;
            case "AwaitMission":
                if (!c.mission) { this.supervise(); return undefined; }
                c.nextPhase = c.settled ? "Supervise" : "AssessSituation"; break;
            case "AssessSituation":
                if (!c.registrationComplete) { c.nextPhase = "Bootstrap"; break; }
                c.nextPhase = c.mission ? "BuildPlan" : "AwaitMission"; break;
            case "BuildPlan": this.build(false); break;
            case "RepairPlan": this.build(true); break;
            case "CommitChanges":
                this.supervise();
                if (c.nextPhase === "FinalizeMission") break;
                this.commit();
                if (c.nextPhase === "CommitChanges") return undefined;
                break;
            case "Supervise":
                this.supervise();
                if (c.nextPhase === "Supervise") return undefined;
                break;
            case "FinalizeMission":
                c.outcome = this.ruleOutcome;
                c.settled = c.mission ? c.mission.id + "/" + c.mission.revision : "";
                c.message = "Rule authority confirmed mission outcome: " + c.outcome;
                c.nextPhase = "AwaitMission"; break;
            case "Hold":
                this.supervise();
                if (c.nextPhase === "FinalizeMission") break;
                if (this.now() < this.nextHoldRetry && !c.affected.size) { c.nextPhase = "Hold"; return undefined; }
                this.nextHoldRetry = this.now() + this.policy.holdSeconds;
                c.nextPhase = c.registrationComplete ? "AssessSituation" : "Bootstrap"; break;
            case "AuthorityRecovery":
                this.discardCandidates();
                if (c.mode === "Suspended") { this.supervise(); c.nextPhase = "AuthorityRecovery"; return undefined; }
                c.reports.clear();
                for (const a of Object.values(c.plan?.assignments ?? {})) {
                    a.commandEpoch = c.commandEpoch; a.runId = this.runId; a.ready = false;
                    a.permit.commandEpoch = c.commandEpoch; a.permit.runId = this.runId;
                    a.permit.allowed = false; ++a.permit.gateVersion;
                    this.ports.get(a.platoonId)?.reconfirm(clone(a));
                }
                this.authorityLost = false; this.nextPoll = 0; c.nextPhase = "AssessSituation"; break;
            case "Inactive": this.shutdown(); return true;
        }
        return true;
    }
    shutdown(): void {
        if (this.stopped) return;
        this.discardCandidates();
        for (const a of Object.values(this.context.plan?.assignments ?? {})) this.cancelAssignment(a);
        this.stopped = true; this.ports.clear(); this.context.phase = "Inactive";
    }
    save(): string {
        const value = clone({ schema: 1, companyId: this.companyId, teamId: this.teamId,
            mission: this.context.mission, plan: this.context.plan, locks: [...this.context.locks],
            mode: this.context.mode, outcome: this.context.outcome, settled: this.context.settled,
            repairs: this.context.repairs, resources: this.ledger.snapshot().filter(c => !c.provisional), savedAt: this.now() });
        const remaining = (deadline: number) => deadline > 0 ? Math.max(0.001, deadline - value.savedAt) : 0;
        if (value.mission) { value.mission.deadline = remaining(value.mission.deadline);
            for (const o of value.mission.objectives) o.mission.deadline = remaining(o.mission.deadline); }
        if (value.plan) for (const a of Object.values(value.plan.assignments)) a.mission.deadline = remaining(a.mission.deadline);
        for (const r of value.resources) r.expiresAt = remaining(r.expiresAt);
        return JSON.stringify(value);
    }
    validateRestore(text: string): boolean {
        if (text.length > 1048576 || !this.authorized()) return false;
        try {
            const v = JSON.parse(text);
            if (v?.schema !== 1 || v.companyId !== this.companyId || v.teamId !== this.teamId ||
                (v.mission !== undefined && validateMission(v.mission)) || !Array.isArray(v.locks) || v.locks.length > 64 ||
                new Set(v.locks).size !== v.locks.length || v.locks.some((id: string) => !this.expected.has(id)) ||
                !["Autonomous", "PlayerAssisted", "PlayerManual", "Suspended"].includes(v.mode) ||
                !["Running", "Succeeded", "Failed", "Cancelled"].includes(v.outcome) || typeof v.settled !== "string" ||
                !v.repairs || Object.keys(v.repairs).length > 128 || Object.values(v.repairs).some(n => !Number.isInteger(n) || (n as number) < 0)) return false;
            const p: FCompanyPlan | undefined = v.plan;
            if (v.resources !== undefined && (!Array.isArray(v.resources) || v.resources.length > 4096 ||
                v.resources.some((r: Claim) => !r || typeof r.key !== "string" || typeof r.owner !== "string" ||
                    !Number.isInteger(r.amount) || r.amount < 1 || !finite(r.expiresAt) || r.expiresAt < 0 || r.provisional))) return false;
            if (!p) return true;
            if (typeof p.id !== "string" || !p.id || !Number.isInteger(p.revision) || p.revision < 1 ||
                !p.assignments || typeof p.assignments !== "object" || Array.isArray(p.assignments) ||
                Object.keys(p.assignments).length > 64 || !Array.isArray(p.reservePlatoonIds) ||
                p.reservePlatoonIds.some(id => !this.expected.has(id))) return false;
            const identities = new Set<string>();
            for (const [id, a] of Object.entries(p.assignments)) {
                if (!a || !this.ports.has(id) || a.platoonId !== id || typeof a.id !== "string" || !a.id || identities.has(a.id) ||
                    !Number.isInteger(a.revision) || a.revision < 1 || a.companyId !== this.companyId ||
                    !validPlatoonMission(a.mission, true) || a.mission.id !== a.id || a.mission.revision !== a.revision ||
                    !["Accepted", "Preparing", "Executing", "Succeeded", "Failed", "Cancelled"].includes(a.state) ||
                    !a.permit || !Number.isInteger(a.permit.gateVersion) || a.permit.gateVersion < 1 ||
                    a.permit.assignmentId !== a.id || a.permit.assignmentRevision !== a.revision ||
                    !Array.isArray(a.dependencies) || !Array.isArray(a.latchedDependencies) ||
                    (!v.mission?.objectives.some((o: FCompanyObjective) => o.id === a.objectiveId) &&
                        !(v.locks.includes(id) && a.objectiveId === "Player:" + id) &&
                        !(a.objectiveId === "Fallback:" + id && ["Recover", "Withdraw"].includes(v.mission?.fallbackPolicy) &&
                            a.mission.areaId === v.mission?.fallbackAreaId))) return false;
                identities.add(a.id);
            }
            return true;
        } catch { return false; }
    }
    restore(text: string): boolean {
        if (!this.validateRestore(text)) return false;
        if (text.length > 1048576 || !this.authorized()) return false;
        let value: ReturnType<typeof JSON.parse>;
        try { value = JSON.parse(text); } catch { return false; }
        if (value?.schema !== 1 || value.companyId !== this.companyId || value.teamId !== this.teamId ||
            (value.mission && validateMission(value.mission)) || !Array.isArray(value.locks) ||
            value.locks.some((id: string) => !this.expected.has(id)) ||
            !["Autonomous", "PlayerAssisted", "PlayerManual", "Suspended"].includes(value.mode)) return false;
        const mission: FCompanyMission = clone(value.mission);
        const deadline = (remaining: number) => remaining > 0 ? this.now() + remaining : 0;
        if (mission) { mission.deadline = deadline(mission.deadline);
            for (const o of mission.objectives) o.mission.deadline = deadline(o.mission.deadline); }
        const plan: FCompanyPlan | undefined = value.plan;
        if (plan && (!plan.id || !plan.assignments || Object.keys(plan.assignments).length > 64 ||
            Object.values(plan.assignments).some(a => !this.ports.has(a.platoonId) || !a.id || a.revision < 1 ||
                (!mission?.objectives.some(o => o.id === a.objectiveId) && !a.objectiveId.startsWith("Player:") &&
                    !a.objectiveId.startsWith("Fallback:"))))) return false;
        this.discardCandidates();
        for (const a of Object.values(this.context.plan?.assignments ?? {})) this.ledger.release(a.id + "/" + a.revision);
        this.context.mission = mission; this.context.plan = clone(plan);
        this.context.locks = new Set(value.locks); this.context.mode = value.mode;
        this.context.outcome = value.outcome; this.context.settled = value.settled;
        this.context.repairs = value.repairs ?? {}; this.context.reports.clear();
        ++this.context.commandEpoch;
        for (const a of Object.values(this.context.plan?.assignments ?? {})) {
            a.mission.deadline = deadline(a.mission.deadline);
            a.runId = this.runId; a.commandEpoch = this.context.commandEpoch;
            a.membershipRevision = this.ports.get(a.platoonId)!.membershipRevision; a.ready = false;
            a.permit = { companyId: this.companyId, runId: this.runId, commandEpoch: a.commandEpoch,
                membershipRevision: a.membershipRevision, assignmentId: a.id, assignmentRevision: a.revision,
                gateVersion: a.permit.gateVersion + 1, allowed: false };
            a.permit.phaseId = "Execute"; a.permit.dependencyVersions = {};
            if (!this.ports.get(a.platoonId)!.reconfirm(clone(a))) {
                delete this.context.plan!.assignments[a.platoonId]; this.markAffected(a.objectiveId);
            } else {
                const claims: Claim[] = (value.resources ?? []).filter((r: Claim) => r.owner === a.id + "/" + a.revision)
                    .map((r: Claim) => ({ ...r, expiresAt: this.now() + 10 }));
                if (!claims.length && a.mission.areaId) claims.push({ key: a.mission.areaId, owner: a.id + "/" + a.revision,
                    amount: 1, expiresAt: this.now() + 10, provisional: false });
                if (!this.ledger.reserve(claims, this.now())) this.markAffected(a.objectiveId);
            }
        }
        this.nextPoll = 0; this.nextAssessment = 0; this.context.nextPhase = "AssessSituation";
        return true;
    }
    describe(): string {
        const c = this.context;
        return JSON.stringify({ companyId: this.companyId, runId: this.runId, commandEpoch: c.commandEpoch,
            phase: c.phase, nextPhase: c.nextPhase, mode: c.mode, message: c.message, outcome: c.outcome,
            registrationComplete: c.registrationComplete, changedAssignments: c.changedAssignments,
            cancelledAssignments: c.cancelledAssignments, plan: c.plan, objectiveResults: c.knowledge.objectiveResults,
            locks: [...c.locks], reports: [...c.reports.values()].map(r => ({ platoonId: r.platoonId,
                reportAge: this.now() - r.observedAt, freshness: fresh(r.observedAt, this.now(), this.policy.maxReportAge) ? "Fresh" : "Unknown" })),
            resources: this.ledger.snapshot() });
    }
}
