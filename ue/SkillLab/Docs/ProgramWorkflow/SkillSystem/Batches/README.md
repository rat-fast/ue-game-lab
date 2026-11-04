# SkillSystem 程序批次产物

每个批次建立独立目录 `Batches/<BatchName>/`，与策划侧批次名称对应。

## 当前批次

- [SkillChainRefactor](SkillChainRefactor/README.md)：通用运行基建和当前已覆盖技能定义迁移。

批次目录按需包含：

- `README.md`：本批次输入、范围和 Agent 读取边界；
- `ArchitectureBaseline.md`：架构 Agent 的放行包；
- `Architecture/`：确实需要独立交接的专题；
- `Implementation/`：实现摘要和可复现验证事实；
- `Review/`：Review 结论和文档同步状态。

不建立独立验证 Agent 或原始日志目录。
