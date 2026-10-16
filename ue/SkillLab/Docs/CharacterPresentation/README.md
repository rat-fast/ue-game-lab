# 角色与技能表现共享业务基线

## 定位

本目录定义 SkillLab 当前可供表现策划、程序、资源和 Review Agent 共同依赖的最小表现语义。它回答三件事：表现可以读取哪些运行事实、哪些角色与技能表现事实已经在 Git 基线中存在、哪些内容仍没有足够证据。

本目录不定义具体技能方案、数值、C++ 类名、Blueprint 拓扑、Montage 生产工具、JSON Schema、完整动画框架或未来资源计划。旧表现文档位于 `experiments/SkillLab/PresentationDocs_PreReset_2026-10-17/`，只用于发现历史冲突，不属于当前上下文；当前工作区未提交的 Native Animation 实验同样不属于本基线。

## 基线来源与状态

- 主要基线：Git 提交 `cb7c4ef5ede2072f9aa64a648beb12daec35dc52`。
- 业务语义交叉来源：`../SkillSystem/`，尤其是领域模型、施法生命周期、反馈与观察边界。
- 能力核对来源：`../ProjectContext/`、基线提交中的 `Source/`、`Config/`、`Content/` 和已有验证入口。
- 本目录文档为静态整理；本轮没有编译、启动 UE、执行自动化测试或重新验证动画资产。

## 阅读顺序

1. `01-CharacterPresentation.md`：角色 Mesh、AnimBP 和基础动作的业务边界；
2. `02-SkillActionPresentation.md`：技能动作、运行事件和表现接线语义；
3. `03-AssetsAndVerification.md`：基线资产清单、证据等级、限制和待确认问题。

## 快速结论

- 当前承诺集中在“表现读取运行事实，不能伪造技能结果”；不承诺某个动画框架或某套资源一定已在运行。
- Git 基线包含 Mannequin 角色/基础动画资产、技能动作资产、技能动画适配代码和四个技能的动画绑定文本。
- 现有代码与文档没有证明默认角色 Mesh、AnimBP、技能动作播放、Notify 释放或完整视觉体验在当前版本已通过 UE 运行验证。

