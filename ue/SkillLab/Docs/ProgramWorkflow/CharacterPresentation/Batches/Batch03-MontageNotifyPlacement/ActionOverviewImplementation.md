# AnimSequence 动作总览实现与验证

本文件记录 2026-10-28 动作总览实现，需求与边界见 [动作总览架构交接](ActionOverviewArchitectureHandoff.md)及其权威需求。独立 Review 由 PM 派发；本文件不代表 Review 或游戏内表现验收通过。

## 实现入口与职责

- `Source/SkillLabAnimationEditor/Public/SkillLabActionOverviewCommandlet.h` 与 `Private/SkillLabActionOverviewCommandlet.cpp`：独立只读 Commandlet、请求校验、本机设置、批次身份与逐项失败报告。
- `Private/SkillLabActionOverview.h/.cpp`：Sequence/DataModel、只读预览 Mesh 选择、源姿势求值、Root Motion 数据、整段蒙皮范围扫描、固定相机、RHI 回读和逐帧 PNG。新增依赖限于 Editor 模块的 `RenderCore`、`RHI`、`ImageCore`。
- 仓库 `tools/skilllab_action_overview.py`：调用 UE、Saved staging 完整性核验、正式分页归档和三种诊断分流。
- 仓库 `schemas/skilllab-action-overview-request.schema.json`、`skilllab-action-overview-manifest.schema.json`、`skilllab-action-overview.schema.json`：分别描述请求、Saved 内部完整 manifest 和正式精简索引。跨字段、图像解码与页帧映射由脚本 `validate` 检查。
- `tools/tests/test_skilllab_action_overview.py`：定向产物故障注入；合成图只验证发布机制，不充当 UE 渲染证据。

稳定机制在 Editor C++，可变输入在请求 JSON；manifest 是本次输入和实际预览条件快照。未修改 Notify 内容工具协议、技能定义、技能运行时、Lua、Blueprint 或动画资产，不调用 SavePackage。

## 调用

脚本需要 Python 3.10+ 和 Pillow。本机已验证的 Python 为 `C:/Users/Administrator/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe`。默认 UE 路径为 `D:/Epic/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe`，可用 `--editor`、`--project` 覆盖。

```powershell
python tools/skilllab_action_overview.py configure --output-root E:/agent/game/ue/SkillLab/Saved/ActionOverviews
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

UE staging 保持在项目 `Saved/ActionOverviews/<batchId>/<itemId>/`，完整逐帧 manifest、PNG 和批次报告留在 Saved。发布成功后只复制分页图和精简索引到 `Docs/ArtWorkflow/Actions/<objectName[--pathHash]>/`；其余状态进入 `Diagnostics/MetadataOnly|Failed|Incomplete/`。完整对象路径参与冲突判断，批次不作为归档主键。正式资产目录只保留当前有效总览；新 generation 先验证，再更新 `page-*.png` 和 `overview.json`，清理旧页和旧 generation 正式副本。旧完整逐帧结果仅在 Saved staging 中追溯。旧 `Docs/ArtWorkflow/Exports` 不再作为输出根路径。

正式 `overview.json` 只记录资产完整对象路径/对象名、generationId、状态、帧率、sampleCount、总时长、可推导帧公式、分页首末帧和格子布局。逐帧 seconds、Root Motion、输入指纹等完整数据保留在 Saved staging 或诊断报告中。

退出码：UE 为 `0=所有资产逐帧捕获成功`、`2=存在失败或仅元数据`、`1=请求/设置/报告写入失败`。脚本为 `0=全部正式发布/校验通过`、`2=批次存在非 complete 项`、`1=调用或归档异常`。批次部分成功时成功资产仍归档，其余逐项进 Diagnostics，批次摘要进 `Diagnostics/Incomplete/Batches/<batchId>/`。

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

2026-10-28 首轮实现验证及 2026-11-03 归档调整后的复核见下节。本文的本机证据根为 `E:/agent/game/ue/SkillLab/Saved/`；目录名使用 UTC，不把前一天 UTC 日期误认为本地验证日期。

| 验证 | 结果与可复查证据 |
|---|---|
| 当前构建 | `SkillLabEditor Win64 Development` 成功；`ActionOverviewVerification/build-final.log`。 |
| 发布与故障注入 | 10 项 unittest 通过；`ActionOverviewVerification/publication-tests.log`。覆盖有理源帧率/目标不同帧率、末端/跨页、缺帧、坏页、源文件变化、重复索引、旧页防覆盖、GPU revision 冻结、混合失败、进程中断。源帧率特殊情况使用合成元数据，未冒充实际 UE 资产测试。 |
| 首轮真实 RHI 批次（历史） | 旧 `Docs/ArtWorkflow/Exports/` 已按归档入口调整移出，不作为当前正式归档；当时 UE 日志为 `ActionOverviewCalls/419f2333a6c84c5b9673d423564569ca.log`。最新归档规则及四资产结果以下节为准。 |
| Attack03 | 上述批次 `0000-863494B44AE2F4F2C5FACC879CE8F24B/`：源30/1 FPS、50个间隔、51个采样点、1.666667s、5页。Skeleton 保存的 Quinn 预览引用；Root Motion 启用，整段提取平移 `(0,216,0)cm`。 |
| Land | 上述批次 `0001-E71482504D6B874E7AFECB987E1FAD2D/`：源30/1 FPS、26个间隔、27个采样点、0.866667s、3页；Root Motion 未启用，提取平移为零。 |
| 图片与索引 | 逐帧 PNG 全部解码、尺寸/SHA256、完整覆盖、各页范围、源格像素对照通过；两个最终资产均独立执行 `validate` 通过。打开实际页目检确认动作变化、固定取景、标注/空格可辨认；首/中/末与跨页在本轮直接查看。 |
| 重复与覆盖 | `20261027T172301-41B9798145E820993E09D8B67220192D` 和最终批次包含相同资产；独立目录，历史页保留。明确 Manny Mesh 的单资产批次 `20261027T172405-FDE9E62548A83349820C24A4708CCA79/0000-634E95A8496B9F82137D8ABEA58BB70D/` 另有51帧/5页，完整性检查通过。该早先批次视角文字为 `frontThreeQuarter`，最终已改为中性的 `fixedThreeQuarter`；实际坐标相机相同，不应依文字推断角色正面。 |
| 持久输出根路径 | 先 configure 后退出，再多个独立 UE 进程生成，未重传根路径仍沿用。日志 `ActionOverviewCalls/586e55bba9ec4d2d9ac9ed9562ad8983.log`；当前本机设置与最终 batch 的根路径一致。 |
| 路径失败 | 把已存在的 `single.json` 文件作为输出根路径，明确失败、UE 返回1，保存的原根路径未被替换；日志 `ActionOverviewCalls/11265327a7324c4094ad5671520609ca.log`。未专门模拟真实磁盘耗尽或权限拒绝。 |
| 环境与输入失败 | NullRHI 在 `renderEnvironment` 明确失败且不发布图片；日志 `ActionOverviewCalls/74a8c23e0f4144eeb3008215271604bd.log`。元数据批次 `20261027T171129-B1C133CB4BCD29E9E5950EB0EE484D2F` 分别报告有效 Sequence、Additive、缺失与非 Sequence 资产状态；未将元数据状态算成图像完成。 |
| 可消费观察示例（历史） | 旧 `Docs/ArtWorkflow/Observations/` 已随旧归档结构清理；不作为当前资产级总览。正式语义载体仍按 ArtWorkflow 入口独立维护。 |

首轮 GPU 尝试暴露了两个真实问题：Commandlet 未启用 `IsClient` 时 UE 5.8 创建 dummy scene，缺 SkeletalMeshUpdater；未推进主循环帧身份时 CPU 姿势变化但 GPU 缓冲冻结。两者均已修复。冻结图片批次 `20261027T171823-C2FABA3941D303496B2F72AD533CF2EE` 已明确标记失败，不能给下游消费；早期断言日志 `ActionOverviewCalls/8097169d362e492f84a4a8ccbcafd052.log` 仅用于诊断。进程中断后本次 receipt 与批次保留身份，调用器将尚未交付项标为失败，不伪装完成。

## 未验证与交回 PM

- 本机已完成实际 RHI 渲染，当前没有“环境无法渲染”的阻塞；但尚未在 Animation Editor 取得同一时刻的独立姿势截图对照，不能宣称排除了所有单帧偏差。`visualVerification` 保持待独立检查，最终内容及游戏内表现验收仍归下游。
- 未用真实第二种 Skeleton、有理帧率/源目标采样不同的 UE 资产、超长位移动作、并发同秒生成、物理/布料/后处理/多 Mesh 依赖做验收。相应算法或明确拒绝边界已实现，但不扩写为这些样本已通过。
- 播放映射未建立；本任务允许的最小交接是源时间与完整图像/索引。需要 Montage/动态播放映射时由 PM 确定消费范围并另交任务，不允许直接复用源秒为播放目标秒。
- 原始材质可能包含时间/WPO 等源骨骼之外效果；本版不承诺还原其最终游戏效果，也未建立全依赖图追踪。骨骼、Socket、轨迹等额外辅助层关闭。可变预览偏好及语义结果正式载体仍由 PM 协调，不擅自成为产品要求。
- 本次未改项目代码地图；新增正式工具入口可在独立 Review 通过后按 `project-documentation` 最小同步。未改 Notify 架构交接或增设需求，未派发 Review Agent。

## 2026-11-03 资产级正式归档

新的唯一正式出口按 `Docs/ArtWorkflow/README.md`、`Actions/README.md` 和 `Diagnostics/README.md`：

```text
Docs/ArtWorkflow/Actions/<AnimSequence对象名[--路径哈希]>/
  page-0000.png ...
  overview.json
Docs/ArtWorkflow/Diagnostics/MetadataOnly|Failed|Incomplete/<资产键>/<generationId>/report.json
```

- UE staging 根路径固定为 `Saved/ActionOverviews`。逐帧 `manifest.json`、原始帧图及批次报告留在 Saved，正式 Actions 只复制分页图和精简索引。
- 资产完整对象路径作为主键；同对象名的多个路径使用对象路径 SHA-256 前缀稳定区分。批量只表示一次调用，目录仍按资产和 generationId 组织。
- `overview.json` 只包含资产对象路径、对象名、generationId、complete 状态、帧率、采样数、时长、可推导帧公式、分页文件及首末帧范围、页格布局。不重复记录每帧秒值、Root Motion 和逐帧骨骼数据。
- metadataOnly/failed/incomplete 逐项分别分流到大小写稳定的 Diagnostics 子目录；非完整批次另外写 `Diagnostics/<状态>/Batches/<batchId>/batch.json`。完整资产可在混合批次中独立发布；诊断项不进入 Actions。
- `validate` 同时支持 Saved 完整 manifest 和正式 `overview.json`，校验 PNG 可解码、页帧覆盖范围和正式文件精简字段。相同 generationId 目标已存在时拒绝覆盖。
- `skilllab-action-overview-manifest.schema.json` 继续约束 Saved 内部详细 manifest；新 `skilllab-action-overview.schema.json` 约束正式精简 overview。

归档规则定向验证：15 项 Python unittest 全通过，覆盖同名对象不同路径、完整发布/同目录分页、元数据和失败/中断分流、帧率推导、正式 JSON 无逐帧字段、重复 generationId 防覆盖。测试图片是合成夹具，只验证发布器；不代替下表中的真实 UE 图片导出。

四项真实动作按请求中给定的完整对象路径重新生成；请求 JSON 只含 `assets` 路径数组，没有 Skill ID。批次 `20261102T170753-A6779C0F4B30FA1845797C949C3B7728` 状态为 `complete`，UE 调用日志为 `Saved/ActionOverviewCalls/b4060118363e4426906de72025ad7d04.log`，批次 receipt 为 `Saved/ActionOverviews/20261102T170753-A6779C0F4B30FA1845797C949C3B7728/batch.json`。四项均为 30/1 FPS：

| AnimSequence | 采样帧 | 页数 | 正式归档 |
|---|---:|---:|---|
| `/Game/SkillLab/Animations/AS_FireballCast.AS_FireballCast` | 21 | 2 | `E:/agent/game/ue/SkillLab/Docs/ArtWorkflow/Actions/AS_FireballCast/` |
| `/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_ChargedAttack.MM_ChargedAttack` | 56 | 5 | `E:/agent/game/ue/SkillLab/Docs/ArtWorkflow/Actions/MM_ChargedAttack/` |
| `/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_01.MM_Attack_01` | 31 | 3 | `E:/agent/game/ue/SkillLab/Docs/ArtWorkflow/Actions/MM_Attack_01/` |
| `/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_02.MM_Attack_02` | 31 | 3 | `E:/agent/game/ue/SkillLab/Docs/ArtWorkflow/Actions/MM_Attack_02/` |

复核命令（从仓库根目录执行；四条均输出 `Integrity=passed` 且 `OfficialOverview=true`）：

```powershell
python tools/skilllab_action_overview.py validate E:/agent/game/ue/SkillLab/Docs/ArtWorkflow/Actions/AS_FireballCast
python tools/skilllab_action_overview.py validate E:/agent/game/ue/SkillLab/Docs/ArtWorkflow/Actions/MM_ChargedAttack
python tools/skilllab_action_overview.py validate E:/agent/game/ue/SkillLab/Docs/ArtWorkflow/Actions/MM_Attack_01
python tools/skilllab_action_overview.py validate E:/agent/game/ue/SkillLab/Docs/ArtWorkflow/Actions/MM_Attack_02
```

归档调整定向测试 `python -m unittest discover -s tools/tests -p test_skilllab_action_overview.py -v`：15 项通过（`Saved/ActionOverviewVerification/archive-tests.log`）；此前发布链测试 10 项通过（`publication-tests.log`）。批次后构建记录 `Saved/ActionOverviewVerification/build-batch04.log` 为 `Target is up to date`、`Result: Succeeded`。四项正式索引校验通过、所有分页 PNG 可解码且页范围覆盖全部采样帧；第一页目检证据分别为各归档目录下的 `page-0000.png`。逐帧采样仍保存在 `Saved/ActionOverviews/20261102T170753-A6779C0F4B30FA1845797C949C3B7728/<itemId>/`。本次不创建额外全局索引 JSON；请求未提供 Skill ID，故结果不含该字段。
