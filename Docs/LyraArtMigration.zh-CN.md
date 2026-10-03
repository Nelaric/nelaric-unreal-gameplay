<!-- Copyright (c) 2026 Nelaric Contributors -->

[English](LyraArtMigration.md) | 简体中文

# Lyra 美术资产迁移

在 Windows 上使用 Python 3.10 或更新版本，从仓库根目录运行 `Scripts/migrate_lyra_art.py`。脚本在 `ThirdParty/` 下查找 `LyraStarterGame.uproject`，并定位匹配的 Unreal Editor。`ThirdParty/` 是被 Git 忽略的本地下载源目录。源工程和目标工程必须使用相同的引擎关联；源工程的编辑器模块须已使用该引擎编译。

先预览资产选择和依赖：

```powershell
python Scripts/migrate_lyra_art.py --extract-art-only
```

实际迁移前关闭源工程和目标工程的编辑器，然后运行：

```powershell
python Scripts/migrate_lyra_art.py --extract-art-only --enable-required-plugins --apply
```

默认目标为 `/Game/ThirdParty/LyraStarterGame/LyraStarterGame`，对应 `NelaricGameplay/Content/ThirdParty/LyraStarterGame/LyraStarterGame/`。源工程资产保留相对目录；工程内容插件中的资产统一放在目标目录内的 `PluginContent/<插件名称>/` 下。

脚本选择模型、材质、贴图、动画序列、蒙太奇、混合空间、骨架、物理资产和音效，包括符合条件的 MetaSound；被引用的物理材质、曲线、曲线图集、动画压缩设置和音频调制资产会随依赖迁移。玩法蓝图、动画蓝图、地图、UI 蓝图和依赖 Lyra 专有原生代码的资产不在迁移范围内。若某个美术资产的硬引用或软引用依赖链包含被排除或不可用的包，该资产也会被排除；具体原因见报告中的 `skipped`。

`--extract-art-only`（别名 `--clean-animation-metadata`）会在副本中删除编辑器动画修改器和项目专用的动画通知对象，并清除骨骼模型的后处理动画蓝图配置，保留骨骼动画、曲线、根运动设置及引擎内置通知。被删除的项目通知不再触发原来的玩法、音效或特效行为，后处理动画蓝图原本提供的模型校正也需在目标工程中重新接入。改动会记录在 `animation_cleanup` 和 `mesh_cleanup` 中。物理碰撞体使用的 Lyra 标签物理材质会转换成引擎物理材质，保留摩擦、弹性、密度、混合模式、质量缩放及表面类型，转换记录见 `physical_material_cleanup`。不使用该参数时，脚本保留资产原样，并排除依赖不兼容内容的资产。

每次运行都使用临时工程副本，保留二进制模块，排除生成缓存和中间产物。Python 与编辑器脚本插件仅在副本中启用，工程内容插件也仅在副本中挂载以供扫描。完成资产准备后，副本仅保留所选美术资产并禁用原工程的玩法模块；随后由新的 Unreal 进程执行移动、更新软引用、保存资源，并在写入目标前检查最终依赖关系，避免玩法代码中的类默认对象引用干扰移动。原始 Lyra 工程不变。需预留一份工程副本及 Unreal 生成缓存所需的磁盘空间。

报告和引擎日志位于 `.tools/lyra-art-migration/<运行目录>/`。预览不会写入目标资产；实际迁移遇到已存在的目标目录会停止，不覆盖或合并资源。保存后的包依赖关系由新的 Unreal 进程检查。引擎自带插件内容保留原有挂载路径，所选资源使用的插件会列在 `required_engine_plugins` 中。脚本先将包复制到同级临时目录，复制全部完成后才生成最终目录。迁移后重新打开目标编辑器，检查代表性的模型、材质、动画和音效。

`--enable-required-plugins` 会在实际迁移时，将报告中列出的引擎插件在目标 `.uproject` 中启用，保留其他插件配置，不复制或启用 Lyra 的玩法模块。不使用该参数时，需手动启用相应插件。预览和验证模式都不会修改目标工程。

可选参数：

- `--source <目录或uproject>`：指定其他源工程。
- `--target-project <uproject>`：指定其他目标工程。
- `--destination /Game/ThirdParty/<名称>/<目录>`：指定新的内容路径。
- `--editor <UnrealEditor-Cmd.exe>`：指定匹配的自定义引擎安装。
- `--report-dir <目录>`：指定报告目录。
- `--validate`：在副本中执行移动和引用验证，不写入目标资产；提取独立美术资源时可搭配 `--extract-art-only`，不能与 `--apply` 同时使用。

入口脚本会在 Unreal 内调用 `Scripts/migrate_lyra_art_unreal.py`，无需直接运行该内部脚本。迁移资产与整个 `NelaricGameplay/Content/` 目录一同排除在版本控制之外。

API 参考：[Unreal Python 脚本](https://dev.epicgames.com/documentation/en-us/unreal-engine/scripting-the-unreal-editor-using-python?application_version=5.6)、[AssetTools](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/AssetTools?application_version=5.6) 和 [Asset Registry](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/AssetRegistry?application_version=5.6)。
