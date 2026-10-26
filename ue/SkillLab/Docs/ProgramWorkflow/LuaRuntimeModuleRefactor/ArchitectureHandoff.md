# SkillLab Lua 运行时模块重构架构交接

## 1. 任务目标与边界

用户目标是建立唯一公共 Lua 运行时模块，建议命名为 `SkillLabLuaRuntime`，让 `SkillLabSkills` 和 `SkillLabPresentation` 依赖它并保持当前行为。当前交接只覆盖 Lua 5.4.8 的源码归属、UBT 构建方式、跨模块接口和验证；不改变技能门控、Cast 生命周期、GAS 结算、角色状态、动画求值、脚本 API 语义或资产。

已明确禁止：修改本交接阶段的实现代码、移动或删除任一现有 Lua 副本、改写 Lua 脚本内容、派策划 Agent。实现阶段可以在本交接允许的落点内新增模块和适配代码，但必须保持下述行为与验证条件。

## 2. 已核对事实与证据

### 模块和工程

- `SkillLab.uproject` 使用 UE `5.8`，当前注册 `SkillLabGAS`、`SkillLabSkills`、`SkillLabPresentation`、`SkillLab` 四个 Runtime 模块及 `SkillLabAnimationEditor` Editor 模块：[`SkillLab.uproject`](../../../SkillLab.uproject:1)。新模块必须作为 Runtime 模块加入同一列表。
- `SkillLabSkills.Build.cs` 当前把 `SkillLabGAS` 放在 Public 依赖，并把本模块下 `ThirdParty/lua-5.4.8/src` 加入 `PrivateIncludePaths`；注释说明 Lua C 源码由 UBT 源码扫描器拾取：[`SkillLabSkills.Build.cs`](../../../Source/SkillLabSkills/SkillLabSkills.Build.cs:8)。
- `SkillLabPresentation.Build.cs` Public 依赖 `SkillLabSkills`，Private 依赖 `AnimGraphRuntime`/`Json`，并把本模块下 Lua `src` 加入 `PrivateIncludePaths`：[`SkillLabPresentation.Build.cs`](../../../Source/SkillLabPresentation/SkillLabPresentation.Build.cs:8)。
- `SkillLab.Build.cs` 以 Private 依赖连接 `SkillLabGAS`、`SkillLabSkills` 和 `SkillLabPresentation`；它不直接使用 Lua：[`SkillLab.Build.cs`](../../../Source/SkillLab/SkillLab.Build.cs:18)。`SkillLabAnimationEditor` 依赖 Skills/Presentation，但没有 Lua C API 调用；只有在实现改动使公共头暴露新类型时才重新评估它。
- 两个现有模块的模块实现都是 `FDefaultModuleImpl`：[`SkillLabSkills.cpp`](../../../Source/SkillLabSkills/Private/SkillLabSkills.cpp:1)、[`SkillLabPresentation.cpp`](../../../Source/SkillLabPresentation/Private/SkillLabPresentation.cpp:1)。新模块可沿用该最小入口，不需要增加 UObject 或全局服务。

### 两个 Lua 副本

- `Source/SkillLabSkills/ThirdParty/lua-5.4.8/src` 与 `Source/SkillLabPresentation/ThirdParty/lua-5.4.8/src` 均为 61 个文件、848,416 bytes；文件名集合和全部文件 SHA-256 相同。代表性哈希：两边 `lua.h` 为 `3B77329F9DEED929A5CBE7A7D5FC81EBBB63FD869A49E0A9302FE9240CAF1930`，`lauxlib.h` 为 `2D9E18E577A6062646268E7F7228264C28BD9B972C186E1AF81B8357179C0AB4`，`lualib.h` 为 `4EA0F67D3E467F5D6A327616E22A3C2DA8E03D47A3C3660916275F2842F521EA`。
- 两个副本的 `src` 完全相同；Presentation 副本额外带上游文档/图片/Makefile/README 文件，Skills 副本的 UBT 实际依赖仍只指向 `src`。因此去重的事实对象是 `src` 和所有直接引用路径，不能只删除某个目录名而不扫引用。
- 上游源码在每个实现文件中自行定义 `LUA_CORE` 或 `LUA_LIB`，例如 `lapi.c:12`、`lauxlib.c:8`、`linit.c:9`。`luaconf.h` 只有定义 `LUA_BUILD_AS_DLL` 时才把 `LUA_API` 变成 `__declspec(dllexport/dllimport)`，否则为普通 `extern`：[`luaconf.h`](../../../Source/SkillLabSkills/ThirdParty/lua-5.4.8/src/luaconf.h:279)。两个模块各编译一份相同 C 实现会复制 Lua 实现和 VM 所依赖的代码；集中到独立 DLL 后若未导出 API，则消费者会出现 unresolved Lua symbols，若仍在多个模块编译则继续保留重复实现和状态来源。

### 全部 Lua 宿主与 API 使用

当前排除 `ThirdParty/**` 后，Lua C API 只出现在两个私有实现和一个表现公共头的 opaque 声明中：

1. `SkillLabSkills/Private/SkillLabSkillBehavior.cpp` 通过 `extern "C"` 包含 `lua.h`、`lauxlib.h`：[`SkillLabSkillBehavior.cpp`](../../../Source/SkillLabSkills/Private/SkillLabSkillBehavior.cpp:10)。`ValidateScript` 创建临时 VM、设置 100000 指令 hook、加载/执行脚本、检查 `OnSkillCast` 并关闭 VM：[`SkillLabSkillBehavior.cpp`](../../../Source/SkillLabSkills/Private/SkillLabSkillBehavior.cpp:111)。`RunScript` 重复加载/执行流程，向脚本绑定 `SkillLabSkill_RequestAction` 与 `SkillLabSkill_Finish`，传入 `CastId`/`RequestId`/`SkillId`，错误后调用现有 `Fail`，最后关闭 VM：[`SkillLabSkillBehavior.cpp`](../../../Source/SkillLabSkills/Private/SkillLabSkillBehavior.cpp:248)。
2. `SkillLabPresentation/Private/SkillLabCharacterPresentationLuaComponent.cpp` 通过 `extern "C"` 包含 `lua.h`、`lauxlib.h`、`lualib.h`：[`SkillLabCharacterPresentationLuaComponent.cpp`](../../../Source/SkillLabPresentation/Private/SkillLabCharacterPresentationLuaComponent.cpp:15)。Begin/EndPlay 管理 VM 生命周期，加载 `Config/CharacterPresentation.lua`，打开标准库并注册 `SkillLabPresentation` 桥：[`SkillLabCharacterPresentationLuaComponent.cpp`](../../../Source/SkillLabPresentation/Private/SkillLabCharacterPresentationLuaComponent.cpp:102)。桥提供 `PlaySkillAction`、`StopSkillAction`、`SetLocomotionIntent`、`Log`：[`SkillLabCharacterPresentationLuaComponent.cpp`](../../../Source/SkillLabPresentation/Private/SkillLabCharacterPresentationLuaComponent.cpp:155)。事件回调 `OnSkillEvent` 和状态回调 `OnBaseStateChanged` 只把真实事件/状态传给 Lua，失败时回到原生 fallback：[`SkillLabCharacterPresentationLuaComponent.cpp`](../../../Source/SkillLabPresentation/Private/SkillLabCharacterPresentationLuaComponent.cpp:223)。
3. 表现公共头只前向声明 `lua_State`，但把 C API 类型放进私有静态桥和成员句柄：[`SkillLabCharacterPresentationLuaComponent.h`](../../../Source/SkillLabPresentation/Public/SkillLabCharacterPresentationLuaComponent.h:8)。这不是完整 Lua 头暴露，却仍把第三方类型泄漏到模块公共头；重构应优先把 Lua 句柄和回调细节收回私有实现或新模块的 opaque facade。

没有在 `SkillLab`、`SkillLabGAS`、`SkillLabAnimationEditor` 或其他非 ThirdParty 源码中发现 Lua include/API 使用。配置脚本仍由 `Config/Skill_*.lua` 和 `Config/CharacterPresentation.lua` 提供，脚本路径和脚本行为不在本任务中改动。

## 3. 当前运行链与目标依赖方向

当前技能链为：`SkillLabSkills::USkillLabLuaSkillBehavior` -> 本模块自己编译的 Lua C 源码 -> `Config/Skill_*.lua` -> 现有 `RequestDeclaredAction/Effect` 与 `Complete/Fail`。当前表现链为：`SkillLabPresentation::USkillLabCharacterPresentationLuaComponent` -> 本模块自己编译的 Lua C 源码 -> `Config/CharacterPresentation.lua` -> Router/AnimationComponent/AnimInstance 的现有窄桥。

目标拓扑为：

```text
SkillLabLuaRuntime (唯一 Lua 5.4.8 源码、VM/脚本适配和导出边界)
       ^                         ^
       |                         |
SkillLabSkills              SkillLabPresentation
       ^                         ^
       +----------- SkillLab (现有组装模块) -----------+
```

`SkillLabSkills` 与 `SkillLabPresentation` 可以共同依赖 `SkillLabLuaRuntime`；它们之间保持现有方向，不能让 Lua Runtime 依赖任一业务模块，也不能让 Skills 依赖 Presentation。`SkillLab` 继续只负责角色组装和现有私有模块依赖。新模块应只依赖 UE `Core`（若 facade 使用其他 UE 类型再按实际需要增加），不能依赖 GAS、Skills、Presentation 或角色表现类型。

## 4. 推荐架构与公共/私有边界

### 推荐方案：新模块拥有实现，公开窄 C++ facade

推荐 `SkillLabLuaRuntime` 将 61 个 `src/*.c` 文件集中到一个 `Source/SkillLabLuaRuntime/ThirdParty/lua-5.4.8/src`，并提供少量带 `SKILLLABLUA_RUNTIME_API` 的公共 C++ 适配类型/函数。适配层负责 VM 创建/关闭、脚本缓冲加载、受保护调用、标准库开关、指令预算和 opaque state 生命周期；宿主模块保留各自的业务回调、表字段和行为状态。公共接口不应暴露 `ThirdParty` 路径、Lua 内部头、`lua_State` 的完整布局、GAS/Actor/AnimInstance 类型或业务模块依赖。

宿主职责保持不变：

- `SkillLabSkills` 继续拥有 `USkillLabLuaSkillBehavior`、技能桥接函数、定义校验、`CastId`/Runtime generation 校验、效果/投射物请求、`Fail`/`Complete` 和脚本失败隔离。
- `SkillLabPresentation` 继续拥有角色表现组件、Router 绑定、技能动作/locomotion/log 桥、真实事件字段映射和原生 fallback。建议把公共头中的 `lua_State*`、静态 C 回调签名移入 `.cpp` 或 facade 的 opaque handle，避免第三方类型出现在公共 UHT 边界。
- `SkillLabLuaRuntime` 不知道技能、角色、GAS、动画或配置业务；它只提供执行 Lua 所需的通用机制，不拥有 `SkillLabSkill_RequestAction` 等业务名字。

### 直接暴露 Lua C API 的过渡选项

若为保持修改最小，实施 Agent 选择让两个宿主继续直接调用 Lua C API，则 `SkillLabLuaRuntime` 必须把 Lua headers 作为明确的公共构建接口，并把所有 Lua C 源码只编进新模块一次；不能仅把 `PrivateDependencyModuleNames.Add("SkillLabLuaRuntime")` 当作符号解决方案。必须验证 Windows 模块导出/导入：编译 Lua 实现时定义 `LUA_BUILD_AS_DLL`/`LUA_CORE` 或等价导出策略，消费者看到匹配的 `dllimport` 声明，且 `LUA_API` 符号来自唯一模块。任何宏作用域、`PublicDefinitions`/`PrivateDefinitions` 和头路径调整都须在实际 UE 5.8 链接结果中确认。

该过渡方案不是推荐长期边界，因为它把 `lua_State`、Lua 栈操作和上游头文件继续扩散到业务模块；如果采用，必须在交接实现说明中记录为什么当前 facade 不足，并补充符号检查证据。

## 5. 最小修改落点

实现阶段按以下顺序修改，文件名可由实现 Agent 依据实际代码选择，但职责不得漂移：

1. 新增 `Source/SkillLabLuaRuntime/` 的 `SkillLabLuaRuntime.Build.cs` 和最小模块入口；迁入/复制一份经哈希核对的 Lua `src`，由该 Build.cs 的 include/source 配置负责唯一编译。不要在本交接阶段执行删除。
2. 在 `SkillLab.uproject` 增加 `SkillLabLuaRuntime` Runtime 模块，保持 `EngineAssociation: 5.8` 和现有 LoadingPhase：[`SkillLab.uproject`](../../../SkillLab.uproject:2)。
3. `SkillLabSkills.Build.cs` 删除旧 Lua 私有 include/source 归属，增加对 `SkillLabLuaRuntime` 的依赖；`SkillLabPresentation.Build.cs` 做同样处理，同时保留现有 `SkillLabSkills`、`AnimGraphRuntime`、`Json` 依赖。由于当前 Lua include 只在私有 `.cpp`，优先使用 Private dependency；若采用公共 facade 类型进入公共头，再按 UBT 实际需要提升为 Public dependency。
4. 将 `SkillLabSkillBehavior.cpp` 的 VM/脚本基础操作改为新模块 facade，保留 `ValidateScript`、`RunScript`、指令预算、脚本入口名、桥函数返回表、`RequestAction`/`Finish`、失败原因和关闭时机：原行为证据见 [`SkillLabSkillBehavior.cpp`](../../../Source/SkillLabSkills/Private/SkillLabSkillBehavior.cpp:111) 和 [`SkillLabSkillBehavior.cpp`](../../../Source/SkillLabSkills/Private/SkillLabSkillBehavior.cpp:248)。
5. 将 `SkillLabCharacterPresentationLuaComponent.cpp` 的 VM/标准库/回调基础操作改为同一 facade，保留 BeginPlay/EndPlay 生命周期、四个表现桥、事件字段、状态参数、错误日志和原生 fallback：[`SkillLabCharacterPresentationLuaComponent.cpp`](../../../Source/SkillLabPresentation/Private/SkillLabCharacterPresentationLuaComponent.cpp:102)、[`SkillLabCharacterPresentationLuaComponent.cpp`](../../../Source/SkillLabPresentation/Private/SkillLabCharacterPresentationLuaComponent.cpp:155)、[`SkillLabCharacterPresentationLuaComponent.cpp`](../../../Source/SkillLabPresentation/Private/SkillLabCharacterPresentationLuaComponent.cpp:251)。
6. 在两个宿主的所有 include、Build.cs 路径和源码引用完成迁移并通过静态扫描后，才具备删除旧副本的条件。`SkillLab`、GAS、Editor 模块和脚本配置不应因本任务被改写。

## 6. 副本删除的安全前提

删除 `Source/SkillLabSkills/ThirdParty/lua-5.4.8` 和 `Source/SkillLabPresentation/ThirdParty/lua-5.4.8` 前必须全部满足：

- 新模块的 `src` 与原副本哈希一致，且只有新模块 Build.cs/source scanner 编译 Lua C 源码；Skills/Presentation 不再包含旧路径，也没有 `PrivateIncludePaths`、绝对路径或脚本工具引用。
- `rg -n -i "lua-5\\.4\\.8|#include.*(lua|lauxlib|lualib)|lua_State|luaL_|lua_[A-Za-z0-9_]+" Source Config SkillLab.uproject` 的结果只落在新模块、允许的 facade/宿主私有实现和脚本内容，不出现旧副本路径或重复 C 源文件。
- UE 5.8 `SkillLabEditor` Development 构建通过，链接输出没有 duplicate Lua symbols、unresolved Lua symbols 或错误的 import/export；若直接暴露 C API，还需用 `dumpbin /exports`/链接日志确认 Lua 符号只来自 `SkillLabLuaRuntime`。
- 自动化 `SkillLab.Skills.CastLifecycle`、`SkillLab.Skills.DefinitionValidation` 通过；NullRHI RuntimeProbe 仍能完成四个 Lua 技能和 Reset；表现 Lua 的 BeginPlay/事件/EndPlay 日志仍能出现。删除后再做一次全量构建和最小运行，而不是复用删除前的 Intermediate/Binaries。
- 删除仅针对源码副本和不再使用的上游附属文件；不得删除 `Config/*.lua`、项目日志证据或其他模块已有内容。发现副本存在未核对的外部引用时暂停删除并回交。

## 7. 实现允许与禁止范围

允许：新增 `SkillLabLuaRuntime` Runtime 模块；集中 Lua 5.4.8 源码；新增窄 C++ facade、opaque handle、必要导出宏和模块依赖；将两个宿主的 Lua 调用改为等价入口；新增针对唯一符号、脚本加载/关闭和跨模块调用的自动化或构建检查；补充验证日志。

禁止：改变 `SkillLabSkill_RequestAction`、`SkillLabSkill_Finish`、`SkillLabPresentation.*` 脚本桥的参数/返回语义；把业务回调、技能规则、GAS 结算、动画播放、事件 Router 或配置值移入 Runtime；让 Runtime 依赖 Skills/Presentation；保留两个模块同时编译 Lua C 源码；通过改脚本、改技能数值、改目标/效果规则来规避迁移失败；在没有导出/链接证据时把 Lua headers 当作跨 DLL 公共接口；顺手重构无关模块或删除未核对的资产/日志。

## 8. UE 5.8 重复验证入口

以下命令是已有可复用入口，路径以当前工作区为准；实现后应更新日志目标，避免用旧日志冒充新模块验证。

### 构建

```powershell
D:\Epic\UE_5.8\Engine\Build\BatchFiles\Build.bat SkillLabEditor Win64 Development E:\agent\game\ue\SkillLab\SkillLab.uproject -WaitMutex -NoHotReloadFromIDE
```

必要时补充 Game target：

```powershell
D:\Epic\UE_5.8\Engine\Build\BatchFiles\Build.bat SkillLab Win64 Development -Project=E:\agent\game\ue\SkillLab\SkillLab.uproject -WaitMutex -NoHotReloadFromIDE
```

### 自动化

```powershell
D:\Epic\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe E:\agent\game\ue\SkillLab\SkillLab.uproject -nullrhi -unattended -nosplash -NoSound -ExecCmds="Automation RunTests SkillLab.Skills;Quit" -TestExit="Automation Test Queue Empty" -log -abslog=E:\agent\game\ue\SkillLab\Saved\Logs\LuaRuntimeModule-Automation.log
```

至少确认 `CastLifecycle`、`DefinitionValidation` 为 `Result={Success}`，并检查构建/链接输出没有 duplicate/unresolved Lua symbol。

### RuntimeProbe 与手工入口

```powershell
D:\Epic\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe E:\agent\game\ue\SkillLab\SkillLab.uproject -game -nullrhi -unattended -nosplash -NoSound -SkillLabRuntimeProbe -ExecCmds=SkillReset,SkillTestRecover -log -abslog=E:\agent\game\ue\SkillLab\Saved\Logs\LuaRuntimeModule-RuntimeProbe.log
```

该 Probe 应保持现有四技能、事件、燃烧周期和 Reset 观察语义；它是 NullRHI 运行证据，不替代可见 PIE。可见入口继续使用 [`Run_SkillLab_DX11.bat`](../../../Run_SkillLab_DX11.bat)，并在需要时运行既有 `SkillPresentationSmoke` 检查表现 Lua 桥未断。

### 静态与符号检查

```powershell
rg -n -i "lua-5\.4\.8|#include.*(lua|lauxlib|lualib)|lua_State|luaL_|lua_[A-Za-z0-9_]+" E:\agent\game\ue\SkillLab\Source E:\agent\game\ue\SkillLab\Config E:\agent\game\ue\SkillLab\SkillLab.uproject
Get-ChildItem -Recurse E:\agent\game\ue\SkillLab\Source\SkillLabLuaRuntime\ThirdParty\lua-5.4.8\src -File
```

实现 Agent 应根据最终导出策略选择 `dumpbin /exports <SkillLabLuaRuntime DLL>` 或等价链接/符号工具，并记录命令和结果；不能只以编译成功推断跨模块符号正确。

## 9. 未决风险与实现期验证项

- **必须实现期验证**：UBT 对 `.c` 文件在新 Runtime 模块中的扫描是否与当前模块一致；Windows Development/Editor DLL 是否需要显式 `LUA_BUILD_AS_DLL`；facade 是否能覆盖当前 Skills/Presentation 用到的 Lua 栈、表、回调和错误路径而不改变返回值。
- **必须实现期验证**：公共头收回 `lua_State` 后，UHT、`SkillLabAnimationEditor` 和所有依赖模块是否仍可编译；如公共 facade 类型进入头文件，依赖方向和 API 宏必须经过 UBT 真实构建确认。
- **必须实现期验证**：两个 VM 当前生命周期是否保持隔离（Skills 每次校验/执行的临时 VM，Presentation 持有的长生命周期 VM）；不能为了“共享运行时”误把两个宿主合并成一个状态或改变 `luaL_openlibs` 语义。
- **必须实现期验证**：脚本失败、指令预算、EndPlay 关闭、CastId/Runtime generation 和表现 fallback 的日志/行为与现有证据一致；不以新模块编译通过替代运行验证。
- **风险**：直接导出上游 Lua C API 会扩大公共接口并暴露 DLL ABI；若采用必须明确宏和单一符号所有者。推荐 facade 可降低风险，但可能需要在两个宿主中调整回调注册代码，属于局部等价迁移，不应扩展为业务重构。
- **风险**：`Intermediate/`、`Binaries/` 或旧 DLL 可能掩盖重复定义和旧路径；验证应在构建系统重新生成后执行，必要时清理该模块对应的生成产物，不能把生成目录当作源码事实。
- **未决产品问题**：无。本任务的模块命名、行为保持和删除边界已经由用户目标决定；剩余问题是可由源码、UBT 和运行验证解决的技术问题，不阻塞架构交接。

## 10. 实现 Agent 必读文件

- `AGENTS.md`：仓库目录、语言和授权边界。
- `Docs/README.md`、`Docs/ProjectContext/README.md`、`Docs/ProjectContext/ProgramArchitectureContract.md`、`Docs/ProjectContext/SkillLabCodeArchitecture.md`：SkillLab 文档路由、分层和现有运行链。
- `Source/SkillLabSkills/SkillLabSkills.Build.cs`、`Source/SkillLabPresentation/SkillLabPresentation.Build.cs`、`Source/SkillLab/SkillLab.Build.cs`、`SkillLab.uproject`：模块依赖和工程注册。
- `Source/SkillLabSkills/Private/SkillLabSkillBehavior.cpp`、`Public/SkillLabSkillBehavior.h`：技能 Lua VM、桥和失败边界。
- `Source/SkillLabPresentation/Private/SkillLabCharacterPresentationLuaComponent.cpp`、`Public/SkillLabCharacterPresentationLuaComponent.h`：表现 Lua VM、桥、回调和 fallback。
- `Source/SkillLabSkills/ThirdParty/lua-5.4.8/src/`、`Source/SkillLabPresentation/ThirdParty/lua-5.4.8/src/`：迁移前两份上游源码，核对哈希和 `luaconf.h` 导出规则。
- `Docs/ProjectContext/Modules/SkillLabSkillsRuntime.md`、`CharacterPresentation.md`、`TestAndVerification.md`：当前职责、现有验证入口和限制。

本文件记录的是本任务的架构判断，不替代实现代码或最终 Review。实现已按本交接落地；`SkillLabLuaRuntime` 是整个 SkillLab 工程未来可引用的业务无关公共 Runtime，仅依赖 `Core`，当前只由已核对的 `SkillLabSkills` 和 `SkillLabPresentation` 接入。具体修改文件、实际命令输出、导出/导入检查、删除副本证据和未覆盖项见同目录 `ImplementationSummary.md`。
