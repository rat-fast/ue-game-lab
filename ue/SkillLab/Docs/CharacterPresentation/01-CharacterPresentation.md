# 角色基础表现基线

## 职责

定义角色在表现层的最小业务语义，以及基础 Mesh、动画实例和移动动作在当前版本中的证据边界。角色的技能规则、属性和输入仍由 `../SkillSystem/` 与项目代码文档负责。

## 当前共享承诺

- 角色表现是技能运行事实的观察者。角色外观、基础动作或反馈不能自行决定技能是否成功、是否命中、是否扣费或何时完成。
- 角色基础表现与技能表现是两个可分离的职责：基础移动/待机状态不等于某次施法动作，技能动作也不替代角色的移动、目标和效果规则。
- 技能表现需要能够关联对应施法身份以及取消、完成等终态事实，避免旧施法的表现影响新的施法；具体由哪个对象或技术载体承载，由程序架构和表现实现阶段决定。

上述承诺来自 `Docs/SkillSystem/00-DomainModel.md`、`06-FeedbackAndObservability.md` 的“事件供表现观察”边界，以及基线代码中角色与技能运行组件分职责的事实。

## 当前实现与验证事实

- 基线角色类继承 `ACharacter`，构造相机、能力系统、技能运行组件和技能动画组件；来源：`Source/SkillLab/SkillLabCharacter.h`、`SkillLabCharacter.cpp`（Git 基线提交）。
- 基线内容树包含 Mannequin 骨骼 Mesh：`Content/Characters/Mannequins/Meshes/SK_Mannequin.uasset`，以及 `SKM_Manny_Simple.uasset`、`SKM_Quinn_Simple.uasset`。
- 基线内容树包含 `Content/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.uasset`、待机/移动混合资源 `BS_Idle_Walk_Run.uasset`、`MM_Idle.uasset`、Walk/Jog 方向动作和 Jump 目录动作。文件存在只证明资产进入 Git 基线，不证明默认 Blueprint 已选择或运行时求值正确。
- 基线角色代码会隐藏训练场 Blueprint 中名为 `VisualCapsuleCore`、`VisualCapsuleTop`、`VisualCapsuleBottom` 的静态占位组件；训练假人路径还会隐藏角色 Skeletal Mesh 并创建临时静态几何体。来源：`Source/SkillLab/SkillLabCharacter.cpp`。
- 已有项目文档把默认地图、角色 Blueprint 和正式动画资源绑定列为需要在 UE 5.8 编辑器/游戏进程中复验的内容；本轮没有重新执行该验证。

## 当前限制

- 没有基线级证据确认 `BP_SkillLabCharacter.uasset` 的默认 Skeletal Mesh、Anim Class、状态机拓扑或引用的骨骼/Socket。
- 没有基线级运行证据确认 Idle、Walk、Jog、Jump、落地或朝向切换在默认地图中可见且正确。
- 训练假人使用静态几何体的代码路径不能被解释为正式角色外观承诺。
- 不冻结角色 C++ 类名、Blueprint 组件名、AnimBP 图结构、状态机、BlendSpace、Montage Slot 或未来移动框架。

## 待确认问题

- 默认可控角色在正式运行入口中实际使用哪个 Skeletal Mesh、AnimBP 和骨骼版本？
- 基础动作的业务覆盖范围是否只需要待机/移动，还是还包括跳跃、受击、死亡等状态？
- 训练假人与正式角色是否需要不同的表现验收标准？

## 与其他基线的关系

技能请求、阶段和结果以 `../SkillSystem/` 为准；技能动作的事件关联见 `02-SkillActionPresentation.md`；资产与验证证据见 `03-AssetsAndVerification.md`。本页不重复技能数值或程序架构。

