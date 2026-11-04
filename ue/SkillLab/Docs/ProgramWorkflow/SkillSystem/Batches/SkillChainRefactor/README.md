# SkillChainRefactor 程序交接入口

## 范围与依据

本轮按 [共同约束与样本验收](../../../../Planning/SkillSystem/Batches/SkillChainRefactor/SkillChainRefactorRequirements.md) 拆分通用运行基建和现有内容迁移。通用能力覆盖当前 SkillLab 技能系统整体；内容迁移仅覆盖当前 GAS 技能链路中已有且被需求覆盖的定义，排除 Batch01 六技能作为新增内容迁移/玩法扩展对象。四个代表样本不是通用能力上限。

策划权威来源：

- [通用技能基建需求](../../../../Planning/SkillSystem/Batches/SkillChainRefactor/SkillInfrastructureRequirements.md)
- [技能配置生产需求](../../../../Planning/SkillSystem/Batches/SkillChainRefactor/SkillContentProductionRequirements.md)
- [共享技能系统基线](../../../../Baseline/SkillSystem/README.md)

## 架构交接映射

- [通用运行基建交接](SkillRuntimeInfrastructureHandoff.md)：GAS Ability 激活/施法、Notify 释放事件接入、请求与 Cast 身份、目标/执行/效果/持续状态事实及终止清理。实现必读其自列文件，不要求先读内容迁移交接。
- [技能定义加载与规范化接口交接](SkillDefinitionLoadingHandoff.md)：C++/GAS 基建实现子任务，定义 Lua 加载期技能声明、JSON 数值校验、唯一 `FSkillLabSkillDefinition` 和内容迁移线消费接口；内容迁移依赖其 Review 通过。
- [现有定义迁移交接](SkillDefinitionMigrationHandoff.md)：当前符合范围的定义清单、Lua/JSON 内容边界、迁移前对照值和依赖。迁移实现依赖通用运行基建先确定可用的内容/运行接口。

两线接口和顺序以各自交接的“交付边界”为准。动画 Notify 事件接入属于通用运行基建与表现适配的窄接口，不新增动画体验。加载期定义能力属于 C++/GAS 线；Lua/JSON 内容线在格式/字段接口经实现和 Review 核验后迁移，不拥有公共 Provider 或结算职责。

## 状态

本文档映射需求与程序交接；架构方案和实现/验证状态分别见对应文档。Lua/JSON 加载期定义能力已由 C++/GAS 基建线实现；内容线须等独立 Review 核验通过后由 PM 重新派发。

## 当前程序推进

| 职责 | 状态 | 对话 |
|---|---|---|
| 架构核实 | 运行链交接与加载期定义能力交接已完成 | 交接映射见上方 |
| C++/GAS 通用基建 | Notify/Ability 事件链与加载期定义接口已实现并通过定向自动化；迁移后四技能有限 Runtime 验证已完成，部分场景与 PIE 未验证 | `01a1a2a6-559a-7ba2-b73e-8f66d7e0b14c`；[实现摘要与验证](ImplementationSummary.md)、[运行验证摘要](Implementation/RuntimeValidationSummary.md) |
| 基建独立 Review | Notify/Ability 事件链与加载期定义接口 Review 通过；真实内容结果仅有有限运行证据 | [基础设施 Review](Review/InfrastructureReview.md)；`01a1a2ba-f313-7e41-b40f-5bd5be146189` |
| Lua/JSON 内容迁移 | 四项候选内容已迁移；Provider 接受四项定义，有限 Runtime Probe 已完成；部分场景对照及 PIE 未验证 | [内容迁移摘要](Implementation/ContentMigrationSummary.md)、[运行验证摘要](Implementation/RuntimeValidationSummary.md) |
| 内容迁移独立 Review | 静态内容 Review 通过；迁移后有限 Runtime 验证通过，行为等价及可见 PIE 未完成 | [内容迁移 Review](Review/ContentMigrationReview.md)、[运行验证摘要](Implementation/RuntimeValidationSummary.md) |

### 当前阶段

加载期接口已实现且独立 Review 通过；字段契约、构建、自动化及迁移前真实目录拒绝情况见[实现摘要](ImplementationSummary.md)和[基础设施 Review](Review/InfrastructureReview.md)。四项 JSON/Lua 已迁移并通过静态内容 Review；迁移后 Provider 已接受四项定义，NullRHI Runtime Probe 已记录四项请求和有限结算结果。观察与候选快照部分一致，但不构成全场景行为等价；Mend 受伤治疗、Fireball 阻挡/未命中、AreaBurst 多目标/空区域及可见 PIE 仍未验证，详见[运行验证摘要](Implementation/RuntimeValidationSummary.md)。此前因旧内容格式导致的 `NotInitialized` Probe 不是迁移后运行证据。

### 迁移后验证阶段

配置迁移恢复门槛已由[基础设施 Review](Review/InfrastructureReview.md)通过；迁移后 Provider/Runtime Probe 的有限结果、候选快照对照和未覆盖项见[运行验证摘要](Implementation/RuntimeValidationSummary.md)。后续仍需按[共同约束验收表](../../../../Planning/SkillSystem/Batches/SkillChainRefactor/SkillChainRefactorRequirements.md)补足受伤治疗、Fireball 阻挡/未命中、AreaBurst 多目标/空区域等场景及可见 PIE。交接中记录的 SHA-256 是迁移前候选快照标识，不是批准的产品版本。基础设施 Review 所列 reload 原子保持、磁盘改动后的 LuaSource 缓存行为、负向自动化和字段组合覆盖缺口仍未验证。
