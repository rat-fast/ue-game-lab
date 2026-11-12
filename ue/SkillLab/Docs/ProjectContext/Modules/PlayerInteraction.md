# PlayerInteraction 玩家交互与 HUD

## Module Responsibility

`SkillLab` 模块把玩家输入和控制器生命周期连接到技能运行时，提供移动、镜头、技能目标选择、快捷键过滤和运行时 HUD。它展示事件和状态，不拥有效果结算。

## Code And Assets

- 角色/控制器：`ASkillLabCharacter`、`ASkillLabPlayerController`、`USkillLabCameraRigComponent`。
- 输入：`USkillLabPlayerInputComponent`、`USkillLabSkillInputComponent`、`USkillLabSkillShortcutComponent`。
- HUD：`USkillLabHUDManagerComponent`、`USkillLabHUDWidget`、`USkillLabHUDSkillSlotWidget`、`FSkillLabHUDSnapshot`。
- 上述输入/HUD 组件头文件位于 `Source/SkillLab/Public/`，实现位于 `Source/SkillLab/Private/`；角色、控制器和 CameraRig 实现在 `Source/SkillLab/`。
- 输入资产在 `Content/SkillLab/Input/`；控制器和角色 Blueprint 在 `Content/SkillLab/`。

## Runtime Entry

`ASkillLabPlayerController::BeginPlay` 初始化技能输入、测试命令和 HUD，设置鼠标可见与 GameAndUI 输入模式；`SetupInputComponent` 绑定 Enhanced Input 和技能快捷键。`USkillLabPlayerInputComponent` 运行时创建映射，把右键、鼠标二维移动和滚轮接入移动/镜头。

## Runtime Flow

数字键由 Shortcut 过滤后调用 `BeginSkill`。默认 `1/2/3/4` 分别对应 Fireball、AreaBurst、Mend、BurningBuff。单位技能通过 `ResolveCursorUnitTarget` 解析目标；地面技能经 `ResolveGroundLocation` 投射成功后进入预览，左键 `ConfirmSkill` 再次解析地面并提交，`Escape` 取消。投射失败不会启动预览或提交。普通右键释放在非地面选点时执行导航移动；拖动和滚轮控制镜头。

HUD manager 在 `InitializeForPlayer` 中创建 Widget 并 `AddToViewport`；`BuildSnapshot` 要求 Runtime 已初始化，按共同快捷键列表逐项解析定义，缺失项不生成技能槽。HUD widget 在 `NativeTick` 刷新生命/能量、消耗和冷却；HUD 与技能卡都在 `RebuildWidget` 调用 `Super` 前构建 WidgetTree，避免 `NativeConstruct` 才建树而错过 Slate 根节点读取。Widget 创建日志或快照只说明生命周期/数据事实，可见性需结合画面证据。

## Current Responsibilities And Evolution Boundaries

修改按键、槽位和确认/取消行为优先看 `SkillLabSkillShortcutComponent` 与 `SkillLabSkillInputComponent`；修改移动/镜头看 PlayerInput/CameraRig；修改界面结构看 HUD widget。当前快捷键默认值在 C++ 构造函数中，Blueprint 可覆盖属性；这不等于已有运行时配装系统。技能动作资源由定义和表现链消费，定位见 [表现模块](CharacterPresentation.md)。

## Verification

`SkillLab.UI.WidgetTreeBeforeSlate` 是 HUD/技能卡树、文本和冷却进度的自动化入口；历史全量运行曾有 Slate 包装类型断言失败，本轮未复测，不能据入口存在声称通过。

2026-11-12 实际工程 DX11 `-game` 的 `Saved/Logs/HUDInputDeliveryGame.log` 记录四技能加载、四快捷键绑定及 `Snapshot HP=100/100 Energy=100/100 Skills=4`；[实际窗口截图](../../../Saved/Logs/HUDInputDeliveryWindow.png) 可见 HP、Energy 和四技能槽。真实按键 3 进入 Mend 并完成提交；按键 2 和左键也到达入口，但当次地面投射失败，不能记为 AreaBurst 成功。用户随后人工确认“技能能放出来”。构建/加载证据与限定结论见 [测试模块](TestAndVerification.md)；本轮只核对已有材料，没有交互操作。

## Current Limitations

已有实际窗口的基础 HUD 显示和用户释放确认；完整键鼠组合、地面预览/确认/取消、碰撞、不同分辨率布局及 PIE 尚未全面验收。没有证据支持手柄、触摸、网络输入或失焦恢复；Runtime 的单槽缓存意图见 [Skills 模块](SkillLabSkillsRuntime.md)，不据此宣称全部输入体验通过。

## Extension Directions

新增输入来源应转换为相同 `FSkillLabSkillRequest` 语义；新增 HUD 信息应消费运行时快照/事件，避免在 UI 层推断技能结果。
