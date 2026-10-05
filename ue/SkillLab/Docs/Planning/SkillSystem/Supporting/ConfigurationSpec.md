# 技能内容配置规范（策划层）

## 文档定位

本文档定义技能内容配置**表达什么**，不规定最终使用 `DataAsset`、`DataTable`、JSON 或其他技术载体。程序负责把本规范映射为可加载、可校验、可回滚的配置入口；程序不得因为载体方便而删除字段语义或改变默认规则。

## 配置对象层级

技能内容由以下对象组成：

| 对象 | 用途 | 是否独立配置 |
|---|---|---|
| SkillDefinition | 技能身份、输入、目标、时序和执行模板 | 是 |
| EffectDefinition | 伤害、治疗、护盾、属性修改和状态操作 | 是，可被多个技能引用 |
| StatusDefinition | 持续状态、标签、免疫和控制行为 | 是，可被多个效果引用 |
| PresentationBinding | 逻辑事件到动画、特效、音频、UI 的绑定 | 是，可被多个技能复用 |
| LoadoutEntry | 单位可用技能和槽位默认装配 | 是 |

技能配置引用效果、状态和表现绑定，不把所有内容复制进一个巨大技能对象。引用必须能被校验和定位。

## 所有配置的通用字段

| 字段 | 含义 | 要求 |
|---|---|---|
| Id | 稳定身份 | 必填，项目内唯一，不因显示名变化而改变 |
| DisplayName | 面向玩家/调试的名称 | 可本地化，不能作为运行时身份 |
| Description | 内容说明 | 可选，不参与逻辑 |
| Version | 配置版本 | 修改规则时递增 |
| Tags | 内容分类和查询标签 | 必须使用已登记标签 |
| Enabled | 是否允许进入正式内容集合 | 禁用时不能被默认装配或请求 |
| Source | 来源、作者或生成任务 | 必填，便于回溯 |

## SkillDefinition 配置

### 基础与输入

| 字段 | 类型/单位 | 必填 | 规则 |
|---|---|---|---|
| SkillId | 稳定标识 | 是 | 与技能内容身份一一对应 |
| InputMode | Direct / Target / Ground / Direction / Channel | 是 | 决定 M01/M04 的前置交互 |
| Template | Direct / UnitTarget / GroundArea / LineCone / Projectile / PersistentArea / Channel / Movement | 是 | 必须是已实现模板 |
| CancelPolicy | 取消策略 | 是 | 必须声明可取消阶段和结果 |
| InterruptPolicy | 打断策略 | 是 | 必须声明死亡、受击和状态阻塞处理 |

### 资源与时间

| 字段 | 类型/单位 | 默认 | 规则 |
|---|---|---|---|
| Cost | 数值+资源类型 | 0 | 非负；激活时点由公共规则决定 |
| Cooldown | 秒 | 0 | 非负；启动时点由公共规则决定 |
| Windup | 秒 | 0 | 不允许为负 |
| ActiveDuration | 秒 | 0 | 引导/区域等需要时必填 |
| Recovery | 秒 | 0 | 结束阶段时长 |
| SegmentCount | 整数 | 1 | 引导/多段模板使用；Direct 必须为 1 |
| SegmentInterval | 秒 | 0 | 多段模板使用 |

### 目标与空间

| 字段 | 类型/单位 | 必填 | 规则 |
|---|---|---|---|
| TargetRelation | Self / Ally / Enemy / Neutral / Any | 是 | 与目标类型共同校验 |
| TargetType | Self / Unit / Ground / Direction / Area | 是 | 必须匹配 InputMode 和 Template |
| MaxRange | 厘米 | 视目标而定 | Ground、Unit、Area 通常需要 |
| Shape | Point / Line / Cone / Circle / Capsule | 模板决定 | 只允许模板支持的形状 |
| Radius/Length/Angle | 厘米/度 | 视形状而定 | 必须在项目允许范围内 |
| RequireLineOfSight | 布尔 | false | 开启后由 M04 执行视线检查 |
| RetargetOnRelease | 布尔 | false | 只能在目标规则允许时开启 |

### 执行、效果与表现

| 字段 | 含义 | 规则 |
|---|---|---|
| Effects | 效果引用和应用阶段 | 引用的 EffectDefinition 必须存在 |
| StatusOperations | 状态添加、移除、驱散或免疫 | 必须声明目标关系和应用阶段 |
| SpawnObject | 投射物/区域/位移对象参数 | 只有对应 Template 可以使用 |
| PresentationBinding | 表现绑定引用 | 缺失是否允许由项目默认规则决定 |
| HitPolicy | 单次、周期、穿透、去重策略 | 必须与 Template 匹配 |

## EffectDefinition 配置

效果至少定义：EffectId、EffectType、Magnitude、Attribute、Duration、Period、StackPolicy、MaxStacks、ImmunityTags、Dispellable、SourceTags 和 TargetTags。

效果类型至少包括 Damage、Heal、Shield、ModifyAttribute、ApplyStatus、RemoveStatus、Dispel 和 ResourceChange。即时效果的 Duration 必须为 0；周期效果必须有正 Period；持续效果必须声明结束和移除规则。

叠加策略只能从项目支持的枚举中选择：Reject、Refresh、Replace、Stack、Independent。选择 Stack 时必须填写 MaxStacks；选择 Refresh 时必须说明刷新强度是否覆盖。

## StatusDefinition 配置

状态至少定义：StatusId、GrantedTags、BlockedTags、ImmunityTags、Duration、Period、StackPolicy、DispelCategory、MovementPolicy、AbilityPolicy 和 PresentationBinding。

状态配置不能直接写任意代码。控制、免疫、不可选中和沉默等公共语义必须引用已登记标签和项目规则。

## PresentationBinding 配置

表现绑定至少定义：BindingId、EventType、ResourceType、ResourceReference、AttachRule、Loop、StopEvent、PreviewOnly、Priority 和 MissingResourcePolicy。

`PreviewOnly=true` 的资源不能绑定正式命中事件；`Loop=true` 必须存在 StopEvent；持续表现必须能通过 CastId 或 ObjectId 找到并清理。

## LoadoutEntry 配置

装配配置至少定义 UnitTemplateId、AvailableSkills、DefaultSlots、InputAction、SlotId 和 ReplacePolicy。技能必须先存在于 AvailableSkills，才能进入 DefaultSlots；禁用或缺失技能不能静默装配。

## 校验规则

提交配置前必须检查：

1. Id 唯一且格式合法；
2. 所有引用存在、启用且版本兼容；
3. InputMode、TargetType、Template 和 Shape 相互匹配；
4. 数值、时间、层数和角度在允许范围；
5. 多段、周期、持续表现和空间对象都有结束条件；
6. 效果、状态、免疫和驱散引用合法；
7. 表现循环有停止事件，缺失资源策略已声明；
8. 配置没有试图覆盖公共契约中的固定约束。

校验失败的配置不能进入正式技能列表。程序可以提供编辑器校验、命令行校验和运行前校验，但校验规则的业务含义来自本文档。

## 策划与程序边界

策划负责字段是否存在、字段的业务意义、默认值、合法范围、字段之间的组合关系和玩家可观察结果。程序负责配置载体、序列化、加载、缓存、引用解析、性能和错误展示。AI 可以生成配置实例，但必须先通过校验并带来源和版本。

本文档未定义具体技能数值；具体样本只能在本规范允许的字段和范围内填写内容。
