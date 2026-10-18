# SkillLab 项目代码文档入口

本目录记录 SkillLab 当前有效的代码、配置、资产、运行入口和验证事实。业务语义仍由同级 `../SkillSystem/` 共享基线定义；本目录不把未来程序架构、策划方案或历史日志结论写成当前代码事实。

## 阅读顺序

1. `SkillLabCodeArchitecture.md`：项目角色、模块地图、正式入口、运行链、配置/资产来源和验证边界。
2. 程序架构、实现、集成或 Review 任务读取 `ProgramArchitectureContract.md`：SkillLab 程序分层和跨层职责约定。
3. `Modules/README.md`：按稳定职责选择模块文档。
4. 当前任务所需的模块文档：技能运行时、GAS、玩家交互/HUD 或测试验证。
5. `../SkillSystem/`：仅在需要核对共享技能业务语义时读取，不用它替代源码事实。

## 事实来源

文档事实按以下顺序核对：`ue/SkillLab/Source/`、`Config/`、`Content/`、`SkillLab.uproject`、`Run_SkillLab_DX11.bat` 和已有 `Saved/Logs` 验证材料。源码、配置和资产优先于旧文档；日志只作为历史验证证据，并在文档中标明未重新执行。

## 当前维护边界

本目录只维护当前项目上下文。不要在这里恢复旧版本类名、接口、运行链或验证结论；新功能的未来方案放在对应 `ProgramWorkflow/`，共享技能业务语义变化先更新 `Docs/SkillSystem/`。
