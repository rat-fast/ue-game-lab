# Batch01：AI 适配的动画表现链路

## 批次定位

本批提出角色与技能动画表现链路的策划需求，使其更适合 AI 和人类共同读取、修改、回读、验证与 Review。它不是技能玩法重设计，也不是单独建设 Agent 工作流功能。程序负责评估并改造现有链路；工作区当前可能存在的未提交动画实验不视为完成事实，程序实现需先核对再决定保留或重写。

## 当前共享基线依赖

- 角色基础表现边界、资产证据和未验证项：`../../../../CharacterPresentation/01-CharacterPresentation.md`、`../../../../CharacterPresentation/03-AssetsAndVerification.md`。
- 技能动作与运行事件语义：`../../../../CharacterPresentation/02-SkillActionPresentation.md`。
- 技能阶段、终态与可观察事实：`../../../../SkillSystem/02-RequestLifecycle.md`、`../../../../SkillSystem/06-FeedbackAndObservability.md`。
- 当前基线提交为表现共享基线入口所记载的提交。资产路径和代码存在不等于运行验证通过；本批不重做基线冻结。

## 需求入口

- [公共程序需求索引](Infrastructure/README.md)：动画基础数据和状态、结构化动画内容、Montage 生产入口、技能表现关联。
- [公共验收](Infrastructure/04-SharedAcceptance.md)：按能力、内容、运行链和 AI 适配目标验收。
- [具体表现配置入口](Configurations/README.md)：配置暂缓，仅定义何时具备建立条件。

## 本批策划承诺

- 动画基础数据计算应从 Animation Blueprint Event Graph 的核心判断逻辑中迁出，减少图中承担的核心计算和状态判断。
- 基础状态逐步覆盖 Idle、Move、Jump、Fall、Land。每个状态/迁移步骤都须与改造前可观察结果对比，不以一次整体替换掩盖回归。
- 可变动画内容应有结构化、可读、可定位、可修改、可回读和可验证的生产入口。
- Montage 生产入口至少覆盖 Slot、Section、Notify、Blend、Play Rate、动画片段和 Skeleton 等必要内容，能够机器读取、修改、回读和校验。
- 技能表现要关联真实技能运行事实，正确区分动作开始、释放/效果和取消/完成，并在结束或取消后恢复基础姿势。
- 先以火球完成代表性纵向验证，再迁移到区域爆发、修复和燃烧效果。
- AI 友好性是上述能力的整体目标和完成标准，不独立建设“AI 工作流功能”，不规定 Agent 数量、协作步骤或自动化编排。

## 建议实现方向

需求层要求共享稳定的动画语义与可检查的生产入口；具体领域模型、编辑方式、机器接口及其到 C++、配置、Data Asset、Blueprint 或其他 UE 载体的映射由程序架构评估。本批不要求把 Blueprint 节点序列化或一一映射，也不预设必须替换现有专业工具。

## 当前限制与明确排除项

- 当前没有证据证明默认角色绑定、动画状态运行、四项技能动作可加载播放、Notify 已落在资源中，或释放帧与运行事件对齐。
- 不改变 UE 底层骨骼融合、姿势求值、动画压缩等引擎执行能力；目标是调整项目可读、可生产和可验证的表现链路。
- 不重设计技能目标、命中、伤害、治疗、持续效果、施法成本或生命周期规则；表现不能伪造这些运行结果。
- 不在策划需求中冻结 C++ 类名、Schema、Slot 名、Notify 标签、工具实现方式、Blueprint 拓扑或最终资源引用。
- 第 0 批已完成的共享基线冻结不进入本批。

## 原始七项需求归并

| 原需求 | 本批归属 |
| --- | --- |
| 第 1、2 项 | [动画基础数据与状态](Infrastructure/00-AnimationDataAndBaseStates.md) |
| 第 3 项 | [结构化动画内容](Infrastructure/01-StructuredAnimationContent.md) |
| 第 4 项 | [Montage 生产入口](Infrastructure/02-MontageProductionEntry.md) |
| 第 5、6 项 | [技能表现关联](Infrastructure/03-SkillPresentationIntegration.md) 与火球代表性验证、三项迁移验收 |
| 第 7 项 | 本批整体目标及 [公共验收](Infrastructure/04-SharedAcceptance.md)，不单独建文件 |

原第 0 批是已经存在的共享表现基线，不属于当前批次。

## 待用户确认

- 基础状态比较应以默认可控角色为唯一验收对象，还是同时覆盖训练假人？在确认前，公共验收要求至少明确默认可控角色的运行入口和测试场景。
- 当技能取消或完成时，若仍有独立持续效果在场，是否要求角色动作立刻回到基础姿势？本批遵循共享基线中 Cast 动作终止和持续效果独立的边界，具体表现延续规则待确认。
- 本批不锁定最终释放动作时机；待资源/运行证据建立后，应由技能配置逐项确认动作与释放事实的关系。
