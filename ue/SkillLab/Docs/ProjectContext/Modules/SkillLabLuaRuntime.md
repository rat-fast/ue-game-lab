# SkillLabLuaRuntime 公共 Lua 运行时

## Module Responsibility

`SkillLabLuaRuntime` 是整个 SkillLab 工程可按需依赖的业务无关 Runtime 模块，唯一持有和编译 Lua 5.4.8。它提供 VM 创建/关闭、标准库开关、受保护脚本调用、指令预算、栈/值操作和 opaque C++ facade；不承载技能、角色、GAS、动画或配置业务。

## Code And Dependencies

- 模块入口：`Source/SkillLabLuaRuntime/SkillLabLuaRuntime.Build.cs`、`Private/SkillLabLuaRuntime.cpp`。
- 公共入口：`Source/SkillLabLuaRuntime/Public/SkillLabLua.h`；不暴露 Lua 头、内部布局或业务模块类型。
- 唯一 Lua 源码：`Source/SkillLabLuaRuntime/ThirdParty/lua-5.4.8/src/`。
- 依赖边界：只依赖 UE `Core`；不依赖 `SkillLabSkills`、`SkillLabPresentation`、`SkillLabGAS` 或 `SkillLab`。当前已核对的 Lua 宿主 `SkillLabSkills` 与 `SkillLabPresentation` 通过 Private dependency 接入，其他未使用 Lua 的模块不增加依赖。

## 第三方源码来源与维护边界

- 当前唯一源码是引入的 Lua 5.4.8：`ThirdParty/lua-5.4.8/src/lua.h` 的 `LUA_VERSION_RELEASE` 为 `"8"`，文件头和版权文本标明 `Lua.org, PUC-Rio`；这部分源码不是 SkillLab 自研代码。
- Lua.org/PUC-Rio 的来源标识位于上游源码头部，版权与许可证正文位于上游源码文件内（例如 `lua.h` 末尾）及上游发布材料中。当前仓库副本只有 `src/`，没有单独的 `LICENSE` 文件，不应据此声称仓库内存在独立许可证文件。
- UBT 只从 `SkillLabLuaRuntime` 编译这份 Lua 源码；`SkillLabSkills` 和 `SkillLabPresentation` 只通过 `SkillLabLuaRuntime` 的 opaque facade 接入，不在业务模块复制或分叉 Lua 实现。
- 未来升级 Lua 时，应替换这份单一副本，并重新核对版本、文件哈希、UBT 编译归属、导出/导入符号和现有行为；不得在业务模块新增副本或维护分叉。

## Runtime Entry

Skills 继续为每次校验/施法创建并关闭临时 VM；Presentation 继续持有长生命周期 VM，并在 EndPlay 关闭。两者共享 Runtime 能力但不共享 Lua 状态或业务回调。

## Verification

模块迁移后的 Editor/Game target 构建、导出/导入、`SkillLab.LuaRuntime.Boundary` 自动化和 NullRHI RuntimeProbe 证据见 `Docs/ProgramWorkflow/LuaRuntimeModuleRefactor/ImplementationSummary.md`。独立 Review 最终通过；2026-10-26 用户进游戏测试，反馈技能均能跑出、与原来未察觉区别，作为人工冒烟证据。删除后全量 Automation、完整视口表现、脚本异常注入及非四样本脚本场景仍未逐项验证；Game 可执行程序运行也未单独验收。
