# SkillLab 程序工作流中间产物

本目录按具体程序任务隔离 AI 协作过程。每个任务使用一个同级目录：

```text
ProgramWorkflow/
├── SkillSystem/          # 当前技能系统实现任务
├── <OtherTask>/          # 未来其他功能，各自独立
└── README.md
```

程序 Agent 只读取当前任务目录。例如 SkillSystem Agent 读取 `ProgramWorkflow/SkillSystem/`，不读取 `<OtherTask>/`。一次性任务上下文、子任务拆分和交付默认通过对话传递；只有需要跨对话复用的架构基线、专题结论和验证记录才写入当前任务目录。

