# B：动画内容工具入口

`ue/SkillLab/Source/SkillLabAnimationEditor` 是 Editor-only 模块，提供 `USkillLabAnimationContentCommandlet`。

在 UE 5.8 Editor 命令行中执行：

```text
UnrealEditor-Cmd.exe SkillLab.uproject -run=SkillLabAnimationContent -unattended -nop4 -Spec="<project>/Config/Animation/FireballMontage.json" -Report="<project>/Saved/AnimationReports/fireball.json" -Write
```

不带 `-Write` 只读和校验资产。带 `-Write` 时，Commandlet 加载 `UAnimMontage`，应用局部 Patch，保存 Package，`ResetLoaders` 后重新加载同一对象并把读回字段写入报告。报告包含 Montage、Skeleton、Slot、Section、片段、Notify、Blend、Play Rate 和定位到字段的 issue；写回报告额外包含 `before.*` 与 `after.*` 快照，可直接比较保存前后的字段。

结构化入口的 schema 位于仓库 `schemas/skilllab-animation-content.schema.json`。`Config/Animation/InvalidAnimationSpec.json` 覆盖缺失资产，`Config/Animation/InvalidPlayRateSpec.json` 覆盖非法 Play Rate；两者都必须返回非零退出码并输出机器可读 issue。

`segment` 接受可加载且与 Montage Skeleton 相同的 `UAnimSequenceBase`，当前写回范围是首个 Slot 的首个 Segment；未知字段由 schema 和命令入口共同拒绝。UE 的 `FSlotAnimationTrack` 只保存 `SlotName`，Slot Group 由 Skeleton 注册表推导，因此入口拒绝 `SlotGroup.SlotName`，避免静默丢弃 Group 语义。Notify 按已有 `USkillLabSkillAnimNotify.EventTag` 定位，找不到时追加；它不是按数组顺序定位。运行时 `SkillLabSkills` 不读取草稿文件，仍只消费已保存的 UE 资产。

本轮已在 `D:\Epic\UE_5.8` 的 UE 5.8 环境完成构建、只读、负例和项目副本写回/重载验证。验证使用过 `Content/TempAnimationValidation` 临时副本，验证后已清理，未修改基线 Montage。
