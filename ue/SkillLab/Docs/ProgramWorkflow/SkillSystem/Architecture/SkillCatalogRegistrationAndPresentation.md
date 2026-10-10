# 技能目录、注册与表现接口架构

## 文档状态

- 任务：SkillSystem
- 版本：v0.3 专题修订
- 状态：当前冻结规则，细化 `ArchitectureBaseline.md` 的定义、注册和表现边界
- 范围：技能目录加载、行为注册、表现事件订阅和动画资源适配
- 排除：运行时施法生命周期、PlayerController、HUD、测试场景和技能行为语义

## 当前实现落点

| 职责 | 接口/实现 | 约束 |
| --- | --- | --- |
| 内容目录 | `FSkillLabSkillDefinitionProvider` | JSON 只覆盖内容字段；候选目录完整校验后再替换当前目录 |
| 技能行为注册 | `FSkillLabSkillBehaviorRegistry` | 每个技能文件返回一个注册描述项；注册按 `SkillId` 幂等，不能静默覆盖已有行为 |
| 表现绑定 | `FSkillLabSkillDefinitionProvider::ResolvePresentationBinding` | 表现适配器只取得 `SkillId` 对应的表现绑定，不读取运行规则 |
| 事实分发 | `USkillLabSkillPresentationRouter::OnPresentationEvent` | 路由器订阅运行事实并广播给动画、UI 或其他表现适配器 |
| 动画适配 | `USkillLabSkillAnimationComponent` | 订阅表现路由器；只处理 Started/Cancelled/Completed 和动画通知，不判断技能规则 |

## 冻结规则

1. `SkillId` 是目录身份，不再作为通用运行流程中的技能分支条件。行为差异由注册描述项和具体行为类承载。
2. 行为注册函数只返回 `{SkillId, Builder}`，注册表负责统一登记。内置注册使用描述项数组接入，新增技能只增加自己的注册函数和行为文件。
3. 重复加载、重复初始化或重复 wiring 不得替换已有注册行为。目录读取失败保留上一份有效目录。
4. 定义提供器是表现绑定的权威入口。动画适配器不得通过完整技能定义反查内容规则；缺失绑定只记录诊断，不阻止技能运行。
5. `USkillLabSkillPresentationRouter` 是运行事实到表现适配器的单向出口。适配器只能订阅 `OnPresentationEvent`，不能向运行组件回写规则、资源、扣费或效果结果。
6. 动画 Montage 的生命周期按 `CastId` 归属；旧施法的延迟完成事件不能停止新施法的 Montage，动画通知必须匹配当前 Montage 实例。

## 公共接口不变量

- `Resolve`、`ResolvePresentationBinding` 对未知 `SkillId` 返回 `false`，并清空输出结构。
- `Register` 忽略空身份或空构建器；同一身份的重复注册保持首次注册结果。
- 路由器先广播结构化事实，再执行默认屏幕/世界反馈；表现订阅者不会改变运行结果。
- `Bind(nullptr)` 必须解除运行事件订阅；表现适配器在 `EndPlay` 解除路由器订阅并停止当前 Montage。
- 表现资源缺失、没有 Skeletal Mesh 或 Montage 播放失败都只影响表现，不改变伤害、治疗、DoT、目标关系或事件语义。

## 扩展边界

新增技能的最小变更集合是：

1. 新增技能行为类和 `Configure` 函数；
2. 新增返回 `FSkillLabSkillBehaviorRegistration` 的技能注册函数；
3. 在内置 wiring 数组加入注册函数；
4. 在 JSON 目录增加该 `SkillId` 的内容字段；
5. 如需表现资源，只增加定义中的绑定，不修改动画适配器或运行生命周期。

未来迁移到 Data Asset 或其他结构化载体时，只替换目录读取/校验实现，继续保留 `Resolve`、`ResolvePresentationBinding`、注册表和表现订阅接口。

## 验收边界

- 四个现有技能的 `SkillId`、JSON 字段含义、目标关系、伤害/治疗/DoT 和事件触发保持不变。
- 目录校验测试继续覆盖未知字段、重复 ID、负数值和有效注册行为。
- 静态检查确认动画组件不直接订阅运行时事件，也不通过完整定义读取表现绑定。
- `git diff --check` 和 Game 编译必须通过；不启动 Editor、PIE 或游戏。
