# 01 技能系统空间配置支持：架构交接

## 任务和范围

本交接服务于 [01 需求](../../../../Planning/CharacterPresentation/Batches/Batch04-OnDemandSpatialAdaptation/01-SkillSpatialConfigurationSupport.md)。交付是按技能对象配置、可由同类新内容复用的正式空间能力，并用 Fireball、AreaBurst、Mend、BurningBuff 接入验证；不得按技能名增加 C++ 专用分支，不改目标合法性、作用范围、伤害/治疗/持续效果、释放时点或碰撞规则。用户另授权四个样本使用显著的临时测试空间值，具体值、恢复方法和运行证据留给实现摘要，不把测试值当正式适配结论。

本文区分已核对事实与实现建议。架构核对为静态阅读，未构建、启动、运行自动化或检查 UE 资产内部；空间能力尚未实施。

## 当前运行链与已核对事实

- 输入最终进入 `USkillLabSkillRuntimeComponent::SubmitSkillRequest`；定义 Provider 将 `Config/SkillDefinitions.json` 和对应 `Config/Skill_*.lua` 声明解析成定义快照。Gameplay Ability/Anim Notify 产生 `SkillLab.Event.Cast` 后，Runtime 以本 Cast 的定义和提交点启动 Lua Behavior。当前四技能 JSON 未显式列 `submissionPoints`，沿用 Script 默认提交点。
- 四个 Lua 定义只声明目标形状、关系、效果、执行模板和规则 profile。`USkillLabLuaSkillBehavior::RunScript` 创建本次调用 VM，只向技能 Lua 暴露 `SkillLabSkill_RequestAction`、`SkillLabSkill_ApplyEffect`、`SkillLabSkill_Finish`；没有 Actor/组件/Socket 查询、空间运算或表现对象 API。脚本执行后即关闭 VM，不支持常驻协程。
- Fireball 正式提交位于 `USkillLabLuaSkillBehavior::ExecuteDeclaredAction`。当前 `GetSkillLabProjectileAimPoint(Source)` 取施法者启用碰撞的根 Primitive bounds 中心；无有效根碰撞时取 Actor bounds 中心，bounds 退化才回退 Actor location。该点传入 Deferred Spawn，`Initialize` 再用当前投射物位置至目标碰撞 bounds 中心/已有 TargetLocation 的向量确定 Velocity。实际出生点目前不是手/Socket，也不是显式前方发射点。
- `ASkillLabFireballProjectile` 根是 Sphere Collision，JSON 的 `projectileCollisionRadius` 直接配置碰撞半径，`projectileVisualScale` 只缩放其子 Visual mesh；两者分离已有基础。Projectile 保留来源、目标、Cast/提交身份，交付确认后才开启碰撞和移动，按命中/阻挡/超时结束；交付后的 Actor 不随 Cast 取消、完成或 Runtime 替换而销毁。需求新增的起点位移可能改变出生时与墙/单位的重叠及最初轨迹，`AlwaysSpawn` 允许重叠出生，碰撞启用可很快触发既有 overlap/hit 结果。必须实测近墙和遮挡；不得用穿墙或改碰撞响应绕过。
- AreaBurst 的正式命中通过 `FSkillLabTargetingService::FindAreaTargets(Runtime, Request.TargetLocation, Definition, ...)` 查询，作用位置/半径由已有请求和定义决定。表现 Router 在 `TargetSelected` 时使用事件 `TargetLocation` 与 `VisualRadius` 生成短时 Ring。表现位置能力须只变更新建的视觉对象，不能改 TargetLocation 或 targeting 查询。
- Mend 与 BurningBuff 当前均是 `InstantTarget`，前者对 Source 应用即时治疗，后者对 Request Target 应用目标持有的周期 GameplayEffect。Runtime/Effect Adapter 发出 Hit、EffectApplied、BuffApplied、DamageTick、BuffRemoved 等真实事件；Buff 移除和周期效果独立于 Cast 动作寿命。`CharacterPresentation.lua` 对上述事件主要记日志；C++ Router 当前根据 Hit/DamageTick/BuffApplied 生成固定位置、固定尺寸、短时占位 Marker/Text，并没有按技能/对象读取空间配置或持续附着。
- Presentation Router 隶属角色表现组件，由角色持有、绑定 Runtime 委托；Fireball 则由 Skills 行为创建并独立持有运行。Lua Presentation 是长生命周期表现策略宿主，Lua Runtime 自身只提供业务无关 VM/facade，不能把空间业务接口加进公共 VM 模块。
- JSON 目前拥有投射速度、碰撞半径、寿命、视觉缩放和穿越非目标单位策略；`FSkillLabSkillDefinition` 快照携带 Lua 声明和 Projectile 参数。JSON Provider 原子替换有效目录，reload 为 `SkillLabSkillDefinitionProvider::ReloadDefinitions`；活跃 Cast 使用定义快照。空间字段应沿同一内容源解析/校验并随新执行快照生效，禁止 Lua/JSON/UE 资产重复维护同一绑定或参数。

## 复用与缺口

| 需求能力 | 可复用事实 | 当前缺口/边界 |
| --- | --- | --- |
| 正式事件/执行点接入 | Runtime 定义快照、Cast token、Gameplay Event 释放及提交点；不需新建 Ability、GE 或 Cue | 对应事件上还没有空间定义求值和通用执行调用 |
| 实时来源与骨骼/Socket 参照 | UE Scene/Primitive/Actor transform 可提供底层查询；当前 Fireball 用根碰撞 bounds 算 aim point | 无角色/目标/地面作用位置统一且受控的来源解析、模型绑定、坐标语义与失败反馈 |
| 偏移/朝向/尺寸/跟随 | Fireball 已将碰撞根与视觉 mesh 分开；FeedbackActor 使用无碰撞显示组件 | 无 local/world/execution-direction 偏移组合、模型朝向校正；现有 Marker 固定尺寸/寿命、无持续跟随 |
| Lua 组合与空间计算 | 技能定义 Lua 已作内容行为绑定；Presentation Lua 收到类型化技能事件回调 | 技能 Lua 宿主只提供正式请求动作接口；Presentation Lua 也无对象空间 API。应在所属业务模块提供窄化、验证后的能力，不能直接暴露任意 Actor/ASC |
| 目标/区域/效果结果 | 已有 targeting、GAS 结算和带完整 Cast/Submission/Effect 身份的事件 | 表现配置消费事件，不可反向选目标、提交效果或制造命中/移除事实 |

## 分层和建议修改落点

以下为满足当前需求的最小落点建议；具体类型名/API/字段和配置组织由实现阶段按局部依赖确定。

- **C++ 稳定机制**：在 Skills 所属的执行路径为真实 Projectile 解析并消费生成空间定义；空间解析/变换若需供执行和表现共同使用，应提供职责窄、输入受控的共享机制，而不是在 Fireball 和 Router 各自实现不同的坐标规则。需要新 C++ 文件时，优先将空间解析与变换拆为独立职责单元；Projectile 仍负责自身运动、碰撞与交付寿命。
- **C++ 表现执行与寿命**：在 Presentation 模块扩展 Router/独立表现对象能力，按真实事件创建或复用视觉对象，应用已求值变换、管理附着/跟随和结束；若 Router 会同时承担空间解析、配置选择、创建、逐帧跟随与清理，应拆出具有明确所有权的表现对象管理职责。反馈 Actor 当前约 145 行，Router 约 107 行，预计职责边界允许局部扩展；实施后若明显超过约 400 行参考值或扩大首轮阅读范围，按架构 Skill 补拆分判断。不要把纯表现对象生命周期绑到施法 Ability，也不要让 Router 裁定战斗结果。
- **Lua 组合层**：在 `SkillLabSkills` 现有受限宿主及 `SkillLabPresentation` 事件宿主分别开放业务所需的类型化空间语义/计算入口。技能 Lua 决定既有事件上使用何对象定义、来源、偏移坐标语义和方向/跟随关系；表现 Lua 只组合表现行为。它们不能直接操作任意 Actor、ASC、GAS 或改写正式请求/提交。持续跟随由 C++/UE 按需维护，不添加常驻/逐帧 Lua 协程。
- **JSON 参数/模型绑定**：沿 `Config/SkillDefinitions.json` 作为现有技能高频参数和软资源/动画绑定入口扩展，并在 `SkillLabSkillEntryParsing.cpp` 做结构化读取、范围/组合校验和明确错误。Socket/bone 等模型绑定属于 JSON 内容，不写成通用 C++ 角色名分支；Lua 与 JSON 不重复写同一个数值。若表现对象配置不能自然归属技能定义，先判断当前 Provider/快照是否能表达其按技能对象的绑定，再定最小结构，不为此重建加载系统。
- **运行时快照边界**：解析后的空间配置和 Lua 源应随定义原子提交并进入 Cast/已交付 Actor 所需快照；新执行读新定义，已交付 Projectile 按其捕获的参数运行，不能中途混读 Provider 新值。事件表现按实际来源 token/submission/effect identity 隔离；一个 Cast/效果的终态、清理不能误移除另一来源的对象。
- **错误与结果边界**：未配置时保持当前表现/投射行为；显式无效参照、绑定、方向或资源应带技能/对象/原因可诊断，不静默退化到 (0,0,0)。纯表现创建/跟随失败不改变技能事实。真实 Projectile 生成失败沿现有提交结果处理，不新增自动成功、补发、退款或碰撞规则；如果实现时发现既有失败结算语义无法满足已确认要求，再提交具体行为缺口。

## 生命周期与身份核对

- Cast 身份为 Runtime + generation + CastId；提交有独立 Production Handle/SubmissionKey。表现事件来源于技能运行结果，不能仅用数字 CastId 或技能名作跨对象清理键。
- 已确认 Projectile 是独立交付对象：Cast 完结/取消不结束飞行，Projectile 自己在命中、阻挡、超时或外部销毁时收束；来源被销毁时不能应用迟到效果。新生成点应只在释放事件当前来源的正式提交中求值一次。
- 持续效果由目标 ASC 持有，其周期及移除事件独立于 Cast；Runtime Reset 按现有语义清效果，Runtime 销毁则解绑观察且不会伪造 BuffRemoved。BurningBuff 视觉附着必须跟随真实效果实例寿命，听从 BuffRemoved/目标失效清理；Cast Completed 不能提前结束。
- Presentation Router 当前由角色组件持有；若其创建的表现对象需要跨施法者或技能动作寿命继续存在，须给对象明确 Owner/Handle 和移除条件。施法者/目标/绑定组件失效时停止该对象的无效跟随并清理视觉对象，但不得据此撤销已经生效的效果。世界销毁/EndPlay 应解绑委托、清掉追踪表与可销毁表现对象。

## 实现首轮必读与验证入口

首轮上下文建议限制为以下文件与指定事实；其他模块按直接依赖补读：

- 需求及基线：[01 需求](../../../../Planning/CharacterPresentation/Batches/Batch04-OnDemandSpatialAdaptation/01-SkillSpatialConfigurationSupport.md)、[角色表现基线](../../../../Baseline/CharacterPresentation/README.md)、[表现事件边界](../../../../Baseline/CharacterPresentation/02-SkillActionPresentation.md)、[效果寿命与清理](../../../../Baseline/SkillSystem/05-OwnershipAndCleanup.md)。
- 项目规则：[程序分层约定](../../../../ProjectContext/ProgramArchitectureContract.md)、[Skills 模块](../../../../ProjectContext/Modules/SkillLabSkillsRuntime.md)、[Presentation 模块](../../../../ProjectContext/Modules/CharacterPresentation.md)、[Lua Runtime 模块](../../../../ProjectContext/Modules/SkillLabLuaRuntime.md)。
- 技能内容/执行：[SkillLabSkillTypes.h](../../../../../Source/SkillLabSkills/Public/SkillLabSkillTypes.h)（`FSkillLabSkillDefinition`、请求/事件结果和 Projectile 参数）、[SkillLabSkillEntryParsing.cpp](../../../../../Source/SkillLabSkills/Private/SkillLabSkillEntryParsing.cpp)、[SkillLabSkillDefinitionProvider.cpp](../../../../../Source/SkillLabSkills/Private/SkillLabSkillDefinitionProvider.cpp)、[SkillLabSkillBehaviorLua.cpp](../../../../../Source/SkillLabSkills/Private/SkillLabSkillBehaviorLua.cpp)、[SkillLabSkillBehaviorProduction.cpp](../../../../../Source/SkillLabSkills/Private/SkillLabSkillBehaviorProduction.cpp)、[SkillLabProjectileTypes.cpp](../../../../../Source/SkillLabSkills/Private/SkillLabProjectileTypes.cpp)、[SkillLabFireballProjectile.cpp](../../../../../Source/SkillLabSkills/Private/SkillLabFireballProjectile.cpp)、四份 `Config/Skill_*.lua` 与 `Config/SkillDefinitions.json`。
- 表现/效果事实：[SkillLabSkillPresentationRouter.cpp](../../../../../Source/SkillLabPresentation/Private/SkillLabSkillPresentationRouter.cpp)、[SkillLabSkillFeedbackActor.cpp](../../../../../Source/SkillLabPresentation/Private/SkillLabSkillFeedbackActor.cpp)、[SkillLabCharacterPresentationLuaComponent.cpp](../../../../../Source/SkillLabPresentation/Private/SkillLabCharacterPresentationLuaComponent.cpp)、`Config/CharacterPresentation.lua`、`SkillLabSkillEffectEventAdapter.cpp`。
- 直接验证线索：[SkillLabSubmittedActionLifetimeTests.cpp](../../../../../Source/SkillLabSkills/Private/Tests/SkillLabSubmittedActionLifetimeTests.cpp)（交付后独立寿命、墙体阻挡、周期 GE 移除/Reset），[SkillLabDefinitionProviderTests.cpp](../../../../../Source/SkillLabSkills/Private/Tests/SkillLabDefinitionProviderTests.cpp)（JSON/定义解析），Presentation 的 `SkillLab.Presentation.SubmissionIdentity` 与 `SkillLab.Presentation.CastAnimationResources` 自动化。

优先扩展这些既有自动化入口，覆盖明显临时参数确实进入真实生成/表现路径，JSON 修改隔离、局部/世界/执行方向语义、释放时点实时取 Socket、外观缩放不改碰撞、地面视觉不改范围、持续附着/移除/无效来源清理、多 Cast 身份隔离，以及 Fireball 空旷/近墙/障碍出生结果。注册入口见 [TestAndVerification](../../../../ProjectContext/Modules/TestAndVerification.md)。运行/编译时按该文档要求使用实际工程目标；真实可见效果需编辑器 PIE/游戏视口核对，NullRHI/单元断言不能替代视觉结论。本文没有执行这些验证。

定义当前通过启动加载；修改后可用 `SkillReloadDefinitions` 验证有效目录 reload，失败应保留上一版定义并报告原因。验证记录需说明工程/DLL 来源、加载配置值和适用的实际运行观察；本交接不指定最终空间参数或正式美术通过标准。

## 未决事项

需求已确定坐标语义、位置/方向分离、视觉尺寸边界、持续跟随关系、失败边界以及 Lua/JSON/C++ 职责，因此这些不是待用户决定项。Socket 名称/测试偏移和临时显著值属于实现验证配置；由实现者按可见模型事实选择并在摘要记录。若空间值无法从已有目标/地面请求、角色/目标组件或可验证模型参照解析，应带着核对证据回交；不得用猜测默认位置伪装成功。架构核对未发现需要先改变目标、伤害、碰撞或释放规则的已知阻塞。
