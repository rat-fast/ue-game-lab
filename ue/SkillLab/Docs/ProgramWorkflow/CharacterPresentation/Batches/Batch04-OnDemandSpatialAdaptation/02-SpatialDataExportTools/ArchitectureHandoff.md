# Batch04-02 架构交接：实际技能释放采集与导出

## 依据、状态与结论

依据：[当前需求](../../../../../Planning/CharacterPresentation/Batches/Batch04-OnDemandSpatialAdaptation/02-SpatialDataExportTools.md)、[批次边界](../../../../../Planning/CharacterPresentation/Batches/Batch04-OnDemandSpatialAdaptation/README.md)、[表现共享基线](../../../../../Baseline/CharacterPresentation/README.md)及正文 01—03、SkillSystem 的目标/执行与独立寿命语义、`ProgramArchitectureContract.md`。2026-11-17 核对本地真实源码及 UE 5.8 头文件；本交接没有构建、启动 UE 或运行测试。

需求首页仍是“需求讨论稿，供用户审阅”；本轮用户已明确授权完整执行，授权原文与来源保存在本目录 README。本交接采用需求正文全部范围，不改需求状态，不将架构完成解释为工具完成。

初步拆分成立：**UE 拥有实际世界查询、正式请求、事件/对象关联、姿势和空间采样、真实渲染；Python 拥有 CLI、参数补齐、启动/墙钟超时/进程收束、请求与结果文件的编排及最终校验**。单靠 Python 或既有 Probe 无法完成需求，需改项目 C++。建议在 `SkillLab` 增加只在导出启动参数存在时启用的独立采集宿主，沿现有 Skills/Presentation 增补窄观察接口；不更换 GameMode/Pawn，不建立技能预览世界或第二套空间解释器。

以下路径除链接外均相对 `ue/SkillLab/`；新增文件名是建议，具体内部类名与序列化字段由实现确定。外部命令、请求字段、文件角色、状态与退出码遵循需求，不重新命名。

## 已确认的运行链与复用缺口

| 环节 | 真实源码事实与复用入口 | 本次缺口/接入 |
| --- | --- | --- |
| 启动与组装 | `Run_SkillLab_DX11.bat` 已用 UE 5.8 `UnrealEditor.exe <project> -game -windowed -d3d11`；`SkillLabCharacter.cpp` 组装 Runtime/表现/动作/相机；Runtime `BeginPlay -> InitializeSkills` | 独立工具请求启动参数、阶段状态及退出；等待原 Pawn 与 Runtime `IsInitialized()`，再完成 warmup，不重新授予或重置 |
| 精准定位 | 玩家可从指定 World 的 PlayerController/Pawn 获取；Actor/组件可读取完整路径 | 对当前关卡 World 内完整路径严格匹配，零/多匹配均失败并返回候选；不使用 label/Contains/第一个 Actor。Caster 下多个 Runtime 不能静默挑第一个。inspect 的候选限定本场景，不扫描全项目 |
| 有效定义 | `Runtime::GetGrantedSkillIds` 取角色授予表；Provider `Resolve` 返回已接受目录；`USkillLabCastExecution::ActiveDefinition/ActiveRequest` 保留实际 Cast 快照 | inspect 先交集授予表与有效定义；capture 在 Started 回调立即复制实际 Cast 定义，不能等调用返回后再找可能已终止的 Cast。Provider 尚无目录来源指纹/版本导出接口 |
| 正式请求 | `SkillLabSkillRuntimeRequests.cpp::SubmitSkillRequest` 包含授予、门控、目标、仲裁；`FSkillLabSkillRequest` 已支持 `RelatedActor` | 单次构造 SkillId/Caster/显式 Target 或 bHasLocation/RelatedActor，并只提交一次；Self 留给既有 `ValidateRequest` 语义。返回拒绝据实失败，不自动找敌人、重试、补资源或直接调用 Executor |
| 技能事实 | `OnNativeSkillEvent` 含完整 CastToken、RequestId、SubmissionKey、EffectInstanceId；`ReleaseCast` 在 TryMarkRelease 后发 PhaseChanged，再发 TargetSelected | 没有单独 Released 枚举，不能把任意 Active 阶段当释放；以 `bHasReleased` 首次变化识别，并保留原事件。提交前订阅，以免丢同步 Started/终态 |
| 真实投射物 | **生成在 `SkillLabSkillBehaviorProduction.cpp::ExecuteDeclaredAction`**，不是 EffectExecutor；Deferred Spawn -> Initialize -> FinishSpawning -> ConfirmDelivery；Projectile 保留原来源，交付后可独立于 Cast 生存 | 当前结果未带生成 Actor/实际生成变换；需原生只读观察通知，把完整来源与 Actor、实际变换/方向关联。不能扫描附近 Fireball 猜归属，也不能抢占业务用的单播 `OnResolved` |
| 空间表现 | `SkillLabSkillPresentationRouter.cpp::ShowSpatial` 用事件快照和 `FSkillLabSpatialTransform::Resolve` 生成无碰撞 Marker；`SkillLabSkillFeedbackActor.cpp` 跟随/清理，Router 私有持有持续对象 | Router/反馈对象尚无对外来源观察。创建完成后发窄通知，带 Cast/事件/效果/对象声明名与 Actor；默认反馈和文本也要保留来源，不能只覆盖显式 spatial 项 |
| 模型与动作 | `SkillLabSkillAnimationComponent.cpp` 私有 Playbacks 保存完整 token、实际 Montage、NotifyAnimation、AnimInstance、InstanceId；`FindMesh()` 实际取角色首个 SkeletalMeshComponent | 提供只读播放记录及播放/停止/Notify 观察，不改变现有选 Mesh 规则。导出实际所选组件与同角色其他相关模型；采样实例位置/Section/速率/权重及 Montage 段源动画/混合线索，不把配置软路径当已播放事实 |
| 图像 | `SkillLabAnimationEditor/Private/SkillLabActionOverview.cpp` 已使用 SceneCapture2D、RenderTarget、ReadPixels、PNGCompressImageArray；Python 总览工具检查真实 RHI 与 PNG | 旧工具用独立 PreviewScene/AnimSequence/正交相机，不能复用业务宿主。可参考其像素与保存原语，本次在真实运行 World 透视采集；Runtime 模块不得反向依赖 Editor 模块 |

**场景副作用必须处理。** `SkillLabPlayerController::BeginPlay -> TestCommands::InitializeForPawn -> TestArena::PrepareForSkillTesting` 默认会生成训练假人，并在 `ConfigureSkillTestArena` 隐藏/关闭 `NavTest_Block_A/B` 碰撞。最小落点是在工具模式下跳过这一测试场准备调用，仍保留原输入/角色/表现初始化，不恢复障碍后再采样。不得启动 `-SkillLabRuntimeProbe`：它另有自动选敌、移目标、改移动、四技能连续请求和退出，违反本次单请求与保留场景条件。需要单位目标的验收场景应预先放置可精确定位的有效技能目标，工具不代建 NPC 系统。

## 最小落点与职责

### UE 采集宿主

- 建议 `Source/SkillLab/Public/SkillLabSpatialExportSubsystem.h` 与 `Private/SpatialExport/` 的 Session、Queries、Serialization、Sampling、Capture 实现单元。可用 `UTickableWorldSubsystem` 或等价 World 生命周期宿主，只接管带本次请求路径参数的 Game World，非工具启动不执行。优先复用已有 `SkillLab` 模块，无须预建新模块/插件。
- 宿主拥有会话状态、弱对象集合、订阅句柄、采样计划、渲染资源与结果写入器。正常结束先停止采样/解绑，再完成帧写入与索引，最后退出自身进程；World EndPlay 释放资源。对象 EndPlay 与进程收束不同，结束采集前仍存活记为“截至结束尚存活”，退出销毁不伪装成技能自然移除。
- `SkillLab.Build.cs` 按实际编译需要私有增加 RenderCore/RHI/ImageWrapper 等依赖；不得给 Skills 引入文件导出、Python、RHI 或 Presentation 依赖。`TestCommands::InitializeForPawn` 只增加工具模式下测试场准备的最小隔离。
- 查询用现有 UE 模型/骨架/Socket API，周期采样不逐个遍历全骨骼；默认采样实际模型、请求 Actor、有效空间配置绑定参照、所生对象，加显式 reference_queries。骨骼和 Socket 按 kind 分别验证，不能让 `DoesSocketExist` 同时接受 bone 的行为掩盖类型错误；记录 Socket 所属骨骼、局部父参照及世界变换。
- 骨骼/Socket 配置语义仍用 `FSkillLabSpatialTransform`。其 `Resolve` 只返回最终变换，未返回实际匹配组件/原始参照；可增只读解析结果，供执行与导出共用，或在实际执行处通知已解析值。禁止 Python 再写空间求值，也禁止导出器复制一个“首组件/Socket”解析版本。
- StaticMesh 从实际运行组件取资源、本地 bounds、组件到 Actor/世界变换、比例与轴向。Projectile 分别记录 Movement velocity/执行方向、碰撞 Sphere 的实际尺寸/变换/碰撞设置、Visual 子组件朝向/比例；Marker 记录实际 ShapeMesh/FramePieces 与尺寸基准，`VisualRadius` 缺省 70cm 和形状缩放会影响最终尺寸，不能只导出 visualScale。

### 窄观察与来源快照

- Skills 中增补原生只读生成/交付观察的最小类型和通知，放在其公开窄头文件、独立实现单元；采集宿主订阅，业务模块不认识导出会话。生成观察应在初始化完成且尚未开放碰撞时捕获；`ConfirmDelivery()` 当前会直接激活碰撞，可能同步 overlap 自毁，**在它返回之后才观察会丢生成对象**。分别记录已创建、已交付和实际影响，不把 Deferred Actor 出现当提交成功。
- Presentation 的 Router 与动作组件提供同类只读观察/快照；Actor 只保留弱引用，事件内立即复制需要持久化的值。观察不得改业务结果、占用单播业务回调或追加同步 IO 到权威提交链。采集宿主可先内存排队，再顺序刷 JSONL。
- `FSkillLabSkillDefinition` 已保留 `LuaScript/LuaSource`，脚本运行从快照取文本，不重读磁盘。Provider 当前仅全局 Definitions 与 bLoadAttempted，没有 JSON 来源版本。建议在成功解析/原子提交时一并保存 JSON 与对应 Lua 内容指纹/来源；失败 reload 仍保留旧来源，与实际目录一致。从规范化有效定义另算快照指纹，连同 Cast 定义保存；事后重读磁盘 hash 只能标为当前磁盘，不能冒充加载快照。
- 资源身份记录实际 UObject 资产路径、类、包身份及可取得的包文件/内容指纹。动态 Montage 记录 transient 身份和原 Sequence/Slot 来源，不能声称存在同名磁盘资产。构建/引擎版本、项目路径、配置指纹与资源身份一起进入 configuration；不可取得的来源明确标缺失，不仅写日期。

### Python 编排

- `tools/export_skill_spatial.py` 提供需求三命令。较多实现放相邻独立模块，分别处理请求/设置、进程与结果校验；不把旧 `skilllab_action_overview.py` 的分页/发布逻辑移成依赖。默认工程按脚本所在工作区定位 SkillLab；引擎可复用已确认的 UE 5.8 固定入口，并校验存在/工程关联；显式覆盖优先，失败不搜索任意引擎。
- `configure` 直接写所选工程 `Saved/SpatialExportTool/settings.json`，不启动 UE，不改技能 JSON。其他工程也可保存设置；inspect/capture 仍需该工程具备本协议宿主，未接入时明确失败，不暗中退回 SkillLab。
- 输入预检绝对路径、有限数值/互斥定位字段、时长/频率/视角/分辨率、目录可写和已知时间预算；运行后再验资产存在、初始化、目标与参照。以参数列表启动进程，不拼 shell 命令；为本次会话提供请求/输出路径和独立日志。`timeout_seconds` 是从启动开始的墙钟期限，不能只给“施法后等待”计时。
- Python 建唯一 run_id、原始与补齐请求、初始失败可写索引，UE 增量写实际材料与阶段；建议 UE 将完成回执写临时文件再原子替换，Python 等 UE 退出后校验并最终发布 manifest，避免两个进程并写最终状态。超时先尝试协作结束，随后只收束本次启动进程及其子进程；保留已有材料，带最后阶段，不按映像名杀所有 UnrealEditor。
- 结果校验覆盖 PNG 能解码/尺寸/对应样本、文件索引、实际范围、UE 异常退出与缺失项。末尾 stdout 恰为需求机器 JSON，日志去 stderr/文件；状态由语义和材料共同决定，不仅看 UE exit code。接受请求且部分材料/查询/视角缺失为 partial；请求拒绝、关键链断裂或零可用画面为 failed；不得把正常采样频率不足但已记录真实覆盖直接误称严格定频成功。

## 数据与调用边界

共享协议先定最小版本与阶段回执，随后 UE 与 Python 可分工实施；最终端到端验证串行接入。两侧是同一采集单元的两个实现面，故本次共用这份分节交接；UE 侧重点读“运行链、宿主、观察、同步”，Python 侧重点读“编排、数据边界、验证”。PM 可按实际工作量拆派，不必再造总控框架。

1. Python -> UE：已补齐请求、mode、run_id、输出目录、协议版本。保持需求文件名，额外内部回执放 Saved/工具会话目录，不能替代 manifest。UE 再校验运行输入，不把参数 JSON 直接反序列化成任意 UObject 操作。
2. UE -> 材料：configuration/references、原始事件 JSONL、周期样本 JSONL、指定视角 PNG、诊断和阶段回执。Python负责文件编排不等于必须跨进程传全部像素/事件；UE 可以直接流式保存，Python做关闭后的校验和最终状态。
3. 身份：用 `run_id + 本次对象ID` 标 Actor/组件/Runtime，Cast 由 `runtime_id + generation + CastId` 组成，保留 RequestId、完整 SubmissionKey 的可序列化字段和 EffectInstanceId；事件/样本各有唯一序号。UObject 指针/FObjectKey 数字和显示名均不能当跨运行身份。
4. 绑定：Started 捕获定义，事件按本次 RequestId/完整 token 过滤；Cast 结束后保留该 token 所生投射物与效果的观察。样本可引用源对象/定义/事件 ID；生成瞬间独立事件内存快照不能被下一周期位置覆盖。记录实际请求返回和事件时间，Notify 记录转发是否通过及动画实例，不能作 Hit 或 success。
5. 约定：所有世界位置厘米，旋转显式 Pitch/Yaw/Roll 或 quaternion XYZW，局部变换标父组件/骨骼；同时记录世界模拟时间、相对**请求提交**时间与墙钟阶段时间。每个图像关联 sample_id、view_id、相机投影参数及实际渲染身份；采样结束列出对象仍存活/尚未观察到的事件。parent_run_id 仅关联独立运行。
6. inspect 仍需完整定位 caster，可省 skill_id，输出当前授予技能/绑定及相关组件骨骼/Socket 名称；不提交、不创建 events/samples/frames。capture 即使事件为空也创建空 events.jsonl。manifest 的字段与三态、退出码 0/2/1 完全按需求，不能沿用旧总览工具的 framesReady/incomplete 外部状态。

## 同步采样与真实 PNG

首选真实 `-game` + DX11 自动窗口，示意参数为 `<project> <map> -game -windowed -d3d11 -ResX=... -ResY=... -SkillLabSpatialExportRequest=<绝对路径>`；这是待实现启动参数，不是现有接口。不使用 commandlet 预览循环代替真实 World 主循环，也不手动递增 GFrameCounter、播放独立 Sequence 或强推骨骼模拟成功。离屏 `-RenderOffscreen` 可在实现期试验，不能阻塞可见窗口方案交付，未验证则明确不承诺。

- UE 5.8 已有 `USkeletalMeshComponent::RegisterOnBoneTransformsFinalizedDelegate`、骨骼变换 revision，World `OnWorldPostActorTick/OnWorldTickEnd`，SceneCapture2D `CaptureScene` 与 PlayerCameraManager `GetCameraCacheView`。这些是候选原语，不是“已证明严格同步”。需在动画并行求值完成、实际组件骨骼已更新且场景变换送往渲染后采集，记录姿势 revision/求值帧、采样帧、模拟时间和实际 readback 帧/滞后；若可见性/节流导致姿势未更新，明确标记，不冒称当前姿势。
- 每次周期先冻结同一状态的 Actor/查询值与各相机参数，在同一游戏线程采样阶段对所有 view 渲染/回读，中间不推进游戏 tick；同步 flush/readback 可作为首版正确性方案，成本导致的实际间隔与漏采照实记录，不能补写虚假等距帧。必要时使用渲染 fence 保障完成；具体放在哪个回调由首个端到端样本核验。
- game 视角从真实游戏相机状态取位置/旋转/FOV/宽高比/后处理；custom 用请求透视参数。复用 SceneCapture 必须说明 game 是游戏相机所见场景，保留相机约束/投影差异，不能把不同 FOV 的另一相机标成游戏画面；无可用游戏相机则该视角明确失败。HUD 是否包含记录清楚，需求核心是实际场景。不能为改画面隐藏障碍或修改碰撞。
- markers 推荐在已回读的干净 PNG 上按同一投影矩阵叠加查询参照名称/三轴，保留原图；无需在真实世界增加会改变渲染的测试 Actor。若用额外渲染通道，也不得推进世界；标明覆盖/遮挡策略。
- NullRHI 仅可验证数据/身份/失败路径；**capture 在无真实 RHI 或零 PNG 时必须 failed**。已有 RuntimeProbe 的 COMPLETE、自动化 Success、旧总览 PNG 均不能当本工具画面证据。

## 实现必读与上下文控制

已读入口和公共约定沿 README 引用，不要求下游重读整个地图/历史批次。以下是首轮直接上下文；相关模块文档仅补其对应章节。

| 实现面 | 首轮文件/符号与用途 |
| --- | --- |
| UE 宿主/查询 | `Source/SkillLab/SkillLabCharacter.cpp` 组装；`SkillLabPlayerController.cpp::BeginPlay`、`Private/SkillLabTestCommandsComponent.cpp::InitializeForPawn`、`SkillLabTestArenaSubsystem.cpp::ConfigureSkillTestArena` 的测试副作用；Skills Public 的 Runtime/Types/CastExecution/SpatialTransform；Provider Resolve/Reload 与 EntryParsing 的来源保留 |
| UE 观察/样本 | `SkillLabSkills/Private/SkillLabSkillRuntimeRequests.cpp` 的请求至 Started；`SkillLabSkillRuntimeExecution.cpp` 的 MakeResult、ReleaseCast 与终态相关函数；`SkillLabSkillBehaviorProduction.cpp::ExecuteDeclaredAction`、`SkillLabFireballProjectile.cpp::Initialize/ConfirmDelivery/ResolveImpact/EndPlay`；Presentation 的 SkillAnimationComponent、SkillPresentationRouter、SkillFeedbackActor、CharacterPresentationLuaComponent 的绑定/ShowSpatial 桥 |
| PNG | `SkillLabAnimationEditor/Private/SkillLabActionOverview.cpp` 仅 CaptureScene/ReadPixels/PNG 与骨骼 revision 处理作参考；UE 5.8 对应 World、SkinnedMesh/SkeletalMeshComponent、SceneCapture2D、PlayerCameraManager 头文件；实际新增采集宿主 |
| Python | 需求外部协议、本交接编排/数据/验证；`tools/skilllab_action_overview.py` 的 main 启动/超时及 PNG 校验作为参考；`Run_SkillLab_DX11.bat`、uproject 与新增宿主请求/回执协议 |

`SkillLabSkillRuntimeExecution.cpp` 已559行，`tools/skilllab_action_overview.py` 已535行，不能继续堆入导出逻辑；前者只补必要窄观察接点，较大的新观察实现独立文件，后者不扩写。Runtime Component.cpp 当前366行，Requests333行、表现 Router164行、Animation132行、Feedback187行，保留其业务职责，只新增短观察/快照入口。新工具按会话生命周期、查询/快照、采样、渲染/编码与序列化分单元，约400行是首轮参考，不为行数制造 UObject 或复制会话状态。共同身份、订阅与资源所有权只归一个会话，跨文件借用，不各自定时推进。

## 验证入口、证据与实现期核对

先构建实际 `SkillLabEditor Win64 Development`（`D:/Epic/UE_5.8/Engine/Build/BatchFiles/Build.bat`），保证实际工程 DLL 更新。定向回归复用 `SkillLab.Skills.DefinitionValidation`、`SkillLab.Skills.SpatialTransform`、`SkillLab.Skills.SubmittedActionLifetime`、`SkillLab.Presentation.SpatialLifecycle`、`SkillLab.Presentation.SubmissionIdentity`、`SkillLab.Presentation.CastAnimationResources`；只按实际受影响范围选取，不宣称全套通过。命令与历史证据路由见 `Docs/ProjectContext/Modules/TestAndVerification.md`；NullRHI 测试只支持机制事实。

端到端从需求三条 Python 命令开始：默认/临时输出与不可写目录；inspect 的原场景玩家、关卡 Actor、有效技能/组件参照且无施法；capture 的自疗、显式单位目标、地面目标；另一个支持角色或内容不增加技能名分支；显式 related_actor 原生请求已可支持，须验证与 target 独立及缺失不回退。需预置有效目标的场景由验证夹具/调用者准备，不能让正式工具偷偷寻找敌人。

PNG 证据至少回读首/中/末及动作变化帧，对照 Montage 实例、骨骼 revision、样本时间、实际生成瞬间，包含 game/custom 同状态多视角和 markers 开关；检查真实飞行方向与子网格朝向/碰撞可区分，Buff 与投射物在 Cast 终态后仍被观察，时长结束存活状态真实。异常覆盖错误 Actor/组件/bone/socket、缺目标、门控拒绝、初始化未就绪、NullRHI/零图像、漏帧/缺视角、引擎退出及墙钟超时，只收束本次进程；仅凭 stdout 的 manifest 路径让独立读取者定位全部材料。

实现期需取得证据：实际生产 Socket 的唯一匹配、移动后真实释放采样、Fireball 近墙/遮挡/重叠出生，及姿势到 PNG 的对应/延迟。需求未提供自动移动轨迹参数，移动场景可以用专门验证夹具驱动正常场景移动，不能给工具加入默认 Probe 轨迹或把这些风险从验收中删掉。新验证材料存 Saved，长期说明仅留复现入口/结果/限制。

当前没有需要用户再决定的范围问题。真实游戏 World 的多视角同步、离屏可用性、生产资产/场景目标可用性、运行来源指纹和提前生成观察的接入时机属于实现期核对；发现交接与源码不同可修正技术方案，但不得把尚未完成的 PNG、动画对应或通用角色支持转述为已完成或默认排除项。
