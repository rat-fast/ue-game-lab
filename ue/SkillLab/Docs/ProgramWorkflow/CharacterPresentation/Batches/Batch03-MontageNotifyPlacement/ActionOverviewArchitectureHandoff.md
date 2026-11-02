# Batch03：AnimSequence 动作总览架构交接

## 1. 定位、来源与审核状态

本文件交接通用 AnimSequence 动作观察能力及其向 Notify 配置流程提供的输出边界。以下架构核查记录于 2026-10-27；工具随后已实现并生成资产级总览，当前实现、输出结构和验证证据以 [ActionOverviewImplementation.md](ActionOverviewImplementation.md) 与 [美术工作流入口](../../../../ArtWorkflow/README.md) 为准。实现记录了尚待独立视觉比对的限制；总览产物本身不等于动作语义或游戏内表现验收。

权威需求为同批策划目录中的 [动作总览需求](../../../../Planning/CharacterPresentation/Batches/Batch03-MontageNotifyPlacement/AnimSequenceActionOverviewRequirements.md)，重点是第 3、5、6、7、9 节；[批次入口](../../../../Planning/CharacterPresentation/Batches/Batch03-MontageNotifyPlacement/README.md) 和 [Notify 需求](../../../../Planning/CharacterPresentation/Batches/Batch03-MontageNotifyPlacement/MontageNotifyPlacementRequirements.md) 用于界定下游消费关系。项目约束来自 `Docs/ProjectContext/ProgramArchitectureContract.md`、角色表现基线 README/02/03，以及批次入口要求的技能生命周期、反馈基线。

已有 [ArchitectureHandoff.md](ArchitectureHandoff.md) 继续承担 Notify 内容配置与运行接入交接。本文不重建 Notify 架构，不定义事件协议或技能/Lua 行为；批次入口分别链接动作总览与 Notify 交接。下文保留当时的架构分析，已落地事实以实现记录和美术工作流入口为准。

以下分别标注策划要求、已核对事实、架构建议、实现期核验及待决定事项。建议的工具形式、语言、目录名和数据格式不是产品约束，也不因本交接而冻结。

## 2. 策划已明确的交付边界

- 输入为任意满足明确兼容条件的 `AnimSequence`，支持批量，不能绑定某个技能或只支持当前四技能。
- 对每个源帧生成稳定角色图像，输出可读分页图、帧号/时间码和机器可读 manifest，保留逐帧到页/格子的完整索引。
- 输出根路径可配置并持久保存；后续默认沿用。按资产和生成批次隔离，重复生成不静默覆盖，返回实际输出位置，路径或生成失败必须明确反馈。
- 记录 Skeleton、预览 Mesh 及来源、帧时间规则、相机/渲染条件、Root Motion 核对范围、启用的辅助信息及生成身份。
- 观察材料可供 AI 形成落盘的动作阶段、候选时点、依据和不确定性；这些判断不能伪装成已配置 Notify 或业务事实。
- 生成器不写 Notify、不修改 Montage、不决定技能事件，也不替代最终游戏内验证。

分页密度、默认视角/额外视角、辅助层默认开关、语义结果载体和工作流编排仍按需求第 9 节交用户/总控讨论。未确定这些偏好不妨碍静态架构核查。

## 3. 已核对项目事实与可复用能力

### 3.1 Editor 动画内容工具

`SkillLab.uproject` 关联 UE 5.8，已注册 `SkillLabAnimationEditor` Editor 模块。其 `Build.cs` 依赖 Engine、Json、JsonUtilities、AssetRegistry、UnrealEd 等，并私有依赖 Skills/Presentation。现有入口为 `USkillLabAnimationContentCommandlet`：

```text
-run=SkillLabAnimationContent -Spec=<文件> -Report=<文件> [-Write]
  -> ReadSpec
  -> 加载 UAnimMontage
  -> ReadMontage / ValidateMontage
  -> 可选 ApplySpec / SavePackage / ResetLoaders / 再读取
  -> WriteReport
```

核对 `SkillLabAnimationContentCommandlet.cpp`、`SkillLabAnimationContentTypes.h` 和仓库 `schemas/skilllab-animation-content.schema.json`：

| 已有能力 | 对动作总览的复用边界与缺口 |
| --- | --- |
| 按对象路径加载 Montage，读取 Skeleton、RateScale、Blend、Slot、Section 和片段引用 | 可参考资产定位、结构读取和 issue 定位方式；当前入口不能把 Sequence 作为主输入。 |
| `segment` 写入可加载的 `UAnimSequenceBase`，比较 Skeleton | 这只是首个 Slot/首个 Segment 的替换路径，不能证明逐帧 Sequence 读取或预览兼容能力。 |
| JSON 解析、未知字段拒绝、机器可读 issues、命令退出码 | 可沿用项目工具风格；既有 Patch schema 不适合直接充当观察 manifest。 |
| Section 以字符串报告，Segment 只报告资源路径 | 缺少 Segment 时间裁剪、轨道起点、速率和循环身份，不足以建立可靠帧到 Montage 映射。 |
| Report 路径由每次调用传入 | 未提供持久输出根路径、资产/生成版本目录和批次隔离。`WriteReport` 未检查 `SaveStringToFile` 返回值，不能照搬为可靠产物成功判据。 |
| 保存和重载代码 | 属于下游资产修改职责；动作总览只读，不调用这条写回链。现有代码重新加载非空本身也不能证明新进程冷加载。 |

在 `Source/SkillLabAnimationEditor`、项目相关源码及仓库 `tools/` 中未定位到逐帧渲染、分页合成、动作总览 manifest 或输出路径设置能力。此结论限定于已核对的项目入口，不声称引擎或外部工具没有这些能力。

旧 `Docs/ProgramWorkflow/AnimationWorkflow/Batches/Batch01-CharacterFoundation/AnimationContentTool.md` 只作为上述工具的操作说明和历史验证入口使用，不继承其单样本范围，也不把历史工具成功当作本能力通过。

### 3.2 当前播放链和实际输入

静态核对 `Config/SkillDefinitions.json`、`SkillLabSkillDefinitionProvider.cpp` 和 `SkillLabSkillAnimationComponent.cpp`：

```text
技能定义动作软路径
  -> ResolvePresentationBinding
  -> CastMontage.LoadSynchronous()
     或 CastAnimation.LoadSynchronous()
        -> CreateSlotAnimationAsDynamicMontage
  -> 角色 Mesh 的 AnimInstance.Montage_Play
```

- Provider 将 `castAnimation` 解析为 `TSoftObjectPtr<UAnimSequence>`，与 `castMontage` 互斥。
- 当前四技能均配置 `castAnimation`：Fireball 为 `/Game/SkillLab/Animations/AS_FireballCast.AS_FireballCast`，其余分别为 `MM_ChargedAttack`、`MM_Attack_01`、`MM_Attack_02`。这只是现有配置引用，不证明资产内部数据或视觉质量。
- 动态创建调用指定 `DefaultSlot`、BlendIn `0.08`、BlendOut `0.12`、传入播放率 `1.0`，之后 `Montage_Play` 传入 `1.0`。实际时间关系仍要计入引擎的 Sequence/Segment/Montage RateScale 等，不能仅凭两个实参宣称恒等映射。
- `Config/Animation/FireballMontage.json` 的旧工具示例指向 `MM_Pistol_Fire_Montage`，不是当前 Fireball 的 `castAnimation`。不能用该示例生成“实际游戏所用动作”的映射。
- `ASkillLabCharacter` 的原生构造不指定 Skeletal Mesh。`Content/Characters/Mannequins/Meshes/` 下能定位 `SK_Mannequin`、`SKM_Manny_Simple`、`SKM_Quinn_Simple` 文件；本轮未在 UE 内加载资产，未确认其实际类型、Skeleton、角色 Blueprint 默认 Mesh 或 Sequence 保存的预览 Mesh。文件名不作为绑定证据。

### 3.3 截图和骨骼信息的已有入口

`SkillLabTestCommandsComponent.cpp::CaptureAnimationSmokeState` 在 Idle/Move/Jump/Fall/Land 状态变化时请求游戏视口截图，写入固定 `Saved/AnimationStateVisual`；存在状态去重和截图忙时跳过。它不按源动画帧采样，也不确认本次请求已编码/落盘，不能作为动作总览生成入口。

`SkillLabPlayerController.cpp::LogRuntimeProbeSample` 读取特定角色的 pelvis/root 变换并写日志；它依赖运行角色和特定 AnimInstance，不是通用 Sequence 骨骼/Socket 导出器。可以参考坐标查询方式，不复用其运行依赖、采样时钟或骨骼名称。

## 4. 已核对 UE 5.8 接口与证据限制

本机引擎源码根为 `D:/Epic/UE_5.8/Engine/Source/`。以下为源码/声明核查，**均未构建、调用或生成渲染证据**：

| 能力 | 核对位置与结论 |
| --- | --- |
| 源帧数据 | `Runtime/Engine/Classes/Animation/AnimData/IAnimationDataModel.h` 提供源 `GetFrameRate`、`GetNumberOfFrames`、`GetNumberOfKeys`、`GetPlayLength`；`AnimSequence.h` 提醒源数据与目标 sampled keys/frame rate 不同。 |
| 时间采样 | `AnimSequenceBase.h/.cpp` 的 `GetTimeAtFrame` 使用 `GetSamplingFrameRate` 并夹取到长度；`UAnimSequence::GetSamplingFrameRate` 使用平台目标帧率。不能未经核实拿它当源 DataModel 帧率。 |
| 姿势求值 | `AnimSequence.h` 提供 `GetBonePose`、Additive 相关接口；`SkeletalMeshComponent.h/.cpp` 提供 SingleNode 路径的 `SetAnimation`、`SetPosition`、`RefreshBoneTransforms`。`SetPosition` 默认会触发通知，必须显式抑制通知并核验求值完成。 |
| 预览 Mesh | `AnimationAsset.h` / `Private/AnimationAsset.cpp` 与 `Skeleton.h` / `Private/Animation/Skeleton.cpp` 均提供预览 Mesh 读取。非 const 资产读取遇不兼容引用可调用 `SetPreviewMesh(nullptr)`；Skeleton 自动查找也会更新预览引用。只读工具应读取已保存引用并独立校验，避免隐式修复或回存。 |
| 渲染 | `Runtime/Engine/Public/PreviewScene.h`、`Editor/AdvancedPreviewScene/Public/AdvancedPreviewScene.h` 提供预览场景；`SceneCaptureComponent2D.h` 有 RenderTarget/CaptureScene；`Runtime/Engine/Public/UnrealClient.h` 有像素回读。接口存在不等于 commandlet/RHI、资源初始化和同步条件已验证。 |
| Root Motion | `AnimSequence.h` 的 `HasRootMotion()` 返回启用标志，另有 Root Lock、ForceRootLock、归一化缩放及范围提取接口。启用标志不等于检测到非零位移，也不等于游戏角色已移动。 |
| Montage 映射 | `AnimCompositeBase.h` 的 `FAnimSegment` 提供 StartPos、AnimStartTime/EndTime、AnimPlayRate、LoopingCount 和 `ConvertTrackPosToAnimPos`；其有效播放率包含源动画 RateScale。`Private/Animation/AnimMontage.cpp` 动态创建路径建立临时 Montage、Segment 和 Default Section。 |

这些证据支持下节的最小方案建议，不能据此宣称所有 Sequence 已兼容、预览 Mesh 已可用或 GPU 渲染已成功。

## 5. 架构建议：只读观察流水线

### 5.1 职责与落点

建议在现有 `SkillLabAnimationEditor` 职责边界增加独立的只读动作总览入口，让渲染任务与原有 Montage Patch/保存入口可独立调用、独立验证。若实施中选择脚本或外部图像合成，仍由同一观察任务负责输入/输出完整性；不新增游戏 Runtime 依赖，不把相机、分页或内容语义塞入技能运行组件。

```text
批量资产输入 + 已保存输出根路径 + 预览条件
  -> 只读解析、兼容检查、帧时间表
  -> Mesh 选择和固定预览条件
  -> 各源帧姿势求值、渲染、像素回读
  -> 标注和分页、完整性检查
  -> manifest 和批次结果发布、返回实际目录
  -> AI 读取图片与索引，独立保存语义结果
  -> Notify 流程消费；按实际播放目标另外核对映射
```

建议按实际需要拆出三个可独立维护的职责：UE 资产/预览采样、分页与产物发布、用户设置/调用入口。不是强制三个类或三个 Agent；接口共享时可由同一实现任务完成。稳定执行、校验和清理适合放 Editor C++；可调整预览参数、批量输入和资源引用保持为可读配置。Blueprint 可作为调用/预览外壳，Lua/MCP 均不是本功能前提。最终语言、命令/编辑器入口和格式由实现期依据核验选择。

最小建议落点：

- `Source/SkillLabAnimationEditor/Public/`、`Private/`：新增动作总览请求、结果及只读执行入口，名称待实现确定；必要公共读取逻辑经核对后从旧 Commandlet 局部提取，避免把全部工作追加到其单文件。
- `SkillLabAnimationEditor.Build.cs`：仅按实际选定的预览、编码、设置接口补依赖；不预先要求整套 Movie Render Pipeline、Python、Slate 或新插件。
- 设置入口：保存个人/本机输出根路径；可变预览条件与资产覆盖由单一权威配置提供，不在 C++、设置与 manifest 中分别手改同一值。manifest 只是本次实际条件的快照。
- 如需外部合成脚本及 schema，分别落仓库 `tools/` 和 `schemas/`；这只是选用外部流程时的目录约定，本轮不新建脚本或 schema。

原 Notify 写回 schema、运行组件、技能定义、Lua 和资产不属于此任务的默认修改落点。实际使用绑定仅在请求映射时只读解析。

### 5.2 兼容条件与预览 Mesh 来源

建议请求包含一个或多个完整 Sequence 对象路径，可选明确 Mesh 覆盖、预览方案和要核对的播放目标。先逐项检查加载类型、Skeleton、可求值数据、有效时间表、Mesh 兼容、依赖加载和渲染环境，再报告实际兼容范围。按能力拒绝并定位原因，不按技能 ID 或固定资产名单白名单限制。

Mesh 选择建议按“明确覆盖 → 动画资产保存的预览 Mesh → Skeleton 保存的预览 Mesh”顺序。每次记录候选来源、采用路径及兼容检查结果。缺失时可以报告可选候选，由调用者明确指定；不要自动拿游戏 Pawn 或目录中第一个 Mesh 当成功结果，也不回存预览引用。兼容骨架/重定向路径若被实现支持，必须记录所用机制；不能从旧工具的 Skeleton 指针相等检查推断 UE 全部兼容规则。

普通绝对姿势 Sequence 是最直接路径。Additive 需要可解释的基准姿势和求值规则；缺少基准时报告不兼容及原因，不能把增量当最终角色姿态。依赖重定向、后处理 AnimBP、Control Rig、物理/布料或多 Mesh 组合的最终效果，要明确本次仅观察源 Sequence 的范围；若其必需依赖导致无法还原可解释姿态，应失败或标为不支持该条件，不生成伪总览。具体支持情况属于实现期核验，不在本轮无证据承诺通用支持。

### 5.3 源帧时间表和逐帧渲染

建议以 Editor 可读的源 DataModel 建立源帧时间表，记录有理帧率、帧间隔数量、采样点数量、动画长度和末端规则。帧间隔与采样点通常并非同一计数；不得只用 `round(时长×FPS)` 作为“总帧数”。约定帧号从 0 开始，每个实际源采样点有唯一帧号和秒值；末端样本是否存在及其时间由数据核验决定。循环动作仍保留源末端规则，不擅自删除相似首尾帧。

源与平台目标帧率不同时仍按源时间点求值，记录实际使用原始/压缩数据的路径及差异。若源 DataModel 不可读，不静默改为目标帧率后称为源逐帧；报告不兼容或单列明确的采样模式，后者不能冒充本需求验收。

可行候选入口是隔离的预览场景 + SkeletalMeshComponent SingleNode 定点求值 + SceneCapture/RenderTarget 回读；也可在核验后选择编辑器预览接口。每帧显式定位时间，关闭自动播放、通知派发和无关业务 Tick，完成骨骼/组件变换刷新后才捕获，并确认渲染和编码完成。不能依赖真实时间播放后的偶然截图，不能让后台 Tick 把截图推到下一帧。预览不创建正式游戏角色或技能运行对象，加载动作上的其他 Notify 也不得产生副作用。

渲染需有效 RHI；NullRHI 可验证元数据，但不能作为图像验收路径。如果选 Commandlet，要验证其渲染启用参数、世界/场景初始化和资源生命周期；若该环境不足，可选择有渲染能力的编辑器执行入口，保持同一只读请求/产物边界。无论采用哪条路径，实际调用方式由实施后给出，本文不虚构可用的动作总览命令。

### 5.4 稳定画面与辅助层

建议一次任务内锁定 Mesh、组件朝向/缩放、LOD、相机位置/投影/FOV 或正交宽度、分辨率、光照、曝光、背景/地面参照和后处理。可先扫描动作包围范围，再为整段锁定取景；不能逐帧自动跟踪/缩放。记录全段裁切检查，若长距离位移导致姿态不可读，应报告并选择明确的预览方案，不能靠动态镜头掩盖。

Root Motion 至少区分启用设置、源 root 运动/提取结果和本次预览应用方式。建议保留按帧或可回查区间的 root 平移/旋转与坐标系，说明是否去除/应用位移、Root Lock 是否影响可见姿态；固定角色的姿态图可以配轨迹参照，但必须明确不是世界运动还原。缺少曲线/提取结果时写“未知/不可核对”及范围，不能用零位移代替未知。

骨骼、Socket、武器挂点和轨迹为可选层。启用时记录名称、父骨、Skeleton/Mesh 来源、帧号、变换坐标系/单位以及与图片层的关系；实际武器外形须有明确预览附件，不能由 Socket 存在推断。默认画面应保留主体可读性，过密标签可另出辅助页/数据。未启用、没有该项和读取失败分别表示，Socket 的离线姿态不等于游戏 AnimBP/IK/融合后的精确位置。

## 6. 分页、manifest、输出路径与版本建议

### 6.1 输出内容的逻辑契约

最终文件格式和字段名由实现选择，下列是需要表达的语义，不是已冻结 schema：

| 信息组 | 最少可回查信息 |
| --- | --- |
| 生成身份 | manifest 格式版本、生成器版本、UE 版本、生成时间及时区、唯一生成 ID、批次 ID、项状态。 |
| 输入快照 | 完整对象/包路径、实际类、Skeleton、可识别输入变化的内容指纹；记录依赖的 Mesh/预览设置版本，不能只靠资产短名。 |
| 时间规则 | 源有理帧率、时长、帧间隔数/采样点数、帧号起点、末端/循环规则、使用的数据/求值路径及源与目标采样差异。 |
| 预览条件 | Mesh 路径与来源、组件变换、相机、尺寸、LOD、地面/空间参照、光照/曝光/后处理、Root Motion 和可选层设置。 |
| 页帧索引 | 每页文件、视角 ID、页码、源帧范围；每帧精确时间、页/行/列或像素格区域、渲染/编码状态。页、行、列起点须明示。 |
| 辅助与映射 | Root Motion 核对范围、骨骼/Socket/轨迹引用；存在时提供第 7 节的播放目标映射，不可用时明确状态。 |
| 产物与错误 | 配置输出根路径、解析后的实际根路径和项目录、文件清单及完整性标识、错误阶段/资产/帧/页/原因。 |

每格直接标注帧号和时间码，每页有资产/生成身份、视角和范围；图像内标注应与 manifest 一致。分页按固定可读格子尺寸增长页数，最后一页空格标为空，不把缺失帧伪装为空格。额外视角按独立视角维度索引，不能挤占或改变原帧号。

AI 语义结果独立于生成器事实：保存观察生成 ID/输入版本、帧或时间范围、图页/格子证据、阶段名称、候选时点、判断依据和不确定性。生成器不必内置语义模型；正式保存载体由用户/总控决定。下游不得只转述一次性聊天结论而丢失证据引用。

### 6.2 路径持久化与批量发布

建议将输出根路径作为“当前项目、当前用户/本机”的 Editor 设置持久保存，避免把某台机器的绝对目录强制共享给其他使用者。可采用 UE 用户设置或等效持久配置；最终保存位置、设置命令/界面由实现说明并通过进程重启验证。未配置时明确要求先配置；已配置但不可用时失败，不改写默认值、不静默回退 `Saved/`。临时覆盖不是本次必要能力，若提供，应显式标注且不改持久默认值。

目录示意为 `<根>/<资产完整路径派生的稳定键>/<唯一生成ID>/`；可加批次父目录，命名不是强制格式。稳定键应避免同短名和路径非法字符冲突，生成 ID 应保证并发/同秒重跑互不覆盖。即使输入未变也不能覆盖旧产物；如后续支持复用缓存，必须返回明确命中的已有版本，而不是伪装成新生成。

建议先在本次专属目录记录进行中状态，逐帧/逐页核验后才发布完成 manifest。页数、帧覆盖、图片可解码性、尺寸、文件存在及索引一致性均成功才完成该资产；写文件/编码/磁盘空间/权限错误必须影响状态和返回结果。失败保留可定位信息，不用复制邻帧、占位图或黑帧补齐为成功。

批次报告逐资产列实际目录和结果，允许其他独立资产完成，但整批不能在有失败时只返回“全部成功”。失败资产及缺失页帧不可供正常语义消费；播放映射失败则明确阻止该映射用于 Notify 配置，不必否定已完整的源动作图像。输入在生成期间变化时，报告快照不一致并重试或失败，不能混合两个版本。

建议默认保留已有结果、只清理本次拥有的临时文件；不引入自动删除历史产物的策略。记录持久配置位置和实际目录，确保后续 Agent 无需猜测 `Saved/` 或最近修改文件。

## 7. 交给 Notify 配置流程的映射边界

源 Sequence 没有唯一的“实际 Montage”。同一 Sequence 可被多个 Montage、Slot、Segment、循环和裁剪区间使用，当前动态 Montage 也没有稳定的已保存 Montage 资产路径。不要要求总览生成器扫描全项目来猜最终目标。

建议总览基础产物始终以 Sequence 源时间为权威；需要 Notify 配置时，由调用方提供实际播放绑定或明确目标 Montage。映射读取仍只读，输出绑定来源和版本，不创建 Montage、不修改配置。接口可以位于同一 Editor 工具中的按需读取能力，也可由 Notify 流程读取真实目标后引用观察版本；避免双方各维护一份目标时序权威数据。

| 实际路径 | 应交出的映射与限制 |
| --- | --- |
| 当前 `castAnimation` 动态路径 | 引用技能定义来源、Sequence 版本和动态创建调用条件，用“动态播放方案”标识目标，不伪造持久 Montage 路径。源秒到 Montage 秒需由创建后的 Segment/RateScale 核实，不能预设 1:1。 |
| 保存的 Montage | 回读完整对象、Slot/轨道、Segment 索引与引用、StartPos、源裁剪范围、有效速率/方向、循环次数、Section 范围/链接和 Montage 播放条件；同一源帧可能无对应、多处对应，必须分别返回。 |
| 未指定/无法解析目标 | 总览仍可用于动作审核，但标明“仅源时间，未建立实际播放映射”；不能直接把候选源时间写入某个 Montage。 |

映射建议优先用引擎 Segment 转换规则核对，而非复制一套只适用于正向单段的换算；反向、裁剪边界、循环末端、多个轨道、Section 跳转和时间伸缩需有明确状态。无法可靠建立时返回缺失项/歧义。保存 Montage 的时间坐标和游戏墙钟时间不同：后者还受运行播放率、Section 行为、暂停/中断及融合影响，只能在下游运行验收核实。

交给下游的是：实际观察目录、完成 manifest、源帧/时间引用、独立语义结果及其依据、可选播放映射和未决项。下游负责把候选时点配置进真正使用内容、保存/回读、运行接入和游戏内验收；协议、技能事件含义与 Lua 行为继续按既有 Notify 交接处理，本文不定义它们。

## 8. 实现期核验与验证入口

### 8.1 现有入口能证明什么

- 元数据与旧工具复查：`D:/Epic/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe` 配合 `SkillLab.uproject -run=SkillLabAnimationContent -Spec=<spec> -Report=<report>`，**不带 `-Write`**。只能核对 Montage 现有报告，不能生成本需求总览。实际执行前仍需取得实现/运行授权。
- `SkillAnimationSmoke` 的截图、RuntimeProbe、既有自动化和 NullRHI 历史日志不能验证源逐帧图像、分页或持久路径。本轮没有重新执行。
- 总览执行命令、Editor 入口、输出根路径设置和批量调用已在 [ActionOverviewImplementation.md](ActionOverviewImplementation.md) 记录；资产级正式输出结构见 [美术工作流入口](../../../../ArtWorkflow/README.md)。
- 正式游戏入口 `Run_SkillLab_DX11.bat` 只留给下游最终时序验收，非本工具正常生成的必要路径。

### 8.2 实施后必须取得的证据

| 核验主题 | 可执行的判断方式 |
| --- | --- |
| 资产兼容 | 加载非技能绑定 Sequence 和多个 Skeleton 的样本；逐项记录 Mesh 来源。缺 Skeleton/Mesh、不兼容、Additive 缺基准分别明确失败，不回存资产。 |
| 源帧完整性 | 至少核对首/中/末帧、跨页边界、短/长动作、有理帧率及源/目标帧率不同情况；将时间表与源 DataModel 对照，确认没有缺帧、重复索引或末端夹取冒充。 |
| GPU 与姿势同步 | 在实际 RHI 下逐帧求值并回读；对比原动画同时间姿势，确认非滞后一帧、空白或未加载材质。记录通知/物理关闭、资源就绪及场景清理方式。NullRHI 不得通过图像项。 |
| 预览稳定 | 检查整段相机/比例恒定、主体不裁切、文字可读；重复生成的条件快照相同且姿态时间对应。渲染像素不必承诺跨机器完全相同，但条件差异必须可识别。 |
| 辅助信息 | 用有/无 root 运动的资产区分启用标志和实际轨迹；若启用 Socket/骨骼，抽查命名、坐标系和帧关联，关闭时明确记录。 |
| 分页与产物 | 遍历每个源帧恰有完整视角映射，打开各页并核对首末格、页范围和图中标注；编码/写入故障、缺页、缺帧均不能完成。 |
| 输出配置与版本 | 配置一次后重启执行环境再生成；同名不同路径、重复运行、并发/同秒运行均隔离。配置失效/无权限/磁盘写失败保留原配置并明确失败，返回实际目录。 |
| 批量和失败 | 多资产混合成功/失败，逐项可追溯；失败项不供语义消费、整批不报全部通过。核验输入变更后旧观察能被识别为过期。 |
| 播放映射 | 对当前动态路径和已指定保存 Montage 分别回读；覆盖裁剪、速率、循环、多次引用及未指定目标。源时间、Montage 时间、运行时间不混用。 |
| 语义交接与边界 | AI 基于真实页/格输出并保存阶段/候选/歧义，另一读取者能回查证据；生成前后确认输入包未改写，无 Notify 或技能配置写入。 |

不必为此建立全项目测试框架。时间换算、索引覆盖、目录隔离和失败传播适合定向自动化；图像质量、姿态正确和足够支持动作判断必须有实际可读图片与对照证据。实现期核验若否定建议路径，先报告影响和最小替代，再调整技术选择；不能删减已明确的产品结果。

## 9. 实施期最小阅读集合（历史交接）

以下是当时给动作总览实现任务准备的最小上下文路由；通用工具现已实现，当前操作与验证方法以 [ActionOverviewImplementation.md](ActionOverviewImplementation.md) 为准。该集合仍可帮助定位源代码职责，不构成新的实施授权要求。

1. 本文与动作总览需求；批次入口和 Notify 需求第 3、4、5、8 节用于消费边界。无需阅读整个 Notify 运行协议或 Lua 行为源码。
2. `Source/SkillLabAnimationEditor/SkillLabAnimationEditor.Build.cs`、`Public/SkillLabAnimationContentTypes.h`、`Public/SkillLabAnimationContentCommandlet.h`、`Private/SkillLabAnimationContentCommandlet.cpp`：确认现有 Editor 入口和可复用/不可复用的部分。
3. 旧 `AnimationContentTool.md` 与 `schemas/skilllab-animation-content.schema.json`：只在复用现有工具入口或数据处理时读取，不能把旧 Patch schema 当观察 schema。
4. `Source/SkillLabPresentation/Private/SkillLabSkillAnimationComponent.cpp`、`Source/SkillLabSkills/Private/SkillLabSkillDefinitionProvider.cpp`、`Config/SkillDefinitions.json`：仅在实现实际播放映射时读取。
5. 第 4 节已定位的引擎动画、预览、捕获和 Segment 接口及直接依赖；项目 `SkillLab.uproject`。必要时查看 `CharacterPresentation.md`、`TestAndVerification.md` 相关入口，不扩读其他模块。

`SkillLabTestCommandsComponent.cpp`、`SkillLabPlayerController.cpp` 的截图/Probe 代码只是本轮排除错误复用的依据，不是新生成器必须依赖或修改的文件。资产内部、真实 Mesh 绑定和渲染结果留实施后核验。

## 10. 已确认的产物边界与剩余验证

后续工作流已采用以下资产级归档结构，细节以 [美术工作流入口](../../../../ArtWorkflow/README.md) 为准：

1. 一个源 AnimSequence 对应 `Actions/<assetName>/`；分页总览图与精简 `overview.json` 放在该目录，动作语义放在同级 `observation.md`。语义记录应明确列出身体/肢体动作峰值和视觉线索峰值，并附帧/帧段及分页证据。
2. `Actions/Index.md` 提供文字检索入口，并链接到各资产目录的观察记录和全部分页图；不复制图片到集中总览目录。
3. `source-to-montage.json` 只记录真实建立并核验的 Sequence 到持久 Montage 映射。动作观察与配置职责分开，观察记录不表示 Notify 已配置或技能业务事件已发生。

仍需区分的验证项：

1. 生成器已通过真实 RHI 渲染验证；同一时刻与 Animation Editor 结果的独立视觉比对仍按 [ActionOverviewImplementation.md](ActionOverviewImplementation.md) 记录的限制处理。
2. 峰值记录给内容配置提供可复查的视觉时间锚点，但动作图不能确认亮点是否为特效，也不能确认释放、命中或技能效果的业务含义；点位选择仍由内容配置流程结合技能需求判断。
3. Sequence 时间只有在播放映射已明确且读回后才能换算为 Montage 时间。生成器不猜测同一 Sequence 在其他 Montage、Slot、裁剪或速率下的播放位置。

历史核查结论：当时项目缺少源帧渲染、稳定预览、分页/manifest、持久输出路径和发布流水线；这些通用能力已有实现记录和真实渲染验证。本文保留原架构分析，不再表示等待用户决定或等待启动实现。
