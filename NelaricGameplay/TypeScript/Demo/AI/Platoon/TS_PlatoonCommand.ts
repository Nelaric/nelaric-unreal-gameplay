// Copyright (c) 2026 Nelaric Contributors

import * as UE from "ue";
import { PlatoonCoordinator, platoons, guidKey } from "./PlatoonCoordinator";
import { FPlatoonMission, nativeValue } from "../Company/CommandContracts";

/** Body-free platoon assembly; one native membership and one authored tree. */
class TS_PlatoonCommand extends UE.Actor {
    @UE.uproperty.uproperty(UE.uproperty.EditAnywhere, UE.uproperty.BlueprintReadWrite)
    Squads!: UE.TArray<UE.DemoSquadCommandActor>;
    @UE.uproperty.uproperty(UE.uproperty.EditAnywhere, UE.uproperty.BlueprintReadWrite)
    PlatoonId!: string;
    @UE.uproperty.uproperty(UE.uproperty.EditAnywhere, UE.uproperty.BlueprintReadWrite)
    TeamId!: number;
    @UE.uproperty.uproperty(UE.uproperty.EditAnywhere, UE.uproperty.BlueprintReadWrite)
    MaxReportAge!: number;
    @UE.uproperty.uproperty(UE.uproperty.EditAnywhere, UE.uproperty.BlueprintReadWrite)
    MinimumMobileMembers!: number;
    @UE.uproperty.uproperty(UE.uproperty.EditAnywhere, UE.uproperty.BlueprintReadWrite)
    MinimumAmmoReadiness!: number;
    @UE.uproperty.uproperty(UE.uproperty.VisibleInstanceOnly, UE.uproperty.BlueprintReadOnly)
    LastCommandMessage!: string;

    StartCommander(): boolean {
        if (!this.HasAuthority() || !UE.KismetSystemLibrary.IsServer(this)) return false;
        if (platoons.has(this)) return true;
        const tree = this.GetComponentByClass(UE.GameAIStateTreeComponent.StaticClass()) as UE.GameAIStateTreeComponent;
        const membership = this.GetComponentByClass(UE.DemoCompanyMembershipComponent.StaticClass()) as UE.DemoCompanyMembershipComponent;
        if (!UE.KismetSystemLibrary.IsValid(tree) || !UE.KismetSystemLibrary.IsValid(membership)) {
            this.LastCommandMessage = "Configure the StateTree and company membership component."; return false;
        }
        const squads = Array.from(this.Squads);
        if (!squads.length || squads.some(s => !UE.KismetSystemLibrary.IsValid(s))) return false;
        const team = squads[0].GetSquadContext().GetSituationReport().TeamId;
        if (team === 255 || (this.TeamId >= 0 && this.TeamId !== team)) {
            this.LastCommandMessage = "Platoon team does not match configured squads."; return false;
        }
        if (!this.PlatoonId) this.PlatoonId = guidKey(UE.KismetGuidLibrary.NewGuid());
        const age = this.MaxReportAge > 0 ? this.MaxReportAge : 3;
        const mobile = this.MinimumMobileMembers > 0 ? this.MinimumMobileMembers : 1;
        const ammo = this.MinimumAmmoReadiness;
        if (![age, mobile, ammo].every(Number.isFinite) || age > 30 || !Number.isInteger(mobile) ||
            mobile > 512 || ammo < 0 || ammo > 1) return false;
        const coordinator = new PlatoonCoordinator(this, this.PlatoonId, team,
            { maxReportAge: age, minimumMobile: mobile, minimumAmmo: ammo, retryDelay: 5, maximumRepairs: 2 }, membership);
        if (!coordinator.bind(squads)) { this.LastCommandMessage = coordinator.message; return false; }
        platoons.set(this, coordinator);
        membership.StepHandler.Bind(operation => {
            const result = coordinator.step(operation);
            this.LastCommandMessage = coordinator.describe();
            return result === undefined ? UE.EStateTreeRunStatus.Running : result ? UE.EStateTreeRunStatus.Succeeded : UE.EStateTreeRunStatus.Failed;
        });
        tree.StartLogic();
        if (!tree.IsRunning()) { membership.StepHandler.Unbind(); coordinator.shutdown(); platoons.delete(this); this.LastCommandMessage = "Platoon tree failed to start."; return false; }
        this.LastCommandMessage = "Virtual platoon ready."; return true;
    }
    SubmitMission(mission: UE.DemoPlatoonMission): boolean {
        try { return platoons.get(this)?.submitMission(nativeValue<FPlatoonMission>(UE.DemoCommandLibrary.EncodePlatoonMission(mission))) ?? false; }
        catch { return false; }
    }
    MoveToArea(center: UE.Vector, radius: number, duration: number): boolean { return this.issue(UE.EDemoSquadMissionType.Move, center, radius, duration); }
    RegroupAt(center: UE.Vector, radius: number, duration: number): boolean { return this.issue(UE.EDemoSquadMissionType.Regroup, center, radius, duration); }
    DefendArea(center: UE.Vector, radius: number, duration: number): boolean { return this.issue(UE.EDemoSquadMissionType.Defend, center, radius, duration); }
    SecureArea(center: UE.Vector, radius: number, duration: number): boolean { return this.issue(UE.EDemoSquadMissionType.Control, center, radius, duration); }
    SearchArea(center: UE.Vector, radius: number, duration: number): boolean { return this.issue(UE.EDemoSquadMissionType.Search, center, radius, duration); }
    WithdrawToArea(center: UE.Vector, radius: number, duration: number): boolean { return this.issue(UE.EDemoSquadMissionType.Withdraw, center, radius, duration); }
    private issue(type: UE.EDemoSquadMissionType, center: UE.Vector, radius: number, duration: number): boolean {
        const accepted = platoons.get(this)?.submit(type, center, radius, duration) ?? false;
        this.LastCommandMessage = accepted ? "Mission accepted." : "Mission rejected by authority or capability constraints.";
        return accepted;
    }
    CancelMission(): void { if (!UE.KismetSystemLibrary.IsValid((this.GetComponentByClass(
        UE.DemoCompanyMembershipComponent.StaticClass()) as UE.DemoCompanyMembershipComponent)?.GetCompanySource())) platoons.get(this)?.cancel(); }
    ConfirmCommander(): boolean { return platoons.get(this)?.confirmLeadership() ?? false; }
    GetSituationReport(): string { return JSON.stringify(platoons.get(this)?.situation() ?? {}); }
    GetDebugStatus(): string { return platoons.get(this)?.describe() ?? this.LastCommandMessage; }
    StopCommander(): void {
        const tree = this.GetComponentByClass(UE.GameAIStateTreeComponent.StaticClass()) as UE.GameAIStateTreeComponent;
        if (UE.KismetSystemLibrary.IsValid(tree)) tree.StopLogic("Platoon stopped");
        const membership = this.GetComponentByClass(UE.DemoCompanyMembershipComponent.StaticClass()) as UE.DemoCompanyMembershipComponent;
        if (UE.KismetSystemLibrary.IsValid(membership)) membership.StepHandler.Unbind();
        platoons.get(this)?.shutdown(); platoons.delete(this);
    }
    ReceiveEndPlay(_reason: UE.EEndPlayReason): void { this.StopCommander(); }
}
export default TS_PlatoonCommand;
