// Copyright (c) 2026 Nelaric Contributors
#include "Input/NelaricInputComponent.h"

#include "EnhancedInputSubsystems.h"
#include "UserSettings/EnhancedInputUserSettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogNelaricInput, Log, All);
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
			UE_LOG(LogNelaricInput, Error, TEXT("Input config %s on %s contains a null mapping context (tag=%s)."),
			       *GetNameSafe(InputConfig), *GetName(), *Mapping.MappingTag.ToString());
			continue;
		}
		if (Mapping.bRegisterWithSettings)
		{
			if (UEnhancedInputUserSettings* Settings = InputSubsystem->GetUserSettings())
			{
				if (!Settings->IsMappingContextRegistered(Context))
				{
					if (!Settings->RegisterInputMappingContext(Context))
					{
						UE_LOG(LogNelaricInput, Error,
						       TEXT("Input mapping settings registration failed on %s (tag=%s)."), *GetName(),
						       *Mapping.MappingTag.ToString());
					}
				}
			}
			else
			{
				UE_LOG(LogNelaricInput, Error,
				       TEXT("Cannot register input mapping settings on %s (tag=%s): user settings are unavailable."),
				       *GetName(), *Mapping.MappingTag.ToString());
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
	if (!Subsystem || !Mapping)
	{
		UE_LOG(LogNelaricInput, Error, TEXT("Cannot activate input mapping on %s: tag=%s config=%s subsystem=%s."),
		       *GetName(), *MappingTag.ToString(), *GetNameSafe(ActiveInputConfig), *GetNameSafe(Subsystem));
		return false;
	}
	return ActivateMapping(*Mapping, Subsystem);
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
		UE_LOG(LogNelaricInput, Error, TEXT("Cannot activate input mapping on %s (tag=%s): mapping context is null."),
		       *GetName(), *Mapping.MappingTag.ToString());
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

void UNelaricInputComponent::ReportNativeBindingFailure(const UNelaricInputConfig* InputConfig,
                                                        const FGameplayTag& InputTag, ETriggerEvent TriggerEvent) const
{
	UE_LOG(LogNelaricInput, Error,
	       TEXT("Cannot bind native action on %s: config=%s tag=%s trigger=%d; check the action and callback target."),
	       *GetName(), *GetNameSafe(InputConfig), *InputTag.ToString(), static_cast<int32>(TriggerEvent));
}
