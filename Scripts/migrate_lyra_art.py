#!/usr/bin/env python3
# Copyright (c) 2026 Nelaric Contributors
"""Run Lyra art migration in an isolated Unreal project snapshot."""

from __future__ import annotations

import argparse
from contextlib import contextmanager
from datetime import datetime
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_DESTINATION = "/Game/ThirdParty/LyraStarterGame/LyraStarterGame"
EXCLUDED = {"Intermediate", "Saved", "DerivedDataCache", ".git", ".vs", ".idea"}


def filesystem_path(path: Path) -> str:
    resolved = str(path.resolve())
    if sys.platform == "win32" and not resolved.startswith("\\\\?\\"):
        return "\\\\?\\UNC\\" + resolved[2:] if resolved.startswith("\\\\") else "\\\\?\\" + resolved
    return resolved


@contextmanager
def temporary_project():
    directory = Path(tempfile.mkdtemp(prefix="nelaric-lyra-art-")).resolve()
    try:
        yield directory
    finally:
        if directory.parent != Path(tempfile.gettempdir()).resolve() or not directory.name.startswith("nelaric-lyra-art-"):
            raise RuntimeError("Refusing to clean an unexpected temporary project path.")
        for attempt in range(3):
            try:
                shutil.rmtree(filesystem_path(directory))
                break
            except OSError as error:
                if attempt == 2:
                    print(f"Temporary snapshot cleanup failed: {directory}: {error}", file=sys.stderr)
                else:
                    time.sleep(1)


def read_json(path: Path) -> dict:
    text = path.read_text(encoding="utf-8-sig")
    # Unreal descriptors permit comments and trailing commas; keep quoted strings intact.
    text = re.sub(
        r'"(?:\\.|[^"\\])*"|/\*.*?\*/|//[^\r\n]*',
        lambda match: match[0] if match[0].startswith('"') else "", text, flags=re.DOTALL,
    )
    text = re.sub(
        r'"(?:\\.|[^"\\])*"|,\s*(?=[}\]])',
        lambda match: match[0] if match[0].startswith('"') else "", text,
    )
    try:
        return json.loads(text)
    except ValueError as error:
        raise ValueError(f"Invalid JSON descriptor {path}: {error}") from error


def write_json(path: Path, value: dict) -> None:
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def engine_descriptors(root: Path):
    excluded = EXCLUDED | {"Binaries", "Content", "Source", "Resources", "Config", "Platforms", "Tests"}
    for directory, children, files in os.walk(root):
        children[:] = [name for name in children if name not in excluded]
        for name in files:
            if name.endswith(".uplugin"):
                yield Path(directory) / name


def discover_project(source: Path) -> Path:
    if source.is_file() and source.suffix == ".uproject":
        return source
    projects = []
    for directory, children, files in os.walk(source):
        children[:] = [name for name in children if name not in EXCLUDED and name != "Binaries"]
        projects.extend(Path(directory) / name for name in files if name == "LyraStarterGame.uproject")
    if len(projects) != 1:
        raise RuntimeError(f"Expected one LyraStarterGame.uproject under {source}; found {len(projects)}.")
    return projects[0].resolve()


def find_editor(explicit: Path | None, version: str) -> Path:
    if explicit:
        candidates = [explicit.resolve()]
    else:
        installations = []
        if sys.platform == "win32":
            import winreg

            for hive in (winreg.HKEY_LOCAL_MACHINE, winreg.HKEY_CURRENT_USER):
                try:
                    with winreg.OpenKey(hive, rf"SOFTWARE\EpicGames\Unreal Engine\{version}") as key:
                        installations.append(Path(winreg.QueryValueEx(key, "InstalledDirectory")[0]))
                except OSError:
                    pass
            launcher = Path(os.environ.get("PROGRAMDATA", "C:/ProgramData")) / "Epic/UnrealEngineLauncher/LauncherInstalled.dat"
            if launcher.is_file():
                for item in read_json(launcher).get("InstallationList", []):
                    if item.get("AppName") == f"UE_{version}":
                        installations.append(Path(item["InstallLocation"]))
        candidates = [path / "Engine/Binaries/Win64/UnrealEditor-Cmd.exe" for path in installations]
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    raise RuntimeError("Cannot locate UnrealEditor-Cmd.exe. Supply --editor with the matching engine executable.")


def snapshot_project(source: Path, destination: Path) -> tuple[Path, dict[str, str], list[str]]:
    def ignore(directory: str, names: list[str]) -> list[str]:
        return [name for name in names if name in EXCLUDED or (Path(directory) / name).is_symlink()]

    shutil.copytree(source.parent, destination, ignore=ignore)
    project = destination / source.name
    descriptor = read_json(project)
    plugins = {entry["Name"]: entry for entry in descriptor.get("Plugins", [])}
    mounts = {"/Game": str(destination / "Content")}
    native_modules = [entry["Name"] for entry in descriptor.get("Modules", [])]
    for plugin_file in sorted((destination / "Plugins").rglob("*.uplugin")):
        plugin = read_json(plugin_file)
        native_modules.extend(entry["Name"] for entry in plugin.get("Modules", []))
        if not plugin.get("CanContainContent"):
            continue
        name = plugin_file.stem
        content = plugin_file.parent / "Content"
        if not content.is_dir():
            continue
        # Game Feature plugins normally load explicitly; mount them for this snapshot only.
        plugin["ExplicitlyLoaded"] = False
        write_json(plugin_file, plugin)
        plugins[name] = {"Name": name, "Enabled": True}
        mounts[f"/{name}"] = str(content)
    for name in ("PythonScriptPlugin", "EditorScriptingUtilities"):
        plugins[name] = {"Name": name, "Enabled": True}
    descriptor["Plugins"] = list(plugins.values())
    write_json(project, descriptor)
    return project, mounts, native_modules


def publish(content: Path, target: Path, manifest: list[dict]) -> None:
    """Publish only verified packages; never merge into an existing destination."""
    files = []
    for entry in manifest:
        relative = Path(entry["destination"].removeprefix("/Game/"))
        main = content / relative.with_suffix(".uasset")
        if not Path(filesystem_path(main)).is_file():
            raise RuntimeError(f"Missing saved asset: {entry['destination']}")
        files.append(main)
        for suffix in (".uexp", ".ubulk", ".uptnl"):
            sidecar = main.with_suffix(suffix)
            if Path(filesystem_path(sidecar)).is_file():
                files.append(sidecar)
    package_root = manifest[0]["destination_root"].removeprefix("/Game/")
    asset_root = content / package_root
    target.parent.mkdir(parents=True, exist_ok=True)
    # Copy beside the final directory, then publish by rename on the same volume.
    temporary = Path(tempfile.mkdtemp(prefix=".lyra-art-publish-", dir=target.parent)).resolve()
    try:
        staging = temporary / "assets"
        staging.mkdir()
        for file in files:
            relative = file.relative_to(asset_root)
            output = staging / relative
            Path(filesystem_path(output.parent)).mkdir(parents=True, exist_ok=True)
            shutil.copy2(filesystem_path(file), filesystem_path(output))
        if target.exists():
            raise RuntimeError(f"Destination appeared during migration; refusing to overwrite: {target}")
        os.rename(filesystem_path(staging), filesystem_path(target))
    finally:
        if temporary.parent != target.parent.resolve() or not temporary.name.startswith(".lyra-art-publish-"):
            raise RuntimeError("Refusing to clean an unexpected publication staging path.")
        try:
            shutil.rmtree(filesystem_path(temporary))
        except OSError as error:
            print(f"Publication staging cleanup failed: {temporary}: {error}", file=sys.stderr)


def isolate_art_project(project: Path, mounts: dict[str, str], manifest: list[dict], required_plugins: list[str]) -> None:
    """Keep only planned art packages and remove source gameplay modules in the snapshot."""
    snapshot = project.parent.resolve()
    keep = {entry["source"].casefold() for entry in manifest}
    for mount, directory in mounts.items():
        content = Path(directory).resolve()
        if not content.is_relative_to(snapshot):
            raise RuntimeError("Refusing to prune content outside the disposable snapshot.")
        for file in content.rglob("*"):
            if not file.is_file() or file.suffix.lower() not in {".uasset", ".umap", ".uexp", ".ubulk", ".uptnl"}:
                continue
            package = mount + "/" + file.relative_to(content).with_suffix("").as_posix()
            if package.casefold() not in keep:
                file.unlink()
    descriptor = read_json(project)
    descriptor["Modules"] = []
    enabled = set(required_plugins) | {mount.lstrip("/") for mount in mounts if mount != "/Game"}
    enabled.update({"PythonScriptPlugin", "EditorScriptingUtilities"})
    plugins = {entry["Name"]: entry for entry in descriptor.get("Plugins", [])}
    for name, entry in plugins.items():
        entry["Enabled"] = name in enabled
    for name in enabled:
        plugins.setdefault(name, {"Name": name})["Enabled"] = True
    for name in {"GameFeatures", "Water"} - enabled:
        plugins[name] = {"Name": name, "Enabled": False}
    descriptor["Plugins"] = list(plugins.values())
    write_json(project, descriptor)
    for plugin_file in (snapshot / "Plugins").rglob("*.uplugin"):
        plugin = read_json(plugin_file)
        plugin["Modules"] = []
        plugin["Plugins"] = [entry for entry in plugin.get("Plugins", []) if entry["Name"] in enabled]
        write_json(plugin_file, plugin)
    # Source gameplay settings can instantiate source-native CDOs. Use engine defaults for relocation.
    config_directory = snapshot / "Config"
    for file in config_directory.rglob("*.ini"):
        file.unlink()
    config_directory.mkdir(exist_ok=True)
    (config_directory / "DefaultEngine.ini").write_text(
        "[/Script/Engine.Engine]\nAssetManagerClassName=/Script/Engine.AssetManager\n", encoding="utf-8",
    )


def enable_engine_plugins(project: Path, names: list[str]) -> None:
    descriptor = read_json(project)
    plugins = {entry["Name"]: entry for entry in descriptor.get("Plugins", [])}
    changed = False
    for name in names:
        if name not in plugins or not plugins[name].get("Enabled"):
            plugins.setdefault(name, {"Name": name})["Enabled"] = True
            changed = True
    if changed:
        descriptor["Plugins"] = list(plugins.values())
        project.write_text(json.dumps(descriptor, indent="\t", ensure_ascii=False) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=ROOT / "ThirdParty", help="Source directory or Lyra .uproject.")
    parser.add_argument("--target-project", type=Path, default=ROOT / "NelaricGameplay/NelaricGameplay.uproject")
    parser.add_argument("--destination", default=DEFAULT_DESTINATION, help="Destination package root under /Game/ThirdParty/.")
    parser.add_argument("--editor", type=Path, help="Matching UnrealEditor-Cmd executable.")
    parser.add_argument("--apply", action="store_true", help="Relocate, verify, and copy assets; default only reports a plan.")
    parser.add_argument("--validate", action="store_true", help="Relocate and verify inside the snapshot without copying to the target.")
    parser.add_argument("--extract-art-only", "--clean-animation-metadata", dest="clean_animation_metadata", action="store_true", help="Remove project animation hooks and convert tagged physics materials to engine materials in the snapshot.")
    parser.add_argument("--enable-required-plugins", action="store_true", help="Enable the art assets' engine plugins in the target .uproject when applying.")
    parser.add_argument("--report-dir", type=Path, default=ROOT / ".tools/lyra-art-migration")
    args = parser.parse_args()
    if args.apply and args.validate:
        parser.error("Use either --apply or --validate, not both.")
    source = discover_project(args.source.resolve())
    target_project = args.target_project.resolve()
    target_descriptor = read_json(target_project)
    source_descriptor = read_json(source)
    version = source_descriptor.get("EngineAssociation", "")
    if version != target_descriptor.get("EngineAssociation"):
        raise RuntimeError("Source and target EngineAssociation values must match.")
    if not re.fullmatch(r"/Game/ThirdParty/[A-Za-z0-9_]+(?:/[A-Za-z0-9_]+)*", args.destination):
        raise RuntimeError("Destination must be a package path under /Game/ThirdParty/ with valid directory names.")
    target = target_project.parent / "Content" / args.destination.removeprefix("/Game/")
    if source.parent == target_project.parent or target.is_relative_to(source.parent) or source.parent.is_relative_to(target):
        raise RuntimeError("Source and destination must not contain each other.")
    if args.apply and target.exists():
        raise RuntimeError(f"Destination already exists; choose a new --destination to avoid overwriting: {target}")
    editor = find_editor(args.editor, version)
    engine_mounts = {}
    engine_modules = {}
    for plugin_file in engine_descriptors(editor.parents[2] / "Plugins"):
        plugin = read_json(plugin_file)
        if plugin.get("CanContainContent"):
            engine_mounts[f"/{plugin_file.stem}"] = plugin_file.stem
        for module in plugin.get("Modules", []):
            engine_modules[module["Name"]] = plugin_file.stem
    report_dir = args.report_dir.resolve() / (datetime.now().strftime("%Y%m%d-%H%M%S") + f"-{os.getpid()}")
    report_dir.mkdir(parents=True, exist_ok=False)
    mode = "apply" if args.apply else "validate" if args.validate else "preview"
    print(f"Mode: {mode}\nSource: {source}\nDestination: {target}\nReports: {report_dir}", flush=True)
    with temporary_project() as temporary_root:
        print("Copying the source project into a temporary snapshot (binaries retained, caches excluded)...", flush=True)
        project, mounts, native_modules = snapshot_project(source, temporary_root / source.parent.name)
        report = report_dir / "report.json"
        config = temporary_root / "migration.json"
        write_json(config, {
            "project": str(project), "mounts": mounts, "native_modules": native_modules,
            "destination": args.destination, "apply": args.apply or args.validate,
            "mode": mode, "report": str(report),
            "clean_animation_metadata": args.clean_animation_metadata,
            "phase": "prepare",
            "engine_mounts": engine_mounts, "engine_modules": engine_modules,
        })
        environment = os.environ.copy()
        environment["NELARIC_LYRA_ART_CONFIG"] = str(config)
        worker = Path(__file__).with_name("migrate_lyra_art_unreal.py")
        log = report_dir / "UnrealEditor.log"
        print("Scanning assets and dependencies in Unreal; progress is recorded in UnrealEditor.log...", flush=True)
        def run_unreal(phase: str) -> dict:
            phase_log = log if phase == "prepare" else report_dir / f"{phase}-UnrealEditor.log"
            report.unlink(missing_ok=True)
            with (report_dir / f"{phase}-console.log").open("w", encoding="utf-8") as console:
                process = subprocess.run([
                    str(editor), str(project), "-run=pythonscript", f"-script={worker}",
                    "-unattended", "-nop4", "-nosplash", "-nullrhi", "-stdout", "-FullStdOutLogOutput",
                    f"-abslog={phase_log}",
                ], env=environment, stdout=console, stderr=subprocess.STDOUT, check=False)
            if not report.is_file():
                raise RuntimeError(f"Unreal did not produce a report (exit {process.returncode}). See {phase_log}.")
            result = read_json(report)
            if process.returncode or result.get("status") != "success":
                raise RuntimeError(f"Migration failed: {result.get('error', 'Unreal process failed')}. See {report} and {phase_log}.")
            return result

        cleanup = {}
        if args.clean_animation_metadata:
            configuration = read_json(config)
            configuration["phase"] = "clean"
            write_json(config, configuration)
            cleanup = run_unreal("clean")
            write_json(report_dir / "cleanup.json", cleanup)
            configuration["phase"] = "prepare"
            configuration["clean_animation_metadata"] = False
            write_json(config, configuration)
            print("Animation cleanup saved; rescanning dependencies in a fresh Unreal process...", flush=True)
        result = run_unreal("prepare")
        for field in ("animation_cleanup", "mesh_cleanup", "physical_material_cleanup"):
            if field in cleanup:
                result[field] = cleanup[field]
        write_json(report, result)
        assets = result["assets"]
        print(f"Eligible packages: {len(assets)}; skipped art assets: {len(result['skipped'])}.", flush=True)
        print("Engine plugins used: " + ", ".join(result["required_engine_plugins"]), flush=True)
        if not assets:
            raise RuntimeError("No eligible art assets were found; inspect the report.")
        if args.apply or args.validate:
            write_json(report_dir / "plan.json", result)
            print("Preparing an art-only snapshot and relocating packages in a fresh Unreal process...", flush=True)
            isolate_art_project(project, mounts, assets, result["required_engine_plugins"])
            configuration = read_json(config)
            configuration["phase"] = "relocate"
            configuration["clean_animation_metadata"] = False
            write_json(config, configuration)
            relocated = run_unreal("relocate")
            if {entry["source"] for entry in relocated["assets"]} != {entry["source"] for entry in assets}:
                raise RuntimeError("Art-only relocation changed the planned package selection; inspect the report.")
            configuration["phase"] = "verify"
            configuration["manifest"] = assets
            write_json(config, configuration)
            print("Checking the saved package dependency graph in a fresh Unreal process...", flush=True)
            verified = run_unreal("verify")
            result["references_verified"] = verified["references_verified"]
            write_json(report, result)
        if args.apply:
            print("Publishing relocated packages to the target content directory...", flush=True)
            publish(project.parent / "Content", target, assets)
            if args.enable_required_plugins:
                enable_engine_plugins(target_project, result["required_engine_plugins"])
                result["enabled_engine_plugins"] = result["required_engine_plugins"]
            result["published"] = True
            write_json(report, result)
            print("Migration complete. Reopen the target editor to refresh the Asset Registry.")
        elif args.validate:
            print("Validation complete; relocated package references verified. No target assets were written.")
        else:
            print("Preview complete; no target assets were written. Use --apply to migrate.")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, RuntimeError) as error:
        print(f"Error: {error}", file=sys.stderr)
        raise SystemExit(1)
