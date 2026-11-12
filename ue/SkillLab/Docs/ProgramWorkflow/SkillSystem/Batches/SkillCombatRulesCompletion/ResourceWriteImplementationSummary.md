# 资源写入修复阶段交付

状态：GAS 回执与 GE 输出保护已实现并通过以下定向自动化；尚待独立 Review。Runtime 所有者正在接入 Reset，Reset 端到端和最新跨模块组合未在本验证副本验证。

## 实现与边界

- `SkillLabAbilitySystemResource.cpp` 按钳制后的目标值写 GAS base，并检查 current/base 与预留；保留现有唯一账本和结算语义。`SkillLabAbilitySystemEnergyReservations.cpp` 从原 ASC 实现拆出预留逻辑，不复制状态。
- `RequestResourceChangeDetailed` 区分 `Rejected`、`Queued`、`Applied`。新增 `RequestResourceChangeTracked` 接受调用方预建 AttemptId、弱所有者、BeforeApply 和 OnFinished；回执含原 ASC 身份及 Applied/Rejected/Failed/Unavailable。Queued 仅为受理，回调可以同步；队列出队及写入保护结束后回调，EndPlay/BeginDestroy 闭合未执行项。
- ASC 的引擎 `GameplayEffectApplicationQueries` 在普通 GE 应用前拒绝静态直接 Energy/MaxEnergy modifier。已移除对所有 execution 的全局拒绝；CombatAttributeSet 的 `PreGameplayEffectExecute` 按实际输出逐项 veto 资源修改，非资源输出继续执行。混合 GE 的非资源输出可能已生效，不保证整体回滚或句柄代表所有输出成功；calculation 自行调用原生写入或其他副作用不在此保护内。
- `RequestResourceEffect` 将纯即时、单资源 modifier 的 Additive/Override spec 转为协调数值请求；仅支持 ScalableFloat、SetByCaller，拒绝持续、复杂组件和其他不支持配置。它不是完整 GE 应用，不产生 GE handle、授予效果或执行其他 GE 语义。持续资源聚合在生效前拒绝。没有证明已有生产资源 GE；审计中的 GE 为测试夹具。
- `ClampVitals` 对非 Applied 结果记录未完成。Reset 的最终消费由 Runtime 所有者实现，本 GAS 验证不证明其端到端完成。

## 验证证据

- 最新 GAS/跨模块定向集合 `E:/agent/game/ue/SkillLab/Saved/Logs/ResourceCompletionFinalValidation.log` 14 项 Success：先前 13 项及新增 ResourceExecutionBoundary；对应构建 `ResourceCompletionFinalBuild.log`。execution 夹具通过真实 GE 验证非资源、Energy/MaxEnergy、两种混合顺序和周期 Tick/自然结束，不代表整个 GE 回滚。
- 此后只增加 BeginDestroy 回执用例并重新编译；`E:/agent/game/ue/SkillLab/Saved/Logs/ResourceDestroyValidation.log` 的 ResourceWriteIntegration Success，覆盖立即、队列应用/拒绝、真实预留冲突、写入后故障注入 Failed、EndPlay/BeginDestroy、接收者失效及回调重入。其他 13 项未在此测试文件扩展后重跑，GAS 生产代码未变。
- 最终快照逐文件 SHA-256 位于 `E:/agent/game/ue/SkillLab/Saved/Logs/ResourceDestroyHashes.json`。416 个验证副本 Source/Config/Content 文件中，3 个与生产不同：并行修改的 RuntimeComponent.h/.cpp 和 ResetResourceCompletionTests.cpp；无生产侧新增路径。其余文件包括 GAS 输入一致。该快照不证明当前 Runtime 组合已编译或测试；原 A 负责交叉集成验证。

- 隔离副本 `C:/Users/Administrator/AppData/Local/Temp/SkillLabResourceRepairValidation`，UE 5.8 Development Editor 构建成功；证据日志位于生产项目目录 `E:/agent/game/ue/SkillLab/Saved/Logs/`，不是隔离副本目录。最终构建为该目录下 `ResourceRepairFinalBuild.log`。
- 修前 `E:/agent/game/ue/SkillLab/Saved/Logs/ResourceRepairBeforeValidation.log` 真实失败：Energy current/base 分别为 100/125、100/150；Max current/base 为 0/-10，三个后续 Begin 均失败。进程退出码 0 不代表测试通过。
- 修后最终单项 `E:/agent/game/ue/SkillLab/Saved/Logs/ResourceRepairFinalValidation.log` 中 `SkillLab.GAS.ResourceWriteIntegration` Success：上述三个钳制及后续 Begin、多笔预留及重复结算、Max 降低、同步重入排队和重新校验拒绝、即时资源 GE 协调及原生即时/持续资源 GE 拒绝。测试未覆盖销毁/session 结束、所有 magnitude 形式或完整 Reset 链。
- 此前 `E:/agent/game/ue/SkillLab/Saved/Logs/ResourceRepairAfterValidation.log` 记录 13 项 Success：ResourceWriteIntegration、EnergyLedger、EnergySettlementHistory、DamageCalculation、CastExecutionOwnership、CastIntentLifecycle、CastRequestReentry、CastSubmissionIsolation、LateSubmission、MultipleSubmissions、ProductionRecords、SubmissionReconciliation、UnconfirmedSubmission。此轮之后仅扩展 `SkillLabResourceWriteIntegrationTests.cpp` 测试用例，GAS 生产代码未变；扩展后只重跑最终单项，没有重跑其余 12 项。此前测试文件版本的哈希未单独保存，不能用当前清单证明其哈希。
- 最终单项运行后比较生产与隔离副本的相对路径和 SHA-256：Source 220/220、Config 43/43、Content 149/149，差异均为 0；uproject 哈希一致。补充生成的 `E:/agent/game/ue/SkillLab/Saved/Logs/ResourceRepairSourceHashes.json` 逐项记录当前生产和隔离副本 Source 哈希及额外路径；生成时 220 项一致、无额外路径。这是生成时的当前快照，不是此前 13 项的完整历史清单。日志中仍有既有启动 `Condition failed`/GameFeatureData 限制，未运行 PIE 或可见验收。

## 待交接

Runtime 所有者负责 Reset 接线及其端到端验证。当前 `Queued` 可在 drain 时因预留校验失败，调用方不得将其解释为恢复成功。若未来需要混合 execution 的全有全无语义，须针对实际内容定义，不能将逐输出 veto 当作事务回滚。未运行 PIE 或可见验收。
