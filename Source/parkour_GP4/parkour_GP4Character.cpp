// Copyright Epic Games, Inc. All Rights Reserved.

#include "parkour_GP4Character.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
//#include "MotionWarpingComponent.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

//////////////////////////////////////////////////////////////////////////
// Aparkour_GP4Character

Aparkour_GP4Character::Aparkour_GP4Character()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
	MeshP = GetMesh();
	IsSliding = false;
	SpeedToStopSliding = 50.0f;
	DefaultMeshLocation = FVector(0.0f, 0.0f, -90.0f);
	DefaultMeshRotation = FRotator(0.0f, 0.0f, 270.0f);

	//// Create Motion Warping Component
	//PMotionWarpingComponent = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarping"));
	////PMotionWarpingComponent->Set // Not Working
}

void Aparkour_GP4Character::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();

	//Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Input

void Aparkour_GP4Character::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Vault Jumping
		//EnhancedInputComponent->BindAction(VaultJumpAction, ETriggerEvent::Started, this, &Aparkour_GP4Character::Vaulting);

		// Sliding
		EnhancedInputComponent->BindAction(SlideAction, ETriggerEvent::Started, this, &Aparkour_GP4Character::Slide);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &Aparkour_GP4Character::Move);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &Aparkour_GP4Character::Look);
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void Aparkour_GP4Character::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	
		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void Aparkour_GP4Character::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}



#pragma region Sliding

/// <summary>
/// Start sliding.
/// </summary>

void Aparkour_GP4Character::Slide()
{
	if (UKismetMathLibrary::VSize(GetCharacterMovement()->Velocity) > 200)
	{
		//GetCapsuleComponent()->
		if (MeshP->GetAnimInstance()->Montage_IsPlaying(SlidingMontage) && GetCharacterMovement()->IsFalling())
		{
			//UE_LOG(LogTemp, Warning, TEXT("GetCharacterMovement()->IsCrouching() is Working!!!"))
		}
		else
		{
			IsSliding = true;

			MeshP->GetAnimInstance()->Montage_Play(SlidingMontage);

			GetCharacterMovement()->Crouch();
			/*GetCapsuleComponent()->SetCapsuleHalfHeight();
				GetCapsuleComponent()->SetCapsuleRadius();*/

			FVector StartPoint(55.0f, -70.0f, -71.0f);
			MeshP->SetRelativeLocation(StartPoint);

			FRotator StartRot(30.0f, 40.0f, 360.0f);
			MeshP->SetRelativeRotation(StartRot);

			UE_LOG(LogTemp, Warning, TEXT("1Before.... GetCharacterMovement()->IsCrouching() is Working!!!"))

			if (!GetCharacterMovement()->IsCrouching()) // !IsSliding
			{
				UE_LOG(LogTemp, Warning, TEXT("2After.... GetCharacterMovement()->IsCrouching() is Working!!!"))

				GetCharacterMovement()->UnCrouch();

				MeshP->SetRelativeLocation(DefaultMeshLocation);
				MeshP->SetRelativeRotation(DefaultMeshRotation);

				/*
				FVector StartPoint(0.0f, 0.0f, -90.0f);
				FVector EndPoint(0.0f, 0.0f, 0.0f);
				FVector InterpVector = FMath::VInterpTo(StartPoint, EndPoint, GetWorld()->GetDeltaSeconds(), 5.0f);*/

				//MeshP->SetRelativeLocation(InterpVector);
				/*GetCapsuleComponent()->SetCapsuleHalfHeight();
				GetCapsuleComponent()->SetCapsuleRadius();
				*/
			}
			GetWorld()->GetTimerManager().SetTimer(SlideTraceHandle, this, &Aparkour_GP4Character::TraceFloorWhileSliding, 0.01f, true);
		}
	}
}
void Aparkour_GP4Character::TraceFloorWhileSliding()
{
	CheckIfOnFloor();
	//if (ContinueSlidingHandle.IsValid())
	//{
	//	GetWorld()->GetTimerManager().ClearTimer(ContinueSlidingHandle); // this might not work
	//	GetCharacterMovement()->UnCrouch();
	//	UE_LOG(LogTemp, Warning, TEXT("6ContinueSlidingHandle.IsValid()!!!"))
	//		//ResetXYRotation();
	//}
	//else
	//{
	//	GetCharacterMovement()->UnCrouch();
	//	UE_LOG(LogTemp, Warning, TEXT("6 False... ContinueSlidingHandle.IsValid()!!!"))
	//		//ResetXYRotation();
	//}
	AlignPlayerToFloor();
	CheckIfHitSurface();
}

/// <summary>
/// Uses a capsule trace to check if the player is grounded while sliding,
/// so if they slide off a ledge they will stop sliding and the animation stops with a blend.
/// </summary>
void Aparkour_GP4Character::CheckIfOnFloor()
{
	UE_LOG(LogTemp, Warning, TEXT("3Check If On Floor.... is Working!!!"))

	// Define the parameters for the capsule trace
	FVector Start = MeshP->GetComponentLocation(); // Starting location of the trace
	FVector End = MeshP->GetComponentLocation(); // Ending location of the trace
	float TraceRadius = 4.0f; // Radius of the capsule
	float TraceHalfHeight = 18.0f; // Half-height of the capsule
	float MontageBlendOutTime = 0.2f;
	ETraceTypeQuery TraceChannel = UEngineTypes::ConvertToTraceType(ECC_Visibility); // Trace channel to use

	// Perform the capsule trace
	FHitResult OutHit;
	bool bHit = UKismetSystemLibrary::CapsuleTraceSingle(GetWorld(), Start, End, TraceRadius, TraceHalfHeight, TraceChannel, false, TArray<AActor*>(), EDrawDebugTrace::ForDuration, OutHit, true);

	// Check if the trace hit something
	if (bHit)
	{
		UE_LOG(LogTemp, Warning, TEXT("5Check If On Floor.... bHit True!!!"))
	}
	else
	{
		IsSliding = false;
		MeshP->GetAnimInstance()->Montage_Stop(MontageBlendOutTime);
		GetWorld()->GetTimerManager().ClearTimer(SlideTraceHandle); // this might not work
		UE_LOG(LogTemp, Warning, TEXT("4Check If On Floor.... is sliding False!!!"))
	}

}

/// <summary>
/// Finds the rotation of the current floor and aligns the player to it smoothly using interpolation.
/// </summary>
void Aparkour_GP4Character::AlignPlayerToFloor()
{
	UE_LOG(LogTemp, Warning, TEXT("8AlignPlayerToFloor!!!"))

	FRotator TargetRotate(GetActorForwardVector().X, 0.0f, GetCharacterMovement()->CurrentFloor.HitResult.ImpactNormal.Z);

	float deltaTime = GetWorld()->GetDeltaSeconds();

	float InterpSpeed = 5.0f;

	FRotator interpRotate = FMath::RInterpTo(GetActorRotation(), TargetRotate, deltaTime, InterpSpeed);

	SetActorRotation(interpRotate);

	//UE_LOG(LogTemp, Warning, TEXT("Mantle Move is working!!"))
}



/// <summary>
/// Check if the player should continue sliding by the checking the angle of the floor. 
/// If it goes down a certain angle, then they will continue, otherwise they will stand up.
/// </summary>



/// <summary>
/// The player should not be able to slide through walls 
/// so a trace is done in the foot that is placed more forward which is the left in this animation. 
/// If the trace hits anything, the sliding will stop.
/// </summary>
void Aparkour_GP4Character::CheckIfHitSurface()
{
	UE_LOG(LogTemp, Warning, TEXT("9CheckIfHitSurface!!!"))

	float TraceZOffset = 0.0f;
	FVector OffsetTraceVector(0, 0, TraceZOffset);
	FVector TraceVector = MeshP->GetSocketLocation("foot_l") + OffsetTraceVector;
	ETraceTypeQuery TraceChannel = UEngineTypes::ConvertToTraceType(ECC_Visibility); // Trace channel to use

	TArray<AActor*> ActorsArray;
	ActorsArray.Add(GetCharacterMovement()->CurrentFloor.HitResult.GetActor());

	float MontageBlendOutTime = 0.2f;

	// Perform the sphere trace
	FHitResult OutHit;

	bool bSphereHit = UKismetSystemLibrary::SphereTraceSingle(GetWorld(), TraceVector, TraceVector, 20.0f, TraceChannel, false, ActorsArray, EDrawDebugTrace::ForDuration, OutHit, true, FLinearColor::Yellow);


	if (bSphereHit) // this is false because it doesn't hit surface.
	{
		UE_LOG(LogTemp, Warning, TEXT("10CheckIfHitSurface... bSphereHit True!!!"))

		FVector offsetZ(0.0f, 0.0f, 1.0f);
		// Calculate the dot product
		float DotProduct = OutHit.ImpactNormal.Dot(offsetZ);

		// Calculate the arc cosine of the dot product
		float ArcCosValue = FMath::Acos(DotProduct);

		// Convert from radians to degrees
		float ArcCosDegrees = FMath::RadiansToDegrees(ArcCosValue);
		float AbsoluteArcCosDegrees = FMath::Abs(ArcCosDegrees);
		float CompareAngle = 80.0f;


		if (AbsoluteArcCosDegrees > CompareAngle)
		{
			UE_LOG(LogTemp, Warning, TEXT("12CheckIfHitSurface... AbsoluteArcCosDegrees > CompareAngle True!!!"))
			GetWorld()->GetTimerManager().ClearTimer(SlideTraceHandle); // this might not work
			//GetWorld()->GetTimerManager().ClearTimer(ContinueSlidingHandle); // this might not work
			//PlayGettingUpEvent();
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("13CheckIfHitSurface... AbsoluteArcCosDegrees > CompareAngle False!!!"))
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("11CheckIfHitSurface... bSphereHit False!!!"))
	}
}


void Aparkour_GP4Character::CheckShouldContinueSliding()
{
	UE_LOG(LogTemp, Warning, TEXT("17CheckShouldContinueSliding!!!"))

	/*if (IsSliding)
	{
		if (IsSlopeUp())
		{
			GetCharacterMovement()->Velocity = CurrentSlidingVelocity;
			GetWorld()->GetTimerManager().SetTimer(ContinueSlidingHandle, this, &AfreeRunning_GP_4Character::ContinueSliding, 0.001f, true);

			CurrentAngle = FindCurrentFloorAngleAndDirection();
		}
		else
		{
			PlayGettingUpEvent();
		}
	}*/
}






#pragma endregion 