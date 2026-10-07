// Copyright (c) 2026 Nelaric Contributors

/** Shared value-only contracts. No UObject, timer, navigation or tree handles. */
export interface Point { x: number; y: number; z: number }
export interface Area { center: Point; radius: number }
export type MissionType = "Move" | "SecureArea" | "Defend" | "Search" | "Withdraw" | "Regroup";
export type CommandMode = "Autonomous" | "PlayerAssisted" | "PlayerManual" | "Suspended";
export type TaskState = "Accepted" | "Preparing" | "Executing" | "Succeeded" | "Failed" | "Cancelled";
export type Phase = "Bootstrap" | "AwaitMission" | "AssessSituation" | "BuildPlan" | "CommitChanges" |
    "Supervise" | "RepairPlan" | "FinalizeMission" | "Hold" | "AuthorityRecovery" | "Inactive";
export interface Dependency {
    id: string;
    mode: "BeforeStartOnly" | "MaintainDuringExecution";
    completion: boolean;
}
export interface FPlatoonMission {
    id: string; revision: number; type: MissionType; areaId: string; goal: Area;
    preparationAreaId: string; deadline: number; engagement: "HoldFire" | "SelfDefense" | "FireAtWill";
    hasBoundary: boolean; boundary: Area; minimumMobile: number; minimumAmmo: number;
    objectiveScope?: string;
    searchAreaIds?: string[];
}
export interface ObjectiveRule {
    type: "Arrive" | "Regroup" | "Capture" | "Maintain" | "Search" | "Escort" | "All" | "Any";
    areaId: string; minimumPresent: number; requiredFraction: number; participants: string[];
    holdSeconds: number; graceSeconds: number; children: string[]; checkpoints: string[];
    continuous: boolean; publicResult: boolean;
}
export interface FCompanyObjective {
    id: string; priority: number; mandatory: boolean; minimumPlatoons: number; minimumMobile: number;
    mission: FPlatoonMission; rule: ObjectiveRule; dependencies: Dependency[];
}
export interface FCompanyMission { id: string; revision: number; objectives: FCompanyObjective[]; deadline: number;
    strategicPolicy?: "Advance" | "Maintain" | "Recover" | "Disengage";
    hasOperatingBoundary?: boolean; operatingBoundary?: Area; maximumKnownRisk?: number;
    rulesOfEngagement?: "HoldFire" | "SelfDefense" | "FireAtWill";
    successCriteria?: string[]; failureCriteria?: string[];
    fallbackPolicy?: "Hold" | "Recover" | "Withdraw"; fallbackAreaId?: string }
export interface FExecutionPermit {
    companyId: string; runId: string; commandEpoch: number; membershipRevision: number;
    assignmentId: string; assignmentRevision: number; gateVersion: number; allowed: boolean;
    phaseId?: string; dependencyVersions?: Record<string, number>;
}
export interface FCompanyAssignment {
    id: string; revision: number; companyId: string; runId: string; commandEpoch: number;
    platoonId: string; membershipRevision: number; companyPlanId: string; objectiveId: string;
    role: "MainObjective" | "AreaSecurity" | "Support" | "Reserve" | "Recovery";
    mission: FPlatoonMission; dependencies: Dependency[]; permit: FExecutionPermit;
    state: TaskState; ready: boolean; startedAt: number; latchedDependencies: string[];
}
export interface FPlatoonSituationReport {
    platoonId: string; membershipRevision: number; runId: string; commandEpoch: number;
    reportSequence: number; taskSequence: number; observedAt: number; taskObservedAt: number;
    commandAvailable: boolean; teamId: number; effective: number; mobile: number; ammo: number;
    integrity: number; recovery: number; knownRisk: number; location: Point;
    assignmentId: string; assignmentRevision: number; state: TaskState; ready: boolean;
    gateVersion: number;
    preparationReady: boolean; failure: string;
}
export interface FObjectiveResult {
    id: string; completionRecorded: boolean; currentConditionValid: boolean;
    evaluationState: "Valid" | "Unknown"; state: "Running" | "Succeeded" | "Failed";
    progress: number; resultSequence: number; evaluatedAt: number;
    conditionVersion?: number;
}
export interface FCompanyKnowledge { objectiveResults: Record<string, FObjectiveResult> }
export interface FCompanyPlan {
    id: string; revision: number; sourceMissionId: string; sourceMissionRevision: number;
    assignments: Record<string, FCompanyAssignment>; reservePlatoonIds: string[];
    startedAt: number; lastReplannedAt: number;
}
export interface Passage { id: string; to: string; capacity: number; available: boolean; opensAt: number; closesAt: number }
export interface TacticalArea { id: string; goal: Area; capacity: number; passages: Passage[] }
export interface Policy {
    maxReportAge: number; reassessmentSeconds: number; holdSeconds: number; reassignmentSeconds: number;
    publishBudget: number; acceptanceSeconds: number; registrationSeconds: number; maximumRepairs: number;
    valueWeight: number; urgencyWeight: number; unlockWeight: number; riskWeight: number;
    opportunityWeight: number; reassignmentGain: number;
}
export const defaultPolicy: Policy = { maxReportAge: 3, reassessmentSeconds: 4, holdSeconds: 8,
    reassignmentSeconds: 15, publishBudget: 2, acceptanceSeconds: 30, registrationSeconds: 30, maximumRepairs: 2,
    valueWeight: 1, urgencyWeight: 2, unlockWeight: 1, riskWeight: 2, opportunityWeight: 1, reassignmentGain: 0.1 };
export interface PlatoonPort {
    readonly id: string;
    readonly membershipRevision: number;
    readonly teamId: number;
    read(): FPlatoonSituationReport;
    stage(assignment: FCompanyAssignment, playerAuthorized?: boolean): boolean;
    preparationReady(assignment: FCompanyAssignment): boolean;
    activate(assignment: FCompanyAssignment): boolean;
    permit(permit: FExecutionPermit): boolean;
    cancel(id: string, revision: number): boolean;
    discardCandidate(id: string, revision: number): void;
    reconfirm(assignment: FCompanyAssignment): boolean;
}
export interface RuleAreaSample {
    known: boolean; observedAt: number; presentIds: string[]; contested: boolean; gathered: boolean;
    searched: boolean;
    searchScopes?: string[];
    searchCounts?: Record<string, number>;
}
export interface RuleUnitSample { known: boolean; alive: boolean; teamId: number; location: Point; observedAt: number }
/** Private rule-authority input. This is never stored in CompanyKnowledge. */
export interface RuleInputs { areas: Record<string, RuleAreaSample>; units: Record<string, RuleUnitSample> }
export interface Claim { key: string; owner: string; amount: number; expiresAt: number; provisional: boolean; replaces?: string }
export function clone<T>(value: T): T { return value === undefined ? value : JSON.parse(JSON.stringify(value)) as T; }
export function finite(value: unknown): value is number { return typeof value === "number" && Number.isFinite(value); }
export function fresh(observed: number, now: number, age: number): boolean {
    return finite(observed) && observed >= 0 && observed <= now + 0.1 && now - observed <= age;
}
export function continuous(type: MissionType): boolean { return type === "Defend" || type === "SecureArea"; }
export function validArea(area: Area): boolean {
    return !!area && !!area.center && [area.center.x, area.center.y, area.center.z, area.radius].every(finite) && area.radius >= 1;
}
export function validPlatoonMission(m: FPlatoonMission, requireIdentity = false): boolean {
    return !!m && typeof m.id === "string" && m.id.length <= 128 && (!requireIdentity || !!m.id) &&
        Number.isInteger(m.revision) && m.revision >= 1 && typeof m.areaId === "string" && m.areaId.length <= 128 &&
        typeof m.preparationAreaId === "string" && m.preparationAreaId.length <= 128 && validArea(m.goal) && validArea(m.boundary) &&
        ["Move", "SecureArea", "Defend", "Search", "Withdraw", "Regroup"].includes(m.type) &&
        finite(m.deadline) && m.deadline >= 0 && Number.isInteger(m.minimumMobile) && m.minimumMobile >= 1 &&
        finite(m.minimumAmmo) && m.minimumAmmo >= 0 && m.minimumAmmo <= 1 && typeof m.hasBoundary === "boolean" &&
        ["HoldFire", "SelfDefense", "FireAtWill"].includes(m.engagement) &&
        (m.searchAreaIds === undefined || (Array.isArray(m.searchAreaIds) && m.searchAreaIds.length <= 64 &&
            m.searchAreaIds.every(id => typeof id === "string" && id.length > 0 && id.length <= 128)));
}
export function inside(point: Point, area: Area): boolean {
    return (point.x - area.center.x) ** 2 + (point.y - area.center.y) ** 2 <= area.radius ** 2;
}
export function equivalent(left: unknown, right: unknown): boolean {
    const sorted = (v: unknown): unknown => Array.isArray(v) ? v.map(sorted) : v && typeof v === "object" ?
        Object.fromEntries(Object.entries(v).sort(([a], [b]) => a.localeCompare(b)).map(([k, f]) => [k, sorted(f)])) : v;
    return JSON.stringify(sorted(left)) === JSON.stringify(sorted(right));
}
export function validateMission(mission: FCompanyMission): string {
    if (!mission || !mission.id || mission.id.length > 128 || !Number.isInteger(mission.revision) || mission.revision < 1 ||
        !finite(mission.deadline) || mission.deadline < 0 || !Array.isArray(mission.objectives) ||
        mission.objectives.length < 1 || mission.objectives.length > 64) return "InvalidMission";
    if (mission.objectives.some(o => !o || typeof o.id !== "string")) return "InvalidObjective";
    const objectives = new Map(mission.objectives.map(o => [o.id, o]));
    if (objectives.size !== mission.objectives.length) return "DuplicateObjective";
    if ((mission.hasOperatingBoundary && !validArea(mission.operatingBoundary!)) ||
        (mission.maximumKnownRisk !== undefined && (!finite(mission.maximumKnownRisk) || mission.maximumKnownRisk < 0 || mission.maximumKnownRisk > 1)) ||
        (mission.rulesOfEngagement !== undefined && !["HoldFire", "SelfDefense", "FireAtWill"].includes(mission.rulesOfEngagement)) ||
        (mission.fallbackPolicy !== undefined && !["Hold", "Recover", "Withdraw"].includes(mission.fallbackPolicy)) ||
        (mission.fallbackPolicy && mission.fallbackPolicy !== "Hold" && !mission.fallbackAreaId) ||
        [mission.successCriteria, mission.failureCriteria].some(ids => ids !== undefined &&
            (!Array.isArray(ids) || ids.length > 64 || ids.some(id => !objectives.has(id))))) return "InvalidMissionCriteria";
    for (const o of mission.objectives) {
        const r = o.rule, m = o.mission;
        if (!o.id || o.id.length > 128 || !finite(o.priority) || !Number.isInteger(o.minimumPlatoons) || o.minimumPlatoons < 1 ||
            o.minimumPlatoons > 64 || !Number.isInteger(o.minimumMobile) || o.minimumMobile < 1 || !validPlatoonMission(m) || !m || !validArea(m.goal) ||
            !validArea(m.boundary) || !["Move", "SecureArea", "Defend", "Search", "Withdraw", "Regroup"].includes(m.type) ||
            !finite(m.deadline) || m.deadline < 0 || !Number.isInteger(m.minimumMobile) || m.minimumMobile < 1 ||
            !finite(m.minimumAmmo) || m.minimumAmmo < 0 || m.minimumAmmo > 1 ||
            !["HoldFire", "SelfDefense", "FireAtWill"].includes(m.engagement) || !r ||
            !["Arrive", "Regroup", "Capture", "Maintain", "Search", "Escort", "All", "Any"].includes(r.type) ||
            !Number.isInteger(r.minimumPresent) || r.minimumPresent < 1 || !finite(r.requiredFraction) ||
            r.requiredFraction <= 0 || r.requiredFraction > 1 || !finite(r.holdSeconds) || r.holdSeconds < 0 ||
            !finite(r.graceSeconds) || r.graceSeconds < 0 || !Array.isArray(r.children) || !Array.isArray(r.checkpoints) ||
            !Array.isArray(r.participants) || new Set(r.participants).size !== r.participants.length ||
            r.participants.length > 512 || r.children.length > 64 || r.checkpoints.length > 64 ||
            ((r.type === "All" || r.type === "Any") && r.children.length === 0) ||
            (r.type === "Escort" && (r.participants.length === 0 || r.checkpoints.length === 0)) ||
            !Array.isArray(o.dependencies) || o.dependencies.length > 64) return "InvalidObjective:" + o.id;
        if (r.children.some(id => !objectives.has(id)) || o.dependencies.some(d => !objectives.has(d.id) ||
            !["BeforeStartOnly", "MaintainDuringExecution"].includes(d.mode))) return "MissingDependency:" + o.id;
    }
    const visiting = new Set<string>(), visited = new Set<string>();
    const visit = (id: string): boolean => {
        if (visiting.has(id)) return false;
        if (visited.has(id)) return true;
        visiting.add(id);
        const o = objectives.get(id)!;
        for (const dependency of [...o.rule.children, ...o.dependencies.map(d => d.id)]) if (!visit(dependency)) return false;
        visiting.delete(id); visited.add(id); return true;
    };
    return mission.objectives.every(o => visit(o.id)) ? "" : "DependencyCycle";
}

/** Unreal JSON uses reflected names; normalize casing without changing values. */
export function nativeValue<T>(text: string): T {
    const normalize = (value: unknown): unknown => {
        if (Array.isArray(value)) return value.map(normalize);
        if (value && typeof value === "object") {
            const result: Record<string, unknown> = {};
            for (const [key, field] of Object.entries(value)) {
                const name = typeof field === "boolean" && /^b[A-Z]/.test(key) ? key.slice(1) : key;
                result[name.charAt(0).toLowerCase() + name.slice(1).replace(/ID/g, "Id")] = normalize(field);
            }
            return result;
        }
        return value;
    };
    return normalize(JSON.parse(text)) as T;
}
