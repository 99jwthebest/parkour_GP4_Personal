// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "parkour_GP4Character.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
class USkeletalMeshComponent;
class UAnimMontage;
class UMotionWarpingComponent;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

UCLASS(config=Game)
class Aparkour_GP4Character : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;


	///** Motion Warping Component */
	//UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	//UMotionWarpingComponent* PMotionWarpingComponent;  // Not Working
	

	/** MappingContext */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
		UInputMappingContext* DefaultMappingContext;

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
		UInputAction* JumpAction;

	/** Slide Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
		UInputAction* SlideAction;

	/** VaultJump Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
		UInputAction* VaultJumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
		UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
		UInputAction* LookAction;

public:
	Aparkour_GP4Character();
	

protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);
			
	/******   *******
	**   Sliding   **
	******   *******/

	/** Called for sliding input */
	void Slide();
	void TraceFloorWhileSliding();
	void CheckIfOnFloor();
	void AlignPlayerToFloor();
	void CheckIfHitSurface();
	UFUNCTION(BlueprintCallable, Category = "Movement")
		void CheckShouldContinueSliding();
	float FindCurrentFloorAngleAndDirection();
	bool IsSlopeUp();
	void PlayGettingUpEvent();
	void ContinueSliding();
	void ResetXYRotation();
	UFUNCTION(BlueprintCallable, Category = "Movement")
		void TraceForCeiling();

	/******   *******
	**   Vaulting   **
	******   *******/
	UFUNCTION(BlueprintCallable, Category = "Movement")
		bool Vaulting();
	UFUNCTION(BlueprintCallable, Category = "Movement")
		void VaultTrace(float InitialTraceLength, float SecondaryTraceZOffset, float SecondaryTraceGap, float LandingPositionForwardOffset);


	/******   *******
	**   Mantling   **
	******   *******/
	UFUNCTION(BlueprintCallable, Category = "Movement")
		void MantleTrace(float InitialTraceLength, float SecondaryTraceZOffset, float FallingHeightMultiplier);

protected:
	// APawn interface
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	
	// To add mapping context
	virtual void BeginPlay();

public:
	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	UPROPERTY(EditAnywhere, Category = Mesh)
		USkeletalMeshComponent* MeshP;


	// Sliding

	UPROPERTY(EditAnywhere, Category = Animation)
		UAnimMontage* SlidingMontage;
	UPROPERTY(EditAnywhere, Category = Animation)
		UAnimMontage* SlidingEndMontage;
	UPROPERTY(EditAnywhere, Category = Movement)
		bool IsSliding;
	UPROPERTY(EditAnywhere, Category = Movement)
		FTimerHandle SlideTraceHandle;
	
	UPROPERTY(BlueprintReadWrite, Category = Movement)
		FVector CurrentSlidingVelocity;
	UPROPERTY(EditAnywhere, Category = Movement)
		FTimerHandle ContinueSlidingHandle;
	UPROPERTY(EditAnywhere, Category = Movement)
		float CurrentAngle;
	UPROPERTY(EditAnywhere, Category = Movement)
		float SpeedToStopSliding;


	// Vaulting

	UPROPERTY(BlueprintReadWrite, Category = Movement)
		FVector VaultStartLocation;
	UPROPERTY(BlueprintReadWrite, Category = Movement)
		FVector VaultMiddleLocation;
	UPROPERTY(BlueprintReadWrite, Category = Movement)
		FVector VaultLandLocation;
	UPROPERTY(BlueprintReadWrite, Category = Movement)
		int VaultDistance;
	UPROPERTY(BlueprintReadWrite, Category = Movement)
		bool CanVault;


	// Mantling

	UPROPERTY(BlueprintReadWrite, Category = Movement)
		FVector MantlePosition1;
	UPROPERTY(BlueprintReadWrite, Category = Movement)
		FVector MantlePosition2;
	UPROPERTY(BlueprintReadWrite, Category = Movement)
		bool CanMantle;
};

