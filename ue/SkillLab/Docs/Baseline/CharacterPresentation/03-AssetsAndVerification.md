# 资产与验证边界

## 职责

说明资产/工具存在、程序运行记录和玩家可见验证之间的证据差别，不冻结内容生产工具或资源规范。

## 当前承诺

- 资产文件、软路径、代码播放分支或内容工具成功，只证明其各自可定位/执行的范围，不证明完整视觉链已通过。
- 自动化、构建和 NullRHI 运行记录可支持相应局部代码/事件结论；它们不能替代玩家可见 PIE/视口验收。
- 视觉验收应分别覆盖目标体验、实际动作/状态、施法结果与清理，不能由某一条运行日志推定全体验通过。

## 当前实现与验证证据

- `a2c5969` 是原基线静态盘点所用历史快照，不是当前实现指针。该提交中定位到的资产路径和内容工具能力不自动延伸到当前未提交工作树。
- 2026-10-22 CharacterPresentation 程序交接记录 UE 5.8 Development 构建通过、`CastLifecycle`、`DefinitionValidation`、`DamageCalculation` 三项局部自动化成功及 NullRHI RuntimeProbe 局部观察；这些记录未在本轮重跑。
- 当前工作树中程序源码、配置和程序交接有未提交修改；当前稳定代码状态未定。交接将 P0/P1 标记为未通过，缺可见 PIE/视口验收、Fall 专门检查、旧/新 Cast 竞态复现和差异化技能可见反馈验收。
- 玩家反馈游戏内表现与改造前基本一致，为有限人工观察，不是可复现视口验收记录。

## 当前限制与待确认

未有本轮可见证据证明基础五态完整视觉、四技能差异化动作反馈、释放时点对齐、取消/完成后的姿势恢复、资源缺失降级的玩家可辨识度或竞态处理。正式 VFX、音频、受击表现和资源方案仍是候选/待确认，不纳入当前承诺。

## 关系

程序状态和后续证据入口见 [CharacterPresentation Lua 试点程序入口](../../ProgramWorkflow/CharacterPresentationLuaPilot/README.md)；体验验收条件见 [Batch02 策划入口](../../Planning/CharacterPresentation/Batches/Batch02-LuaCharacterPresentationPilot/README.md)。
