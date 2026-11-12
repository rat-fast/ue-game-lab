# Cast 执行、多提交点与集成验证交付

## 实际工程交付修复（2026-11-12）

用户反馈 HUD 与技能操作缺失后，核对用户会话日志 `E:/agent/game/ue/SkillLab/Saved/Logs/HUDInputRegression_UserSession_20261112.log`：旧二进制拒绝目录字段 `combatRules`，快捷键收到后因技能定义缺失无法继续。用户保存并关闭编辑器后，对实际 `E:/agent/game/ue/SkillLab/SkillLab.uproject` 执行 SkillLabEditor Win64 Development 构建，`E:/agent/game/ue/SkillLab/Saved/Logs/HUDInputDeliveryBuild.log` 记录 Succeeded、退出 0；未复制隔离工程 DLL 或修改生产配置。实际 `Binaries/Win64/UnrealEditor-SkillLabSkills.dll` 更新至 2026-11-12 22:08:46，SHA-256 `8126AE03637123E3565B91DF6E891B858E4AB55F69CB5E99A0D131BAAEAA807A`；实际游戏进程模块检查确认从该工程 `Binaries/Win64` 加载 Skills、GAS、Presentation 和游戏模块。

实际工程 DX11 `-game` 日志 `E:/agent/game/ue/SkillLab/Saved/Logs/HUDInputDeliveryGame.log` 记录四技能定义加载成功、1–4 快捷键绑定及 HUD Snapshot `Skills=4`。实际游戏窗口截图 `E:/agent/game/ue/SkillLab/Saved/Logs/HUDInputDeliveryWindow.png` 可见血量、能量和四个技能槽。真实按键 3 进入 Mend 并完成提交；按键 2 与左键确认已到达输入入口，当次光标未投射到地面，不能据此声称该次区域技能成功。用户随后人工反馈“技能能放出来”。

本次证据支持实际工程旧二进制造成的定义加载回归已修复，且 HUD 已实际显示；用户人工确认技能可释放。完整键鼠组合、PIE、双 Montage 并播及全部可见体验未全面验证。本轮未重跑全面自动化或资产审计，未提交或推送。

## 最终运行与可见验收（2026-11-12）

当前交互会话有 1920×1080 屏幕，桌面捕获可读，但核对时前台已有其他全屏交互程序正在运行；为避免抢占焦点及向该程序误发输入，本轮未启动或操作可见 SkillLab 窗口，也未启动 Unreal Editor PIE。桌面探测截图仅用于能力核对，已删除；无本次 SkillLab 可见截图。`-game` 和 PIE 的画面、实际键鼠、动画姿势及 Notify 视觉时序均待人工验收，不把日志当画面证据。

为补最终版本生产运行链，在独占工程执行现有 `UnrealEditor-Cmd.exe <独占工程>/SkillLab.uproject -game -nullrhi -unattended -nosplash -nosound -nop4 -SkillLabRuntimeProbe -abslog=<独占工程>/Saved/Logs/FinalProductionRuntimeProbe.log`。进程自行结束、退出 0，日志记录四个生产技能请求及各自提交：Fireball 目标 Health 100→60、AreaBurst 60→35，Mend 在满血时实际治疗 0，BurningBuff 产生 3 次各 8 点周期伤害；后续 Fireball 提交前取消，Reset 记录 Applied/ResetCompleted、Energy 100、Idle 和火球冷却 0，Probe COMPLETE。该记录是 NullRHI 正式内容运行样本，不能证明可见动作质量、玩家输入或 PIE。启动中 NiagaraToolsets Python 报缺少 `unreal.NiagaraToolset_Info`，随后 Probe 实际运行；不能称全日志无错误。

运行前后生产和隔离副本的 Source/Config/Content 与已接受 `ResetImmediateRejectedHashes.json` 均为 416 文件且 SHA-256 差异 0，uproject 两份哈希均为 `E2C37AECEABA523273FFFD7001604A306EEB412DB7192259672C6D4C1BBFDC4F`；复用该快照及已构建二进制，不覆盖旧日志。新 Probe 日志 SHA-256 为 `E7710BE5158CD351E945A470D819CB6C636A096F969A88AF8CE7D539E4B6EF96`。未修改生产代码、配置或资产，验证进程已自行退出。

最小人工验收：

1. 空闲桌面运行 `ue/SkillLab/Run_SkillLab_DX11.bat`，这是独立 `-game` 窗口而非 PIE。聚焦游戏，使用控制台 `SkillSpawnTargets`，再分别用快捷键 1/2/3/4（或 `SkillTest`/`SkillArea`/`SkillTestRecover`/`SkillBurn`）选择并确认目标，观察四技能动作、释放时点、效果及资源/冷却；选点以鼠标左键确认、Escape 取消预览。施法中执行 `SkillReset`，检查取消、动作停止及后续输入。
2. 分别运行 `SkillAnimationSmoke`、`SkillPresentationSmoke` 观察既有序列；后者序列结束会主动退出进程。保存对应画面和日志，并将人工观察与日志事件身份对应。
3. 单独用 Unreal Editor 打开 `SkillLab.uproject` 和默认测试地图，进入 Play 后重复四技能、终止与 Reset；单独标注 PIE 证据，不能把 `-game` 或 NullRHI 作为 PIE。

四个现有 Montage 共用 DefaultGroup/DefaultSlot；可检查各自动作与普通终止，不能以这些资产证明不同组双 Montage 同时播放。兼容双组资源和对应可见并播证据仍缺；本轮未新增或修改技能、Slot、AnimBP 或资产。未重跑资产审计或全面回归。

## Reset 复审补证（2026-11-12）

针对 [CastExecutionReview.md](CastExecutionReview.md) 末节三类缺口，生产实现未修改，只扩展 `Source/SkillLabSkills/Private/Tests/SkillLabResetResourceCompletionTests.cpp`。以下不可达判断限定于当前已核对的正式资源入口和有效会话，不扩展到任意原生旁路或未审计内容。

**无未决的立即资源 Rejected：已实测可达。** 原“立即不可达”判断错误：`CompleteReset` 取得 ResetEnergy 后的 Health 同步委托可以合法改变资源和建立独立预留。`SkillLab.Skills.ResetResourceCompletion` 先用真实技能建立已确认冷却，将 Health 降至 40；正式 Reset 获取旧 MaxEnergy=100 后，Health 委托通过 `RequestResourceChange` 将 MaxEnergy/Energy 均设为 200，并通过 `BeginEnergyEscrow` 建立 150 预留。旧目标 100 因低于预留被 GAS 拒绝，回执及 `ResetResourceChangeRejected` 实际各记录一次。测试断言 Health 委托一次、Energy 仅发生委托中的一次合法写入，实际/最大/预留/可用分别保持 200/200/150/50，已有冷却不被清除，另一技能请求证明屏障释放，预留仍可正常释放；未重取 Max、重试或补偿。未发现需要修改生产实现的缺陷。

**排队后资源 Rejected 的限定分析**：`SubmitResourceChange` 在 ResourceWriteDepth 内入队，外写结束后 `DrainPendingResourceChanges` 移出项并重新执行相同校验。排队期间 `BeginEnergyEscrow` 因写入深度或非空队列拒绝新预留；先于 Reset 的合法 MaxEnergy 变更不能降至使现有预留失效，因而 Reset 目标按执行时 MaxEnergy 钳制后仍不低于已有预留；增大 MaxEnergy 也不会使旧预留超过入队时目标。协调写入禁止非有限值。该分析已由原 Reviewer 在有限、无聚合、有效会话和无原生旁路条件下接受；不外推到非法初始属性、运行中移除 AttributeSet 或未审计资产。ASC 会话结束是 Unavailable；Runtime/generation 失效或新增未决是许可或未决阻断，不等同资源 Rejected。

新增测试的私有工程证据位于 `C:/Users/Administrator/AppData/Local/Temp/SkillLabCastExecutionValidation/Saved/Logs/`：`ResetImmediateRejectedBuild.log` 构建成功退出 0，`ResetImmediateRejectedValidation.log` 的 ResetResourceCompletion 实际 Success、1 tests performed、退出 0；启动仍有既有 Condition failed。`ResetImmediateRejectedHashes.json` 冻结 416 文件（Source 224），生产与副本哈希差异 0；相对 `ResetEvidenceFinalHashes.json` 仅 Reset 独立测试文件不同。清单、构建日志、运行日志 SHA-256 依次为 `7A0A64A063F094D9D37FDBD5288EB90DE749493A261793BAF405E054C220ACA1`、`89A88F5FBFE9BB637D619D1E8522ECAD81B5B2878EBA73F966D8D85AFE4FCE9A`、`6368BC28DC034F29071D2C7ACE911AC91BCA4D51FEE21E45B08CFB4257CCBCD9`。构建沿原 Build.bat 入口，运行仅 `Automation RunTests SkillLab.Skills.ResetResourceCompletion`；待原 Reviewer 关闭此项。

**取消收束时新增未决**：新增实际注册的 `SkillLab.Skills.ResetSubmissionDuringCompletion`，正式 Cast/Begin 后产生真实生产记录，在提交仍进行时调用 Reset，再丢失结果进入 Indeterminate；验证 Reset 等待收束，记录先交接、随后只报告一次 `ResetBlockedByUnconfirmedSubmission`，唯一预留 2、实际能量 100、同技能隔离且未提前启动冷却；对真实生产记录重复查询后只扣一次至 98、释放预留并启动冷却，Reset 不再次终结。直接链为 `AbortBehaviorSubmission` → `StartIndeterminateWait` → `FinishSubmissionTermination` → `CompleteReset`（`SkillLabSkillRuntimeSubmission.cpp:175`、`SkillLabSkillRuntimeUnconfirmed.cpp:13`、`SkillLabSkillRuntimeExecution.cpp:529`）。

**成功路径重入**：现有正式成功回执由 GAS 直接调用 Runtime 的 `OnResetResourceFinished`；成功分支仅清确认冷却、`FinishResetAttempt` 锁终态并写日志，没有对外成功委托或调用方代码，因而不存在可由产品监听者在该回执内部启动新 Attempt 的入口，不能伪造回执或将属性委托当作最终回执。实际可重入的是 `SetNumericAttributeBase` 内的 Energy 变化委托，发生于资源写保护内、最终回执之前。测试在该委托内重复 Reset 和发起技能请求，验证一个写入、请求仍被 Resetting 屏障拒绝、回执后新请求成功；另一用例在该委托内销毁 Runtime，确认实际写入保留、旧接收者失效且替换 Runtime 可请求。旧栈不会清新 Attempt 的分析限于当前成功分支没有外部回调；若未来新增成功通知，须补通知内新 Attempt 的运行测试。

私有工程 `C:/Users/Administrator/AppData/Local/Temp/SkillLabCastExecutionValidation/Saved/Logs/` 中 `ResetEvidenceFinalBuild.log` 构建成功退出 0；`ResetEvidenceFinalValidation.log` 两项 ResetResourceCompletion、ResetSubmissionDuringCompletion 逐项 Success、退出 0。启动仍有既有 Condition failed。`ResetEvidenceFinalHashes.json` 416 文件（Source 224），生产与隔离快照差异 0；相对原 Reviewer 接受的 `ResetResourceFinal3Hashes.json` 仅上述测试文件变化。清单、构建日志、运行日志的 SHA-256 依次为 `0B11F9F4BCBFF25B88486899C95178803A6AEDBF717A5A48B0A271EF9C0C2DBA`、`E7ABD8D61BB08BF59483C87D65EAC69701CC9D2319A469D105FC36223C45154E`、`4835E46FC422DFA17A4EC78060D3096502B4A0C994B106C7A3F92521A059A48D`。构建和运行沿下节原入口，RunTests 名称替换为上述两项。旧 8 项、资产和 PIE 未重跑；待原 Reviewer 判断补证是否满足合同。

## Reset 资源完成接入（2026-11-12）

按 [VerificationHandoff.md](VerificationHandoff.md) 消费 R 的 `RequestResourceChangeTracked`。Runtime 在取消前登记 Attempt、原 ASC 和 generation，清理推进 generation 后绑定，资源请求前设置等待态；`BeforeApply` 弱校验原 Runtime、ASC、Attempt、generation 和未决隔离。Queued 保持屏障，最终回执按身份收束；仅 Applied 后清理已确认冷却。失败以 Rejected 原因报告，EndPlay 立即失效旧 Attempt；不补偿 Health/效果、不自动重试、不伪造 Cast 完成。Reset 成功和回执原因日志关联 Attempt/generation。未改 GAS、生产内容或资产。

`SkillLab.Skills.ResetResourceCompletion` 经正式 ResetSkills 覆盖立即应用、Health 委托中重复 Reset/请求屏障、真实资源委托内排队及重复 Reset/请求屏障、旧 Runtime 排队时销毁替换且新 Cast 先启动、Actor 销毁、ASC EndPlay 导致 Unavailable 及失败通知重入。另以明确的原生旁路故障注入干扰真实资源写入，验证 Failed 仅报告一次且不补偿实际副作用；该故障注入不是合法资源写入场景。现有 `UnconfirmedSubmission` 覆盖首次及已计费未决跨 Reset 的隔离/冷却，`LateSubmission` 覆盖 Reset 受阻后的真实迟到收敛。

私有验证目录 `C:/Users/Administrator/AppData/Local/Temp/SkillLabCastExecutionValidation/Saved/Logs/`：`ResetResourceFinal3Build.log` 构建成功、退出 0；`ResetResourceFinal3Validation.log` 的 ResetResourceCompletion、UnconfirmedSubmission、LateSubmission、CastRequestReentry、EnergyLedger、EnergySettlementHistory、ResourceWriteIntegration、ResourceExecutionBoundary 共 8 项逐项 Success，进程退出 0。启动日志仍含既有 Condition failed，不能称整份日志无错误。`ResetResourceFinal3Hashes.json` 冻结 416 文件（Source 224），生产与快照 SHA-256 差异 0；已消费 R 最终 GAS 源码，未覆盖历史快照和日志。快照清单 SHA-256 为 `1049706A67CAA6CBE2E46D2F7D3588ABD82A5849E33DF8748E2B96CF7EAF8A70`，构建日志 `07D1E8628DFCA6DFBC25E33AB75D6E70F521CF8F5B4C7C7E0A2A5A1C68562C6F`，测试日志 `BB47738F74E609C113FA8461FB09AE7109679F3AC47C1CCA5DBE0AE681E865B9`。

以上为首次接入时的历史验证范围；该轮留下的三项缺口及后续复查结果以上方“Reset 复审补证”为准。资产审计和可见 PIE 未重跑，未提交或推送。

## P2-CE-1 修复交付（当前，2026-11-12）

已按 [CastExecutionReview.md](CastExecutionReview.md) 的唯一阻断修复请求重入边界，待原 Reviewer 定向复查。未开展全面重审。以下原 23 项结果保留为修前已接受证据；本轮新增的构建、15 项运行与哈希证据独立列于本节，不将旧结果当作修后全套运行。

**修前复现**：新增 `Source/SkillLabSkills/Private/Tests/SkillLabCastRequestReentryTests.cpp`，通过共享正式 fixture 解析目录及 Lua、正式请求和真实 Ability 完成事件，执行 A 限制期间缓存 C → A Completed 监听请求 D → C 的 BufferedIntentReplaced 通知启动 E。`CastCE1BeforeValidation2.log` 实际运行 `SkillLab.Skills.CastRequestReentry`，结果 Fail：E 分别选择 Reject/Buffer 时，D 仍出现一次 Started、一次提交并扣 7 能量（应保持 97，实际 90）；通知中完成 Reset 后，D 仍启动并将能量从 100 扣到 93。该进程退出 0，故修前失败依据逐项断言，不能用进程码冒充通过。

**修复原因与落点**：仅修改 `Source/SkillLabSkills/Private/SkillLabSkillRuntimeRequests.cpp` 的请求编排，不改 B1、GAS、生产内容或资产。

- 请求进入时保存 RuntimeGeneration。缓存拒绝通知、闪避清槽及取消通知返回后，检查对象/Owner/ASC/World 有效性、Reset/EndPlay 屏障与原 generation；一次同步 Reset 已完成、bResetting 恢复为 false 时，旧请求仍因 generation 改变而失效。
- 将旧缓存的清理放到正式取消计划执行前。清槽同步通知后重新调用同一 `BuildActivationPlan`：新 E 的 Reject 直接拒绝 D；Buffer 将 D 放入角色单槽等待；新计划明确授权的取消来源才会被取消。不能沿用通知前的 Allow 或旧取消列表。
- 普通取消通知后再次仲裁，拒绝/等待按当前事实处理；若回调又产生新的取消来源，结束本请求并给出 OperationReentered，不循环追逐和取消新执行。正式激活前也复核会话、同技能未决隔离和请求 ID。
- 回调若已写入新的缓存意图，保留该槽并以 OperationReentered 结束较早请求，不重复清槽、覆盖更新意图或递归重试。整个编排无重评估循环；后续资源/冷却/目标仍通过原正式门控。

**针对性验收**：`SkillLab.Skills.CastRequestReentry` 修后覆盖五种通知回调：启动 Reject E、启动 Buffer E、Reset、真实 DestroyComponent/EndPlay，以及启动 Buffer E 后重新缓存较新的 C。断言 D 未提前 Started、未施加效果/提交或计费；E 保持原执行；Reject/Buffer 原因明确；D 仅在 Buffer E 自然结束后消费；旧 C 仅收到一次替换通知且不会复活；较新的 C 保留并只按新请求身份消费一次。EndPlay 后失效接收者不再要求收到 native 通知，外层返回 OperationReentered；Reset 同样使原请求失效。

同一私有验证工程 `C:/Users/Administrator/AppData/Local/Temp/SkillLabCastExecutionValidation/` 的新证据，均在 `Saved/Logs/`：

| 证据 | 实际结果 |
| --- | --- |
| `CastCE1BeforeBuild2.log` / `CastCE1BeforeValidation2.log` | 修前新测试单元实际编译；1 项运行 Fail，证实旧计划越过 Reject/Buffer/Reset。首次未收录测试的 0 项日志不作为复现证据 |
| `CastCE1FinalBuild.log` | SkillLabEditor Win64 Development Succeeded，退出 0 |
| `CastCE1FinalValidation.log` | 15 项逐项 Success，15 tests performed，退出 0 |
| `CastCE1BeforeSourceHashes.json` / `CastCE1AfterSourceHashes.json` | 修前/修后独立冻结清单；修后 409 文件，其中 Source 217 文件 |
| `CastCE1Verification.json` | 修前 Fail、修后 15 项 Success、退出码、生产/副本/清单三方一致性与基准差异 |
| `CastExecutionAccepted23SourceHashes.json` / `CastExecutionAccepted23Verification.json` | 原 Reviewer 已接受的 408 文件与 23 项结果备份；原构建及运行日志保留 |

修后 409 份 Source/Config/Content 文件与生产工作区、隔离副本、修后清单三方 SHA-256 一致，工程描述相同。相对原已接受 408 文件，代码仅有 Requests.cpp 修改及新回归测试一份；本摘要为本轮唯一文档更新。

本轮保留并通过的正式目标为 CastRequestReentry、SharedCastArbitration、CastExecutionOwnership、CastSubmissionIsolation、CastIntentLifecycle、CastGameplayEvent、MultipleSubmissions、ProductionRecords、SubmissionReconciliation、LateSubmission、UnconfirmedSubmission、AreaSubmissionSettlement、SubmissionFeedbackRouting、SubmissionFeedbackIdentity，以及 Presentation.SubmissionIdentity。完整前缀名称见 `CastCE1Verification.json`。构建沿用下文入口；自动化将该 15 个 name 以 `+` 连接，使用原 NullRHI/Automation/TestExit 入口即可复现。

本轮未重跑不受影响的纯解析/GAS 算法、资产回查、生产 Probe 或可见运行；旧资源/PIE、暂停审计及独立 Review 边界继续保留。代码修复与定向验证完成不替代 Reviewer 对 P2-CE-1 的关闭判断。未提交或推送。

## 原合并执行交付（修前 23 项证据保留）

2026-11-12。合并执行范围（原 A+B2+C2）已完成本次代码接入与命令行功能验证，可交独立 Review。最终 Editor 构建成功，23 项正式自动化均为 Success，进程退出码 0。可见 PIE、兼容双 Montage 资源及暂停的资源写入审计仍需由 PM 接续，不据此宣布整批玩家验收完成。

## 依据与修改范围

读取项目 Docs/README、ProjectContext/README、ProgramArchitectureContract，UE 上下文、模块实现及 AI Native 程序 Skill；需求权威来源为 [SkillCombatRulesRequirements.md](../../../../Planning/SkillSystem/Batches/SkillCombatRules/SkillCombatRulesRequirements.md)，尤其第 2–8.2 节。实现消费本批 TimingRulesHandoff、SubmissionEffectsHandoff 的执行合同、VerificationHandoff 的 C2、测试入口文档及技能/表现共享基线。定义解析合同见 [SubmissionDefinitionImplementationSummary.md](SubmissionDefinitionImplementationSummary.md)。第 8.2 节已定案，无产品待决。

本次修改 Skills 的 Runtime/Execution/Ability、Behavior/生产动作/Projectile/Executor/效果观察，以及 Presentation 的 Animation/Notify/Lua 宿主和必要测试。公共 Types/Provider/解析及其定义测试由 B1 独占交付；发现的两项解析测试兼容问题已交其修复，并消费其实际落盘结果。GAS 账本、会话未决协议、生产 Lua/JSON、二进制资产和历史批次未由执行线改写。未提交或推送。

稳定执行机制和点调度使用 C++，Lua 组合经校验的声明与公共调用，JSON 沿用 B1 的唯一规范化入口；新增场景均用测试目录/脚本文本，未新增生产技能或另一套资源/生产账本。

## 执行所有权与仲裁

- `USkillLabCastExecution` 独占每 Cast 的 Definition/Request、Lifecycle、Ability/Behavior、窗口、Timer、提交、延迟终止和成功效果索引。Runtime 维护执行集合、角色共享单槽意图、状态订阅及 Reset/EndPlay 屏障；候选请求不覆盖已有执行。
- 使用 B1 的 `FSkillLabCastExecutionToken { Runtime, RuntimeGeneration, CastId }`。Ability 为 `InstancedPerExecution/LocalOnly`，激活通过 spec+token 短寿命绑定；真实 GameplayEvent 校验原 Runtime、Ability、token 与 Instigator。同步广播处保护执行对象，并按集合中的原身份重新核对。
- 所有仍有效 Cast 共用 `BuildActivationPlan`：Reject 优先；没有 Reject 而有 Buffer 时写角色级 latest-wins；全部允许后再经过正式门控。自然结束只解除本 Cast 的限制，安全点重评估全体及资源/冷却/目标。闪避先取消全部 Cast，再尝试激活，失败不恢复；普通请求只取消明确授权 CancelAndActivate 的来源。普通窗口冲突仍为 RuleWindowConflict。
- Reset/EndPlay 等待正在提交的执行交接完成，再清理各自资源。未确认记录继续归既有会话，不因上下文结束释放预留或重发动作。

公开窄入口位于 `Source/SkillLabSkills/Public/SkillLabSkillRuntimeComponent.h`：

```cpp
FindExecution(const FSkillLabCastExecutionToken&)
FindExecutionByCastId(int32)
GetExecutionTokens()
GetSingleExecution()
CancelCast(const FSkillLabCastExecutionToken&, FName)
ForwardValidatedNotify(const FSkillLabCastExecutionToken&, FGameplayTag)
ForwardValidatedNotifyStateEnd(const FSkillLabCastExecutionToken&, FGameplayTag)
CancelBehavior(const USkillLabSkillBehavior*, int32, FName)
```

`GetSingleExecution` 和旧无参观察仅在恰有一个执行时有值；多个执行返回空/0/None/Idle，不隐选最新项。`CancelActiveCast` 为角色级取消调用时全部执行；内部单 Cast 操作用 token。Behavior 的 Begin/Resolve/Abort/GetProductionHandle/ApplyBehaviorEffect 仍用原 Behavior+Cast 合同，内部同时核对 generation。

Runtime 按所有权查询、请求/缓存、规则、生命周期、提交、未决等待和确认拆 cpp；相关新增单元为 `SkillLabCastExecution.h`、`SkillLabSkillRuntimeOwnership.cpp`、`SkillLabSkillRuntimeRequests.cpp`、`SkillLabSkillRuntimeRules.cpp`、`SkillLabSkillRuntimeExecution.cpp`。

## 多提交点与独立动作

- Behavior 仅保留一张点表。`SkillLabSubmissionPointExecution.cpp` 统一核对 token、声明、Release/Timer/GameplayEvent/Script 证据，再调用既有 Begin→实际动作→Resolve/Abort。点成功、等待、未知和终态不能重复；明确失败可显式重试，产生新 operation，无自动重试。
- Lua 的 RequestAction/ApplyEffect 共用点入口和返回事实；无参仅兼容隐式单点。显式点 ID 在 int64→int32 前检查范围，不能以溢出数字借用已声明点。Lua 调用结束时有合法定时/事件/异步等待不误报 ScriptNoFinish；完成检查整组点，全部失败保留失败原因。
- Runtime 按 Cast+point+目标去重。不同点可对同一目标施加真实 GE；首次成功沿原账本计费/启动冷却，后点 transaction=0，生产键各自独立。同步资源写回重入另一 Cast 时，GAS 对当前资源写入中的新预留仍可明确拒绝；拒绝属于新点，不破坏原提交，也不伪称它已成功。
- `SkillLabSkillBehaviorLua.cpp` 负责 Lua 宿主，`SkillLabSkillBehaviorProduction.cpp` 负责实际单体/范围/投射物动作。`SkillLabSubmittedEffect.cpp` 仅接受已确认交付、未消费的 Projectile 能力，经共享 `ApplySelected` 和 GAS Resolver 结算，不依赖当前 Cast，不再次计费或改交付回执。
- Projectile 保留原 token/point/生产键及定义、Source/Target/ASC 弱引用；先记录交付再开放碰撞。每点独立 Actor；同来源动作不在共同出生点互相阻挡。命中、阻挡、过期和外部销毁收束自身并报告原事实。普通 Cast 清理只销毁未交付动作；已交付动作解绑旧 Behavior 后继续，Runtime 替换不能借新 Cast 身份。
- 持续 GE 归目标 ASC。普通完成/取消后继续周期与自然移除；Reset 显式移除跟踪效果；EndPlay 只解除观察，不移除仍有效 GE，也不伪报 BuffRemoved。

## 表现与共享测试

Animation 按完整 token 记录实际 AnimInstance、Montage 与 InstanceId；Notify 同时核对真实实例、资源、Mesh 和原执行。停止使用实例 ID，仅停止原播放实例。Lua 生产脚本的 int Play/Stop 命令由宿主附加当前回调 token，回调结束恢复原归属；原生回退直接传 token。此适配不向表现层开放业务裁决。

`SkillLabCombatTestFixture.h` 统一真实 World/ASC/Runtime、正式目录解析/恢复、请求、Ability 事件和 Tick。旧 ProductionFixture/未决/GameplayEvent 测试迁到唯一执行观察；终态交易断言保存原交易身份，不在清理后读取不存在的执行。

## 最终验证与复现

独占验证工程：`C:/Users/Administrator/AppData/Local/Temp/SkillLabCastExecutionValidation/SkillLab.uproject`。最终 Source/Config/Content 共 408 文件，其中 Source 216 文件，与生产工作区、隔离副本及冻结 manifest 三方 SHA-256 一致，差异均为 0；工程描述相同。原已接受验证工程未被覆盖。

证据均位于该验证工程 `Saved/Logs/`：

| 证据 | 结果 |
| --- | --- |
| `CastExecutionFinalBuild.log` | UE 5.8 SkillLabEditor Win64 Development Succeeded，退出 0；使用 `-NoUBTMakefiles` 确保新增测试单元实际收录 |
| `CastExecutionFinalValidation2.log` | 23 tests performed，逐项 Success，退出 0 |
| `CastExecutionAccepted23SourceHashes.json` | 原 408 文件 Source/Config/Content 哈希备份 |
| `CastExecutionAccepted23Verification.json` | 原三方一致性、完整 23 项结果与退出码汇总备份 |
| `CastExecutionProductionProbe.log` | 四生产技能动画/Notify/Ability/真实效果、周期和 Reset；COMPLETE，退出 0。执行于最终测试补充前的构建 11，不作为最终全源码快照验证替代 |
| `CastExecutionFireballReadback.json` / `.log` | 正式只读 commandlet：valid、assetUnchanged、saved=false，退出 0 |

新增/扩充行为验证：

| 正式测试名 | 实际覆盖 |
| --- | --- |
| `SkillLab.Skills.CastExecutionOwnership` | 正式同 spec 双实例、候选门控失败隔离、错误真实 GameplayEvent、外部 Ability End、动态事件完整 token、共享能量竞争、真实 Notify 超时/清理/迟到事件、Runtime 替换 |
| `SkillLab.Skills.SharedCastArbitration` | 旧 Recovery 与新 Allow 共同限制、单槽替换/持续等待/新身份消费、闪避成功和失败均取消全体 |
| `SkillLab.Skills.CastControlAndRules` | 旧 Reject 与新 Allow、窗口冲突、真实普通/重击标签、移动、两个真实 GE 来源的沉默/硬直、共享规则集中修改及差异配置 |
| `SkillLab.Skills.CastIntentLifecycle` | 独立准备结束仍等真实 Release、缓存消费重查资源、Reset 清槽、真实致死伤害 |
| `SkillLab.Skills.MultipleSubmissions` | 正式四触发点同目标、错点/错触发/重复/显式无参/Lua ID 溢出、唯一费用/冷却、后点交易 0、整组自动完成 |
| `SkillLab.Skills.CastSubmissionIsolation` | 实际能量回调启动第二个同 spec 多点 Cast、原提交隔离、明确预留拒绝、双 Timer、真实关系失败/显式重试/新 operation、取消 Timer、目标 Actor 销毁后点失败/不计费/收尾 |
| `SkillLab.Skills.SubmittedActionLifetime` | 多个真实 Projectile、同步出生 overlap 自毁、交付后取消/Runtime 替换命中、真实阻挡/过期/外部销毁、Source 销毁、交付记录不变/不重扣；真实持续 GE 在普通完成/取消后周期、自然移除、Reset 与 EndPlay 差异 |
| `SkillLab.Presentation.SubmissionIdentity` | 原反馈 ID/请求身份保真；真实 Lua 回调 int 停止命令保持完整 token，同号旧取消不移除替换 Runtime 的播放绑定（适配器级） |
| `SkillLab.Presentation.CastAnimationResources` | 实际只读加载四个生产 Montage，各有唯一释放 Notify，并回查 group/slot/长度 |

B1 的 `SkillLab.Skills.SubmissionDefinition` 已在同一最终进程实际 Success，覆盖其正式解析、反射复制及坏配置测试，关闭“仅编译未运行”证据缺口。

受影响的原十四项全部保留且通过：CastLifecycle、CastGameplayEvent、DefinitionValidation、EnergyLedger、EnergySettlementHistory、DamageCalculation、ProductionRecords、SubmissionReconciliation、LateSubmission、UnconfirmedSubmission、AreaSubmissionSettlement、SubmissionFeedbackRouting、SubmissionFeedbackIdentity，以及表现 SubmissionIdentity。完整带前缀名称见验证 JSON。先前失败日志保留作修复过程，不作为当前通过结论。

复现构建：`D:/Epic/UE_5.8/Engine/Build/BatchFiles/Build.bat SkillLabEditor Win64 Development <ProjectPath> -WaitMutex -NoHotReloadFromIDE -NoUBTMakefiles`。自动化使用 `UnrealEditor-Cmd.exe <ProjectPath> -unattended -nop4 -NullRHI -nosplash -nosound "-ExecCmds=Automation RunTests <以+连接验证JSON内23个name>" "-TestExit=Automation Test Queue Empty" -abslog=<独立日志路径>`。生产 Probe 使用同工程 `-game -SkillLabRuntimeProbe -NullRHI -unattended -nop4`，等待自身 COMPLETE/退出；Fireball 回查使用本批 VerificationHandoff 的只读 commandlet 入口，不加 Write。

## 限制与后续职责

- 四个生产 Montage 都是 `DefaultGroup/DefaultSlot`；释放时间为 Fireball 0.366667、AreaBurst 1.200000、Mend 0.500000、BurningBuff 0.533333 秒。同 group 由引擎替换视觉播放，业务 Allow 不提供第二个动作 group。没有资源兼容的双动作可见证据；需要时由内容/表现职责配置，不能称任意双 Montage 已验收。
- 生产 Probe 是 NullRHI 的真实运行链证据；Lua 停止测试是适配器级绑定验证。均不证明可见 PIE、动作质量或真实双实例 Montage 的视觉效果。
- 沉默/硬直由测试中真实 GE 授予；本次没有新增可游玩的生产授予者。普通/重击测试经权威 ASC 标签入口，不从伤害数值推断强度；具体生产攻击者的类别选择仍按项目内容核实。
- 日志启动阶段仍有既有 `LogAutomationTest: Error: Condition failed` 与插件/SDK 噪声，不能称日志完全无错误。23 项结论依据实际 Test Completed/队列总数/退出码，非只读进程 0。
- 本次保留已接受未决账本/迟到身份回归，没有重做资源写入审计或全面质量 Review；不承诺跨进程恢复，不替代需求第 8.1 节整体及玩家验收。
- Review 后需按项目文档职责同步 Runtime 多执行所有权、点调度、已交付动作寿命、表现 token 接口及测试入口的当前代码事实；本交付只更新本批摘要。
