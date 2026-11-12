#include "SkillLabSkillAnimationComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimNotifyQueue.h"
#include "Animation/ActiveMontageInstanceScope.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "SkillLabSkillDefinitionProvider.h"
#include "SkillLabSkillRuntimeComponent.h"
#include "SkillLabSkillTypes.h"

USkillLabSkillAnimationComponent::USkillLabSkillAnimationComponent() { PrimaryComponentTick.bCanEverTick = false; }
void USkillLabSkillAnimationComponent::BeginPlay()
{
 Super::BeginPlay();
 SkillRuntime = GetOwner() ? GetOwner()->FindComponentByClass<USkillLabSkillRuntimeComponent>() : nullptr;
}
void USkillLabSkillAnimationComponent::EndPlay(const EEndPlayReason::Type Reason)
{
 StopAllCastActions(); SkillRuntime = nullptr;
 Super::EndPlay(Reason);
}
bool USkillLabSkillAnimationComponent::PlayCastAction(const int32 CastId, const FName SkillId)
{
 const auto* E = SkillRuntime ? SkillRuntime->FindExecutionByCastId(CastId) : nullptr;
 return E && PlayCastAction(E->Token, SkillId);
}
bool USkillLabSkillAnimationComponent::PlayCastAction(const FSkillLabCastExecutionToken& Token, const FName SkillId)
{
 FSkillLabSkillResult Result; Result.CastId = Token.CastId; Result.CastToken = Token; Result.SkillId = SkillId;
 return PlayCastMontage(Result);
}
void USkillLabSkillAnimationComponent::StopCastAction(const int32 CastId)
{
 // 兼容命令只停止绑定当前 Runtime 的同号记录，不反查替换 Runtime 的新 Cast。
 TArray<FSkillLabCastExecutionToken> Tokens;
 for (const auto& P : Playbacks) if (P.Token.Runtime == SkillRuntime && P.Token.CastId == CastId) Tokens.Add(P.Token);
 for (const auto& Token : Tokens) StopCastMontage(Token);
}
void USkillLabSkillAnimationComponent::StopCastAction(const FSkillLabCastExecutionToken& Token) { StopCastMontage(Token); }
void USkillLabSkillAnimationComponent::StopAllCastActions()
{
 while (!Playbacks.IsEmpty()) StopCastMontage(Playbacks.Last().Token);
}
bool USkillLabSkillAnimationComponent::PlayCastMontage(const FSkillLabSkillResult& Result)
{
 const auto Token = Result.CastToken;
 if (!Token.IsValid() || !Token.Runtime->FindExecution(Token)) return false;
 for (const auto& P : Playbacks) if (P.Token == Token) return true;
 const auto* E = Token.Runtime->FindExecution(Token);
 if (!E || E->ActiveDefinition.SkillId != Result.SkillId) return false;
 FSkillLabCastAnimationPlayback P; P.Token = Token;
 // 绑定运行快照，热重载目录不能改变已经受理的动作身份或资源。
 P.Montage = E->ActiveDefinition.CastMontage.LoadSynchronous(); P.NotifyAnimation = P.Montage;
 if (!P.Montage)
 {
  if (auto* Sequence = E->ActiveDefinition.CastAnimation.LoadSynchronous())
  {
   P.NotifyAnimation = Sequence;
   P.Montage = UAnimMontage::CreateSlotAnimationAsDynamicMontage(Sequence, TEXT("DefaultSlot"), 0.08f, 0.12f, 1.0f);
  }
 }
 auto* Mesh = FindMesh(); P.AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
 if (!P.Montage || !P.AnimInstance.IsValid())
 {
  UE_LOG(LogTemp, Warning, TEXT("[SkillLabAnimation] Cast=%d Skill=%s missing montage or AnimInstance"), Token.CastId, *Result.SkillId.ToString());
  return false;
 }
 // 不主动停止其他 group；同 group 的资源替换仍由引擎处理，不等于业务取消。
 if (P.AnimInstance->Montage_Play(P.Montage, 1.0f, EMontagePlayReturnType::MontageLength, 0.0f, false) <= 0.0f) return false;
 auto* Instance = P.AnimInstance->GetActiveInstanceForMontage(P.Montage);
 if (!Instance) return false;
 P.InstanceId = Instance->GetInstanceID();
 Playbacks.Add(P);
 UE_LOG(LogTemp, Log, TEXT("[SkillLabAnimation] Cast=%d Generation=%llu Montage=%s Instance=%d Group=%s"),
  Token.CastId, Token.RuntimeGeneration, *P.Montage->GetName(), P.InstanceId, *P.Montage->GetGroupName().ToString());
 return true;
}
bool USkillLabSkillAnimationComponent::ForwardCastEvent(const int32 CastId, const FGameplayTag EventTag)
{
 const FSkillLabCastAnimationPlayback* Match = nullptr;
 for (const auto& P : Playbacks)
 {
  if (P.Token.CastId != CastId) continue;
  if (Match) return false;
  Match = &P;
 }
 if (!Match || !Match->Token.Runtime.IsValid()) return false;
 return Match->Token.Runtime->ForwardValidatedNotify(Match->Token, EventTag);
}
bool USkillLabSkillAnimationComponent::ForwardAnimNotify(UAnimSequenceBase* Animation, const FGameplayTag EventTag,
 const FAnimNotifyEventReference& Reference, const bool bStateEnd, USkeletalMeshComponent* NotifyMesh)
{
 const auto* Context = Reference.GetContextData<UE::Anim::FAnimNotifyMontageInstanceContext>();
 if (!Animation || !Context) return false;
 for (const auto& P : Playbacks)
 {
  if (P.InstanceId != Context->MontageInstanceID || !P.AnimInstance.IsValid()
   || (Animation != P.Montage && Animation != P.NotifyAnimation) || !P.Token.Runtime.IsValid()) continue;
  const auto* Instance = P.AnimInstance->GetMontageInstanceForID(P.InstanceId);
  if (!Instance || !Instance->IsPlaying() || Instance->Montage != P.Montage) continue;
  // 同一 Notify 还须归属实际 Mesh 的 AnimInstance，不能跨角色借相同 InstanceId。
  if (NotifyMesh && NotifyMesh != P.AnimInstance->GetSkelMeshComponent()) continue;
  const auto Token = P.Token;
  return bStateEnd ? Token.Runtime->ForwardValidatedNotifyStateEnd(Token, EventTag) : Token.Runtime->ForwardValidatedNotify(Token, EventTag);
 }
 return false;
}
void USkillLabSkillAnimationComponent::StopCastMontage(const FSkillLabCastExecutionToken& Token)
{
 const int32 Index = Playbacks.IndexOfByPredicate([&](const auto& P) { return P.Token == Token; });
 if (Index == INDEX_NONE) return;
 const auto Playback = Playbacks[Index]; Playbacks.RemoveAt(Index);
 if (Playback.AnimInstance.IsValid())
  if (auto* Instance = Playback.AnimInstance->GetMontageInstanceForID(Playback.InstanceId); Instance && Instance->Montage == Playback.Montage)
   Instance->Stop(FAlphaBlend(0.12f));
}

USkeletalMeshComponent* USkillLabSkillAnimationComponent::FindMesh() const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}
	TArray<USkeletalMeshComponent*> Meshes;
	Owner->GetComponents<USkeletalMeshComponent>(Meshes);
	return Meshes.Num() > 0 ? Meshes[0] : nullptr;
}
