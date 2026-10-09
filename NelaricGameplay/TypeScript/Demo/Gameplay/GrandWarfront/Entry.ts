// Copyright (c) 2026 Nelaric Contributors

import * as UE from "ue";
import { argv } from "puerts";
import { GrandWarfrontCoordinator } from "./GrandWarfrontCoordinator";
const scripts = argv.getByName("Scripts") as UE.DemoScriptSubsystem;
if (!scripts) throw new Error("Battlefront requires the shared script subsystem");
scripts.StartBattlefrontHandler.Bind((mode) => {
    if (!mode || !UE.KismetSystemLibrary.IsValid(mode) || !mode.HasAuthority()) return;
    const match = new GrandWarfrontCoordinator(mode);
    mode.UpdateHandler.Bind(() => match.tick());
    mode.StopHandler.Bind(() => match.stop());
    console.log("[Warfront] World lifecycle bound.");
});
