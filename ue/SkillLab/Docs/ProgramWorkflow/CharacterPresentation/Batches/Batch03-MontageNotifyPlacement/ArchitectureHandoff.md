# Batch03：持久 Montage Notify 基建与技能配置架构交接

## 1. 文档定位与阶段边界

本文是 Batch03 Notify 侧唯一有效的程序架构交接，依据当前批次需求、动作总览交接、`Docs/ProjectContext/ProgramArchitectureContract.md` 及角色表现/技能系统基线。本轮只完成结构核查和文档，不修改代码、Blueprint、配置、Lua 或 UE 资产，不把建议写成已实现事实。

正式 Notify 目标是已保存的 `UAnimMontage`。`castAnimation -> CreateSlotAnimationAsDynamicMontage` 仅记录为当前代码事实，不再作为 Batch03 Notify 写回路线。

本批拆为两块：

- **A 基建**：C++ Editor 读取持久 Montage，消费结构化 Patch，在白名单内新增/更新/删除 Notify，保存、重载、回读和差异校验；Runtime 复用现有 Notify 桥及身份、取消、迟到保护。
- **B 配置**：四个技能创建或绑定持久 Montage，`SkillDefinitions.json` 使用 `castMontage`，依据动作观察和业务语义填写 Notify Patch，仅为确有需要的技能切换 `GameplayEvent`；Lua 只在行为确实需要时修改。

依赖为：动作总览产物 -> A -> B -> 结构、运行和游戏内时序验收。A 的契约和验证是 B 的前置，B 不应通过内容缺口反向改写通用基建。

**当前状态更新（2026-11-03）**：A 的多事件 Notify Patch、持久 Montage 保存/重载/回读和运行桥接基础已实现并有工具/自动化验证，具体证据见 [ImplementationSummary.md](ImplementationSummary.md)。B 的四技能持久 Montage、Notify 和 `SkillDefinitions.json` 绑定已完成；写入、独立进程回读、差异校验和实际 `SkillReloadDefinitions` Provider reload 均通过。最终值及逐项证据见[配置交付说明](../../../../../Config/Animation/SkillContentDelivery.md)。尚未启动游戏或 PIE，游戏内时序和可见表现仍待用户验收。本文后续 A 方案描述是架构交接，不表示 A 仍待实现。

## 2. 已核对的真实运行链

### 2.1 播放和通知链

`Config/SkillDefinitions.json` 由 `FSkillLabSkillDefinitionProvider` 解析，`castMontage` 和 `castAnimation` 互斥。`USkillLabSkillAnimationComponent::PlayCastMontage` 先加载 `CastMontage`，没有时才加载 `CastAnimation` 并动态创建 Montage。B 完成后四个目标技能必须走 `castMontage`，动态对象不能成为 Notify 资产权威。

```text
SubmitSkillRequest
  -> CastId/生命周期 -> Started
  -> PresentationRouter / Lua
  -> SkillLabSkillAnimationComponent::PlayCastMontage
  -> 持久 UAnimMontage / AnimInstance
  -> USkillLabSkillAnimNotify::Notify
  -> ForwardAnimNotify -> ForwardCastEvent
  -> SkillLabSkillRuntimeComponent::ReceiveCastEvent
```

现有 `USkillLabSkillAnimNotify` 只携带 `FGameplayTag EventTag`。`ForwardAnimNotify` 要求动画属于活动 Montage、Montage 仍播放、存在有效 `ActiveCastId`，且 `FAnimNotifyMontageInstanceContext::MontageInstanceID` 匹配活动实例。运行时 `ReceiveCastEvent` 再按活动 Cast、阶段和声明的 Tag 裁定：Windup 匹配 `ReleaseEventTag` 时释放，Active 匹配 `BehaviorEventTags` 时转给行为。取消、Reset、EndPlay、阶段 Token、运行代次和行为身份检查提供迟到/旧 Cast 保护。Notify 到达不等于命中、伤害、治疗、Buff、扣费或完成事实。

### 2.2 现有 Editor 能力与验证范围

`SkillLabAnimationContent` Commandlet 读取持久 Montage 并输出结构报告；`-Write` 保存后通过包重载回读。现有简单内容 Spec 仍可表达单个 `notify` 及播放结构字段；Notify Patch 格式由 `schemas/skilllab-animation-content.schema.json` 的 Patch 分支约束，支持多个 instant/State 操作、更新/删除、稳定操作 ID、证据来源、白名单检查和差异报告。配置任务必须使用 Patch 格式和已验证的写入流程，不把旧单 Notify Spec 当成多事件配置入口。

临时 Montage 成功与失败输入、回滚和差异检查的证据见 [ImplementationSummary.md](ImplementationSummary.md)。这些证据验证的是通用写入能力，不证明四个正式技能的 Montage 已绑定，也不证明其 Notify 时序已经在游戏中验收。`Config/Animation/FireballMontage.json` 指向的 `MM_Pistol_Fire_Montage` 不是四技能实际绑定证据。

## 3. 单一权威来源与输入/输出契约

### 3.1 三类事实

| 层 | 权威内容 | 作用 |
|---|---|---|
| AI 文本源 | `Docs/ArtWorkflow/Actions/<assetName>/` 中的 `overview.json`、分页图、`observation.md` 和配置工作生成的 Notify Patch | 记录动作观察、候选点位、事件意图和受控修改请求；完整 manifest 可由配置工作复制到其 per-skill 目录供 Patch 校验 |
| UE 资产 | 目标持久 `UAnimMontage` 及已保存 `Notifies` | 正式运行载体和最终配置权威 |
| 运行时事实 | `CastId`、阶段、Montage 实例、`ReceiveCastEvent`、技能结果和游戏内观察 | 证明通知到达、业务接受/拒绝和最终效果 |

Sequence 总览以源 Sequence 时间为权威。观察记录的 `playbackMapping` 未建立持久 Montage 映射时，只能作为候选依据，不能把源秒数直接写入 Montage。

### 3.2 推荐 Patch 字段

实现可扩展现有 JSON 入口或在同一 Editor 模块建立唯一的新入口，但应支持下列契约：

```json
{
  "version": 1,
  "asset": "/Game/.../Skill_Fireball_Montage.Skill_Fireball_Montage",
  "source": { "observation": "Docs/ArtWorkflow/Actions/AS_FireballCast/source-to-montage.json", "generationId": "...", "inputPreviewFingerprintSha256": "..." },
  "duplicatePolicy": "error",
  "operations": [
    { "op": "upsert", "id": "Skill_Fireball.release.01", "kind": "instant", "eventTag": "SkillLab.Event.Cast", "time": 0.32,
      "evidence": { "sourceFrames": [18], "sourceSeconds": [0.30], "basis": "出手手势完成", "uncertainty": "low" } },
    { "op": "upsert", "id": "Skill_Fireball.window.01", "kind": "state", "stateClass": "<已注册 Notify State>", "eventTag": "<事件 Tag>", "startTime": 0.40, "endTime": 0.62 },
    { "op": "delete", "id": "Skill_Fireball.release.old" }
  ]
}
```

受控资产字段仅为 Notify 新增/更新/删除、事件 Tag、瞬时 `time`、State 的 `startTime`/`endTime`。`version`、稳定 `id`、`duplicatePolicy`、拥有范围和 `source/evidence` 是 Patch 元数据，不写入资产。未知 Notify State 类、未知 Tag、重复身份、越界时间、缺少目标或观察映射歧义都必须失败。实现必须定义稳定 `id` 如何映射到现有 `FAnimNotifyEvent`，不能用数组下标当身份。

输出报告至少包括目标资产/Skeleton、Patch 来源、逐项操作状态、变更前后身份/类型/Tag/时间或区间、保留的无关 Notify、`saved`、`reloaded`、`valid`、差异校验、错误代码和未完成范围。任一解析、校验、写入、保存、重载、回读或差异失败都不能宣称成功，并需证明 `assetUnchanged=true` 或等效事实。

## 4. 修改白名单和相邻职责

### 4.1 允许修改

- 目标 Montage 的 Notify/Notify State 新增、更新、删除；
- Notify 事件 Tag、瞬时时间点、State 开始/结束区间；
- Patch 的稳定身份、拥有关系和重复规则元数据；
- B 为实际使用持久动作而更新 `SkillDefinitions.json` 的 `castMontage` 绑定，并移除对应 `castAnimation`。

### 4.2 禁止修改

本次不得改 `Skeleton`、`Slot`、`Section`、`Segment`、`Blend`、`RateScale`、`Root Motion` 或源 `AnimSequence`，不得因 Notify 配置重排播放拓扑或重存无关字段。目标缺失、Skeleton/映射不兼容或 State 类不可用时失败并保持原资产不变。

### 4.3 不改变的相邻职责

- 动作总览只读 Sequence，正式分页图和 `overview.json` 写入 `Docs/ArtWorkflow/Actions/<assetName>/`；动作语义记录写入同目录 `observation.md`。观察职责不写 Montage、Notify 或技能配置。
- `SkillLabSkills` 负责请求门控、Cast 生命周期、许可、行为、效果、取消和终态，不依赖 Editor。
- Lua 只组合合法运行时行为请求对应的伤害、治疗、Buff 等行为，不写资产、不选点位、不替 C++ 裁定业务许可和结果。
- Blueprint/AnimBP 继续负责对象组装、资源绑定和姿势求值，不承载新的跨模块写回或业务状态机。

## 5. 两块任务的落点和依赖

### A：C++ Editor/Runtime 基建（已实现）

已落点包括：

1. `Source/SkillLabAnimationEditor/Public/SkillLabAnimationContentTypes.h`、`Public/SkillLabAnimationContentCommandlet.h`、`Private/SkillLabAnimationContentCommandlet.cpp`：复用 Montage 加载、`FAnimNotifyEvent`、`SavePackage`、`ResetLoaders` 和报告能力，增加多事件 Patch、稳定身份、白名单检查、回滚/失败保护和完整回读。
2. `schemas/skilllab-animation-content.schema.json` 或同模块唯一的新 schema：不得与动作总览 request/manifest schema 混用；入口必须指向唯一 schema。
3. `SkillLabSkillAnimNotify.h/.cpp`、`SkillLabSkillAnimationComponent.h/.cpp`、`SkillLabSkillRuntimeComponent.h/.cpp`：保留现有 `CastId`、Montage 实例、阶段 Token、取消/Reset/EndPlay 和迟到保护。若引入计数、顺序、State 窗口，先定义窄接口和幂等规则。

A 的 Patch 契约、目标资产读写报告、失败回滚、重载差异和运行事件桥接验证见 `ImplementationSummary.md`。A 不配置四技能点位；本批后续仅在 B 遇到可复现的通用能力缺口时，再由 PM 路由新的代码任务。

### B：四技能持久 Montage 与内容配置（已完成）

1. 四个技能已各自绑定持久 `UAnimMontage`，均为完整单段、正向 1 倍速、单次播放，源 Sequence 到 Montage 时间映射为 1:1。
2. `SkillDefinitions.json` 已移除这四项的 `castAnimation`，绑定 `castMontage` 并配置 `GameplayEvent`、`SkillLab.Event.Cast`；默认 3 秒释放超时保持不变。
3. 每个 Montage 有一个 `<SkillId>.release.01` 瞬时 Notify。最终点位和前后摇候选值以[配置交付说明](../../../../../Config/Animation/SkillContentDelivery.md)为准；四项均已保存、重载、独立进程回读和差异校验。
4. `SkillReloadDefinitions` 已成功装载正式定义。四个 Lua、C++、Blueprint、Schema、工具及源 Sequence 均未修改。

B 的完整绑定清单、Patch、逐项验证和未决游戏内检查见[配置交付说明](../../../../../Config/Animation/SkillContentDelivery.md)。结构配置完成不代表游戏内表现验收完成。

## 6. 验证入口与顺序

### A

- 只读报告目标 Montage 的类型、Skeleton、已有 Notify/State；schema 拒绝未知字段、非法时间、重复身份、未知 Tag/State 和非白名单字段。
- 已在测试持久 Montage 上覆盖多个瞬时 Notify、Notify State 新增/更新/删除、保留无关事件、保存后重载和差异比较；成功、拒绝及回滚证据见 `ImplementationSummary.md`。这些工具验证不替代 B 的四技能配置和运行时测试。
- 可复用命令入口：`UnrealEditor-Cmd.exe <SkillLab.uproject> -run=SkillLabAnimationContent -Spec=<patch> -Report=<report> [-Write]`。执行 Patch 时使用多操作 Patch 格式；结构读取或旧单 Notify Spec 不能代替 Patch 写入与回读验证。
- 通过 `SkillLabSkillAnimNotify -> ForwardAnimNotify -> ForwardCastEvent -> ReceiveCastEvent` 覆盖有效通知、重复、未声明 Tag、取消后迟到、Reset 后旧 Cast 和新旧 Cast 交错，并记录拒绝原因。

### B

- 四项 `castMontage` 绑定、字段互斥和 `GameplayEvent` 声明已通过 `SkillReloadDefinitions` Provider reload；逐项结果见[配置交付说明](../../../../../Config/Animation/SkillContentDelivery.md)。
- 每项 Patch 的保存、重载、回读必须指向对应技能的持久 Montage，不能使用无关 `MM_Pistol_Fire_Montage` 样本。
- 使用 `Run_SkillLab_DX11.bat` 或既有 `SkillTest`、`SkillArea`、`SkillBurn`、`SkillTestRecover`、`SkillReset` 入口观察动作、Notify 到达、业务接受/拒绝和终态。静态列表、日志或工具成功不能单独证明时序通过。
- 尚无游戏或 PIE 实测；用户侧仍需观察动作和释放时序、后摇计时、取消后的迟到 Notify/旧 Cast 隔离，以及播放失败时的既有 3 秒超时表现。

## 7. 实现 Agent 最小阅读集合

A：本交接、`SkillLabAnimationEditor.Build.cs`、`SkillLabAnimationContentTypes.h`、`SkillLabAnimationContentCommandlet.h/.cpp`、`schemas/skilllab-animation-content.schema.json`、`SkillLabSkillAnimNotify.h/.cpp`、`SkillLabSkillAnimationComponent.h/.cpp`、`SkillLabSkillRuntimeComponent.h/.cpp`。

B：本交接第 3、4、5 节和需求中的输入/失败/验收部分、`Config/SkillDefinitions.json`、四个授权 Lua 配置、`Docs/ArtWorkflow/README.md`、`Actions/README.md`、`Actions/Index.md`、四个动作目录的 `overview.json`、全部分页图与 `observation.md`，以及 A 的 Patch schema/使用入口和报告。只有字段或运行接口不清时才向 PM 索取接口摘要；配置 Agent 不默认读取全量 C++ 源码或无关程序文档。

## 8. 未决问题与阶段结论

1. 四项 Montage 绑定、Sequence 到 Montage 映射和 Provider reload 均已验证通过；资产与定义层证据见[配置交付说明](../../../../../Config/Animation/SkillContentDelivery.md)。
2. 最终测试候选值为：Fireball f11 / 0.366667 秒 / recovery 0.20 秒；AreaBurst f36 / 1.20 秒 / recovery 0.633333 秒；Mend f15 / 0.50 秒 / recovery 0.50 秒；BurningBuff f16 / 0.533333 秒 / recovery 0.466667 秒。对照新的峰值记录：Fireball f11 在首个明显白光 f9 之后，但 f17–f18 又有白亮线索，且动作没有清晰肢体峰值；AreaBurst f36 落在 f34–f38 的前伸/倾身峰值区间；Mend f15 落在 f13–f17 的伸展峰值并接近 f14 视觉线索峰值；BurningBuff f16 落在 f15–f18 的高举手臂峰值区间。详见 [AS_FireballCast](../../../../ArtWorkflow/Actions/AS_FireballCast/observation.md)、[MM_ChargedAttack](../../../../ArtWorkflow/Actions/MM_ChargedAttack/observation.md)、[MM_Attack_01](../../../../ArtWorkflow/Actions/MM_Attack_01/observation.md)、[MM_Attack_02](../../../../ArtWorkflow/Actions/MM_Attack_02/observation.md)。这些对应关系用于解释测试候选值，不表示图像证明了特效、释放或命中语义；最终值仍以用户明确指定为准。
3. 四技能已配置 `GameplayEvent` 释放声明，Lua 未修改。Notify 只提出技能执行请求，不证明命中或效果成功。
4. 游戏内动作、释放时序、取消后的迟到通知隔离及缺失 Notify 时的超时表现尚未验收；配置不能新增取消、退款或继续执行语义。

阶段结论：A 的通用配置能力已有实现和验证；B 的四技能持久 Montage、Notify、定义绑定、资产回读和 Provider reload 均已完成。下一步是用户进行游戏内表现与时序验证。
