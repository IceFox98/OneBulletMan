// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"

#include "CharacterBase.generated.h"

class UHealthComponent;
class UOBM_AbilitySystemComponent;
class UOBM_AbilitySet;

UCLASS()
class ONEBULLETMAN_API ACharacterBase : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ACharacterBase();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

public:

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return (UAbilitySystemComponent*)AbilitySystemComponent; };

	virtual USceneComponent* GetItemHoldingComponent() const { return Cast<USceneComponent>(GetMesh()); }

	UFUNCTION()
		virtual void HandleDeath(AActor* InInstigator);

protected:

	/** Components that manages the player abilities */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OBM|Components")
		TObjectPtr<UOBM_AbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OBM|Components")
		UHealthComponent* HealthComponent;

public:

	// Ability sets to grant to this pawn's ability system.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OBM|Abilities")
		TArray<TObjectPtr<UOBM_AbilitySet>> AbilitySets;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OBM|Grip")
		FName GripPointName;
};
