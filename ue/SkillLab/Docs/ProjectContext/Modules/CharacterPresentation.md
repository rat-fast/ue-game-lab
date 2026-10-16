# CharacterPresentation 角色与技能表现

## Module Responsibility

记录当前版本中角色 Mesh、动画实例、AnimBP、技能动作播放、表现事件接线和相关验证入口的真实代码与资产事实。

## Code And Assets

待依据当前 Git 版本和实际资产重新核对。本文不提前冻结 C++ 类名、AnimBP 拓扑、Montage 资产或外部配置格式。

## Runtime Entry

待核对角色默认对象、玩家控制入口、技能表现组件和正式运行入口。

## Runtime Flow

待依据真实代码和资产填写：角色资源加载、基础动画求值、技能动作播放、事件/Notify 接线、表现清理。

## Current Behavior

待重新验证。旧表现批次文档已归档，不在本文中继承其结论。

## Current Responsibilities And Evolution Boundaries

- 当前职责：待核对；
- 默认修改入口：待核对；
- 可接受的架构调整：由当前批次程序架构文档定义；
- 重构触发条件：由 Review 根据真实依赖和验证结果判断；
- 当前阶段限制：重建期间不扩大技能数量或高级动画框架范围。

## Verification

待补充可复现的构建、资产编译、PIE/运行、骨骼/Socket 和技能表现验证入口。

## Current Limitations

当前文档尚未确认正式表现能力；不要把实验日志或旧批次结论当作当前项目事实。

## Extension Directions

未来扩展方向由新的架构批次提出，不在项目事实文档中提前冻结。
