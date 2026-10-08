// Copyright (c) 2026 Nelaric Contributors

import * as UE from "ue";
import { GrandWarfrontRules } from "./GrandWarfrontRules";

interface RosterSeat { id: string; team: number; squad: UE.DemoSquadCommandActor }
interface Role { seat: RosterSeat; body: UE.DemoCharacter; roam: boolean; index: number }
interface Route {
    scope: string; goal: UE.Vector; area: UE.DemoSquadArea; step: number;
    selectedAt: number; checkedAt: number; lastPosition: UE.Vector; stalled: number; chasing: boolean;
}
const valid = (o: UE.Object | undefined | null): boolean => !!o && UE.KismetSystemLibrary.IsValid(o);
const distance = (a: UE.Vector, b: UE.Vector): number => Math.hypot(a.X - b.X, a.Y - b.Y);

/** Faction-wide quotas; squad orders retain their native authority and leases. */
export class GrandWarfrontMovement {
    private roles = new Map<UE.DemoCharacter, Role>();
    private routes = new Map<string, Route>();
    private roamSeats = new Set<string>();
    private counts = new Map<number, string>();
    constructor(private readonly mode: UE.DemoGrandWarfrontGameMode) {}

    update(seats: RosterSeat[], rules: GrandWarfrontRules): void {
        this.roles.clear();
        if (rules.state.phase !== "RegionActive") { this.routes.clear(); return; }
        const mobile = new Set<UE.DemoCharacter>();
        for (const squad of new Set(seats.map(s => s.squad))) {
            const members = squad.GetSquadContext().GetMembers();
            for (let i = 0; i < members.Num(); ++i) {
                const member = members.Get(i), body = member.Character;
                if (body && member.bAlive && member.bMobile && !member.bPlayerControlled) mobile.add(body);
            }
        }
        for (const team of [rules.state.attackerTeamId, rules.state.defenderTeamId]) {
            const available = seats.filter(s => s.team === team).map(seat => ({ seat, body: this.mode.GetSeatBody(seat.id) }))
                .filter(p => valid(p.body) && mobile.has(p.body) && p.body.IsAlive() && !p.body.IsPlayerControlled() &&
                    p.body.GetPawnInitializationComponent().IsPawnInitialized());
            const desiredRoam = available.length - Math.ceil(available.length * 0.8);
            // Keep surviving roles stable. New vacancies use a distributed seat order.
            const candidates = [...available].sort((a, b) => Number(this.roamSeats.has(b.seat.id)) - Number(this.roamSeats.has(a.seat.id)) ||
                a.seat.id.split("_M")[1].localeCompare(b.seat.id.split("_M")[1]) || a.seat.id.localeCompare(b.seat.id));
            const roam = new Set(candidates.slice(0, desiredRoam).map(p => p.seat.id));
            for (const seat of seats.filter(s => s.team === team)) this.roamSeats.delete(seat.id);
            for (const id of roam) this.roamSeats.add(id);
            available.forEach((p, index) => this.roles.set(p.body, { ...p, roam: roam.has(p.seat.id), index }));
            const counts = `${available.length - desiredRoam}/${desiredRoam}`;
            if (this.counts.get(team) !== counts) {
                this.counts.set(team, counts);
                console.log(`[Warfront] Movement team=${team} capture/roam=${counts}`);
            }
        }
        const active = new Set([...this.roles.values()].map(r => r.seat.id));
        for (const id of this.routes.keys()) if (!active.has(id)) this.routes.delete(id);
    }

    resolve(body: UE.DemoCharacter, order: UE.DemoSquadMemberOrder, rules: GrandWarfrontRules): UE.DemoSquadMemberOrder {
        const role = this.roles.get(body);
        if (!role || rules.state.phase !== "RegionActive") return order;
        const now = UE.GameplayStatics.GetTimeSeconds(this.mode), location = body.K2_GetActorLocation();
        const mission = role.seat.squad.GetSquadContext().GetMission();
        // Use the company's assigned point, not hidden enemy transforms or a parallel objective selector.
        const point = [...rules.region.points].sort((a, b) =>
            Math.hypot(a.center.x - mission.Goal.Center.X, a.center.y - mission.Goal.Center.Y) -
            Math.hypot(b.center.x - mission.Goal.Center.X, b.center.y - mission.Goal.Center.Y))[0];
        const center = new UE.Vector(point.center.x, point.center.y, point.center.z);
        const scope = `${rules.state.regionRevision}/${point.id}/${role.roam}`;
        let route = this.routes.get(role.seat.id);
        const soldier = body.GetSoldierComponent(), memory = valid(soldier) ? soldier.GetMemory() : undefined;
        if (route?.scope === scope && valid(soldier) &&
            [UE.EDemoSoldierBehavior.Reload, UE.EDemoSoldierBehavior.AvoidGrenade].includes(soldier.GetBehavior())) {
            // Do not cancel necessary actions by issuing a fresh patrol waypoint.
            route.stalled = 0; route.checkedAt = now; route.lastPosition = location;
            order.Goal = new UE.DemoSquadArea(route.goal, 80); order.MovementArea = route.area;
            order.bAllowStopToFight = false; order.bAllowLocalReposition = false;
            return order;
        }
        const chasing = role.roam && !!memory?.bTargetVisible && distance(center, memory.TargetLocation) < 2500;
        let chaseGoal: UE.Vector | undefined;
        if (chasing && memory) {
            const target = memory.TargetLocation, dx = location.X - target.X, dy = location.Y - target.Y;
            const length = Math.max(1, Math.hypot(dx, dy));
            // Close to 4 m, then change firing position around the observed contact.
            const angle = length <= 500 ? 0.8 : 0;
            chaseGoal = new UE.Vector(target.X + (dx * Math.cos(angle) - dy * Math.sin(angle)) / length * 400,
                target.Y + (dx * Math.sin(angle) + dy * Math.cos(angle)) / length * 400, center.Z);
        }
        if (route && now - route.checkedAt >= 1) {
            route.stalled = distance(location, route.lastPosition) < 35 ? route.stalled + now - route.checkedAt : 0;
            route.lastPosition = location; route.checkedAt = now;
        }
        const arrived = route && (distance(location, route.goal) < 120 || soldier.GetOrderStatus() === UE.EDemoSoldierOrderStatus.Completed) && now - route.selectedAt >= 0.5;
        const targetChanged = route && chasing && chaseGoal && distance(route.goal, chaseGoal) > 300 && now - route.selectedAt >= 2;
        if (!route || route.scope !== scope || route.chasing !== chasing || arrived || targetChanged || route.stalled >= 6) {
            const step = (route?.step ?? -1) + 1;
            const angle = role.index * 2.399963 + step * 1.7;
            const radius = role.roam ? point.radius + 250 + (role.index % 3) * 120 : point.radius * (0.25 + (role.index % 3) * 0.1);
            const goal = chaseGoal ?? new UE.Vector(center.X + Math.cos(angle) * radius, center.Y + Math.sin(angle) * radius, center.Z);
            const areaCenter = new UE.Vector((location.X + center.X) / 2, (location.Y + center.Y) / 2, center.Z);
            const area = role.roam
                ? new UE.DemoSquadArea(areaCenter, Math.max(distance(areaCenter, location), distance(areaCenter, goal)) + 500)
                : new UE.DemoSquadArea(center, Math.max(30, point.radius - 70));
            route = { scope, goal, area, step, selectedAt: now, checkedAt: now, lastPosition: location, stalled: 0, chasing };
            this.routes.set(role.seat.id, route);
        }
        order.Goal = new UE.DemoSquadArea(route.goal, 80);
        order.MovementArea = route.area;
        // Keep moving and use the existing moving-fire executor; grenade avoidance and reload still apply.
        order.bAllowStopToFight = false;
        order.bAllowLocalReposition = false;
        return order;
    }

    clear(): void { this.roles.clear(); this.routes.clear(); this.roamSeats.clear(); this.counts.clear(); }
}
