# SkillLab Code Architecture

## Project Role

SkillLab 是用于学习和验证 UE 游戏研发流程的最小项目。当前重点是理解 C++、Blueprint、配置、资产和 UE 运行时系统之间的职责分配。

## Module Map

### Gameplay Framework

GameMode、PlayerController、Character 和 CharacterMovementComponent 组成基础运行对象关系。

### Input

Enhanced Input 资产描述输入，PlayerController 接收并解释鼠标操作。

### Camera

CameraRigComponent 管理 SpringArm 和 Camera 的距离、角度、旋转及缩放。

### Navigation

NavigationSystem 使用关卡 NavMesh 计算地面路径，CharacterMovementComponent 执行移动。

### Level And Debug

测试地图提供地面、墙体、障碍、PlayerStart、NavMeshBoundsVolume 和运行时调试反馈。

### Character Presentation

负责 Skeletal Mesh、骨骼、材质和动画。当前仍是待接入的表现层。

## Current Runtime Flow

```text
右键点击
→ SkillLabPlayerController
→ 鼠标射线
→ NavigationSystem 路径查询
→ 绿色/红色调试标记
→ 有效路径时提交移动请求
→ CharacterMovementComponent 执行移动
```

右键拖动和滚轮输入走 Camera 模块，不进入导航移动链。

## Source Locations

- C++：`Source/SkillLab/`
- 配置：`Config/`
- UE 资产：`Content/SkillLab/`
- 项目文档：`Docs/`

## Evolution Note

以上是当前小型原型的职责分配。新功能如果引入多单位、多种移动方式、战斗、技能或可复用能力，应根据新的运行生命周期和复用需求重新划分模块，不要机械地把逻辑继续堆进 Character 或 PlayerController。
