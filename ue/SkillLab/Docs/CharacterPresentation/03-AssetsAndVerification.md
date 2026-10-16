# 表现资产与验证边界

## 职责

集中记录 Git 基线中可定位的角色/动画资产、表现接线和验证证据，防止资产存在、代码存在或一次历史日志被误写成业务承诺。

## 当前共享承诺

- 资产或代码存在不能单独证明运行表现已经验证。
- 当前共享基线只依赖 Git 基线提交 `cb7c4ef5ede2072f9aa64a648beb12daec35dc52` 及现有项目文档；未提交 Native Animation 实验和旧归档文档不能改变当前承诺。

## 当前实现与验证事实

### 基线资产

| 范围 | Git 基线路径 | 可确认事实 |
| --- | --- | --- |
| 角色骨骼 Mesh | `Content/Characters/Mannequins/Meshes/SK_Mannequin.uasset`、`SKM_Manny_Simple.uasset`、`SKM_Quinn_Simple.uasset` | 资产存在于基线提交 |
| AnimBP/基础动作 | `Content/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.uasset`、`BS_Idle_Walk_Run.uasset`、`MM_Idle.uasset`、`Walk/`、`Jog/`、`Jump/` | 基础动画资产存在；未证明默认运行绑定 |
| 技能动作 | `Content/SkillLab/Animations/AS_FireballCast.uasset`；Unarmed `Attack/MM_Attack_01/02.uasset`、`MM_ChargedAttack.uasset` | 基线配置引用了四个技能动作路径；未证明加载和播放 |
| 运行接线 | `Source/SkillLabSkills/Private/SkillLabSkillAnimationComponent.cpp`、`SkillLabSkillAnimNotify.cpp`、`SkillLabSkillPresentationRouter.cpp` | 存在事件订阅、动作播放尝试、Notify 转发和临时反馈代码 |

### 验证证据

- 基线自动化测试名可从 `Source/SkillLabSkills/Private/Tests/SkillLabCastLifecycleTests.cpp`、`SkillLabDefinitionProviderTests.cpp` 定位；它们验证生命周期和定义校验，不验证资产视觉结果。
- 项目文档记录过 NullRHI smoke 日志，观察到初始化、区域命中、修复以及燃烧/火球拒绝事件；这些是历史运行证据，且不是本轮重新执行的动画验收。
- `Docs/ProjectContext/Modules/TestAndVerification.md` 和 `SkillLabSkillsRuntime.md` 明确写出：本轮未启动 UE、未编译、未执行自动化测试，并没有完整动画释放验证。
- 二进制 `.uasset` 的路径和 Git 对象存在可以支持“资产在基线中”，不能单凭文件名确认内部 Skeleton、Anim Class、Notify、Slot 或引用关系。

## 当前限制

- 没有本轮可复现的 UE 5.8 编辑器/PIE/游戏截图、日志或自动化结果，不能宣称角色基础动作或技能动作已验收。
- 没有证据确认默认地图 `L_SkillLab_NavTest` 中玩家角色的实际表现绑定；训练假人路径还会主动隐藏 Skeletal Mesh。
- 代码支持的四个技能动作绑定和事件接线不能自动扩大为更多技能、更多状态或完整表现资产协议。
- 本页不建立 JSON Schema、资源命名规范、Montage 工具规则、批次验收模板或 AI 生产工作流。

## 待确认问题

- 是否需要在后续验证批次中逐项确认 Mesh、Skeleton、AnimBP、基础动作、技能动作和 Notify 的资产引用？
- 是否需要把技能事件到视觉反馈的覆盖矩阵作为单独的人类验收材料？
- 历史 NullRHI smoke 是否仍对应当前基线运行入口，还是需要重新生成可复现证据？

## 与其他基线的关系

本页只保存证据等级和验证边界。业务事件语义见 `02-SkillActionPresentation.md` 与 `../SkillSystem/06-FeedbackAndObservability.md`；项目级测试入口见 `../ProjectContext/Modules/TestAndVerification.md`。发现证据不足时，不在本页替未来方案补齐承诺。

