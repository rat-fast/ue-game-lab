# Loadout Ability 归属交接

## 范围与落点

处理主审 F7；业务边界见 `Docs/Baseline/SkillSystem/05-OwnershipAndCleanup.md`。`InitializeSkills` 创建 Runtime 所有的 Loadout，`SkillLabSkillLoadout.cpp` 对每个定义 GiveAbility 并保存 handle；`FinishEndPlayCleanup` 先处理活跃 Cast、延迟提交和观察解绑，当前不撤销 spec。正式请求使用 Loadout 精确 handle。首读上述两文件、`SkillLabSkillLoadout.h`、`SkillLabSkillGameplayAbility.cpp`，按需读 `SkillLabSkillRuntimeExecution.cpp` 的取消/结束及 `SkillLabFireballProjectile.cpp` 的独立交付。

建议由 Loadout 管理其自身授予的 handle，在 Runtime 结束且提交重入收束后对称撤销；先核实 GAS 在 Ability 正激活、同步回调、ASC 已失效以及重复清理时的行为。只撤销本 Loadout 授予的 spec，保留其他系统的 Ability；禁止 `ClearAllAbilities`，不能把普通 Reset 变成撤销，也不能取消已交付 Projectile/GE。若重用 ASC 级授予更合适，须证明单一所有权和替换/重入安全，而不是只按 Ability class 搜索并误认其他授予。

## 验证与依赖

复用 `SkillLab.Skills.SubmittedActionLifetime`、`CastExecutionOwnership`、`CastRequestReentry` 夹具；增加同 Actor/ASC 多次 Runtime 替换前后 spec 数量、精确 handle 和独立 Ability 存续测试，含活跃 Cast、延迟提交重入、已交付投射物/持续效果以及重复结束。构建与运行入口见 `Docs/ProjectContext/Modules/TestAndVerification.md`。`SkillLabSkillRuntimeComponent.cpp` 与 F1/F3 及效果风险核对可能重叠，等行为修复合入后编辑并与效果任务协调；与历史方案逻辑独立，但应回归跨 Runtime 未决反馈。
