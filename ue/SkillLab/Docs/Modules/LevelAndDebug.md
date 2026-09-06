# Level And Debug Module

## Module Responsibility

提供最小可运行箱庭、导航测试空间和运行时可观察反馈。

## Assets And Objects

- `/Game/SkillLab/L_SkillLab_Test_Box`：最小运行地图。
- `/Game/SkillLab/L_SkillLab_NavTest`：带外围墙、中间障碍和 NavMesh 的导航测试地图。
- 地面、外围墙、中间障碍、PlayerStart。
- `NavMeshBoundsVolume`。

## Configuration

`Config/DefaultEngine.ini` 设置项目编辑器启动地图和游戏默认地图为 `/Game/SkillLab/L_SkillLab_NavTest`，并设置默认 GameMode；同时启用导航运行时动态生成。

## Current Behavior

`L_SkillLab_NavTest` 约为 30m × 30m 的小箱庭，不是开放世界。导航目标可达性通过点击位置生成的成功/失败 Niagara 特效观察。特效实现位于 `SkillLabPlayerController`，资源引用配置在 `BP_SkillLabPlayerController`。

## Current Responsibilities And Evolution Boundaries

当前关卡只服务基础运行、导航和调试验证。未来可以增加正式环境、美术资源、战斗测试区或专门验证地图；不要求继续扩大当前测试地图，也不应把调试标记直接当作正式 Gameplay 或技能表现。

## Verification

地图检测无错误时，在 PIE 中确认 PlayerStart、地面、墙体、障碍、NavMesh 和点击反馈特效均可观察。当前用户已确认移动反馈效果正常。
