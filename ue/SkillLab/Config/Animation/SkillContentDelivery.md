# 四技能持久 Montage 与 Notify 配置交付

四项正式持久 Montage 已创建、保存并绑定；最终 Notify 与前后摇采用用户本轮明确提供的测试配置，覆盖此前候选建议。资产保存、重载、独立进程回读及实际 Provider reload 已通过。未启动游戏或 PIE，游戏内时序待用户验证。

| 技能 | 源动作 | 持久 Montage | Notify源帧 | Notify / 前摇（秒） | 后摇（秒） |
|---|---|---|---:|---:|---:|
| Skill_Fireball | AS_FireballCast | /Game/SkillLab/Animations/AM_Skill_Fireball.AM_Skill_Fireball | f11 | 0.366667 | 0.200000 |
| Skill_AreaBurst | MM_ChargedAttack | /Game/SkillLab/Animations/AM_Skill_AreaBurst.AM_Skill_AreaBurst | f36 | 1.200000 | 0.633333 |
| Skill_Mend | MM_Attack_01 | /Game/SkillLab/Animations/AM_Skill_Mend.AM_Skill_Mend | f15 | 0.500000 | 0.500000 |
| Skill_BurningBuff | MM_Attack_02 | /Game/SkillLab/Animations/AM_Skill_BurningBuff.AM_Skill_BurningBuff | f16 | 0.533333 | 0.466667 |

帧索引从0开始，源动画30fps；真实Montage均为完整单段、正向1倍速、一次循环，源时间与Montage时间为1:1。秒数按用户表格保留6位。

## 本轮取值依据

- Fireball：f11是主闪光结束后的候选边界，测试“亮光先提醒，再发射”。后摇仍从投射物结果返回后开始计时，再停顿0.20秒，不按动画尾段推算。
- AreaBurst：f36时冲步出拳已经充分展开，测试比冲步起点更明确的出拳时点。
- Mend：f15取水平直拳充分伸展区间的中间位置，测试拳到位后生效。
- BurningBuff：f16取向上挥臂接近最高姿态的时点，测试效果与上挥动作对齐。

2、3、4的后摇候选值覆盖释放点到源动画末帧的剩余时间。运行时仍从技能行为完成后开始计时，本轮先按行为很快完成来测试；并未把后摇实现改成由动画剩余时长驱动。这些是用户指定的测试候选，不表示游戏内体验已经验收。

## 配置与验证

每个Montage仅一个`SkillLab.Event.Cast`瞬时Notify，稳定ID为`<SkillId>.release.01`。四个技能定义移除`castAnimation`，改为`castMontage`，使用`releaseTrigger=GameplayEvent`及匹配的Tag；`releaseEventTimeout=3.0`沿用原默认故障超时，不是正常释放时间。四个单技能Lua保持原样。

四项资产使用`SK_Mannequin`、`DefaultSlot`、`Default@0->None`。创建时BlendIn/BlendOut沿用0.08/0.12秒；Notify Patch没有改变Skeleton、Slot、Section、Segment、RateScale、Blend或源Sequence。源时长分别约0.666667、1.833333、1、1秒。

各技能`Config/Animation/<SkillId>/`保存Patch、写入报告、独立进程回读、结构报告、只读Spec及源manifest摘要；对应动作目录保存`source-to-montage.json`。四项写入报告`saved/reloaded/valid/diffValid`均为true；独立回读valid=true，每项只有一个正确ID/Tag/时间，没有重复项。只读报告的saved/reloaded/diffValid=false表示仅读取；保存与重载以写入报告为准。写入报告时间保留6位，只读文本保留3位。

实际`SkillDefinitions.json`通过现有`SkillReloadDefinitions`入口成功装载；独立Python Commandlet同时确认四项资产均为持久`UAnimMontage`，结构与绑定一致。证据见`definition-validation-report.json`与`Saved/Logs/ContentConfigurationDefinitions.log`。Python仅用于临时调用和核对现有UE能力，不是游戏运行依赖，未修改仓库程序或工具。

## 待游戏内观察

重点观察Fireball是否形成亮光提醒后发射，Mend与BurningBuff是否在拳/手臂到位时才生效；同时核对每次施法只请求一次事件、后摇计时、完成/取消，以及旧Cast/取消后迟到通知隔离与缺Notify时3秒超时表现。本轮未启动游戏或PIE。

机器可读汇总：`skill-content-delivery-report.json`。
