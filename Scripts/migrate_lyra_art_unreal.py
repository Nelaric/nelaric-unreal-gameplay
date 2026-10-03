# Copyright (c) 2026 Nelaric Contributors
"""Unreal worker for migrate_lyra_art.py; runs only in the disposable snapshot."""

from __future__ import annotations

from collections import Counter
import json
import os
from pathlib import Path, PurePosixPath
import re
import traceback

import unreal

ART_CLASSES = {
    "StaticMesh", "SkeletalMesh", "Skeleton", "PhysicsAsset", "MorphTarget",
    "GeometryCollection",
    "Material", "MaterialInstanceConstant", "MaterialFunction", "MaterialFunctionInstance",
    "MaterialFunctionMaterialLayer", "MaterialFunctionMaterialLayerBlend",
    "MaterialFunctionMaterialLayerInstance", "MaterialFunctionMaterialLayerBlendInstance",
    "MaterialParameterCollection", "SubsurfaceProfile", "Texture2D", "TextureCube",
    "Texture2DArray", "TextureCubeArray", "VolumeTexture", "TextureRenderTarget2D",
    "TextureRenderTargetCube", "TextureRenderTarget2DArray", "RuntimeVirtualTexture",
    "SparseVolumeTexture", "AnimSequence", "AnimComposite", "AnimMontage",
    "BlendSpace", "BlendSpace1D", "AimOffsetBlendSpace", "AimOffsetBlendSpace1D", "PoseAsset",
    "SoundWave", "SoundCue", "SoundClass", "SoundMix", "SoundConcurrency", "SoundAttenuation",
    "ReverbEffect", "MetaSoundSource", "MetaSoundPatch", "MetaSound",
}
SUPPORT_CLASSES = {
    "PhysicalMaterial", "PhysicalMaterialMask", "SkeletalMeshLODSettings", "CurveFloat", "CurveVector", "CurveLinearColor",
    "CurveLinearColorAtlas", "CurveTable", "AnimBoneCompressionSettings", "AnimCurveCompressionSettings",
    "SoundSubmix", "SoundModulationPatch", "SoundControlBus", "SoundControlBusMix",
    "SoundModulationParameter", "SoundModulationParameterVolume", "SoundModulationParameterFrequency",
    "SoundModulationParameterFilterFrequency", "SoundModulationParameterLPFFrequency",
    "SoundModulationParameterHPFFrequency", "SoundModulationParameterBipolar", "SoundModulationParameterUnipolar",
    "SoundEffectSubmixPreset", "SubmixEffectReverbPreset", "SubmixEffectEQPreset",
    "SubmixEffectDynamicsProcessorPreset", "SubmixEffectFilterPreset", "AudioBus",
    "EndpointSubmix", "AudioImpulseResponse", "SubmixEffectConvolutionReverbPreset",
    "SubmixEffectMultibandCompressorPreset", "SubmixEffectTapDelayPreset", "ITDSpatializationSourceSettings",
}
DEPENDENCIES = unreal.AssetRegistryDependencyOptions(
    include_soft_package_references=True, include_hard_package_references=True,
    include_searchable_names=False, include_soft_management_references=False,
    include_hard_management_references=False,
)


def write_report(path: Path, report: dict) -> None:
    path.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def asset_class(data) -> str:
    return str(data.asset_class_path.asset_name)


def supported(data) -> bool:
    return asset_class(data) in ART_CLASSES | SUPPORT_CLASSES or (
        str(data.asset_class_path.package_name) == "/Script/AudioModulation"
        and asset_class(data).startswith("SoundModulation")
    )


def destination(package: str, root: str) -> str:
    mount, relative = package.lstrip("/").split("/", 1)
    return f"{root}/{relative}" if mount == "Game" else f"{root}/PluginContent/{mount}/{relative}"


def clean_animations(assets: dict, registry, mounts: dict, report: dict) -> None:
    """Remove gameplay hooks and replace custom physical materials in the snapshot."""
    changed = []
    report["animation_cleanup"] = []
    report["mesh_cleanup"] = []
    report["physical_material_cleanup"] = []
    plain_materials = {}
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    for package, data in assets.items():
        if asset_class(data) != "PhysicalMaterialWithTags":
            continue
        original = data.get_asset()
        new = tools.create_asset(
            str(data.asset_name), "/Game/ArtMigrationSupport/PhysicalMaterials",
            unreal.PhysicalMaterial, unreal.PhysicalMaterialFactoryNew(),
        )
        if not new:
            raise RuntimeError(f"Could not create a plain physical material for {package}")
        for field in (
            "friction", "static_friction", "restitution", "density", "raise_mass_to_power", "surface_type",
            "friction_combine_mode", "override_friction_combine_mode",
            "restitution_combine_mode", "override_restitution_combine_mode",
        ):
            new.set_editor_property(field, original.get_editor_property(field))
        plain_materials[original.get_path_name()] = new
        changed.append(new)
        report["physical_material_cleanup"].append({"source": package, "replacement": new.get_path_name()})
    for package, data in assets.items():
        if asset_class(data) != "PhysicsAsset":
            continue
        physics = data.get_asset()
        replaced = False
        # The PhysicsAsset body array is not exposed to Python; its loaded body subobjects are.
        for body in unreal.ObjectIterator(unreal.BodySetup):
            if body.get_outermost() != physics.get_outermost():
                continue
            material = body.get_editor_property("phys_material")
            if material and material.get_path_name() in plain_materials:
                body.set_editor_property("phys_material", plain_materials[material.get_path_name()])
                replaced = True
            instance = body.get_editor_property("default_instance")
            material = instance.get_editor_property("phys_material_override")
            if material and material.get_path_name() in plain_materials:
                instance.set_editor_property("phys_material_override", plain_materials[material.get_path_name()])
                body.set_editor_property("default_instance", instance)
                replaced = True
        if replaced:
            changed.append(physics)
    for package, data in assets.items():
        if asset_class(data) == "SkeletalMesh":
            mesh = data.get_asset()
            post_process = mesh.get_editor_property("post_process_anim_blueprint")
            if post_process:
                mesh.set_editor_property("post_process_anim_blueprint", None)
                changed.append(mesh)
                report["mesh_cleanup"].append({"package": package, "removed_post_process": post_process.get_path_name()})
            continue
        if asset_class(data) not in {"AnimSequence", "AnimComposite", "AnimMontage"}:
            continue
        asset = data.get_asset()
        if not asset:
            raise RuntimeError(f"Could not load animation for cleanup: {package}")
        removed = []
        user_data = list(asset.get_editor_property("asset_user_data"))
        retained = []
        for item in user_data:
            if item and item.get_class().get_name() == "AnimationModifiersAssetUserData":
                removed.append("Editor animation modifiers")
            else:
                retained.append(item)
        if len(retained) != len(user_data):
            asset.set_editor_property("asset_user_data", retained)
        notifies = unreal.AnimationLibrary.get_animation_notify_events(asset)
        custom_names = set()
        builtin_names = set()
        for event in notifies:
            objects = [event.get_editor_property("notify"), event.get_editor_property("notify_state_class")]
            custom = [item for item in objects if item and (
                item.get_class().get_path_name().startswith(tuple(mount + "/" for mount in mounts if mount != "/Game") + ("/Game/",))
                or item.get_class().get_path_name().startswith(tuple(f"/Script/{name}." for name in report["native_modules"]))
            )]
            if custom:
                removed.extend(item.get_class().get_path_name() for item in custom)
                custom_names.add(str(event.get_editor_property("notify_name")))
            else:
                builtin_names.add(str(event.get_editor_property("notify_name")))
        if custom_names & builtin_names:
            raise RuntimeError(f"Custom and built-in notifies share a name in {package}; cannot clean selectively.")
        for name in custom_names:
            unreal.AnimationLibrary.remove_animation_notify_events_by_name(asset, name)
        if removed:
            changed.append(asset)
            report["animation_cleanup"].append({"package": package, "removed": removed})
    if changed and not unreal.EditorAssetLibrary.save_loaded_assets(changed, only_if_is_dirty=False):
        raise RuntimeError("Could not save cleaned snapshot animations.")
    if changed:
        registry.scan_paths_synchronous(list(mounts), force_rescan=True)


def run(config: dict, report: dict) -> None:
    project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.get_project_file_path())).resolve()
    if project != Path(config["project"]).resolve():
        raise RuntimeError("Refusing to run outside the temporary project snapshot.")
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    mounts = config["mounts"]
    registry.search_all_assets(synchronous_search=True)
    registry.scan_paths_synchronous(list(mounts), force_rescan=True)
    assets = {}
    multiple_exports = set()
    for mount, directory in mounts.items():
        found = registry.get_assets_by_path(mount, recursive=True, include_only_on_disk_assets=True)
        if any(Path(directory).rglob("*.uasset")) and not found:
            raise RuntimeError(f"Source content mount was not registered: {mount}")
        for data in found or []:
            package = str(data.package_name)
            if package in assets:
                multiple_exports.add(package)
                continue
            assets[package] = data
    if config.get("phase") == "verify":
        expected = {entry["destination"] for entry in config["manifest"]}
        for entry in config["manifest"]:
            package = entry["destination"]
            if package not in assets or asset_class(assets[package]) != entry["class"]:
                raise RuntimeError(f"Missing or invalid relocated asset: {package}")
            for dependency in registry.get_dependencies(package, DEPENDENCIES) or []:
                name = str(dependency)
                root_mount = "/" + name.lstrip("/").split("/", 1)[0]
                engine_content = root_mount in config.get("engine_mounts", {}) and root_mount not in mounts
                if not name.startswith(("/Engine/", "/Script/")) and name not in expected and not engine_content:
                    raise RuntimeError(f"Reference was not relocated: {package} -> {name}")
        report["references_verified"] = True
        report["assets"] = config["manifest"]
        return
    native_tokens = [f"/Script/{name}" for name in config["native_modules"]]
    report["native_modules"] = config["native_modules"]
    if config.get("clean_animation_metadata"):
        clean_animations(assets, registry, mounts, report)
    if config.get("phase") == "clean":
        return
    native_bytes = [(token, token.encode(), token.encode("utf-16-le")) for token in native_tokens]
    reasons = {}
    dependencies = {}
    engine_dependencies = {}
    engine_modules = {}
    for package, data in assets.items():
        class_name = asset_class(data)
        if package in multiple_exports:
            reasons[package] = "Multiple exported assets in a single package"
            continue
        if not supported(data):
            reasons[package] = f"Excluded asset class: {class_name}"
            continue
        # Exact engine class names alone are insufficient for custom subclasses or embedded notify types.
        class_package = str(data.asset_class_path.package_name)
        if class_package in native_tokens:
            reasons[package] = f"Lyra native asset class: {class_package}"
            continue
        mount, relative = package.lstrip("/").split("/", 1)
        file = Path(mounts[f"/{mount}"]) / (relative + ".uasset")
        if not file.is_file():
            reasons[package] = "Missing source package file"
            continue
        contents = file.read_bytes()
        engine_modules[package] = {
            config["engine_modules"][name.decode()]
            for name in re.findall(rb"/Script/([A-Za-z0-9_]+)", contents)
            if name.decode() in config.get("engine_modules", {})
        }
        native = next((token for token, narrow, wide in native_bytes if narrow in contents or wide in contents), None)
        if native:
            reasons[package] = f"Serialized dependency on Lyra code: {native}"
            continue
        dependencies[package] = [str(name) for name in registry.get_dependencies(package, DEPENDENCIES) or []]
        for dependency in dependencies[package]:
            if dependency.startswith(("/Engine/", "/Script/")):
                continue
            root_mount = "/" + dependency.lstrip("/").split("/", 1)[0]
            if root_mount in config.get("engine_mounts", {}) and root_mount not in mounts:
                if registry.get_assets_by_package_name(dependency, include_only_on_disk_assets=True):
                    engine_dependencies.setdefault(package, set()).add(config["engine_mounts"][root_mount])
                    continue
            if dependency not in assets:
                reasons[package] = f"Dependency outside source art mounts: {dependency}"
                break
    # Propagate exclusions through hard and soft dependencies, including cycles.
    changed = True
    while changed:
        changed = False
        for package, linked in dependencies.items():
            if package in reasons:
                continue
            blocked = next((name for name in linked if name in reasons), None)
            if blocked:
                reasons[package] = f"Depends on excluded package: {blocked}"
                changed = True
    seeds = sorted(package for package, data in assets.items() if asset_class(data) in ART_CLASSES)
    selected = set()
    pending = [package for package in seeds if package not in reasons]
    while pending:
        package = pending.pop()
        if package in selected:
            continue
        selected.add(package)
        pending.extend(name for name in dependencies[package] if name in assets)
    prefix = config["destination"]
    report["skipped"] = [{"package": name, "reason": reasons[name]} for name in seeds if name in reasons]
    report["class_counts"] = dict(sorted(Counter(asset_class(assets[name]) for name in selected).items()))
    report["required_engine_plugins"] = sorted({
        plugin for name in selected
        for plugin in engine_modules.get(name, set()) | engine_dependencies.get(name, set())
    })
    report["assets"] = [
        {"source": name, "destination": destination(name, prefix), "destination_root": prefix,
         "class": asset_class(assets[name])}
        for name in sorted(selected)
    ]
    report["mode"] = config.get("mode", "apply" if config["apply"] else "preview")
    report["source_class_counts"] = dict(sorted(Counter(asset_class(data) for data in assets.values()).items()))
    if len({entry["destination"].casefold() for entry in report["assets"]}) != len(selected):
        raise RuntimeError("Destination package collision; no assets were migrated.")
    write_report(Path(config["report"]), report)
    unreal.log(f"Lyra art: {len(selected)} eligible packages; {len(report['skipped'])} skipped art assets.")
    if not config["apply"] or not selected or config.get("phase") == "prepare":
        return
    loaded = []
    renames = []
    soft_paths = {}
    for entry in report["assets"]:
        asset = assets[entry["source"]].get_asset()
        if not asset:
            raise RuntimeError(f"Could not load {entry['source']}")
        loaded.append(asset)
        new = PurePosixPath(entry["destination"])
        renames.append(unreal.AssetRenameData(asset=asset, new_package_path=str(new.parent), new_name=new.name))
        soft_paths[unreal.SoftObjectPath(asset.get_path_name())] = unreal.SoftObjectPath(f"{new}.{new.name}")
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    unreal.log(f"Relocating {len(loaded)} loaded art assets...")
    if not tools.rename_assets(renames):
        raise RuntimeError("Unreal could not relocate every selected asset; target files were not written.")
    unreal.log("Updating remaining soft object paths...")
    tools.rename_referencing_soft_object_paths([asset.get_outermost() for asset in loaded], soft_paths)
    if not unreal.EditorAssetLibrary.save_loaded_assets(loaded, only_if_is_dirty=True):
        raise RuntimeError("Could not save all relocated packages.")
    report["relocation_saved"] = True


def main() -> None:
    config = json.loads(Path(os.environ["NELARIC_LYRA_ART_CONFIG"]).read_text(encoding="utf-8"))
    report = {"status": "running", "published": False, "assets": [], "skipped": []}
    try:
        run(config, report)
        report["status"] = "success"
    except Exception as error:
        report["status"] = "failed"
        report["error"] = str(error)
        report["traceback"] = traceback.format_exc()
        raise
    finally:
        write_report(Path(config["report"]), report)


if __name__ == "__main__":
    main()
