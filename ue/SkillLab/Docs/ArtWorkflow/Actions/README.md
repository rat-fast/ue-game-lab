# 动作资源

每个 `AnimSequence` 资产使用一个目录，目录名来自资产对象名，不来自技能名。一个资产可以被多个技能引用，因此技能关系不得写入动作目录名。

```text
Actions/
├─ Index.md
└─ MM_Attack_01/
   ├─ page-0000.png
   ├─ overview.json
   ├─ observation.md
   └─ source-to-montage.json
```

动作资产目录直接保存当前有效总览。重新生成时，用新分页图和 `overview.json` 更新同目录结果，并清理旧结果中多余的分页图；不在正式归档中保留历史 generation 目录。`generationId` 保留在 `overview.json` 与 Saved staging 中用于追溯。

`Index.md` 用简短文字索引各动作可见特征，并链接到该动作的观察记录和总览图。`observation.md` 除按时间描述动作外，还应明确列出身体/肢体峰值和颜色、白光等视觉线索峰值，附源帧或帧段及总览页证据；峰值是配置参考，不等于 Notify 或业务事件。无法判定亮点性质时保留不确定性。`source-to-montage.json` 只有建立实际播放映射后才创建。这些文件在对应工作完成前可以不存在；不能创建空占位文件冒充完成。
