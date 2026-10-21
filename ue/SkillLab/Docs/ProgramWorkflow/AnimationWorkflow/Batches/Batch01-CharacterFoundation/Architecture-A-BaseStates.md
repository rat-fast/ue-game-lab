# A：角色动画数据与基础状态架构交接

## 范围与依据

本任务对应策划需求 `Infrastructure/00-AnimationDataAndBaseStates.md`，范围是把角色动画基础数据采集/计算和状态语义从 Animation Blueprint Event Graph 的核心判断中分离，并逐步建立 `Idle`、`Move`、`Jump`、`Fall`、`Land`。不设计技能动作、Montage 生产工具或训练假人的正式外观。

### 策划明确要求

- Event Graph 不再承担主要的项目级核心计算和状态判断。
- 五个基础状态可逐项进入、离开和观察；每次迁移能在相同入口与输入下和迁移前结果比较。
- 姿势求值、骨骼融合和动画压缩继续由 UE 执行，不改造引擎底层能力。

### 已核对项目事实

- `ASkillLabCharacter` 是 `ACharacter`，构造移动组件、相机、ASC、技能运行组件和技能动画组件；代码设置 `MaxWalkSpeed=600`、`bOrientRotationToMovement=true`，但未提供独立的动画状态数据提供器（`Source/SkillLab/SkillLabCharacter.h/.cpp`）。
- 静态读取 `BP_SkillLabCharacter.uasset` 可定位到实际 `AnimClass=/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed` 和 `SKM_Manny_Simple`。这是资产引用事实，仍需 UE 运行核对。
- `ABP_Unarmed.uasset` 可定位到 `BS_Idle_Walk_Run`、`MM_Idle`、`MM_Jump`、`MM_Fall_Loop`、`MM_Land` 和 `DefaultSlot`；这支持资源存在和引用定位，不证明状态迁移在运行时正确。
- 当前角色代码会隐藏训练场占位静态网格；`ConfigureAsTrainingDummy` 还会隐藏 Skeletal Mesh 并生成静态训练假人（`Source/SkillLab/SkillLabCharacter.cpp`）。因此训练假人不是本任务默认角色状态验收对象。
- 现有项目事实明确标记默认角色状态运行尚未在 UE 5.8 编辑器/PIE 中复验。

## 运行链

`ACharacter/CharacterMovementComponent` 提供速度、加速度、是否在空中和落地相关事实；角色侧的动画数据计算/状态语义入口生成稳定快照；`ABP_Unarmed` 或其 AnimInstance 只读取该快照并选择基础姿势资源；状态变化仍由 UE 动画求值和角色移动生命周期呈现。技能动作由 C 任务单独管理，不应写入基础状态判定。

当前已知默认 AnimBP/角色资产可定位，但仍须在 UE 中确认实际 Mesh、Skeleton 和 Anim Class 生效，再决定是给 `ASkillLabCharacter` 增加组件/接口，还是在已有 AnimInstance 的受控入口补数据读取。

## 可复用能力与职责边界

- 复用 `ACharacter` 的 `UCharacterMovementComponent` 和 UE 的 AnimInstance/状态求值；不复制移动事实到动画资产或 Event Graph。
- 角色类负责提供角色拥有的稳定动画输入，动画实例负责表现求值，动画资源负责片段/BlendSpace 内容。状态计算不得进入技能运行组件或 `SkillLabSkillAnimationComponent`。
- 角色训练场和占位网格继续由现有代码管理；本任务只以默认可控角色为运行入口。

## 建议修改落点

1. 在 `SkillLab` 模块内增加角色动画数据/状态的独立 C++ 组件或等价角色侧提供器，读取 Movement 事实并输出可定位的状态快照；具体类型名和载体保持实现选择。
2. 修改 `BP_SkillLabCharacter`/`ABP_Unarmed` 的接入点，使 Event Graph 只做必要转发，不继续承载核心速度、空中和落地判断。
3. 按 `Idle -> Move`、`Move/Idle -> Jump`、`Jump -> Fall`、`Fall -> Land`、`Land -> Idle/Move` 的可观察顺序逐项接入，保留每步对照记录；不扩展到受击、死亡或技能动作。

这是架构建议，不冻结状态机拓扑、BlendSpace、变量名、Blueprint 节点或最终资源引用。

## 实现必读文件

- `ue/SkillLab/Docs/Planning/CharacterPresentation/Batches/Batch01-AIAdaptedAnimationPipeline/Infrastructure/00-AnimationDataAndBaseStates.md`
- `ue/SkillLab/Docs/CharacterPresentation/01-CharacterPresentation.md`、`03-AssetsAndVerification.md`
- `ue/SkillLab/Docs/ProjectContext/SkillLabCodeArchitecture.md`、`Modules/CharacterPresentation.md`、`Modules/PlayerInteraction.md`
- `ue/SkillLab/Source/SkillLab/SkillLabCharacter.h`、`SkillLabCharacter.cpp`、`Source/SkillLab/Private/SkillLabPlayerInputComponent.cpp`
- `ue/SkillLab/Content/SkillLab/BP_SkillLabCharacter.uasset`
- `ue/SkillLab/Content/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.uasset`、`BS_Idle_Walk_Run.uasset`、`Jump/MM_Jump.uasset`、`Jump/MM_Fall_Loop.uasset`、`Jump/MM_Land.uasset`
- `ue/SkillLab/Config/DefaultEngine.ini`、`Run_SkillLab_DX11.bat`

## 直接依赖

- 上游：默认角色 Blueprint 的 Skeletal Mesh/Anim Class、CharacterMovement 状态和 UE 5.8 动画实例行为。
- 下游：C 任务的动作结束/取消后基础姿势恢复；恢复必须回到本任务状态入口，不由技能组件伪造移动状态。
- 不依赖 `Config/SkillDefinitions.json`、Montage 内容生产接口或具体技能行为。

## 验证入口与实现期验证项

- 静态核对：确认 `BP_SkillLabCharacter` 的实际 Skeletal Mesh、Skeleton、Anim Class 及 `ABP_Unarmed` 是否相符；确认状态输入来自角色移动事实而非 Event Graph 的重复计算。
- 运行入口：`DefaultEngine.ini` 的 `L_SkillLab_NavTest`/GameMode，使用 `Run_SkillLab_DX11.bat` 或等价 UE 5.8 PIE；以默认可控角色复现站立、移动、跳跃、下落、落地和再次站立/移动。
- 对每次状态迁移记录相同输入下的旧/新可观察结果、允许差异和未覆盖状态。验证应包含状态进入/离开条件、资源实际播放和基础姿势恢复。
- 现有自动化技能测试不能证明动画状态；本任务完成前不得以 `.uasset` 存在或编译通过代替运行证据。

## 实现期核对

默认可控角色的 Mesh、Skeleton、Anim Class 和 AnimBP 绑定属于实现期资产核对，先沿现有资源和接入点确认并完成需求范围内的可运行接入。只有核对后证实必须扩大策划范围、改变公共接口或改变角色运行语义才能满足需求时，才带着证据和最小替代方案回交；资产尚未验证本身不构成开工前阻塞。
