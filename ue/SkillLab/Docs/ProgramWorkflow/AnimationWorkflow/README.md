# 角色与技能表现程序工作流

## 职责

本目录保存角色与技能表现程序任务的架构基线、实现交接、独立 Review 和最小验证证据。

它不保存表现策划正文，不维护共享业务基线，也不把历史调试过程写成当前规则。

## 标准批次结构

```text
Batches/<BatchName>/
├─ README.md
├─ ArchitectureBaseline.md
├─ Implementation.md
├─ Review.md
└─ Evidence/
```

## 批次流程

```text
共享表现基线
  -> 策划批次需求
  -> 程序/表现架构基线
  -> 实现交接
  -> 构建、资产和运行验证
  -> 独立 Review
  -> 主控决定下一批
```

## 当前状态

当前角色表现 Lua 试点由 [CharacterPresentationLuaPilot](../CharacterPresentationLuaPilot/README.md) 作为唯一程序任务入口，需求以 [Batch02-LuaCharacterPresentationPilot](../../Planning/CharacterPresentation/Batches/Batch02-LuaCharacterPresentationPilot/README.md) 为准。

Lua 任务目录已有架构交接和实现/验证状态记录。记录显示 Lua 接入、构建及局部自动化已完成，但 P0/P1 尚未通过：缺少可见 PIE/视口验收，且仍有 Fall、施法竞态复现和技能差异化可见反馈等证据缺口；独立 Review 文档尚未建立。不得将“已有实现记录”表述为整批已验收。

`Batches/Batch01-CharacterFoundation/` 是先前的角色基础实现批次，保留其实现和 Review 记录作历史参考；其未关闭切片项不等同于 Lua 试点状态，也不作为 Lua 批次入口。
