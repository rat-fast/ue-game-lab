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

旧的 Native Animation 批次已归档。新的第一批需要在共享基线和策划需求重新建立后创建。
