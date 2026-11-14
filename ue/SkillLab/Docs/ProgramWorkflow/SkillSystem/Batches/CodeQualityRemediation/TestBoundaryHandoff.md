# 测试编译边界与综合测试交接

## 范围与边界

处理主审 F2 和结构专项 R1/R2；R3 的大文件判断见 `StateCleanupHandoff.md`。在 F1/F3 合入后开始整理，保留其新增回归。首读 `SkillLabCastGameplayEventTests.cpp`、`SkillLabCombatTestFixture.h`、`SkillLabProductionFixture.h`、`SkillLabSkillBehavior.h` 与 `SkillLabSkillBehaviorProduction.cpp` 的无参重载，按场景核对现有独立测试，而非只按名字判重。

无参 `ExecuteDeclaredAction()` 是 private 且仅供 friend fixture 的测试适配，真实生产调用是带 `FPointExecution` 的重载。将测试专用适配限制在合适的 `WITH_DEV_AUTOMATION_TESTS` 编译边界或测试访问层，保持丢回执时仍调用真实生产动作；不能移除 Lua 使用的正式 `RequestDeclaredAction`，也不能在 fixture 重写业务规则。确认 UHT 和非测试 Target 的实际预处理/编译，别仅凭 Editor 编译推断。

## 测试职责与验证

综合测试 903 行且共享 World、ASC、Provider 目录和 latent 清理；按独立可验证的规则、重入、Reset、缓存等场景迁移至已有职责测试或小型独立测试，逐条核对正式入口和断言覆盖。保留 Provider 加载 -> Runtime 正式请求 -> GAS Ability -> 身份校验 GameplayEvent -> 效果/一次费用的端到端烟囱，并保留旧身份隔离和自然完成后延迟消费的必要保障。共享目录为进程全局状态，确保失败、提前返回及 latent 结束均恢复，不能假设同进程并行安全。

运行拆分前后目标测试、现有 `CastControlAndRules`、`CastRequestReentry`、`CastIntentLifecycle`、`CastSubmissionIsolation`、`MultipleSubmissions`、`ResetResourceCompletion`、生产记录和反馈相关测试；构建实际 Editor 及支持的非测试 Game Target，记录实际 target 和测试注册名变化。命令和证据约定见 `Docs/ProjectContext/Modules/TestAndVerification.md`。与其他任务可能共享测试夹具；待其测试合入后整理，避免覆盖并行改动。不要仅凭文件变短或删除旧断言视为完成。
