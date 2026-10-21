# Batch01 角色基础表现验证证据

只记录可复现验证入口、结果摘要、证据位置和未验证项。不要把完整编译日志、运行日志或历史调试过程复制到这里。

## 当前证据索引

本批已经有分切片验证证据，但尚未形成整批最终验收包：

| 切片 | 证据 | 能证明什么 | 不能证明什么 |
| --- | --- | --- | --- |
| A | `Saved/Logs/SkillLabAnimationVisual-Acceptance.log`、`Saved/AnimationStateVisual/`、`Saved/AnimAssetVerify.log` | 默认角色 AnimInstance 消费快照，五态状态迁移和基础姿势烟测已执行 | 迁移前/迁移后对照、主观视觉质量和技能结束后的恢复 |
| B | `Saved/AnimationReports/copy-write-readback.json`、B 工具文档中的只读/负例报告 | 临时资产副本的结构化读、写、保存、重载、回读和部分错误校验 | 复杂 Notify、多片段精确定位、不兼容 Skeleton 全覆盖 |
| C | `Saved/Logs/SkillLab-C-PresentationSmoke-20261018.log`、`SkillLab-C-CastHandoff-20261018.log`、`SkillLab-C-Automation-Editor-20261018.log` | 四技能公开请求、动作开始、效果事实、CastId 隔离和自动化测试 | 缺失资源降级、Notify 精确时点、取消后姿势恢复、PIE 可视质量 |

## 证据使用规则

- 编译、资产存在和静态读回不能单独证明视觉表现通过。
- NullRHI 日志只能证明运行链或事件，不证明屏幕上的姿势质量。
- C 的 `SkillPresentationSmoke` 是测试适配入口，不能替代玩家输入路径，也不能扩展为新的运行时权威。
- 缺口关闭后，应在本目录补充最小的入口、输入、结果、证据路径和未覆盖项；不要复制完整日志。
