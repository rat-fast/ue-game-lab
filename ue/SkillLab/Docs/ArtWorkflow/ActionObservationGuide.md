# AI 动作观察指南

## 输入

从 `Exports/<batch>/<item>/manifest.json` 读取资产身份、源帧率、帧时间、分页图、输入指纹和 `playbackMapping`。只消费 `status=complete` 的单项；批次 `incomplete` 时跳过失败项。

## 输出

观察记录写入 `Observations/<动作名>.observation.json`，并可附带同名 Markdown 说明。记录必须引用：

- `asset`
- `generationId`
- `inputPreviewFingerprintSha256`
- `manifest`
- `sourceFrames`、`sourceSeconds`
- `page`、`row`、`column`
- `basis`
- `uncertainty`

## 边界

观察只能描述画面中可复查的姿势、阶段和候选时点。它不能直接断言接触、命中、伤害、技能完成或 Notify 已触发。若 `playbackMapping` 仍为 `sourceOnly_notEstablished`，不得把源 Sequence 秒数直接写入 Montage。

## 后续配置

Notify 配置请求应另行引用观察记录，并声明目标持久 `UAnimMontage`、事件 Tag、事件顺序和时间映射。C++ Editor 工具负责写入、保存、重载和回读；Lua 只处理运行时技能行为。
