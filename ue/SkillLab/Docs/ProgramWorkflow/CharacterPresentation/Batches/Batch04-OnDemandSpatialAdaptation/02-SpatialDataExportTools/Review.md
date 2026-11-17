# Batch04-02 独立程序 Review

## 结论与范围

**2026-11-17：不通过，当前不能按完整需求交付。** 已有真实采集能力，但存在以下实现阻塞，不能将剩余工作概括为仅缺场景覆盖。修复交实现负责人；本轮仅写本 Review 与任务 README，未修改实现、配置、资产、Planning 或实现摘要。

Reviewer：threadId `01a1e5b5-2347-7d51-aee8-a8aaad4e7207`，hostId `local`，与实现作者及程序 PM 分属不同对话。依据：[完整需求](../../../../../Planning/CharacterPresentation/Batches/Batch04-OnDemandSpatialAdaptation/02-SpatialDataExportTools.md)、批次 README、表现基线 README/01—03、SkillSystem 目标与独立效果寿命约定、当前架构交接、实际工作树相关源码/工具/测试改动与运行产物。需求首页状态不缩减范围；摘要转述的历史 Review 不作为通过依据。下文 `Source/` 相对 `ue/SkillLab/`，`tools/` 相对工作区。

## 阻塞问题

1. **R1 / P1：测试场隔离未生效，实际改变了采集场景。** `Source/SkillLab/Private/SkillLabTestCommandsComponent.cpp:38` 用 `FParse::Param`，Python `tools/export_skill_spatial.py:414` 却传 `-SkillLabSpatialExportRequest=<路径>`。UE 5.8 `Core/Private/Misc/Parse.cpp:325` 要求参数名后为空白或结束，不接受此处的 `=`。实际证据：`Saved/Logs/SkillLab-backup-2026.11.16-18.45.44.log:492` 对应双视角 run `20261117T024528-f0b7ce0b`，1653、1654 行隐藏/关闭两个 NavTest 障碍，1662、1669、1676 行生成三个训练目标；最新 `Saved/Logs/SkillLab.log` 同样如此。这违反需求第 2、3、5 节，摘要“没有生成假人”的结论不成立。**最小复查**：修复开关识别，重跑同请求，确认原障碍可见且保持碰撞，没有新增训练目标，重新取得场景保真证据。

2. **R2 / P1：对象来源过滤和身份关联不完整。** `SpatialExport/SkillLabSpatialExportObservations.cpp:80` 的进程级投射物观察只比较 SkillId，不比较 World、Caster、Request/Cast；`SkillLabSpatialExportSampling.cpp:82` 扫描全部反馈 Actor，无来源过滤。`Source/SkillLabSkills/Public/SkillLabSkillObservation.h` 不携带 Runtime/generation/SubmissionKey，无法可靠关联同 Cast 多提交点的具体投射物与效果；反馈只保存数字 Request/Cast。`RecordSkillEvent:22` 在提交前 RequestId=0 时也可收入同技能旧事件。其他来源或旧对象会混入本次结果，违反需求第 4、6 节的采集单元与稳定身份约定。**最小复查**：同技能不同拥有者/旧新请求、同 Cast 多提交点均可由导出数据唯一关联，仅选中来源进入本 run；不改业务门控。

3. **R3 / P1：实际生成瞬间及独立对象寿命记录缺失。** Router 的 `SetObservationSource` 仅写对象成员，导出侧 `Sampling.cpp:82` 只作周期扫描，没有反馈对象生成瞬间的变换和时间。投射物仅有 created/delivery 观察，弱引用失效后直接移出采样；`SkillLabSpatialExportSubsystem.cpp:272` 的 Finish/receipt 没有对象移除、仍存活或“截至结束尚未观察到”的记录。低频采样可完全漏掉短命反馈，持续对象也无法区分自然移除、参照失效和采集截止。这是需求第 4、5 节的真实实现缺失。**最小复查**：记录生成瞬间及对象移除/截止状态；先用 Mend 短命反馈核验低频不丢生成事实，再补 Cast 完成后仍存活的投射物/Buff 场景。

4. **R4 / P2：生成对象的几何与资源版本线索不足。** `SkillLabSpatialExportConfiguration.cpp:127` 的本地 bounds 只覆盖采集前 caster/target/related 的 StaticMesh；生成后的反馈网格 (`Sampling.cpp:94`) 和投射物视觉 (`Observations.cpp:113`) 仅有路径/变换，没有本地 bounds、实际可见件和额外尺寸基准。Mend 结果列 ShapeMesh 与四个 FramePieces，却不说明有效可见件及反馈半径基准，读取者无法仅凭材料解释几何尺寸。资源也只有对象路径，没有区分同路径源资产改版的线索或缺失声明；已实现的目录 JSON+Lua 指纹不能替代资产版本。需求第 5、6 节未满足。**最小复查**：导出实际生成组件的几何、父参照、有效可见件/尺寸基准及可取得的资源版本线索，明确不可取得项；无需读源码即可解释同一 Mend 对象。

5. **R5 / P2：查询输出与真实参照语义有误。** `Configuration.cpp:114` 将 `GetAllSocketNames()` 全部列作 sockets；实际 Manny 输出 bones=89、sockets=94，89 个骨骼全部混入 sockets，但显式 socket 查询的 `GetSocketByName` 会拒绝它们。`Queries.cpp:125` 对无 Mesh 的 origin 把世界变换原样写作 local，再标 parent=`component` 且组件路径为空。`Subsystem.cpp:192` 仅保存输入 Target，Self 正式解析到自身后不更新观察上下文，合法 Self 的 target 查询/Target 空间绑定仍对空 Actor 求值。精确定位失败不提供需求第 3 节要求的候选；inspect 的 selected_skill_id 也未核验是否属于角色有效技能。**最小复查**：inspect 名称可直接用于同 kind 查询；origin 局部父空间明确；省略 target 的 Self 可查询正式自身目标；无效技能/对象返回明确失败及适用候选。

6. **R6 / P2：请求前样本使用预计提交时刻，覆盖范围也非实测。** `Sampling.cpp:30` 用 warmup+pre_request 推算相对时间，但正式提交发生于同步读回之后 (`Subsystem.cpp:215`)。双视角 run 的 `000001` 与 Started 都在 world time `1.0058625069796108`，样本却标约 `-0.2446` 秒，不能作为相对实际请求的时间。receipt coverage 直接写请求 duration，不给实际覆盖起止。违反需求第 5、6 节的时间/覆盖语义。**最小复查**：请求前样本保留可由实际提交时刻计算的时间基准，给出实际范围；首帧读回跨过等待时间时也不能产生虚假负偏移。

7. **R7 / P1：新增 Notify 观察在同步回调后读取失效播放记录。** `Source/SkillLabPresentation/Private/SkillLabSkillAnimationComponent.cpp:94` 在 `ForwardValidatedNotify` 返回后读取 `Match->NotifyAnimation`。正式转发可同步进入 Ability/行为的 Completed/Cancelled 回调，表现 Lua/适配调用 StopCastAction，`:121` 从 Playbacks 删除元素，Match 即失效。这是本次新增代码对公开 ForwardCastEvent 路径的回归，也影响普通游戏。**最小复查**：转发前冻结所需观察数据；用会同步终止并清理当前动作的合法事件验证，无失效读取且仍观察原 Token。

8. **R8 / P2：失败机器协议、结果校验和阶段反馈有缺口。** `tools/export_skill_spatial.py:474` 的 argparse 错误未纳入机器协议；本轮实际执行 `python tools/export_skill_spatial.py capture` 返回 **2**、stdout 为空，违反校验失败应 failed/1 并返回 JSON 的约定。`finalize_manifest:234` 对 configuration/references 只检查存在，UE 对这些文件的写入返回值也不检查，截断/无效 JSON 仍可能 complete。UE receipt 只在 Finish 写入且无阶段，Python 超时仅说“阶段见 ue.log 和回执”；run 内 ue.log 实际只有启动/LiveCoding 输出，不能保证定位阶段。**最小复查**：参数错误、配置/参照文件无效、启动/采集超时均给可读 JSON、正确状态及阶段，保留材料并证明只收束本次进程。保留现有 JSONL、PNG 与多视角编号检查。

9. **R9 / P2：数据说明和缺失项交接未完成。** 目前只有源码、请求样例和高层摘要，没有需求第 6、7 节要求的 configuration/events/samples 内部字段说明。manifest 在上述语义缺失时仍 complete、missing/limitations 为空；inspect 未生成文件的说明、实际角色/Runtime/Cast 关联入口、源资产版本缺口也未明确交接。**最小复查**：补最小字段说明、索引与限制声明，使独立读取者从 manifest 定位身份、空间、事件、PNG 和缺口；不新增框架或改策划标准。

## 已支持结果与待补证据

- 本轮复跑既有 **8 项 Python 测试全通过**，但未覆盖上述关键缺陷。核对 `Saved/Logs/SpatialExportTargetedTestsFinal.log`：6 项指定 UE 自动化均 Success、队列结束。核对 `C:/Users/Administrator/AppData/Local/UnrealBuildTool/Log.txt`：2026-11-17 02:44:53 的实际 SkillLabEditor Win64 Development 构建 Succeeded，run 日志加载实际工程 DLL。本轮未重新构建或启动 UE。
- 已核对摘要所引 inspect、Mend 双视角、AreaBurst、坏参照、坏目标和坏施法者 manifests，索引文件均存在。PNG/marked 可读，视觉核对有真实场景、角色和参照标记。Mend 经正式 Runtime 门控受理，Montage/Notify、完整 SubmissionKey 字段、姿势 finalized frame/revision、相机矩阵确有数据。game/custom 在同一次 World tick 尾部冻结并同步 Capture/ReadPixels，中间不推进 World tick；HUD/AA 差异有声明。**缺少 GPU 自身帧号不单独阻塞，也不能声称已有该帧号。**
- [Mend 双视角](../../../../../../Saved/SpatialExports/20261117T024528-f0b7ce0b/manifest.json) 的 complete 是当前代码计算结果，受 R1 等影响，不接受为完整需求验收。[AreaBurst](../../../../../../Saved/SpatialExports/20261117T022506-9bf57ef8/manifest.json) 的 AreaEmpty 只证明空区域请求和采集，不证明有效单位效果。坏目标/施法者 failed、坏查询 partial 有真实材料支持。
- **接口已实现、场景证据不足**：正式 actor/ground/self/RelatedActor 传入、投射物创建/交付观察、速度/碰撞球与视觉朝向分离、Cast 后保持订阅/采样、跨技能入口均有代码。有效单位请求、Fireball 飞行/碰撞、BurningBuff 独立效果及跟随/移除、RelatedActor 独立来源与缺失、精确关卡 Actor、其他支持角色/内容仍待实际验证；不能因缺样本就说接口不存在，R2—R5 的真实缺陷也不能仅列为待证。
- **需要支持场景的外部验收条件**：有效敌方单位、不同角色/内容、移动后释放、近墙/重叠出生、持续效果观察，需要调用者或验证负责人提供正常场景/夹具。正式工具不得自动找敌人、生成假人或改变障碍；当前缺场景不代表需求已验收，也不要求新增生产 NPC 系统。
- **不依赖有效敌方资产的待证项**：超时/进程树收束、无 RHI/零 PNG、引擎异常退出、不可写目录、默认/临时输出优先级及适用门控拒绝场景仍缺端到端覆盖。高速/并行动画边界也待证。坏查询样例同时使用坏组件路径，未覆盖有效组件上的错误 bone/socket 各分支；这些不能统称为等待有效单位资产。

## 可选建议与复查边界

Python CLI 当前 483 行，将参数/设置、进程编排、PNG/结果校验集中一处，架构建议拆分但未说明例外理由。建议负责人适度拆分或说明共享状态/阅读边界，不因行数单独阻塞，不引入新框架。

修复后按 R1—R9 定向复查，无需重跑全部历史 Review。项目入口/测试文档尚未反映本工具，通过后由文档负责人同步。当前可交接的是“已有可运行采集链、含已知实现缺陷的阶段产物”；程序 PM 应据此修正状态和后续派工。

## 2026-11-17 R1—R9 定向复查

本轮仅核实原 R1—R9 中声称已修复的实现/工具缺陷；按用户最新指示，不以 Fireball、BurningBuff、RelatedActor、跨角色及异常场景的端到端样本不足判定本轮修复失败。保留上文原始 Review 作为历史记录。

**结论：R1—R9 的原实现问题均已修复。** R2 最后剩余的 Notify Runtime-generation 过滤已补齐并通过定向自动化；本轮未发现原 Review 中仍未修复的实现缺陷。完整需求场景验收仍按 README 记录为未完成，本轮不以此否定 R1—R9 修复。

1. **R1 已修复。** `Source/SkillLab/Private/SkillLabTestCommandsComponent.cpp:39` 以 `FParse::Value` 读取 `-SkillLabSpatialExportRequest=<路径>`，工具模式不调用 TestArena 准备。真实采集 `20261117T033844-6a2e47ba` 的 `configuration.json.scene_preservation` 记录两个受保护障碍未隐藏、碰撞启用且组件可见，dummy tag count 为 0；支持原场景未被旧开关副作用改变。
2. **R2 已修复。** `Source/SkillLab/Private/SpatialExport/SkillLabSpatialExportObservations.cpp:231` 在写 `events.jsonl` 前同时校验 Runtime 指针、`Token.RuntimeGeneration == CapturedRuntimeGeneration` 和 CastId。新增 `SkillLab.SpatialExport.AnimationNotifyGeneration` 自动化直接调用记录入口：generation 22 对 captured generation 23 时确认 `events.jsonl` 未创建；generation 23 时确认只写入一行且保留 generation 23/CastId 7。`Saved/Logs/SpatialExportR2NotifyGeneration.log` 显示测试 `Result={Success}` 且 Automation Queue Empty。实现摘要记录实际工程 UE 5.8 Development 构建成功。本项缺口关闭。
3. **R3 已修复。** 反馈 Actor 创建时广播并快照实际变换/时间，`EndPlay` 广播移除原因；投射物 `EndPlay` 同样广播移除。Finish 写入 `projectile_not_observed_at_capture_end`、`alive_at_capture_end` 或 `observation_lost` cutoff 事实。`20261117T033844-6a2e47ba/events.jsonl` 实际含 Mend `feedback_created` 与 `feedback_removed`（`LifeSpanExpired`），两条都关联 Request/Cast/runtime generation/submission key，证明低频采样之间的短命对象不会丢创建/移除事实。
4. **R4 已修复。** 生成反馈与投射物导出静态网格 bounds、父组件、可见性、世界变换及 package/lighting GUID；反馈还提供 radius basis/visual scale。上述 Mend 创建事件列出 ShapeMesh 与四个 FramePieces 的实际可见状态、bounds 和版本线索。文档明确 GUID 不是源文件字节哈希，避免过度承诺。
5. **R5 已修复。** inspect 通过 `GetSocketByIndex` 单独枚举 sockets；`20261117T033532-9897d60f/references.json` 实际为 89 bones、5 sockets，`weapon_r` 仅为 bone、`weapon_r_muzzle` 仅为 socket。空间 origin 记录 RootComponent 父路径与局部变换；Self 会把正式目标上下文设为 Caster；精确 Actor 路径错误返回候选，capture 校验技能已授予且可解析。该结论以对应源码和 inspect 产物为据，不声称本轮运行了所有无效输入负例。
6. **R6 已修复。** 请求前样本把相对 world/wall 时间写为 JSON null，并标 `before_request`；真实 capture 首样本符合此语义。receipt 使用首末实测 sample 时间计算实际 coverage 起止与持续时间，未再把请求 duration 冒充实测范围。新样本与回执中的实际时间字段相互对应。
7. **R7 已修复。** `SkillLabSkillAnimationComponent.cpp:93-94` 和 `:112-113` 均在进入可能同步结束 Cast 的转发前复制 Token/动画指针，回调后不再解引用 `Playbacks` 中的记录。`SkillLab.Presentation.SubmissionIdentity` 自动化成功。测试通过观察委托同步清空 Playbacks 并确认收到原 Token；该测试没有直接构造 Runtime 转发期间的 Completed/Cancelled 清理，但代码修复点明确，当前未见原 use-after-invalidation 路径残留。
8. **R8 已修复（代码路径核对）。** 实际执行缺少 `--request` 的 `capture` 命令返回 stdout JSON `{"status":"failed",...}` 和退出码 1。`finalize_manifest` 解析并校验 configuration/references JSON 及运行身份、场景保真、Started 和 JSONL/PNG 关联；UE 写 configuration/references 的失败会加入错误并阻止 complete。UE 更新 `ue_phase.json` 阶段检查点，receipt 含 phase/failure_phase，Python 超时读取最近阶段并只按本次 PID 收束进程树。超时/异常退出等路径尚无本轮端到端运行证据，故仅确认实现逻辑修复，不宣称这些故障场景已实测。
9. **R9 已修复。** `ImplementationSummary.md` 已增加 manifest、configuration、references、events、samples 与生成对象字段读取说明，并说明资源 GUID 的限制；README 已指向 inspect 的 `not_generated`、场景保真与尚待端到端覆盖的场景。本轮按授权未修改实现摘要。字段索引与限制交接满足原 Review 所指的文档缺口。

**复查结果：** 原 R1—R9 实现缺陷均已关闭。有效单位目标、Fireball/BurningBuff、RelatedActor、跨角色等场景验收仍按原需求记录为后续验收范围，但依本轮用户指示不影响本次局部修复复审结论。
