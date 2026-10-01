// Copyright (c) 2026 Nelaric Contributors
#include "Input/NelaricInputConfig.h"
const UInputAction* UNelaricInputConfig::FindNativeInputActionForTag(const FGameplayTag& InputTag) const
{
	if (InputTag.IsValid())
	{
		for (const FNelaricInputAction& Action : NativeInputActions)
		{
			if (Action.InputAction && Action.InputTag == InputTag)
			{
				return Action.InputAction;
			}
		}
	}
	return nullptr;
}

const FNelaricInputMapping* UNelaricInputConfig::FindInputMappingForTag(const FGameplayTag& MappingTag) const
{
	if (MappingTag.IsValid())
	{
		for (const FNelaricInputMapping& Mapping : MappingContexts)
		{
			if (Mapping.MappingContext && Mapping.MappingTag == MappingTag)
			{
				return &Mapping;
			}
		}
	}
	return nullptr;
}
