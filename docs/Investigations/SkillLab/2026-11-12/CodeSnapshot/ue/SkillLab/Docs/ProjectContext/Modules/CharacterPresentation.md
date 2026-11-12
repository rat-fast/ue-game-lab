# SkillLabPresentation 角色与技能表现

## Module Responsibility

`SkillLabPresentation` 采集角色动画状态、编排 Lua 表现意图、适配 AnimBP、播放技能动作并提供通用占位反馈。它消费技能事实，不拥有技能规则、效果结算或资源状态。

## Code And Assets

源码位于 `Source/SkillLabPresentation/Public/`、`Private/`，通过 Build.cs 和 `SkillLab.uproject` 注册为 Runtime 模块。`SkillLab` 依赖它组装角色/地面选点反馈；它依赖 `SkillLabSkills` 读取事件与定义，Skills 不反向依赖表现模块。Lua facade 来自私有依赖 `SkillLabLuaRuntime`，第三方 Lua 源码不归属表现模块。

- `USkillLabCharacterAnimationStateComponent` 采集 Idle、Move、Jump、Fall、Land 快照与迁移。
- `USkillLabCharacterPresentationLuaComponent` 加载 `Config/CharacterPresentation.lua`，持有并绑定 `USkillLabSkillPresentationRouter`，将状态/技能事件交给 Lua，通过类型化回调执行表现命令。
- `USkillLabCharacterAnimInstance` 缓存 Lua locomotion 意图，并将状态字段写入 `ABP_Unarmed` 既有输入；`USkillLabAnimInstance` 是直接采样 MovementComponent 的独立路径。
- `USkillLabSkillAnimationComponent` 用 `FSkillLabCastAnimationPlayback` 列表记录完整 token、Montage、NotifyAnimation、弱 AnimInstance 与实例 id；按完整身份播放、停止和转发 Notify。
- Router 与 `ASkillLabSkillFeedbackActor` 提供屏幕文字、范围环、目标框、数值占位反馈；FeedbackActor 也用于地面选点预览。`DefaultEngine.ini` 的 CoreRedirects 保留迁移前反射路径兼容。
- `Config/SkillDefinitions.json` 当前四技能以 `castMontage` 绑定 `/Game/SkillLab/Animations/AM_Skill_*`；Provider 还支持兼容 `castAnimation`。Lua 不复制数值或动作资源路径。`AnimationDefinitions.json` 是独立原生 AnimInstance 的基础调参入口。

## Runtime Entry

`ASkillLabCharacter` 创建状态、Lua 表现与技能动画组件；Lua 表现组件 BeginPlay 创建 Router 并订阅 Runtime `OnNativeSkillEvent`，EndPlay 解绑。动画组件不自行订阅技能事件。`Run_SkillLab_DX11.bat` 启动 `-game` 游戏窗口，编辑器 Play 才是 PIE；命令见 [测试模块](TestAndVerification.md)。

## Runtime Flow

状态组件观察 CharacterMovement 的真实速度、方向、加速度和 Falling/落地窗口，产生五态快照；Lua 映射为 locomotion intent，`USkillLabCharacterAnimInstance` 同步意图与快照到 `ABP_Unarmed`，UE 执行姿势求值与融合。

技能事件经 Router 进入 Lua：Started 请求动作，Cancelled/Completed 停止对应动作；结果类事件和持续效果当前主要记录日志，通用占位反馈仍由 C++ Router 执行。持续效果结果与 Cast 动作终态各自路由，不混为一个生命周期。

播放先以事件的完整 token 调用 Runtime `FindExecution` 校验，并从该执行的定义快照读取软路径。`Montage_Play` 使用不停止全部 Montage 的参数；播放记录按 token 保存，停止只定位匹配记录和保存的 Montage 实例，不以数字新旧顺序清理其他 Cast。

Notify 转发同时核对完整 token、实际 AnimInstance、MontageInstanceID、Montage/Sequence、播放状态及 NotifyMesh，再调用 Runtime 的 validated 入口。旧实例、其他 Runtime 或 generation 的同数字 CastId 不能操作当前执行。

## Current Behavior

Lua 回调仍可接收数字 castId，但 C++ 以 `CallbackCastToken` 和 `TGuardValue` 将 play/stop 绑定到当前事件的完整身份，不能借相同数字切换 Runtime；原生回退也使用完整 token。`submissionId` 透传已接受的完整提交 Id，不重新从当前 Cast 推断来源。

Lua 脚本或回调不可用时，C++ 提供基础 locomotion 与 Started/终态动作回退；资源、Mesh、AnimInstance 或 Montage 失败则记警告、跳过动作，保留技能运行。当前 Lua 对 Hit、Rejected、PhaseChanged、TargetSelected、EffectApplied、BuffApplied、DamageTick、BuffRemoved 等主要记日志，没有正式差异化 VFX 编排。

多 Cast 的身份隔离不保证资源可同时播放：当前四个生产 Montage 都在 `DefaultGroup/DefaultSlot`，仍受 UE 同组播放互斥约束。尚无兼容双 group 资源与可见双 Montage 验收证据。

## Current Responsibilities And Evolution Boundaries

C++ 拥有状态事实、来源身份、对象安全与动画执行；Lua 组合表现意图；AnimBP 拥有姿势求值。基础策略优先查 `CharacterPresentation.lua`，状态事实查状态组件，动作绑定查技能 JSON/动画资产，token 与实例校验查动画组件。新增表现语义可重新调整桥接；发现旧终态影响新动作、身份在 Lua/C++ 间丢失或输入重复维护时，应复核分工。

## Verification

`SkillLab.Presentation.SubmissionIdentity`、`SkillLab.Presentation.CastAnimationResources`、`SkillLab.Animation.DefinitionProvider` 为当前自动化入口。分版本运行、历史 Probe 与资源检查的范围统一见 [TestAndVerification](TestAndVerification.md)。本次仅同步文档，没有运行 smoke、Probe、测试或可见验收。

## Current Limitations

NullRHI 与身份测试不能证明可见姿势或同组动作并行。仍缺完整 Jump→Fall→Land 可见链、取消/完成后姿势恢复、缺资源/AnimInstance 降级的玩家可读性，以及生产多 Cast 视觉体验的验收材料。旧日志中的入场 Fall→Land 不能替代跳跃链，AnimBP 消费状态接口的日志也不能证明 Lua intent 最终造成可见姿势变化。CoreRedirects 不等于旧资产本轮已打开核对。

## Extension Directions

正式动作、双 Montage 或 VFX 的扩展应先确认资源与体验范围，再用可见运行核对，保持技能来源与播放实例可追溯。
