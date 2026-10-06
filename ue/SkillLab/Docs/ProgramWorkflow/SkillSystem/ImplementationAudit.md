# SkillSystem 首阶段实现审计（历史快照）

## 文档性质

本文记录首阶段重构前的代码事实、风险和审计结论，不代表当前实现状态。当前事实以 `ImplementationSlice.md` 为准，编译与运行证据以 `Validation/` 为准。本文件不修改 Planning 文档。

## 审计时的旧实现

- `USkillLabSkillComponent::RequestTestPulse` 同时承担测试入口、请求门控、Pulse 能力查找、资源/冷却、效果协调和日志。
- `Skill_TestPulse`、消耗、冷却和效果强度散落在组件、Pulse Ability 和 Pulse Effect 中。
- 请求结果只有基础身份字段，没有正式的 `SkillRequest`、定义状态、定义快照或结构化效果应用结果。
- 日志是唯一可观察出口，没有统一 `SkillEvent`、阶段字段、效果失败事件或 PresentationRouter。
- 请求曾允许组件隐式使用 Owner 作为施法者，不能证明输入来源单位和实际执行单位一致。
- AttributeSet 构造函数和 SkillComponent 同时保存初始属性/资源值。

## 历史风险结论

1. 具体技能内容没有与通用生命周期隔离。
2. 请求身份、来源、目标和效果来源无法在结构化结果中完整追踪。
3. 失败、取消、重置和表现清理没有统一事实出口。
4. 命令行 ExecCmds 不能替代 PIE 交互证据，早期日志只证明地图初始化。

## 修复路由

上述问题对应的当前实现分别位于：

- 类型、阶段、定义状态、请求/结果/事件：`Source/SkillLab/SkillLabSkillTypes.h`；
- 定义解析和版本/来源：`SkillLabSkillDefinitionProvider.*`；
- 统一请求、身份校验、生命周期和重置：`SkillLabSkillComponent.*`；
- 通用效果入口：`SkillLabSkillAbility.*`；
- 表现事件接收和清理：`SkillLabPresentationRouter.*`。

不要把本历史审计中的旧函数名、旧字段或旧验证结论当作当前契约。

