// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/Components/OvrlCharacterMovementComponent.h"

#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "KismetTraceUtils.h"
#include "MotionWarpingComponent.h"

#include "Player/OvrlPlayerCharacter.h"

#include "OvrlGameplayTags.h"

UOvrlCharacterMovementComponent::UOvrlCharacterMovementComponent()
{
	GetNavAgentPropertiesRef().bCanCrouch = true;
	GetNavAgentPropertiesRef().bCanJump = true;

	bCanWalkOffLedgesWhenCrouching = true;

	// Parkour variables
	WallrunCheckDistance = 75.f;
	WallrunCheckAngle = 35.f;
	WallrunCameraTiltAngle = 15.f;
	WallrunResetTime = .35f;
	WallrunJumpVelocity = FVector(1000.f, 1000.f, 800.f);

	SlideDistanceCheck = 200.f;
	SlideForce = 800.f;
	SlideGroundFriction = 0.f;
	SlideBraking = 1000.f;
	SlideMaxWalkSpeedCrouched = 0.f;

	TraversalCheckDistance = FVector2D(100.f, 50.f);
	MaxVaultHeight = 120.f;
	MaxMantleHeight = 220.f;
}

void UOvrlCharacterMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	// Use the character movement mode to set the locomotion mode to the right value. This allows you to have a
	// custom set of movement modes but still use the functionality of the default character movement component.

	switch (MovementMode)
	{
	case MOVE_Walking:
	case MOVE_NavWalking:
		SetLocomotionMode(OvrlLocomotionModeTags::Grounded);
		break;

	case MOVE_Falling:
		SetLocomotionMode(OvrlLocomotionModeTags::InAir);
		break;

	default:
		SetLocomotionMode(FGameplayTag::EmptyTag);
		break;
	}

	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
}

void UOvrlCharacterMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	Character = Cast<AOvrlPlayerCharacter>(GetCharacterOwner());

	// Save original character values due to reset them after applying parkour movement.
	DefaultGravity = GravityScale;
	DefaultMaxWalkSpeed = MaxWalkSpeed;
	DefaultMaxWalkSpeedCrouched = MaxWalkSpeedCrouched;
	DefaultGroundFriction = GroundFriction;
	DefaultBrakingDecelerationWalking = BrakingDecelerationWalking;
}

void UOvrlCharacterMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	HandleWallrun(DeltaTime);

	if (ShouldCancelSliding())
	{
		// TODO: Replace with Gameplay Ability
		Character->UnCrouch();
		CancelSliding();
	}
}

void UOvrlCharacterMovementComponent::Crouch(bool bClientSimulation)
{
	if (!HasValidData())
	{
		return;
	}

	if (!bClientSimulation && !CanCrouchInCurrentState())
	{
		return;
	}

	// See if collision is already at desired size.
	if (CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() == CrouchedHalfHeight)
	{
		if (!bClientSimulation)
		{
			CharacterOwner->bIsCrouched = true;
		}

		CharacterOwner->OnStartCrouch(0.f, 0.f);
		SetStance(OvrlStanceTags::Crouching);
		return;
	}

	if (bClientSimulation && CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy)
	{
		// restore collision size before crouching
		ACharacter* DefaultCharacter = CharacterOwner->GetClass()->GetDefaultObject<ACharacter>();
		CharacterOwner->GetCapsuleComponent()->SetCapsuleSize(DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleRadius(), DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight());
		bShrinkProxyCapsule = true;
	}

	// Change collision size to crouching dimensions
	const float ComponentScale = CharacterOwner->GetCapsuleComponent()->GetShapeScale();
	const float OldUnscaledHalfHeight = CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	const float OldUnscaledRadius = CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleRadius();
	// Height is not allowed to be smaller than radius.
	const float ClampedCrouchedHalfHeight = FMath::Max3(0.f, OldUnscaledRadius, CrouchedHalfHeight);
	CharacterOwner->GetCapsuleComponent()->SetCapsuleSize(OldUnscaledRadius, ClampedCrouchedHalfHeight);
	float HalfHeightAdjust = (OldUnscaledHalfHeight - ClampedCrouchedHalfHeight);
	float ScaledHalfHeightAdjust = HalfHeightAdjust * ComponentScale;

	if (!bClientSimulation)
	{
		// Crouching to a larger height? (this is rare)
		if (ClampedCrouchedHalfHeight > OldUnscaledHalfHeight)
		{
			FCollisionQueryParams CapsuleParams(SCENE_QUERY_STAT(CrouchTrace), false, CharacterOwner);
			FCollisionResponseParams ResponseParam;
			InitCollisionParams(CapsuleParams, ResponseParam);
			const bool bEncroached = GetWorld()->OverlapBlockingTestByChannel(UpdatedComponent->GetComponentLocation() + ScaledHalfHeightAdjust * GetGravityDirection(), GetWorldToGravityTransform(),
				UpdatedComponent->GetCollisionObjectType(), GetPawnCapsuleCollisionShape(SHRINK_None), CapsuleParams, ResponseParam);

			// If encroached, cancel
			if (bEncroached)
			{
				CharacterOwner->GetCapsuleComponent()->SetCapsuleSize(OldUnscaledRadius, OldUnscaledHalfHeight);
				return;
			}
		}

		if (bCrouchMaintainsBaseLocation)
		{
			// Intentionally not using MoveUpdatedComponent, where a horizontal plane constraint would prevent the base of the capsule from staying at the same spot.
			UpdatedComponent->MoveComponent(ScaledHalfHeightAdjust * GetGravityDirection(), UpdatedComponent->GetComponentQuat(), true, nullptr, EMoveComponentFlags::MOVECOMP_NoFlags, ETeleportType::TeleportPhysics);
		}

		CharacterOwner->bIsCrouched = true;
	}

	bForceNextFloorCheck = true;

	// OnStartCrouch takes the change from the Default size, not the current one (though they are usually the same).
	const float MeshAdjust = ScaledHalfHeightAdjust;
	ACharacter* DefaultCharacter = CharacterOwner->GetClass()->GetDefaultObject<ACharacter>();
	HalfHeightAdjust = (DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() - ClampedCrouchedHalfHeight);
	ScaledHalfHeightAdjust = HalfHeightAdjust * ComponentScale;

	AdjustProxyCapsuleSize();
	CharacterOwner->OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	SetStance(OvrlStanceTags::Crouching);

	// Don't smooth this change in mesh position
	if ((bClientSimulation && CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy) || (IsNetMode(NM_ListenServer) && CharacterOwner->GetRemoteRole() == ROLE_AutonomousProxy))
	{
		FNetworkPredictionData_Client_Character* ClientData = GetPredictionData_Client_Character();
		if (ClientData)
		{
			ClientData->MeshTranslationOffset -= MeshAdjust * -GetGravityDirection();
			ClientData->OriginalMeshTranslationOffset = ClientData->MeshTranslationOffset;
		}
	}
}

void UOvrlCharacterMovementComponent::UnCrouch(bool bClientSimulation)
{
	if (!HasValidData())
	{
		return;
	}

	ACharacter* DefaultCharacter = CharacterOwner->GetClass()->GetDefaultObject<ACharacter>();

	// See if collision is already at desired size.
	if (CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() == DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight())
	{
		if (!bClientSimulation)
		{
			CharacterOwner->bIsCrouched = false;
		}

		CharacterOwner->OnEndCrouch(0.f, 0.f);
		SetStance(OvrlStanceTags::Standing);
		return;
	}

	const float CurrentCrouchedHalfHeight = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

	const float ComponentScale = CharacterOwner->GetCapsuleComponent()->GetShapeScale();
	const float OldUnscaledHalfHeight = CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	const float HalfHeightAdjust = DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() - OldUnscaledHalfHeight;
	const float ScaledHalfHeightAdjust = HalfHeightAdjust * ComponentScale;
	const FVector PawnLocation = UpdatedComponent->GetComponentLocation();

	// Grow to uncrouched size.
	check(CharacterOwner->GetCapsuleComponent());

	if (!bClientSimulation)
	{
		// Try to stay in place and see if the larger capsule fits. We use a slightly taller capsule to avoid penetration.
		const UWorld* MyWorld = GetWorld();
		const float SweepInflation = UE_KINDA_SMALL_NUMBER * 10.f;
		FCollisionQueryParams CapsuleParams(SCENE_QUERY_STAT(CrouchTrace), false, CharacterOwner);
		FCollisionResponseParams ResponseParam;
		InitCollisionParams(CapsuleParams, ResponseParam);

		// Compensate for the difference between current capsule size and standing size
		const FCollisionShape StandingCapsuleShape = GetPawnCapsuleCollisionShape(SHRINK_HeightCustom, -SweepInflation - ScaledHalfHeightAdjust); // Shrink by negative amount, so actually grow it.
		const ECollisionChannel CollisionChannel = UpdatedComponent->GetCollisionObjectType();
		bool bEncroached = true;

		if (!bCrouchMaintainsBaseLocation)
		{
			// Expand in place
			bEncroached = MyWorld->OverlapBlockingTestByChannel(PawnLocation, GetWorldToGravityTransform(), CollisionChannel, StandingCapsuleShape, CapsuleParams, ResponseParam);

			if (bEncroached)
			{
				// Try adjusting capsule position to see if we can avoid encroachment.
				if (ScaledHalfHeightAdjust > 0.f)
				{
					// Shrink to a short capsule, sweep down to base to find where that would hit something, and then try to stand up from there.
					float PawnRadius, PawnHalfHeight;
					CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleSize(PawnRadius, PawnHalfHeight);
					const float ShrinkHalfHeight = PawnHalfHeight - PawnRadius;
					const float TraceDist = PawnHalfHeight - ShrinkHalfHeight;
					const FVector Down = TraceDist * GetGravityDirection();

					FHitResult Hit(1.f);
					const FCollisionShape ShortCapsuleShape = GetPawnCapsuleCollisionShape(SHRINK_HeightCustom, ShrinkHalfHeight);
					const bool bBlockingHit = MyWorld->SweepSingleByChannel(Hit, PawnLocation, PawnLocation + Down, GetWorldToGravityTransform(), CollisionChannel, ShortCapsuleShape, CapsuleParams);
					if (Hit.bStartPenetrating)
					{
						bEncroached = true;
					}
					else
					{
						// Compute where the base of the sweep ended up, and see if we can stand there
						const float DistanceToBase = (Hit.Time * TraceDist) + ShortCapsuleShape.Capsule.HalfHeight;
						const FVector Adjustment = (-DistanceToBase + StandingCapsuleShape.Capsule.HalfHeight + SweepInflation + MIN_FLOOR_DIST / 2.f) * -GetGravityDirection();
						const FVector NewLoc = PawnLocation + Adjustment;
						bEncroached = MyWorld->OverlapBlockingTestByChannel(NewLoc, GetWorldToGravityTransform(), CollisionChannel, StandingCapsuleShape, CapsuleParams, ResponseParam);
						if (!bEncroached)
						{
							// Intentionally not using MoveUpdatedComponent, where a horizontal plane constraint would prevent the base of the capsule from staying at the same spot.
							UpdatedComponent->MoveComponent(NewLoc - PawnLocation, UpdatedComponent->GetComponentQuat(), false, nullptr, EMoveComponentFlags::MOVECOMP_NoFlags, ETeleportType::TeleportPhysics);
						}
					}
				}
			}
		}
		else
		{
			// Expand while keeping base location the same.
			FVector StandingLocation = PawnLocation + (StandingCapsuleShape.GetCapsuleHalfHeight() - CurrentCrouchedHalfHeight) * -GetGravityDirection();
			bEncroached = MyWorld->OverlapBlockingTestByChannel(StandingLocation, GetWorldToGravityTransform(), CollisionChannel, StandingCapsuleShape, CapsuleParams, ResponseParam);

			if (bEncroached)
			{
				if (IsMovingOnGround())
				{
					// Something might be just barely overhead, try moving down closer to the floor to avoid it.
					const float MinFloorDist = UE_KINDA_SMALL_NUMBER * 10.f;
					if (CurrentFloor.bBlockingHit && CurrentFloor.FloorDist > MinFloorDist)
					{
						StandingLocation -= (CurrentFloor.FloorDist - MinFloorDist) * -GetGravityDirection();
						bEncroached = MyWorld->OverlapBlockingTestByChannel(StandingLocation, GetWorldToGravityTransform(), CollisionChannel, StandingCapsuleShape, CapsuleParams, ResponseParam);
					}
				}
			}

			if (!bEncroached)
			{
				// Commit the change in location.
				UpdatedComponent->MoveComponent(StandingLocation - PawnLocation, UpdatedComponent->GetComponentQuat(), false, nullptr, EMoveComponentFlags::MOVECOMP_NoFlags, ETeleportType::TeleportPhysics);
				bForceNextFloorCheck = true;
			}
		}

		// If still encroached then abort.
		if (bEncroached)
		{
			return;
		}

		CharacterOwner->bIsCrouched = false;
	}
	else
	{
		bShrinkProxyCapsule = true;
	}

	// Now call SetCapsuleSize() to cause touch/untouch events and actually grow the capsule
	CharacterOwner->GetCapsuleComponent()->SetCapsuleSize(DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleRadius(), DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight(), true);

	const float MeshAdjust = ScaledHalfHeightAdjust;
	AdjustProxyCapsuleSize();
	CharacterOwner->OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	SetStance(OvrlStanceTags::Standing);

	// Don't smooth this change in mesh position
	if ((bClientSimulation && CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy) || (IsNetMode(NM_ListenServer) && CharacterOwner->GetRemoteRole() == ROLE_AutonomousProxy))
	{
		FNetworkPredictionData_Client_Character* ClientData = GetPredictionData_Client_Character();
		if (ClientData)
		{
			ClientData->MeshTranslationOffset += MeshAdjust * -GetGravityDirection();
			ClientData->OriginalMeshTranslationOffset = ClientData->MeshTranslationOffset;
		}
	}
}

bool UOvrlCharacterMovementComponent::DoJump(bool bReplayingMoves, float DeltaTime)
{
	const FTraversalResult TraversalResult = CheckForTraversal();

	if (TraversalResult.bFound) // Let's overcome the traversal
	{

		// Allow the animation's root motion to ignore the gravity.
		SetMovementMode(EMovementMode::MOVE_Flying);

		// @TODO: Valutare se � meglio lasciare questo oppure no, o farlo in base all'animazione.
		// Perch� per un ostacolo corto, rende la scavalcata pi� smooth, ma per uno pi� lungo sembra che faccia uno "scattino" in su quando finisce l'animazione.
		Character->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		switch (TraversalResult.Type)
		{
		case ETraversalType::Vault:
			SetVaultWarpingData(TraversalResult);
			HandleVault();
			break;

		case ETraversalType::Mantle:
			SetMantleWarpingData(TraversalResult);
			HandleMantle();
			break;

		default:
			break;
		}

		return false; // Avoid jumping
	}

	OnPlayerJumped();

	// Do the default jump
	return Super::DoJump(bReplayingMoves, DeltaTime);
}

void UOvrlCharacterMovementComponent::ResetTraversal()
{
	Character->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	SetMovementMode(EMovementMode::MOVE_Falling);
}

void UOvrlCharacterMovementComponent::OnPlayerJumped()
{
	bShouldSlideOnLanded = false;

	if (LocomotionAction == FGameplayTag::EmptyTag)
	{
		// The player will still be on the ground (not falling) when the Jump function is called
		if (!IsFalling())
		{
			bCanCheckWallrun = true;
		}
	}
	else
	{
		HandleWallrunJump();
	}
}

void UOvrlCharacterMovementComponent::OnPlayerLanded()
{
	EndWallrun();

	if (bShouldSlideOnLanded)
	{
		Character->Crouch();

		// Is Player still moving?
		if (GetLastUpdateVelocity().Length() > 0)
		{
			HandleSliding();
		}

		bShouldSlideOnLanded = false;
	}
}

void UOvrlCharacterMovementComponent::HandleCrouching(bool bInWantsToCrouch)
{
	const bool bIsPlayerGrounded = !IsFalling();

	if (bInWantsToCrouch)
	{
		if (bIsPlayerGrounded)
		{
			Character->Crouch();
		}

		HandleSliding();
	}
	else
	{
		Character->UnCrouch();

		CancelSliding();
	}
}

FTraversalResult UOvrlCharacterMovementComponent::CheckForTraversal()
{
	FTraversalResult TraversalResult;

	FVector TraceStart = Character->GetActorLocation();
	FVector TraceEnd = TraceStart + Character->GetActorForwardVector() * TraversalCheckDistance.X;

	const float CapsuleRadius = Character->GetCapsuleComponent()->GetScaledCapsuleRadius();
	const float CapsuleHalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Character);

	// Make first sweep trace to find if there's any obstacle in front of us
	FHitResult ForwardTraversalHit;
	GetWorld()->SweepSingleByChannel(ForwardTraversalHit, TraceStart, TraceEnd, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight), QueryParams);

	//DrawDebugCapsuleTraceSingle(World, Start, End, Radius, HalfHeight, DrawDebugType, bHit, OutHit, TraceColor, TraceHitColor, DrawTime);

	if (!ForwardTraversalHit.bBlockingHit) // No traversals found
		return TraversalResult;

	//DrawDebugDirectionalArrow(GetWorld(), )

	const FVector ForwardImpactPoint = ForwardTraversalHit.ImpactPoint;

	// This position will always be the bottom of the player capsule
	const FVector FeetLocation = GetActorFeetLocation();

	// If we found a traversal, we make a downward capsule sweep to find the height of the traversal.
	const float InwardOffset = 20.f;

	// Calculate a vector with opposite direction of the hit normal
	const FVector InwardPosition = ForwardImpactPoint - ForwardTraversalHit.ImpactNormal * InwardOffset;

	TraceStart = FVector(InwardPosition.X, InwardPosition.Y, FeetLocation.Z + (CapsuleHalfHeight * 2.f) + TraversalCheckDistance.Y);
	TraceEnd = FVector(InwardPosition.X, InwardPosition.Y, FeetLocation.Z); // @TODO: Forse � meglio che il trace finisca a 10/15cm pi� sopra della mesh location

	const float DownwardCapsuleRadius = 10.f;
	const float DownwardCapsuleHalfHeight = 20.f;

	// Perform downward trace
	FHitResult DownwardTraversalHit;
	GetWorld()->SweepSingleByChannel(DownwardTraversalHit, TraceStart, TraceEnd, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeCapsule(DownwardCapsuleRadius, DownwardCapsuleHalfHeight), QueryParams);

	DrawDebugCapsuleTraceSingle(GetWorld(), TraceStart, TraceEnd, DownwardCapsuleRadius, DownwardCapsuleHalfHeight, EDrawDebugTrace::ForDuration, DownwardTraversalHit.bBlockingHit, DownwardTraversalHit, FLinearColor::Red, FLinearColor::Green, 5.f);

	//// Will always hit?
	//if (!TraversalHit.bBlockingHit) // No traversals found
	//	return TraversalResult;

	const FVector DownwardImpactPoint = DownwardTraversalHit.ImpactPoint;

	const float TraversalHeight = FMath::Abs(DownwardImpactPoint.Z - FeetLocation.Z);

	const FVector FrontEdgeLocation = FVector(ForwardImpactPoint.X, ForwardImpactPoint.Y, DownwardImpactPoint.Z);

	if (TraversalHeight <= MaxVaultHeight) // Vault
	{
		TraversalResult.bFound = true;
		TraversalResult.UpperImpactPoint = DownwardImpactPoint;
		TraversalResult.FrontEdgeLocation = FrontEdgeLocation;
		TraversalResult.Type = ETraversalType::Vault;
	}
	else if (TraversalHeight <= MaxMantleHeight) // Mantle
	{
		TraversalResult.bFound = true;
		TraversalResult.UpperImpactPoint = DownwardImpactPoint;
		TraversalResult.FrontEdgeLocation = FrontEdgeLocation;
		TraversalResult.Type = ETraversalType::Mantle;
	}

	if (TraversalResult.bFound)
	{
		// Calculate Landing Point
		// @TODO: Cambiare logica di calcolo del LandinPoint, cos� � molto brutale
		TraversalResult.LandingPoint = FeetLocation + Character->GetActorForwardVector() * 100.f;
	}

	return TraversalResult;
}

void UOvrlCharacterMovementComponent::SetVaultWarpingData(const FTraversalResult& TraversalResult)
{
	if (UMotionWarpingComponent* MotionWarping = Character->GetMotionWarpingComponent())
	{
		const FRotator WarpRotation = Character->GetActorRotation();

		FMotionWarpingTarget StartWarpTarget;
		StartWarpTarget.Name = StartTraversalWarpTargetName;
		StartWarpTarget.Location = TraversalResult.UpperImpactPoint;
		StartWarpTarget.Rotation = WarpRotation;
		MotionWarping->AddOrUpdateWarpTarget(StartWarpTarget);

		FMotionWarpingTarget EndWarpTarget;
		EndWarpTarget.Name = EndTraversalWarpTargetName;
		EndWarpTarget.Location = TraversalResult.LandingPoint;
		EndWarpTarget.Rotation = WarpRotation;
		MotionWarping->AddOrUpdateWarpTarget(EndWarpTarget);
	}
}

void UOvrlCharacterMovementComponent::SetMantleWarpingData(const FTraversalResult& TraversalResult)
{
	if (UMotionWarpingComponent* MotionWarping = Character->GetMotionWarpingComponent())
	{
		const FRotator WarpRotation = Character->GetActorRotation();

		FMotionWarpingTarget StartWarpTarget;
		StartWarpTarget.Name = StartTraversalWarpTargetName;
		StartWarpTarget.Rotation = WarpRotation;

		const FVector WarpTargetLocation = FVector(GetActorFeetLocation().X, GetActorFeetLocation().Y, TraversalResult.UpperImpactPoint.Z);
		StartWarpTarget.Location = WarpTargetLocation;
		
		MotionWarping->AddOrUpdateWarpTarget(StartWarpTarget);
	}
}

void UOvrlCharacterMovementComponent::HandleVault()
{
	SetLocomotionAction(OvrlLocomotionActionTags::Vaulting);
	Character->PlayAnimMontage(VaultMontage);
}

void UOvrlCharacterMovementComponent::HandleMantle()
{
	SetLocomotionAction(OvrlLocomotionActionTags::Mantling);
	Character->PlayAnimMontage(MantleMontage);
}

void UOvrlCharacterMovementComponent::HandleWallrun(float DeltaTime)
{
	if (bCanCheckWallrun)
	{
		const FVector PlayerInputVector = GetLastInputVector();
		const FVector PlayerForwardVector = Character->GetActorForwardVector();

		const bool bIsPlayerNotGrounded = IsFalling();
		//const bool bIsPlayerMoving = !PlayerInputVector.IsZero();
		const bool bIsPlayerMovingForward = FVector::DotProduct(PlayerInputVector, PlayerForwardVector) > 0;

		if (bIsPlayerNotGrounded && bIsPlayerMovingForward)
		{
			if (HandleWallrunMovement(true))
			{
				//MovementType = EParkourMovementType::WallrunLeft;
			}
			else if (HandleWallrunMovement(false)) // Try the right side
			{
				//MovementType = EParkourMovementType::WallrunRight;
			}
		}
	}

	// The tilting is now managed in the PlayerController class
	//HandleWallrunCameraTilt(DeltaTime);
}

bool UOvrlCharacterMovementComponent::HandleWallrunMovement(bool bIsLeftSide)
{
	const float WallDirection = bIsLeftSide ? -1.f : 1.f;

	const FVector BackwardVector = (Character->GetActorRightVector() * WallrunCheckDistance * WallDirection) + Character->GetActorForwardVector() * -WallrunCheckAngle;

	const FVector StartTrace = Character->GetActorLocation();
	const FVector EndTrace = Character->GetActorLocation() + BackwardVector;

	FHitResult OutHit;
	UKismetSystemLibrary::LineTraceSingle(this, StartTrace, EndTrace, ETraceTypeQuery::TraceTypeQuery1, false, {}, EDrawDebugTrace::None, OutHit, true);

	bool bHandled = false;

	if (OutHit.bBlockingHit)
	{
		WallrunNormal = OutHit.Normal;

		// Returns the direction of where the player should be launched. It follows the wall surface.
		const FVector WallForwardDirection = FVector::CrossProduct(WallrunNormal, -GetGravityDirection());
		const FVector LaunchVelocity = WallForwardDirection * DefaultMaxWalkSpeed * -WallDirection;

		Character->LaunchCharacter(LaunchVelocity, true, true);

		const FGameplayTag TargetGameplayTag = bIsLeftSide ? OvrlLocomotionActionTags::WallrunningLeft : OvrlLocomotionActionTags::WallrunningRight;
		SetLocomotionAction(TargetGameplayTag);
		bHandled = true;
	}

	return bHandled;
}

void UOvrlCharacterMovementComponent::HandleWallrunCameraTilt(float DeltaTime)
{
	FRotator TargetRotation = Character->GetControlRotation();

	if (IsWallrunning())
	{
		// Tilt camera depending on which wall the player is on.
		TargetRotation.Roll = LocomotionAction == OvrlLocomotionActionTags::WallrunningLeft ? WallrunCameraTiltAngle : -WallrunCameraTiltAngle;
	}
	else
	{
		TargetRotation.Roll = 0.f;
	}

	// Lerp and apply rotation
	const FRotator FinalRotation = UKismetMathLibrary::RInterpTo(Character->GetControlRotation(), TargetRotation, DeltaTime, 10.f);
	Character->GetController()->SetControlRotation(FinalRotation);
}

void UOvrlCharacterMovementComponent::HandleWallrunJump()
{
	if (IsWallrunning())
	{
		const float WallDirection = LocomotionAction == OvrlLocomotionActionTags::WallrunningLeft ? -1.f : 1.f;

		EndWallrun();

		const float AwayVelocity = WallrunJumpVelocity.X;
		const float ForwardVelocity = WallrunJumpVelocity.Y;
		const float UpwardVelocity = WallrunJumpVelocity.Z;

		// Get the wall forward vector as forward vector for the direction
		const FVector ForwardDirection = FVector::CrossProduct(WallrunNormal, -GetGravityDirection()) * -WallDirection;

		// Combine all directions together
		const FVector LaunchVelocity = (WallrunNormal * AwayVelocity) + (ForwardDirection * ForwardVelocity) + (-GetGravityDirection() * UpwardVelocity);

		//UKismetSystemLibrary::DrawDebugArrow(this, Character->GetActorLocation(), Character->GetActorLocation() + LaunchVelocity, 4.f, FLinearColor::Green, 10.f, 5.f);

		Character->LaunchCharacter(LaunchVelocity, false, true);
	}
}

void UOvrlCharacterMovementComponent::ResetWallrun()
{
	if (LocomotionAction == FGameplayTag::EmptyTag)
	{
		bCanCheckWallrun = true;
	}
}

void UOvrlCharacterMovementComponent::EndWallrun()
{
	if (IsWallrunning())
	{
		bCanCheckWallrun = false;
		SetLocomotionAction(FGameplayTag::EmptyTag);

		FTimerHandle TimerHandle;
		GetWorld()->GetTimerManager().SetTimer(TimerHandle, this, &UOvrlCharacterMovementComponent::ResetWallrun, WallrunResetTime, false);
	}
}

void UOvrlCharacterMovementComponent::HandleSliding()
{
	if (IsSliding())
		return;

	const bool bIsPlayerGrounded = !IsFalling();
	const bool bIsPlayerMoving = GetLastUpdateVelocity().Length() > 0;

	bShouldSlideOnLanded = !bIsPlayerGrounded;

	if (bIsPlayerGrounded && bIsPlayerMoving)
	{
		// Trace a down vector to check if sliding is available.
		const FVector StartTrace = Character->GetActorLocation();
		const FVector EndTrace = StartTrace + GetGravityDirection() * SlideDistanceCheck;

		FHitResult OutHit;
		UKismetSystemLibrary::LineTraceSingle(this, StartTrace, EndTrace, ETraceTypeQuery::TraceTypeQuery1, false, {}, EDrawDebugTrace::ForDuration, OutHit, true);

		if (OutHit.bBlockingHit)
		{
			GroundFriction = SlideGroundFriction;
			BrakingDecelerationWalking = SlideBraking;
			MaxWalkSpeedCrouched = SlideMaxWalkSpeedCrouched;

			// Cross with floor normal to get the direction of where to launche the character
			const FVector FloorSlopeDirection = FVector::CrossProduct(Character->GetActorRightVector(), OutHit.Normal);
			AddImpulse(FloorSlopeDirection * SlideForce, true);

			SetLocomotionAction(OvrlLocomotionActionTags::Sliding);
		}
	}
}

bool UOvrlCharacterMovementComponent::ShouldCancelSliding()
{
	if (!IsSliding())
		return false;

	//OvrlLOG("%f", GetLastUpdateVelocity().Length());

	return GetLastUpdateVelocity().Length() <= 575.f;
}

void UOvrlCharacterMovementComponent::CancelSliding()
{
	GroundFriction = DefaultGroundFriction;
	BrakingDecelerationWalking = DefaultBrakingDecelerationWalking;
	MaxWalkSpeedCrouched = DefaultMaxWalkSpeedCrouched;

	SetLocomotionAction(FGameplayTag::EmptyTag);
}

void UOvrlCharacterMovementComponent::SetLocomotionAction(const FGameplayTag& NewLocomotionAction)
{
	if (LocomotionAction == NewLocomotionAction)
		return;

	LocomotionAction = NewLocomotionAction;
}

void UOvrlCharacterMovementComponent::SetLocomotionMode(const FGameplayTag& NewLocomotionMode)
{
	if (LocomotionMode == NewLocomotionMode)
		return;

	LocomotionMode = NewLocomotionMode;
}

void UOvrlCharacterMovementComponent::SetStance(const FGameplayTag& NewStance)
{
	if (Stance == NewStance)
		return;

	Stance = NewStance;
}

void UOvrlCharacterMovementComponent::SetGait(const FGameplayTag& NewGait)
{
	if (Gait == NewGait)
		return;

	Gait = NewGait;
}
