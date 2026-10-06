// Copyright (c) 2026 Nelaric Contributors

import * as UE from "ue";
import { toDelegate, toManualReleaseDelegate, releaseManualReleaseDelegate } from "puerts";
import { issueMission, returnControl, takeControl } from "./SquadMissions";

type ReadyCallback = Parameters<UE.PawnInitializationComponent["RegisterAndCallPawnInitialized"]>[0];

interface MemberBinding {
    character: UE.DemoCharacter;
    initialization: UE.PawnInitializationComponent;
    callback: ReadyCallback;
    callbackFunction: (component: UE.PawnInitializationComponent | null) => void;
    joined: boolean;
    callbackReleased: boolean;
}

interface BootstrapState {
    timer?: UE.TimerHandle;
    startedAt: number;
    bindings: Map<number, MemberBinding>;
    commanderStarted: boolean;
    ending: boolean;
}

const states = new WeakMap<TS_SquadBootstrap, BootstrapState>();

function fail(actor: TS_SquadBootstrap, state: BootstrapState, reason: string): void {
    state.ending = true;
    actor.LastSetupMessage = reason;
    console.error("[SquadSetup] " + actor.GetName() + ": " + reason);
    stopWaiting(actor, state);
    state.bindings.forEach(binding => {
        if (!binding.joined || !UE.KismetSystemLibrary.IsValid(binding.character)) {
            return;
        }
        const member = binding.character.GetComponentByClass(
            UE.DemoSquadMemberComponent.StaticClass()
        ) as UE.DemoSquadMemberComponent;
        if (UE.KismetSystemLibrary.IsValid(member) && member.GetSquad() === actor.Squad) {
            member.LeaveSquad();
        }
        binding.joined = false;
    });
}

function stopWaiting(actor: TS_SquadBootstrap, state: BootstrapState): void {
    if (state.timer) {
        UE.KismetSystemLibrary.K2_ClearTimerHandle(actor, state.timer);
        state.timer = undefined;
    }
    state.bindings.forEach(binding => {
        if (UE.KismetSystemLibrary.IsValid(binding.initialization)) {
            if (!binding.callbackReleased) {
                binding.initialization.UnregisterPawnInitializationCallback(binding.callback);
                releaseManualReleaseDelegate(binding.callbackFunction);
                binding.callbackReleased = true;
            }
        }
    });
}

function configuredRole(actor: TS_SquadBootstrap, index: number): UE.PawnInitializationConfig {
    return index === 0 ? actor.LeaderConfig : index === 1 ? actor.DeputyConfig :
        index < 2 + actor.SupportMemberCount ? actor.SupportConfig : actor.RiflemanConfig;
}

function tryStart(actor: TS_SquadBootstrap, state: BootstrapState): void {
    if (state.ending || state.commanderStarted || state.bindings.size !== actor.SpawnPoints.Num() ||
        !Array.from(state.bindings.values()).every(binding => binding.joined &&
            UE.KismetSystemLibrary.IsValid(binding.character) &&
            binding.initialization.IsPawnInitialized())) {
        return;
    }
    const context = actor.Squad.GetSquadContext();
    context.SetCommandMode(actor.InitialCommandMode);
    if (!context.SetMission(actor.InitialMission)) {
        fail(actor, state, "Initial mission was rejected; check areas, revision and deadline.");
        return;
    }
    if (!actor.Squad.StartCommander()) {
        fail(actor, state, "Commander did not start; check StateTree and squad configuration.");
        return;
    }
    state.commanderStarted = true;
    actor.SetupSucceeded = true;
    actor.LastSetupMessage = "Registered " + state.bindings.size + " members; commander started.";
    console.log("[SquadSetup] " + actor.GetName() + ": " + actor.LastSetupMessage);
    stopWaiting(actor, state);
}

function pollMembers(actor: TS_SquadBootstrap, state: BootstrapState): void {
    if (state.ending || state.commanderStarted) {
        return;
    }
    if (UE.GameplayStatics.GetTimeSeconds(actor) - state.startedAt >= actor.SetupTimeoutSeconds) {
        fail(actor, state, "Timed out waiting for active, initialized squad members.");
        return;
    }
    for (let index = 0; index < actor.SpawnPoints.Num(); ++index) {
        if (state.bindings.has(index)) {
            continue;
        }
        const point = actor.SpawnPoints.Get(index);
        const character = point.GetSpawnedCharacter();
        if (!UE.KismetSystemLibrary.IsValid(character)) {
            continue;
        }
        if (Array.from(state.bindings.values()).some(binding => binding.character === character)) {
            fail(actor, state, "Spawn points resolved the same character twice.");
            return;
        }
        const initialization = character.GetPawnInitializationComponent();
        if (!UE.KismetSystemLibrary.IsValid(initialization)) {
            fail(actor, state, "Character has no pawn initialization component.");
            return;
        }
        // Components are installed before the pool lease is activated; retain GAS and possession readiness.
        const callbackFunction = (component: UE.PawnInitializationComponent | null): void => {
            if (component && UE.KismetSystemLibrary.IsValid(actor) && !state.ending) {
                TS_SquadBootstrap.prototype.OnMemberInitialized.call(actor, component);
            }
        };
        const callback = toManualReleaseDelegate(callbackFunction);
        state.bindings.set(index, { character, initialization, callback, callbackFunction, joined: false, callbackReleased: false });
        initialization.RegisterAndCallPawnInitialized(callback);
        if (state.ending || state.commanderStarted) {
            return;
        }
    }
    tryStart(actor, state);
}

/** Authored references and TS startup logic for one native squad executor. */
class TS_SquadBootstrap extends UE.Actor {
    @UE.uproperty.uproperty(UE.uproperty.EditAnywhere, UE.uproperty.BlueprintReadWrite)
    Squad!: UE.DemoSquadCommandActor;

    @UE.uproperty.uproperty(UE.uproperty.EditAnywhere, UE.uproperty.BlueprintReadWrite)
    SpawnPoints!: UE.TArray<UE.DemoInitialCharacterSpawnPoint>;

    @UE.uproperty.uproperty(UE.uproperty.EditAnywhere)
    LeaderConfig!: UE.PawnInitializationConfig;

    @UE.uproperty.uproperty(UE.uproperty.EditAnywhere)
    DeputyConfig!: UE.PawnInitializationConfig;

    @UE.uproperty.uproperty(UE.uproperty.EditAnywhere)
    SupportConfig!: UE.PawnInitializationConfig;

    @UE.uproperty.uproperty(UE.uproperty.EditAnywhere)
    RiflemanConfig!: UE.PawnInitializationConfig;

    @UE.uproperty.uproperty(UE.uproperty.EditAnywhere, UE.uproperty.BlueprintReadWrite)
    InitialMission!: UE.DemoSquadMission;

    @UE.uproperty.uproperty(UE.uproperty.EditAnywhere, UE.uproperty.BlueprintReadWrite)
    InitialCommandMode!: UE.EDemoSquadCommandMode;

    @UE.uproperty.uproperty(UE.uproperty.EditAnywhere, UE.uproperty.BlueprintReadWrite)
    SetupTimeoutSeconds!: number;

    @UE.uproperty.uproperty(UE.uproperty.EditAnywhere, UE.uproperty.BlueprintReadWrite)
    SupportMemberCount!: number;

    @UE.uproperty.uproperty(UE.uproperty.VisibleInstanceOnly, UE.uproperty.BlueprintReadOnly)
    SetupSucceeded!: boolean;

    @UE.uproperty.uproperty(UE.uproperty.VisibleInstanceOnly, UE.uproperty.BlueprintReadOnly)
    LastSetupMessage!: string;

    ReceiveBeginPlay(): void {
        this.StartSquad();
    }

    @UE.ufunction.ufunction(UE.ufunction.BlueprintCallable)
    StartSquad(): void {
        if (!this.HasAuthority() || states.has(this)) {
            return;
        }
        this.SetupSucceeded = false;
        const points = Array.from(this.SpawnPoints);
        const configs = [this.LeaderConfig, this.DeputyConfig, this.SupportConfig, this.RiflemanConfig];
        const errors: string[] = [];
        const squadValid = UE.KismetSystemLibrary.IsValid(this.Squad);
        if (!squadValid) errors.push("Squad reference missing");
        if (points.length === 0) errors.push("SpawnPoints empty");
        if (points.some(point => !UE.KismetSystemLibrary.IsValid(point))) errors.push("SpawnPoints contain invalid references");
        if (new Set(points).size !== points.length) errors.push("SpawnPoints duplicated");
        if (points.length > 0 && points.every(point => UE.KismetSystemLibrary.IsValid(point)) &&
            points.some(point => point.TeamId !== points[0].TeamId)) errors.push("SpawnPoint teams differ");
        if (squadValid) {
            const definition = this.Squad.GetSquadContext().Definition;
            if (!UE.KismetSystemLibrary.IsValid(definition)) errors.push("Squad Definition missing");
            else if (points.length > definition.MemberLimit) errors.push("Roster exceeds MemberLimit");
        }
        configs.forEach((config, index) => {
            if (!UE.KismetSystemLibrary.IsValid(config)) errors.push("Role config missing: " + index);
        });
        if (!Number.isFinite(this.SetupTimeoutSeconds) || this.SetupTimeoutSeconds <= 0)
            errors.push("SetupTimeoutSeconds must be positive: " + this.SetupTimeoutSeconds);
        if (!Number.isInteger(this.SupportMemberCount) || this.SupportMemberCount < 0)
            errors.push("SupportMemberCount must be a nonnegative integer: " + this.SupportMemberCount);
        if (errors.length > 0) {
            this.LastSetupMessage = errors.join("; ");
            console.error("[SquadSetup] " + this.GetName() + ": " + this.LastSetupMessage);
            return;
        }
        const state: BootstrapState = {
            startedAt: UE.GameplayStatics.GetTimeSeconds(this), bindings: new Map(),
            commanderStarted: false, ending: false,
        };
        states.set(this, state);
        state.timer = UE.KismetSystemLibrary.K2_SetTimerDelegate(
            toDelegate(this as TS_SquadBootstrap, "PollMembers"), 0.1, true
        );
        pollMembers(this, state);
    }

    @UE.ufunction.ufunction(UE.ufunction.BlueprintCallable)
    PollMembers(): void {
        const state = states.get(this);
        if (state && !state.ending) pollMembers(this, state);
    }

    @UE.ufunction.ufunction(UE.ufunction.BlueprintCallable)
    OnMemberInitialized(component: UE.PawnInitializationComponent): void {
        const state = states.get(this);
        if (!state || state.ending || state.commanderStarted || !component.IsPawnInitialized()) return;
        for (const binding of state.bindings.values()) {
            if (binding.initialization !== component || binding.joined) continue;
            const member = binding.character.GetComponentByClass(
                UE.DemoSquadMemberComponent.StaticClass()
            ) as UE.DemoSquadMemberComponent;
            const receiver = binding.character.GetComponentByClass(UE.DemoSquadOrderReceiverComponent.StaticClass());
            if (!UE.KismetSystemLibrary.IsValid(member) || !UE.KismetSystemLibrary.IsValid(receiver)) {
                fail(this, state, "Squad components are missing from " + binding.character.GetName() + ".");
                return;
            }
            const index = Array.from(state.bindings.values()).indexOf(binding);
            member.Role = index === 0 ? UE.EDemoSquadRole.Leader : index === 1 ? UE.EDemoSquadRole.Deputy :
                index < 2 + this.SupportMemberCount ? UE.EDemoSquadRole.Support : UE.EDemoSquadRole.Rifleman;
            member.SuccessionPriority = index === 0 ? 0 : index === 1 ? 10 : 100 + index;
            member.bRequired = index < 2;
            if (!member.JoinSquad(this.Squad, new UE.Guid())) {
                fail(this, state, "Member registration failed for " + binding.character.GetName() + ".");
                return;
            }
            binding.joined = true;
        }
        tryStart(this, state);
    }

    ReceiveEndPlay(_reason: UE.EEndPlayReason): void {
        this.StopSquad();
    }

    @UE.ufunction.ufunction(UE.ufunction.BlueprintCallable)
    StopSquad(): void {
        const state = states.get(this);
        if (!state) {
            return;
        }
        state.ending = true;
        stopWaiting(this, state);
        if (state.commanderStarted && UE.KismetSystemLibrary.IsValid(this.Squad)) {
            this.Squad.StopCommander();
        }
        state.bindings.forEach(binding => {
            if (UE.KismetSystemLibrary.IsValid(binding.character)) {
                const member = binding.character.GetComponentByClass(
                    UE.DemoSquadMemberComponent.StaticClass()
                ) as UE.DemoSquadMemberComponent;
                if (UE.KismetSystemLibrary.IsValid(member) && member.GetSquad() === this.Squad) {
                    member.LeaveSquad();
                }
            }
        });
        states.delete(this);
    }

    @UE.ufunction.ufunction(UE.ufunction.BlueprintCallable)
    IssueMoveMission(goal: UE.Vector, radius: number): boolean {
        return issueMission(this.Squad, UE.EDemoSquadMissionType.Move, goal, radius,
            new UE.Vector(1, 0, 0), UE.EDemoSquadEngagement.FireAtWill);
    }

    @UE.ufunction.ufunction(UE.ufunction.BlueprintCallable)
    IssueDefendMission(goal: UE.Vector, radius: number, facing: UE.Vector): boolean {
        return issueMission(this.Squad, UE.EDemoSquadMissionType.Defend, goal, radius,
            facing, UE.EDemoSquadEngagement.FireAtWill);
    }

    @UE.ufunction.ufunction(UE.ufunction.BlueprintCallable)
    IssueControlMission(goal: UE.Vector, radius: number, facing: UE.Vector): boolean {
        return issueMission(this.Squad, UE.EDemoSquadMissionType.Control, goal, radius,
            facing, UE.EDemoSquadEngagement.FireAtWill);
    }

    @UE.ufunction.ufunction(UE.ufunction.BlueprintCallable)
    IssueWithdrawMission(goal: UE.Vector, radius: number): boolean {
        return issueMission(this.Squad, UE.EDemoSquadMissionType.Withdraw, goal, radius,
            new UE.Vector(1, 0, 0), UE.EDemoSquadEngagement.SelfDefense);
    }

    @UE.ufunction.ufunction(UE.ufunction.BlueprintCallable)
    IssueRegroupMission(goal: UE.Vector, radius: number): boolean {
        return issueMission(this.Squad, UE.EDemoSquadMissionType.Regroup, goal, radius,
            new UE.Vector(1, 0, 0), UE.EDemoSquadEngagement.SelfDefense);
    }

    @UE.ufunction.ufunction(UE.ufunction.BlueprintCallable)
    CancelMission(): boolean {
        return issueMission(this.Squad, UE.EDemoSquadMissionType.None, new UE.Vector(), 500,
            new UE.Vector(1, 0, 0), UE.EDemoSquadEngagement.SelfDefense);
    }

    @UE.ufunction.ufunction(UE.ufunction.BlueprintCallable)
    SubmitMission(mission: UE.DemoSquadMission): boolean {
        return UE.KismetSystemLibrary.IsValid(this.Squad) && this.Squad.HasAuthority() &&
            this.Squad.GetSquadContext().SetMission(mission);
    }

    @UE.ufunction.ufunction(UE.ufunction.BlueprintCallable)
    ConfirmObjectiveCompleted(missionId: UE.Guid, revision: number): boolean {
        return UE.KismetSystemLibrary.IsValid(this.Squad) && this.Squad.HasAuthority() &&
            this.Squad.GetSquadContext().ConfirmMissionCompleted(missionId, revision);
    }

    @UE.ufunction.ufunction(UE.ufunction.BlueprintCallable)
    SetCommandMode(mode: UE.EDemoSquadCommandMode): void {
        if (UE.KismetSystemLibrary.IsValid(this.Squad) && this.Squad.HasAuthority()) {
            this.Squad.GetSquadContext().SetCommandMode(mode);
        }
    }

    @UE.ufunction.ufunction(UE.ufunction.BlueprintCallable)
    SetManualTactic(tactic: UE.EDemoSquadTactic): boolean {
        return UE.KismetSystemLibrary.IsValid(this.Squad) && this.Squad.HasAuthority() &&
            this.Squad.GetSquadContext().SetManualTactic(tactic);
    }

    @UE.ufunction.ufunction(UE.ufunction.BlueprintCallable)
    TakeControl(controller: UE.DemoPlayerController, character: UE.DemoCharacter): boolean {
        return takeControl(controller, character);
    }

    @UE.ufunction.ufunction(UE.ufunction.BlueprintCallable)
    ReturnControl(controller: UE.DemoPlayerController): boolean {
        return returnControl(controller);
    }
}

export default TS_SquadBootstrap;
