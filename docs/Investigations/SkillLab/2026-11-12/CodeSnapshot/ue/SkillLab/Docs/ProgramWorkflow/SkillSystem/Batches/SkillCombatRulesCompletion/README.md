# SkillCombatRulesCompletion 程序任务入口

需求权威来源：[SkillCombatRulesRequirements.md](../../../../Planning/SkillSystem/Batches/SkillCombatRules/SkillCombatRulesRequirements.md)，尤其第 2–6、8–9 节；旧批次仅作为历史实现和验证证据。

| 任务 | 架构交接 | 需求映射 |
| --- | --- | --- |
| 定义解析：原 B1 | [SubmissionEffectsHandoff.md](SubmissionEffectsHandoff.md) 的 B1 章节 | 第 2、5、8 节 |
| 执行交付：原 A + B2 + C2 合并 | [TimingRulesHandoff.md](TimingRulesHandoff.md)；多提交交接的 B2；验证交接的 C2，按阶段读取 | 第 2–8 节 |
| 资源写入及最终运行证据 | [VerificationHandoff.md](VerificationHandoff.md) 对应章节，代码恢复可编译后接续 | 第 7–9 节 |

用户要求按上下文重叠收敛任务，先完成现有需求，再另做全面质量 Review。本轮只恢复下列两条实现线；原五线安排停止适用。原 A/B2/C2 共读施法身份、提交/终止与测试观察接口，合并给同一负责人；B1 的定义解析上下文独立，保留并行。下文分组、修改所有者与派发状态优先于三份交接中的旧任务所有权说明，技术合同继续沿用。

普通窗口冲突保持 RuleWindowConflict。原需求负责人已将共同仲裁、缓存、闪避及独立效果规则写入唯一需求正文第 8.2 节，PM 已核对条款及验收场景；据此恢复下列两条实现线，原产品待决解除。必要构建和功能验证随实现完成，全面结构审查不在本轮展开。

## 当前推进状态（2026-11-12）

进入交付收尾：代码实现、专项独立 Review、资源审计和项目事实文档同步已完成。实际工程曾遗漏构建，旧技能DLL不接受新配置的combatRules字段；用户日志备份 `Saved/Logs/HUDInputRegression_UserSession_20261112.log`。用户保存关闭编辑器后，原A报告实际工程构建成功、新DLL已加载，四技能定义及快捷键日志正常，并已在实际游戏画面观察到血量、能量与四技能槽。用户随后明确反馈“我自己去测试了，技能能放出来”，据此记录技能释放人工验证通过。

本轮交付记录已收尾，不再追加开发或重复技能释放测试。原A已将实际构建、加载日志及HUD截图路径补入[原执行摘要](CastExecutionImplementationSummary.md)顶部；PM已核对构建日志Succeeded、实际Skills DLL的SHA-256、四定义加载与四快捷键/HUD日志，并查看截图确认血量、能量和四技能槽可见。用户反馈不外推为完整键鼠操作、PIE或兼容双组Montage并播验收；这些范围仍未完整验证。全面结构质量Review按用户原计划另行开展，不在收尾阶段自动启动。

| 工作 | 当前状态与负责人 | 交付及下一步 |
| --- | --- | --- |
| GAS 账本、Runtime/HUD、同会话未决所有权、有限等待、冷却重建及结算委托销毁 P2 | 原独立 Reviewer 已限定范围关闭 | 保留回归，不视为需求第 8.1 节整体通过 |
| 真实生产记录 → GAS 结算查询 → 会话迟到消费、隔离解除与冷却补报 | 原 Reviewer `01a1c634-ef31-7a32-85ca-0cda996fa5ad` 已接受本次交付范围；两项 P2 均关闭 | 十四项目标测试成功、Source 全部 194 份文件与隔离副本及快照一致；证据见摘要顶部及 `SubmissionFeedbackIdentityBuild.log` / `SubmissionFeedbackIdentityValidation.log`。不代表第 8.1 节或整批完成 |
| 定义解析（B1） | 原对话 `01a1cb69-3ed7-7541-9e5f-1af05427651c` 的修复已被最终集成消费，两项目标均实际 Success；本轮独立 Review 未发现阻塞 | 缺 GameplayEvent eventTag 的诊断及旧 Allow 负向用例修复后，`SubmissionDefinition`、`DefinitionValidation` 在最终23项中通过。PM已独立核对三份修复源码在生产、B1副本、最终集成副本及冻结manifest中的SHA256均一致；[定义解析摘要](SubmissionDefinitionImplementationSummary.md) 中“未运行/需补跑”仅指B1自有启动尝试，不代表最终集成仍缺证据，不据此重复测试 |
| 执行交付（A+B2+C2） | 原 A 对话 `01a1cb69-8cbc-72a1-8397-5958666dea20` 的P2-CE-1修复已通过定向复查，本次已审代码集成可接受 | [执行交付摘要](CastExecutionImplementationSummary.md) 顶部为当前修复。仅Requests.cpp及新CastRequestReentry测试两处代码变化；修前Fail、修后构建及15项Success证据在原独占日志目录 `CastCE1*`，Reviewer已独立核对。原408文件/23项证据另存 `CastExecutionAccepted23*`，保留版本区别 |
| 本次集成功能与证据独立 Review | 原 Reviewer `01a1c634-ef31-7a32-85ca-0cda996fa5ad` 已关闭P2-CE-1；在已审范围内通过 | [原审查报告](CastExecutionReview.md) 当前结论及定向关闭证据；保留资源审计、可见验收及事实文档缺口，不代表8.1整体或整批完成，不代替全面结构质量Review |
| 原 B2/C2 对话 | `01a1cb69-d2e4-77f0-b54b-082fda2ef903`、`01a1cb6a-3e4f-7e03-9545-482f2ce4273d` 保持停工 | 已落盘修改移交合并执行负责人，不继续编辑或启动验证 |
| 正式资源写入审计（C1） | 原审计对话 `01a1cb61-3863-7280-b6d9-0612ca38f548` 已完成[资源审计报告](ResourceWriteAudit.md)及两地图补证，本轮不再派发 | 149资产加载；两地图Actor无ASC，独立只读Editor C++调用GetLevelScriptBlueprint(true)均返回None，无现存图及其直接引用可追。`ResourceWriteAudit_20261112_LevelScriptRead.log` 第1854–1856行及完成退出0已由PM核对，报告记录内容同一与地图未改哈希。正式资源缺陷仍由R修复，不以审计完成称机制已修好；后续仅按实际改动补证 |
| 资源协调写入修复（R） | 原GAS实现对话 `01a1c147-ab69-7af2-8381-3b5936bcc903` 的交付已由A交叉验证，R+Reset专项独立审查通过 | [资源修复摘要](ResourceWriteImplementationSummary.md)及[审查报告](CastExecutionReview.md)末节；早期R副本三文件差异保留为历史边界，当前已接受的416文件组合及专项补证已独立核对，不再等待实现交付 |
| Reset结果衔接与GE兼容边界 | 原架构 `01a1c143-8242-7733-97e5-d13da66b7973` 已交局部合同，PM核对后已分别派原R与原A实施 | [验证交接](VerificationHandoff.md) 的Reset合同与GE兼容小节为当前依据：R先提供原队列的受跟踪最终回执与GAS验证，再由原A消费并跑正式Reset端到端。GE移除execution全局封禁，改为实际资源输出生效前veto，非资源保留；R独立完成并测试，不等待A。没有新的产品待决 |
| Runtime Reset消费 | 原Reviewer已关闭最后立即Rejected缺口，R资源修复与Reset跨层接入本轮专项验收通过 | [审查报告](CastExecutionReview.md)末节确认正式拒绝、预留/冷却/屏障及416文件一致；本轮仅测试变更，无生产缺陷修复。保留各版本运行边界，不称旧8项/2项本轮重跑；不再为已关闭专项扩测 |
| 生产端到端、可见 PIE/Montage | 最终NullRHI Probe通过；随后原A报告实际游戏HUD可见，用户确认技能能释放；完整PIE及双Montage验收仍未覆盖 | [原执行摘要](CastExecutionImplementationSummary.md)记录分轮证据；`FinalProductionRuntimeProbe.log`的COMPLETE/退出0与SHA256已由PM核对，运行前后416文件一致由作者报告。此前未启动可见运行的限制仅对应Probe当轮。四Montage同DefaultGroup/DefaultSlot，兼容双组资源及可见并播仍缺，不改资产凑证据 |
| 当前项目事实文档同步 | 原架构 `01a1c143-8242-7733-97e5-d13da66b7973` 已完成五份既有文档最小同步，PM已核对关键内容 | 更新代码地图及Skills/GAS/表现/测试模块文档，保留分版本证据、资源及可见范围；PM另补已核对的最终Probe入口。无源码/配置/资产变化，不重复构建测试 |
| 实际工程HUD/技能回归 | 实际构建成功、四技能定义及快捷键加载、HUD画面已由PM核对；用户人工确认技能能释放，交付记录已补齐 | 证据为实际工程Saved/Logs下的HUDInputDeliveryBuild.log、HUDInputDeliveryGame.log、HUDInputDeliveryWindow.png；实际Skills DLL SHA-256为8126AE03637123E3565B91DF6E891B858E4AB55F69CB5E99A0D131BAAEAA807A。原A已停止交互；完整移动、镜头、所有键鼠操作及PIE不据此宣称通过 |

程序 PM 对话为 `01a1c141-9845-7451-a590-98ca95590f9a`，以上对话均在 `local`。实现完成后主动向 PM 回报，PM 接续独立 Review 和剩余任务；派发成功不等于实现完成。跨进程持久恢复不在本次需求承诺内。

## 本轮上下文分配

所有程序任务先取得项目入口路由的适用规范；共用这些短规范不等于具体实现上下文高度重叠。判断合并看同一技术章节、代码符号、状态与反复交接的实际重叠，不只看文档路径同名。

| 负责人 | 本任务必读 | 源码与按需扩读 | 无需默认加载 |
| --- | --- | --- | --- |
| 定义解析 | 需求第 2、5、8 节及直接适用 8.1；多提交交接 B1 | Types、Provider 组装/Parse 入口、新定义/点/规则/Lua 解析单元、独立解析测试；只读消费者所需公开合同 | Runtime/Behavior 全文、三份交接全文、旧大测试及全部历史摘要 |
| 合并执行 | 需求第 2–8 节中的当前阶段；时序交接执行合同 → 多提交交接 B2 → 验证交接 C2，按阶段取得 | 先 Runtime/Execution/Ability 与必要 fixture 恢复编译；再 Behavior/生产/Projectile/Adapter 接点与异步；最后双执行/事件/表现及交叉验证。B1 只读已冻结类型与最小样本 | Provider 内部实现、原三对话长历史、未受影响 GAS 算法、全面结构审查材料 |

两条实现线以 Types/规范化候选合同对接，公共字段唯一由定义解析负责人修改；执行与共享测试在同一所有者内接通，不再反复派接口微任务。先完成需求所需修改，不为缩短文件另开全仓重构。

## 上下文控制规则

约 400 行是实现者首轮读取上下文的控制偏好，不是按行数机械拆文件的阈值。架构交接应为每个实现任务指定首轮必读的文件、符号/章节和直接依赖，使首轮代码上下文通常不超过约 400 行；只有调用链或共享状态证明必要时再按需扩读，并记录扩读原因。超过该规模且存在独立变化原因、输入/输出和验证入口的职责应拆到窄接口或独立测试文件；共享生命周期、身份、资源所有权或同步重入无法隔离时保留在同一编排边界，避免为减少行数复制状态或制造空壳。Runtime、Provider 和长自动化测试均按此规则评估，具体候选见各交接；相关仓库 Skill 已表达同一职责分层原则，本偏好是本批交接约束，不修改通用 Skill。
