# PlayerInteraction 玩家交互与 HUD

## Module Responsibility

`SkillLab` 模块把玩家输入和控制器生命周期连接到技能运行时，提供移动、镜头、技能目标选择、快捷键过滤和运行时 HUD。它展示事件和状态，不拥有效果结算。

## Code And Assets

- 角色/控制器：`ASkillLabCharacter`、`ASkillLabPlayerController`、`USkillLabCameraRigComponent`。
- 输入：`USkillLabPlayerInputComponent`、`USkillLabSkillInputComponent`、`USkillLabSkillShortcutComponent`。
- HUD：`USkillLabHUDManagerComponent`、`USkillLabHUDWidget`、`USkillLabHUDSkillSlotWidget`、`FSkillLabHUDSnapshot`。
- 输入资产在 `Content/SkillLab/Input/`；控制器和角色 Blueprint 在 `Content/SkillLab/`。

## Runtime Entry

`ASkillLabPlayerController::BeginPlay` 初始化技能输入、测试命令和 HUD，设置鼠标可见与 GameAndUI 输入模式；`SetupInputComponent` 绑定 Enhanced Input 和技能快捷键。`USkillLabPlayerInputComponent` 运行时创建映射，把右键、鼠标二维移动和滚轮接入移动/镜头。

## Runtime Flow

数字键由 Shortcut 过滤后调用 `BeginSkill`。单位技能通过 `ResolveCursorUnitTarget` 解析目标；地面技能进入预览，左键 `ConfirmSkill`，`Escape` 取消。运行时事件通过 HUD manager 构建生命/能量、快捷键、消耗和冷却快照，HUD widget 在 Slate/UMG 树中生成技能卡。普通右键释放在非地面选点时执行导航移动；拖动和滚轮控制镜头。

## Current Responsibilities And Evolution Boundaries

修改按键、槽位和确认/取消行为优先看 `SkillLabSkillShortcutComponent` 与 `SkillLabSkillInputComponent`；修改移动/镜头看 PlayerInput/CameraRig；修改界面结构看 HUD widget。当前快捷键默认值在 C++ 构造函数中，Blueprint 可覆盖属性；这不等于已有运行时配装系统。正式动画、音频和特效资源绑定尚未形成代码契约。

## Verification

`SkillLab.UI.WidgetTreeBeforeSlate` 覆盖 HUD 和技能卡在 Slate 构造前后的 WidgetTree、文本和冷却进度。历史 smoke 日志证明技能事件能进入反馈；本轮未重新执行交互验证。

## Current Limitations

没有证据支持手柄、触摸、网络输入、输入缓冲、失焦恢复或完整玩家体验验收。地面投射、碰撞和最终 HUD 布局仍需编辑器/游戏进程确认。

## Extension Directions

新增输入来源应转换为相同 `FSkillLabSkillRequest` 语义；新增 HUD 信息应消费运行时快照/事件，避免在 UI 层推断技能结果。
