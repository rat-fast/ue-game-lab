# 现有技能定义迁移架构交接

## 本次行为与范围

依据[技能配置生产需求](../../../../Planning/SkillSystem/Batches/SkillChainRefactor/SkillContentProductionRequirements.md)及[共同约束](../../../../Planning/SkillSystem/Batches/SkillChainRefactor/SkillChainRefactorRequirements.md)，把当前 GAS 技能链路中现存、被本轮需求覆盖的技能专属定义从 C++ 注册配置迁往需求指定的 Lua 定义方向和 JSON 高频纯数值方向，并对照迁移前行为。不得扩大为玩法扩展或 Batch01 六技能新增内容迁移。

## 完整符合范围清单

按 `SkillLabSkillDefinitionProvider` 的 JSON 载入项、`FSkillLabSkillBehaviorRegistry` 注册项、`USkillLabSkillLoadout` 授予入口、源码中的技能注册文件、`Config/Skill_*.lua`、`Config/Animation/Skill_*` 目录及 `Content/SkillLab/Animations` 资产名交叉核对，当前可解析且已授予的定义只有以下四项；四项均在共享基线及本轮需求覆盖中，因此完整符合迁移范围：

| 当前定义 | 当前语义摘要 | 现有入口 |
| --- | --- | --- |
| `Skill_Fireball` | 敌方单位、投射物、伤害 | `SkillLabFireballSkill.cpp`、JSON、`Skill_Fireball.lua` |
| `Skill_AreaBurst` | 地面位置、范围敌方、伤害 | `SkillLabAreaBurstSkill.cpp`、JSON、`Skill_AreaBurst.lua` |
| `Skill_Mend` | 自身即时治疗 | `SkillLabMendSkill.cpp`、JSON、`Skill_Mend.lua` |
| `Skill_BurningBuff` | 敌方单位即时施加周期伤害 | `SkillLabBurningSkill.cpp`、JSON、`Skill_BurningBuff.lua` |

注册器只有这四个 builder；默认定义 ID 从注册器取得，JSON 也只有这四个条目。四个 Lua 文件和四个动画配置目录/软路径同样只对应这四项。没有发现四样本以外的非 Batch01 当前 GAS 技能定义。对 Batch01 排除名单 `Skill_Firebolt`、`Skill_Mend`、`Skill_ArcStrike`、`Skill_BurnField`、`Skill_ChannelBeam`、`Skill_Barrier` 的独立核对结果是：它们只在 `Docs/Planning/SkillSystem/Batches/Batch01-SixSkillVerticalSlice/` 及被其引用的程序占位文档中出现；在 `Source/SkillLabSkills`、`Config/SkillDefinitions.json`、`Config/Skill_*.lua`、`Config/Animation/Skill_*` 和 `Content/SkillLab/Animations` 中没有对应注册、定义条目、脚本、动画目录或动画资产。`Saved/` 下的 Probe/报告是生成快照，且同样只含四项，不是额外权威来源。故不列为新增迁移对象。

注意 `Skill_Mend` 标识在 Batch01 也出现：本轮只迁移当前 `Skill_Mend` 自身治疗定义并保持当前结果，不迁移 Batch01 的 Self/Ally 扩展或其其他新规则；不得因同 ID 将未来策划内容合并进当前定义。

## 当前分层事实与建议

当前 JSON 是数值、阶段、动作软路径、释放 Tag/超时、阻塞标签与 Lua 文件绑定的事实来源；四份 Lua 实际都只有 `SkillLabSkill_RequestAction()` 后接 `SkillLabSkill_Finish()`。目标形状、目标关系、效果类型、执行模板、四种 C++ BehaviorClass 和 Fireball 投射物参数由四个 C++ 注册 builder 给出。Provider 先应用 builder 再覆盖 JSON 支持的字段。因此“脚本已绑定”不等于专属定义已从 C++ 迁出。

建议内容职责：

- Lua 表达需求已指向的技能专属目标形态/关系、效果类型和技能组合；执行模板等分类由实现按业务含义确认后通过受限 API 使用。
- JSON 只放被确认属于高频纯数值的值（如适用的伤害/治疗量、距离、半径、速度、持续时长/周期），以及为维持当前内容确需保留且不是 Lua 组合的引用/配置。字段归类和最终 schema 是实现选择；不可把目标关系、效果类别或流程偷换成纯数值。
- C++ 基建保留统一请求、Ability/Cast 身份、门控、目标合法性、实际命中、效果提交、结果事件和清理；脚本不能直接结算或宣告结果。
- 保持一个权威来源。若 UE 资产是运行载体，需由文本源导入/生成并可 diff、校验；当前没有要求具体工具或热更新。

内容迁移依赖[通用运行基建交接](SkillRuntimeInfrastructureHandoff.md)提供稳定的内容表达和动作执行接口。先定接口后按四项迁移，可在各技能 Lua/JSON 内容与共享配置 Schema 之间并行，但 C++ 核心职责由基础线负责；不得由内容线修改公共 Ability 生命周期来绕过基建依赖。

## 迁移前对照快照

对照对象为本次架构核对时的当前工作树，非已批准内容版本，也不代表策划已确认所有未冻结参数。复现定位：

- Git `HEAD`: `47f65a6acdade6a13e7fd94f88c68395e751a958`。
- `Source/SkillLabSkills`、`Source/SkillLabGAS`、`Source/SkillLabPresentation`、`Config`、四个动画资产目录的 tracked/untracked 工作树状态均干净；这些路径没有 staged/unstaged 差异。全局工作树有不相关文档和 `.obsidian` 修改，技能链改造策划目录也为 untracked，但不改动此快照所指的运行定义。
- `Config/SkillDefinitions.json` SHA-256：`504A2E3EBCF13399DD1A33F60686AD9A2A68EF804B503AFDB47703E0FE4B7DFF`。
- 四份 Lua 当前 SHA-256 均为 `24539FCCE29D65C9ED3A0DD076EB4F86843320D82B01336B5CD0603FF669F16F`，彼此相同。
- 上述 JSON 的 HEAD blob 为 `40762a140fdb904f2e59e04310217b4ce3f18b67`；四份 Lua 的 HEAD blob 均为 `51efe6ff1fc281b36cc268a426b262f02258ddf1`。工作树文件与这些 HEAD blob 内容一致。

当前 JSON 内容值如下，迁移前后按相同请求和初始状态复现；这些记录值不自动构成新的策划规则。软路径为 `/Game/SkillLab/Animations/AM_Skill_<id>.AM_Skill_<id>`，四项均绑 `SkillLab.Event.Cast`、`GameplayEvent` 和 3 秒事件超时，阻塞标签为 Dead/Stunned/Silenced。

| 技能 | Cost | Cooldown | Magnitude | PowerCoefficient | Windup / Recovery | Duration / Period |
| --- | ---: | ---: | ---: | ---: | --- | --- |
| Fireball | 10 | 1.0 | 30 | 1.0 | 0.366667 / 0.2 | - |
| AreaBurst | 20 | 3.0 | 20 | 0.5 | 1.2 / 0.633333 | - |
| Mend | 15 | 2.0 | 40 | 未配置 | 0.5 / 0.5 | - |
| BurningBuff | 10 | 2.0 | 6 | 0.2 | 0.533333 / 0.466667 | 4.0 / 1.0 |

其它可核对的现有 C++ 定义：Fireball range 1200、投射物速度 1250/碰撞半径 18/寿命 5/视觉缩放 0.62；AreaBurst range 1000/radius 350；Mend range 800；BurningBuff range 900。它们是当前代码默认值，不代表需要归属 JSON 还是 Lua 的最终决定。属性 Power 默认值及实际伤害公式须以运行结果为对照；例如声明 Magnitude 不等同于实际伤害。

若实现开始前任一相关文件、注册项或动画资产已发生变化，重新计算快照并保存原值/变更差异；未提交差异只能作为候选对照并须报告，不能称为批准版本或等价基线。遇到无法安全归属的变化，停止对该条目的等价判断并回报选项，不用 HEAD 或记忆值替代当前快照。

## 实现必读入口

- `Config/SkillDefinitions.json` 和四份 `Config/Skill_<id>.lua`：当前文本定义/组合。
- `Source/SkillLabSkills/Private/SkillLabSkillDefinitionProvider.cpp`、`Public/SkillLabSkillDefinitionProvider.h`、`Public/SkillLabSkillTypes.h`：解析、C++ builder 合并、字段校验和运行定义结构。
- `Private/SkillLabSkillBehaviorRegistry.cpp`、`Private/SkillLabFireballSkill.cpp`、`SkillLabAreaBurstSkill.cpp`、`SkillLabMendSkill.cpp`、`SkillLabBurningSkill.cpp`：当前完整注册清单和技能专属值/模板。
- `Private/SkillLabSkillBehavior.cpp`、`Public/SkillLabSkillBehavior.h`：Lua 宿主 API 与声明动作映射。
- `Private/SkillLabTargetingService.cpp`、`Private/SkillLabSkillEffectExecutor.cpp`、`Source/SkillLabGAS/Private/SkillLabAbilitySystemComponent.cpp`、`SkillLabCombatAttributeSet.cpp`：权威目标和结果路径；迁移不可复制这类规则到脚本。
- `Config/Animation/Skill_<id>/` 和 `Content/SkillLab/Animations/AM_Skill_<id>.uasset`：释放 Notify 对照入口，不是技能数值/定义权威源。
- 需求/约定：[本轮配置生产需求](../../../../Planning/SkillSystem/Batches/SkillChainRefactor/SkillContentProductionRequirements.md)、[共同约束](../../../../Planning/SkillSystem/Batches/SkillChainRefactor/SkillChainRefactorRequirements.md)、[ProgramArchitectureContract](../../../../ProjectContext/ProgramArchitectureContract.md)、[SkillLabSkills 模块事实](../../../../ProjectContext/Modules/SkillLabSkillsRuntime.md)。

## 独立验证与未决

每项按迁移前快照逐一核对请求、目标/命中、实际伤害/治疗、资源/冷却、投射物或持续效果身份/移除和 Cast 终态；四样本的特别场景见共同需求验收表。检查脚本异常、定义校验失败和 Runtime Probe/现有 `SkillLab.Skills.DefinitionValidation`，但本交接不要求新增工具框架。文本解析通过不能代替实际结果一致；未运行的场景明确标待验证。

需求未冻结的其它数值、目标边界和异常规则不能由当前默认值升级成新业务承诺；遇到结果差异先分类为字段归属/实现差异/既有行为证据缺口，再按需求回交。不要新增前后摇、受击或正式 VFX 体验，也不要将内容迁移扩展至 Batch01 六技能。
