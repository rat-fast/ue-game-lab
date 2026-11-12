# Energy / MaxEnergy 写入审计

范围：需求第 8.1 节及相关 7–9 节，当前项目的正式资源路径。此报告不替代独立 Review；资源修复交 `VerificationHandoff.md` 的 R。下述运行证据来自本任务独立隔离副本，不代表后续执行层改动已重新验证。

## 已确认问题

1. **正式协调写入的钳制与 GAS base 不一致。** `Source/SkillLabGAS/Private/SkillLabAbilitySystemResource.cpp` 的 `ApplyResourceChange` 根据钳制值判断成功，却将原始数值交给 `ApplyModToAttribute`。隔离 ASC 初始 Energy/MaxEnergy 为 100/100，通过 `RequestResourceChange(false, Additive, 25)` 返回成功后，实际值为 100、base 为 125；`BeginEnergyEscrow(..., 2)` 拒绝，尽管实际和可用能量均为 100。Override 150 同样产生 base 150；MaxEnergy Override -10 得到 current 0、base -10。对 Energy 精确 Override 100 可恢复前一示例的 base 并允许预留。这是正式协调 API 的可复现缺陷，不要求存在第三方旁路；现有四技能未发现调用该回复入口的生产内容，不把夹具称为已发生的玩家流程。建议 R 在权威协调写入处处理 GAS base 与钳制后的目标一致性，并覆盖上限、下限及已有聚合时的计算；不能仅事后标记成功。需求影响：合法资源变化后，实际/可用值充足却无法预留，影响技能门控与 8.1 资源一致性。
2. **资源 GE 与聚合尚无生效前保护。** 隔离诊断先预留 80，再施加动态即时 Energy Override 1 的 GE：GE 接受，实际 Energy 变 1、预留仍为 80，可用为 0，`Unsupported=1` 是写后诊断而非拦截。另一个持续 MaxEnergy -90 GE 在预留 80 后接受，使 Energy 100、MaxEnergy 10、预留 80；已有 Energy -10 聚合时 actual 90/base 100，Begin 在应用前拒绝新增预留。上述 GE 是审计夹具，未证明当前生产内容配置了这些效果；它们证明现有 GAS 可应用路径缺少需求所需的保护，不能把任意原生旁路诊断描述为已保护。R 交接已覆盖合法 GE 的权威应用前检查、拒绝或协调以及聚合测试；具体允许的 GE 类型应在其实现范围核实，不扩为第三方任意代码安全承诺。

## 路径与约束

| 路径 | 可达性与结果 |
| --- | --- |
| 初始化 | `SkillLabCombatAttributeSet.cpp` 构造 100/100；正式角色由 `SkillLabCharacter.cpp` 创建 SkillLab ASC，`InitializeSkillLabAbilityActorInfo` 查找或创建 AttributeSet 并注册监听。角色 Blueprint 的 ASC CDO `default_starting_data=[]`；隔离 EnergyLedger 实测初始 actual 100。构造默认值发生于预留之前，不经协调写入。 |
| 首次费用 | 当前 `SkillLabSkillRuntimeSubmission.cpp` 在首次提交调用 Begin 预留，成功后调用 Resolve；`SkillLabAbilitySystemResource.cpp` 通过内部 `ApplyResourceChange` 扣费。预留检查 authority、可用值、base/current 和状态；后续点已有计费标记则不再预留。此前的隔离账本测试通过；当前多 Cast 调用调整后只作静态接线复核，未将旧测试冒称新版本运行。 |
| Reset | 当前 `SkillLabSkillRuntimeComponent.cpp::CompleteReset` 有未确认记录时跳过资源恢复并报告 `ResetBlockedByUnconfirmedSubmission`；否则将 Energy Override 到 MaxEnergy，经 `RequestResourceChange`。调用方未检查该返回值，若协调器拒绝或应用异常，未见独立 Reset 失败反馈；该条件及需求影响需 R/A 定向验证，不把静态疑点称为已复现。旧快照 RuntimeProbe 正常 Reset 恢复 Energy 100，不覆盖当前多 Cast 或未决 Reset。 |
| 属性钳制 | `PreAttributeChange` 限制当前值；`PostGameplayEffectExecute → ClampVitals` 仅在 Energy 越界时通过 SkillLab ASC 的协调入口回写。非 SkillLab ASC 才回退 `SetEnergy`，并非本角色的 ASC；受控入口返回值未检查。持续聚合变化不等价于执行 `PostGameplayEffectExecute`，不能用此钳制保证聚合不变量。 |
| 协调入口和重入 | `RequestResourceChange` 接受有限 Additive/Override；影响预留保护额时先拒绝，同步重入写入队列并在退出后重新校验。`EnergyLedger` 覆盖多笔预留、MaxEnergy 降低拒绝、重入排队及结算；越界写入缺陷见上。原生属性写入及通知监听只用于故障注入和异常诊断，不能证明应用前拦截。 |
| 项目正式效果 | `SkillLabEffectResolver` 仅分发 Damage、Healing、DamageOverTime；原生即时 GE 修改 IncomingDamage/IncomingHealing，动态持续/周期 GE 修改 IncomingDamage，不直接写 Energy。隔离诊断中正式伤害及治疗 GE 在预留 80 期间实际应用，actual/base=100/100、reserved=80，未触发资源旁路。现有配置的效果种类由 Lua 解析为有限枚举，未知值拒绝；未发现可配置任意资源 GE 类的项目技能入口。 |
| Ability 费用与二进制资产 | 运行时 Loadout 从定义授予原生 `USkillLabSkillGameplayAbility`；其 CDO Cost/Cooldown GE 均为 None。`/Game` 149 个资产实际加载；未枚举到独立 GE 资产。角色、GameMode、PlayerController Blueprint 图实际读取：仅构造入口及事件节点，没有资源写入/GE 应用节点；GameMode CDO 默认角色和控制器分别为项目 Blueprint，角色 ASC 默认初始化数组为空。Animation Blueprint 已读取普通 K2 节点；Control Rig 专用图未由 K2 节点枚举覆盖，不推断其完整逻辑。资产检查只覆盖当时快照及实际加载范围，不代表任意外部动态类。 |
| 两张地图 | 隔离 Editor 的 `LevelEditorSubsystem.load_level` 均成功，NavTest 18 个、Test_Box 9 个已放置 Actor，均无 ASC；两地图 WorldSettings 的 GameMode 覆盖为 None，沿 `DefaultEngine.ini` 的项目 GameMode。针对 Python 不暴露 Level Blueprint 属性的问题，独立 Editor C++ commandlet 加载两张 `UWorld`，对各自 PersistentLevel 调用 `GetLevelScriptBlueprint(true)` 均返回 `None`，没有已有脚本图、节点或由该图产生的直接动态引用可继续追踪；日志 `ResourceWriteAudit_20261112_LevelScriptRead.log` 第 1854–1856 行。该结论限于这两张加载的地图及其 Level Blueprint，不涵盖其他动态代码。 |

## 版本与证据

- 旧隔离快照与当前生产 Source 按路径及 SHA-256 比较：原 194 个文件中 29 个变化、无删除，当前新增 23 个、共 217 个；GAS 原有 18 个文件未变。对当前新增/变化代码按上述资源写入、GE 应用、费用与 Reset 调用定向检索；非相关代码未作完整 Review。Config/Content 192 个文件逐项比较，无新增、删除或修改。旧运行证据只适用于旧快照，当前调用链结论是静态核对。
- 原始日志：`Saved/Logs/ResourceWriteAudit_20261112_DiagnosticFinal.log` 的 `RESOURCE_WRITE_AUDIT` 记录；诊断测试 Success 表示观测执行成功，并非上述机制无缺陷。临时源码为 `C:/Users/Administrator/AppData/Local/Temp/SkillLabResourceWriteAudit_20261112/Source/SkillLabGAS/Private/Tests/SkillLabResourceWriteAuditDiagnostic.cpp`，仅存在于隔离副本。
- 编译日志 `ResourceWriteAudit_20261112_Build.log` 与 `ResourceWriteAudit_20261112_DiagnosticBuildFinal.log` 均 Succeeded；后者加入独立诊断源码，其 SHA-256 为 `3793007AC9E9DF79D251150DB709BDD9575870A14916C33282DEE4CF347F686A`。旧 Source 与 Config/Content 哈希清单分别为 `ResourceWriteAudit_20261112_SourceHashes.json`、`ResourceWriteAudit_20261112_ContentHashes.json`。
- 复现：UE 5.8 Editor 命令行以独占副本 `C:/Users/Administrator/AppData/Local/Temp/SkillLabResourceWriteAudit_20261112/SkillLab.uproject`，`-unattended -nop4 -NullRHI -nosplash -nosound`，执行 `-ExecCmds="Automation RunTests SkillLab.Audit.ResourceWriteBoundaries"`、`-TestExit="Automation Test Queue Empty"`。旧生产内容 Probe 使用同一副本的 `-game -SkillLabRuntimeProbe`，以 `ResourceWriteAudit_20261112_RuntimeProbe.log` 的 COMPLETE 结束；不是可见 PIE。
- `ResourceWriteAudit_20261112_AssetsFinal.json` 保存 AssetRegistry、实际加载、CDO、图节点及引用；`ResourceWriteAudit_20261112_AssetLoadFinal.log` 记录 UE Python 运行，退出码 0。地图补证 `ResourceWriteAudit_20261112_MapLoadFinal.log` 退出码 0，脚本仅在临时副本。
- Level Blueprint 补证：独占临时工程 Editor 模块额外加入 `SkillLabResourceMapAuditCommandlet.h/.cpp` 和 `BlueprintGraph` 构建依赖；构建 `ResourceWriteAudit_20261112_LevelScriptBuild.log` Succeeded，`UnrealEditor-Cmd.exe <独占副本>/SkillLab.uproject -run=SkillLabResourceMapAudit -unattended -nop4 -NullRHI -nosplash -nosound` 退出 0。命令使用 `GetLevelScriptBlueprint(true)`，不创建、编译或保存资产；若存在脚本则列举所有图、节点、函数目标及引脚默认引用。两个地图返回 None，故没有需要进一步枚举的直接图引用。诊断 C++ SHA-256 `8BAEB4CDC0569916AA113AC757B24F348FABCEDDAEBB2C9DBED42D3D21F40728`，头文件 `55B869130397FE19A8AC8236878476CEDD51DB095381C4048AE6DB22A9CB24EC`，临时 Build.cs `054A0575593749126C57CA52EDF94C38170E1074618191ECE35D74CD74B1A4D6`。运行前 192 个 Config/Content 文件在清单、生产与隔离副本间逐项一致；运行后两地图三方哈希仍一致。

- `ResourceWriteAudit_20261112_Automation.log` 中 `EnergyLedger`、`EnergySettlementHistory`、`DamageCalculation`、`CastGameplayEvent`、`UnconfirmedSubmission` 均 Success；该次额外诊断因预期错误日志匹配问题为 Fail，随后单独诊断修正匹配并 Success。启动日志有既有错误，不称整份日志无错误。

未验证：非 K2 的 Control Rig 图、未来引入的其他资源 GE/计算、当前执行线修改后的定向运行及可见 PIE。R 修改后针对受影响的协调写入、GE/聚合、Reset 返回结果及相关资产增量复核；不重复未受影响资产的全部审计。
