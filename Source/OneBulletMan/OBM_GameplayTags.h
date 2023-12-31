// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NativeGameplayTags.h"

namespace OBM_GameplayTags
{
	//ONEBULLETMAN_API	FGameplayTag FindTagByString(const FString& TagString, bool bMatchPartialString = false);

	// Declare all of the custom native tags that OneBulletMan will use
	ONEBULLETMAN_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_ActivateFail_IsDead);
	ONEBULLETMAN_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_ActivateFail_Cooldown);
	ONEBULLETMAN_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_ActivateFail_Cost);
	ONEBULLETMAN_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_ActivateFail_TagsBlocked);
	ONEBULLETMAN_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_ActivateFail_TagsMissing);
	ONEBULLETMAN_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_ActivateFail_Networking);
	ONEBULLETMAN_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_ActivateFail_ActivationGroup);

	ONEBULLETMAN_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Behavior_SurvivesDeath);

	ONEBULLETMAN_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Move);
	ONEBULLETMAN_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Look_Mouse);
	ONEBULLETMAN_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Look_Stick);
	ONEBULLETMAN_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Crouch);
	ONEBULLETMAN_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_AutoRun);

	ONEBULLETMAN_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Weapon_Fire);
	ONEBULLETMAN_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Weapon_Reload);

	ONEBULLETMAN_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InitState_Spawned);
	ONEBULLETMAN_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InitState_DataAvailable);
	ONEBULLETMAN_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InitState_DataInitialized);
	ONEBULLETMAN_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InitState_GameplayReady);

	ONEBULLETMAN_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayEvent_Death);
	ONEBULLETMAN_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayEvent_Reset);
	ONEBULLETMAN_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayEvent_RequestReset);

	ONEBULLETMAN_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Damage);
	ONEBULLETMAN_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Heal);

	ONEBULLETMAN_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cheat_GodMode);
	ONEBULLETMAN_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cheat_UnlimitedHealth);

	ONEBULLETMAN_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Crouching);
	ONEBULLETMAN_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_AutoRunning);
	ONEBULLETMAN_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Death);
	ONEBULLETMAN_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Death_Dying);
	ONEBULLETMAN_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Death_Dead);

	// These are mappings from MovementMode enums to GameplayTags associated with those enums (below)
	ONEBULLETMAN_API	extern const TMap<uint8, FGameplayTag> MovementModeTagMap;
	ONEBULLETMAN_API	extern const TMap<uint8, FGameplayTag> CustomMovementModeTagMap;

	ONEBULLETMAN_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Movement_Mode_Walking);
	ONEBULLETMAN_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Movement_Mode_NavWalking);
	ONEBULLETMAN_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Movement_Mode_Falling);
	ONEBULLETMAN_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Movement_Mode_Swimming);
	ONEBULLETMAN_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Movement_Mode_Flying);

};
