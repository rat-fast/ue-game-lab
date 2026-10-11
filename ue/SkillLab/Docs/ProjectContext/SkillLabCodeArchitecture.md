# SkillLab 当前代码架构地图

> 本文只记录当前工作区可定位的代码、配置、资产和验证事实。它不是未来程序架构方案，也不把当前类划分冻结为长期接口。

## 项目角色

SkillLab 是一个单机技能系统实验项目：`SkillLab` 模块提供玩家角色、控制器、输入、HUD 和测试场；`SkillLabSkills` 模块提供技能定义、loadout、请求门控、施法生命周期、目标/行为/事件；`SkillLabGAS` 模块提供 Gameplay Ability System（GAS）承载的战斗属性、即时伤害/治疗和持续伤害结算。

项目文件为 `SkillLab.uproject`，声明三个 Runtime 模块并启用 `GameplayAbilities`、`ModelContextProtocol`、`AllToolsets` 等插件。当前默认地图和 GameMode 来自 `Config/DefaultEngine.ini`：`/Game/SkillLab/L_SkillLab_NavTest` 与 `/Game/SkillLab/BP_SkillLabGameMode.BP_SkillLabGameMode_C`。

## 模块地图

| 稳定职责 | 代码入口 | 主要依赖与边界 |
| --- | --- | --- |
| 角色与玩家控制 | `Source/SkillLab/SkillLabCharacter.*`、`SkillLabPlayerController.*` | 组合 ASC、技能运行组件、动画组件、相机；控制器连接输入/HUD/测试命令，不直接结算效果 |
| 玩家输入、技能快捷键与 HUD | `Source/SkillLab/Public/SkillLab*Input*`、`SkillLabSkillShortcutComponent.*`、`SkillLabHUD*` | 快捷键列表是输入和 HUD 的共同来源；目标解析和展示通过 `SkillLabSkills` 的公开接口完成 |
| 技能运行时与内容 | `Source/SkillLabSkills/Public/SkillLabSkillRuntimeComponent.h` 及同模块实现 | 负责定义加载、loadout、请求门控、Cast 生命周期、行为注册/执行、事件转发；不拥有角色移动和 HUD 布局 |
| 目标、技能行为与反馈 | `SkillLabTargetingService.*`、`SkillLab*SkillBehavior.*`、`SkillLabFireball*`、`SkillLabAreaBurstSkill.*`、`SkillLabMendSkill.*`、`SkillLabBurningSkill.*`、`SkillLabSkillFeedbackActor.*` | 目标关系/距离/视线和具体执行方式在此表达；效果数值交给 `SkillLabGAS` |
| GAS 属性与效果结算 | `Source/SkillLabGAS/Public/SkillLabAbilitySystemComponent.h`、`SkillLabCombatAttributeSet.h`、`SkillLabDamageCalculation.h` | 负责生命、能量、护甲、力量、伤害减免、生命/能量钳制、死亡 Tag 和持续效果句柄；不决定技能输入或技能槽位 |
| 测试场与命令 | `SkillLabTestCommandsComponent.*`、`SkillLabTestArenaSubsystem.*`、`Source/*/Private/Tests/*` | 运行时 Exec 命令和训练假人用于 smoke；自动化测试覆盖纯伤害计算、Cast 生命周期、定义解析和 HUD 构建 |

## 代码到运行链

```text
DefaultEngine.ini
  -> BP_SkillLabGameMode / BP_SkillLabPlayerController / BP_SkillLabCharacter
  -> ASkillLabPlayerController::BeginPlay / SetupInputComponent
  -> SkillLabPlayerInputComponent + SkillLabSkillShortcutComponent
  -> SkillLabSkillInputComponent（单位目标或 Ground 预览）
  -> USkillLabSkillRuntimeComponent::SubmitSkillRequest
  -> SkillLabSkillLoadout / SkillLabSkillDefinitionProvider
  -> FSkillLabCastLifecycle + USkillLabSkillGameplayAbility
  -> SkillLabSkillBehavior（即时、投射物或范围）
  -> USkillLabSkillEffectExecutor
  -> USkillLabAbilitySystemComponent / USkillLabCombatAttributeSet
  -> SkillLabSkillEffectEventAdapter / PresentationRouter / HUD / FeedbackActor
```

`ASkillLabCharacter` 构造 `USkillLabAbilitySystemComponent`、`USkillLabSkillRuntimeComponent` 和 `USkillLabSkillAnimationComponent`。运行时组件在 `BeginPlay` 初始化 ASC actor info、属性集、loadout、效果执行器和表现路由，并向外发布 `FSkillLabSkillResult` 事件。控制器的 Exec 命令通过 `USkillLabTestCommandsComponent` 复用快捷键或直接重置运行时。

## 配置、资产和内容来源

- 数值定义文件：`Config/SkillDefinitions.json`。当前列出 `Skill_Fireball`、`Skill_AreaBurst`、`Skill_Mend`、`Skill_BurningBuff` 的 cost、cooldown、magnitude、powerCoefficient、阶段时间及燃烧 duration/period/block tags。
- 输入资产：`Content/SkillLab/Input/IMC_SkillLab.uasset`、`IA_MoveClick.uasset`、`IA_CameraLook.uasset`、`IA_CameraZoom.uasset`；技能槽位与确认/取消键当前由 `USkillLabSkillShortcutComponent` 的默认值提供。
- 运行入口资产：`Content/SkillLab/BP_SkillLabGameMode.uasset`、`BP_SkillLabPlayerController.uasset`、`BP_SkillLabCharacter.uasset`、`L_SkillLab_NavTest.umap`；另有 `L_SkillLab_Test_Box.umap`。
- 外部启动入口：`Run_SkillLab_DX11.bat` 使用 UE 5.8 `UnrealEditor.exe` 以 `-game -windowed -ResX=1280 -ResY=720 -d3d11 -log` 启动。

## 正式入口与测试入口

正式默认入口是 `DefaultEngine.ini` 的 GameDefaultMap/GameMode，经 Blueprint 资产实例化 C++ 角色和控制器。玩家路径为右键移动、鼠标拖动镜头、滚轮缩放；数字键 `1`-`4` 触发四个默认技能，地面技能用左键确认、`Escape` 取消。

测试入口分为两层：

1. 控制器 Exec：`SkillSpawnTargets`、`SkillTest`、`SkillArea`、`SkillBurn`、`SkillTestRecover`、`SkillReset`、`SkillReloadDefinitions`。
2. UE 自动化：`SkillLab.GAS.DamageCalculation`、`SkillLab.Skills.CastLifecycle`、`SkillLab.Skills.DefinitionValidation`，以及 `SkillLab.UI.WidgetTreeBeforeSlate`。

已有日志证明过 `-game -nullrhi -unattended -ExecCmds=SkillSpawnTargets,SkillTest,SkillArea,SkillBurn,SkillTestRecover` 的运行链，并观察到初始化、区域命中、修复和拒绝事件；这属于已有验证证据，不等同于本轮重新执行。

## 当前限制与未验证项

- 本文没有当前现场编译、启动或自动化测试结果；已有 `Saved/Logs` 只能作为历史运行证据，不能替代重新验证。
- 默认地图、Blueprint 属性覆盖、正式动画/音频/特效资源绑定和完整鼠标体验仍需在 UE 5.8 编辑器/游戏进程中复验。
- 当前代码事实是本地单机路径；网络权威、多人同步、输入缓冲、更多技能内容和正式内容生产协议没有证据支持。
- `SkillLabGAS` 的 ASC 在代码中设置为 replicated，但项目当前文档和验证没有证明多人同步行为，应由后续程序架构审查单独判断。

## 维护入口

模块细节见 `Modules/`。技能系统的业务语义继续以 `Docs/SkillSystem/` 共享基线为准；本目录只补充代码类、资产路径、运行入口和验证证据。
