// Copyright (c) 2026 Nelaric Contributors

import * as UE from "ue";

/** Resolve an unspecified order heading once when selected by the GameAI tree. */
class TS_SoldierResolveHeading extends UE.GameAIStateTreeTaskBlueprintBase {
    ReceiveLatentEnterState(_transition: UE.StateTreeTransitionResult): void {
        if (!UE.KismetSystemLibrary.IsValid(this.Pawn)) {
            this.FinishTask(false);
            return;
        }
        const soldier = this.Pawn.GetComponentByClass(
            UE.DemoSoldierComponent.StaticClass()
        ) as UE.DemoSoldierComponent;
        if (!UE.KismetSystemLibrary.IsValid(soldier)) {
            this.FinishTask(false);
            return;
        }
        const order = soldier.GetOrder();
        const direction = order.FacingDirection;
        if (direction.X * direction.X + direction.Y * direction.Y > 0.000001) {
            this.FinishTask(true);
            return;
        }
        const yaw = UE.KismetMathLibrary.RandomFloatInRange(-180, 180);
        const heading = UE.KismetMathLibrary.GetForwardVector(
            UE.KismetMathLibrary.MakeRotator(0, 0, yaw)
        );
        const accepted = soldier.UpdateOrderFacing(order.Id, heading);
        if (accepted) {
            console.log("[RandomHeading] " + this.Pawn.GetName() + " yaw=" + yaw.toFixed(2));
        }
        this.FinishTask(accepted);
    }
}

export default TS_SoldierResolveHeading;
