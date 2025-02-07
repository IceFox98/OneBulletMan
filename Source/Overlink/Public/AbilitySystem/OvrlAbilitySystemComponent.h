// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "OvrlAbilitySystemComponent.generated.h"

/**
 * 
 */
UCLASS()
class OVERLINK_API UOvrlAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()
	
public:

	void AbilityInputTagPressed(const FGameplayTag& InputTag);

	void AbilityInputTagReleased(const FGameplayTag& InputTag);

protected:

	// Handles to abilities that had their input pressed this frame.
	TArray<FGameplayAbilitySpecHandle> InputPressedSpecHandles;

	// Handles to abilities that had their input released this frame.
	TArray<FGameplayAbilitySpecHandle> InputReleasedSpecHandles;

	// Handles to abilities that have their input held.
	TArray<FGameplayAbilitySpecHandle> InputHeldSpecHandles;
};
