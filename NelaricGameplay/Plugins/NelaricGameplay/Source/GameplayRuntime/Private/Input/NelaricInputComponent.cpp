// Copyright (c) 2026 Nelaric Contributors
#include "Input/NelaricInputComponent.h"

#include "EnhancedInputSubsystems.h"
#include "UserSettings/EnhancedInputUserSettings.h"
void UNelaricInputComponent::RemoveBinds(TArray<uint32>& BindHandles)
{
	for (uint32 Handle : BindHandles)
	{
		RemoveBindingByHandle(Handle);
	}
	BindHandles.Reset();
}

bool UNelaricInputComponent::AddInputMappings(const UNelaricInputConfig* InputConfig,
                                              UEnhancedInputLocalPlayerSubsystem* InputSubsystem)
{
	RemoveInputMappings();
	if (!InputConfig || !InputSubsystem)
	{
		return false;
	}
	ActiveInputConfig = InputConfig;
	MappingSubsystem = InputSubsystem;
	for (const FNelaricInputMapping& Mapping : InputConfig->MappingContexts)
	{
		UInputMappingContext* Context = Mapping.MappingContext;
		if (!Context)
		{
			continue;
		}
		if (Mapping.bRegisterWithSettings)
		{
			if (UEnhancedInputUserSettings* Settings = InputSubsystem->GetUserSettings())
			{
				if (!Settings->IsMappingContextRegistered(Context))
				{
					Settings->RegisterInputMappingContext(Context);
				}
			}
		}
		if (Mapping.bActivateOnStart)
		{
			ActivateMapping(Mapping, InputSubsystem);
		}
	}
	return true;
}

bool UNelaricInputComponent::AddInputMappingByTag(const FGameplayTag& MappingTag)
{
	UEnhancedInputLocalPlayerSubsystem* Subsystem = MappingSubsystem.Get();
	const FNelaricInputMapping* Mapping =
	    ActiveInputConfig ? ActiveInputConfig->FindInputMappingForTag(MappingTag) : nullptr;
	return Subsystem && Mapping && ActivateMapping(*Mapping, Subsystem);
}

bool UNelaricInputComponent::RemoveInputMappingByTag(const FGameplayTag& MappingTag)
{
	UEnhancedInputLocalPlayerSubsystem* Subsystem = MappingSubsystem.Get();
	const FNelaricInputMapping* Mapping =
	    ActiveInputConfig ? ActiveInputConfig->FindInputMappingForTag(MappingTag) : nullptr;
	if (!Subsystem || !Mapping)
	{
		return false;
	}
	UInputMappingContext* Context = Mapping->MappingContext;
	for (int32 Index = 0; Index < OwnedMappingContexts.Num(); ++Index)
	{
		if (OwnedMappingContexts[Index] == Context)
		{
			const bool bWasActive = Subsystem->HasMappingContext(Context);
			if (bWasActive)
			{
				Subsystem->RemoveMappingContext(Context);
			}
			OwnedMappingContexts.RemoveAtSwap(Index);
			return bWasActive;
		}
	}
	return false;
}

bool UNelaricInputComponent::ActivateMapping(const FNelaricInputMapping& Mapping,
                                             UEnhancedInputLocalPlayerSubsystem* InputSubsystem)
{
	UInputMappingContext* Context = Mapping.MappingContext;
	if (!Context)
	{
		return false;
	}
	if (!InputSubsystem->HasMappingContext(Context))
	{
		FModifyContextOptions Options;
		Options.bIgnoreAllPressedKeysUntilRelease = true;
		InputSubsystem->AddMappingContext(Context, Mapping.Priority, Options);
		OwnedMappingContexts.AddUnique(Context);
	}
	return true;
}

void UNelaricInputComponent::RemoveInputMappings()
{
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = MappingSubsystem.Get())
	{
		for (UInputMappingContext* Context : OwnedMappingContexts)
		{
			Subsystem->RemoveMappingContext(Context);
		}
	}
	OwnedMappingContexts.Reset();
	ActiveInputConfig = nullptr;
	MappingSubsystem.Reset();
}

void UNelaricInputComponent::OnUnregister()
{
	RemoveInputMappings();
	Super::OnUnregister();
}
