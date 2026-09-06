---
name: git-repository-management
description: "管理本仓库的 Git 和 Git LFS，包括仓库检查、UE 资产分类、暂存、提交、远端推送和版本管理故障排查；仅在任务明确涉及 Git/LFS 或上传时使用。"
---

# Git Repository Management

## When To Use

当用户要求初始化、检查、配置、提交、推送、回退或排查 Git/Git LFS，或者要求上传本仓库时使用。仅修改代码或 UE 资产、但没有版本管理任务时，不自动执行 Git 操作。

## Required Context

开始操作前先读取根目录 `AGENTS.md`。如果任务涉及 `ue/` 下的 Unreal Engine 项目，还必须先读取 `$ue-project-context` 的规则以及相关项目文档。

## Core Rules

- 普通文本、C++、配置、文档和项目描述文件由 Git 管理。
- 需要保留历史的 `.uasset`、`.umap` 以及相关关卡外部对象由 Git LFS 管理。
- `Binaries/`、`DerivedDataCache/`、`Intermediate/`、`Saved/`、`.vs/` 不得提交。
- `Content/Vendor/`、`Content/Marketplace/`、`Content/ThirdParty/` 用于可重新下载且不保留历史的外部资产，默认由 `.gitignore` 排除。
- 是否提交资产由路径职责、来源和历史价值决定，不由项目当前大小或文件扩展名单独决定。
- 对未分类路径、疑似外部资源、缺少 LFS 属性或关卡依赖不完整的情况，先报告并请求确认。

## Safe Workflow

1. 只读检查仓库根目录、远端、当前分支和 `git status`。
2. 根据 UE 项目规则分类新增、修改和删除的文件。
3. 检查 `.gitignore`、`.gitattributes`、`git check-attr` 和 `git lfs status`。
4. 只暂存本次任务范围内的文件，不盲目使用 `git add .`。
5. 用 `git diff --cached --stat` 和文件清单预览提交内容。
6. 在用户明确授权后执行 commit；向远端 push 也需要明确授权。
7. 提交或推送后再次检查状态，并报告提交、LFS 文件和未处理变更。

禁止使用 `git reset --hard`、`git clean -fd` 或其他破坏性命令，除非用户明确要求且目标已经核实。

## Review Standard

提交说明按一个可验证的功能或工作成果组织，例如“完成敌人生成系统”或“更新点击移动测试关卡”，不要按零散的编辑器保存次数组织。Blueprint 和关卡通常只能进行文件级版本恢复，不能假定 Git 能提供节点级 diff。
