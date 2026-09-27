// Copyright (c) 2026 Nelaric

#include "Pawn/NelaricPawnInitStateComponent.h"

UNelaricPawnInitStateComponent::UNelaricPawnInitStateComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UNelaricPawnInitStateComponent::IsInitApplicable() const
{
	return false;
}

bool UNelaricPawnInitStateComponent::IsRequiredForPawnReady() const
{
	return false;
}

void UNelaricPawnInitStateComponent::GatherInitDependencies(TArray<Nelaric::FInitDependency>&) const
{
}

bool UNelaricPawnInitStateComponent::TryChangeInitState()
{
	return false;
}
