#include "SkillLabSkillRuntimeComponent.h"
#include "SkillLabSkillBehavior.h"
#include "SkillLabSkillGameplayAbility.h"

USkillLabCastExecution* USkillLabSkillRuntimeComponent::FindMutableExecution(const int32 CastId) const
{
 const auto* Entry = Executions.Find(CastId);
 return Entry ? Entry->Get() : nullptr;
}
const USkillLabCastExecution* USkillLabSkillRuntimeComponent::FindExecutionByCastId(const int32 CastId) const
{
 return FindMutableExecution(CastId);
}
const USkillLabCastExecution* USkillLabSkillRuntimeComponent::FindExecution(const FSkillLabCastExecutionToken& Token) const
{
 if (Token.Runtime.Get() != this || Token.RuntimeGeneration != RuntimeGeneration) return nullptr;
 const auto* Execution = FindMutableExecution(Token.CastId);
 return Execution && Execution->Token == Token ? Execution : nullptr;
}
USkillLabCastExecution* USkillLabSkillRuntimeComponent::FindExecution(const USkillLabSkillBehavior* Behavior, const int32 CastId) const
{
 auto* Execution = FindMutableExecution(CastId);
 return Behavior && Behavior->Runtime == this && Behavior->RuntimeGeneration == RuntimeGeneration
  && Execution && Execution->ActiveBehavior == Behavior && IsExecutionCurrent(*Execution) ? Execution : nullptr;
}
bool USkillLabSkillRuntimeComponent::IsExecutionCurrent(const USkillLabCastExecution& Execution) const
{
 return FindExecution(Execution.Token) == &Execution;
}
const USkillLabCastExecution* USkillLabSkillRuntimeComponent::GetSingleExecution() const
{
 if (Executions.Num() != 1) return nullptr;
 for (const auto& Pair : Executions) return Pair.Value;
 return nullptr;
}
int32 USkillLabSkillRuntimeComponent::GetActiveCastId() const
{
 const auto* Execution = GetSingleExecution();
 return Execution ? Execution->Token.CastId : 0;
}
TArray<FSkillLabCastExecutionToken> USkillLabSkillRuntimeComponent::GetExecutionTokens() const
{
 TArray<FSkillLabCastExecutionToken> Tokens;
 for (const auto& Pair : Executions) if (Pair.Value) Tokens.Add(Pair.Value->Token);
 // 稳定诊断顺序；不以排序结果决定仲裁权。
 Tokens.Sort([](const auto& A, const auto& B) { return A.CastId < B.CastId; });
 return Tokens;
}
bool USkillLabSkillRuntimeComponent::AnySubmitting() const
{
 for (const auto& Pair : Executions) if (Pair.Value && Pair.Value->bSubmitting) return true;
 return false;
}
void USkillLabSkillRuntimeComponent::CancelCast(const FSkillLabCastExecutionToken& Token, const FName Reason)
{
 if (FindExecution(Token)) CancelExecution(*FindMutableExecution(Token.CastId), Reason);
}
bool USkillLabSkillRuntimeComponent::ForwardValidatedNotify(const FSkillLabCastExecutionToken& Token, const FGameplayTag EventTag)
{
 return FindExecution(Token) && ForwardValidatedNotify(Token.CastId, EventTag);
}
bool USkillLabSkillRuntimeComponent::ForwardValidatedNotifyStateEnd(const FSkillLabCastExecutionToken& Token, const FGameplayTag BeginEventTag)
{
 return FindExecution(Token) && ForwardValidatedNotifyStateEnd(Token.CastId, BeginEventTag);
}
