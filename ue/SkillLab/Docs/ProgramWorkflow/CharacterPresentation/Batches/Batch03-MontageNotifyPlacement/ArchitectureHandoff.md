# Batch03：持久 Montage Notify 基建与技能配置架构交接

## 1. 文档定位与阶段边界

本文是 Batch03 Notify 侧唯一有效的程序架构交接，依据当前批次需求、动作总览交接、`Docs/ProjectContext/ProgramArchitectureContract.md` 及角色表现/技能系统基线。本轮只完成结构核查和文档，不修改代码、Blueprint、配置、Lua 或 UE 资产，不把建议写成已实现事实。

正式 Notify 目标是已保存的 `UAnimMontage`。`castAnimation -> CreateSlotAnimationAsDynamicMontage` 仅记录为当前代码事实，不再作为 Batch03 Notify 写回路线。

本批拆为两块：

- **A 基建**：C++ Editor 读取持久 Montage，消费结构化 Patch，在白名单内新增/更新/删除 Notify，保存、重载、回读和差异校验；Runtime 复用现有 Notify 桥及身份、取消、迟到保护。
- **B 配置**：四个技能创建或绑定持久 Montage，`SkillDefinitions.json` 使用 `castMontage`，依据动作观察和业务语义填写 Notify Patch，仅为确有需要的技能切换 `GameplayEvent`；Lua 只在行为确实需要时修改。

依赖为：动作总览产物 -> A -> B -> 结构、运行和游戏内时序验收。A 的契约和验证是 B 的前置，B 不应通过内容缺口反向改写通用基建。

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

### 2.2 现有 Editor 能力和缺口

`Source/SkillLabAnimationEditor/Private/SkillLabAnimationContentCommandlet.cpp` 通过 `StaticLoadObject(UAnimMontage::StaticClass(), ...)` 读取持久 Montage，输出 Skeleton、Slot、Section、Segment、Blend、RateScale 和 Notify 摘要；`-Write` 保存 Package，`ResetLoaders` 后重载并回读。现有 `FSkillLabAnimationContentSpec` 与 `schemas/skilllab-animation-content.schema.json` 支持单个 `notify`，并允许修改 RateScale、Blend、Slot、Section、Segment。

这些是可复用的资产定位、保存/重载、报告和校验底座，不是本批完成事实。现有 schema 不能表达多事件、Notify State、删除、稳定身份、拥有范围、重复规则或观察证据；现有 writer 允许的非白名单字段必须在 A 中拒绝或隔离。`Config/Animation/FireballMontage.json` 指向的 `MM_Pistol_Fire_Montage` 也不能证明四技能正在使用该资产。

## 3. 单一权威来源与输入/输出契约

### 3.1 三类事实

| 层 | 权威内容 | 作用 |
|---|---|---|
| AI 文本源 | `Docs/ArtWorkflow/Exports/` 完成 `manifest.json`、`Docs/ArtWorkflow/Observations/` 观察记录、Notify Patch | 记录观察证据、候选点位、事件意图和受控修改请求 |
| UE 资产 | 目标持久 `UAnimMontage` 及已保存 `Notifies` | 正式运行载体和最终配置权威 |
| 运行时事实 | `CastId`、阶段、Montage 实例、`ReceiveCastEvent`、技能结果和游戏内观察 | 证明通知到达、业务接受/拒绝和最终效果 |

Sequence 总览以源 Sequence 时间为权威。观察记录的 `playbackMapping` 未建立持久 Montage 映射时，只能作为候选依据，不能把源秒数直接写入 Montage。

### 3.2 推荐 Patch 字段

实现可扩展现有 JSON 入口或在同一 Editor 模块建立唯一的新入口，但应支持下列契约：

```json
{
  "version": 1,
  "asset": "/Game/.../Skill_Fireball_Montage.Skill_Fireball_Montage",
  "source": { "observation": "Docs/ArtWorkflow/Observations/Skill_Fireball.observation.json", "generationId": "...", "inputPreviewFingerprintSha256": "..." },
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

- 动作总览只读 Sequence，产物写入 `Docs/ArtWorkflow/Exports/`；观察写入 `Docs/ArtWorkflow/Observations/`，不写 Montage、Notify 或技能配置。
- `SkillLabSkills` 负责请求门控、Cast 生命周期、许可、行为、效果、取消和终态，不依赖 Editor。
- Lua 只组合合法运行时行为请求对应的伤害、治疗、Buff 等行为，不写资产、不选点位、不替 C++ 裁定业务许可和结果。
- Blueprint/AnimBP 继续负责对象组装、资源绑定和姿势求值，不承载新的跨模块写回或业务状态机。

## 5. 两块任务的落点和依赖

### A：C++ Editor/Runtime 基建

建议落点：

1. `Source/SkillLabAnimationEditor/Public/SkillLabAnimationContentTypes.h`、`Public/SkillLabAnimationContentCommandlet.h`、`Private/SkillLabAnimationContentCommandlet.cpp`：复用 Montage 加载、`FAnimNotifyEvent`、`SavePackage`、`ResetLoaders` 和报告能力，增加多事件 Patch、稳定身份、白名单检查、回滚/失败保护和完整回读。
2. `schemas/skilllab-animation-content.schema.json` 或同模块唯一的新 schema：不得与动作总览 request/manifest schema 混用；入口必须指向唯一 schema。
3. `SkillLabSkillAnimNotify.h/.cpp`、`SkillLabSkillAnimationComponent.h/.cpp`、`SkillLabSkillRuntimeComponent.h/.cpp`：保留现有 `CastId`、Montage 实例、阶段 Token、取消/Reset/EndPlay 和迟到保护。若引入计数、顺序、State 窗口，先定义窄接口和幂等规则。

A 输出可执行 Patch 契约/示例、目标资产读写报告、失败不污染证据、重载差异和运行事件拒绝原因；A 不配置四技能点位。

### B：四技能持久 Montage 与内容配置

1. 为 `Skill_Fireball`、`Skill_AreaBurst`、`Skill_Mend`、`Skill_BurningBuff` 创建或确认持久 `UAnimMontage`，只读核对 Skeleton、Slot、Segment、时长和播放条件。
2. 更新 `ue/SkillLab/Config/SkillDefinitions.json`，各技能使用 `castMontage`，不再以 `castAnimation` 作为正式 Notify 目标；用 Provider reload/自动化验证互斥和资产路径。
3. 消费完成的动作总览 manifest 和观察记录，核实 Sequence 到目标 Montage 的映射后生成 Patch；事件数量、顺序、容差有冲突时报告，不任意放点。
4. 仅当业务确需 Notify 释放/行为事件时增加 `releaseTrigger: "GameplayEvent"`、合法 `releaseEventTag` 和正 `releaseEventTimeout`；否则保持 `Timer`。Lua 仅在实际行为需要不同事件组合时修改。

B 输出绑定清单、每个 Patch 和观察证据、保存/回读报告、运行事件与业务结果的分离证据和未完成项；不得修改非白名单 Montage 字段。

## 6. 验证入口与顺序

### A

- 只读报告目标 Montage 的类型、Skeleton、已有 Notify/State；schema 拒绝未知字段、非法时间、重复身份、未知 Tag/State 和非白名单字段。
- 在测试持久 Montage 上覆盖多个瞬时 Notify、Notify State 新增/更新/删除、保留无关事件、保存后重载和稳定身份差异比较；任一失败证明原资产未污染。
- 可复用命令入口：`UnrealEditor-Cmd.exe <SkillLab.uproject> -run=SkillLabAnimationContent -Spec=<patch> -Report=<report> [-Write]`。在 A 完成前，此入口只证明旧有限能力，不能作为 Batch03 通过证据。
- 通过 `SkillLabSkillAnimNotify -> ForwardAnimNotify -> ForwardCastEvent -> ReceiveCastEvent` 覆盖有效通知、重复、未声明 Tag、取消后迟到、Reset 后旧 Cast 和新旧 Cast 交错，并记录拒绝原因。

### B

- `SkillLabDefinitionProvider` 自动化/`SkillReloadDefinitions` 验证四项 `castMontage` 绑定、互斥字段、资产可加载和必要 `GameplayEvent` 声明。
- 每项 Patch 的保存、重载、回读必须指向对应技能的持久 Montage，不能使用无关 `MM_Pistol_Fire_Montage` 样本。
- 使用 `Run_SkillLab_DX11.bat` 或既有 `SkillTest`、`SkillArea`、`SkillBurn`、`SkillTestRecover`、`SkillReset` 入口观察动作、Notify 到达、业务接受/拒绝和终态。静态列表、日志或工具成功不能单独证明时序通过。
- 按技能事件语义、次数/顺序、观察依据和事先约定容差记录游戏内实测；没有可见 PIE/游戏证据时只报告结构/运行链验证。

## 7. 实现 Agent 最小阅读集合

A：本交接、`SkillLabAnimationEditor.Build.cs`、`SkillLabAnimationContentTypes.h`、`SkillLabAnimationContentCommandlet.h/.cpp`、`schemas/skilllab-animation-content.schema.json`、`SkillLabSkillAnimNotify.h/.cpp`、`SkillLabSkillAnimationComponent.h/.cpp`、`SkillLabSkillRuntimeComponent.h/.cpp`。

B：本交接第 3、4、5 节和需求中的输入/失败/验收部分、`Config/SkillDefinitions.json`、`SkillLabSkillDefinitionProvider.cpp`、`SkillLabSkillTypes.h`、`Docs/ArtWorkflow/README.md`、`ActionObservationGuide.md`、实际 Exports/Observations，以及 A 的 Patch 契约和报告。

## 8. 未决问题与阶段结论

1. UE 5.8 对多 Notify/Notify State 稳定身份、State 实例化和完整回读字段的具体 API，需要 A 实现期核实；若无资产内稳定身份，必须交付可重复的受控映射并报告限制。
2. 四技能各自的持久 Montage、Sequence 到 Montage 的时间映射、事件次数/顺序/容差和是否需要 State，不能从当前 `castAnimation` 或无关样本推断。
3. 哪些技能真正需要 `GameplayEvent`，以及 Lua 是否需要多事件/非一次性行为，由 B 按业务语义确认；未确认时保持 `Timer`/现有 Lua。
4. Notify 缺失、动作中断、释放超时和 State 提前关闭的技能终态沿已确认规则处理，程序不能自行发明取消、退款或继续执行语义。

阶段结论：现有代码已有持久 Montage 加载/播放分支、有限 Editor 保存回读能力和运行时身份桥，但尚未形成 Batch03 白名单、多事件 Patch、四技能持久绑定或游戏内时序证据。应先完成并验证 A，再由 B 逐技能配置；动作总览交接仍是上游观察输入。
