# Batch02 Lua 技能组合层试点：实施摘要

## 交付范围

本轮将以下四个当前技能接入 `USkillLabLuaSkillBehavior`：

- `Skill_Fireball`
- `Skill_AreaBurst`
- `Skill_Mend`
- `Skill_BurningBuff`

四个技能的定义分别绑定 `Config/Skill_Fireball.lua`、`Config/Skill_AreaBurst.lua`、`Config/Skill_Mend.lua` 和 `Config/Skill_BurningBuff.lua`。`Skill_Mend` 保持 Lua 路径；另外三个技能已从原生 Behavior 运行入口切换为 Lua Behavior，未保留原生静默回退。

Batch01 的 `Firebolt`、`ArcStrike`、`BurnField`、`ChannelBeam` 和 `Barrier` 不在本轮接入范围内。

## 实现边界

- Lua 只使用受限宿主接口 `SkillLabSkill_RequestAction()` 和 `SkillLabSkill_Finish()`；未开放 `luaL_openlibs` 或任意引擎对象访问。
- 脚本继续经过文件、Lua 编译和 `OnSkillCast` 入口预检；脚本执行受 100000 条指令预算、Runtime generation、Cast 身份和行为身份约束。
- C++/GAS 继续负责请求门控、Cast 身份与阶段、目标重检、费用和冷却、效果执行、实际结果、Recovery/终态、Reset 以及旧行为清理。
- `RequestAction` 按已校验的 `Execution` 分派：
  - `Projectile`：创建现有 `ASkillLabFireballProjectile`，由 Projectile 处理命中、阻挡、过期和一次性回调。
  - `GroundArea`：使用现有 `FSkillLabTargetingService::FindAreaTargets`，对每个合法目标调用现有效果执行器。
  - `InstantTarget`：调用现有效果执行器。
- Runtime 的效果记录支持同一 Cast 的多个合法目标效果，避免 AreaBurst 被单效果幂等逻辑错误拦截；每个实际效果仍有独立操作记录和真实 GAS 结果。
- 与原生行为保持兼容：Executor/GAS 效果失败继续通过 Hit/效果结果事件报告真实 `bEffectApplied=false` 和失败 `Reason`，但不升级为 Cast 取消。同步动作在已完成后仍可按既有技能路径请求正常收尾；Fireball 异步 Projectile 回调在命中后无论效果结果如何均 `Complete(ProjectileReason)`，不把效果失败升级为脚本取消。
- 脚本执行异常、入口缺失、非法调用、预算耗尽和未收尾仍映射为明确的脚本取消路径；这与正常动作返回的效果失败是不同情况。

## 变更位置

- `Source/SkillLabSkills/Public/SkillLabSkillBehavior.h`
- `Source/SkillLabSkills/Private/SkillLabSkillBehavior.cpp`
- `Config/Skill_Mend.lua`
- `Source/SkillLabSkills/Public/SkillLabSkillRuntimeComponent.h`
- `Source/SkillLabSkills/Private/SkillLabSkillRuntimeComponent.cpp`
- `Source/SkillLabSkills/Public/SkillLabSkillTypes.h`
- `Source/SkillLabSkills/Private/SkillLabSkillDefinitionProvider.cpp`
- `Source/SkillLabSkills/Private/SkillLabSkillEffectExecutor.cpp`
- `Source/SkillLabSkills/Private/SkillLabFireballSkill.cpp`
- `Source/SkillLabSkills/Private/SkillLabAreaBurstSkill.cpp`
- `Source/SkillLabSkills/Private/SkillLabMendSkill.cpp`
- `Source/SkillLabSkills/Private/SkillLabBurningSkill.cpp`
- `Source/SkillLabSkills/SkillLabSkills.Build.cs`
- `Source/SkillLabSkills/ThirdParty/lua-5.4.8/src/`
- `Config/SkillDefinitions.json`
- `Config/Skill_Fireball.lua`
- `Config/Skill_AreaBurst.lua`
- `Config/Skill_Mend.lua`
- `Config/Skill_BurningBuff.lua`

## 构建验证

执行命令：

```text
D:\Epic\UE_5.8\Engine\Build\BatchFiles\Build.bat SkillLabEditor Win64 Development E:\agent\game\ue\SkillLab\SkillLab.uproject -WaitMutex -NoHotReloadFromIDE
```

结果：`Succeeded`。Lua 5.4.8 C 源由 `SkillLabSkills.Build.cs` 编译并成功链接 `UnrealEditor-SkillLabSkills.dll`。第一次执行曾因运行中的 `UnrealEditor.exe` 锁定 DLL 报 `LNK1104`，结束残留进程后使用同一命令重跑成功；最终结果以成功重跑为准。

## 自动化验证

日志：`Saved/Logs/Batch02-Lua-FourSkill-Automation.log`

- `SkillLab.Skills.CastLifecycle`：`Result={Success}`
- `SkillLab.Skills.DefinitionValidation`：`Result={Success}`
- 进程退出码：`0`

兼容性修复后已重跑同一 `SkillLab.Skills` 自动化命令，两项均成功。新日志记录本次运行；Automation 清理目录时生成的 `Batch02-Lua-FourSkill-Automation-backup-2026.10.22-15.57.25.log` 是较早运行的备份。

## RuntimeProbe 验证

主日志路径：`Saved/Logs/Batch02-Lua-FourSkill-RuntimeProbe.log`

兼容性修复后尝试重跑 `-nullrhi -unattended -SkillLabRuntimeProbe -ExecCmds=SkillReset,SkillTestRecover`，但进程停留在引擎初始化阶段，未产生项目运行事件；已停止该挂起进程。本次没有取得修复后的 RuntimeProbe 结果。下述四技能证据来自上一份成功运行日志 `Saved/Logs/Batch02-Lua-FourSkill-RuntimeProbe-backup-2026.10.22-15.58.36.log`，仅作为修复前基线，不证明兼容性修复后的运行结果：

- `Skill_Mend`（Cast 1 和 Cast 4）：`Started -> Active -> TargetSelected -> EffectApplied -> Recovery -> Completed`。两次均为满血目标，`requested=40`、`actual=0`、`health=100 -> 100`；第二次释放时能量为 `55 -> 40`，冷却为 `2.00` 秒。
- `Skill_Fireball`（Cast 2）：创建 Projectile 并命中；`requested=30`、`actual=40`、目标生命 `100 -> 60`，随后进入 Recovery 并以 `Hit` 完成。
- `Skill_AreaBurst`（Cast 3）：非空范围找到目标并施效；`requested=20`、`actual=25`、目标生命 `60 -> 35`，随后进入 Recovery 并完成。该路径覆盖了单次探针中的范围效果记录。
- `Skill_BurningBuff`（Cast 5）：首个效果记录 `requested=6`，随后记录 `BuffApplied`；探针观察到三次 `DamageTick`，每次 `requested=8`、`actual=8`，目标生命由 `35` 降至 `11`，最终记录 `BuffRemoved` / `EffectTrackingCleared`。Reset 后能量恢复为 `100`，Fireball 冷却为 `0.00`。

## 尚未验证

- Fireball 未命中、阻挡和过期路径尚未单独注入验证。
- AreaBurst 空区域尚未单独验证；多目标范围同时命中的独立场景尚未采集。
- Mend 的 `Health 50 -> 90`、`90 -> 100` 精确迁移场景尚未采集；当前证据只有满血治疗。
- 脚本缺失、语法错误、非法调用、死循环、未收尾和预算耗尽尚未重新注入运行。
- Executor/GAS 效果失败尚未专门注入，尚未取得 `EffectFailed` 及真实失败原因日志。
- 本次兼容性修复后的 RuntimeProbe 未能完成；Fireball 异步效果失败及其他技能效果失败保持 Cast 正常完成的行为尚未通过运行日志验证。代码现已按原生收尾语义处理，但只完成构建与既有两项自动化验证。
- 重复 `RequestId`、Ability 激活失败以及“只产生 Rejected、不产生正式 Cast Cancelled”的注入场景尚未专门运行。
- Reset 在前摇、同步效果回调、Recovery 重入，以及死亡后 Reset 再施法尚未专门验证。
- 无活动重载、坏内容恢复和完整自动化集合尚未验证。
- 未执行可见 PIE/视口视觉验收；当前 RuntimeProbe 使用 NullRHI，日志证据不能替代视觉验收。

## 独立 Review 结论

已完成独立程序 Review。结论为“代码接入和基本冒烟通过，但不能按四技能完整验收放行”。Review 确认四个 Configure 和 JSON 均指向 Lua；同时指出 AreaBurst 多目标、Fireball 异步失败、BurningBuff 自然生命周期、脚本/效果失败、重复请求、Reset 重入和完整 Lua 运行归属证据仍未闭环。四个技能文件中保留的旧原生 Execute 也属于迁移残留，当前注册路径不会实例化，但后续应明确清理或标记为不可达基准。

本轮未执行 Git 提交或远端推送。
