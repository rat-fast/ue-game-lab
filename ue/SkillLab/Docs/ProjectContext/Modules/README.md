# SkillLab 模块文档索引

以下文档按稳定职责组织，描述当前代码事实、入口、运行链、边界和验证方式。先读架构地图，再按任务读取对应模块；不要把模块文档中的“演进边界”当作已批准的未来设计。

- [SkillLabSkills 技能运行时](SkillLabSkillsRuntime.md)：定义加载、loadout、请求、Cast 生命周期、行为和事件。
- [SkillLabLuaRuntime 公共 Lua 运行时](SkillLabLuaRuntime.md)：集中 Lua 5.4.8 唯一源码，提供业务无关的 VM/facade 能力；只依赖 `Core`，由需要 Lua 的项目模块按需通过 Private dependency 接入。
- [SkillLabGAS 战斗属性与效果](SkillLabGAS.md)：ASC、属性集、即时效果、持续伤害和纯结算测试。
- [PlayerInteraction 玩家交互与 HUD](PlayerInteraction.md)：角色、控制器、Enhanced Input、技能选点、快捷键和 HUD。
- [TestAndVerification 测试入口与验证](TestAndVerification.md)：训练场、Exec 命令、自动化测试、日志证据和未验证项。
- [SkillLabPresentation 角色与技能表现](CharacterPresentation.md)：角色 Mesh、AnimBP、动画播放、表现接线和当前验证入口。
