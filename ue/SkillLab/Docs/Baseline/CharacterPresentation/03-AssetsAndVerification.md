# 表现资产、内容工具与验证边界

## 职责

记录 a2c5969 中可定位的角色/动画资产、动作配置、内容读写工具和证据等级，防止资产存在、代码存在或一次命令成功被误写成完整视觉验收。

## 已确认业务承诺

- 表现内容应能被人和 AI 定位、局部修改、回读并校验；具体结构化格式、字段和编辑器入口由程序实现与策划需求共同维护，不在本基线冻结。
- 内容工具的成功只证明工具操作和校验范围；运行时视觉验收仍需独立的 UE 运行证据。

## a2c5969 中已实现事实

### 角色与动作资产

| 范围 | Git 路径 | 事实边界 |
| --- | --- | --- |
| Mesh/Skeleton | `Content/Characters/Mannequins/Meshes/SK_Mannequin.uasset`、`SKM_Manny_Simple.uasset`、`SKM_Quinn_Simple.uasset` | 文件在指定提交中；未由本轮重新打开资产确认内部绑定 |
| 基础动画 | `Content/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.uasset`、`BS_Idle_Walk_Run.uasset`、`MM_Idle.uasset`、Walk/Jog/Jump 目录 | 资源存在；不单独证明默认角色运行求值正确 |
| 技能动作 | `Content/SkillLab/Animations/AS_FireballCast.uasset`、Unarmed Attack 下的 `MM_Attack_01/02.uasset`、`MM_ChargedAttack.uasset` | 四项 JSON 绑定的软路径目标；不单独证明可播放、Notify 或时点 |
| 正式 Montage 样本 | `Content/Characters/Mannequins/Anims/Pistol/MM_Pistol_Fire_Montage.uasset` | 内容工具样本资产；不是四技能当前配置承诺 |

### 配置与内容工具

- `Config/SkillDefinitions.json` 为四项技能提供 `castAnimation` 软路径。代码支持 `castMontage` 与 `castAnimation` 互斥解析；当前配置没有 `castMontage`。
- `Source/SkillLabAnimationEditor/` 的 Editor-only Commandlet 按 `schemas/skilllab-animation-content.schema.json` 读取 Montage 内容，可读取并校验 Skeleton、Slot、Section/NextSection、片段、Notify、Blend、Play Rate；`-Write` 可对支持范围执行局部 Patch、保存、重载和回读。
- a2c5969 中存在 `Config/Animation/FireballMontage.json` 及缺失资产、非法 Play Rate 负例。工具文档明确写出首个 Slot/Segment 等写入边界，以及复杂 Notify、多片段精确定位和不兼容 Skeleton 覆盖不足。

## 现有证据已验证范围

- 本地 `Saved/AnimationReports/` 实际存在只读、缺失资产、非法速率和 `copy-write-readback.json`；批次记录称其曾完成临时 Montage 副本的读、写、保存、重载、回读和负例验证。未在本轮重跑，且验证对象为临时副本，不能推导基线 Montage 已被修改或四技能视觉已通过。
- 本地 `Saved/AnimationStateVisual/` 实际存在 Idle/Move/Jump/Fall/Land 截图；本地日志和批次记录曾将其与默认角色状态烟测关联。它们是历史证据，不是本轮重新生成的视口验收包。
- 本地四技能 smoke、Cast handoff 和 Editor 自动化日志实际存在，支持局部运行链、身份隔离和定义校验的历史结论；不支持完整视觉验收。

## 当前限制与待确认

- 没有当前可复现的 UE 5.8 PIE/游戏视口证据确认默认 Mesh、AnimBP、四技能动作、释放事件和取消清理的最终视觉质量。
- 没有证据确认四项动作都含有效 Notify、Notify 时点与运行事件一致，或缺资源时已有明确的玩家可辨识降级表现。
- 内容工具尚未覆盖复杂 Notify、多片段精确定位、不兼容 Skeleton 的完整负例；不把工具当前写入范围扩展成通用资产生产协议。
- 未来是否需要事件覆盖矩阵、逐项资源引用验收和可重复视觉回归，由表现与程序 Review 共同确认。

## 与其他基线的关系

角色和基础状态见 `01-CharacterPresentation.md`；技能事件和动作清理见 `02-SkillActionPresentation.md`；具体内容要求、工具批次和验收条件分别见 `../../Planning/CharacterPresentation/` 与 `../../ProgramWorkflow/AnimationWorkflow/`。
