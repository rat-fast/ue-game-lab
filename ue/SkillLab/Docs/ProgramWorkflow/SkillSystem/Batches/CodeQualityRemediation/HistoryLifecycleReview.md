# 终态历史与未决记录整改 Review

## 结论

**F5 可按“未决活动扫描收敛”这一已交付范围关闭。** 当前改动解决了 SL-02 中闭合未决记录仍每帧参加扫描的问题，同时保留身份去重、待反馈、迟到确认和查询所需的会话历史。未发现阻止本批关闭的实现问题。

此结论不表示生产结果、能源结算历史或所有会话记录已变为有界。它们的安全回收时机仍取决于尚未定义的消费者终止契约；在契约明确前继续保留是正确行为。长期内存上限及帧耗时没有测量，本次也未重新构建或运行测试。

## 核对依据

- 需求与业务约定：`Docs/Baseline/SkillSystem/02-RequestLifecycle.md`、`05-OwnershipAndCleanup.md`，以及 `Planning/SkillSystem/Batches/SkillCombatRules/SkillCombatRulesRequirements.md` 第 8.1、8.2 节。尤其 8.1 要求未决事实跨 Runtime/Reset 保持隔离、迟到结果绑定原身份、不得猜测结果或重复结算；对象失效时需保留可诊断事实，但不承诺向失效对象恢复或通知。
- 当前任务边界：`HistoryLifecycleHandoff.md`；实现说明：`HistoryLifecycleSummary.md`。
- 项目规范与验证入口：`ProgramArchitectureContract.md`、`Modules/TestAndVerification.md`。
- 实际核对了 `SkillLabUnconfirmedSubmissions.*`、`SkillLabSubmissionReconciliation.cpp`、`SkillLabSubmissionResults.*`、相关 Query 调用方、ASC 能源事务接口，以及 `SubmissionReconciliation`、反馈路由、迟到结果、Area、Projectile 和能源历史测试。

## 评审发现

### 无阻塞发现：活动集与保留历史分离

`ActiveRecordIndices` 只收录仍待结算或仍待反馈的稳定数组下标。Tick 先快照活动下标，再逐项复查成员资格；同轮回调新增记录不会被重入扫描，已结算但反馈接收方暂不可用的条目仍保持活动。反馈成功后才移除活动索引，`Records` 本身不压缩，因此 `GetRecords()`、待反馈索引及其他原下标读取者不受移动影响。

`BlocksSkill`、`HasUnconfirmed` 和失主诊断现在遍历活动集。每条记录的 `bReportedLostOwner` 与单调会话诊断位替代按历史下标累积的 `ReportedLostOwnerRecords`，既避免闭合记录继续参加诊断扫描，也保留 `HasLostResourceOwnerDiagnosis()` 的历史查询结果。测试覆盖同步回调追加、闭合后仍可查询、未决期间保持隔离、收敛后活动集清空等边界。

### 保留历史的接口约束

- **生产结果与 Area 子结果**：`FSkillLabSubmissionResults::Query(Key)` 以完整身份查询结果。当前调用方包括执行器、Runtime 提交/执行/未决路径和 `SkillLabSubmittedEffect`；已交付 Projectile 可在 Cast 结束后延迟命中，Area 父结果需依赖 `Children` 聚合。当前没有消费者释放或租约结束信号，不能证明任意旧操作在固定时间后绝无查询。因此保留 `Results` 与父子结果是必要的。若未来要求有界，PM 需确定句柄消费者何时完成最后查询，并覆盖 Projectile 与 Area 子项；或明确改变查询承诺。
- **能源结算历史与已用 ID**：`QueryEnergySettlement(TransactionId)` 被未决对账读取，`BindEnergyOperation` 还需核对历史快照；结算历史支撑迟到确认的幂等闭合。公开 `BeginEnergyEscrow(TransactionId, ...)` 接受调用方 ID，`UsedEnergyTransactionIds` 因而不能只依靠自增分配器防止 ID 复用。若未来裁剪，需先限制为 ASC 独占分配，或定义明确的历史/会话终止边界。
- **未决记录本体**：`Records` 继续追加，且 `Record()` 的重复身份检测仍扫描该历史。这是低频交接写路径，不是逐帧活动对账；本批没有足够规模证据支持再维护一份等量无界身份索引。若该路径未来成为实际瓶颈，再按证据设计稳定身份索引或回收契约。
- `AllocatedEnergyTransactionIds` 是已分配但尚未 Begin 的临时 ID，正常 Allocate→Begin 会移除；它与用于闭合后防重的 `UsedEnergyTransactionIds` 语义不同，不应混为一谈。

上述限制是需要 PM 决定的后续接口契约，不是 F5 当前活动扫描收敛的阻塞项。任何裁剪都不得用 TTL、固定容量、Reset 清空或猜测迟到事件已消失来替代生命周期证明。

## 验证证据与限制

实现摘要记录 UE 5.8 对实际工程的 `SkillLabEditor Win64 Development` 构建成功。`Saved/Logs/HistoryLifecycleValidation2.log` 中指定 10 项自动化均为 `Success`，随后记录 `Automation Test Queue Empty 10 tests performed` 和 `TestExit`；覆盖生产记录、活动对账、Area、迟到确认、反馈身份/路由、已交付动作寿命、能源结算历史和资源写入边界。本 Review 只读核对了该日志和当前源码，没有重新构建或运行。

规模测试构造 128 条已闭合和 128 条未决记录，验证未决收敛后活动索引为 0、`Records` 仍有 256 条且结果仍可查。它是集合规模与扫描成员验证，不是 128 次真实施法的性能基准；没有帧耗时或内存上限证据。日志启动阶段存在通用 `Condition failed` 等引擎输出，但所选 10 项均报告成功，退出状态为 0。

## PM 后续决策

F5 可关闭，关闭范围限于未决活动扫描收敛与其身份/反馈边界。生产结果、能源结算和终态记录继续保留；若要求这些账本有界，先由 PM 确定生产句柄的消费者释放/最后查询时点，以及能源事务 ID 的唯一分配者或历史终止边界，再建立对应需求与验证范围。
