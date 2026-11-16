# 01 技能空间配置：实现摘要

## 当前状态

通用空间能力已接入现有技能定义、Fireball 正式提交路径及 Presentation 事件路径。四技能已配置 `TEMP-SPATIAL-VALIDATION`，仅用于空间验证，不是正式适配参数。无空间定义时沿原有路径运行。显式 Marker 使用现有无碰撞反馈代理，不是正式 VFX。代码和当前配置已通过完整构建、五项定向自动化及四技能 NullRHI Probe。AreaBurst 修正后已通过一次可见 DX11 游戏窗口局部表现核验；其他三技能及更完整体验仍待游戏内验证。

## 实现事实与权威入口

- 稳定空间计算及 Actor/SkinnedMesh socket/bone 来源解析位于 `Source/SkillLabSkills/Public/SkillLabSpatialTransform.h` 和 `Private/SkillLabSpatialTransform.cpp`。支持 Source、Target、Ground、Related、Reference、World、ExecutionDirection、独立方向选择、Once/Follow、可选 socket、方向修正及视觉缩放。Related 由受信任的 C++ 调用方在 `FSkillLabSkillRequest::RelatedActor` 显式指定，作为弱引用随 Cast 和结果事件传递，既不替代 Target，也不向 Lua 暴露 Actor；缺失、失效或与 Source 不在同一 World 时空间解析失败。空间偏移不继承参照缩放；无效 Actor、socket 歧义/缺失、非有限数据及零执行方向返回具体错误。
- `Config/Skill_<name>.lua` 以 `SkillLabSkill_Spatial(name, object, event, source, offsetSpace, direction, relation, rotationRelation)` 声明对象、事件、参照和行为。支持 object=`Projectile|Marker`；event=`Release|TargetSelected|Hit|BuffApplied`（按对象约束）；source=`Source|Target|Ground|Related`；offsetSpace/direction=`Reference|World|ExecutionDirection`/`Reference|World|ExecutionDirection`；relation=`Once|Follow`；rotationRelation=`FollowRotation|KeepRotation`。Lua 只作内容组合声明，重放时从已解析快照取值，不运行逐帧逻辑。
- `Config/SkillDefinitions.json` 每个技能可声明 `spatial` 对象，键须与 Lua 的对象名一致。允许参数：`socket`、`mesh`（软对象路径）、`offset:[x,y,z]`、`worldDirection:[pitch,yaw,roll]`、`visualRotation:[pitch,yaw,roll]`、`visualScale`。显式 Lua 对象要求同名 JSON 参数；对象/事件组合、未知字段、资源路径形式、数值范围和绑定均经校验。Provider 仍原子替换有效定义；Cast/事件携带已捕获配置快照。
- Projectile 定义在正式释放提交时解析当前参照并改变真实 Spawn 点。既有目的点、速度、碰撞半径和碰撞响应不变；视觉比例/网格/朝向仅作用于可见子网格。失败明确记录技能、对象、原因并沿现有提交失败入口拒绝。
- Presentation 的 Lua 事件宿主通过 `SkillLabPresentation.ShowSpatial()` 请求当前事件上的配置。C++ Router 依据已有事件创建无碰撞代理，按配置定位/缩放/资源替换；Follow 由代理 Actor 自身 C++ Tick 更新，Lua 不逐帧运行。Buff 跟随键为 EffectInstanceId + object name，BuffRemoved 只清理对应实例；新 Router/组件 EndPlay 解绑并清理自有代理。没有适配项时保留旧 Marker 路径。

内容定义范例（示意，不替后续配置者决定最终技能参数）：

```lua
SkillLabSkill_Spatial("muzzle", "Projectile", "Release", "Source", "ExecutionDirection", "ExecutionDirection", "Once", "KeepRotation")
```

```json
"spatial": {
  "muzzle": {
    "socket": "MuzzleSocket",
    "offset": [0, 0, 120],
    "visualRotation": [0, 0, 0],
    "visualScale": 1.0
  }
}
```

修改后执行控制台 `SkillReloadDefinitions`（或重启）；失败时保留上一个有效目录。新 Cast 用新快照，已生成 Projectile 和已有表现按各自捕获的参数继续。

## 四技能临时验证配置

权威入口是 `Config/SkillDefinitions.json` 对应技能的 `spatial` 对象，加上该技能 Lua 中同名 `SkillLabSkill_Spatial(...)` 声明。Plane/Cube 只作无碰撞测试代理，不代表正式 VFX。

| 技能 / 对象 | Lua 空间组合 | JSON 参数 |
| --- | --- | --- |
| Fireball / `tempSpatialProjectile` | `Projectile, Release, Source, Reference, ExecutionDirection, Once, KeepRotation` | 偏移 `[80,120,70]`；世界朝向/修正 `[0,0,0]`；比例 `1.0`；省略 Mesh/Socket，沿用现有弹体主体 |
| AreaBurst / `tempSpatialGroundMarker` | `Marker, TargetSelected, Ground, World, World, Once, KeepRotation` | Opaque Engine Plane 测试代理；偏移 `[450,0,10]`（世界轴，4.5m，位于350cm伤害半径之外）；世界朝向 `[0,35,0]`；比例 `0.2`，按空间表现的缩放公式预计约1.4m见方，不代表伤害边界 |
| Mend / `tempSpatialSelfMarker` | `Marker, Hit, Source, Reference, World, Once, KeepRotation` | Cube；偏移 `[0,-180,140]`；世界朝向 `[0,45,0]`；视觉修正 `[20,0,0]`；比例 `1.8` |
| BurningBuff / `tempSpatialBuffMarker` | `Marker, BuffApplied, Target, Reference, Reference, Follow, FollowRotation` | Cube；偏移 `[0,160,130]`；视觉修正 `[0,45,0]`；比例 `1.25` |

Fireball 使用 Source Actor 局部参照，未填写未经核实的手部 Socket；近墙/遮挡下偏移会改变轨迹和碰撞结果，本轮没有改碰撞或瞄准规则，须在可见游戏中另测。AreaBurst 代理位置与真实伤害中心故意分离，不可当作命中范围提示；Mend 仍自疗；BurningBuff Marker 按真实效果实例跟随和结束，不依赖 Cast 终态。

### AreaBurst 全图覆盖修正

最初 `visualScale=8.0` 造成场景覆盖。原因已按实际代码和资产几何核实：Router 先按 `(VisualRadius=350 × visualScale) / 50` 设置代理 XY 缩放，所以原值得到56倍；Engine BasicShapes Plane 的本地 bounds 是 X/Y 各 `-50..50` cm，即基础边长100cm，最终约56m见方。Cylinder 上创建的动态材质虽收到 `Opacity=0.75`，材质 Blend Mode 为 Opaque 时该参数不产生半透明；替换 Plane 也沿用这个不透明材质。因此不能用 opacity 值解释为可透视或用材质替换规避覆盖。

AreaBurst 临时 profile 已从偏移 `[300,0,35]`、比例 `8.0` 调整为世界偏移 `[450,0,10]`、比例 `0.2`。由 `(350 × 0.2)/50=1.4` 倍 Plane 缩放，对应约140cm见方的实心平面；侧移450cm超过既有350cm作用半径，并仅产生一个偏离中心的局部代理，不画出或暗示伤害边界。保持无碰撞、1.5秒短时生命周期和既有色材质行为。伤害中心、radius=350、目标和其他战斗规则均未变。该面片仍不透明但局部有界；可见材质颜色、落地高度、被场景遮挡情况和玩家是否能清楚区分仍需游戏窗口确认。

**恢复方式：** 四个技能进入临时适配前均没有 `spatial` 字段，也没有空间 Lua 声明。恢复到本轮修正前的 AreaBurst 值时，将其 JSON `mesh` 设回 `/Engine/BasicShapes/Plane.Plane`、`offset` 设回 `[300,0,35]`、`worldDirection` 保持 `[0,35,0]`、`visualRotation` 保持 `[0,0,0]`、`visualScale` 设回 `8.0`；Lua 组合不变。这个恢复值会重现约56m方形覆盖缺陷，不建议继续运行。若要撤销整个临时适配，删除四个 JSON `spatial` 属性和四个 Lua 中完整的 `TEMP-SPATIAL-VALIDATION` 注释/声明块，保留原 `SkillLabSkill_Define` 和 `OnSkillCast`，再执行 `SkillReloadDefinitions` 或重启。若文件后来有其他改动，只撤销相应字段/块，不覆盖整文件。配置职责已演练逐字节恢复；原文件 SHA-256 与新增 Lua 块记录在本机内容配置交付清单中。

## 验证证据

- Related 修复后实际工程 UE 5.8 `SkillLabEditor Win64 Development` 构建成功：`Saved/Logs/Batch04RelatedBuild2.log`。定向自动化 `SubmittedActionLifetime` 与 `Presentation.SpatialLifecycle` 两项成功：`Saved/Logs/Batch04RelatedTests2.log`。它们覆盖 Target 与 Related 不同时的真实 Projectile Spawn、结果事件与表现对象位置，Source/Target/Ground 的来源选择和缺失 Related 不回退。测试启动仍有 13 条 `Condition failed`；首次 fixture 因出生点与关联对象碰撞而失败，日志 `Saved/Logs/Batch04RelatedTests.log`，修正 fixture 的碰撞后重跑通过，未修改生产碰撞规则。
- 当前完整代码和四技能临时配置的实际工程 UE 5.8 `SkillLabEditor Win64 Development` 构建成功：`Saved/Logs/Batch04SpatialFinalWithConfigBuild.log`。
- 当前完整版本五项定向自动化全部成功：`SpatialTransform`、`DefinitionValidation`、`SubmittedActionLifetime`、`Presentation.SpatialLifecycle`、`Presentation.SubmissionIdentity`；日志 `Saved/Logs/Batch04SpatialFinalWithConfigTests.log`，5 tests performed、队列清空。覆盖配置校验/缺省兼容、坐标语义、真实 Spawn 消费偏移、碰撞体隔离、障碍阻挡、Target/Related 分离、Buff 多实例跟随/移除清理及表现身份。
- 当前四技能配置实际 `-game -NullRHI -SkillLabRuntimeProbe` 完成，日志 `Saved/Logs/Batch04SpatialFinalWithConfigProbe.log`。四条 `[SkillLabSpatial]` 分别记录 Fireball 真实 Spawn、AreaBurst `Scale=8.00`、Mend `Scale=1.80`、BurningBuff `Scale=1.25`，最终有 `COMPLETE`。这证明配置被加载且对象逻辑消费参数；NullRHI 不证明可见差异。
- PM 先前一次正式 DX11 `-game -windowed` 窗口尝试遇锁屏，未能操作/观察场景；该进程随后关闭。实际角色 Socket/Bone、Fireball 近墙/遮挡场景及其他三个样本的视觉体验仍待游戏内验证。
- AreaBurst 初始空间 Probe 记录 `[SkillLabSpatial] ... Scale=8.00`；用户在数字键2施放后报告全图不透明覆盖。静态根因及当前修复值见“AreaBurst 全图覆盖修正”；可见复测结果见下方独立记录。
- **AreaBurst 修正后的实测：** 配置重载/参数断言日志 `Saved/Logs/Batch04AreaBurstProfileReload.log` 记录 Provider `SkillDefinitions reload succeeded`、radius=`350`、scale=`0.2`、计算 Plane bounds=`140x140cm`、偏移=`450cm`。定向自动化 `SpatialLifecycle` 和 `DefinitionValidation` 均 Success（`Saved/Logs/Batch04AreaBurstSpatialTests.log`，2 tests performed）；测试启动有13条既有 `Condition failed`，目标测试通过。NullRHI RuntimeProbe `Saved/Logs/Batch04AreaBurstProfileProbe.log` 记录 `Scale=0.20`、后续 AreaBurst 命中和 `COMPLETE`；同时仍有 NiagaraToolsets 插件 AttributeError，Probe 完成。
- 已在非 PIE 的 DX11 `-game -windowed` 交互窗口调用 `SkillReloadDefinitions`，随后按数字键2并左键选地施放 AreaBurst。日志 `Saved/Logs/Batch04AreaBurstVisibleGame.log` 记录 reload succeeded、`BeginSkill skill=Skill_AreaBurst`、空间对象 `Event=TargetSelected ... Scale=0.20`，该无敌人选点按预期返回 `AreaEmpty`。截图 `Saved/Logs/Batch04AreaBurstVisibleMarker.png` 显示约140cm方形不透明 Plane 独立位于角色旁，场景其余部分可见，没有全图覆盖；这是针对覆盖故障的可见验收证据。单次空区域施放只验证局部 Marker 显示和 AreaEmpty 不变，未验证有效敌人伤害范围、不同地图遮挡或全部摄像机距离。该游戏进程由本轮启动、验证后正常退出；没有结束其他进程。
- 本轮最终可见核验仅覆盖 AreaBurst 覆盖修复。Fireball 的 Socket/近墙/遮挡、Mend/BurningBuff 的视觉体验仍待观察；未改变它们的临时配置或战斗规则。
- 自动化启动阶段有 13 条既有 `Condition failed`；上述五项目标测试均成功。RuntimeProbe 另有 NiagaraToolsets Python AttributeError 等引擎插件诊断，但 Probe 执行到四技能空间记录和 `COMPLETE`。未改写投射物穿墙、重叠出生、目标/伤害/范围规则。

## 交接与风险

- 四技能临时 Profile 已由当前配置 Probe 验证对象事件和计算位置；可见效果仍未验收。它们不是正式适配参数，正式适配需在后续按游戏观察替换。
- Mesh 目前支持 StaticMesh soft path；没有 Niagara/特效系统或材质参数配置。持续表现 Follow 限定在 BuffApplied，资源/参照失效会记录并清理视觉对象，不改效果结果。
- 当前事件来源已有 Cast/Submission/EffectInstance 身份；Buff 视觉按 EffectInstance 隔离。非持续 Hit/TargetSelected 表现为短时对象，不追踪提交键。
- 实现增加了 Skills 空间解析及 Presentation 代理生命周期职责；PM 应在独立 Review 通过后最小更新 `SkillLabSkillsRuntime.md` 与 `CharacterPresentation.md` 当前代码事实。规划目录两个 README 的既存修改来自用户工作树，需按本轮 01 状态同步，并保留 02—04 的范围与说明。
