# Character Presentation Module

## Module Responsibility

负责角色的 Skeletal Mesh、骨骼、材质和动画表现，与角色的碰撞、移动和镜头逻辑分开。

## Current State

当前 `BP_SkillLabCharacter` 仍主要作为 C++ Character 的 Blueprint 外壳，角色外观和正式动画尚未接入。胶囊体是碰撞体，不是角色模型或动画资源。

## Expected Assets

- Skeletal Mesh
- Skeleton
- Animation Sequence
- Animation Blueprint
- 必要的材质和纹理

候选方向包括 UE 自带 Manny/Mannequin 作为技术占位，或适合俯视角的低成本风格化角色资源。当前尚未将隔壁项目资产直接引用到 SkillLab。

## Expected Runtime Flow

```text
CharacterMovementComponent 速度和状态
→ Animation Instance
→ Animation Blueprint
→ Skeletal Mesh
→ 角色画面表现
```

## Current Responsibilities And Evolution Boundaries

表现模块当前只负责外观和动画，不决定导航、输入或技能规则。未来如果加入攻击、受击、技能释放、武器或专属姿态，可以增加 Montage、状态数据或专门的动画层；如果角色类型不再共享同一套骨骼，也可以建立不同的表现适配层。当前“使用 Manny”只是候选占位，不限制最终美术方向。

## Verification

接入资源后，在 PIE 中分别确认模型显示、待机播放、移动动画切换以及角色碰撞仍与视觉模型一致。
