# 测试边界与综合测试独立 Review

## 结论与范围

2026-11-14：本轮 F2、R1、R2 的代码与定向验证通过，未发现阻塞问题。新增测试注册名尚待同步至项目测试入口文档；本结论不代表整批整改、完整测试全集或玩家体验验收完成。

本 Review 使用独立于实现者的上下文，只读检查实际源码、差异和既有证据；没有修改被审实现，没有重新构建、运行自动化或操作 UE 编辑器。本轮唯一写入产物是本报告。

已读取 `ue-project-context`、`ai-native-programming`、`ai-program-review`、项目 `Docs/README.md`、`ProjectContext/README.md`、`ProgramArchitectureContract.md`、`Modules/TestAndVerification.md`，以及同目录 `TestBoundaryHandoff.md`、`TestBoundarySummary.md`。只读差异检查前另读取了 `git-repository-management`。

工作树存在其他整改批次的未提交变更。审查归属按交接、摘要、实际差异及拆分前快照区分，不把整个工作树差异归于本任务；未重新审查其他批次的生产修复。

## 核对结果

### F2：测试适配退出生产实现

- `Source/SkillLabSkills/Public/SkillLabSkillBehavior.h:46` 保留正式 `RequestDeclaredAction(int32)`，`:74` 只声明带 `FPointExecution&` 的生产执行重载；无参 `ExecuteDeclaredAction()` 声明及实现均已移除。
- `Private/SkillLabSkillBehaviorProduction.cpp:9` 的带参重载相对当前 Git 基线没有业务改写，实际差异仅删除旧无参适配。`Private/Tests/SkillLabProductionFixture.h:118` 接回原有的定位提交点、赋予 production handle、记录结果与调用生产重载步骤，位于文件的 `WITH_DEV_AUTOMATION_TESTS` 内，没有另写效果、费用、冷却或投射物规则。
- `SkillLabSkillBehavior.h:59` 和 `SkillLabSkillRuntimeComponent.h:118` 将原综合测试/latent 友元访问归于测试目录的 `FSkillLabCastScenarioFixture`；生产头保留访问声明，不包含测试实现。正式 Lua 请求接口没有因删除适配而消失。
- `Saved/Logs/TestBoundaryEditorProbe.i:848702` 存在 ProductionFixture 实现，`:848814` 调用真实带参重载，`:848854` 展开宏为 1；Shipping 探针只保留 friend 声明，`:776970` 宏为 0，未包含夹具实现。Shipping 的正式请求和执行声明分别见 `TestBoundaryShippingProbe.i:776759`、`:776787`；生产执行定义见 `TestBoundaryShippingProduction.i:796801`。两种探针均无已删除无参重载。
- `.rsp` 引用实际 Editor Development / UnrealGame Shipping 模块共享参数、强制包含头和定义头。真实 Editor UHT 执行记录位于 `TestBoundaryBuild.log:55` 起；该早期完整构建后来有 C++ 失败，不能当作最终成功构建。最终成功由 Delivery 构建补足。Shipping UHT 和实际 Game 编译链接也有独立记录，非测试边界不是由 Editor 结果推断。

### R1/R2：按场景拆分与清理

原 `TestBoundaryOriginalCastGameplayEvent.cpp` 确为 903 行。当前端到端文件为 85 行，六个职责测试分别为 149、169、166、100、49、249 行，共享场景夹具为 90 行。拆分后的首读范围为对应场景、内容声明和已有组装夹具，无需通读全部场景；没有新增生产框架或第二套生命周期。

核对 `TestBoundaryAssertionCoverage.json` 的 225 项与实际源码：224 条旧标签可定位，原 `Automation world exists` 由 `SkillLabCombatTestFixture.h` 的 `Independent combat World exists` 替代。进一步比较断言表达式及相邻触发过程，主要变化是私有观察改经夹具、共享定义加夹具限定、窗口场景使用自己的 `WindowCast`；未发现仅留标签而移除触发或状态断言的情况。

关键覆盖仍成立：

- `SkillLabCastGameplayEventTests.cpp:17` 起经正式 Provider、Runtime 请求、GAS Ability 与有身份事件执行，保留裸事件拒绝、错误/重复事件隔离、一次效果/费用、命中中断不退款；`:70` 起保留 Reset 后单调身份及旧 Notify/旧窗口结束隔离。
- Timing、Unconfirmed、Settlement、RuleWindow、Control 文件分别保留原资源门控/重试/重入、未决与迟到回调、结算终止、规则窗口和控制操作场景。请求及实际效果继续走生产接口；必要的私有访问和故障注入集中在测试夹具，没有替代业务执行。
- `SkillLabCastDeferredIntentTests.cpp:180` 起保留自然完成前的正式请求和缓存；完成动作前捕获 StartedCount，再在 `:193` 检查无同步新启动，语义比旧常数 10 更独立。latent 命令 `:31` 起使用真实 TimerManager tick，`:50` 先检查执行存在，再核对新 RequestId、Cast、Ability、旧身份拒绝及消费时资源重检。
- `SkillLabCombatTestFixture.h:13` 的既有目录 RAII 保存内容和 load-attempt 标志；`:40` 的 World/Actor 清理先于目录成员析构。ProductionFixture `:54`、`:163` 同样先清理 World，再恢复目录，修正原先提前恢复目录的顺序。
- latent 命令持有共享夹具，析构先移除捕获自身的回调，随后释放夹具；成功、返回失败和超时返回均归于同一析构路径。`SkillLabCastDeferredIntentTests.cpp:199` 起的 Cleanup 回归实际检查活动 Cast 的提前返回、未执行命令释放、World context 移除和目录/标志恢复。目录仍是进程全局状态，当前串行执行证据不能升级为同进程并发安全。

## 验证证据复核

以下路径均相对 `ue/SkillLab/Saved/Logs/`，均为既有运行，本 Reviewer 未重跑。

| 证据 | 独立核对结果 |
| --- | --- |
| `TestBoundaryBaselineBuild.log`、`TestBoundaryBaselineTests.log` | 构建 Succeeded；拆分前 10 项 Test Completed 均 Success，含交接指定关联测试；退出状态 0 |
| `TestBoundaryDeliveryBuild.log` | 实际工程 `SkillLabEditor Win64 Development` 构建 Succeeded，重新编译职责测试并链接实际 Skills DLL |
| `TestBoundaryGameShippingBuild.log` | 实际 `SkillLab Win64 Shipping` 执行 UHT、编译原生模块、链接 `SkillLab-Win64-Shipping.exe`，Result Succeeded |
| `TestBoundaryDeliveryGameBuild.log` | 最终增量 Game Shipping 构建 Succeeded，实际 exe 重新链接 |
| `TestBoundaryDeliveryTests.log:496` | 实际命令请求八项整理后测试及关联回归；SubmissionFeedback 前缀实际展开为 Identity、Routing 两项 |
| 同日志 `:2194` 至 `:5636` | 实际统计 25 项 Success、0 项失败；交接要求的 ControlAndRules、RequestReentry、IntentLifecycle、SubmissionIsolation、MultipleSubmissions、ResetResourceCompletion、ProductionRecords 和反馈测试均在其中，且保留 F1/F3 关联回归 |
| 同日志 `:1269`、`:5664` | 从实际工程 `Binaries/Win64/UnrealEditor-SkillLabSkills.dll` 加载模块；最终退出状态 0 |
| `TestBoundaryDeliverySourceHashes.json` | 本 Reviewer 重新计算当前 234 个 Source 文件及 Skills/GAS 两个 DLL 共 236 项 SHA-256，全部与清单一致，0 项缺失或变化 |

当前 Skills DLL 为 `DCFE6633FB5080BCBE0C9DB60B42100B1BFBEA9143C2E994E6B3BC7F6425F29F`，GAS DLL 为 `590A3ABEC2D895E09D395E6438E58280E6989CF6A4337F151214B78FE1BDD4BC`，均与摘要相符。该复核支持当前文件与交付快照的对应关系，不额外声称清单之外的配置、资产或插件均已验证。

两次自动化启动均可见 13 条 `Condition failed`；最终目标测试随后全 Success。摘要已如实说明，不能据此称整个日志无错误。构建进程退出 0 采用实现摘要记录，构建日志独立支持 Succeeded；自动化退出 0 另有日志中的 RequestExitWithStatus 支持。

## 非阻塞交接缺口与限制

`Docs/ProjectContext/Modules/TestAndVerification.md` 的 Current Behavior 注册表尚未列出新增的 `CastTimingAndReservations`、`CastUnconfirmedReentry`、`CastSettlementTermination`、`CastRuleWindowTransitions`、`CastControlTransitions`、`CastDeferredIntent`、`CastFixtureCleanup`。摘要已指出该待同步项，当前可通过摘要和源码定位，不阻塞本轮代码接受；PM 应在最终项目文档维护时补齐注册入口及本轮证据引用。最小复查为对照七个新增注册名和最终日志，确认入口文档没有把本轮定向通过写为全集通过。

120 帧超时没有被人为触发，清理只取得实际提前返回/命令释放证据与析构路径静态依据。没有执行可见游戏、PIE、完整自动化全集、Shipping Cook/打包/启动或玩家验收；Shipping 证据只支持原生编译链接。以上限制与实现摘要一致，不作为本轮新增阻塞要求。
