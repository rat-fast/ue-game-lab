# Camera Module

## Module Responsibility

管理当前角色的俯视镜头距离、角度、水平旋转、缩放和平滑处理。

## Code And UE Objects

- `Source/SkillLab/SkillLabCameraRigComponent.h/.cpp`
- `Source/SkillLab/SkillLabCharacter.h/.cpp`
- `Source/SkillLab/SkillLabPlayerController.cpp`
- `USpringArmComponent`
- `UCameraComponent`
- `USkillLabCameraRigComponent`

## Runtime Flow

```text
PlayerController
→ SkillLabCharacter
→ SkillLabCameraRigComponent
→ SpringArmComponent
→ CameraComponent
```

## Current Behavior

右键拖动旋转镜头，滚轮缩放。镜头使用俯视角，不继承角色的 Pitch、Yaw 和 Roll，也不会因为角色移动方向自动改变朝向。

## Current Responsibilities And Evolution Boundaries

当前输入绑定在 PlayerController，镜头状态和镜头参数在 CameraRigComponent，角色只负责组装组件和转发请求。未来可以增加战斗锁定、观战、镜头模式或多单位跟随，并根据需求新增镜头策略或控制器；当前单镜头结构不应被视为永久限制。

如果 PlayerController 开始直接修改 SpringArm 参数，或 CameraRigComponent 开始判断玩家业务输入，应重新评估职责分配。

## Verification

PIE 中确认右键拖动旋转、滚轮缩放，角色移动时镜头不自动切换朝向。
