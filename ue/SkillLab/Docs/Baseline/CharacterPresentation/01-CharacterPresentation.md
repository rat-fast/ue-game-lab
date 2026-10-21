# 角色基础表现基线

## 职责

定义角色 Mesh、AnimBP/AnimInstance 和基础动作在表现层的业务边界。技能规则、属性、输入和施法生命周期分别由 `../SkillSystem/` 与项目代码文档负责。

## 已确认业务承诺

- 角色表现观察技能和角色运行事实，不能自行决定技能是否成功、命中、扣费或完成。
- 基础移动表现与技能动作是可分离职责；技能动作退出后应接续角色当时真实的基础状态，不应无条件回到 `Idle`。是否保留收招表现仍待确认，具体资源和技术载体不在本页冻结。
- 项目继续使用 UE 的骨骼、姿势求值、融合和压缩能力；本方向只要求把项目级逻辑放到可维护的代码/内容入口，不要求重写 UE 底层动画能力。

## a2c5969 中已实现事实

- `ASkillLabCharacter` 创建角色动画状态组件；该组件从 `ACharacter`/`UCharacterMovementComponent` 读取速度、方向、加速度和 Falling 状态，形成 `Idle`、`Move`、`Jump`、`Fall`、`Land` 快照。
- `USkillLabCharacterAnimInstance` 读取快照并桥接到既有 `ABP_Unarmed` 变量。`ABP_Unarmed` 仍由 UE 动画状态机和资源求值姿势；代码没有替换骨骼融合或姿势求值。
- Git 基线包含 `SK_Mannequin`、Manny/Quinn Mesh、`ABP_Unarmed`、Idle/Walk/Jog/Jump 等基础动画资源。训练假人路径会隐藏 Skeletal Mesh 并使用临时静态几何体，不能作为正式角色外观事实。

## 现有证据已验证范围

- a2c5969 的实现交接记录了 UE 5.8 构建、AnimBP 资产编译和默认角色运行烟测；本地存在 `Saved/Logs/SkillLabAnimationVisual-Acceptance.log`、`Saved/AnimationStateVisual/` 和 `Saved/AnimAssetVerify.log`，记录过 AnimInstance 消费快照及 `Idle -> Move -> Jump -> Fall -> Land` 迁移。
- 上述材料是未在本轮重跑的本地历史证据；它支持状态链曾被运行观察，不等于本轮重新验证，也没有提供迁移前/迁移后一对一质量对照或完整技能结束后姿势验收。

## 当前限制与待确认

- `BP_SkillLabCharacter` 的默认 Mesh、Skeleton、Anim Class、状态机内部结构和 Socket 绑定虽可从基线资产定位，但没有在本轮重新打开 UE 核验。
- 没有当前可复现证据证明 Idle、Move、Jump、Fall、Land 在正式入口中的主观视觉质量、所有边界迁移或技能动作结束后的姿势恢复。
- 不把 `ABP_Unarmed` 的节点、状态机、BlendSpace、Slot 或类名冻结为公共承诺；基础动作是否扩展到受击、死亡等状态待策划确认。

## 与其他基线的关系

技能动作和施法事件见 `02-SkillActionPresentation.md`；资产、内容工具和证据等级见 `03-AssetsAndVerification.md`；技能阶段和结果以 `../SkillSystem/02-RequestLifecycle.md`、`../SkillSystem/06-FeedbackAndObservability.md` 为准。
