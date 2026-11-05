# 技能系统策划入口

## 目录原则

策划文档按“长期目标”和“批次”组织。长期目标描述希望系统逐步获得的能力方向；每个批次先明确具体技能需求，再从这些技能中抽取可复用的公共基建需求。

```text
Planning/SkillSystem/
├── README.md
├── LongTerm/
│   ├── README.md
│   └── Infrastructure/       # M01–M09 长期能力主题
└── Batches/
    └── Batch01-SixSkillVerticalSlice/
        ├── README.md
        ├── Skills/           # 本批具体技能需求
        └── Infrastructure/   # 为满足本批技能而提取的公共能力需求
```

当前技能系统通用链路及内容生产边界改造需求见 [技能链路改造](Batches/SkillChainRefactor/README.md)。历史 Lua 技能组合试点见 [Batch02-LuaSkillCompositionPilot](Batches/Batch02-LuaSkillCompositionPilot/README.md)；其冲突条款在本次改造范围内由新需求取代。

技能时序、操作规则和 Tag 语义见 [技能战斗规则框架](Batches/SkillCombatRules/README.md)；当前批次规则已由用户确认，程序实现与验收状态见[对应程序专题入口](../../ProgramWorkflow/SkillSystem/Batches/SkillCombatRules/README.md)。

## 阅读方式

- 了解长期能力方向：读 [长期目标](LongTerm/README.md)。
- 设计或实现某一批：先读该批 [README](Batches/Batch01-SixSkillVerticalSlice/README.md)，再看该批 `Skills/` 与 `Infrastructure/`。
- 技能需求描述具体玩家体验、目标、时序、效果、参数和验收；基建需求从技能组合中抽取公共能力、通用规则和独立验收。
- 两类需求在文档和实现职责上解耦，但必须通过批次覆盖关系保持紧密联系。基建应服务已列出的技能需求，不能先写一套与样本无关的“大而全”能力清单。

## 与共享基线的关系

[共享技能基线](../../Baseline/SkillSystem/README.md)只记录当前已经确认并稳定承诺的公共能力。长期目标和批次策划可以提出尚未具备的新能力；能力实现并验证、经确认后，才考虑纳入基线。程序现状作为能力核对证据，不替代策划需求。

策划定义体验、业务规则、参数含义、反馈和验收，不读取程序工作流或项目实现文档。程序技术接口、GAS/C++ 组织、序列化字段与构建/测试流程由程序侧负责；具体任务中用户明确指定的技术方向以该任务需求为准。


