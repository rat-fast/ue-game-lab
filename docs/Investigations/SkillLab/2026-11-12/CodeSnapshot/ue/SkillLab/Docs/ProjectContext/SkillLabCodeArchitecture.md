# SkillLab 当前代码架构地图

> 本文记录当前代码、配置、资产入口与模块关系；具体实现和验证证据放在模块文档，不把当前类划分冻结为长期接口。

## 项目角色

SkillLab 是单机技能系统实验项目。`SkillLab` 组装角色、控制器、输入、HUD 和测试场；`SkillLabSkills` 提供内容解析、loadout、多 Cast 运行、点提交和来源记录；`SkillLabGAS` 提供战斗属性、效果结算及资源协调；`SkillLabPresentation` 消费结构化事件并适配角色与技能表现；`SkillLabLuaRuntime` 提供业务无关的 Lua 运行时。

`SkillLab.uproject` 声明上述五个 Runtime 模块及一个 Editor 模块，启用 `GameplayAbilities`、`ModelContextProtocol`、`AllToolsets` 等插件。默认地图/GameMode 来自 `Config/DefaultEngine.ini`：`/Game/SkillLab/L_SkillLab_NavTest`、`/Game/SkillLab/BP_SkillLabGameMode.BP_SkillLabGameMode_C`。

## 模块地图

| 稳定职责 | 代码入口 | 细节与边界 |
| --- | --- | --- |
| 角色与玩家控制 | `Source/SkillLab/SkillLabCharacter.*`、`SkillLabPlayerController.*` | 组合 ASC、技能运行时、表现与相机；不直接结算效果 |
| 输入、快捷键与 HUD | `SkillLabPlayerInputComponent.*`、`SkillLabSkillInputComponent.*`、`SkillLabSkillShortcutComponent.*`、`SkillLabHUD*` | 快捷键列表是输入和 HUD 的共同来源；目标与技能事实通过 Skills 接口获取 |
| 技能内容与运行 | `Source/SkillLabSkills/` | [Skills 模块](Modules/SkillLabSkillsRuntime.md)：定义、每 Cast 上下文、共同仲裁、点调度、交付寿命、Reset |
| 属性、效果与资源 | `Source/SkillLabGAS/` | [GAS 模块](Modules/SkillLabGAS.md)：伤害/治疗、actual/reserved/available、资源写入队列与最终回执 |
| 角色与技能表现 | `Source/SkillLabPresentation/` | [表现模块](Modules/CharacterPresentation.md)：状态采集、Lua 意图、AnimBP 适配、完整 Cast token、Montage/Notify |
| 公共 Lua 运行时 | `Source/SkillLabLuaRuntime/`、`SkillLabLua.h` | Lua 5.4.8 唯一源码、VM、受保护调用、指令预算和 opaque facade；只依赖 `Core` |
| 测试场与验证 | `SkillLabTestCommandsComponent.*`、`SkillLabTestArenaSubsystem.*`、各模块 `Private/Tests/` | [测试模块](Modules/TestAndVerification.md)：命令、自动化入口、分版本证据与验收限制 |

## 代码到运行链

```text
DefaultEngine.ini -> Blueprint GameMode / PlayerController / Character
  -> PlayerInput / SkillShortcut / SkillInput（单位目标或地面选点）
  -> USkillLabSkillRuntimeComponent::SubmitSkillRequest
  -> 定义快照 / loadout / 活动执行共同仲裁 / 完整门控
  -> USkillLabCastExecution + USkillLabSkillGameplayAbility
  -> USkillLabLuaSkillBehavior 的提交点调度
  -> TargetingService / Projectile / SkillEffectExecutor
  -> GAS 效果结算 / 能量预留与一次结算 / 来源记录
  -> OnNativeSkillEvent -> PresentationRouter / Lua 表现 / HUD
```

Runtime 管理角色级执行集合、单槽 latest-wins 意图和会话屏障；`USkillLabCastExecution` 拥有每 Cast 的可变状态。身份使用 Runtime、generation、CastId 组成的完整 token。已确认交付的投射物、目标 ASC 持有的持续 GE 及未决提交记录各有独立寿命，不随普通 Cast 终态一并销毁。详见 Skills 模块。

角色表现另走 `CharacterMovement -> CharacterAnimationStateComponent -> CharacterPresentation.lua -> CharacterAnimInstance -> ABP_Unarmed`；技能事件由 Lua 表现组件创建并绑定的 Router 转发，再按完整 token 播放/停止动作。`SkillLabSkills` 不依赖表现模块、不创建表现对象。公共 Lua facade 由 Skills 与 Presentation 按需私有依赖；Lua 不直接拥有 ASC、Actor 或动画求值。

## 配置、资产和内容来源

- `Config/SkillDefinitions.json` 提供数值、阶段、动画软路径、规则 profile、提交点声明及 `luaScript` 绑定；绑定 Lua 的 `SkillLabSkill_Define` 提供目标、关系、效果、执行模板和规则声明。Provider 统一解析并原子替换有效候选目录；`FSkillLabSkillBehaviorRegistry::ApplySharedRuntimeClasses` 只设置共享 Ability/Behavior 类。解析分工见 Skills 模块。
- 当前四技能为 `Skill_Fireball`、`Skill_AreaBurst`、`Skill_Mend`、`Skill_BurningBuff`，均使用通用 Lua Behavior；生产 JSON 未显式配置多提交点，沿用隐式 Script 点。运行宿主提供受限 `SkillLabSkill_RequestAction`、`SkillLabSkill_ApplyEffect`、`SkillLabSkill_Finish`，不支持常驻协程。
- 四技能当前均以 `castMontage` 绑定 `/Game/SkillLab/Animations/AM_Skill_*`。`castAnimation` 是兼容解析路径；当前生产 Montage 同属 `DefaultGroup/DefaultSlot`，不能据多 Cast token 隔离宣称已完成可见双 Montage 验收。
- `Config/CharacterPresentation.lua` 编排 locomotion 与技能 Started/终态动作；`Config/AnimationDefinitions.json` 用于独立原生 `USkillLabAnimInstance` 路径的移动阈值/方向范围。角色表现链使用 `USkillLabCharacterAnimInstance` 桥接，二者不混用。
- Lua 5.4.8 唯一第三方源码位于 `Source/SkillLabLuaRuntime/ThirdParty/lua-5.4.8/`，由该模块统一编译维护。
- 输入资产位于 `Content/SkillLab/Input/`；槽位与确认/取消键默认值由 `USkillLabSkillShortcutComponent` 提供。入口资产位于 `Content/SkillLab/`：`BP_SkillLabGameMode`、`BP_SkillLabPlayerController`、`BP_SkillLabCharacter`、`L_SkillLab_NavTest`，另有 `L_SkillLab_Test_Box`。
- `DefaultEngine.ini` 的 `[CoreRedirects]` 保留表现反射类型迁移兼容；添加重定向不等于旧 Blueprint/AnimBP 已完成可见验收。

## 正式入口与验证路由

玩家路径为右键移动、鼠标拖动镜头、滚轮缩放；数字键 `1`–`4` 请求四个默认技能，地面技能左键确认、`Escape` 取消。`Run_SkillLab_DX11.bat` 通过 UE 5.8 `UnrealEditor.exe -game -windowed -ResX=1280 -ResY=720 -d3d11 -log` 启动游戏窗口；编辑器 Play 才是 PIE。

Exec、RuntimeProbe、自动化注册名、构建/运行复现方式与当前证据统一见 [TestAndVerification](Modules/TestAndVerification.md)。B1/多 Cast、P2-CE-1、R 资源修复及 Reset 专项的通过结论各有版本范围；文档同步未重新构建或运行，不把旧测试数量合并为当前全套通过。可见 PIE、双 Montage 与未覆盖资产范围仍按测试模块的限制处理。

## 维护入口

本目录维护项目事实；业务语义以 `Docs/Baseline/SkillSystem/` 为入口。功能交接与限定 Review 见 `Docs/ProgramWorkflow/SkillSystem/Batches/SkillCombatRulesCompletion/`，不将其过程记录复制进代码地图。当前验证主要为本地单机路径；ASC 的 replication 设置不构成多人验证证据。
