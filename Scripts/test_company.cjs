// Copyright (c) 2026 Nelaric Contributors

const test = require('node:test');
const assert = require('node:assert/strict');
const path = require('node:path');
const core = process.env.COMPANY_CORE_ROOT || path.join(__dirname, '../NelaricGameplay/Content/JavaScript/Demo/AI/Company');
const { CompanyCoordinator } = require(path.join(core, 'CompanyCoordinator.js'));
const { ObjectiveRules } = require(path.join(core, 'ObjectiveRules.js'));
const { ResourceLedger } = require(path.join(core, 'ResourceLedger.js'));
const { clone, validateMission } = require(path.join(core, 'CommandContracts.js'));

function objective(id, type = 'Move', areaId = id) {
    const goal = { center: { x: 0, y: 0, z: 0 }, radius: 2000 };
    return { id, priority: 10, mandatory: true, minimumPlatoons: 1, minimumMobile: 1,
        mission: { id: '', revision: 1, type, areaId, preparationAreaId: '', goal, boundary: goal,
            deadline: 0, engagement: 'SelfDefense', hasBoundary: false, minimumMobile: 1, minimumAmmo: 0 },
        dependencies: [], rule: { type: type === 'Defend' ? 'Maintain' : 'Arrive', areaId,
            minimumPresent: 1, requiredFraction: 1, participants: [], holdSeconds: 0, graceSeconds: 0,
            children: [], checkpoints: [], continuous: type === 'Defend', publicResult: true } };
}
function mission(objectives) { return { id: 'mission', revision: 1, deadline: 0, objectives }; }
class Port {
    constructor(id, time) {
        this.id = id; this.time = time; this.membershipRevision = 1; this.teamId = 0; this.activations = 0;
        this.report = { platoonId: id, membershipRevision: 1, runId: 'run', commandEpoch: 1,
            reportSequence: 0, taskSequence: 0, observedAt: 1, taskObservedAt: 1,
            commandAvailable: true, teamId: 0, effective: 4, mobile: 4, ammo: 1, integrity: 1,
            recovery: 0, knownRisk: 0, location: { x: 0, y: 0, z: 0 }, assignmentId: '', assignmentRevision: 0,
            state: 'Accepted', ready: false, preparationReady: true, failure: '' };
        this.report.gateVersion = 0;
        this.stale = false; this.accepts = true; this.prepared = true;
    }
    read() { ++this.report.reportSequence; if (!this.stale) this.report.observedAt = this.time.now;
        this.report.taskObservedAt = this.time.now; return clone(this.report); }
    stage(a) { if (!this.accepts) return false; this.candidate = clone(a); return true; }
    preparationReady(a) { return this.prepared && this.candidate?.id === a.id; }
    activate(a) { if (!this.candidate || this.candidate.id !== a.id) return false;
        this.active = clone(a); this.candidate = undefined; ++this.activations;
        this.report.gateVersion = a.permit.gateVersion;
        Object.assign(this.report, { assignmentId: a.id, assignmentRevision: a.revision,
            state: 'Executing', ready: true, taskSequence: this.report.taskSequence + 1 }); return true; }
    permit(p) { if (this.active?.id !== p.assignmentId || this.active.revision !== p.assignmentRevision) return false;
        this.active.permit = clone(p); this.report.ready = p.allowed; this.report.gateVersion = p.gateVersion; ++this.report.taskSequence; return true; }
    cancel(id, revision) { if (this.active?.id !== id || this.active.revision !== revision) return false;
        this.active = undefined; this.report.state = 'Cancelled'; ++this.report.taskSequence; return true; }
    discardCandidate(id) { if (this.candidate?.id === id) this.candidate = undefined; }
    reconfirm(a) { this.active = clone(a); this.report.runId = a.runId; this.report.commandEpoch = a.commandEpoch;
        this.report.assignmentId = a.id; this.report.assignmentRevision = a.revision; this.report.ready = false;
        this.report.gateVersion = a.permit.gateVersion;
        ++this.report.taskSequence; return true; }
}
function fixture(ids = ['p1', 'p2', 'p3'], definitions = ['A', 'B', 'C']) {
    const time = { now: 1 }; let serial = 0;
    const areas = new Map(definitions.map(id => [id, { id, goal: objective(id).mission.goal, capacity: 8, passages: [] }]));
    const co = new CompanyCoordinator('company', 'run', 0, ids, areas, {}, () => 'id' + (++serial), () => time.now, () => true);
    const ports = ids.map(id => new Port(id, time));
    ports.forEach(p => assert.equal(co.register(p), true)); assert.equal(co.completeRegistration(), true);
    function advance(steps = 30) { for (let i = 0; i < steps; ++i) { co.step(co.context.nextPhase); time.now += 0.1; } }
    return { co, ports, time, advance, areas };
}

test('virtual hierarchy assigns concurrent defense and movement without body identities', () => {
    const { co, ports, advance } = fixture();
    assert.equal(co.submit(mission([objective('A', 'Defend'), objective('B')])), true); advance();
    const tasks = Object.values(co.context.plan.assignments);
    assert.equal(tasks.length, 2); assert.deepEqual(new Set(tasks.map(a => a.mission.type)), new Set(['Defend', 'Move']));
    const before = tasks.map(a => [a.platoonId, a.id, a.revision]); const activations = ports.map(p => p.activations);
    advance(200); assert.deepEqual(Object.values(co.context.plan.assignments).map(a => [a.platoonId, a.id, a.revision]), before);
    assert.deepEqual(ports.map(p => p.activations), activations);
});
test('registration is explicit and empty bootstrap is not army destruction', () => {
    const time = { now: 0 }, co = new CompanyCoordinator('c', 'r', 0, [], new Map(), {}, () => 'id', () => time.now, () => true);
    assert.equal(co.completeRegistration(), false); assert.equal(co.step('Bootstrap'), undefined);
    time.now = 31; assert.equal(co.step('Bootstrap'), true); assert.equal(co.context.message, 'RegistrationTimeout');
    assert.equal(co.context.outcome, 'Running');
});
test('local platoon failure changes only the affected objective and hands off before releasing old duty', () => {
    const f = fixture(); f.co.submit(mission([objective('A', 'Defend'), objective('B')])); f.advance();
    const a = Object.values(f.co.context.plan.assignments).find(a => a.objectiveId === 'A');
    const b = Object.values(f.co.context.plan.assignments).find(a => a.objectiveId === 'B');
    const bad = f.ports.find(p => p.id === b.platoonId);
    Object.assign(bad.report, { state: 'Failed', mobile: 0, taskSequence: bad.report.taskSequence + 1 });
    f.advance(50);
    assert.equal(f.co.context.plan.assignments[a.platoonId].id, a.id);
    assert.equal(f.co.context.plan.assignments[a.platoonId].revision, a.revision);
    assert.ok(Object.values(f.co.context.plan.assignments).some(x => x.objectiveId === 'B' && x.platoonId !== b.platoonId));
});
test('expired capability becomes unknown and revokes execution without cancelling unrelated assignments', () => {
    const f = fixture(['p1', 'p2']); f.co.submit(mission([objective('A', 'Defend'), objective('B')])); f.advance();
    const first = f.ports[0], other = f.ports[1].active.id;
    first.stale = true; f.time.now += 10; f.advance();
    assert.equal(f.co.context.plan.assignments[first.id].permit.allowed, false);
    assert.equal(f.co.context.plan.assignments[first.id].ready, false);
    assert.equal(f.ports[1].active.id, other);
    assert.ok(JSON.parse(f.co.describe()).reports.some(r => r.platoonId === first.id && r.freshness === 'Unknown'));
});
test('same capability sample can deliver a new terminal result and an old task cannot replace current evidence', () => {
    const f = fixture(['p1']); f.co.submit(mission([objective('A')])); f.advance();
    const current = f.co.context.reports.get('p1'), report = clone(current);
    report.taskSequence += 10; report.state = 'Succeeded'; report.taskObservedAt = f.time.now;
    assert.equal(f.co.enqueue(report), true); f.co.step('Supervise');
    assert.equal(f.co.context.reports.get('p1').state, 'Succeeded');
    const old = clone(report); old.assignmentId = 'obsolete'; old.taskSequence -= 1;
    f.co.enqueue(old); f.co.step('Supervise');
    assert.equal(f.co.context.reports.get('p1').assignmentId, report.assignmentId);
});
test('a partial force group consumes no resource and a platoon cannot be allocated twice', () => {
    const f = fixture(['p1']); const a = objective('A'); a.minimumPlatoons = 2;
    f.co.submit(mission([a, objective('B')])); f.advance();
    assert.equal(Object.values(f.co.context.plan.assignments).length, 1);
    assert.equal(Object.values(f.co.context.plan.assignments)[0].objectiveId, 'B');
    assert.ok(f.co.ledger.snapshot().every(c => c.key === 'B'));
});
test('candidate rejection and preparation timeout retain existing active responsibilities', () => {
    const f = fixture(['p1', 'p2']); f.co.submit(mission([objective('A', 'Defend')])); f.advance();
    const old = f.ports[0].active.id; f.ports[1].prepared = false;
    const next = mission([objective('A', 'Defend'), objective('B')]); next.revision = 2;
    f.co.submit(next); f.advance(); assert.equal(f.ports[0].active.id, old);
    f.time.now += 31; f.advance(); assert.equal(f.ports[0].active.id, old);
    f.time.now += 31; f.advance(); assert.equal(f.ports[0].active.id, old);
    assert.equal(f.co.context.candidates.length, 0);
});
test('manual scope is local and ordinary allocation cannot overwrite it', () => {
    const f = fixture(); f.co.submit(mission([objective('A'), objective('B')])); f.advance();
    const old = f.ports[0].active.id, epoch = f.co.context.commandEpoch;
    f.co.lockPlatoon('p1', true); f.ports[0].report.mobile = 0; f.advance(200);
    assert.equal(f.ports[0].active.id, old); assert.equal(f.co.context.commandEpoch, epoch);
});
test('dependency cycles and missing prerequisites are rejected before mutation', () => {
    const a = objective('A'), b = objective('B');
    a.dependencies = [{ id: 'B', mode: 'BeforeStartOnly', completion: true }];
    b.dependencies = [{ id: 'A', mode: 'BeforeStartOnly', completion: true }];
    assert.equal(validateMission(mission([a, b])), 'DependencyCycle');
    b.dependencies[0].id = 'missing'; assert.match(validateMission(mission([a, b])), /MissingDependency/);
});
test('capture is contested, unknown never accrues time, and recorded completion is separate from current control', () => {
    const a = objective('A', 'Defend'); a.rule.holdSeconds = 2;
    const rules = new ObjectiveRules(0); rules.configure(mission([a]));
    const input = { areas: { A: { known: true, observedAt: 0, presentIds: ['soldier'], contested: false, gathered: true, searched: false } }, units: {} };
    function at(now, contested = false, known = true) {
        Object.assign(input.areas.A, { observedAt: now, contested, known }); return rules.evaluate(now, input).A;
    }
    assert.equal(at(0).completionRecorded, false); at(1);
    assert.equal(at(2, true).currentConditionValid, false); at(3, false, false); at(4);
    assert.equal(at(5).completionRecorded, false);
    assert.equal(at(6).completionRecorded, true);
    const lost = at(7, true); assert.equal(lost.completionRecorded, true); assert.equal(lost.currentConditionValid, false);
    assert.equal(rules.outcome(7), 'Running'); assert.equal(at(8).currentConditionValid, true);
    assert.equal(rules.outcome(8), 'Succeeded');
});
test('escort uses explicit ordinary-soldier identities and committed death fails the rule', () => {
    const a = objective('A'); Object.assign(a.rule, { type: 'Escort', participants: ['s1'], checkpoints: ['A', 'B'] });
    const rules = new ObjectiveRules(0); rules.configure(mission([a]));
    const input = { areas: { A: { known: true, observedAt: 1, presentIds: ['s1'], contested: false, gathered: true, searched: false } },
        units: { s1: { known: true, alive: true, teamId: 0, observedAt: 1, location: { x: 0, y: 0, z: 0 } } } };
    assert.equal(rules.evaluate(1, input).A.completionRecorded, false);
    input.units.s1.alive = false; input.units.s1.observedAt = 2;
    assert.equal(rules.evaluate(2, input).A.state, 'Failed'); assert.equal(rules.outcome(2), 'Failed');
});
test('private rule inputs are excluded from company knowledge and rules do not issue commands', () => {
    const a = objective('A'); a.rule.publicResult = false;
    const rules = new ObjectiveRules(0); rules.configure(mission([a]));
    assert.deepEqual(rules.evaluate(1, { areas: { A: { known: true, observedAt: 1, presentIds: ['s'], contested: false,
        gathered: true, searched: false } }, units: {} }), {});
    assert.equal(rules.outcome(1), 'Succeeded'); assert.ok(!JSON.stringify(rules.save()).includes('presentIds'));
});
test('resource reservation checks the entire group and shared passage capacity', () => {
    const f = fixture(); f.areas.get('A').capacity = 1;
    f.areas.get('A').passages = [{ id: 'bridge', to: 'B', capacity: 1, available: true, opensAt: 0, closesAt: 0 }];
    const l = new ResourceLedger(f.areas), claim = (key, owner) => ({ key, owner, amount: 1, expiresAt: 10, provisional: true });
    assert.equal(l.reserve([claim('A', 'a'), claim('A', 'b')], 1), false); assert.equal(l.snapshot().length, 0);
    assert.equal(l.reserve([claim('bridge', 'a')], 1), true); assert.equal(l.reserve([claim('bridge', 'b')], 1), false);
    l.release('a'); assert.equal(l.reserve([claim('bridge', 'b')], 1), true);
});
test('restore preserves semantic assignment identities and does not republish every task', () => {
    const f = fixture(['p1']); f.co.submit(mission([objective('A', 'Defend')])); f.advance();
    const identity = f.ports[0].active.id, before = f.ports[0].activations, saved = f.co.save();
    assert.equal(f.co.restore(saved), true); f.advance(50);
    assert.equal(f.ports[0].active.id, identity); assert.equal(f.ports[0].activations, before);
    const corrupt = JSON.parse(saved); corrupt.companyId = 'enemy'; assert.equal(f.co.restore(JSON.stringify(corrupt)), false);
});
test('Hold retries allocation after fresh registration facts arrive', () => {
    const f = fixture(['p1']); f.ports[0].report.commandAvailable = false;
    f.co.submit(mission([objective('A')])); f.advance();
    assert.equal(Object.keys(f.co.context.plan.assignments).length, 0);
    f.ports[0].report.commandAvailable = true; f.advance(150);
    assert.equal(Object.keys(f.co.context.plan.assignments).length, 1);
});
test('a sustained dependency closes only its dependent permit and preserves stage history', () => {
    const f = fixture(['p1', 'p2']); const a = objective('A', 'Defend'), b = objective('B');
    b.dependencies = [{ id: 'A', mode: 'MaintainDuringExecution', completion: false }];
    f.co.submit(mission([a, b]));
    const result = { id: 'A', completionRecorded: true, currentConditionValid: true, evaluationState: 'Valid',
        state: 'Succeeded', progress: 1, resultSequence: 1, evaluatedAt: f.time.now };
    f.co.publishRuleResults({ A: result }, 'Running'); f.advance(20);
    const activeA = Object.values(f.co.context.plan.assignments).find(a => a.objectiveId === 'A');
    const activeB = Object.values(f.co.context.plan.assignments).find(a => a.objectiveId === 'B');
    assert.ok(activeB);
    f.co.publishRuleResults({ A: { ...result, currentConditionValid: false, resultSequence: 2, evaluatedAt: f.time.now } }, 'Running');
    f.advance(10);
    assert.equal(f.co.context.plan.assignments[activeB.platoonId].permit.allowed, false);
    assert.equal(f.co.context.plan.assignments[activeA.platoonId].id, activeA.id);
    assert.equal(f.co.context.knowledge.objectiveResults.A.completionRecorded, true);
});
test('malformed restores are rejected before changing command state, including a missing permit', () => {
    const f = fixture(['p1']); f.co.submit(mission([objective('A', 'Defend')])); f.advance();
    const before = f.co.save(), damaged = JSON.parse(before);
    delete Object.values(damaged.plan.assignments)[0].permit;
    assert.equal(f.co.restore(JSON.stringify(damaged)), false);
    assert.equal(f.co.save(), before);
    assert.equal(f.co.restore('{"schema":1,"mission":null}'), false);
    const empty = fixture(['p1']); assert.equal(empty.co.restore(empty.co.save()), true);
});
test('rule participant manifests never silently shrink after casualty or reassignment', () => {
    const a = objective('A'), rules = new ObjectiveRules(0); rules.configure(mission([a]));
    assert.equal(rules.bindParticipants('A', ['s1', 's2']), true);
    rules.bindParticipants('A', ['s1']);
    const result = rules.evaluate(1, { areas: { A: { known: true, observedAt: 1, presentIds: ['s1', 'unrelated'],
        contested: false, gathered: true, searched: false } }, units: {} });
    assert.equal(result.A.completionRecorded, false);
});
test('search coverage requires the complete platoon group and survives semantic restore', () => {
    const a = objective('A', 'Search'); a.rule.type = 'Search'; a.minimumPlatoons = 2; a.rule.checkpoints = ['A', 'B'];
    const rules = new ObjectiveRules(0); rules.configure(mission([a]));
    const sample = n => ({ known: true, observedAt: 1, presentIds: [], contested: false, gathered: false, searched: false,
        searchCounts: { 'mission/1/A': n } });
    const inputs = { areas: { A: sample(1), B: sample(0) }, units: {} };
    assert.equal(rules.evaluate(1, inputs).A.progress, 0);
    inputs.areas.A = sample(2); assert.equal(rules.evaluate(1, inputs).A.progress, 0.5);
    const restored = new ObjectiveRules(0); restored.configure(mission([a])); assert.equal(restored.restore(rules.save()), true);
    inputs.areas.A = sample(0); inputs.areas.B = sample(2);
    assert.equal(restored.evaluate(1, inputs).A.completionRecorded, true);
});
test('infeasible intent defaults to Hold and only an explicit policy permits fallback', () => {
    const f = fixture(['p1']); const o = objective('A'); o.minimumPlatoons = 2;
    f.co.submit(mission([o])); f.advance(); assert.equal(Object.keys(f.co.context.plan.assignments).length, 0);
    const authorized = { ...mission([o]), revision: 2, fallbackPolicy: 'Recover', fallbackAreaId: 'B' };
    f.co.submit(authorized); f.advance();
    const task = Object.values(f.co.context.plan.assignments)[0];
    assert.equal(task.mission.type, 'Regroup'); assert.equal(task.mission.areaId, 'B'); assert.equal(task.permit.allowed, true);
    f.advance(200); assert.equal(Object.values(f.co.context.plan.assignments)[0].id, task.id);
});
test('explicit failure conditions are settled by rule authority', () => {
    const a = objective('A'), b = objective('B');
    const rules = new ObjectiveRules(0); rules.configure({ ...mission([a, b]), successCriteria: ['A'], failureCriteria: ['B'] });
    rules.evaluate(1, { areas: { A: { known: true, observedAt: 1, presentIds: ['s'], contested: false, gathered: true, searched: false },
        B: { known: true, observedAt: 1, presentIds: ['s'], contested: false, gathered: true, searched: false } }, units: {} });
    assert.equal(rules.outcome(1), 'Failed');
});
test('membership revocation is exact and a configured replacement may register', () => {
    const f = fixture(['p1']); f.co.submit(mission([objective('A')])); f.advance();
    assert.equal(f.co.unregister('p1', 2), false); assert.equal(f.co.unregister('p1', 1), true);
    assert.equal(f.co.context.registrationComplete, false);
    const replacement = new Port('p1', f.time); replacement.membershipRevision = 2;
    replacement.report.membershipRevision = 2;
    assert.equal(f.co.register(replacement), true); assert.equal(f.co.completeRegistration(), true);
    f.advance(30); assert.equal(Object.keys(f.co.context.plan.assignments).length, 1);
});
test('late readiness from a revoked gate cannot release a continuing responsibility', () => {
    const f = fixture(['p1']); f.co.submit(mission([objective('A', 'Defend')])); f.advance();
    const a = Object.values(f.co.context.plan.assignments)[0], report = clone(f.co.context.reports.get('p1'));
    a.permit.allowed = false; ++a.permit.gateVersion;
    report.taskSequence += 10; report.taskObservedAt = f.time.now; report.ready = true;
    f.co.enqueue(report); f.co.step('Supervise');
    assert.equal(a.ready, false);
});
test('repeated actual execution failure reaches a bounded company repair limit', () => {
    const f = fixture(['p1']); f.co.submit(mission([objective('A')])); f.advance();
    for (let i = 0; i < 2; ++i) {
        f.ports[0].report.state = 'Failed'; ++f.ports[0].report.taskSequence; f.advance(50);
    }
    const count = f.ports[0].activations; f.advance(400);
    assert.ok(f.co.context.repairs.A >= 2); assert.equal(f.ports[0].activations, count);
    assert.equal(f.co.context.nextPhase, 'Hold');
});
test('player tasks pass the same operating boundary, engagement and continuing-duty constraints', () => {
    const f = fixture(['p1']); const a = objective('A', 'Defend');
    const global = { ...mission([a]), hasOperatingBoundary: true, operatingBoundary: a.mission.goal,
        rulesOfEngagement: 'HoldFire', maximumKnownRisk: 0.5 };
    f.co.submit(global); f.advance(); f.co.lockPlatoon('p1', true); f.advance(5);
    const outside = { ...clone(a.mission), id: 'manual', revision: 1, goal: { center: { x: 9999, y: 0, z: 0 }, radius: 500 } };
    assert.equal(f.co.submitManual('p1', outside), false);
    assert.equal(f.co.submitManual('p1', { ...clone(a.mission), id: 'manual', revision: 1, type: 'Move' }), false);
    assert.equal(f.co.submitManual('p1', { ...clone(a.mission), id: 'manual', revision: 1, engagement: 'FireAtWill' }), true);
    f.advance(); assert.equal(f.ports[0].active.mission.engagement, 'HoldFire');
});

test('revising one recipient transfers its resource ownership and ends the commit without a self handoff', () => {
    const f = fixture(['p1']); f.areas.get('A').capacity = 1;
    f.co.submit(mission([objective('A')])); f.advance();
    const before = clone(f.co.context.plan.assignments.p1);
    const changed = objective('A'); changed.mission.goal.center.x = 100;
    f.co.submit({ ...mission([changed]), revision: 2 }); f.advance();
    const after = f.co.context.plan.assignments.p1;
    assert.equal(after.id, before.id); assert.equal(after.revision, before.revision + 1);
    assert.equal(f.co.context.candidates.length, 0);
    assert.equal(f.co.context.nextPhase, 'Supervise');
    assert.deepEqual(f.co.ledger.snapshot().map(c => c.owner), [after.id + '/' + after.revision]);
    assert.equal(f.ports[0].activations, 2);
});
