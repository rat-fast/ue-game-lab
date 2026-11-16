# Batch04-01 独立程序 Review

## 结论

**复审结论：此前两项 P1 与一项 P2 均已关闭。** 当前代码、配置加载和验证证据映射清楚；近墙配置后起点与可见窗口仍未验证，因此不宣称四样本视觉验收完成。此次只读复核了更新后的实现摘要和对应证据位置；没有修改代码或 Config。

## 已关闭的阻塞

### Related 来源现在是独立关联对象

`FSkillLabSkillRequest::RelatedActor` 是弱引用；Runtime 将其随 Cast 请求快照传递到 `FSkillLabSkillResult`，Projectile 解析和 Presentation Router 分别使用该字段，不再把 Target 充作 Related。空间解析器拒绝无效 Related，并核对它与 Source 属于同一 World。`Batch04SpatialFinalWithConfigTests.log` 中 `SubmittedActionLifetime` 与 `Presentation.SpatialLifecycle` 均通过；测试创建与 Target 不同的关联对象，验证实际 Projectile Spawn、事件保留及表现对象定位。缺失 Related 不回退 Target 也有断言。原 P1 已关闭。

### 当前配置与最终运行证据已对齐

最终工作树构建记录 `Saved/Logs/Batch04SpatialFinalWithConfigBuild.log` 为成功（Target up to date；Related 修复的实际编译见 `Batch04RelatedBuild2.log`）。完整 5 项定向自动化在 `Saved/Logs/Batch04SpatialFinalWithConfigTests.log` 全部 Success，队列清空。其后 `Saved/Logs/Batch04SpatialFinalWithConfigProbe.log` 从实际工程加载 4 个技能，明确记录 `tempSpatialProjectile`、`tempSpatialGroundMarker`、`tempSpatialSelfMarker`、`tempSpatialBuffMarker` 的位置/比例并输出 COMPLETE。原 P1 已关闭；此前早于 Config 改动的日志仅作为历史证据。

## 剩余风险与交接缺口

- **Fireball 实时释放起点及近墙边界仍未证明。** Fixture 的 `SpatialShot` 覆盖空间偏移进入真实 Spawn，但没有在 Cast 开始后移动角色/模型或 Socket，再于真实 Release 事件采样。用户确认近墙配置后起点尚未验证；既有墙体阻挡测试不能替代该场景。保留 `AlwaysSpawn` 重叠出生及起点改变轨迹/遮挡的风险，不据旧墙体测试推断通过。
- **可见窗口仍未验收。** DX11 `-game` 进程因 Windows 锁屏无法操作或观察，且已停止；因此不对临时代理的可见性、屏幕空间位置/比例、BurningBuff 跟随观感或 Fireball 视觉方向/尺寸作通过判断。NullRHI Probe 和自动化是代码/加载证据，不代替可见游戏或 PIE 验收。`Saved/Logs/Batch04SpatialVisibleWindow.png` 的存在也不作为本次可见验收结论。
- **Related 负向边界测试尚不完整。** 代码明确拒绝无效/跨 World 的 Related；自动化覆盖缺失 Related 不回退 Target，但本次检索未发现专门的跨 World 自动化案例。可在后续定向复查中补一个跨 World Fixture，确认具体错误路径；目前这是覆盖缺口，不是已观察到的运行缺陷。
- **特定模型 Socket 尚未运行验证。** 当前四技能临时配置未指定生产 Socket；解析器的 Socket/Bone 唯一匹配逻辑没有经目标生产模型资源验证。

**P2 已关闭：** [ImplementationSummary.md](ImplementationSummary.md) 现已引用 `Batch04SpatialFinalWithConfigBuild.log`、`Batch04SpatialFinalWithConfigTests.log` 和 `Batch04SpatialFinalWithConfigProbe.log`，报告五项自动化全部通过、四个临时对象的位置/比例记录及 Probe COMPLETE，也注明 NiagaraToolsets 诊断与可见/近墙未验收限制。恢复步骤和临时配置参数亦已纳入摘要。

## 已核对事项

- Lua 声明对象名与 JSON `spatial` 键在解析时要求一一匹配；拒绝缺失/多余键、未知参数、无效类型、非有限数值和非正比例。JSON 结构化解析拒绝重复字段和无效 JSON；语法错误目前给通用 `invalid JSON`，定位信息较弱，但未发现其静默接受错误内容。
- 缺省空间配置保留原 Fireball 起点和旧表现路径；定义由 Provider 快照携带。显式配置无效的 Projectile 走既有失败确认分支；无效纯表现参照会记录警告并跳过对象。
- `Reference`、`World`、`ExecutionDirection` 偏移及方向分离由共享变换函数实现；Projectile 的可见缩放作用于子 Mesh，Sphere 碰撞根独立。样本 Fireball 的起点偏移在当前定义中沿施法者局部轴，方向仍为执行方向。
- AreaBurst 的 Ground 空间来源使用事件 `TargetLocation`，视觉 Marker 半径基于事件 `VisualRadius` 再应用独立 `visualScale`；该调用不写回请求或 TargetLocation。Mend Lua 仍声明 Self/Healing，临时 Marker 使用 Source，代码未见改治疗目标的分支。
- BurningBuff 的附着事件来自真实 `BuffApplied`；EffectAdapter 将 EffectInstance 标识复制到周期/移除事件，Router 按 `EffectInstanceId + object` 建立跟随标记，`BuffRemoved` 对应销毁；组件 EndPlay 解绑 Router 并清理跟踪对象。新增反馈 Actor 使用无碰撞显示组件，不参与结算。隔离自动化检查了双身份、移动跟随、移除与解绑。
- 四项临时值均足够醒目；本次 Config 差异仅增加 JSON `spatial` 数值及对应 Lua 声明/临时注释，未改 cost、cooldown、magnitude、range、radius、duration、period、提交点等范围外字段。各 Lua 注释与实现摘要均提供删除临时声明及 JSON 字段并 reload/重启的恢复步骤。
- 相关新增空间源/头、Router、FeedbackActor、Lua 解析、Entry 解析及测试文件均低于约 400 物理行。此次触及的 `SkillLabSkillRuntimeExecution.cpp` 为 532 行，但只增一行快照转递，属于既有大文件的局部接线，交接没有要求新增职责继续集中，因此本次不据行数单独阻塞。
- Planning 状态已同步：两个规划 README 将 01 标为已实施并说明四技能值仅作临时验证，将 02—04 保留为未实施。01 需求正文自身仍标注为修订稿/供用户审阅；这是需求文本状态，与已授权实施及程序交付记录并列保留，不据此改写需求权威来源。

## 文档交接建议

代码职责同步已完成：`SkillLabSkillsRuntime.md`、`CharacterPresentation.md`、`SkillLabCodeArchitecture.md`、`TestAndVerification.md` 及本批程序工作流 README 已记录空间能力、验证入口/范围和当前 Review 状态。Review 不再要求 PM 重复同步这些文档；后续仅在实现事实或状态变化时做对应最小更新。
