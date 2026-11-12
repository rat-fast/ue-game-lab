# 定义解析实现摘要

## 实现

- `FSkillLabCastExecutionToken` 是反射 `USTRUCT`，以 `Runtime`（`TWeakObjectPtr<USkillLabSkillRuntimeComponent>`）、`RuntimeGeneration`（`uint64`）和 `CastId`（`int32`）共同标识 Cast。三者均为 `UPROPERTY`；`RuntimeGeneration` 不暴露给 Blueprint。`FSkillLabSkillResult.CastToken` 反射保存同一 token，供反射委托值复制保真。
- `ESkillLabRuleDecision::Allow` 只接受于 `combatRules.skill`、`combatRules.dodge` 和 `abilityCategories`。移动、主动取消和受击类别仍按各自 Continue/Interrupt/Reject 约束解析；不把 Continue 解释成允许技能激活。
- `FSkillLabSubmissionPointDefinition`：`Id:int32`、`Trigger:ESkillLabSubmissionTrigger`（Script/Release/Timer/GameplayEvent）、`DelaySeconds:float`、`EventTag:FGameplayTag`、`TargetBinding:ESkillLabSubmissionTargetBinding`（Source/RequestTarget/RequestLocation）。`FSkillLabSkillDefinition` 同时提供 `SubmissionPoints`、`bHasExplicitSubmissionPoints` 和 `CompletionMode`（ScriptOrEvent/AllSubmissionPointsResolved）。
- JSON 技能字段 `submissionPoints` 接受非空对象数组；每项必需 `id`、`trigger`、`target`。`delay` 仅限 Timer，必须为有限非负数且不超过 float 最大值；`eventTag` 仅限 GameplayEvent，必须是注册的 `SkillLab.Event` 子 Tag，并加入该 Ability 的 `BehaviorEventTags`。ID 必须为技能内唯一正 int32，直接保留，不重编号。目标绑定仅允许 Self→Source、Unit→RequestTarget、Ground→RequestLocation，与当前 Lua 声明的目标/执行组合共同校验。
- 未声明 `submissionPoints` 时规范化为隐式点 `{Id=1, Trigger=Script}`，目标绑定依旧技能 Shape 映射，保持旧 Lua 无参提交兼容。`completionMode=AllSubmissionPointsResolved` 要求显式点表且不能同时声明 `completionEventTag`；默认仍为 ScriptOrEvent。
- 解析职责拆分为 Lua 声明、JSON 技能条目、共享规则/窗口、提交点四个私有解析单元。Provider 只负责完整目录候选组装、重复字段/技能 ID、规则引用、配置文件读取及成功后的原子替换；`ParseCatalogText` 失败不覆盖调用方已有结果，`ReloadDefinitions` 失败不替换当前目录。

## 正式解析样本

下列夹具使用 `ParseCatalogText` 和正式 Lua 声明解析，不代表新增或迁移生产技能：

```json
{
  "skills": [{
    "id": "Skill_SubmissionFixture",
    "luaScript": "fixture.lua",
    "cost": 1,
    "cooldown": 0,
    "magnitude": 1,
    "range": 500,
    "submissionPoints": [
      { "id": 17, "trigger": "Release", "target": "RequestTarget" },
      { "id": 42, "trigger": "Timer", "delay": 0.25, "target": "RequestTarget" },
      { "id": 99, "trigger": "GameplayEvent", "eventTag": "SkillLab.Event.Attack.Heavy", "target": "RequestTarget" }
    ],
    "completionMode": "AllSubmissionPointsResolved"
  }]
}
```

Lua fixture 声明目标、关系、效果和执行模板；它不声明提交时机或目标身份：

```lua
SkillLabSkill_Define("targetShape", "Unit", "relation", "Enemy", "effect", "Damage", "execution", "InstantTarget")
function OnSkillCast(context) SkillLabSkill_Finish() end
```

## 验证

- 冻结快照：`ue/SkillLabB1Validation_20261112_Final/`，复制当时源码、配置、工程描述和 Plugins；排除 `.git`、`.vs`、`Binaries`、`Intermediate`、`Saved` 与 DDC。快照包含合并执行线当时的 Runtime/GAS/表现迁移及已适配测试。
- UE 5.8 `SkillLabEditor Win64 Development` 全量构建成功；UHT 接受反射 token 中 `UPROPERTY uint64`（不暴露给 Blueprint）。Lua、Skill Entry、Rule、Submission 解析、Provider 与 `SkillLabSubmissionDefinitionTests.cpp` 均独立编译成功，`SkillLabSkills` 与 Editor 均链接成功。日志：`B1DefinitionBuild.log`。
- 新增自动化名 `SkillLab.Skills.SubmissionDefinition`，覆盖原四条生产定义的无修改兼容、隐式点、多个显式点、timer/event 规范化、Allow 限域、反射 token/result 复制及失败候选不泄漏；坏输入断言覆盖空数组、分数/溢出/重复 ID、未知触发、delay/eventTag 使用错误、未知事件、目标 shape 冲突、释放/窗口事件角色冲突和不兼容完成方式。缺失 GameplayEvent `eventTag` 的断言现在匹配 `submissionPoints[0].eventTag is required for GameplayEvent trigger`。
- 追加旧测试适配：`SkillLab.Skills.DefinitionValidation` 原用例将 `skill=Allow` 当作非法值，与 B1 已接受的 Allow 合同冲突。现改为 `skill=Continue`，并断言 `invalid decision 'Continue' for skill`，继续保留负向覆盖及字段级诊断检查。
- 修复后的三份源码已同步到自有验证快照 `ue/SkillLabB1Validation_20261112_Final/`。Win64 `SkillLabEditor Development` 增量构建成功，重新编译 SubmissionDefinition 解析器及两个测试源并成功链接；日志：`B1DefinitionRepairBuild.log`。
- 本次两项自动化未执行成功：本机首次启动在全平台 `ValidatePlatforms` 报缺 LinuxArm64/VisionOS SDK 后退出；尝试跳过预检后 Editor 以退出码 0 结束，但没有生成 Editor 或自动化日志。Win64 SDK 有效，构建通过不等于测试通过；本次不报告断言通过。预检记录位于快照 `Saved/Logs/AutoSDKInfo.txt`，没有 `B1SubmissionDefinitionAutomation.log`。

## 错误诊断

解析拒绝并返回字段/条目级原因，包括：`submissionPoints must be a nonempty array`、`submissionPoints[n].id must be a positive int32`、`duplicate submission point id`、`trigger must be Script, Release, Timer, or GameplayEvent`、Timer delay 缺失/越界或用于非 Timer、GameplayEvent 缺少时返回 `submissionPoints[n].eventTag is required for GameplayEvent trigger`，以及未知 Tag 或与释放/完成事件冲突、目标与 Shape 不兼容、AllSubmissionPointsResolved 缺少显式点或与完成事件并用、`invalid decision '<value>' for <field>`。多窗口冲突继续拒绝并要求显式 `windowConflict=Reject`。

## 未覆盖

- `SkillLab.Skills.SubmissionDefinition` 与 `SkillLab.Skills.DefinitionValidation` 仍需在可正常启动 Win64 Editor 自动化的环境中补跑；本次修复源码和构建日志已保存在自有 B1 验证快照，未写入执行线验证工程。
- 本任务只解析和规范化合同；提交触发调度、各点实际目标解析、所有点完成态与并发身份消费由合并执行线接通，本摘要不声称那些运行行为已通过。
