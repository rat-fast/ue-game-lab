# Batch04-02 实现与验证摘要

## 交付状态

**R1—R9 原实现问题已由原 Reviewer 定向复查并确认关闭；完整需求场景验收未完成。** 需求首页仍为“需求讨论稿，供用户审阅”，本轮按用户授权执行正文，没有改写该状态或缩小需求范围。

Python 工具已能启动实际 SkillLab `-game` 工程，执行只读 inspect，或沿 `SubmitSkillRequest` 正式提交一次技能请求并保存实际场景透视读回、事件、运行配置、模型、动作和指定参照数据。最新 Mend RHI capture `20261117T033844-6a2e47ba` 验证原场景障碍/碰撞保持且未生成训练目标，包含双视角与标记图、同步姿势、Notify、真实来源关联及周期之外的反馈移除事件。旧 run `20261117T024528-f0b7ce0b` 因 TestArena 实际改变场景，已撤销其场景保真证据资格；它的 PNG/sync 机制证据仍可单独引用。`complete` 只表示所请求的工具采集完成，不代表技能命中；尚缺有效单位目标、Fireball/BurningBuff 独立对象寿命、RelatedActor、其他角色/配置复用及多种失败路径的端到端覆盖。

## 产物和职责

- `tools/export_skill_spatial.py` 提供 `inspect`、`capture`、`configure`。校验输入、补齐参数、创建不覆盖的 run 目录、启动显式 UE 可执行文件、执行墙钟超时收束、校验 PNG、写最终 manifest 和末尾机器 JSON。PNG 校验检查块 CRC、IDAT 解压、格式、宽高；无可用截图的 capture 为 failed。配置保存在所选工程 `Saved/SpatialExportTool/settings.json`。
- `ue/SkillLab/Source/SkillLab/Public/SkillLabSpatialExportSubsystem.h` 与 `Private/SpatialExport/` 的 Session、Configuration、Queries、Sampling、Models、Observations、Rendering 单元实现仅由本工具启动参数启用的 World 会话。请求通过正式 `SubmitSkillRequest` 提交；取样在 World tick 尾部发生，多视角 SceneCapture 在同一 game frame 冻结相机并同步 RHI readback，中间不推进 World tick。每份 PNG 记录相机/投影矩阵、game-thread capture frame、world time、RHI 和工具生成的 capture/readback ID；UE 未暴露 GPU 自身帧号，结果明确标为 unavailable。
- `SkillLabTestCommandsComponent::InitializeForPawn` 在工具启动时跳过 `PrepareForSkillTesting`，避免 TestArena 自动生成假人及改动障碍碰撞；其他场景初始化仍沿原路径执行。
- Skills 新增只读投射物观察总线。Deferred Actor 完成初始化和 `FinishSpawning` 后、`ConfirmDelivery` 开碰撞前发送 created 记录；交付调用后另记 accepted/rejected。Provider 在候选目录成功接受时对原始 `SkillDefinitions.json` 与排序后的已加载 Lua 源计算 `sha1:` 指纹；失败重载保留旧定义和旧指纹。配置导出使用该内存快照指纹，不用事后磁盘 hash 冒充。
- `FSkillLabSpatialTransform::ResolveReference` 返回执行使用的同一参照 Actor、Mesh、bone/socket 类型和实际变换；运行求值与导出共用它，不在导出器重做空间来源解析。显式 bone/socket 查询分别校验类型，输出父参照、局部和世界变换；采样也读取有效空间定义绑定的非空 socket。
- Presentation 动作组件提供当前 Montage 播放的窄只读副本；周期样本记录 Montage、瞬态身份、Notify Animation、Instance、Section、位置、速率、权重和分段源动画。Notify 转发观察记录 Cast 身份、Tag、状态结束和是否通过。反馈 Actor 保存创建事件/技能/效果/目标/声明名身份，样本持续记录 Actor 与静态网格；投射物样本记录移动方向、速度、碰撞球与视觉子组件。
- Python 将原始请求和规范化请求分别保存，manifest 索引有效 PNG 与标记图，并逐样本核对所有请求视角与编号。事件/样本 JSONL 使用单行紧凑序列化；Python 逐行解析、capture 受理后要求 accepted `request_return`，UE 追加写失败进入 receipt errors。损坏 JSONL/PNG、错误输入、无 RHI、漏视角和参照查询错误进入状态及 diagnostics。
- 请求样例：[`spatial-inspect-request.json`](../../../../../../../../tools/examples/spatial-inspect-request.json) 与 [`spatial-capture-self-request.json`](../../../../../../../../tools/examples/spatial-capture-self-request.json)。后者针对当前 NavTest 关卡的玩家 0 和 `Skill_Mend`，指定 `weapon_r_muzzle` Socket。

## 可复现验证

工作区根目录为 `E:/agent/game`，引擎为 UE 5.8：

```powershell
& 'D:/Epic/UE_5.8/Engine/Build/BatchFiles/Build.bat' SkillLabEditor Win64 Development 'E:/agent/game/ue/SkillLab/SkillLab.uproject' -WaitMutex -NoHotReloadFromIDE
python -m unittest tools.tests.test_export_skill_spatial -v
python tools/export_skill_spatial.py inspect --request 'E:/agent/game/tools/examples/spatial-inspect-request.json' --engine 'D:/Epic/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe'
python tools/export_skill_spatial.py capture --request 'E:/agent/game/tools/examples/spatial-capture-self-request.json' --engine 'D:/Epic/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe'
```

最终实际工程 `SkillLabEditor Win64 Development` 构建成功。9 项空间导出 Python 测试通过，覆盖输入校验、argparse JSON 失败协议、PNG 完整性、配置落盘和多视角/标记 PNG 索引/来源身份一致性。R1—R9 复查后的 NullRHI 定向自动化 6 项全部 Success：`SkillLab.Skills.DefinitionValidation`、`SkillLab.Skills.SpatialTransform`、`SkillLab.Skills.SubmittedActionLifetime`、`SkillLab.Presentation.SpatialLifecycle`、`SkillLab.Presentation.SubmissionIdentity`、`SkillLab.Presentation.CastAnimationResources`；日志为 [`SpatialExportReviewR1R9.log`](../../../../../../Saved/Logs/SpatialExportReviewR1R9.log)。`SubmissionIdentity` 包含同步通知观察器清理 playback 的终态回归。该自动化只验证机制，不替代可见运行证据。

最终 inspect：`ue/SkillLab/Saved/SpatialExports/20261117T015826-57544f7d/manifest.json`，状态 `complete`；实际定位 NavTest 中 Player 0，列出已授予四技能、有效 Provider 定义/空间配置、Manny Mesh/Skeleton、骨骼及 Socket 名称。Provider 快照指纹在对应 `configuration.json` 中。

最终 capture：`ue/SkillLab/Saved/SpatialExports/20261117T015750-9e113a7a/manifest.json`，状态 `partial`、退出码 2。请求 `Skill_Mend` 自疗一次，RequestId/CastId 均为 1，事件记录到 Started、释放阶段、TargetSelected、Hit、EffectApplied、SubmissionCommitted 和 Completed；`weapon_r_muzzle` Socket 查询为 resolved，含父骨骼 `weapon_r`、局部/世界变换。生成 24 张 1280×720 真实游戏视口 PNG，其中 22 张在请求后；记录实际采样约 7.33 Hz、5 个预期采样漏点。`frames/game/000010.png` 可见游戏世界、Mend 命中 HUD 文本及技能栏。截图与骨骼求值帧的同步身份没有证据，因此 manifest 不宣称 complete。

`Saved/SpatialExports` 是生成证据，重复调用会创建新 run，不覆盖旧结果。旧的 `20261117T013722-5df869ae` 是 SHA256 平台 API 断言导致的失败启动；之后改为 UE 5.8 支持的 FSHA1，并由上面的成功 inspect/capture 复验。

**旧多视角机制样本（场景保真资格已撤销）**：[`20261117T024528-f0b7ce0b/manifest.json`](../../../../../../Saved/SpatialExports/20261117T024528-f0b7ce0b/manifest.json) 仍可证明该次 PNG、双视角同步、姿势 finalized frame/revision、Montage/Notify 和标记图机制；Review 查明同 run 误触发 TestArena，隐藏 NavTest 障碍并生成三个训练目标。因此不得继续用它证明障碍保留、碰撞未改或无训练目标。

**R1—R9 定向修复与真实 RHI 复跑（2026-11-17）**：[`20261117T033844-6a2e47ba/manifest.json`](../../../../../../Saved/SpatialExports/20261117T033844-6a2e47ba/manifest.json) 以原双视角请求重新运行 Mend。`configuration.json.scene_preservation` 记录 `StaticMeshActor_6`、`StaticMeshActor_7` 均未隐藏且 Actor/组件 collision enabled、组件可见，dummy 标签数为0。5个样本/两个视角及标记 PNG 均有效，目标2Hz、漏采0。`samples.jsonl` 的 `000001` 为请求前样本，两个 `since_request_*` 字段为 null；capture 的实际覆盖起止/持续时间从已读 World 样本计算。正式 Started 绑定 request/cast/runtime generation；反馈 created 与 `LifeSpanExpired` 事件精确关联同一 submission key，时间位于周期采样之间；读取者可见四个 FramePieces，基础 Cube bounds 为中心 `[0,0,0]`、extent `[50,50,50]`，各 piece 的父组件、世界变换、比例、可见性和 radius basis 均已记录。游戏 PNG 可见原有大障碍在角色前方。GPU 自身帧号仍是 unavailable。

**R2 代际 Notify 补修（2026-11-17）**：`RecordAnimationNotify` 现在要求 `Token.RuntimeGeneration == CapturedRuntimeGeneration`，旧 Runtime 代际的迟到 Notify 不会追加到本次 `events.jsonl`。新增 `SkillLab.SpatialExport.AnimationNotifyGeneration` 自动化，直接调用记录入口并读取实际 JSONL：旧代际后文件仍不存在，当前代际匹配后恰有一条记录且 CastId/generation 正确。UE 5.8 实际工程 `SkillLabEditor Win64 Development` 构建成功；[`SpatialExportR2NotifyGeneration.log`](../../../../../../Saved/Logs/SpatialExportR2NotifyGeneration.log) 中该测试 `Result={Success}`、队列清空。原 Reviewer 定向复查通过，见 [Review.md](Review.md)。

**R5 inspect 定向复跑**：[`20261117T033532-9897d60f/manifest.json`](../../../../../../Saved/SpatialExports/20261117T033532-9897d60f/manifest.json) 结果为 complete，references 列出 89 bones 与 5 sockets；`weapon_r` 仅在 bones，`weapon_r_muzzle` 仅在 sockets。inspect 不生成 `events.jsonl`、`samples.jsonl`、`frames/`、`marked/`；coverage 的无样本起止字段为 null。

**地面目标运行（2026-11-17）**：[`20261117T022506-9bf57ef8/manifest.json`](../../../../../../Saved/SpatialExports/20261117T022506-9bf57ef8/manifest.json) 对 `Skill_AreaBurst` 正式提交 `[250,0,0] cm`，5 个样本/5 张 PNG，2 Hz，complete。事件为 `TargetSelected`、`Hit(AreaEmpty)`、`SubmissionCommitted` 和 `Completed`；当前场景没有有效单位，工具没有生成训练假人或伪造命中。

**异常参照运行（2026-11-17）**：[`20261117T024956-ab2a22c6/manifest.json`](../../../../../../Saved/SpatialExports/20261117T024956-ab2a22c6/manifest.json) 请求不存在的 Mesh/socket，技能请求仍沿正式路径受理，保留真实样本和 PNG，manifest `partial` 并明确组件路径匹配 0；同一查询错误已去重为单条。更早 run `20261117T022541-7a3cd277` 是去重修正前结果，不作为当前行为证据。

**必需单位目标缺失（2026-11-17）**：[`20261117T023320-43193b43/manifest.json`](../../../../../../Saved/SpatialExports/20261117T023320-43193b43/manifest.json) 使用 `Skill_Fireball` 和不存在的精确目标对象路径；正式请求返回 `TargetInvalid`，工具状态 `failed`，未回退或自动选择场景对象。

**施法者快速失败（2026-11-17）**：[`20261117T023941-32cc15ba/manifest.json`](../../../../../../Saved/SpatialExports/20261117T023941-32cc15ba/manifest.json) 使用无 Pawn 的玩家索引，初始化后立即写入单条精确失败并退出；约 0.8 秒，无需等墙钟期限。

**独立 Review（2026-11-17）**：[Review.md](Review.md) 的原始 R1—R9 定向复查指出 R2 尚有 Notify 代际过滤缺口。该缺口已按上文补修；原 Reviewer 后续定向复查确认 R1—R9 原实现问题均关闭。本摘要不将该结论扩展为完整需求场景验收通过。

## 字段读取说明

- `manifest.json` 是唯一索引入口：`files` 为相对路径，`missing/errors/limitations` 区分缺失材料、失败和已知限制；`coverage` 的实际覆盖起止/持续范围只由写入样本计算。inspect 的 `not_generated` 明示不会创建的文件/目录。
- `configuration.json`：`caster_runtime` 固定 actor path、runtime component path、generation；`granted_skills` 只列有效授予且 Provider 可解析的定义；`scene_preservation` 记录 TestArena 保护目标状态与 dummy 标签计数。Mesh 的 `package_guid`/`lighting_guid` 是引擎可读版本线索，不是源文件内容 hash。
- `references.json`：`actors[].skeletal_meshes[].bones` 与 `.sockets` 分开枚举；查询 `kind` 分别验证。`parent_reference` 是实际父 bone 或组件对象路径，`local_transform` 相对该父参照，位置厘米、欧拉角顺序 Pitch/Yaw/Roll；空间 `origin` 绑定没有 SkeletalMesh 时相对实际 RootComponent。
- `events.jsonl`：每行一条原始运行事实。Cast 身份用 caster/runtime component + runtime generation + request id + cast id；submission 对象另用完整 submission GUID 与 point id 区分同 Cast 多提交。对象创建/移除事件记录实际 world/wall 时间、创建/移除变换与原因；样本时间不可替代这些事件时间。
- `samples.jsonl`：周期状态快照，`sample_id` 与固定宽度 PNG 文件名一致；请求前 `since_request_*` 为 null，`request_time_relation=before_request`。`views[]` 用 `render_capture_id`、game frame、图像相对路径把状态样本与同步场景 PNG 关联。`feedback_actors`/`projectiles` 只包含本次正式请求来源匹配的对象；截至结束仍存活或弱引用失效由 events cutoff 事件交代。
- 生成对象 StaticMesh 使用 local bounds center/extent、parent component、world transform、visible 状态和 package/lighting GUID。Mend 的 TargetFrame 实际可见组件是四个 `FramePieces`，`ShapeMesh.visible=false`；`Cube` 默认 bounds extent `[50,50,50]` 即边长100cm，piece 运行比例能解释长度/厚度；radius 基准记录为 70cm，visualScale约1.8，最终 configured radius 126cm。UE 当前不能提供这些资源的确定性源文件字节 hash，GUID 是版本提示而非内容哈希保证。

## 未完成验收

- 尚未端到端验证有效单位目标、Fireball 生成后飞行/撞击/独立寿命、BurningBuff 持续效果、RelatedActor 缺失/独立定位、其他角色/技能复用和精确关卡 Actor 定位。地面目标 `AreaEmpty` 只证明请求和工具采集链，不证明有单位受影响。
- 多视角采样以真实 RHI 同步 SceneCapture 完成；不包含 HUD，Game Camera unconstrained 时采用请求输出纵横比。记录骨骼 finalize frame、revision、模拟时间、唯一 capture ID 和读回完成时的 game-thread frame；UE GPU renderer frame number 未导出。仍未用专门移动角色/高速动画夹具核验并行动画边界情形。
- 已实现 Montage/Notify、源分段、姿势哈希、投射物几何/速度和反馈对象来源快照；本次 Mend 观察到的默认反馈已关联实际 Request/Cast/Event，BurningBuff 独立效果跟随与真实投射物仍缺场景证据。StaticMesh 包指纹、材质完整身份及 Marker 逐网格实际尺寸还未导出。
- 已接受目录有 JSON+Lua 内容指纹；独立资源包内容指纹、材质/资源实际加载身份仍缺。配置字段只反映 Provider 的有效定义和当前实际查询到的对象。
- 尚未端到端运行启动超时收束、无 RHI/零图、门控拒绝、引擎异常退出等故障路径。无效 Mesh/socket 与必需目标路径无匹配已有真实运行样本；近墙/遮挡/重叠出生、移动后释放和其他技能姿势变化待后续场景支持。

上述缺项保留在状态中，不能靠既有 RuntimeProbe、旧动作总览图片或 NullRHI 自动化替代。当前实现迭代的剩余验收集中在有效单位目标/投射物链、Buff 独立效果、跨角色复用和异常启动路径；必须获得支持这些场景的现成资产/关卡条件后再运行，不能通过工具自动生成假人、改场景碰撞或使用 Probe 代替正式单请求。
