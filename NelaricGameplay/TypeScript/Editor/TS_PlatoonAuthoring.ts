// Copyright (c) 2026 Nelaric Contributors

import * as UE from "ue";

/** Editor-only reflected property access used by the Python asset authoring script. */
class TS_PlatoonAuthoring extends UE.EditorUtilityObject {
    SetFactorySchema(factory: UE.StateTreeFactory, schema: UE.Class): void {
        factory.StateTreeSchemaClass = schema;
    }
    GetEditorData(tree: UE.StateTree): UE.StateTreeEditorData {
        return tree.EditorData_EditorOnly as UE.StateTreeEditorData;
    }
    SetRoot(editor: UE.StateTreeEditorData, root: UE.StateTreeState): void {
        editor.SubTrees.Empty();
        editor.SubTrees.Add(root);
    }
    AddChild(parent: UE.StateTreeState, child: UE.StateTreeState): void {
        child.Parent = parent;
        parent.Children.Add(child);
    }
    GetCompiledStateCount(tree: UE.StateTree): number { return tree.States.Num(); }
}

export default TS_PlatoonAuthoring;
