# Batch04-02 空间数据导出工具

## 任务状态

- 状态：R1—R9 原实现问题均经独立定向复查确认修复，包括 R2 Notify Runtime-generation 过滤及其专门自动化。完整需求验收仍未完成，尤其缺有效单位目标、Fireball/BurningBuff 独立寿命、RelatedActor、其他角色复用及若干异常运行场景证据；这些验收范围与本轮代码缺陷复查结论分开记录。
- 需求首页仍标注“需求讨论稿，供用户审阅”。该状态与本轮明确执行授权并存；本文按需求正文作为本轮实现依据，不把首页状态解释成需求缩减或用户已完成需求审阅。
- 当前范围：交付自动启动 SkillLab、inspect 查询和 capture 实际技能释放的脚本工具及 UE 运行侧采集能力，依需求提供数据、画面、失败状态和可复现验证。
- **场景保真复跑（有效证据）**：[`20261117T033844-6a2e47ba`](../../../../../../Saved/SpatialExports/20261117T033844-6a2e47ba/manifest.json)，使用旧双视角 Mend 请求，正式 `Skill_Mend` 自疗受理、RHI 双视角与 marked PNG 共 5 组、2 Hz；`configuration.json.scene_preservation` 记录两个 NavTest 障碍均 `hidden_in_game=false`、Actor/组件 collision enabled、组件可见，`skill_lab_dummy_tag_count=0`。`samples.jsonl` 的 `000001` 明确为 `before_request`，请求相对时间为 JSON null；周期之间即时记录 Mend feedback 的创建和 `LifeSpanExpired` 移除事件，并导出四个可见 FramePieces 的 bounds/父级/比例/半径基准。GPU 自身帧号仍未暴露。
- **场景保真结论撤销**：旧 run [`20261117T024528-f0b7ce0b`](../../../../../../Saved/SpatialExports/20261117T024528-f0b7ce0b/manifest.json) 的 PNG 与同步机制材料仍有效，但 Review 已确认该 run 触发 TestArena，隐藏障碍并生成三个训练目标；它不再具备“原场景未改变”或“未生成假人”的证据资格。
- **R5 inspect 复跑**：[`20261117T033532-9897d60f`](../../../../../../Saved/SpatialExports/20261117T033532-9897d60f/manifest.json) 成功读取当前玩家和技能；references 为 89 bones、5 sockets，`weapon_r` 只列在 bone，`weapon_r_muzzle` 只列在 socket。inspect 未生成 events/samples/frames/marked，并把零覆盖时间写为 null。
- 地面目标验证：[`20261117T022506-9bf57ef8`](../../../../../../Saved/SpatialExports/20261117T022506-9bf57ef8/manifest.json) 对 `Skill_AreaBurst` 正式提交地面位置并返回 `complete`；技能事件是 `AreaEmpty`，证明请求与采集链通过，不证明有效单位效果。
- 异常参照验证：[`20261117T024956-ab2a22c6`](../../../../../../Saved/SpatialExports/20261117T024956-ab2a22c6/manifest.json) 对不存在的 Mesh/socket 路径返回 `partial`，保留可用画面并把同一查询错误去重为单条。
- 必需单位目标缺失：[`20261117T023320-43193b43`](../../../../../../Saved/SpatialExports/20261117T023320-43193b43/manifest.json) 对不存在的精确目标路径运行 Fireball 请求，正式 Runtime 返回 `TargetInvalid`，工具 `failed`，没有自动选目标或改用其他 Actor。
- 必需施法者缺失：[`20261117T023941-32cc15ba`](../../../../../../Saved/SpatialExports/20261117T023941-32cc15ba/manifest.json) 的玩家索引无 Pawn，工具在 UE 启动后约 0.8 秒内收束为 `failed`，只输出一条错误，不等待 120 秒墙钟超时。
- 事件完整性：capture manifest 逐行校验 events/samples JSONL，并要求受理请求含 accepted `request_return`；缺行、坏 JSON 或事件追加落盘失败都不会标 `complete`。
- 当前关键缺口：未取得无修改场景前提下可精确定位的单位目标，因此 Fireball 投射物生成/飞行/碰撞、BurningBuff 独立效果寿命、RelatedActor 与有效单位目标尚无端到端证据；其他角色/内容复用和超时、无 RHI、请求拒绝等负例也尚未完整运行。不得用 AreaEmpty 地面样本代替这些验收项。
- 用户授权来源：本轮用户消息要求创建新的 SkillLab 程序 PM，并完整执行 Batch04-02，完成后向原 PM 回报；用户授权原话为“这个是工具的需求文档。嗯，你可以去派发新的PMH的，这这是个新的，不需要派发老的去执行这个功能了，这个功能它不确定要不要去改项目里的代码，反正让他去确定了，因为我要去睡觉了，所以说任务让他自己去完成。我明天起来验收”。

## 权威需求与共享约定

- 当前需求：[02 实际技能释放采集与导出工具](../../../../../Planning/CharacterPresentation/Batches/Batch04-OnDemandSpatialAdaptation/02-SpatialDataExportTools.md)
- 批次任务关系与共用边界：[Batch04 入口](../../../../../Planning/CharacterPresentation/Batches/Batch04-OnDemandSpatialAdaptation/README.md)
- 表现共享语义：[CharacterPresentation 基线入口](../../../../../Baseline/CharacterPresentation/README.md)

## 任务交接

| 产物 | 路径 | 状态 |
| --- | --- | --- |
| 架构交接 | [ArchitectureHandoff.md](ArchitectureHandoff.md) | 已完成源码核对与实现交接；运行验证记录于实现摘要 |
| 实现说明与验证 | [ImplementationSummary.md](ImplementationSummary.md) | R1—R9 修复证据已交付；R2 generation 自动化与构建证据已补；场景验收缺口另行记录 |
| 独立 Review | [Review.md](Review.md) | 原始问题记录保留；2026-11-17 定向复查确认 R1—R9 原实现问题均已修复；完整需求场景验收仍未完成 |

## 职责对话

- 架构：threadId `01a1e557-9b19-7893-bc15-c6141d5c1117`，hostId `local`；已完成复用能力、实现拆分、改动落点与验证入口的静态核对。
- 实现：threadId `01a1e55d-bb19-7893-bc15-c6141d5c1117`，hostId `local`；R1—R9 修复及复现证据见 [ImplementationSummary.md](ImplementationSummary.md)。
- Review：threadId `01a1e5b5-2347-7d51-aee8-a8aaad4e7207`，hostId `local`；原始 Review 与 2026-11-17 R1—R9 定向复查结论见 [Review.md](Review.md)。
