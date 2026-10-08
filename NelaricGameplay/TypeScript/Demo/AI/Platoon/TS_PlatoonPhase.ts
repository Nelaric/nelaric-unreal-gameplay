// Copyright (c) 2026 Nelaric Contributors

import * as UE from "ue";

/** One atomic lifecycle operation per authored StateTree state. */
class TS_PlatoonPhase extends UE.GameAIStateTreeTaskBlueprintBase {
    @UE.uproperty.uproperty(UE.uproperty.EditAnywhere, UE.uproperty.BlueprintReadWrite)
    Operation!: string;

    ReceiveLatentEnterState(_transition: UE.StateTreeTransitionResult): void { this.advance(); }
    ReceiveLatentTick(_delta: number): void { this.advance(); }

    private advance(): void {
        const membership = this.OwnerActor?.GetComponentByClass(UE.DemoCompanyMembershipComponent.StaticClass()) as UE.DemoCompanyMembershipComponent;
        if (!membership || !UE.KismetSystemLibrary.IsValid(membership)) { this.FinishTask(false); return; }
        const result = membership.StepPlatoon(this.Operation);
        if (result !== UE.EStateTreeRunStatus.Running) this.FinishTask(result === UE.EStateTreeRunStatus.Succeeded);
    }
}

export default TS_PlatoonPhase;
