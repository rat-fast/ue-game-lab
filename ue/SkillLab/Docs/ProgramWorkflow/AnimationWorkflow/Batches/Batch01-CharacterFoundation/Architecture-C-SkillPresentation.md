# C：技能表现运行接入与组合层

## 目标与分层

依据策划需求 `Infrastructure/03-SkillPresentationIntegration.md`，将技能运行事实接入角色动作；先验证 `Skill_Fireball`，再以同一套公共能力接入 `Skill_AreaBurst`、`Skill_Mend`、`Skill_BurningBuff`。不修改技能目标、命中、效果、成本、冷却或持续效果规则。

本任务遵守 SkillLab 的三层职责。这里的两种 C++ 用途必须区分：稳定内核承载固定、通用的运行机制；脚本化组合层在内核接口上拼装可变流程。组合层可以用 C++、Blueprint 或其他脚本技术实现，语言相同不代表职责相同。

| 层 | 本任务职责 | 当前状态与边界 |
| --- | --- | --- |
| 稳定运行内核 | 公共 C++ 能力：读取运行事实、按 `CastId` 播放/停止动作、隔离迟到事件、清理和降级 | 已有 `USkillLabSkillPresentationRouter`、`USkillLabSkillAnimationComponent`、`USkillLabSkillAnimNotify`；只在需求无法由现有接口满足时补公共能力，不写技能专属分支或资源参数 |
| 脚本化组合层 | 基于内核窄接口拼装表现流程，例如阶段响应、表现事件桥和收招调用 | 当前表现侧尚无明确、可复用的流程组合接口；若现有统一流程足够则不新增，若不足则新增可被多个技能复用的组合能力，不按技能各建一套代码 |
| 文本配置层 | 技能动作资源、可调参数及已由程序接口支持的内容声明 | 当前 JSON 有 `castAnimation`，解析器支持 `castMontage`；没有完整阶段/表现事件流程配置。配置不承担生命周期或技能结算 |

策划要求：动作开始、技能释放/效果事实和施法终态分别表达；动画事件不能扣费、选目标、判定命中或应用效果。取消/完成只清理对应施法动作，基础姿势恢复不结束独立持续效果；缺失动作资源不能改变技能事实，降级表现不能误导玩家。

## 当前运行链与复用点

`SubmitSkillRequest -> USkillLabSkillRuntimeComponent` 建立 `CastId`、推进阶段并发布 `FSkillLabSkillResult` -> `USkillLabSkillPresentationRouter` 广播 -> `USkillLabSkillAnimationComponent` 解析绑定并播放/停止 Montage -> A 提供的基础姿势链恢复。

可直接复用：

- `FSkillLabSkillResult` 和 `USkillLabSkillPresentationRouter`：统一运行事实及订阅入口。
- `USkillLabSkillAnimationComponent`：现有绑定解析、动作播放/停止、施法身份隔离和失败清理。
- `USkillLabSkillAnimNotify`：已有受 Montage 实例与活动施法身份检查的表现事件桥。
- B 的 Montage 内容生产、读回和校验工具：用于确认动画内容，不代替技能运行验证。

当前四技能仍由 Timer/行为链释放；动画 Notify 未驱动释放。火球 Active 后才生成投射物，命中/阻挡/过期再产生结果；范围爆发、修复和燃烧分别产生即时范围伤害、即时治疗和持续伤害。`USkillLabSkillBehavior`/Registry 是玩法行为层，可作为职责分离的参考，但不是动画表现组合层，也不应复用其结算代码。

## 实现落点

1. 核对现有 Router、AnimationComponent、Notify 和 A 的基础姿势接入，先用火球验证已有流程及 B 产出的动作资源；只补已确认缺失的公共能力。
2. 对照四技能的运行事实判断现有公共流程是否足够。只有存在无法表达的流程时，才增加可复用组合能力，并说明具体调用方与现有接口缺口；不因逐技能验收而逐技能增加 C++。
3. 只有已确认内容需要而当前配置契约不支持时，才扩展绑定/解析字段。字段表达资源与内容声明，不承载流程算法；具体技能内容由策划/动画内容侧维护在程序提供的接口范围内。
4. 保持技能释放、命中、效果和持续效果由技能运行时拥有。若考虑让动画事件改变 Timer 释放语义，先确认需求并评估公共运行接口影响；当前实现默认保留 Timer/行为链。

## 必读文件

- 策划要求：`ue/SkillLab/Docs/Planning/CharacterPresentation/Batches/Batch01-AIAdaptedAnimationPipeline/Infrastructure/03-SkillPresentationIntegration.md`
- 项目分层约定：`ue/SkillLab/Docs/ProjectContext/ProgramArchitectureContract.md`
- 运行与事件：`ue/SkillLab/Source/SkillLabSkills/Public/SkillLabSkillTypes.h`、`SkillLabSkillRuntimeComponent.h`、`Private/SkillLabSkillRuntimeComponent.cpp`
- 表现接入：`ue/SkillLab/Source/SkillLabSkills/Public/SkillLabSkillPresentationRouter.h`、`SkillLabSkillAnimationComponent.h`、`SkillLabSkillAnimNotify.h` 及对应 `Private` 实现
- 配置绑定：`ue/SkillLab/Source/SkillLabSkills/Private/SkillLabSkillDefinitionProvider.cpp`、`ue/SkillLab/Config/SkillDefinitions.json`
- 玩法行为分层参考：`ue/SkillLab/Source/SkillLabSkills/Public/SkillLabSkillBehavior.h`、`SkillLabSkillBehaviorRegistry.h` 及对应实现
- 资源生产和验收入口：`ue/SkillLab/Docs/ProgramWorkflow/AnimationWorkflow/Batches/Batch01-CharacterFoundation/AnimationContentTool.md`

## 验证与未决项

- 火球纵向验证应分别观察动作开始、Active/释放、投射物命中/效果和 Cast 终态；覆盖前摇取消、释放后取消、连续新旧 Cast、动作缺失及基础姿势恢复。
- 用同一公共能力验证其余三项：范围爆发的命中/伤害、修复的治疗结果、燃烧的 Buff 与 DamageTick 生命周期；动作终态不得改变这些事实。
- 检查公共内核无技能专属资源、参数或 `SkillId` 分支；若新增组合层，至少用两个技能证明同一能力可复用。已有 `SkillLab.Skills.CastLifecycle` 不覆盖 UE 资产播放和姿势恢复。
- 尚未由需求和现有证据决定、且可能改变实现的事项：四技能最终动作与释放时点配置；组合流程是否超出现有统一播放能力；是否要改变当前 Timer 释放语义。前两项先结合资源和运行核对，后一项若有需求再回交确认。

当前没有四技能动作播放、Notify 对齐、取消清理或姿势恢复的 UE 运行证据；本交接不代表这些行为已经实现或验收。
