# Input Module

## Module Responsibility

把鼠标操作转换为移动、镜头旋转和镜头缩放请求。

## Code And Assets

- `Source/SkillLab/SkillLabPlayerController.h/.cpp`
- `/Game/SkillLab/Input/IA_MoveClick`
- `/Game/SkillLab/Input/IA_CameraLook`
- `/Game/SkillLab/Input/IA_CameraZoom`
- `/Game/SkillLab/Input/IMC_SkillLab`

当前 Controller 会创建运行时 Mapping Context，并映射右键、Mouse2D 和 MouseWheelAxis；资产引用仍由 `BP_SkillLabPlayerController` 保存。

## Runtime Flow

```text
Enhanced Input Action
→ SkillLabPlayerController
→ 点击移动或镜头输入
```

右键按下后根据拖动距离区分单击和拖动：单击请求移动，拖动请求镜头旋转。

## Current Responsibilities And Evolution Boundaries

输入模块当前只负责输入接收、手势区分和请求转发，不负责具体移动算法或镜头内部计算。未来如果出现键盘、手柄、触控或多种控制方案，可以替换输入映射或增加输入解释层，不要求保持当前鼠标实现。

如果 Controller 开始承担单位能力判断、战斗执行或复杂命令路由，应考虑把输入解释和业务命令分开。

## Verification

PIE 中确认右键单击移动、右键拖动旋转镜头、滚轮缩放，且拖动不会误触发移动。
