---
name: ue-mcp
description: 用于在 Unreal Editor 运行时连接、诊断和使用 UE 的 Model Context Protocol 服务。仅在用户明确要求使用 UE MCP、检查 MCP 连接、读取或修改 UE 编辑器对象时加载；普通 UE 学习和仓库浏览不要默认加载。
---

# UE MCP 使用 Skill

## 文件职责

本 Skill 负责 UE MCP 的连接前提、端口选择、能力发现、只读检查和写入边界。

不同项目的端口登记表位于：

`E:/agent/game/ai/ue-mcp/ue-mcp-ports.json`

项目实际读取的启动配置仍位于各自 UE 项目的：

`<项目路径>/Config/DefaultEditorPerProjectUserSettings.ini`

本 Skill 不代替项目文档，也不记录具体 Blueprint、关卡或玩法实现事实。

如果目标是某个 `ue/` 项目，先读取 `.codex/skills/ue-project-context/SKILL.md`，获得项目入口、文档路由和资产边界；然后再读取本 Skill 的 MCP 连接规则。UE MCP 不替代项目上下文入口。

## 何时加载

仅在以下情况加载：

- 用户明确要求使用 UE MCP。
- 需要诊断某个 UE 项目的 MCP 是否启动。
- 需要通过 MCP 读取或修改 UE 编辑器对象。
- 需要为新 UE 项目配置 MCP 或登记端口。

## 连接前提

UE MCP 由正在运行的 Unreal Editor 进程提供，不是独立的常驻后台服务。开始连接前：

1. 读取 `ai/ue-mcp/ue-mcp-ports.json`，找到目标项目的端口和 endpoint。
2. 确认目标项目的 `.uproject` 启用了 `ModelContextProtocol`。
3. 需要完整 UE 工具集时，确认 `.uproject` 启用了 `AllToolsets`。
4. 确认 Unreal Editor 正在打开该项目。
5. 查看项目 `Saved/Logs/<ProjectName>.log`，确认出现：

```text
Starting MCP server on port <port>
Created new HttpListener on 127.0.0.1:<port>
```

6. 再对登记的 endpoint 执行 MCP `initialize`、`notifications/initialized` 和只读 `tools/list` 检查。

如果端口没有监听，先报告 MCP 服务未启动，不要把“对话中没有动态工具”当成 UE 项目没有 MCP。

## 项目配置

每个项目的 `Config/DefaultEditorPerProjectUserSettings.ini` 至少应包含：

```ini
[/Script/ModelContextProtocolEngine.ModelContextProtocolSettings]
ServerUrlPath=/mcp
ServerPortNumber=<项目端口>
bAutoStartServer=True
bEnableToolSearch=True
```

新项目应在端口登记表中选择未占用端口，并同步更新自己的 `.uproject` 和上述配置。多个同时运行的编辑器不能使用同一端口。

## 能力发现顺序

连接后按以下顺序发现能力：

```text
initialize
-> notifications/initialized
-> tools/list
-> list_toolsets
-> describe_toolset
-> 目标工具调用
```

优先使用项目和编辑器提供的结构化工具，不使用 Computer Use 模拟点击来替代可以由 MCP 完成的操作。

## 读写边界

读取 Blueprint、关卡、Actor、Component、StateTree、资产引用、配置和日志属于只读勘察。创建、删除、复制、覆盖或修改 Blueprint、关卡、资产、配置和编辑器状态属于写入操作，必须先得到用户对具体范围的明确授权。

执行写入前，说明：

- 目标项目和对象路径。
- 将要改变的字段、资产或对象。
- 是否会生成、覆盖或删除产物。
- 如何通过日志、工具返回值或 PIE 验证。

执行写入后，确认工具返回成功，并重新读取目标对象或文件；不能只根据调用没有报错就宣布完成。

## 失败诊断

- 插件未加载：检查 `.uproject` 和启动日志。
- 插件加载但没有 `Starting MCP server`：检查 `DefaultEditorPerProjectUserSettings.ini` 和是否重启编辑器。
- 端口冲突：检查端口登记表和本机监听进程，改用未占用端口后重启编辑器。
- `initialize` 成功但后续请求返回 404：重新初始化会话并正确保留 `Mcp-Session-Id`。
- MCP 能连接但工具集很少：检查 `AllToolsets`、相关 UE 插件和 Toolset Registry 日志。
- MCP 无法读取某类资产：记录具体工具、失败步骤和错误，改用仓库文件、UE 编辑器只读检查或日志补充确认。

## 验证标准

至少保留以下证据：

1. 目标编辑器进程正在运行。
2. 目标端口处于监听状态。
3. MCP `initialize` 返回成功并获得会话 ID。
4. `tools/list` 或 `list_toolsets` 返回成功。
5. 对具体项目对象的读取结果能够定位到对象路径或文件路径。
