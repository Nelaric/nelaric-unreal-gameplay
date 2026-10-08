// Copyright (c) 2026 Nelaric Contributors

import * as UE from "ue";
import { toDelegate } from "puerts";
import { platoons } from "../Platoon/PlatoonCoordinator";
import TS_PlatoonCommand from "../Platoon/TS_PlatoonCommand";
import { CompanyCoordinator } from "./CompanyCoordinator";
import { clone, FCompanyMission, FPlatoonMission, nativeValue, Phase, PlatoonPort, TacticalArea } from "./CommandContracts";
import { getObjectiveAuthority, ObjectiveAuthority } from "./ObjectiveAuthority";
import { BattlefrontCompanyAdapter, BattlefrontSession } from "./BattlefrontObjectives";

interface Runtime {
    front: BattlefrontCompanyAdapter; core: CompanyCoordinator; rules: ObjectiveAuthority; lease: number; timer?: UE.TimerHandle;
    nextRules: number; nextStep: number; inputRevision: number; bindings: Map<string, UE.Actor>;
}
const companies = new WeakMap<UE.Actor, Runtime>();
const battlefronts = new WeakMap<UE.DemoObjectiveWorldSubsystem, BattlefrontSession>();
function battlefront(actor: UE.Actor): BattlefrontSession | undefined {
    const world = UE.DemoCommandLibrary.GetObjectives(actor);
    return valid(world) ? battlefronts.get(world) : undefined;
}
function valid(value: UE.Object | undefined | null): boolean { return !!value && UE.KismetSystemLibrary.IsValid(value); }

/** Sole company lifecycle owner. All decisions remain in the shared coordinator. */
class TS_CompanyCommand extends UE.DemoCompanyCommandActor {
    StartCommander(): boolean {
        if (!this.HasAuthority() || !UE.KismetSystemLibrary.IsServer(this) || !valid(this.Definition) || !valid(this.Policy)) return false;
        if (companies.has(this)) { if (!this.GetCommandTree().IsRunning()) this.GetCommandTree().StartLogic(); return this.GetCommandTree().IsRunning(); }
        const registry = UE.DemoCommandLibrary.GetRegistry(this);
        if (!valid(registry)) return false;
        const lease = registry.RegisterCompany(this, this.Definition.CompanyId, this.Definition.TeamId);
        if (lease <= 0) return false;
        const nativeRules = UE.DemoCommandLibrary.GetObjectives(this);
        if (!valid(nativeRules)) { registry.UnregisterCompany(this, lease); return false; }
        try {
            const areas = new Map<string, TacticalArea>();
            for (const a of JSON.parse(nativeRules.GetAreaDefinitions()).areas as TacticalArea[]) areas.set(a.id, a);
            const p = this.Policy;
            const core = new CompanyCoordinator(this.Definition.CompanyId, registry.GetRunId(), this.Definition.TeamId,
                Array.from(this.Definition.ExpectedPlatoonIds), areas,
                { maxReportAge: p.MaxReportAge, reassessmentSeconds: p.ReassessmentSeconds, holdSeconds: p.HoldSeconds,
                    reassignmentSeconds: p.ReassignmentSeconds, publishBudget: p.PublishBudget,
                    acceptanceSeconds: p.AcceptanceSeconds, registrationSeconds: p.RegistrationSeconds, maximumRepairs: p.MaximumRepairs,
                    valueWeight: p.ValueWeight, urgencyWeight: p.UrgencyWeight, unlockWeight: p.UnlockWeight,
                    riskWeight: p.RiskWeight, opportunityWeight: p.OpportunityWeight, reassignmentGain: p.ReassignmentGain },
                () => UE.KismetGuidLibrary.Conv_GuidToString(UE.KismetGuidLibrary.NewGuid()),
                () => UE.GameplayStatics.GetTimeSeconds(this), () => valid(registry) && registry.HasPublicationAuthority(this, lease));
            const runtime: Runtime = { front: new BattlefrontCompanyAdapter(core), core, lease, rules: getObjectiveAuthority(nativeRules, this.Definition.TeamId, p.MaxReportAge),
                nextRules: 0, nextStep: 0, inputRevision: -1, bindings: new Map() };
            companies.set(this, runtime);
            this.BattlefrontUpdateHandler.Bind((publisher, snapshot) => publisher ? this.acceptBattlefrontState(publisher, snapshot) : false);
            this.flush(runtime);
            this.PollCompanyInputs();
            if (valid(this.InitialMission)) this.SubmitCompanyMission(this.InitialMission.Mission);
            runtime.timer = UE.KismetSystemLibrary.K2_SetTimerDelegate(toDelegate(this as TS_CompanyCommand, "PollCompanyInputs"), 0.5, true);
            this.GetCommandTree().StartLogic();
            if (!this.GetCommandTree().IsRunning()) { this.StopCommander(); return false; }
            return true;
        } catch (error) {
            console.error("[Company] " + String(error)); this.BattlefrontUpdateHandler.Unbind(); registry.UnregisterCompany(this, lease); companies.delete(this); return false;
        }
    }
    PollCompanyInputs(): void {
        const r = companies.get(this);
        if (!r) return;
        const registry = UE.DemoCommandLibrary.GetRegistry(this);
        if (!valid(registry) || !registry.HasPublicationAuthority(this, r.lease)) return;
        const core = r.core, nativeRules = UE.DemoCommandLibrary.GetObjectives(this);
        if (valid(nativeRules)) {
            const definitions = JSON.parse(nativeRules.GetAreaDefinitions()).areas as TacticalArea[];
            for (const area of definitions) core.areas.set(area.id, area);
        }
        for (const [id, actor] of [...r.bindings]) if (!valid(actor) || !platoons.has(actor)) {
            const port = core.ports.get(id);
            if (port) core.unregister(id, port.membershipRevision);
            r.bindings.delete(id);
        }
        for (const actor of Array.from(this.Platoons)) {
            if (!valid(actor)) continue;
            let platoon = platoons.get(actor);
            if (!platoon) {
                // Registration is deferred until the lower layer has initialized its squads.
                const command = actor as TS_PlatoonCommand;
                if (typeof command.StartCommander === "function") command.StartCommander();
                platoon = platoons.get(actor);
            }
            if (!platoon || platoon.teamId !== this.Definition.TeamId) continue;
            platoon.updateCompanyAuthority(core.context.commandEpoch);
            if (!core.ports.has(platoon.platoonId)) {
                const membership = platoon.claimCompany(this, core.runId, core.context.commandEpoch);
                if (membership <= 0) continue;
                const receiver = platoon;
                const port: PlatoonPort = { id: receiver.platoonId, teamId: receiver.teamId, membershipRevision: membership,
                    read: () => receiver.situation(), stage: (a, manual) => receiver.stageCompany(this, a, manual, core.areas.get(a.mission.preparationAreaId)?.goal),
                    preparationReady: a => receiver.preparationReady(a), activate: a => receiver.activateCompany(this, a),
                    permit: p => receiver.permitCompany(this, p), cancel: (id, revision) => receiver.cancelCompany(this, id, revision),
                    discardCandidate: (id, revision) => receiver.discardCandidate(this, id, revision),
                    reconfirm: a => receiver.reconfirmCompany(this, a) };
                if (!core.register(port)) receiver.membership.ReleaseCompany(this, membership);
                else r.bindings.set(receiver.platoonId, actor);
            }
        }
        if (!core.context.registrationComplete) core.completeRegistration();
        for (const port of core.ports.values()) core.enqueue(port.read());
        const now = UE.GameplayStatics.GetTimeSeconds(this);
        const state = battlefront(this)?.read();
        if (state) {
            if (core.context.registrationComplete) r.front.update(state, now);
            r.nextStep = 0; this.flush(r); return;
        }
        if (now >= r.nextRules && core.context.mission && core.context.registrationComplete && valid(nativeRules)) {
            r.nextRules = now + 0.5;
            const published = r.rules.poll(now, core.context.plan, r.bindings);
            core.publishRuleResults(published.results, published.outcome);
        }
        r.nextStep = 0;
    }
    StepCompany(phase: string): UE.EStateTreeRunStatus {
        const r = companies.get(this);
        if (!r) return UE.EStateTreeRunStatus.Failed;
        const now = UE.GameplayStatics.GetTimeSeconds(this), revision = this.GetCompanyContext().InputRevision;
        const waiting = ["Bootstrap", "AwaitMission", "Supervise", "Hold", "AuthorityRecovery"].includes(phase);
        if (revision !== r.inputRevision) { r.inputRevision = revision; r.nextRules = 0; this.PollCompanyInputs(); r.nextStep = 0; }
        if (waiting && now < r.nextStep && r.core.context.nextPhase === phase) return UE.EStateTreeRunStatus.Running;
        r.nextStep = now + 0.5;
        const result = r.core.step(phase as Phase);
        this.flush(r);
        return result === undefined ? UE.EStateTreeRunStatus.Running : result ? UE.EStateTreeRunStatus.Succeeded : UE.EStateTreeRunStatus.Failed;
    }
    private acceptBattlefrontState(publisher: UE.Actor, snapshot: string): boolean {
        if (!this.HasAuthority() || !UE.KismetSystemLibrary.IsServer(this) || !valid(publisher) ||
            !UE.KismetMathLibrary.EqualEqual_ObjectObject(publisher, UE.GameplayStatics.GetGameMode(this))) return false;
        const r = companies.get(this), world = UE.DemoCommandLibrary.GetObjectives(this);
        const registry = UE.DemoCommandLibrary.GetRegistry(this);
        if (!r || !valid(world) || !valid(registry) || !registry.HasPublicationAuthority(this, r.lease)) return false;
        let session = battlefronts.get(world);
        if (!session) session = new BattlefrontSession();
        const definitions = JSON.parse(world.GetAreaDefinitions()).areas as TacticalArea[];
        if (!session.publish(snapshot, UE.GameplayStatics.GetTimeSeconds(this), new Set(definitions.map(a => a.id)))) return false;
        battlefronts.set(world, session);
        for (const actor of Array.from(registry.GetCompanies())) {
            const peer = companies.get(actor);
            if (peer && peer.core.context.registrationComplete) {
                peer.front.update(session.read()!, UE.GameplayStatics.GetTimeSeconds(this));
                (actor as TS_CompanyCommand).flush(peer);
            }
        }
        return true;
    }
    SubmitCompanyMission(mission: UE.DemoCompanyMission): boolean {
        const r = companies.get(this);
        if (!r || battlefront(this)?.read()) return false;
        try {
            const value = nativeValue<FCompanyMission>(UE.DemoCommandLibrary.EncodeCompanyMission(mission));
            if (!value.id) value.id = UE.KismetGuidLibrary.Conv_GuidToString(UE.KismetGuidLibrary.NewGuid());
            if (!r.core.submit(value)) { this.flush(r); return false; }
            r.rules.configure(value); r.nextRules = 0; r.nextStep = 0; this.flush(r); return true;
        } catch { return false; }
    }
    SetPlatoonManualScope(id: string, locked: boolean): boolean {
        const r = companies.get(this), actor = r?.bindings.get(id), receiver = actor && platoons.get(actor);
        if (!r || !receiver || !receiver.membership.SetManualScope(this, locked)) return false;
        const accepted = r.core.lockPlatoon(id, locked);
        if (accepted) { r.nextStep = 0; this.flush(r); }
        return accepted;
    }
    SubmitManualPlatoonMission(id: string, mission: UE.DemoPlatoonMission): boolean {
        const r = companies.get(this);
        if (!r) return false;
        try {
            this.PollCompanyInputs();
            const value = nativeValue<FPlatoonMission>(UE.DemoCommandLibrary.EncodePlatoonMission(mission));
            if (battlefront(this)?.read()) {
                const area = r.core.context.mission?.objectives.find(o => o.mission.areaId === value.areaId)?.mission.goal;
                if (!area || !value.goal?.center || !Number.isFinite(value.goal.radius) || value.goal.radius < 1 ||
                    ![value.goal.center.x, value.goal.center.y, value.goal.center.z].every(Number.isFinite) ||
                    Math.hypot(value.goal.center.x - area.center.x, value.goal.center.y - area.center.y) + value.goal.radius > area.radius) return false;
            }
            const accepted = r.core.submitManual(id, value); r.nextStep = 0; this.flush(r); return accepted;
        } catch { return false; }
    }
    SetCompanyMode(mode: string): boolean {
        const r = companies.get(this);
        if (!r) return false;
        if (!["Autonomous", "PlayerAssisted", "PlayerManual", "Suspended"].includes(mode)) return false;
        if (mode !== r.core.context.mode) {
            const pending = JSON.parse(r.core.describe()); ++pending.commandEpoch; pending.mode = mode;
            if (!this.GetCompanyContext().PublishState(JSON.stringify(pending))) return false;
        }
        const accepted = r.core.setMode(mode as typeof r.core.context.mode); r.nextStep = 0; this.flush(r); return accepted;
    }
    CancelCompanyMission(): void { const r = companies.get(this); if (r && !battlefront(this)?.read()) { r.core.cancelMission(); r.nextStep = 0; this.flush(r); } }
    GetDebugStatus(): string { return companies.get(this)?.core.describe() ?? "Company is not running."; }
    SaveCommandState(slot: string): boolean {
        const r = companies.get(this);
        if (!r || battlefront(this)?.read()) return false;
        const platoonStates = Object.fromEntries([...r.bindings].map(([id, actor]) => [id, platoons.get(actor)?.save()]));
        return UE.DemoCommandLibrary.SaveCommands(slot, JSON.stringify({ schema: 1, company: JSON.parse(r.core.save()),
            rules: r.rules.save(), platoons: platoonStates }));
    }
    RestoreCommandState(slot: string): boolean {
        const r = companies.get(this);
        if (!r || battlefront(this)?.read()) return false;
        try {
            const saved = JSON.parse(UE.DemoCommandLibrary.LoadCommands(slot));
            if (saved.schema !== 1 || !r.core.context.registrationComplete) return false;
            if (!r.core.validateRestore(JSON.stringify(saved.company)) ||
                (saved.company.mission && !r.rules.validateRestore(saved.company.mission, saved.rules))) return false;
            for (const [id, actor] of r.bindings) if (!saved.platoons?.[id] || !platoons.get(actor)?.validateRestore(saved.platoons[id])) return false;
            const pending = JSON.parse(r.core.describe()); ++pending.commandEpoch; pending.mode = saved.company.mode;
            if (!this.GetCompanyContext().PublishState(JSON.stringify(pending))) return false;
            for (const [id, actor] of r.bindings) if (!platoons.get(actor)?.restore(saved.platoons[id])) return false;
            if (!r.core.restore(JSON.stringify(saved.company))) return false;
            if (r.core.context.mission) {
                r.rules.configure(r.core.context.mission);
                if (!r.rules.restore(saved.rules)) return false;
            }
            r.nextRules = 0; r.nextStep = 0; this.flush(r); return true;
        } catch { return false; }
    }
    private flush(runtime: Runtime): void {
        const value = JSON.parse(runtime.core.describe());
        value.semantic = JSON.parse(runtime.core.save());
        this.GetCompanyContext().PublishState(JSON.stringify(value));
    }
    StopCommander(): void {
        const r = companies.get(this);
        this.GetCommandTree().StopLogic("Company stopped");
        if (!r) return;
        if (r.timer) UE.KismetSystemLibrary.K2_ClearTimerHandle(this, r.timer);
        r.core.shutdown();
        for (const actor of r.bindings.values()) {
            const p = platoons.get(actor);
            if (p) p.membership.ReleaseCompany(this, p.membership.GetMembershipRevision());
        }
        const registry = UE.DemoCommandLibrary.GetRegistry(this);
        if (valid(registry)) registry.UnregisterCompany(this, r.lease);
        this.BattlefrontUpdateHandler.Unbind();
        companies.delete(this);
    }
}
export default TS_CompanyCommand;
