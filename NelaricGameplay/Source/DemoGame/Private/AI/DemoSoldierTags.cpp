// Copyright (c) 2026 Nelaric Contributors

#include "AI/DemoSoldierTags.h"

namespace Nelaric::Soldier
{
UE_DEFINE_GAMEPLAY_TAG(EnemySeen, "AI.Event.EnemySeen");
UE_DEFINE_GAMEPLAY_TAG(EnemyLost, "AI.Event.EnemyLost");
UE_DEFINE_GAMEPLAY_TAG(TargetChanged, "AI.Event.TargetChanged");
UE_DEFINE_GAMEPLAY_TAG(TargetDead, "AI.Event.TargetDead");
UE_DEFINE_GAMEPLAY_TAG(SoundHeard, "AI.Event.SoundHeard");
UE_DEFINE_GAMEPLAY_TAG(Damaged, "AI.Event.Damaged");
UE_DEFINE_GAMEPLAY_TAG(NearMiss, "AI.Event.NearMiss");
UE_DEFINE_GAMEPLAY_TAG(GrenadeDanger, "AI.Event.GrenadeDanger");
UE_DEFINE_GAMEPLAY_TAG(MoveCompleted, "AI.Event.MoveCompleted");
UE_DEFINE_GAMEPLAY_TAG(MoveFailed, "AI.Event.MoveFailed");
UE_DEFINE_GAMEPLAY_TAG(ReloadCompleted, "AI.Event.ReloadCompleted");
UE_DEFINE_GAMEPLAY_TAG(BurstCompleted, "AI.Event.BurstCompleted");
UE_DEFINE_GAMEPLAY_TAG(OrderChanged, "AI.Event.OrderChanged");
UE_DEFINE_GAMEPLAY_TAG(OrderCompleted, "AI.Event.OrderCompleted");
UE_DEFINE_GAMEPLAY_TAG(OrderFailed, "AI.Event.OrderFailed");
UE_DEFINE_GAMEPLAY_TAG(DecisionChanged, "AI.Event.DecisionChanged");
} // namespace Nelaric::Soldier
