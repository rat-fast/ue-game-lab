# Navigation Module

## Module Responsibility

让地面角色根据鼠标点击位置使用 UE NavMesh 寻路移动，并反馈目标是否可达。

## Code And Configuration

- `Source/SkillLab/SkillLabPlayerController.cpp`
- `Source/SkillLab/SkillLab.Build.cs`
- `Config/DefaultEngine.ini`
- `UAIBlueprintHelperLibrary::SimpleMoveToLocation`
- `UNavigationSystemV1::FindPathToLocationSynchronously`
- `/Game/TopDown/Cursor/FX_Cursor_Success`
- `/Game/TopDown/Cursor/FX_Cursor_Failure`
- `NavMeshBoundsVolume`

模块依赖 `AIModule`、`NavigationSystem` 和 `Niagara`。项目配置启用 `RuntimeGeneration=Dynamic`。

## Runtime Flow

```text
鼠标射线位置
→ NavigationSystem 查询路径
→ 有效路径：播放成功 Niagara 特效并提交移动
→ 无效路径：播放失败 Niagara 特效，不提交移动
→ CharacterMovementComponent 执行路径
```

## Current Behavior

角色可以在 `L_SkillLab_NavTest` 中绕过已参与导航生成的障碍。点击目标后，Controller 根据路径查询结果播放成功或失败 Niagara 特效。特效引用配置在 `BP_SkillLabPlayerController` 的默认属性中，移动和判断逻辑位于 C++。

## Current Responsibilities And Evolution Boundaries

当前只支持地面角色和 NavMesh 移动。未来加入飞行、水上、特殊移动、动态障碍或多单位能力时，可以替换导航查询和移动执行策略，或抽出独立移动执行模块。不要把当前 `SimpleMoveToLocation` 当作所有单位永久唯一的移动方式。

如果 Controller 开始管理多种单位能力，或 Character 需要同时支持多套移动后端，应考虑把移动意图和移动执行分离。

## Verification

在 `L_SkillLab_NavTest` PIE 中点击障碍另一侧，确认角色绕行并出现成功特效；点击不可达区域，确认出现失败特效且角色不进行无效直线移动。当前成功特效已由用户 PIE 确认可见。
