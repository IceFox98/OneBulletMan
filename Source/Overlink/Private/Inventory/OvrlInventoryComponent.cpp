
#include "Inventory/OvrlInventoryComponent.h"

#include "Inventory/OvrlItemInstance.h"
#include "Inventory/OvrlItemDefinition.h"
#include "Inventory/OvrlItemFragment_EquippableItem.h"
#include "Equipment/OvrlEquipmentInstance.h"
#include "Equipment/OvrlEquipmentDefinition.h"

#include "AbilitySystem/OvrlAbilitySystemComponent.h"
#include "AbilitySystem/OvrlAbilitySet.h"

#include "AbilitySystemGlobals.h"

UOvrlInventoryComponent::UOvrlInventoryComponent()
{
}

UOvrlItemInstance* UOvrlInventoryComponent::AddItemDefinition(TSubclassOf<UOvrlItemDefinition> ItemDef, int32 StackCount)
{
	UOvrlItemInstance* ItemInstance = nullptr;

	ItemInstance = NewObject<UOvrlItemInstance>(GetOwner());
	ItemInstance->SetItemDef(ItemDef);

	// Instantiate the item fragments
	for (UOvrlItemFragment* Fragment : GetDefault<UOvrlItemDefinition>(ItemDef)->Fragments)
	{
		if (Fragment)
		{
			Fragment->OnInstanceCreated(ItemInstance);
		}
	}

	// Spawn Item if equippable
	if (const UOvrlItemFragment_EquippableItem* EquipInfo = ItemInstance->FindFragmentByClass<UOvrlItemFragment_EquippableItem>())
	{
		TSubclassOf<UOvrlEquipmentDefinition> EquipDefClass = EquipInfo->EquipmentDefinition;
		if (EquipDefClass)
		{
			const UOvrlEquipmentDefinition* EquipmentDef = GetDefault<UOvrlEquipmentDefinition>(EquipDefClass);

			AOvrlEquipmentInstance* EquippedItem = GetWorld()->SpawnActor<AOvrlEquipmentInstance>(EquipmentDef->InstanceType);
			EquippedItem->EquipmentDefinitionClass = EquipDefClass;
			EquippedItem->AssociatedItem = ItemInstance;
			EquippedItem->SetOwner(GetOwner());
			EquippedItem->SetInstigator(Cast<APawn>(GetOwner()));

			EquippedItems.Emplace(EquippedItem);
		}
	}

	SetActiveSlotIndex(EquippedItems.Num() - 1);

	return ItemInstance;
}

UOvrlAbilitySystemComponent* UOvrlInventoryComponent::GetAbilitySystemComponent() const
{
	return Cast<UOvrlAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()));
}

void UOvrlInventoryComponent::SetActiveSlotIndex(int32 NewIndex)
{
	UnequipItemInSlot();

	SelectedIndex = NewIndex;

	EquipItemInSlot();
}

void UOvrlInventoryComponent::EquipItemInSlot()
{
	// You can equip a new Item only if there's no current equipped item.
	// Be sure to call UnequipCurrentItem first
	if (!SelectedItem && EquippedItems.IsValidIndex(SelectedIndex))
	{
		AOvrlEquipmentInstance* EquipInstance = EquippedItems[SelectedIndex];
		EquipInstance->OnEquipped();

		const UOvrlEquipmentDefinition* EquipmentDef = GetDefault<UOvrlEquipmentDefinition>(EquipInstance->EquipmentDefinitionClass);

		if (UOvrlAbilitySystemComponent* ASC = GetAbilitySystemComponent())
		{
			// When the item is equipped, we give all its abilities/effects/attributes to player's ASC
			for (TObjectPtr<const UOvrlAbilitySet> AbilitySet : EquipmentDef->AbilitySetsToGrant)
			{
				AbilitySet->GiveToAbilitySystem(ASC, /*inout*/ &EquipInstance->GrantedHandles, EquipInstance);
			}
		}

		SelectedItem = EquipInstance;
	}
}

void UOvrlInventoryComponent::UnequipItemInSlot()
{
	if (SelectedItem)
	{
		SelectedItem->OnUnequipped();

		if (UOvrlAbilitySystemComponent* ASC = GetAbilitySystemComponent())
		{
			// When unequip the item, remove all given abilities/effects/attributes from player's ASC
			SelectedItem->GrantedHandles.TakeFromAbilitySystem(ASC);
		}

		SelectedItem = nullptr;
	}
}
