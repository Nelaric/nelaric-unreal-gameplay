// Copyright (c) 2026 Nelaric Contributors

import * as UE from "ue";
import { platoons } from "./PlatoonCoordinator";

/** One atomic lifecycle operation per authored StateTree state. */
class TS_PlatoonPhase extends UE.GameAIStateTreeTaskBlueprintBase {
    @UE.uproperty.uproperty(UE.uproperty.EditAnywhere, UE.uproperty.BlueprintReadWrite)
    Operation!: string;

    ReceiveLatentEnterState(_transition: UE.StateTreeTransitionResult): void { this.advance(); }
    ReceiveLatentTick(_delta: number): void { this.advance(); }

    private advance(): void {
        const coordinator = platoons.get(this.OwnerActor);
        if (!coordinator) { this.FinishTask(false); return; }
        const result = coordinator.step(this.Operation);
        if (result !== undefined) this.FinishTask(result);
    }
}

export default TS_PlatoonPhase;
