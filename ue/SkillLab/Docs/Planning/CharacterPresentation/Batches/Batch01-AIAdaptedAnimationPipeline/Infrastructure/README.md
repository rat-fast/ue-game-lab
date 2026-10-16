# Batch01 公共程序需求

本目录描述程序应提供的可复用表现能力，不定义四个技能各自使用的动作内容。需求以 `../README.md` 所列的当前共享基线为前提。

- [00 动画基础数据与状态](00-AnimationDataAndBaseStates.md)：迁出核心判断，逐步建立并比较基础状态。
- [01 结构化动画内容](01-StructuredAnimationContent.md)：为可变动画内容提供可读、可定位、可修改和可验证的入口。
- [02 Montage 生产入口](02-MontageProductionEntry.md)：建立机器可读、可修改、可回读和可校验的 Montage 内容生产能力。
- [03 技能表现关联](03-SkillPresentationIntegration.md)：把动作过程与带身份的技能运行事实正确关联。
- [04 公共验收](04-SharedAcceptance.md)：验证公共能力、代表性迁移、差异回归和 AI 适配目标。

这些文档给出需求和验收语义。程序架构决定领域模型、类/接口、Schema、文件格式、工具、Blueprint 边界和具体迁移策略，并应检查现存实现与资产后再定案。
