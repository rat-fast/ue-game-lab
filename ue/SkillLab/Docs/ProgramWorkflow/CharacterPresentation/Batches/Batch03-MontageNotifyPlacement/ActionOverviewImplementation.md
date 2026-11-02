# AnimSequence 动作总览实现与验证

本文件记录 2026-10-28 动作总览实现，需求与边界见 [动作总览架构交接](ActionOverviewArchitectureHandoff.md)及其权威需求。独立 Review 由 PM 派发；本文件不代表 Review 或游戏内表现验收通过。

## 实现入口与职责

- `Source/SkillLabAnimationEditor/Public/SkillLabActionOverviewCommandlet.h` 与 `Private/SkillLabActionOverviewCommandlet.cpp`：独立只读 Commandlet、请求校验、本机设置、批次身份与逐项失败报告。
- `Private/SkillLabActionOverview.h/.cpp`：Sequence/DataModel、只读预览 Mesh 选择、源姿势求值、Root Motion 数据、整段蒙皮范围扫描、固定相机、RHI 回读和逐帧 PNG。新增依赖限于 Editor 模块的 `RenderCore`、`RHI`、`ImageCore`。
- 仓库 `tools/skilllab_action_overview.py`：调用 UE、PNG 解码、4×3 分页标注、SHA256 与源帧/页格像素核验、完成 manifest 发布、过期输入检测。
- 仓库 `schemas/skilllab-action-overview-request.schema.json` 和 `schemas/skilllab-action-overview-manifest.schema.json`：请求与 manifest v1 的结构说明。跨字段、时间表、文件解码和页格内容由脚本 `validate` 检查，不能仅靠 JSON Schema 判定完整。
- `tools/tests/test_skilllab_action_overview.py`：定向产物故障注入；合成图只验证发布机制，不充当 UE 渲染证据。

稳定机制在 Editor C++，可变输入在请求 JSON；manifest 是本次输入和实际预览条件快照。未修改 Notify 内容工具协议、技能定义、技能运行时、Lua、Blueprint 或动画资产，不调用 SavePackage。

## 调用

脚本需要 Python 3.10+ 和 Pillow。本机已验证的 Python 为 `C:/Users/Administrator/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe`。默认 UE 路径为 `D:/Epic/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe`，可用 `--editor`、`--project` 覆盖。

```powershell
python tools/skilllab_action_overview.py configure --output-root E:/agent/game/ue/SkillLab/Docs/ArtWorkflow/Exports
python tools/skilllab_action_overview.py generate --spec E:/agent/game/ue/SkillLab/Saved/ActionOverviewVerification/single.json
python tools/skilllab_action_overview.py validate <本次资产目录>
python tools/skilllab_action_overview.py publish-batch <本次批次目录>
```

请求示例，`mesh` 可省略：

```json
{
  "assets": ["/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_03.MM_Attack_03"],
  "mesh": "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"
}
```

设置保存在当前项目的 `Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini`、`[SkillLab.ActionOverview] OutputRoot`；未设置或不可写时失败，不回退。修改设置先探测写入，再保存并从磁盘回读。直接 UE 调用：

```text
UnrealEditor-Cmd.exe SkillLab.uproject -run=SkillLabActionOverview -SetOutputRoot=<绝对根路径> -unattended -nullrhi
UnrealEditor-Cmd.exe SkillLab.uproject -run=SkillLabActionOverview -Spec=<请求JSON> -Result=<本次专属receiptJSON> -AllowCommandletRendering -d3d11 -NoTextureStreaming -unattended
```

`-MetadataOnly` 只读元数据；`-nullrhi` 不带该标志时明确在 `renderEnvironment` 失败。元数据和原始捕获均不是完成总览；UE 成功只返回 `framesReady`，必须经过分页发布才有 `complete`。完整性通过仍保留 `visualVerification=pendingIndependentInspection`，不自动声称姿势或游戏视觉验收。

目录为 `<根>/<UTC时间-GUID>/<序号-GUID>/`。每项有 manifest、逐帧 `frames/frame-NNNNNN.png`、`page-NNNN.png`；批次有 `batch.json`。每次调用独立 GUID；同资产重跑和同名资产不覆盖。根路径失效不会改设置；页已有同名文件时发布拒绝覆盖。

退出码：UE 为 `0=所有资产逐帧捕获成功`、`2=存在失败或仅元数据`、`1=请求/设置/报告写入失败`。脚本为 `0=发布/完整性检查通过`、`2=批次不完整`、`1=调用/解析/基础异常`；部分终端可能归一化非零退出码，仍应读 batch/manifest。

## 兼容与取景规则

- 不按技能或资产名称设白名单。当前支持已保存、源 DataModel 可读、非 Additive、同 Skeleton 的单 Mesh 骨骼 Sequence；源采样点必须等于源帧间隔数加一，末端时间与长度一致。有理源帧率直接转秒，不用平台目标帧率替代。
- 预览 Mesh 顺序：明确覆盖 → 动画保存引用 → Skeleton 保存引用。读取 const API，禁止自动查找并写回。记录实际来源；覆盖无效或 Skeleton 不同直接失败。
- 原始源骨骼姿势通过 `GetBonePose(..., true)` 直接写 PoseableMesh；无 AnimInstance、通知播放、业务 Tick、物理、布料、后处理、Control Rig 和 Morph 曲线。只观察源骨骼动作；不承诺还原依赖这些系统的最终效果。每次捕获推进 `GFrameCounter`，并验证/记录 GPU 骨骼 revision 与渲染帧身份递增。
- 每个采样点扫描 LOD0 蒙皮顶点，合并整段包围范围后固定正交三分之四视角、384×384、单位组件变换和 LOD0，留约 18% 边距。整段取景导致任一帧主体高度小于 64px 时失败，避免长距离位移缩到不可读。相机不逐帧追踪；`fixedThreeQuarter` 表示固定世界坐标视角，不自动识别每个资源的角色正面。
- 地面为世界 Z=0；原始材质、主光3/补光1.5/天空光0.7、可用时加载引擎 DaylightAmbientCubemap、手动曝光、关闭 AA/运动模糊，实际相机/范围/RHI 入 manifest。预览参数本版固定在生成器版本中；额外视角、分页偏好和辅助层仍交 PM 后续决定。
- Root Motion 分开记录启用/ForceRootLock/RootLock、整段提取位移和每源帧 raw root local 变换。组件不作世界移动；Root Lock 可能影响可见姿势，manifest 明确该范围。骨骼线、Socket、武器和轨迹辅助层关闭。
- 输入动画、Skeleton、Mesh、材质父链/纹理及已存在 sidecar 留文件指纹；生成前后与消费时检查。未实现全资产依赖图追踪，依赖的引擎版本及渲染条件另入快照。
- 播放映射为 `sourceOnly_notEstablished`。不得把源帧时间直接写成 Montage 点位；动态/保存 Montage 映射及语义内容由下游另行建立。生成器不内置语义判断，内容侧产物应引用 generationId、输入指纹和页格证据。

## 验证状态

2026-10-28 完成以下定向验证。本文的本机证据根为 `E:/agent/game/ue/SkillLab/Saved/`；目录名使用 UTC，不把前一天 UTC 日期误认为本地验证日期。

| 验证 | 结果与可复查证据 |
|---|---|
| 当前构建 | `SkillLabEditor Win64 Development` 成功；`ActionOverviewVerification/build-final.log`。 |
| 发布与故障注入 | 10 项 unittest 通过；`ActionOverviewVerification/publication-tests.log`。覆盖有理源帧率/目标不同帧率、末端/跨页、缺帧、坏页、源文件变化、重复索引、旧页防覆盖、GPU revision 冻结、混合失败、进程中断。源帧率特殊情况使用合成元数据，未冒充实际 UE 资产测试。 |
| 最终真实 RHI 批次 | `Docs/ArtWorkflow/Exports/20261027T172547-7147AEA04B572F5B3E135D876E551DF8/batch.json`；UE 日志 `ActionOverviewCalls/419f2333a6c84c5b9673d423564569ca.log`。RTX 4060 Laptop / D3D11；两个有效资产完成，Additive 与缺失资产失败，整批如实为 `incomplete`、UE 返回2。 |
| Attack03 | 上述批次 `0000-863494B44AE2F4F2C5FACC879CE8F24B/`：源30/1 FPS、50个间隔、51个采样点、1.666667s、5页。Skeleton 保存的 Quinn 预览引用；Root Motion 启用，整段提取平移 `(0,216,0)cm`。 |
| Land | 上述批次 `0001-E71482504D6B874E7AFECB987E1FAD2D/`：源30/1 FPS、26个间隔、27个采样点、0.866667s、3页；Root Motion 未启用，提取平移为零。 |
| 图片与索引 | 逐帧 PNG 全部解码、尺寸/SHA256、完整覆盖、各页范围、源格像素对照通过；两个最终资产均独立执行 `validate` 通过。打开实际页目检确认动作变化、固定取景、标注/空格可辨认；首/中/末与跨页在本轮直接查看。 |
| 重复与覆盖 | `20261027T172301-41B9798145E820993E09D8B67220192D` 和最终批次包含相同资产；独立目录，历史页保留。明确 Manny Mesh 的单资产批次 `20261027T172405-FDE9E62548A83349820C24A4708CCA79/0000-634E95A8496B9F82137D8ABEA58BB70D/` 另有51帧/5页，完整性检查通过。该早先批次视角文字为 `frontThreeQuarter`，最终已改为中性的 `fixedThreeQuarter`；实际坐标相机相同，不应依文字推断角色正面。 |
| 持久输出根路径 | 先 configure 后退出，再多个独立 UE 进程生成，未重传根路径仍沿用。日志 `ActionOverviewCalls/586e55bba9ec4d2d9ac9ed9562ad8983.log`；当前本机设置与最终 batch 的根路径一致。 |
| 路径失败 | 把已存在的 `single.json` 文件作为输出根路径，明确失败、UE 返回1，保存的原根路径未被替换；日志 `ActionOverviewCalls/11265327a7324c4094ad5671520609ca.log`。未专门模拟真实磁盘耗尽或权限拒绝。 |
| 环境与输入失败 | NullRHI 在 `renderEnvironment` 明确失败且不发布图片；日志 `ActionOverviewCalls/74a8c23e0f4144eeb3008215271604bd.log`。元数据批次 `20261027T171129-B1C133CB4BCD29E9E5950EB0EE484D2F` 分别报告有效 Sequence、Additive、缺失与非 Sequence 资产状态；未将元数据状态算成图像完成。 |
| 可消费观察示例 | `Docs/ArtWorkflow/Observations/MM_Attack_03.observation.json` 独立记录最终 Attack03 的准备、高位踢出、回收观察，引用 generationId、输入指纹、源帧/页格；只演示可复查观察，未建立正式内容契约或写 Notify。 |

首轮 GPU 尝试暴露了两个真实问题：Commandlet 未启用 `IsClient` 时 UE 5.8 创建 dummy scene，缺 SkeletalMeshUpdater；未推进主循环帧身份时 CPU 姿势变化但 GPU 缓冲冻结。两者均已修复。冻结图片批次 `20261027T171823-C2FABA3941D303496B2F72AD533CF2EE` 已明确标记失败，不能给下游消费；早期断言日志 `ActionOverviewCalls/8097169d362e492f84a4a8ccbcafd052.log` 仅用于诊断。进程中断后本次 receipt 与批次保留身份，调用器将尚未交付项标为失败，不伪装完成。

## 未验证与交回 PM

- 本机已完成实际 RHI 渲染，当前没有“环境无法渲染”的阻塞；但尚未在 Animation Editor 取得同一时刻的独立姿势截图对照，不能宣称排除了所有单帧偏差。`visualVerification` 保持待独立检查，最终内容及游戏内表现验收仍归下游。
- 未用真实第二种 Skeleton、有理帧率/源目标采样不同的 UE 资产、超长位移动作、并发同秒生成、物理/布料/后处理/多 Mesh 依赖做验收。相应算法或明确拒绝边界已实现，但不扩写为这些样本已通过。
- 播放映射未建立；本任务允许的最小交接是源时间与完整图像/索引。需要 Montage/动态播放映射时由 PM 确定消费范围并另交任务，不允许直接复用源秒为播放目标秒。
- 原始材质可能包含时间/WPO 等源骨骼之外效果；本版不承诺还原其最终游戏效果，也未建立全依赖图追踪。骨骼、Socket、轨迹等额外辅助层关闭。可变预览偏好及语义结果正式载体仍由 PM 协调，不擅自成为产品要求。
- 本次未改项目代码地图；新增正式工具入口可在独立 Review 通过后按 `project-documentation` 最小同步。未改 Notify 架构交接或增设需求，未派发 Review Agent。
