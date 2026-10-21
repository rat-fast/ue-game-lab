# Batch01 角色基础表现实现交接

## 当前状态

任务 A 已接通默认角色的动画快照、`ABP_Unarmed` 与基础状态机，并完成 Game/Editor 构建、资产编译和默认角色 RHI `-game` 可视烟测。状态迁移日志和按状态保存的游戏视口截图共同验证了默认 AnimInstance 消费快照及 Idle/Move/Jump/Fall/Land 表现；本次使用独立 `-game` 窗口，不是 PIE。

## A：角色动画数据与基础状态

### 修改产物

- `Source/SkillLab/Public/SkillLabCharacterAnimationStateComponent.h`、`Private/SkillLabCharacterAnimationStateComponent.cpp`：新增角色拥有的动画快照及基础状态计算。读取 `ACharacter`/`UCharacterMovementComponent` 的速度、方向和 Falling 事实，输出 `Idle`、`Move`、`Jump`、`Fall`、短暂 `Land`、状态持续时间与迁移序号；状态可由 Blueprint 读取，并记录状态迁移日志。
- `Source/SkillLab/SkillLabCharacter.h/.cpp`：在角色默认对象中创建组件，提供 Blueprint 访问器。
- `Source/SkillLab/Public/SkillLabCharacterAnimInstance.h`、`Private/SkillLabCharacterAnimInstance.cpp`：原生 AnimInstance 从角色动画状态组件读取快照，并将速度、方向、`ShouldMove`、`IsFalling` 同步到既有 AnimBP 属性。
- `Content/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.uasset`：父类改为 `SkillLabCharacterAnimInstance`，移除已由 C++ 快照接管的旧 EventGraph 计算；Jump/Fall 与 Land 回转使用动画序列结束转换，删除会因 `!IsFalling` 立即退出的重复 Land 转换。
- `Source/SkillLab/SkillLabPlayerController.h/.cpp`、`Source/SkillLab/Public/SkillLabTestCommandsComponent.h`、`Private/SkillLabTestCommandsComponent.cpp`：增加 `SkillAnimationSmoke` 验证入口，自动提供移动输入并触发跳跃；按快照状态变化请求游戏视口截图，写入 `Saved/AnimationStateVisual/`。

### 运行链与职责

`CharacterMovementComponent -> USkillLabCharacterAnimationStateComponent -> SkillLabCharacterAnimInstance -> ABP_Unarmed`。组件集中承担移动事实采集与状态语义；原生 AnimInstance 只做快照到既有 AnimBP 变量的桥接，动画姿势和状态机求值仍由 UE 动画系统承担。旧 AnimBP 的 `ShouldMove` 条件为 `GroundSpeed > 0.01` 且 `GetCurrentAcceleration() != 0`；新实现保留这两个事实作为 Move 的进入条件，在加速度采样间隙用 0.15 秒输入宽限/残余速度保持 Move，并在宽限结束且速度低于阈值后退出。`SkillLabSkillAnimationComponent` 继续只处理技能 Montage，本次未改技能运行时、训练假人逻辑或 `Config/SkillDefinitions.json`。

### 验证证据与限制

- UE 5.8 主工程 `Build.bat SkillLab Win64 Development -Project=... -WaitMutex -NoHotReloadFromIDE` 成功，最新快照和 AnimInstance 代码经过 UHT、编译和链接。
- 为绕过主工程当前其他 Editor-only 模块的构建错误，在 `Saved/AProbe` 隔离项目副本中构建 `SkillLabEditor` 成功，并用更新的 Editor 模块启动主工程默认地图。最终状态日志 `Saved/Logs/SkillLabAnimationASmoke-Acceptance.log` 确认 `ABP_Unarmed_C_0 consumes CharacterAnimationState`，默认角色完成 `Idle -> Move -> Jump -> Fall -> Land -> Move -> Idle`；Move 阶段没有重复 Idle/Move 抖动，Land 根据仍有效的输入回到 Move，输入停止后回到 Idle。
- 更正此前对旧 smoke 的记录：旧 `-nullrhi` 日志并非“没有可读状态证据”，它确实证明 AnimInstance 消费快照并记录基础状态迁移；它不渲染可见姿势，因此不能作为表现验收。现在另有 RHI 窗口截图证据。
- AnimBP 资产命令行编译报告为 `Compile status=3 errors=0` 且 `Saved=true`；读回确认父类、旧 EventGraph 和转换属性已写入资产。此前 `migration-editor.log` 中“Land 到 Locomotion 永不触发”告警来自修复前仍有空规则图的资产；修复写回后 Land 出口设为 `bAutomaticRuleBasedOnSequencePlayerInState=true`、`automaticRuleTriggerTime=0.05`，删除重复出口，最终 RHI 运行日志和截图均记录 Land 到 Locomotion 的回转。
- 可复现的默认角色可视入口：在 UE 5.8 主工程空闲时执行 `UnrealEditor.exe <绝对路径>/SkillLab.uproject -game -windowed -ResX=1280 -ResY=720 -nosound -nop4 -NoSplash -abslog=<绝对路径>/Saved/Logs/SkillLabAnimationVisual-Acceptance.log -ExecCmds=SkillAnimationSmoke`。该入口临时将验证相机臂缩至约 540 uu，并按状态变化请求截图到 `Saved/AnimationStateVisual/`；本轮 RHI 输出为 `00_Idle.png`、`01_Move.png`、`02_Jump.png`、`03_Fall.png`、`04_Land.png`、`05_Move.png`、`06_Idle.png`，截图由默认角色视口生成，不是 NullRHI 捕获。
- 未修改 `BP_SkillLabCharacter`；默认角色继续使用现有 Mesh/Skeleton 和 `ABP_Unarmed`。截图验证默认资源确实产生 Idle、Move、Jump、Fall、Land 姿势并回到 Locomotion；本轮没有建立与迁移前画面的一对一主观质量对照。Land 语义状态窗口为 0.20 秒，AnimBP 落地资源过渡由序列结束规则驱动。

## 交付格式

实现 Agent 交付时填写：

- 修改的代码、配置、Blueprint、资产和工具；
- 每项架构要求的代码落点；
- 资源和配置的权威来源；
- 构建、资产编译、运行和回归入口；
- 已验证、未验证和需要回交的问题。

## B：结构化动画内容与 Montage 生产

### 当前状态

B 已完成代码、结构化入口和 UE 5.8 实机验证。编辑器模块构建、只读读回、独立负例、项目副本写回保存和 `ResetLoaders` 重载均已执行；基线 Montage 未被写回。

### 修改产物

- `Source/SkillLabAnimationEditor/`：新增 Editor-only 模块、结构化内容类型和 `USkillLabAnimationContentCommandlet`。
- `SkillLab.uproject`：注册 `SkillLabAnimationEditor` Editor 模块。
- `schemas/skilllab-animation-content.schema.json`：结构化 Montage Patch Schema。
- `Config/Animation/FireballMontage.json`：代表 Montage 的可 diff 局部 Patch 示例。
- `Config/Animation/InvalidAnimationSpec.json`：缺失资产负例。
- `Config/Animation/InvalidPlayRateSpec.json`：非法 Play Rate 规格负例。
- `Docs/ProgramWorkflow/AnimationWorkflow/Batches/Batch01-CharacterFoundation/AnimationContentTool.md`：调用入口和验证边界。

### 实现与职责

Commandlet 在 Editor 进程内读取 JSON，加载 `UAnimMontage`，读出并校验 Skeleton、Slot、Section/NextSection、片段、Notify 时间/对象、Blend 和 RateScale；`-Write` 模式执行局部 Patch、保存 Package、`ResetLoaders` 后重新加载同一资产，再输出读回报告。片段替换要求可加载 `UAnimSequenceBase` 且 Skeleton 与 Montage 一致。Runtime `SkillLabSkills` 不读取草稿，也未修改技能规则、技能数值或 C 的事件语义。

### 已完成的静态/结构化验证

- `SkillLab.uproject`、动画样本 JSON 和 Schema 均通过 Python JSON 解析检查。
- 已确认样本资产文件存在：`AS_FireballCast.uasset` 与 `MM_Pistol_Fire_Montage.uasset`。
- 已确认 Editor 模块、Commandlet、Schema 和样本路径均位于约定目录内。
- 负例输入分别覆盖不存在的资产路径和 `rateScale: -1.0`；Commandlet 已对类型、缺失字段、未知字段、负值和不匹配的 Notify 字段执行入口校验。

### 验证记录与限制

- 已确认 `D:\Epic\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe`、`Build.bat` 和 `UnrealEditor.exe` 存在；首次构建因遗漏 `GameplayTags` 模块依赖在链接阶段失败，补齐依赖后 `SkillLabEditor` 构建成功。
- 只读样本报告：`fireball-read.json` 返回 0，读出 Skeleton、`Arms` Slot、`Default` Section 和真实 Segment，`valid: true`。
- `InvalidAnimationSpec.json` 返回 4，报告 `MissingAsset`；`InvalidPlayRateSpec.json` 返回 3，报告 `InvalidSpec`/`rateScale must be greater than zero`。
- 项目副本写回报告 `copy-write-readback.json` 返回 0，包含 `saved: true`、`reloaded: true`、`valid: true`，并记录 `before.*`/`after.*` 的 RateScale、Blend、Slot、Section、Segment、Notify 差异；基线 `MM_Pistol_Fire_Montage.uasset` 未修改。
- `segment` 已实现首个 Slot 的首个片段替换；多 Slot、多片段精确身份 Patch 和复杂 NotifyState 编辑不在本轮最小范围。UE Montage API 只保存 Slot 名，Group 需由 Skeleton 注册表推导，入口拒绝 `SlotGroup.SlotName`。

## C：技能表现运行接入与组合层

### 实现与职责

- 沿用 `USkillLabSkillPresentationRouter -> USkillLabSkillAnimationComponent`：`Started` 仅启动动作，`PhaseChanged(Active)`、`Hit`、`EffectApplied`、`BuffApplied`、`DamageTick` 等继续作为运行/效果事实广播，`Cancelled`/`Completed` 仅停止相同 `CastId` 的动作。Timer 和技能行为链仍拥有释放、扣费、目标、命中、效果和 Cast 终态。
- 修正公共动作交接：新 Cast 在资源解析前停止旧 Cast；无定义或资源不可用时只降级为无施法动作，不遗留前一 Cast 的动作。直接 Montage 的 Notify 仍按 Montage 对象及活动实例 ID 校验；从配置 `castAnimation` 创建动态 Montage 时，也接受其源 Sequence 作为 Notify 报告对象，但仍必须匹配当前 Montage 实例 ID 和 CastId。
- 共用内容入口仍为 `FSkillLabSkillDefinitionProvider::ResolvePresentationBinding` 与 `castMontage`/`castAnimation`。定义测试增加 Sequence 绑定可解析和两种绑定互斥覆盖。没有新增技能 ID 分支、技能专属 C++、流程框架或 `SkillDefinitions.json` 参数。
- 三层归属：稳定内核是现有 Router、AnimationComponent、Notify 身份检查和解析器；组合层仍由统一 `Started -> Active 事实 -> Cast 终态` 事件流程承担，没有证据要求额外组合组件；文本配置仅持有已有软资源引用。B 的资源生产工具继续独立于运行时消费。

### 验证与限制

- UE 5.8 `Build.bat SkillLab Win64 Development -Project=... -WaitMutex -NoHotReloadFromIDE` 成功；`SkillLabEditor` Development 同样成功。Editor 构建包含 SkillLabSkills 运行时与 DefinitionValidation/CastLifecycle 测试代码的编译和链接。
- 自动化使用 EditorContext 入口执行；修复软资源互斥校验后，`SkillLab.Skills.CastLifecycle` 与 `SkillLab.Skills.DefinitionValidation` 均通过。`-game` 入口会报告 `0 tests performed`，不能作为这两项自动化的执行方式。
- 静态与运行链均确认：火球仅在 Active 后生成投射物，区域爆发、修复、燃烧各自仍在原技能行为中产生即时伤害、治疗或独立持续效果；C 没有改变这些语义。四项配置均沿用同一 `castAnimation` 解析和播放路径，并由一次性 smoke 实际请求和观察。
- A 的基础姿势数据链尚需独立 Review 放行，当前工作树也包含 A 的未提交文件；C 没有修改角色、AnimBP 或基础状态范围。缺失/不可用动作资源降级、Notify 时间对齐、动作取消后的姿势恢复和 UE PIE 可视质量仍未验证。

### C 后续运行复验

- 独立 Review 的“先修复后继续”仅针对运行证据不足，没有确认新逻辑缺陷。收到复验请求时先确认此前遗留的本任务 `UnrealEditor-Cmd`（PID 26736）仍在运行且持续占用资源，使用其对应会话 Ctrl+C 结束后复查 UE/UBT 进程；A 的放大可视 smoke 完成并释放 Editor 后才继续。
- `SkillLab-C-SkillSmoke-20261018.log` 为默认地图 NullRHI 烟测。入口加载并绑定四个 SkillId、接受 4 项定义；NullRHI 没有鼠标地面/单位投射，故火球、区域爆发、燃烧在输入目标层被拒绝，未进入运行 Cast。Mend 自身目标进入 Cast 1 并播放动态 Montage `AnimMontage_0`。该记录不能作为四技能效果验收。
- `SkillLab-C-CastHandoff-20261018.log` 为默认地图非 NullRHI `SkillTestRecover,SkillReset,SkillTestRecover,quit` 序列：Cast 1（Mend）播放 `AnimMontage_0`；Reset 产生 Cast 1 Cancelled；连续新 Cast 2 播放 `AnimMontage_1`。日志确认同一动画路径关联两个不同 CastId，且旧 Cast 取消先于新 Cast 动作。命令行启动的真实 game viewport 仍没有可用鼠标位置，因此不能完成火球、区域爆发与燃烧的目标操作；Montage 被调用播放，但未观察 Notify 对齐或屏幕姿势质量。
- Automation 启动参数已查明：`-game` 会将 `SkillLab.Skills` 两项测试入队但报告 `0 tests performed`，不是 EditorContext 测试运行方式。Editor 模式 `UnrealEditor-Cmd ... -nullrhi -ExecCmds="Automation RunTests SkillLab.Skills" -TestExit="Automation Test Queue Empty"` 成功执行两项测试。首次执行 CastLifecycle 通过、DefinitionValidation 失败，暴露软资源互斥校验错误：原实现用 `TSoftObjectPtr::IsValid()` 检查配置资源，未加载的两个有效路径绕过互斥。已改为检查 `IsNull()`，未改变配置契约。
- 修复后重新构建 `SkillLabEditor` 成功；复跑日志 `SkillLab-C-Automation-Editor-20261018.log` 显示 CastLifecycle 与 DefinitionValidation 均 `Result={Success}`，`2 tests performed` 并触发 `Automation Test Queue Empty`。
- 当前已证明四项定义/快捷键均加载绑定、四个 SkillId 均可通过公开运行时请求进入 Cast、公共动作路径按 CastId 隔离、四种效果事实和终态均可观察，且配置解析自动化通过。缺失/不可用资源降级、Notify 身份、动作取消后的姿势恢复和可视 PIE 质量仍未验证。A 的可视烟测日志 `SkillLabAnimationVisual-Acceptance.log` 已确认 AnimInstance 消费基础状态并记录 Idle/Move/Jump/Fall/Land 迁移，但 A 尚未独立 Review 放行，故 C 的姿势恢复/视觉部分仍标为依赖待验。
- 为解决命令行没有鼠标投射导致的三项技能无法复现，新增现有 `USkillLabTestCommandsComponent` 的一次性 Exec smoke：`SkillPresentationSmoke`。它只通过公开 `USkillLabSkillRuntimeComponent::CastSkill` 请求当前四个固定 SkillId，临时生成一个近距离训练目标，按 Fireball、AreaBurst、Mend、BurningBuff 顺序执行，监听现有 `OnNativeSkillEvent`，完成后销毁临时目标并自动退出。该入口不读取或修改 `SkillDefinitions.json`，不复制玩法行为，也不建立可配置测试框架。
- `SkillLab-C-PresentationSmoke-20261018.log` 为 UE 5.8 `-game -nullrhi -ExecCmds=SkillPresentationSmoke` 实证：
  - `Skill_Fireball` Cast 1：Started/Active，`SkillLabAnimation` 播放 `AnimMontage_0`，Projectile Hit，`EffectApplied` 伤害 40，Recovery，Completed（Reason=Hit）。
  - `Skill_AreaBurst` Cast 2：Started/Active，播放 `AnimMontage_1`，目标命中，`EffectApplied` 伤害 25，Recovery，Completed（Reason=AreaHit）。
  - `Skill_Mend` Cast 3：Started/Active，播放 `AnimMontage_2`，`EffectApplied`/HealingApplied，Recovery，Completed（测试角色满血，实际治疗量为 0，技能事实仍成立）。
  - `Skill_BurningBuff` Cast 4：Started/Active，播放 `AnimMontage_3`，`BuffApplied`/BurningStarted，施法完成后继续出现多次 `DamageTick`（8、8、8、8、3），随后 `BuffRemoved`/EffectRemoved；持续效果生命周期独立于动作终态。四个 CastId 均按 1→4 递增，事件中的 SkillId/CastId 与动作日志一致。
- 本次 smoke 的真实运行证据补齐了四技能的请求、动作开始、Active/效果事实和终态；仍未验证缺失资源降级、Notify 时间对齐、动作取消后姿势恢复、可视 PIE 质量，以及 A 基础姿势 Review 放行。原有 `SkillLab-C-CastHandoff-20261018.log` 继续覆盖取消后新旧 Cast 动作隔离。
