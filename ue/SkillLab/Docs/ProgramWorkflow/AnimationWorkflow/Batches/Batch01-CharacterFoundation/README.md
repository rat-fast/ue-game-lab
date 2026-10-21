# Batch01 角色基础表现程序架构交接

## 状态

本批已完成架构修订和独立架构 Review，A、B、C 三个实现切片也已产生实际代码、资产/工具和运行证据。当前处于**分切片验收与缺口修复阶段**，尚未达到整批最终放行。

当前总控门禁：

- A：实现已接入默认角色并有基础状态烟测，但独立 Review 要求补齐同一输入下的迁移前/迁移后对照；在该对照完成前不能关闭 A。
- B：Editor Commandlet 已构建，并已对临时 Montage 副本完成读、写、保存、重载、回读和负例验证；仍需以当前工作树重新确认独立 Review 记录，复杂 Notify、多片段精确定位和不兼容 Skeleton 负例保持未覆盖。
- C：独立 Review 已允许继续；四技能运行链和 Cast 身份隔离已有证据，但缺失资源降级、Notify 时间对齐、取消后的姿势恢复和 PIE 可视质量仍是后续验收项。
- 整批：不能因为 C 已放行或 A/B 已有编译/烟测证据而宣布完成。总控必须先收敛 A/B 的 Review 状态，再安排表现缺口验证，最后更新 ProjectContext。

本目录只维护本批程序任务状态、交接和验证导航；策划需求、共享表现业务语义和项目代码事实仍以各自权威文档为准。

## 需求与交接映射

| 任务 | 程序范围 | 策划需求入口 | 架构交接 |
| --- | --- | --- | --- |
| A | 角色动画数据与 Idle/Move/Jump/Fall/Land 基础状态 | `Planning/CharacterPresentation/Batches/Batch01-AIAdaptedAnimationPipeline/Infrastructure/00-AnimationDataAndBaseStates.md` | `Architecture-A-BaseStates.md` |
| B | 结构化动画内容与 Montage 生产、读回、校验 | `Planning/CharacterPresentation/Batches/Batch01-AIAdaptedAnimationPipeline/Infrastructure/01-StructuredAnimationContent.md`、`02-MontageProductionEntry.md` | `Architecture-B-AnimationContent.md` |
| C | 技能表现运行内核、组合边界与技能内容接入；先火球，再迁移三项 | `Planning/CharacterPresentation/Batches/Batch01-AIAdaptedAnimationPipeline/Infrastructure/03-SkillPresentationIntegration.md` | `Architecture-C-SkillPresentation.md` |

## 最小总体依赖

- 三项共同读取 `Docs/Baseline/CharacterPresentation/01-CharacterPresentation.md`、`02-SkillActionPresentation.md`、`03-AssetsAndVerification.md`，以及对应的 `Docs/Baseline/SkillSystem/02-RequestLifecycle.md`、`06-FeedbackAndObservability.md`。
- A 依赖默认可控角色的真实 Mesh/AnimBP 绑定核对；C 依赖 A 的基础姿势恢复，并复用 B 提供的动画/Montage 内容读回与校验能力。
- B 不改变技能规则或 `Config/SkillDefinitions.json` 的内容取值；C 按项目三层职责区分稳定 C++ 表现内核、可组合表现流程和文本内容声明，不把动画事件当作命中、扣费或效果事实。
- 资产、路径和源码存在均不是运行验收证据；具体验证入口和未验证项记录在三份交接中。

## 总控后续交接

1. 先由 A 实现对话补齐迁移前/迁移后对照证据，再交 A Review 复核。
2. 由 PM 核对 B 当前实现与独立 Review 的最新状态；若 Review 仍引用修复前事实，先让 B Review 在当前工作树复审，不重新设计 B。
3. 在 A 的基础姿势入口稳定后，由 C 实现对话补做缺失资源降级、Notify 对齐、取消后姿势恢复和可视运行验证；这些是表现验收工作，不改变技能事实权威。
4. 每项修复完成后回到对应独立 Review 对话。Review 只给出“允许继续”或“先修复后继续”，不直接修改被审实现。
5. 所有切片放行且项目事实确实变化后，才由 Review 对话按 `project-documentation` Skill 更新 `ProjectContext/`；本批配置文档不虚构尚未确认的资源和时点。
