#include "SkillLabSkillShortcutComponent.h"

#include "Components/InputComponent.h"
#include "SkillLabSkillInputComponent.h"

USkillLabSkillShortcutComponent::USkillLabSkillShortcutComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	// Only this demo loadout owns skill-to-key mappings. HUD and input consume it.
	FSkillLabSkillShortcut Fireball; Fireball.SkillId = TEXT("Skill_Fireball"); Fireball.Key = EKeys::One;
	FSkillLabSkillShortcut Area; Area.SkillId = TEXT("Skill_AreaBurst"); Area.Key = EKeys::Two;
	FSkillLabSkillShortcut Mend; Mend.SkillId = TEXT("Skill_Mend"); Mend.Key = EKeys::Three;
	FSkillLabSkillShortcut Burn; Burn.SkillId = TEXT("Skill_BurningBuff"); Burn.Key = EKeys::Four;
	Shortcuts = {Fireball, Area, Mend, Burn};
	ConfirmKey = EKeys::LeftMouseButton;
	CancelKey = EKeys::Escape;
}

TArray<FSkillLabSkillShortcut> USkillLabSkillShortcutComponent::GetShortcuts() const
{
	TArray<FSkillLabSkillShortcut> Result;
	TSet<FName> SeenSkills;
	TSet<FKey> SeenKeys;
	for (const FSkillLabSkillShortcut& Shortcut : Shortcuts)
	{
		if (Shortcut.SkillId.IsNone() || !Shortcut.Key.IsValid() || Shortcut.Key == ConfirmKey || Shortcut.Key == CancelKey
			|| SeenSkills.Contains(Shortcut.SkillId) || SeenKeys.Contains(Shortcut.Key)) continue;
		SeenSkills.Add(Shortcut.SkillId);
		SeenKeys.Add(Shortcut.Key);
		Result.Add(Shortcut);
	}
	return Result;
}

void USkillLabSkillShortcutComponent::BindInput(UInputComponent* Input, USkillLabSkillInputComponent* InSkillInput)
{
	UE_LOG(LogTemp, Log, TEXT("[SkillLabInput] BindInput owner=%s input=%s skillInput=%s existingInput=%s"),
		*GetNameSafe(GetOwner()), *GetNameSafe(Input), *GetNameSafe(InSkillInput), *GetNameSafe(BoundInput.Get()));
	if (!Input || BoundInput.Get() == Input)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SkillLabInput] BindInput skipped: input=%s existingInput=%s"),
			*GetNameSafe(Input), *GetNameSafe(BoundInput.Get()));
		return;
	}
	UnbindInput();
	SkillInput = InSkillInput;
	BoundInput = Input;
	const TArray<FSkillLabSkillShortcut> ValidShortcuts = GetShortcuts();
	if (ValidShortcuts.Num() != Shortcuts.Num())
	{
		UE_LOG(LogTemp, Warning, TEXT("[SkillLabInput] Invalid or duplicate skill shortcuts omitted from both input and HUD"));
	}
	for (const FSkillLabSkillShortcut& Shortcut : ValidShortcuts)
	{
		Input->BindKey(Shortcut.Key, IE_Pressed, this, &USkillLabSkillShortcutComponent::HandleShortcutPressed);
		UE_LOG(LogTemp, Log, TEXT("[SkillLabInput] Bound key=%s skill=%s"), *Shortcut.Key.ToString(), *Shortcut.SkillId.ToString());
	}
	if (ConfirmKey.IsValid()) Input->BindKey(ConfirmKey, IE_Pressed, this, &USkillLabSkillShortcutComponent::ConfirmTarget);
	if (CancelKey.IsValid() && CancelKey != ConfirmKey) Input->BindKey(CancelKey, IE_Pressed, this, &USkillLabSkillShortcutComponent::CancelTarget);
	UE_LOG(LogTemp, Log, TEXT("[SkillLabInput] Binding complete count=%d confirm=%s cancel=%s totalKeyBindings=%d"),
		ValidShortcuts.Num(), *ConfirmKey.ToString(), *CancelKey.ToString(), Input->KeyBindings.Num());
}

void USkillLabSkillShortcutComponent::UnbindInput()
{
	if (UInputComponent* Input = BoundInput.Get())
	{
		Input->KeyBindings.RemoveAll([this](const FInputKeyBinding& Binding)
		{
			return Binding.KeyDelegate.IsBoundToObject(this);
		});
	}
	BoundInput.Reset();
	SkillInput.Reset();
}

void USkillLabSkillShortcutComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindInput();
	Super::EndPlay(EndPlayReason);
}

void USkillLabSkillShortcutComponent::ActivateSlot(const int32 SlotIndex)
{
	const TArray<FSkillLabSkillShortcut> Slots = GetShortcuts();
	UE_LOG(LogTemp, Log, TEXT("[SkillLabInput] ActivateSlot index=%d slots=%d skillInput=%s"),
		SlotIndex, Slots.Num(), *GetNameSafe(SkillInput.Get()));
	if (SkillInput.IsValid() && Slots.IsValidIndex(SlotIndex))
	{
		UE_LOG(LogTemp, Log, TEXT("[SkillLabInput] BeginSkill skill=%s from slot=%d"), *Slots[SlotIndex].SkillId.ToString(), SlotIndex);
		SkillInput->BeginSkill(Slots[SlotIndex].SkillId);
	}
}

void USkillLabSkillShortcutComponent::HandleShortcutPressed(const FKey Key)
{
	const TArray<FSkillLabSkillShortcut> Slots = GetShortcuts();
	UE_LOG(LogTemp, Log, TEXT("[SkillLabInput] KeyPressed key=%s slots=%d skillInput=%s"),
		*Key.ToString(), Slots.Num(), *GetNameSafe(SkillInput.Get()));
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		if (Slots[Index].Key == Key) { ActivateSlot(Index); return; }
	}
	UE_LOG(LogTemp, Warning, TEXT("[SkillLabInput] KeyPressed had no matching shortcut key=%s"), *Key.ToString());
}

void USkillLabSkillShortcutComponent::ConfirmTarget()
{
	UE_LOG(LogTemp, Log, TEXT("[SkillLabInput] ConfirmPressed skillInput=%s"), *GetNameSafe(SkillInput.Get()));
	if (SkillInput.IsValid()) SkillInput->ConfirmSkill();
}

void USkillLabSkillShortcutComponent::CancelTarget()
{
	UE_LOG(LogTemp, Log, TEXT("[SkillLabInput] CancelPressed skillInput=%s"), *GetNameSafe(SkillInput.Get()));
	if (SkillInput.IsValid()) SkillInput->CancelSkill();
}
