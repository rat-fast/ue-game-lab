# Batch01 角色基础表现 Review

## 当前总控结论

**整批尚未最终放行，状态为“分切片验收中”。**

架构 v1.2 已经通过独立架构 Review，允许按切片进入实现；这不等于代码、资产、运行或视觉表现已经全部通过。当前总控结论按 A、B、C 分开记录，不能用一个切片的通过替代其他切片的门禁。

## 分切片状态

| 切片 | 实现状态 | 独立 Review 状态 | 当前结论 |
| --- | --- | --- | --- |
| A 基础状态 | 已接入角色动画状态组件、原生 AnimInstance、`ABP_Unarmed` 和五态烟测 | 最新 Review 要求补同一输入下的迁移前/迁移后对照 | **先补证据后继续** |
| B 动画内容 | Editor-only Commandlet 已构建；临时副本读写、保存、重载、回读和负例已执行 | 修复后实现记录已更新，需以当前工作树重新确认 Review 是否已覆盖新证据 | **待 Review 收口** |
| C 技能表现 | 统一 Router/AnimationComponent 接入，四技能 smoke、CastId 隔离和自动化已有证据 | 独立 Review 结论为“允许继续” | **允许继续，保留表现缺口** |

## 已确认的范围

- A 负责角色移动事实到基础动画状态的接入，不负责技能动作和 Montage 生产。
- B 负责结构化动画内容的机器读、局部写、保存、重载、回读和校验，不改变技能规则和运行时事实。
- C 负责统一技能表现运行接入和组合边界，不把动作事件提升为命中、扣费、效果或 Cast 终态的权威。
- 缺失资源降级、Notify 时间对齐、取消后的姿势恢复和 PIE 可视质量属于本批表现验收缺口；应由相应实现对话补齐，再交独立 Review，不在总控文档中伪造为已完成。

## 当前已具备的证据

- A：`Saved/Logs/SkillLabAnimationVisual-Acceptance.log`、`Saved/AnimationStateVisual/`、`Saved/AnimAssetVerify.log`。
- B：`Saved/AnimationReports/copy-write-readback.json` 及 B 工具文档中记录的只读和负例报告。
- C：`Saved/Logs/SkillLab-C-PresentationSmoke-20261018.log`、`SkillLab-C-CastHandoff-20261018.log`、`SkillLab-C-Automation-Editor-20261018.log`。

这些证据支持对应的局部结论，不支持整批视觉质量或全部资源降级行为已经完成。

## 必须关闭的验收项

1. A：在相同入口和输入条件下生成迁移前/迁移后对照，记录允许差异、实际差异和缺陷；完成后由 A Review 复核。
2. B：由 B Review 对当前工作树复核修复后的 Commandlet；至少保持 Schema/未知字段、资产缺失、非法参数、局部字段写回和重载回读的机器可读证据。复杂 Notify、多片段精确定位和不兼容 Skeleton 负例继续明确为未覆盖，除非本批需求另有扩展。
3. C：补缺失/不可用动作资源的降级行为、Notify 实际时间对齐、迟到 Notify 身份隔离、取消/完成后的基础姿势恢复，以及可视 PIE 或等价真实视口证据。
4. 总控：所有切片 Review 收口后，判断是否改变 `ProjectContext/` 的当前事实；只有确实改变且 Review 放行，才同步项目事实文档。

## Review 顺序

1. 架构符合性；
2. 代码、配置和资产实现正确性；
3. 构建、资产加载、运行表现和回归证据；
4. 项目事实文档是否需要同步。

Review 结论只能是“允许继续”或“先修复后继续”，不能用编译成功替代运行和表现验证。
