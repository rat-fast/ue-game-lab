# 测试边界与综合测试整改实现摘要

## 范围与依据

完成 `TestBoundaryHandoff.md` 的 F2、R1、R2 实现及定向验证，供独立 Review 核对；不以本实现摘要代替独立复查结论。

读取了 `ue-project-context`、`ai-native-programming`、`ai-program-module-implementation`，项目 `Docs/README.md`、`ProjectContext/README.md`、`ProgramArchitectureContract.md`、`Modules/TestAndVerification.md`，本次测试交接、指定综合测试/夹具/行为源码及直接依赖；核对了 `BehaviorFixSummary.md` 中 F1/F3 完成和验证范围。工作树检查按 `git-repository-management` 执行。

开工时已有 GAS、提交结算、生命周期、所有权及 F1/F3 等多个批次的未提交改动，均保留。本次只修改下列边界：

- `Source/SkillLabSkills/Public/SkillLabSkillBehavior.h`：删除无参测试重载声明，将旧综合测试 friend 改为测试访问夹具 friend。
- `Private/SkillLabSkillBehaviorProduction.cpp`：删除无参测试重载实现；带 `FPointExecution&` 的生产重载原样保留。
- `Public/SkillLabSkillRuntimeComponent.h`：仅将原综合测试和 latent 命令的两个 friend 收束为 `FSkillLabCastScenarioFixture`；其他批次已有修改保留。
- `Private/Tests/SkillLabProductionFixture.h`：迁入测试动作适配，复用已有目录快照 RAII，并调整到 World 清理后恢复目录。
- `Private/Tests/SkillLabCastGameplayEventTests.cpp`：保留端到端事件烟囱及旧身份隔离，拆出其余场景。
- 新增 `Private/Tests/SkillLabCastScenarioFixture.h` 及下表六个职责测试文件；未修改现有 `CastControlAndRules`、`CastRequestReentry`、`CastIntentLifecycle` 或其他批次测试。

## F2：测试适配与生产边界

无参 `ExecuteDeclaredAction()` 已完全从生产类声明和实现移除。原适配步骤在 `WITH_DEV_AUTOMATION_TESTS` 内的 `FSkillLabProductionFixture::Execute` 中完成：定位真实提交点、赋予真实 production handle，然后调用未改变的 `ExecuteDeclaredAction(FPointExecution&)`。夹具没有重写伤害、投射物、范围查询、费用或冷却规则；失联测试仍执行真实动作，仅丢弃同步返回交付。

Lua 正式调用的 `RequestDeclaredAction(int32)`、提交点状态机和生产动作重载保持正式职责。必要的私有观察、故障注入和旧投射物回调访问集中到测试目录内的 `FSkillLabCastScenarioFixture`，不为每个拆分测试新增生产友元或测试 API。生产头文件保留的 friend 只有访问声明，不包含测试适配函数体。

使用实际 MSVC 响应文件的共享参数、强制包含头和模块定义运行了预处理；仅去掉对象/PCH 输出及依赖日志写入参数，并加 `/P /Fi`。证据位于 `Saved/Logs/TestBoundary{Editor,Shipping}Probe.rsp/.i/.log`，探针源为 `TestBoundaryCompileProbe.cpp`：

| 检查 | Editor Development | Game Shipping |
| --- | --- | --- |
| 展开的 `WITH_DEV_AUTOMATION_TESTS` | 1 | 0 |
| `FSkillLabProductionFixture` 类实现及动作适配 | 存在 | 不存在（仅有生产头中的 friend 声明） |
| 正式 `RequestDeclaredAction(int32)` | 存在 | 存在 |
| 生产 `ExecuteDeclaredAction(FPointExecution&)` | 存在 | 存在 |
| 已删除的无参生产重载 | 不存在 | 不存在 |

实际 Editor 构建执行 UHT 并成功；实际 Shipping Game 也完成原生模块编译和可执行文件链接，未仅由 Editor 构建推断非测试兼容。

## R1/R2：场景迁移与清理

原 903 行综合文件保留为 85 行端到端测试。其余断言按独立责任迁移，而非按名称认定已有覆盖后删除。测试内容声明集中在 90 行的 `SkillLabCastScenarioFixture.h`，仍经正式 Provider 解析和替换；夹具复用既有独立 World/ASC/Runtime 组装，没有另一套业务状态机。

| 注册名（均以 `SkillLab.Skills.` 为前缀） | 当前职责与来源 |
| --- | --- |
| `CastGameplayEvent`（保留原名） | Provider → Runtime 正式请求 → GAS Ability → 有身份的 GameplayEvent → 效果/一次费用；裸事件、错误/重复事件、Recovery 缓存、分类命中中断与不退款；Reset 后单调 Cast 身份、旧 Notify/旧窗口结束隔离、取消窗口清理 |
| `CastTimingAndReservations`（新增） | 即时/独立准备与释放、可用能量门控、效果失败与重试、提交/结算重入、第二点不重复费用/冷却、受控写入与 escrow 观察、失败事务只释放自己的预留 |
| `CastUnconfirmedReentry`（新增） | 非受控写入导致未决、迟到完成/投射物回调不能结束未决 Cast 或消费缓存、Lua Action/Effect 交叉入口、结算中取消/污染、Lua 同步取消/Reset、直接 Effect 未决 |
| `CastSettlementTermination`（新增） | 扣费回调排队资源写入、非法直接写入的未决账本、效果回调取消，以及效果/扣费中的 EndPlay、Reset、预留与队列收束 |
| `CastRuleWindowTransitions`（新增） | 声明缺省冲突策略/重复窗口拒绝、扩展类别、状态多计数及窗口内重评估、共享 profile 编辑、规则窗口冲突/关闭/重复 begin |
| `CastControlTransitions`（新增） | 控制阻断/打断与移除后恢复、Dodge 正式激活/新身份/资源拒绝、不恢复旧 Cast |
| `CastDeferredIntent`（新增） | 移动/用户取消不打断、latest-wins、取消/Reset/死亡清槽；自然完成先结束旧 Ability，次 tick 激活新请求/身份；消费时重新执行资源门控并拒绝 |
| `CastFixtureCleanup`（新增，同 DeferredIntent 文件） | 真实活动 Cast 的提前返回；latent 命令独占持有夹具，释放未执行命令后销毁 World/Actor，恢复目录内容及原 load-attempt 标志 |

旧综合测试静态提取的 225 条断言标签，224 条在上述测试或共享加载中原名保留；`Automation world exists` 替换为既有夹具的 `Independent combat World exists`。`Saved/Logs/TestBoundaryAssertionCoverage.json` 逐条映射旧行号到当前文件/行号；`TestBoundaryOriginalCastGameplayEvent.cpp` 保存拆分前源码。标签映射仅辅助检查，验收依据还包括实际请求路径、状态断言和运行结果。

自然完成前的 `StartedCount == 10` 改为与完成动作前捕获的计数比较，继续断言旧 Ability 结束时没有同步启动新 Cast，消除对其他无关场景执行次数的依赖。latent 消费保留真实 TimerManager tick，且检查新执行存在后再解引用。

所有场景使用独立 World。目录仍是进程全局状态，本轮沿同一 Automation worker 队列串行运行，不宣称同进程并行目录隔离；并发验证应使用独立进程。已有 `FSkillLabScopedTestCatalog` 在夹具构造前保存目录及 load-attempt 标志，在 Actor/World 清理后恢复。latent 命令持有共享夹具至结束/销毁，并先移除回调；同步返回、提前返回、命令释放和超时的析构路径都走同一清理职责。提前返回和命令释放已实测；没有人为触发 120 帧超时，也不声称已覆盖真实超时运行。

## 验证证据

均使用实际工程 `E:/agent/game/ue/SkillLab/SkillLab.uproject`，日志根目录为其 `Saved/Logs/`：

| 证据 | 结果 |
| --- | --- |
| `TestBoundaryBaselineBuild.log` / `TestBoundaryBaselineTests.log` | 拆分前实际 Editor 构建成功；原综合测试及交接指定关联测试共 10 项 Success，退出 0 |
| `TestBoundaryDeliveryBuild.log` | 最终实际 `SkillLabEditor Win64 Development` 构建 Succeeded、退出 0 |
| `TestBoundaryDeliveryTests.log` | 最终同一次定向执行 25 项 Success、0 Fail，退出 0；明确从实际工程 `Binaries/Win64` 加载 Skills 模块 |
| `TestBoundaryGameShippingBuild.log` / `TestBoundaryDeliveryGameBuild.log` | 首次完整及最终增量 `SkillLab Win64 Shipping` 构建均 Succeeded、退出 0；链接实际 `SkillLab-Win64-Shipping.exe` |
| `TestBoundaryDeliverySourceHashes.json` | 234 个 Source 文件及 Skills/GAS 两个实际 DLL 共 236 项 SHA-256；运行结束后复核 0 项变化 |

最终 25 项为上表八项，加 `CastControlAndRules`、`CastRequestReentry`、`CastIntentLifecycle`、`CastSubmissionIsolation`、`MultipleSubmissions`、`ResetResourceCompletion`、`ResetSubmissionDuringCompletion`、`ProductionRecords`、`SubmissionFeedbackIdentity`、`SubmissionFeedbackRouting`、`DeathReset`、`SubmissionEventTimeout`、`SubmittedActionLifetime`、`SubmissionReconciliation`、`LateSubmission`、`UnconfirmedSubmission`、`AreaSubmissionSettlement`。F1/F3 新增回归及直接受 ProductionFixture 影响的主要入口均保留。

最终 Skills DLL SHA-256 为 `DCFE6633FB5080BCBE0C9DB60B42100B1BFBEA9143C2E994E6B3BC7F6425F29F`，GAS DLL 为 `590A3ABEC2D895E09D395E6438E58280E6989CF6A4337F151214B78FE1BDD4BC`。早期构建中修正过拆分产生的 C++ 类边界、局部变量遮蔽、弱指针及 SharedRef/SharedPtr 编译错误；最终证据以 Delivery 文件为准，不把中间失败记录隐藏成全程绿色。

复现构建：

```powershell
& 'D:/Epic/UE_5.8/Engine/Build/BatchFiles/Build.bat' SkillLabEditor Win64 Development 'E:/agent/game/ue/SkillLab/SkillLab.uproject' -WaitMutex -NoHotReloadFromIDE
& 'D:/Epic/UE_5.8/Engine/Build/BatchFiles/Build.bat' SkillLab Win64 Shipping 'E:/agent/game/ue/SkillLab/SkillLab.uproject' -WaitMutex -NoHotReloadFromIDE
```

Automation 使用 `UnrealEditor-Cmd.exe`、实际工程、`-unattended -nop4 -NullRHI -nosplash -nosound`、`-TestExit="Automation Test Queue Empty"`，通过 `-ExecCmds="Automation RunTests ..."` 用 `+` 连接上述 25 个完整注册名；完整实际命令保留在 `TestBoundaryDeliveryTests.log` 的 Command Line 中。

## 交付限制

这次只改变 C++ 测试编译/访问边界、测试内容声明位置和清理归属，不改变生产内容的 JSON/Lua 权威入口、Blueprint 或模块运行职责。没有更新全局代码地图；新增注册名可由后续 `TestAndVerification.md` 文档维护一并同步。

未执行可见游戏/PIE、完整项目测试全集、玩家体验或 Shipping 打包启动。Shipping 证明原生编译与链接，不证明 Cook/打包内容。测试启动仍有既有 13 条 `Condition failed` 及环境告警，目标测试随后全成功，不称日志无错误。独立 Review 与最终批次集成结论交 PM 协调。
