# 公共契约与默认规则

## 文档状态

本文档是所有技能模块共享的实现交接契约。模块可以细化这里的规则，但不能改变身份、结果和清理语义。

已确认：请求、施法实例、目标解析、实际命中、效果、状态、临时对象和表现事件必须可区分。

当前默认：首阶段采用同步提交、单次请求、激活成功后扣费和启动冷却；输入缓冲是可选能力。

## 身份契约

| 身份 | 创建时机 | 用途 | 结束 |
|---|---|---|---|
| UnitId | 单位初始化 | 追踪单位、阵营和 ASC 归属 | 单位销毁或重置 |
| SkillId | 技能内容定义 | 标识可复用技能 | 内容版本变化 |
| RequestId | 每次提交尝试 | 追踪拒绝、缓冲或开始 | 请求终结 |
| CastId | 激活成功后 | 关联一次实际运行 | 完成、取消、打断或重置 |
| EffectId | 效果应用/建立时 | 追踪瞬时或持续效果 | 应用结束或移除 |
| ObjectId | 投射物/区域创建时 | 追踪临时对象 | 到期、销毁或重置 |

槽位、技能定义、能力授予和施法实例不能互相替代。

## 请求与结果

请求至少包含来源、施法者、SkillId、目标规格、时间和追踪信息。

- `Rejected`：未创建 CastId，不扣费、不启动冷却、不产生正式效果。
- `Buffered`：未开始，保存原始 SkillId 和目标上下文，过期后为 `Expired`。
- `Started`：创建 CastId，进入生命周期。
- `Completed`：正常结束；效果是否命中另有结果。
- `Cancelled`：已开始后被明确取消。
- `Interrupted`：死亡、受击、重置或规则导致的中断。
- `Expired`：缓冲或目标上下文过期。

效果结果至少区分 `Applied`、`Immune`、`InvalidTarget`、`Blocked`、`Failed`。

## 生命周期与时序

生命周期为：Created、Validating、Accepted、Windup、Active、Recovering、Completed。异常出口为 Rejected、Cancelled、Interrupted、Expired。每个终态只能进入一次，清理必须幂等。

默认时序：提交时检查单位、技能、授予、状态、资源、冷却和目标；激活成功后创建 CastId；进入施法阶段时扣除启动资源并启动冷却；前摇取消默认不退款；已发生的效果不因后续取消或死亡自动回滚；确认目标和实际命中都要重检；技能结束不自动销毁仍有合法存续期的投射物或区域。

## 事件契约

事件至少包含：EventType、EventId、UnitId、SkillId、RequestId、CastId、Source、Stage、Time、TargetSpec、Result、Reason。命中/效果事件额外包含目标、位置、EffectId 或 ObjectId。

表现订阅事件，不改变逻辑结果；日志记录同一事件，不创建第二套状态。

## 配置与固定规则

配置可以改变数值、时间、目标过滤、执行模板、效果组合和表现资源引用。配置不能关闭请求校验、身份追踪、统一效果入口、终态幂等和重置清理。

稳定失败原因使用标识而非只写自由文本：NotInitialized、InvalidCaster、SkillNotGranted、InvalidTarget、StateBlocked、InsufficientResource、Cooldown、ConfigurationError、ActivationFailed、Cancelled、Interrupted、Expired、EffectFailed。
