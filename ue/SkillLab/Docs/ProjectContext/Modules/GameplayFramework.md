# Gameplay Framework Module

## Module Responsibility

提供 SkillLab 的基础 Gameplay Framework 对象关系和默认运行入口。

## Code And Assets

- `Source/SkillLab/SkillLabCharacter.h/.cpp`：项目 Character 类。
- `Source/SkillLab/SkillLabPlayerController.h/.cpp`：项目 PlayerController 类。
- `/Game/SkillLab/BP_SkillLabGameMode`：GameMode Blueprint 外壳。
- `/Game/SkillLab/BP_SkillLabCharacter`：Character Blueprint 外壳。
- `/Game/SkillLab/BP_SkillLabPlayerController`：PlayerController Blueprint 外壳。
- `Config/DefaultEngine.ini`：默认地图和默认 GameMode 配置。

## Runtime Flow

```text
GameMode
→ PlayerController
→ Character
→ CharacterMovementComponent
```

## Current Behavior

项目默认使用 `BP_SkillLabGameMode`，由它选择项目 Character 和 PlayerController。Character 提供胶囊体碰撞、移动组件和镜头组件。

## Current Responsibilities And Evolution Boundaries

### Current Responsibilities

当前 Character 承担角色基础运行能力；PlayerController 承担玩家输入和控制入口；Blueprint 主要作为类默认值、组件组装和资产引用外壳。

### Default Modification Entry

- 修改玩家输入或鼠标行为：查看 PlayerController。
- 修改角色移动、碰撞或角色组件：查看 Character 或对应 Component。
- 修改默认类关系：查看 GameMode Blueprint 和 `DefaultEngine.ini`。

### Acceptable Architecture Changes

未来可以增加独立的移动、战斗、交互、单位能力或命令分发模块，并迁移当前 Character 或 PlayerController 中的职责。

### Refactoring Triggers

多个单位需要复用同一能力、单个类混入多个不相关系统、或对象生命周期不再匹配时，应重新评估当前归属。

### Current Stage Limitation

当前只有一个可控制角色，因此暂不建立完整多单位框架。这是范围控制，不是永久限制。

## Verification

在 PIE 中确认默认地图能启动、角色生成、输入可用且 GameMode 使用项目类。
