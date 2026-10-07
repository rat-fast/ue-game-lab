# 技能系统策划入口

## 状态与范围

本目录是策划需求草稿，供用户 review。用户确认的当前方向是单机俯视角、MOBA 式操作、ARPG 式技能丰富度、敌我关系与单位/地面/方向目标，以及 AI 技能生产验证。具体数值、交互和默认规则均为建议；实现事实与运行证据在程序文档记录。

## 目录与阅读顺序

```text
SkillSystem/
├── README.md
├── RepresentativeSkillVerticalSlice.md   # 核心范围与批次覆盖
├── Infrastructure/                      # 基建需求
│   ├── README.md                        # 公共概念、规则、模块导航
│   ├── M01～M09                         # 长期能力模块
│   └── Batches/Batch01...               # 当前批次的实施子集与验收
└── Skills/                              # 技能配置设计
    ├── README.md                        # 内容填写与能力消费约定
    └── SkillDesigns.md                  # 六个技能索引
```

- 看当前整体范围：[核心策划文档](RepresentativeSkillVerticalSlice.md)。
- 做基建实现：读 [基建入口](Infrastructure/README.md)和当前 [Batch01 基建需求](Infrastructure/Batches/Batch01-SixSkillVerticalSlice/README.md)，再读矩阵列出的模块与程序接口。
- 做技能侧实现：读 [技能配置入口](Skills/README.md)和 [具体技能设计](Skills/SkillDesigns.md)，再读程序提供的能力接口。

两层策划分别自足。基建需求用独立例子定义可复用能力，技能配置完整写出自身业务结果；正常实施各读本层策划，通过程序接口协作。接口缺口、含义冲突或需求变化才进入跨层反馈。

## 职责边界

策划定义体验、业务规则、参数含义、反馈和验收。程序接口、GAS/C++ 组织、序列化字段与构建/测试流程由 [程序工作流](../../ProgramWorkflow/SkillSystem/README.md)维护。

这里的“技能配置层”指具体技能内容规格，可以由技能侧代码、参数和资源绑定共同实现。它不预设最终配置格式或 DSL。

## 文档维护

同一公共规则在基建层维护，技能设计写明本技能选择与结果。参数以具体技能正文为准。发现无法通过已有能力表达的需求，交接业务缺口与验收例子，再修订对应层。

旧的总览、模块和配套材料已按职责合并/迁移；新的正文路径以上述目录为准。





