# SkillLab 职责结构与测试生产边界专项 Review

## 审查状态与结论

- **范围**：`ue/SkillLab/Source/SkillLabGAS/`、`Source/SkillLabSkills/` 的自写生产代码、相应 `Private/Tests/`，以及本报告所需的直接调用者和 Build.cs。排除第三方 Lua、生成文件、二进制资产和历史验证副本；只沿生产/测试直接依赖核对 Runtime、ASC、Ability 与测试 fixture。
- **源码状态**：静态核对时 HEAD 为 `937171e7cdc9c8aa4b617b06e19bae114632a370`。`git status --short` 显示 `docs/Investigations/SkillLab/CodeQualityReview/` 未跟踪；其中 `MainReview.md` 是并行产生的材料，不作为本报告的先验结论。本报告完成前无法锁定该目录之后是否又发生并行变化。
- **验证边界**：未运行构建、自动化、编辑器、PIE、游戏或资产检查。代码调用者、宏边界和 Build.cs 为静态证据；不把源码静态审查描述成运行结果。
- **结论**：模块及主要运行职责整体可理解；没有根据行数单独判定生产文件应拆分。确认一项低优先级的测试/生产编译边界维护问题：仅测试 fixture 调用的 `ExecuteDeclaredAction()` 无参重载是 `USkillLabLuaSkillBehavior` 的 `private` 成员，并通过 `friend FSkillLabProductionFixture` 授权测试访问，不是公共 API；但声明和实现均未受开发测试宏约束，静态证据表明它随 Skills Runtime 模块的一般编译面保留。903 行 `CastGameplayEvent` 自动化确有混合职责与共享状态成本，但新测试已分担多项场景，旧测试仍独有关键跨层链路，故应判断为**部分冗余、仍有独立价值**，而非可直接删除。未发现由这些结构证据本身证明的运行正确性缺陷。

## 覆盖清单

- 读取仓库 `AGENTS.md`、项目入口 `ue/SkillLab/Docs/README.md`、`Docs/ProjectContext/README.md`、`ProgramArchitectureContract.md`、架构地图、模块索引、GAS/Skills/测试模块事实及相应 SkillSystem 基线和 `SkillChainRefactor`、`SkillCombatRules` 需求入口。
- 核对 `SkillLabGAS` 与 `SkillLabSkills` 的文件组织、关键生产类型/函数职责、模块公开/私有依赖、导出类、测试宏与直接测试调用者；重点检查 `USkillLabSkillRuntimeComponent`、Lua Behavior、Gameplay Ability、ASC、测试 fixture。
- 将 903 行 `SkillLabCastGameplayEventTests.cpp` 与当前独立的定义、执行仲裁、输入生命周期、重入、提交隔离、资源 Reset、回执/反馈等测试逐项按场景比较。没有逐行重新执行断言，也没有宣称测试当前通过。
- 相邻模块只确认相关依赖边界：Skills 通过 GAS ASC/效果与 Ability 工作，测试共享 Provider 的候选定义替换和真实 World/ASC/Runtime；未作角色、Presentation、Lua 实现或资产的全面 Review。

## 发现

### R1：测试适配实现随 Runtime 模块常驻编译

**分类：低优先级的测试/生产编译边界维护问题；不是公共 API 问题，也不等同于业务逻辑错误。**

[SkillLabSkillBehavior.h:38](/E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Public/SkillLabSkillBehavior.h:38) 将 `USkillLabLuaSkillBehavior` 声明为 `SKILLLABSKILLS_API` 类；但无参 `ExecuteDeclaredAction()` 位于该类的 `private:` 区域（第 58 行起），并由 [同文件第 60 行](/E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Public/SkillLabSkillBehavior.h:60) 的 `friend class FSkillLabProductionFixture` 仅向测试 Fixture 开放访问。它不是公共 API。第 46 行的 `public` 方法 `RequestDeclaredAction(int32)` 是另一个生产入口，与该无参重载不同。无参重载明确注释为“既有失联回执 fixture 的真实生产调用适配”。其实现位于 [SkillLabSkillBehaviorProduction.cpp:9](/E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabSkillBehaviorProduction.cpp:9)，内部获取生产 `ProductionHandle` 后转调真正执行重载；正式生产调用路径则是 [SkillLabSubmissionPointExecution.cpp:91](/E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabSubmissionPointExecution.cpp:91) 的 `ExecuteDeclaredAction(*Point)`。

唯一无参入口的调用者为 `FSkillLabProductionFixture::Execute`：[SkillLabProductionFixture.h:120](/E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/Tests/SkillLabProductionFixture.h:120)。该 Fixture 在多个 `Private/Tests` 用例里用于测试丢失同步交付回执时仍运行真实效果/投射物生产路径，例如 `SkillLabProductionRecordsTests.cpp`、`SkillLabLateSubmissionTests.cpp`、`SkillLabSubmissionFeedbackIdentityTests.cpp`；未发现非测试生产调用者。Fixture 本身在 [头文件第 3 行](/E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/Tests/SkillLabProductionFixture.h:3) 被 `WITH_DEV_AUTOMATION_TESTS` 包围，但无参方法在类声明和 `.cpp` 定义中都没有该条件。`SkillLabSkills.Build.cs` 定义的是常规 `SkillLabSkills` Runtime 模块依赖，测试源码位于该模块的 `Private/Tests`，没有单独测试模块；因此静态证据支持该私有方法实现随模块的一般编译面保留。此结论仅针对源级声明/实现和模块组织；未检查具体 Target 的预处理结果、链接产物或导出符号表。

**为什么有成本**：该方法是受 C++ 访问控制限制、仅供 friend Fixture 使用的内部测试适配，不扩大类的公共 API。维护成本在于测试专用入口和实现仍位于生产类/Runtime 模块的一般编译路径中，阅读实现时需要区分它与由提交点调度的正式入口。测试确实需要绕过普通脚本触发以模拟回执丢失，但执行体继续复用正式动作逻辑是合理的，不能把测试目的误判成行为副本或机制绕过。

**最小后续核对方向**：实现负责人可评估是否值得将该测试适配移入测试专用访问层，或在可行的 Target/编译边界下约束其编译，同时保留“调用真实生产动作、只丢回执”的场景。当前静态证据证明它不是公共 API，也不足以证明其编译开销或链接可见性造成实际影响；Review 没有证明具体改法必要，也不建议删除该保障；这不是修复授权。

### R2：`CastGameplayEvent` 综合测试职责混合，独立测试已覆盖部分职责但不是替代品

**分类：确认的维护成本；独立保障仍有价值。**

旧测试注册于 [SkillLabCastGameplayEventTests.cpp:141](/E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/Tests/SkillLabCastGameplayEventTests.cpp:141)，单个 `RunTest` 从第 144 行延伸至 901 行，文件共 903 行。前段构造并解析多套 JSON/Lua Provider fixture（160–221 行），随后直接在一个 Editor/Game/PIE World 创建 Actor、ASC、Runtime（约 231–264 行）；整个测试反复复用该 Runtime、同一 ASC/Actor、Provider 全局定义目录、能量账本和事件 Delegate。尾部通过 latent command 等待自然完成、下一帧消费缓存意图及失败门控（第 899–900 行）。因此改一个释放/仲裁规则时，首轮阅读往往需要从 fixture 和之前共享状态变更一路追至目标断言；修改某个 setup 或提前失败条件也可能影响后续多个场景。

新测试已经真实分担了一部分旧职责：

- `CastControlAndRules`（67 行）和 `SharedCastArbitration`（`CastExecutionTests.cpp` 中）独立测规则/共同仲裁；
- `CastRequestReentry`（84 行）单测请求重入；
- `CastIntentLifecycle`（48 行）单测意图生命周期；
- `CastSubmissionIsolation`（70 行）、`MultipleSubmissions`（45 行）测提交点及隔离；
- `ResetResourceCompletion` / `ResetSubmissionDuringCompletion`（各由 238 行文件中的独立注册测试承担）测 Reset/资源回执；
- `UnconfirmedSubmission`、`ProductionRecords`、`SubmissionReconciliation`、`LateSubmission` 与两个 SubmissionFeedback 测试分别验证未确认、效果动作、回执与反馈路由。

Fixture 也有职责边界：`FSkillLabCombatTestFixture`（127 行）创建独立 World 并用 scoped helper 保存/恢复 Provider 全局目录；`FSkillLabProductionFixture`（155 行）以真实 Provider、Runtime、ASC 和动作验证生产回执情形。两者复用真实机制，不创建一套替代生产生命周期。需要注意 Provider 定义目录仍为进程级共享状态，Fixture 通过保存/替换/析构恢复，不意味着允许这些测试并行共享进程；综合测试自己的保存与恢复也有 `ON_SCOPE_EXIT` 和 latent command 最终恢复机制。

然而，`CastGameplayEvent` 仍独有完整的跨层烟囱价值：正式请求激活 GAS Ability、裸 ASC GameplayEvent 因缺少身份不能释放、经过 Runtime 校验的 Notify 到当前 Ability 并推进真实行为、效果和资源提交只发生一次（第 743–779 行）；它还在同一真实能力链验证 Recovery 缓存、旧 Cast Notify/window 身份隔离和自然完成后延迟消费（约 800–900 行）。现有小测试多用独立 CombatFixture 或针对单个动作/仲裁断言，不能仅凭测试名或“有新 fixture”认定已替代该 Provider→Runtime→GAS Ability→GameplayEvent 路径。

**失败反馈具体影响**：普通 `TestTrue` / `TestEqual` 通常记下失败并继续，旧测试不是每条断言失败就整体中止。例如第 773 行释放计数失败后，后续仍会断言效果、费用、Recovery；这可能产生次生失败，但能保留本场景更多反馈。确有 setup 的显式 `return false`：Provider fixture 解析失败（160–165 行）、缺少 World（245–248 行）、所需定义解析失败（260–264 行）、以及中段 Hold Behavior 缺失（309 行），会跳过后续整批场景。尾部 latent command 的等待超过 120 帧时在第 60–61、107–108 行报告失败并结束该异步步骤；清理与恢复依赖 latent command 析构。由此可知，单独筛选 `SkillLab.Skills.CastGameplayEvent` 的失败是一个大执行单元的结果，定位到具体场景需结合断言文本和日志，Setup 失败会完全遮蔽后续覆盖。

**策划可理解的例子**：如果策划把释放点从动画 Notify 改成别的触发类型，相关验证不只问“触发有没有到”，还检查是否在触发前不扣费、裸事件不冒充当前施法、重复 Notify 不重复提交、Recovery 期间不抢先起下一招。大测试把这些场景和后面的受击/控制/闪避/缓存及异步消费串在同一个世界状态里。独立测试能更快反馈一部分规则，但需要保留至少一条端到端烟囱以证明跨模块接线仍通。

**最小后续核对方向**：按职责把已由独立用例覆盖的 Provider/仲裁/提交/Reset 场景从综合执行单元逐步迁出，同时保留覆盖 Provider 加载、正式 Ability 激活、身份校验的 GameplayEvent 端到端主线及必要缓存消费烟囱。每迁出一类，都要核对新用例是经同一正式入口、同一提交机制，而非仅复制期望结果；本报告不指定新测试架构，也不建议直接删除旧测试。

### R3：Runtime 执行文件较大，但现有证据不足以将其定性为可独立分离的职责

**分类：证据不足/可选结构观察；不是确认缺陷。**

[SkillLabSkillRuntimeExecution.cpp](/E:/agent/game/ue/SkillLab/Source/SkillLabSkills/Private/SkillLabSkillRuntimeExecution.cpp) 为 555 行，是当前主体生产 `.cpp` 中较大的文件，超过项目约 400 行参考。其内容包括 Notify/Ability 事件接入（68–157 行）、Windup/Release/取消/Recovery/Cast 清理（158–407 行），以及执行身份、效果应用和 Behavior 回调（408–555 行）。这些子职责在概念上可命名，但它们共同操作同一个 `USkillLabSkillRuntimeComponent`、`USkillLabCastExecution`、Ability/Behavior 身份和事件状态；同一个释放更改经常需要从 Notify 校验追到 Release，再看资源提交和阶段清理。另有 `SkillLabSkillRuntimeRequests.cpp` 333 行与 `SkillLabSkillRuntimeComponent.cpp` 365 行，说明当前实现已将请求仲裁和组件初始化/Reset/事件路由从执行文件拆出。

典型任务是调整“Notify 释放之后什么时候进入 Recovery”或“取消后是否还能提交”：首轮仍须阅读 Execute 文件中 Notify、ReleaseCast、BeginRecovery、FinishCast 和行为提交之间的共享调用链。仅按 400 行切成新文件不会自动减少理解成本；若切分成跨文件类/接口，还会增加生命周期调用跳转。当前源代码没有架构交接材料给出该文件超过参考行数的具体例外理由，但静态 Review 也没有证明进一步拆分能隔离状态或缩小安全修改面。

GAS 侧文件呈相对窄的机制边界，例如 `SkillLabAbilitySystemResource.cpp` 把资源写入队列/回执集中在单文件；`SkillLabAbilitySystemComponent.cpp` 同时提供伤害入口和周期结果关联，但文件职责仍围绕 ASC 的战斗事实与转发。此处不因文件行数或可以想象的子模块而要求拆类。若以后某项需求反复只修改执行文件中的一个独立段落，且能证明无需读/改相邻 Cast 状态，则可重新核对拆分收益。

**判断**：约 400 行是查问理由的信号，不是违规阈值。此例不提出强制拆分，也不因“集中”泛称高耦合；决定点是当前可独立任务是否能缩短首轮阅读和回归范围，现有证据不能肯定。

## 值得保留的设计

- `SkillLabSkills` 与 `SkillLabGAS` 是独立模块，Skills 的 Runtime/Behavior/Ability 负责技能执行事实，GAS ASC/AttributeSet 负责资源和效果结算；`SkillLabSkills.Build.cs` 通过 `SkillLabGAS` 明确依赖。策划调整技能目标/效果内容不应直接要求表现层伪造结算。
- `RequestDeclaredAction()` 是 Lua 组合层实际使用的正式入口（Lua 绑定见 `SkillLabSkillBehaviorLua.cpp:38–44`）；`RequestSubmissionPoint()` 校验 Token/Point/Trigger 后才进入统一提交路径。这些是生产 API，虽也常被测试直接调用，不属于测试专用入口。
- 测试采用真实 Provider、ASC、Runtime、定时器和生产动作路径，并以小 fixture 复用装配代码；新测试能提供更窄的失败位置。不要为避免大测试而把生产规则复制到测试 helper，也不要删除仍独有的跨层保障。
- 多个运行类/职责按请求、执行、提交和资源结算拆分已有实现，结构分析应针对具体变化点，而非要求全局重构。

## 未验证事项与建议复核

本次没有确认任何测试运行通过、失败或在 Shipping 下的实际链接行为。关于无参 `ExecuteDeclaredAction()` 的编译边界，仅根据它在 `private:` 区域、friend Fixture 访问、声明/实现未受开发测试宏约束，以及 `SkillLabSkills` Runtime 模块和测试源码的组织作源级判断；没有据此声称它是公共 API 或断言具体 Target 下的符号导出状态。若实现方需要确认各 Target 的实际编译/链接结果，应核对对应构建产物。UE `.uasset`、Blueprint 图、Lua VM 第三方实现、真实输入/PIE、多人和综合测试全部异常路径未审。

建议按以下最小顺序交回项目负责人核对：先判断是否需要收窄无参测试适配在 Runtime 模块一般编译路径中的存在范围（其已确认是私有成员而非公共 API）；再列出 `CastGameplayEvent` 的断言场景与已有独立用例一一映射，决定哪些重复成本确实可移；最后只对反复发生的典型修改场景评估 555 行执行文件拆分是否能缩小首轮阅读集。所有结论与建议均不构成代码修改、测试删除或重构授权。
