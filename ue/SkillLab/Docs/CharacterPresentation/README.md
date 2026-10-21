# 角色与技能表现共享业务基线

## 定位

本目录记录 SkillLab 当前可供表现策划、程序、资源和 Review Agent 共同使用的最小表现语义。它只回答表现可以观察哪些运行事实、a2c5969 中有哪些实现事实、现有证据验证到哪里，以及哪些内容仍不能承诺。

本目录不替代 `../SkillSystem/` 的技能业务语义、`../ProjectContext/` 的代码事实、`../Planning/CharacterPresentation/` 的策划要求或 `../ProgramWorkflow/AnimationWorkflow/` 的批次交接；不冻结 C++ API、Blueprint 拓扑、JSON Schema、Montage 生产规范或最终资源方案。

## 基线版本与证据状态

- 指定事实版本：Git 提交 `a2c59695018ca3c0d3e97b70aa8801d087e14534`（`a2c5969`）。这里指按该提交的完整文件树核对，包含其祖先提交中在该节点仍保留的功能；不是只查看 `a2c5969` 这一条提交新增的差异。
- 方向性承诺：继续复用 UE 的骨骼融合、姿势求值、动画压缩等底层能力；项目逻辑尽量由 C++ 负责，可变内容通过结构化配置和资产读写工具维护。方向不等于所有能力已经实现或验收。
- 本轮没有重新编译、启动 UE、执行自动化测试或重跑动画工具。`ue/SkillLab/Saved/` 中实际存在的日志、报告和截图只作为本地历史证据引用，不属于 Git 快照。

## 阅读顺序

1. `01-CharacterPresentation.md`：角色 Mesh、AnimBP 和基础动作的边界；
2. `02-SkillActionPresentation.md`：技能动作、施法事件和动作清理；
3. `03-AssetsAndVerification.md`：资产、内容工具、证据等级和限制。

## 快速结论

- 表现读取技能运行层事件；表现不能决定技能是否成功、命中、扣费、释放或完成。
- a2c5969 已包含角色基础状态快照到 `ABP_Unarmed` 的代码接入、技能动作适配、四个技能的动作软路径，以及 Montage 内容读写/校验工具入口。
- 当前配置四项技能都使用 `castAnimation`。运行时可将 Sequence 创建为 Dynamic Montage；代码也支持直接读取 `castMontage`，但本提交没有证明四项技能已改用正式 Montage 或其视觉链路已验收。
- 现有证据支持局部运行链和结构化工具验证，不支持完整 PIE/视口视觉验收、Notify 时点验收、缺资源降级验收或整批放行。
