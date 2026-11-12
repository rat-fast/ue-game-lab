#pragma once
#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"

// 两项独立提交测试共用 World 的创建/销毁顺序，避免复制全局编辑器 World。
inline UWorld* CreateSkillLabSubmissionTestWorld(const bool bPhysics = false)
{
	const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
		.CreatePhysicsScene(bPhysics).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	if (World)
	{
		if (GEngine) GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		World->InitializeActorsForPlay(FURL());
		World->BeginPlay();
		// 无 GameMode 的测试世界仍须分发 BeginPlay，供真实 Actor/Projectile 使用。
		World->GetWorldSettings()->NotifyBeginPlay();
	}
	return World;
}
#endif
