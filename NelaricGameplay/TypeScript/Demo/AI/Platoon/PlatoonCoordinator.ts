// Copyright (c) 2026 Nelaric Contributors

import * as UE from "ue";
import { $ref, $unref } from "puerts";
import { Area, clone, continuous, fresh, inside, FCompanyAssignment, FExecutionPermit, FPlatoonMission,
    FPlatoonSituationReport, TaskState, validPlatoonMission } from "../Company/CommandContracts";

export interface PlatoonPolicy {
    maxReportAge: number; minimumMobile: number; minimumAmmo: number; retryDelay: number; maximumRepairs: number;
}
interface Binding { actor: UE.DemoSquadCommandActor; id: string; epoch: number }
interface SquadAssignment {
    binding: Binding; mission: UE.DemoSquadMission; revision: number; repairs: number;
    retryAt: number; dispatched: boolean; ready: boolean; completed: boolean; status: string;
}
interface Plan { id: string; revision: number; mission: FPlatoonMission; assignments: SquadAssignment[]; state: TaskState; searchIndex: number }
export function guidKey(id: UE.Guid): string { return UE.KismetGuidLibrary.Conv_GuidToString(id).toUpperCase(); }
function valid(value: UE.Object | null | undefined): boolean { return !!value && UE.KismetSystemLibrary.IsValid(value); }
/** Virtual platoon context. Only its tree drives squad collaboration. */
export class PlatoonCoordinator {
    private bindings: Binding[] = [];
    private pending?: FPlatoonMission;
    private plan?: Plan;
    private candidate?: FCompanyAssignment;
    private preparationGoal?: Area;
    private preparationTaskId = "";
    private assignment?: FCompanyAssignment;
    private ended = false;
    private serial = 0;
    private nextPoll = 0;
    private nextMonitor = 0;
    private inputRevision = -1;
    private taskSequence = 0;
    private reportSequence = 0;
    private sample?: FPlatoonSituationReport;
    private lastOutcome?: { id: string; revision: number; state: TaskState };
    private companyEpoch = 1;
    private companyRunId = "";
    private permitted = true;
    stage = "AwaitMission";
    message = "Virtual platoon awaiting mission.";
    constructor(private readonly owner: UE.Actor, readonly platoonId: string, readonly teamId: number,
        private readonly policy: PlatoonPolicy, readonly membership: UE.DemoCompanyMembershipComponent) {}

    bind(squads: UE.DemoSquadCommandActor[]): boolean {
        if (this.ended || this.bindings.length || squads.length < 1 || squads.length > 64 ||
            new Set(squads.map(s => valid(s) ? guidKey(s.GetSquadContext().GetSquadId()) : "")).size !== squads.length) return false;
        this.membership.ReleaseSquadWatches();
        for (const actor of squads) {
            if (!valid(actor) || !actor.HasAuthority() || actor.GetWorld() !== this.owner.GetWorld()) return this.rollback("InvalidSquad");
            const context = actor.GetSquadContext(), report = context.GetSituationReport();
            if (report.TeamId !== this.teamId || this.teamId === 255) return this.rollback("WrongTeam");
            if (valid(context.GetMissionSource())) return this.rollback("SquadAlreadyOwned");
            const epoch = context.ClaimMissionSource(this.owner);
            if (epoch <= 0) return this.rollback("SquadLeaseRejected");
            this.bindings.push({ actor, id: guidKey(context.GetSquadId()), epoch });
            this.membership.WatchSquad(actor);
        }
        this.poll(); return true;
    }
    private rollback(reason: string): false {
        for (const b of this.bindings) if (valid(b.actor)) b.actor.GetSquadContext().ReleaseMissionSource(this.owner, b.epoch);
        this.bindings = []; this.message = reason; return false;
    }
    claimCompany(source: UE.Actor, runId: string, epoch: number): number {
        const revision = this.membership.ClaimCompany(source, this.platoonId);
        if (revision > 0) { this.companyRunId = runId; this.companyEpoch = epoch; }
        return revision;
    }
    updateCompanyAuthority(epoch: number): void {
        if (epoch < this.companyEpoch) return;
        if (epoch !== this.companyEpoch) { this.companyEpoch = epoch; this.candidate = undefined; this.nextPoll = 0; ++this.taskSequence; }
    }
    stageCompany(source: UE.Actor, a: FCompanyAssignment, playerAuthorized = false, preparation?: Area): boolean {
        if (!this.validCompany(source, a) || !this.validMission(a.mission) ||
            !this.membership.StageAssignment(source, JSON.stringify(a), playerAuthorized)) return false;
        if (this.candidate?.id === a.id && this.candidate.revision === a.revision) return true;
        this.candidate = clone(a); this.preparationGoal = preparation && clone(preparation);
        if (a.mission.preparationAreaId && preparation && (!this.assignment || !continuous(this.assignment.mission.type)) &&
            !inside(this.situation().location, preparation)) {
            this.preparationTaskId = guidKey(UE.KismetGuidLibrary.NewGuid());
            this.replace({ ...clone(a.mission), id: this.preparationTaskId, revision: 1, type: "Move",
                areaId: a.mission.preparationAreaId, goal: clone(preparation), preparationAreaId: "" });
            this.permitted = true;
        }
        ++this.taskSequence; return true;
    }
    preparationReady(a: FCompanyAssignment): boolean {
        const report = this.situation();
        if (this.candidate?.id !== a.id || this.candidate.revision !== a.revision ||
            !fresh(report.observedAt, this.now(), this.policy.maxReportAge) || !report.commandAvailable ||
            report.mobile < a.mission.minimumMobile || report.ammo < a.mission.minimumAmmo) return false;
        // A staged candidate never moves a platoon away from an existing duty.
        return !a.mission.preparationAreaId || (!!this.preparationGoal && inside(report.location, this.preparationGoal) &&
            (this.plan?.mission.id !== this.preparationTaskId || this.plan.state === "Succeeded"));
    }
    activateCompany(source: UE.Actor, a: FCompanyAssignment): boolean {
        if (!this.validCompany(source, a)) return false;
        if (this.assignment?.id === a.id && this.assignment.revision === a.revision) return true;
        if (!this.preparationReady(a)) return false;
        const p = this.nativePermit(a.permit);
        if (!this.membership.ActivateAssignment(source, p)) return false;
        this.assignment = clone(a); this.candidate = undefined; this.permitted = true;
        this.replace(a.mission); return true;
    }
    discardCandidate(source: UE.Actor, id: string, revision: number): void {
        if (this.candidate?.id === id && this.candidate.revision === revision) {
            this.membership.CancelAssignment(source, id, revision);
            if (this.plan?.mission.id === this.preparationTaskId) this.cancel();
            this.candidate = undefined; this.preparationGoal = undefined; this.preparationTaskId = ""; ++this.taskSequence;
        }
    }
    cancelCompany(source: UE.Actor, id: string, revision: number): boolean {
        if (!this.assignment || this.assignment.id !== id || this.assignment.revision !== revision ||
            !this.membership.CancelAssignment(source, id, revision)) return false;
        this.cancel(); this.assignment = undefined; return true;
    }
    permitCompany(source: UE.Actor, p: FExecutionPermit): boolean {
        const a = this.assignment;
        if (!a || !this.validCompany(source, a) || p.assignmentId !== a.id || p.assignmentRevision !== a.revision ||
            p.runId !== this.companyRunId || p.commandEpoch !== this.companyEpoch ||
            p.membershipRevision !== this.membership.GetMembershipRevision() || p.phaseId !== "Execute" || p.gateVersion < a.permit.gateVersion) return false;
        if (p.gateVersion === a.permit.gateVersion) return a.permit.allowed === p.allowed;
        a.permit = clone(p); this.permitted = p.allowed;
        for (const s of this.plan?.assignments ?? []) if (s.dispatched && valid(s.binding.actor))
            s.binding.actor.GetSquadContext().SetExecutionPermitFromSource(this.owner, s.binding.epoch,
                s.mission.MissionId, s.revision, p.gateVersion, p.allowed);
        ++this.taskSequence; this.nextPoll = 0; return true;
    }
    reconfirmCompany(source: UE.Actor, a: FCompanyAssignment): boolean {
        this.companyEpoch = a.commandEpoch; this.companyRunId = a.runId;
        if (this.assignment?.id === a.id && this.assignment.revision === a.revision) {
            // Reauthorize the same semantic task; do not resubmit every squad mission.
            if (!this.membership.ReconcileAssignment(source, JSON.stringify(a))) return false;
            this.assignment.commandEpoch = a.commandEpoch; this.assignment.runId = a.runId;
            this.assignment.membershipRevision = a.membershipRevision;
            this.nextPoll = 0;
            return this.permitCompany(source, a.permit);
        }
        if (!this.validCompany(source, a) || !this.membership.ReconcileAssignment(source, JSON.stringify(a))) return false;
        this.assignment = clone(a); this.candidate = undefined; this.permitted = false;
        this.replace(a.mission); return true;
    }
    private nativePermit(p: FExecutionPermit): UE.DemoExecutionPermit {
        const value = new UE.DemoExecutionPermit();
        value.CompanyId = p.companyId; value.RunId = p.runId; value.CommandEpoch = p.commandEpoch;
        value.MembershipRevision = p.membershipRevision; value.AssignmentId = p.assignmentId;
        value.AssignmentRevision = p.assignmentRevision; value.GateVersion = p.gateVersion; value.bAllowed = p.allowed;
        value.PhaseId = p.phaseId ?? "Execute";
        for (const [id, version] of Object.entries(p.dependencyVersions ?? {})) value.DependencyVersions.Set(id, version);
        return value;
    }
    private validCompany(source: UE.Actor, a: FCompanyAssignment): boolean {
        const registry = UE.DemoCommandLibrary.GetRegistry(this.owner);
        return !this.ended && valid(source) && valid(registry) &&
            UE.KismetMathLibrary.EqualEqual_ObjectObject(registry.GetCompany(this.teamId), source) &&
            a.platoonId === this.platoonId && a.runId === this.companyRunId && a.commandEpoch === this.companyEpoch &&
            a.membershipRevision === this.membership.GetMembershipRevision();
    }
    private validMission(m: FPlatoonMission): boolean {
        return validPlatoonMission(m, true);
    }
    submitMission(m: FPlatoonMission): boolean {
        if (valid(this.membership.GetCompanySource()) || !this.validMission(m) || !this.owner.HasAuthority() || this.ended ||
            (this.plan?.mission.id === m.id && m.revision <= this.plan.mission.revision)) return false;
        this.replace(m); return true;
    }
    submit(type: UE.EDemoSquadMissionType, center: UE.Vector, radius: number, duration: number): boolean {
        const types: Record<number, FPlatoonMission["type"]> = {
            [UE.EDemoSquadMissionType.Move]: "Move", [UE.EDemoSquadMissionType.Regroup]: "Regroup",
            [UE.EDemoSquadMissionType.Defend]: "Defend", [UE.EDemoSquadMissionType.Control]: "SecureArea",
            [UE.EDemoSquadMissionType.Search]: "Search", [UE.EDemoSquadMissionType.Withdraw]: "Withdraw" };
        if (!types[type] || duration < 0 || duration > 3600) return false;
        const area = { center: { x: center.X, y: center.Y, z: center.Z }, radius };
        return this.submitMission({ id: guidKey(UE.KismetGuidLibrary.NewGuid()), revision: ++this.serial,
            type: types[type], goal: area, areaId: "", preparationAreaId: "", boundary: area, hasBoundary: false,
            minimumMobile: this.policy.minimumMobile, minimumAmmo: this.policy.minimumAmmo,
            deadline: duration > 0 ? this.now() + duration : 0, engagement: "SelfDefense" });
    }
    private replace(m: FPlatoonMission): void {
        this.cancel(); this.pending = clone(m); this.nextMonitor = 0; this.nextPoll = 0; this.permitted = this.assignment?.permit.allowed ?? true;
        this.message = "Mission accepted by virtual platoon."; ++this.taskSequence;
    }
    cancel(): void {
        this.pending = undefined;
        if (this.plan) {
            for (const s of this.plan.assignments) if (s.dispatched && valid(s.binding.actor))
                s.binding.actor.GetSquadContext().CancelMissionFromSource(this.owner, s.binding.epoch, s.mission.MissionId, s.revision);
            this.lastOutcome = { id: this.plan.mission.id, revision: this.plan.mission.revision, state: "Cancelled" };
            this.plan = undefined; ++this.taskSequence;
        }
    }
    confirmLeadership(): boolean { return !this.ended && this.owner.HasAuthority(); }
    step(operation: string): boolean | undefined {
        if (this.ended) return false;
        this.stage = operation;
        if (this.inputRevision !== this.membership.InputRevision) {
            this.inputRevision = this.membership.InputRevision; this.nextPoll = 0; this.nextMonitor = 0;
        }
        this.poll();
        switch (operation) {
            case "AwaitMission": return this.pending || (this.plan && ["Accepted", "Preparing", "Executing"].includes(this.plan.state)) ? true : undefined;
            case "BuildPlan": return this.build();
            case "PublishAssignments": return this.publish();
            case "MonitorPlan": return this.monitor();
            case "FinalizeMission":
                if (this.plan && this.plan.state !== "Executing") {
                    this.lastOutcome = { id: this.plan.mission.id, revision: this.plan.mission.revision, state: this.plan.state };
                }
                return true;
            default: return false;
        }
    }
    private build(): boolean {
        if (this.plan && !this.pending) return true;
        if (!this.pending) return false;
        const mission = this.pending; this.pending = undefined;
        this.plan = { id: guidKey(UE.KismetGuidLibrary.NewGuid()), revision: 1, mission, assignments: [], state: "Preparing", searchIndex: 0 };
        return this.buildSquadAssignments(this.plan);
    }
    private searchAreaId(plan: Plan): string { return plan.mission.searchAreaIds?.[plan.searchIndex] ?? plan.mission.areaId; }
    private buildSquadAssignments(plan: Plan): boolean {
        const mission = plan.mission;
        let destination = mission.goal;
        if (mission.type === "Search" && mission.searchAreaIds?.length) {
            const area = UE.DemoCommandLibrary.GetObjectives(this.owner)?.FindArea(this.searchAreaId(plan));
            if (!valid(area)) return this.fail("SearchAreaUnavailable");
            destination = { center: { x: area.Area.Center.X, y: area.Area.Center.Y, z: area.Area.Center.Z }, radius: area.Area.Radius };
        }
        const types = { Move: UE.EDemoSquadMissionType.Move, SecureArea: UE.EDemoSquadMissionType.Control,
            Defend: UE.EDemoSquadMissionType.Defend, Search: UE.EDemoSquadMissionType.Search,
            Withdraw: UE.EDemoSquadMissionType.Withdraw, Regroup: UE.EDemoSquadMissionType.Regroup };
        for (let i = 0; i < this.bindings.length; ++i) {
            const count = this.bindings.length, angle = i * Math.PI * 2 / count;
            const offset = count === 1 ? 0 : destination.radius * 0.55;
            const goal = new UE.Vector(destination.center.x + Math.cos(angle) * offset,
                destination.center.y + Math.sin(angle) * offset, destination.center.z);
            const m = new UE.DemoSquadMission();
            m.ObjectiveScope = mission.objectiveScope ?? mission.id + "/" + mission.revision;
            m.MissionId = UE.KismetGuidLibrary.NewGuid(); m.Revision = 1; m.Type = types[mission.type];
            m.Goal = new UE.DemoSquadArea(goal, count === 1 ? destination.radius : destination.radius * 0.25);
            m.Deadline = mission.deadline; m.bHasBoundary = mission.hasBoundary;
            m.Boundary = new UE.DemoSquadArea(new UE.Vector(mission.boundary.center.x,
                mission.boundary.center.y, mission.boundary.center.z), mission.boundary.radius);
            m.Engagement = UE.EDemoSquadEngagement[mission.engagement];
            plan.assignments.push({ binding: this.bindings[i], mission: m, revision: 1, repairs: 0,
                retryAt: 0, dispatched: false, ready: false, completed: false, status: "Accepted" });
        }
        ++this.taskSequence; return true;
    }
    private publish(): boolean | undefined {
        if (!this.plan) return false;
        if (!this.permitted) return undefined;
        if (!this.plan.assignments.every(s => this.capable(s))) return this.fail("RequiredSquadCapabilityUnknown");
        for (const s of this.plan.assignments) if (!s.dispatched && !this.dispatch(s)) return this.fail("SquadRejected");
        this.plan.state = "Executing"; ++this.taskSequence; return true;
    }
    private dispatch(s: SquadAssignment): boolean {
        s.dispatched = s.binding.actor.GetSquadContext().SetMissionFromSource(this.owner, s.binding.epoch, s.mission);
        if (s.dispatched && this.assignment) s.binding.actor.GetSquadContext().SetExecutionPermitFromSource(
            this.owner, s.binding.epoch, s.mission.MissionId, s.revision, this.assignment.permit.gateVersion, this.permitted);
        return s.dispatched;
    }
    private capable(s: SquadAssignment): boolean {
        if (!valid(s.binding.actor)) { s.status = "OwnershipLost"; return false; }
        const c = s.binding.actor.GetSquadContext(), r = c.GetSituationReport();
        if (r.MembershipEpoch !== s.binding.epoch || !UE.KismetMathLibrary.EqualEqual_ObjectObject(c.GetMissionSource(), this.owner)) {
            s.status = "OwnershipLost"; return false;
        }
        if (!fresh(r.ObservedAt, this.now(), this.policy.maxReportAge)) { s.status = "ReportExpired"; return false; }
        if (!r.bCommandAvailable || r.Capability.MobileMemberCount < 1 ||
            r.Capability.AmmoReadiness < this.policy.minimumAmmo) { s.status = "CapabilityInsufficient"; return false; }
        return true;
    }
    private monitor(): boolean | undefined {
        const plan = this.plan;
        if (this.now() < this.nextMonitor) return undefined;
        this.nextMonitor = this.now() + 1;
        if (!plan || plan.state === "Failed" || plan.state === "Cancelled") return true;
        if (!this.permitted) return undefined;
        if (plan.mission.deadline > 0 && this.now() >= plan.mission.deadline && plan.state !== "Succeeded") return this.fail("DeadlineExpired");
        for (const s of plan.assignments) {
            if (!this.capable(s)) { s.ready = false; if (s.status === "OwnershipLost") return this.fail(s.status); continue; }
            if (!s.dispatched && !this.dispatch(s)) return this.fail("ExecutionRestoreRejected");
            const r = s.binding.actor.GetSquadContext().GetSituationReport(), result = r.MissionResult;
            if (guidKey(result.MissionId) !== guidKey(s.mission.MissionId) || result.Revision !== s.revision) { s.ready = false; continue; }
            s.ready = r.bReady && r.GateVersion === (this.assignment?.permit.gateVersion ?? 0);
            if (result.State === UE.EDemoSquadMissionState.Succeeded) {
                s.completed = true; s.status = "Succeeded";
                if (plan.mission.type === "Search" && plan.mission.areaId) {
                    const area = UE.DemoCommandLibrary.GetObjectives(this.owner)?.FindArea(this.searchAreaId(plan));
                    if (valid(area)) area.RecordSearch(s.binding.actor, s.mission.MissionId, s.revision);
                }
            }
            else if (result.State === UE.EDemoSquadMissionState.Failed) {
                if (s.repairs >= this.policy.maximumRepairs) return this.fail("LocalRepairLimit");
                if (!s.retryAt) s.retryAt = this.now() + this.policy.retryDelay;
                if (this.now() >= s.retryAt) {
                    ++s.repairs; s.mission.Revision = ++s.revision; ++plan.revision; s.retryAt = 0;
                    s.ready = false; s.completed = false;
                    if (!this.dispatch(s)) return this.fail("RepairRejected");
                }
            } else if (result.State === UE.EDemoSquadMissionState.Cancelled) return this.fail("RequiredSquadCancelled");
        }
        if (!continuous(plan.mission.type) && plan.assignments.every(s => s.completed)) {
            if (plan.mission.type === "Search" && plan.searchIndex + 1 < (plan.mission.searchAreaIds?.length ?? 0)) {
                for (const s of plan.assignments) s.binding.actor.GetSquadContext().CancelMissionFromSource(
                    this.owner, s.binding.epoch, s.mission.MissionId, s.revision);
                ++plan.searchIndex; ++plan.revision; plan.assignments = [];
                if (!this.buildSquadAssignments(plan)) return false;
                this.publish(); this.nextPoll = 0; return undefined;
            }
            plan.state = "Succeeded"; ++this.taskSequence; return true;
        }
        return undefined;
    }
    private fail(reason: string): false {
        if (this.plan) this.plan.state = "Failed";
        this.message = reason; ++this.taskSequence; return false;
    }
    private poll(): void {
        if (this.inputRevision !== this.membership.InputRevision) {
            this.inputRevision = this.membership.InputRevision; this.nextPoll = 0; this.nextMonitor = 0;
        }
        if (this.now() < this.nextPoll && this.sample) return;
        this.nextPoll = this.now() + 1;
        const reports = this.bindings.filter(b => valid(b.actor)).map(b => b.actor.GetSquadContext().GetSituationReport());
        let effective = 0, mobile = 0, ammo = 0, integrity = 0, knownRisk = 0, observedAt = this.now();
        const location = { x: 0, y: 0, z: 0 };
        for (const r of reports) {
            observedAt = Math.min(observedAt, r.ObservedAt);
            effective += r.Capability.EffectiveMemberCount; mobile += r.Capability.MobileMemberCount;
            ammo += r.Capability.AmmoReadiness * r.Capability.EffectiveMemberCount;
            integrity += r.Capability.FormationIntegrity; knownRisk += r.Capability.KnownThreatPressure;
            location.x += r.Location.X; location.y += r.Location.Y; location.z += r.Location.Z;
        }
        const count = reports.length || 1;
        location.x /= count; location.y /= count; location.z /= count;
        const mission = this.pending ?? this.plan?.mission;
        const state = this.pending ? "Accepted" : this.plan?.state ?? this.lastOutcome?.state ?? "Accepted";
        const ready = this.permitted && !!this.plan?.assignments.length && this.plan.assignments.every(s => s.ready && this.capable(s));
        const next: FPlatoonSituationReport = { platoonId: this.platoonId, membershipRevision: this.membership.GetMembershipRevision(),
            runId: this.companyRunId, commandEpoch: this.companyEpoch, reportSequence: ++this.reportSequence,
            taskSequence: this.taskSequence, observedAt: reports.length === this.bindings.length ? observedAt : -1,
            taskObservedAt: this.now(), commandAvailable: !this.ended && this.owner.HasAuthority() &&
                reports.length === this.bindings.length && reports.every(r => r.bCommandAvailable),
            teamId: this.teamId, effective, mobile, ammo: effective ? ammo / effective : 0,
            integrity: integrity / count, recovery: effective ? 1 - ammo / effective : 1, knownRisk: knownRisk / count, location,
            assignmentId: this.assignment?.id ?? mission?.id ?? this.lastOutcome?.id ?? "",
            assignmentRevision: this.assignment?.revision ?? mission?.revision ?? this.lastOutcome?.revision ?? 0,
            gateVersion: this.assignment?.permit.gateVersion ?? 0,
            state, ready, preparationReady: !!this.candidate && fresh(observedAt, this.now(), this.policy.maxReportAge), failure: this.message };
        if (this.sample && (this.sample.ready !== next.ready || this.sample.state !== next.state ||
            this.sample.assignmentId !== next.assignmentId || this.sample.assignmentRevision !== next.assignmentRevision)) next.taskSequence = ++this.taskSequence;
        this.sample = next;
    }
    situation(): FPlatoonSituationReport { this.poll(); return clone(this.sample!); }
    /** Captures only semantic IDs, geometry and remaining durations. */
    save(): unknown {
        const remaining = (deadline: number) => deadline > 0 ? Math.max(0.001, deadline - this.now()) : 0;
        const mission = this.plan && clone(this.plan.mission);
        if (mission) mission.deadline = remaining(mission.deadline);
        return { schema: 1, platoonId: this.platoonId, teamId: this.teamId, taskSequence: this.taskSequence,
            assignment: this.assignment && clone(this.assignment), plan: this.plan && { id: this.plan.id,
                revision: this.plan.revision, mission, state: this.plan.state, searchIndex: this.plan.searchIndex, squads: this.plan.assignments.map(s => ({
                    key: UE.KismetSystemLibrary.GetPathName(s.binding.actor).replace(/UEDPIE_[0-9]+_/g, ""),
                    bindings: UE.DemoCommandLibrary.CaptureSquadBindings(s.binding.actor),
                    squadId: s.binding.id,
                    missionId: guidKey(s.mission.MissionId), revision: s.revision, repairs: s.repairs,
                    goal: { center: { x: s.mission.Goal.Center.X, y: s.mission.Goal.Center.Y, z: s.mission.Goal.Center.Z }, radius: s.mission.Goal.Radius },
                    completed: s.completed, status: s.status })) } };
    }
    validateRestore(value: unknown): boolean {
        try {
            const saved = value as ReturnType<PlatoonCoordinator["save"]> & { schema: number; platoonId: string; teamId: number;
                taskSequence: number; plan?: { mission: FPlatoonMission; squads: { key: string; missionId: string;
                    revision: number; repairs: number; goal: Area; bindings?: string; squadId?: string }[] } };
            if (!saved || saved.schema !== 1 || saved.platoonId !== this.platoonId || saved.teamId !== this.teamId ||
                !Number.isInteger(saved.taskSequence) || saved.taskSequence < 0) return false;
            if (!saved.plan) return true;
            if (!this.validMission(saved.plan.mission) || !Array.isArray(saved.plan.squads) || saved.plan.squads.length !== this.bindings.length ||
                new Set(saved.plan.squads.map(s => s.key)).size !== saved.plan.squads.length) return false;
            for (const item of saved.plan.squads) {
                const b = this.bindings.find(b => UE.KismetSystemLibrary.GetPathName(b.actor).replace(/UEDPIE_[0-9]+_/g, "") === item.key);
                const identity = $ref(new UE.Guid()), success = $ref(false);
                if (typeof item.missionId !== "string") return false;
                UE.KismetGuidLibrary.Parse_StringToGuid(item.missionId, identity, success);
                if (!b || !$unref(success) || !Number.isInteger(item.revision) || item.revision < 1 ||
                    !Number.isInteger(item.repairs) || item.repairs < 0 || !item.goal ||
                    ![item.goal.radius, item.goal.center.x, item.goal.center.y, item.goal.center.z].every(Number.isFinite) ||
                    (item.bindings && !UE.DemoCommandLibrary.RestoreSquadBindings(b.actor, item.bindings, false))) return false;
                if (item.squadId) {
                    const id = $ref(new UE.Guid()), ok = $ref(false); UE.KismetGuidLibrary.Parse_StringToGuid(item.squadId, id, ok);
                    if (!$unref(ok) || !b.actor.GetSquadContext().RestoreIdentity($unref(id), false)) return false;
                }
            }
            return true;
        } catch { return false; }
    }
    restore(value: unknown): boolean {
        if (!this.validateRestore(value)) return false;
        const saved = value as { schema: number; platoonId: string; teamId: number; taskSequence: number;
            assignment?: FCompanyAssignment; plan?: { id: string; revision: number; mission: FPlatoonMission;
                state: TaskState; searchIndex?: number; squads: { key: string; bindings?: string; squadId?: string; missionId: string; revision: number; repairs: number;
                    goal: FPlatoonMission["goal"]; completed: boolean; status: string }[] } };
        if (!saved || saved.schema !== 1 || saved.platoonId !== this.platoonId || saved.teamId !== this.teamId ||
            !Number.isInteger(saved.taskSequence) || saved.taskSequence < 0) return false;
        if (!saved.plan) return true;
        const data = saved.plan;
        if (!this.validMission(data.mission) || !Array.isArray(data.squads) || data.squads.length !== this.bindings.length) return false;
        const types = { Move: UE.EDemoSquadMissionType.Move, SecureArea: UE.EDemoSquadMissionType.Control,
            Defend: UE.EDemoSquadMissionType.Defend, Search: UE.EDemoSquadMissionType.Search,
            Withdraw: UE.EDemoSquadMissionType.Withdraw, Regroup: UE.EDemoSquadMissionType.Regroup };
        const restored: SquadAssignment[] = [];
        const mission = clone(data.mission);
        mission.deadline = mission.deadline > 0 ? this.now() + mission.deadline : 0;
        for (const item of data.squads) {
            const binding = this.bindings.find(b => UE.KismetSystemLibrary.GetPathName(b.actor).replace(/UEDPIE_[0-9]+_/g, "") === item.key);
            if (!binding || !Number.isInteger(item.revision) || item.revision < 1 || !Number.isInteger(item.repairs) ||
                item.repairs < 0 || !item.goal || ![item.goal.radius, item.goal.center.x, item.goal.center.y, item.goal.center.z].every(Number.isFinite)) return false;
            const identity = $ref(new UE.Guid()), success = $ref(false);
            UE.KismetGuidLibrary.Parse_StringToGuid(item.missionId, identity, success);
            if (!$unref(success)) return false;
            if (item.squadId) {
                const id = $ref(new UE.Guid()), ok = $ref(false); UE.KismetGuidLibrary.Parse_StringToGuid(item.squadId, id, ok);
                if (!$unref(ok) || !binding.actor.GetSquadContext().RestoreIdentity($unref(id))) return false;
                binding.id = guidKey($unref(id));
            }
            if (item.bindings && !UE.DemoCommandLibrary.RestoreSquadBindings(binding.actor, item.bindings)) return false;
            const current = binding.actor.GetSquadContext().GetMission();
            const alreadyActive = guidKey(current.MissionId) === guidKey($unref(identity)) && current.Revision === item.revision;
            const nativeMission = alreadyActive ? current : new UE.DemoSquadMission();
            if (!alreadyActive) {
                nativeMission.MissionId = $unref(identity); nativeMission.Revision = item.revision; nativeMission.Type = types[mission.type];
                nativeMission.ObjectiveScope = mission.objectiveScope ?? mission.id + "/" + mission.revision;
                nativeMission.Goal = new UE.DemoSquadArea(new UE.Vector(item.goal.center.x, item.goal.center.y, item.goal.center.z), item.goal.radius);
                nativeMission.bHasBoundary = mission.hasBoundary;
                nativeMission.Boundary = new UE.DemoSquadArea(new UE.Vector(mission.boundary.center.x, mission.boundary.center.y, mission.boundary.center.z), mission.boundary.radius);
                nativeMission.Engagement = UE.EDemoSquadEngagement[mission.engagement]; nativeMission.Deadline = mission.deadline;
            }
            restored.push({ binding, mission: nativeMission, revision: item.revision, repairs: item.repairs,
                retryAt: 0, dispatched: alreadyActive, ready: false, completed: item.completed && alreadyActive, status: item.status });
        }
        this.assignment = saved.assignment && clone(saved.assignment);
        this.plan = { id: data.id, revision: data.revision, mission, assignments: restored,
            state: restored.every(s => s.dispatched) ? data.state : "Preparing", searchIndex: data.searchIndex ?? 0 };
        this.pending = undefined; this.permitted = false; this.taskSequence = Math.max(this.taskSequence, saved.taskSequence) + 1;
        this.nextPoll = 0; this.nextMonitor = 0;
        return true;
    }
    private now(): number { return UE.GameplayStatics.GetTimeSeconds(this.owner); }
    shutdown(): void {
        if (this.ended) return;
        this.cancel(); this.rollback("Platoon stopped.");
        const source = this.membership.GetCompanySource();
        if (valid(source)) this.membership.ReleaseCompany(source, this.membership.GetMembershipRevision());
        this.ended = true;
    }
    describe(): string { return JSON.stringify({ platoonId: this.platoonId, stage: this.stage,
        message: this.message, report: this.situation(), plan: this.plan && { id: this.plan.id,
            revision: this.plan.revision, mission: this.plan.mission, state: this.plan.state,
            assignments: this.plan.assignments.map(s => ({ squadId: s.binding.id, missionId: guidKey(s.mission.MissionId),
                revision: s.revision, ready: s.ready, status: s.status, repairs: s.repairs })) } }); }
}
export const platoons = new WeakMap<UE.Actor, PlatoonCoordinator>();
