# SkillLab 共享领域基线

本目录集中存放跨任务、供相关领域工作共同消费的已确认业务语义和能力边界。参与某个领域任务的 Agent 应读取该领域基线；不要求每个 Agent 加载所有领域的全部基线。

## 当前领域

- [SkillSystem](SkillSystem/README.md)：技能系统的共享业务基线。
- [CharacterPresentation](CharacterPresentation/README.md)：角色与技能表现的共享业务基线。

## 使用边界

基线记录稳定、已确认且可复用的领域承诺，不替代策划需求、项目当前代码事实或程序任务交接：

- 本次需要实现什么，见 `../Planning/<Domain>/`。
- 当前代码、配置、资产与验证事实，见 `../ProjectContext/`。
- 单个程序任务的架构、实现和 Review 状态，见 `../ProgramWorkflow/<Task>/`。

更新基线时就地维护领域目录中的唯一当前版本；不要另存日期版或平行副本。不同领域的语义有交集时，引用主要负责的基线，不复制成第二份权威规则。
