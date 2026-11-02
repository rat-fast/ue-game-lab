# SkillLab 美术工作流

本目录保存可供 AI 和人类复查的角色表现资源工作产物。当前包含按 AnimSequence 资产归档的动作总览、视觉语义记录和实际播放 Montage 映射；其他资源类型按 `VFX/`、`Audio/` 等独立目录扩展。

## 目录

```text
ArtWorkflow/
├─ README.md
├─ Actions/
│  ├─ README.md
│  ├─ Index.md
│  └─ <AnimSequence资产名>/
│     ├─ page-0000.png
│     ├─ ...
│     ├─ overview.json
│     ├─ observation.md                  # 动作语义完成后生成
│     └─ source-to-montage.json          # 建立实际播放映射后生成
└─ Diagnostics/
   ├─ README.md
   ├─ MetadataOnly/                      # 只有结构信息、没有视觉页图
   ├─ Failed/                             # 加载、渲染或发布失败
   └─ Incomplete/                         # 中断或产物不完整
```

## 归档规则

- 资产完整 UE 对象路径是唯一主键；目录名取对象名，不使用技能名。
- 一个资产一个 `Actions/<assetName>/` 目录；同名资产冲突时使用对象路径短哈希区分。
- 批量处理只是一次调用方式，不改变资产归档主键；正式产物按源 AnimSequence 名归档，不保留批次 ID 或 generation ID 子目录。
- 成功总览分页图、`overview.json` 和动作语义记录直接放入对应资产目录；重生成更新当前总览，不在正式归档保留历史 generation 副本。
- 仅元数据、失败或中断结果进入 `Diagnostics/`，不能供动作语义 Agent 当作完整视觉输入。
- 技能与动作的多对多关系由后续配置/映射文件表达，不写入资产目录名。

## 总览与语义边界

- `Actions/Index.md` 供人快速按动作特征查找资产，并链接观察记录和全部总览页。
- `page-*.png` 是人类和视觉 Agent 观察动作的主要材料；动作语义 Agent 应逐页查看本次指定资产的全部分页图。
- `overview.json` 是精简索引，只记录资产、生成身份、帧率、样本数、总时长、分页帧范围和产物状态；帧时间由帧率规则推导。
- `observation.md` 记录动作阶段、可辨识的峰值与关键时点、分页证据和不确定性。身体/肢体动作峰值与颜色、白光等视觉线索峰值分开描述；可记录帧段、多个峰值或“无清晰峰值”，不强行精确到单帧。
- 峰值为后续设计和配置提供视觉时间锚点，不自动代表 Notify、释放、碰撞、命中或业务效果；特效性质无法从图片判断时，记录可见线索并保留不确定性。
- `source-to-montage.json` 只在真实播放 Montage 建立后记录 AnimSequence 到 Montage 的资产与时间映射；它不替代技能定义中的实际 Montage 绑定。
- 持久 Montage 的创建/绑定及 Notify 写入属于技能内容配置工作；语义观察只提供动作证据，不自行决定事件用途或写入资源。
- Root Motion、骨骼或 Socket 轨迹等详细数据不默认写入 `overview.json`，确有需求时作为独立诊断文件输出。

动作总览工具可以批量生成，但发布后的正式结果必须按资产目录归档。动作语义 Agent 只读取本次指定资产的 `overview.json` 及其分页图片；诊断目录仅用于排查，不作为正常视觉分析输入。
