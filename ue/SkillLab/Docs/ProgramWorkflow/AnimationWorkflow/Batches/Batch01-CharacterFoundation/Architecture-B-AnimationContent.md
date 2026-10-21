# B：结构化动画内容与 Montage 生产架构交接

## 范围与依据

本任务对应 `Infrastructure/01-StructuredAnimationContent.md` 和 `02-MontageProductionEntry.md`，负责可变动画内容及 Montage 的机器可读、局部修改、实际资源回读和结构校验。配置类最终取值不在本轮范围；本任务不改变技能请求、命中、效果或施法生命周期。

### 策划明确要求

- 内容入口必须能定位语义条目、资源关联、局部修改并回读实际结果。
- Montage 至少覆盖 Slot、Section 及关系、Notify 位置/关联、Blend、Play Rate、动画片段关系，以及 Montage/片段/Skeleton 兼容引用。
- 缺失、无效、不兼容或不支持字段必须给出可定位校验结果；UE 资产限制要说明编辑边界和替代验证。

### 已核对项目事实

- 当前文本内容入口是 `Config/SkillDefinitions.json`，四技能只有 `castAnimation` 软路径；`FSkillLabSkillDefinition` 另有 `CastMontage` 字段，但 JSON 解析器只接受有限字段，不提供 Montage 内部结构读写（`Source/SkillLabSkills/Public/SkillLabSkillTypes.h`、`Private/SkillLabSkillDefinitionProvider.cpp`）。
- `USkillLabSkillAnimationComponent` 会优先加载 `CastMontage`，否则用 `CastAnimation` 动态创建 `DefaultSlot` Montage，按 `Started` 播放；这证明运行适配存在，不证明资产内容生产或回读能力存在（`Source/SkillLabSkills/Private/SkillLabSkillAnimationComponent.cpp`）。
- 内容树有 `AS_FireballCast.uasset`、Unarmed Attack 片段和 `MM_Pistol_Fire_Montage.uasset`。静态资产字符串可定位 Skeleton/片段引用，但不能从二进制路径确认 Slot、Section、Notify、Blend、Play Rate 的完整内部值。
- `SkillLabSkills` 是 Runtime 模块；当前没有已核对的 Editor/Commandlet 动画内容生产入口、结构化动画 Schema 或工具脚本。

## 运行/生产链

生产链应分为三个边界：结构化内容模型（可读、可 diff 的语义条目） -> UE 资产适配器（读取/修改/保存或报告不可编辑字段） -> 重新加载后的资产读回与校验。运行时只消费提交后的 `UAnimMontage`/`UAnimSequence` 及技能表现绑定，不读取草稿、不把校验工具变成技能规则。

需求要求机器能够修改内容、写回实际资源并回读校验。UE 资产编辑通常需要 Editor 侧 API 或受控导入/导出路径；具体载体由实现阶段选择。若某字段在所选路径上不能可靠写回，应继续核对其他可行技术路径；不能以只读报告或人工在编辑器中落盘代替机器写回要求，也不能把输入草稿当成生效资源。只有技术核对证明当前需求无法实现时，才回交限制及最小替代方案。

## 可复用能力与职责边界

- 复用 UE 的 `UAnimMontage`、`UAnimSequenceBase`、`USkeleton` 和 Montage 运行接口；不重建姿势求值、骨骼融合或压缩。
- 复用 `FSkillLabSkillDefinitionProvider` 的“候选内容先完整校验、成功后替换”思路作为校验/提交行为参考，但不要把技能 JSON 当作动画内容权威源，也不要扩展本任务外的技能字段。
- 结构化内容负责资产关系和字段一致性；`SkillLabSkillAnimationComponent` 只负责运行时播放和事件桥；技能运行时继续拥有施法事实。

## 建议修改落点

1. 在可被工具调用的程序侧建立动画内容领域模型和读回结果，至少能稳定标识 Montage、片段、Section、Notify、Slot、Blend、Play Rate、Skeleton 关系；具体文本格式、Schema 和 API 保持实现选择。
2. 增加 UE 资产读取/写回适配入口，优先放在 Editor/工具边界而非 Runtime 技能模块；需要新增模块时先确认 `SkillLab.uproject` 的 Editor 目标和 UE 5.8 API 可用性。
3. 为读取、局部修改、保存、重新加载、差异比较和校验分别保留可定位结果。校验至少覆盖空引用、失效片段/Notify、Skeleton 不兼容、Section/Slot 关系和不支持字段。
4. 以火球代表 Montage/动画内容作为第一条样本，验证读取结果确实来自已保存资产，再供 C 使用；不在本任务中固化四技能具体动作或数值。

这些是架构建议，不把 JSON、某个 Schema、命令行报告格式、资产命名或专用编辑器升级为硬验收条件。配置类内容仍由策划/内容任务另行决定。

## 实现必读文件

- `ue/SkillLab/Docs/Planning/CharacterPresentation/Batches/Batch01-AIAdaptedAnimationPipeline/Infrastructure/01-StructuredAnimationContent.md`、`02-MontageProductionEntry.md`
- `ue/SkillLab/Docs/Baseline/CharacterPresentation/03-AssetsAndVerification.md`、`02-SkillActionPresentation.md`
- `ue/SkillLab/Source/SkillLabSkills/Public/SkillLabSkillTypes.h`
- `ue/SkillLab/Source/SkillLabSkills/Public/SkillLabSkillDefinitionProvider.h`、`Private/SkillLabSkillDefinitionProvider.cpp`
- `ue/SkillLab/Source/SkillLabSkills/Public/SkillLabSkillAnimationComponent.h`、`Private/SkillLabSkillAnimationComponent.cpp`
- `ue/SkillLab/Source/SkillLabSkills/Public/SkillLabSkillAnimNotify.h`、`Private/SkillLabSkillAnimNotify.cpp`
- `ue/SkillLab/SkillLab.uproject`、`Source/SkillLabSkills/SkillLabSkills.Build.cs`
- `ue/SkillLab/Content/SkillLab/Animations/AS_FireballCast.uasset`、`Content/Characters/Mannequins/Anims/Unarmed/Attack/*`、`Pistol/MM_Pistol_Fire_Montage.uasset`

## 直接依赖

- 上游：UE 5.8 动画资产编辑 API、目标 Skeleton 和实际 Montage/片段资产；这些内部字段尚未被完整静态核对。
- 下游：C 任务需要从此入口获得已保存、可回读且校验通过的技能动作资源；若 B 只能读不能写，必须把限制传递给 C 和总控。
- 不依赖角色移动状态或技能效果计算；不修改 `Config/SkillDefinitions.json` 的最终配置内容。

## 验证入口与实现期验证项

- 先静态确认代表资产是否为 Montage、其 Skeleton 和片段引用是否可加载；对二进制资产内部信息不能凭路径推断。
- 对同一 Montage 执行读取 -> 局部修改 -> 保存/提交 -> 重新加载 -> 读回，并比较实际资源字段；验证结果应定位到资产、Section/Notify/片段或字段。
- 用缺失资源、失效片段、Skeleton 不兼容、非法 Section/Slot 关系和不支持字段作为负例，确认报告说明对象和原因。
- 在目标 UE 5.8 编辑器/运行入口重新加载资产，确认读回结果与实际生效资源一致。当前没有现成自动化入口，不能把 `SkillLab.Skills.DefinitionValidation` 当作 Montage 校验。

## 实现期技术选择

机器修改并写回 UE 资产、重新加载回读实际字段并校验是需求；Editor 模块、Commandlet、受控导入/导出或其他可行工具入口属于实现阶段的技术选择。JSON、Schema、工具界面和报告格式也未被策划冻结，应由实现 Agent 根据 UE 5.8 能力和现有项目结构选择，不作为开工前用户决策。验证具体字段无法写回时，继续评估替代技术路径；只有确认没有满足需求的可行路径后，才回交证据、影响和最小替代范围。
