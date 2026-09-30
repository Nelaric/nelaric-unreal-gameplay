// Copyright (c) 2026 Nelaric
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
		if (!InputSubsystem->HasMappingContext(Context))
		{
			FModifyContextOptions Options;
			Options.bIgnoreAllPressedKeysUntilRelease = true;
			InputSubsystem->AddMappingContext(Context, Mapping.Priority, Options);
			OwnedMappingContexts.Add(Context);
		}
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
	MappingSubsystem.Reset();
}

void UNelaricInputComponent::OnUnregister()
{
	RemoveInputMappings();
	Super::OnUnregister();
}
