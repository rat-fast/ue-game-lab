# SpatialDataExportTool 空间数据导出工具

## Module Responsibility

工作区 Python CLI 编排一次 SkillLab 实际游戏世界的查询或技能释放采集；SkillLab Runtime 采集宿主读取 UE 进程内的有效配置、对象、事件、动画/空间状态并采集真实 RHI 场景图像。Python 管进程、请求和文件结果，UE 管真实运行事实；工具不另行执行技能规则或空间变换。

## Code And Assets

- 工作区入口：`tools/export_skill_spatial.py`；Python 单元测试：`tools/tests/test_export_skill_spatial.py`；请求样例：`tools/examples/spatial-inspect-request.json`、`tools/examples/spatial-capture-self-request.json`。
- UE 会话宿主：`Source/SkillLab/Public/SkillLabSpatialExportSubsystem.h` 与 `Source/SkillLab/Private/SpatialExport/`。`USkillLabSpatialExportSubsystem` 是按本工具命令行参数启用的 `UTickableWorldSubsystem`，负责请求阶段、对象定位、事件订阅、采样、渲染读回和 receipt。
- 运行观察接口分布在 `SkillLabSkills` 和 `SkillLabPresentation`：技能 Runtime 正式请求/事件、投射物创建与交付、空间参照解析、Montage/Notify 和反馈对象来源。导出会话只订阅观察，不替换门控、效果结算或表现执行。`SkillLab.Build.cs` 将 `SkillLabSkills`、`SkillLabPresentation`、`Json`、`RenderCore`、`RHI` 和 `ImageWrapper` 列为 `SkillLab` 模块的私有依赖。
- `SkillLabTestCommandsComponent::InitializeForPawn` 在导出模式下跳过 TestArena 准备，避免该测试场隐藏 NavTest 障碍或生成训练目标。
- 默认输出为 `Saved/SpatialExports/`；默认输出根目录设置为 `Saved/SpatialExportTool/settings.json`。这些都是生成/本机设置数据，不是技能运行配置。

## Runtime Entry

从工作区根目录调用：

```text
python tools/export_skill_spatial.py inspect --request <请求JSON绝对路径> [--project <uproject绝对路径>] [--engine <UnrealEditor绝对路径>]
python tools/export_skill_spatial.py capture --request <请求JSON绝对路径> [--project <uproject绝对路径>] [--engine <UnrealEditor绝对路径>]
python tools/export_skill_spatial.py configure --output-root <绝对路径> --save-default [--project <uproject绝对路径>]
```

`inspect` 启动请求指定的地图，定位指定施法者并导出其已授予技能、有效定义、模型和查询参照，不提交技能。`capture` 在同一真实 `-game` World 中通过 `USkillLabSkillRuntimeComponent::SubmitSkillRequest` 提交一次请求并采集；目标、关联对象和参照必须按请求精确提供，不自动挑选目标。`configure` 仅保存指定项目的默认输出根目录，不改技能定义。

每次查询或 capture 使用独立 `run_id` 目录。`manifest.json` 是读取入口，索引 `request.json`、`configuration.json`、`references.json`、`events.jsonl`、`samples.jsonl`、`frames/`、可选 `marked/`、`diagnostics.txt` 和 UE 阶段/回执文件；inspect 在 `not_generated` 中声明不会生成的事件、样本和图像文件。Capture 使用真实 RHI 场景透视读回；NullRHI 或没有有效 PNG 时不能报告 complete。详细请求字段和内部记录语义以 [Batch04-02 需求](../../Planning/CharacterPresentation/Batches/Batch04-OnDemandSpatialAdaptation/02-SpatialDataExportTools.md)及[实现/字段说明](../../ProgramWorkflow/CharacterPresentation/Batches/Batch04-OnDemandSpatialAdaptation/02-SpatialDataExportTools/ImplementationSummary.md)为准。

## Runtime Flow

Python 校验输入、建立唯一输出目录并启动本次 UE 进程。`USkillLabSpatialExportSubsystem` 只在 `Game` World 且启动参数 `-SkillLabSpatialExportRequest`、`-SkillLabSpatialExportOutput`、`-SkillLabSpatialExportMode` 齐备时启用；World BeginPlay 后注册 World tick-end 推进入口。子系统等待原有角色、技能 Runtime 初始化与 warmup；解析玩家索引或精确关卡 Actor 路径，按 Runtime 授予表和 Provider 已接受定义查询内容。Capture 在正式请求前订阅同一 Runtime/施法者的技能、投射物、反馈和 Notify 观察，再调用正式请求入口并继续采样到请求时长结束。UE 写 receipt 并请求退出；Python 等待进程结束，超时才收束本次进程树，随后校验材料并发布 manifest。

每个 run 独立关联 Runtime generation、请求、Cast、Submission 和对象来源。投射物创建事实在 `ConfirmDelivery` 开碰撞之前记录，以免同步 overlap 丢失；反馈和投射物另记录创建/移除或采集结束状态。多视角在同一游戏线程状态下冻结并同步 RHI 读回；骨骼 finalized 帧及读回关联按实际可取得字段记录，GPU 自身帧号不作虚构。

## Current Behavior

CLI 返回 `complete`、`partial`、`failed`，对应退出码 0、2、1；stdout 最后一条机器 JSON 给出 run 与 manifest 路径。UE 阶段文件用于定位启动/采集失败。Manifest 发布前校验索引到的配置、引用、JSONL 和 PNG；失败保留可用材料并列出错误/限制。重复执行创建新目录，不覆盖已有结果。

独立 Review 已确认 R1—R9 原实现问题修复，包括工具模式 TestArena 隔离、对象来源/代际过滤、对象寿命快照、参照查询语义、请求前时间、Notify 同步清理、失败协议和字段说明。R2 的旧代际 Notify 被过滤由 `SkillLab.SpatialExport.AnimationNotifyGeneration` 定向自动化覆盖。对应实现和证据见 [Batch04-02 实现摘要](../../ProgramWorkflow/CharacterPresentation/Batches/Batch04-OnDemandSpatialAdaptation/02-SpatialDataExportTools/ImplementationSummary.md) 与 [独立 Review](../../ProgramWorkflow/CharacterPresentation/Batches/Batch04-OnDemandSpatialAdaptation/02-SpatialDataExportTools/Review.md)。

## Current Responsibilities And Evolution Boundaries

Python 负责外部 CLI、进程期限、输出目录/设置和结果完整性检查；UE 子系统负责当前 World 内的精确查询、权威请求和运行事实、空间/动画采样与真实渲染。Skills/Presentation 只暴露窄观察能力，仍拥有各自业务生命周期。不要将 Probe 的自动选敌/移场、测试场假人、PreviewScene 或磁盘配置解析当成实际 capture 的替代路径。

请求字段、文件角色和状态语义由 Batch04-02 Planning 定义；本模块文档记录当前代码入口，不冻结内部实现类作为长期 API。若工具未来覆盖其他项目，该项目必须实现兼容的运行侧宿主；Python 的 `--project` 路径参数本身不意味着任意项目都可采集。

## Verification

重建实际工程及测试入口见 [TestAndVerification](TestAndVerification.md#空间数据导出工具)。已记录实际 SkillLabEditor Win64 Development 构建、9 项 Python 测试、6 项既有 Skills/Presentation 定向自动化及 R2 专项自动化通过；真实 RHI inspect、Mend 双视角标记 capture 和 AreaBurst 地面 capture 的范围与 manifest 位置见实现摘要。本文维护时未重跑构建、测试或游戏。

## Current Limitations

完整需求的所有场景尚未端到端覆盖。有效单位目标下的 Fireball 投射物/碰撞、BurningBuff 独立寿命、RelatedActor、其他角色复用以及超时、无 RHI、引擎异常退出等运行证据仍有缺口；AreaBurst `AreaEmpty` 只证明空区域请求和采集链。不要将自动化或单次工具 `complete` 提升为技能命中、所有场景或美术验收结论。

## Extension Directions

增加输出字段或新运行观察时，优先沿现有 Runtime/Presentation 的事实来源扩展窄接口，并同步更新本模块的字段入口和 `TestAndVerification` 的复现方式。保持外部 JSON 和文件读取契约由 Planning/任务需求维护。
