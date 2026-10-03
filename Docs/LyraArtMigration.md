<!-- Copyright (c) 2026 Nelaric Contributors -->

English | [简体中文](LyraArtMigration.zh-CN.md)

# Lyra art migration

Run `Scripts/migrate_lyra_art.py` from the repository root with Python 3.10 or later on Windows. It discovers `LyraStarterGame.uproject` under `ThirdParty/` and locates the matching installed Unreal Editor. `ThirdParty/` is an ignored local directory for downloaded source projects. Both source and target projects must use the same engine association; the source's editor modules must already be compiled for that engine.

Preview the selection and dependencies:

```powershell
python Scripts/migrate_lyra_art.py --extract-art-only
```

Close the source and target editors before actual migration, then run:

```powershell
python Scripts/migrate_lyra_art.py --extract-art-only --enable-required-plugins --apply
```

The default destination is `/Game/ThirdParty/LyraStarterGame/LyraStarterGame`, corresponding to `NelaricGameplay/Content/ThirdParty/LyraStarterGame/LyraStarterGame/`. Source project assets retain their relative directories. Assets from project content plugins are placed under `PluginContent/<PluginName>/` inside this destination.

The script selects meshes, materials, textures, animation sequences, montages, blend spaces, skeletons, physics assets, and sound assets, including eligible MetaSounds. It includes supporting physical materials, curves, curve atlases, animation compression settings, and audio modulation assets when referenced. Gameplay Blueprints, animation Blueprints, maps, UI Blueprints, and assets with Lyra-specific native dependencies are excluded. An art asset is also excluded if its hard or soft dependency graph contains an excluded or unavailable package. Review `skipped` in the report for individual reasons.

`--extract-art-only` (alias `--clean-animation-metadata`) removes editor animation modifiers and project-specific animation notify objects from the snapshot, and clears skeletal meshes' post-process animation Blueprint assignments. Bone animation, curves, root-motion settings, and built-in notifies are retained. Removed notifies no longer trigger their original gameplay, sound, or effect behavior; mesh corrections performed by the removed post-process Blueprints also need to be recreated in the target project. Changes are recorded in `animation_cleanup` and `mesh_cleanup`. Lyra's tagged physical materials used by physics bodies are replaced with engine physical materials preserving friction, restitution, density, combine modes, mass scaling, and surface type; replacements are recorded in `physical_material_cleanup`. Omit this option to preserve assets exactly and exclude those with incompatible dependencies.

Every run uses a disposable full project snapshot. Binaries are retained, while generated caches and intermediate output are omitted. Python and editor scripting plugins are enabled only in this snapshot; content-bearing project plugins are mounted there for scanning. After asset preparation, the snapshot is reduced to the selected art packages and source gameplay modules are disabled. A fresh Unreal process relocates the assets, updates soft references, saves the packages, and checks the resulting dependency graph before any files are copied to the target. This avoids source gameplay CDO references interfering with relocation. The original Lyra project is unchanged. Allow disk space for an additional project copy and Unreal's generated caches.

Reports and engine logs are saved under `.tools/lyra-art-migration/<run>/`. Preview writes no target assets. Apply refuses an existing destination; it does not overwrite or merge existing assets. A fresh Unreal process verifies the saved package dependencies. Engine-owned plugin content retains its original mount path; the plugins used by selected resources are listed in `required_engine_plugins`. The script copies packages into a temporary sibling directory and publishes the destination only after copying completes. Reopen the target editor after migration and verify representative models, materials, animations, and sounds.

`--enable-required-plugins` enables the listed engine plugins in the target `.uproject` during apply, preserving other plugin settings. It does not copy or enable Lyra gameplay modules. Without this option, enable the required plugins manually. Preview and validation never modify the target project.

Optional arguments:

- `--source <directory-or-uproject>` selects another source.
- `--target-project <uproject>` selects another target.
- `--destination /Game/ThirdParty/<Name>/<Folder>` selects a new content path.
- `--editor <UnrealEditor-Cmd.exe>` selects a matching custom engine installation.
- `--report-dir <directory>` selects the report directory.
- `--validate` performs relocation and reference verification in the snapshot without publishing target assets; combine it with `--extract-art-only` when extracting independent art. It cannot be combined with `--apply`.

The launcher calls `Scripts/migrate_lyra_art_unreal.py` inside Unreal; do not run that worker directly. Migrated assets are excluded from version control along with the entire `NelaricGameplay/Content/` directory.

API references: [Unreal Python scripting](https://dev.epicgames.com/documentation/en-us/unreal-engine/scripting-the-unreal-editor-using-python?application_version=5.6), [AssetTools](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/AssetTools?application_version=5.6), and [Asset Registry](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/AssetRegistry?application_version=5.6).
