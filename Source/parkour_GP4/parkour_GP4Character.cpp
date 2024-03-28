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

			//FVector StartPoint(55.0f, -70.0f, -71.0f);  // this doesn't work lol
			//MeshP->SetRelativeLocation(StartPoint);

			//FRotator StartRot(30.0f, 40.0f, 360.0f);
			//MeshP->SetRelativeRotation(StartRot);

			UE_LOG(LogTemp, Warning, TEXT("1Before.... GetCharacterMovement()->IsCrouching() is Working!!!"))

			if (!GetCharacterMovement()->IsCrouching()) // !IsSliding
			{
				UE_LOG(LogTemp, Warning, TEXT("2After.... GetCharacterMovement()->IsCrouching() is Working!!!"))

				GetCharacterMovement()->UnCrouch();

				/*MeshP->SetRelativeLocation(DefaultMeshLocation);
				MeshP->SetRelativeRotation(DefaultMeshRotation);*/

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
	GetWorld()->GetTimerManager().ClearTimer(SlideTraceHandle); // this might not work
	if (ContinueSlidingHandle.IsValid())
	{
		GetWorld()->GetTimerManager().ClearTimer(ContinueSlidingHandle); // this might not work
		GetCharacterMovement()->UnCrouch();
		UE_LOG(LogTemp, Warning, TEXT("6ContinueSlidingHandle.IsValid()!!!"))
		ResetXYRotation();
	}
	else
	{
		GetCharacterMovement()->UnCrouch();
		UE_LOG(LogTemp, Warning, TEXT("6 False... ContinueSlidingHandle.IsValid()!!!"))
		ResetXYRotation();
	}
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
			GetWorld()->GetTimerManager().ClearTimer(ContinueSlidingHandle); // this might not work
			PlayGettingUpEvent();
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

	if (IsSliding)
	{
		if (IsSlopeUp())
		{
			GetCharacterMovement()->Velocity = CurrentSlidingVelocity;
			GetWorld()->GetTimerManager().SetTimer(ContinueSlidingHandle, this, &Aparkour_GP4Character::ContinueSliding, 0.001f, true);

			CurrentAngle = FindCurrentFloorAngleAndDirection();
		}
		else
		{
			PlayGettingUpEvent();
		}
	}
}



/// <summary>
/// Finds the current floors angle using math and if the current floor is sloping upwards or downwards based on the direction the player is sliding on it.
/// </summary>
/// <returns>Floor Angle</returns>
float Aparkour_GP4Character::FindCurrentFloorAngleAndDirection()
{
	FVector offsetZ(0.0f, 0.0f, 1.0f);
	float DotProduct = GetCharacterMovement()->CurrentFloor.HitResult.Normal.Dot(offsetZ);

	// Calculate the arc cosine of the dot product
	float ArcCosValue = FMath::Acos(DotProduct);

	// Convert from radians to degrees
	float ArcCosDegrees = FMath::RadiansToDegrees(ArcCosValue);

	//IsSlopeUp();

	return ArcCosDegrees;
}

bool Aparkour_GP4Character::IsSlopeUp()
{

	FRotator MakeRotFromXY(GetCharacterMovement()->CurrentFloor.HitResult.Normal.X, GetActorForwardVector().Y, 0.0f);
	float MultiplyXY = MakeRotFromXY.Roll * MakeRotFromXY.Pitch;

	if (MultiplyXY > 0.0f)
	{
		return true;
	}

	return false;
}

void Aparkour_GP4Character::PlayGettingUpEvent()
{
	UE_LOG(LogTemp, Warning, TEXT("14PlayGettingUpEvent!!!"))

	MeshP->GetAnimInstance()->Montage_Play(SlidingEndMontage); // playGettingup montage event
	FLatentActionInfo FLatentInfo;
	UKismetSystemLibrary::RetriggerableDelay(GetWorld(), 0.02f, FLatentInfo); // might not work

	if (SlideTraceHandle.IsValid())
	{
		GetWorld()->GetTimerManager().ClearTimer(SlideTraceHandle); // this might not work
		UE_LOG(LogTemp, Warning, TEXT("15PlayGettingUpEvent... MyTimerHandleSliding.IsValid() True!!!"))
	}
	GetCharacterMovement()->UnCrouch(); // this area might not work.

	ResetXYRotation();
	UE_LOG(LogTemp, Warning, TEXT("16PlayGettingUpEvent!!!"))
}

void Aparkour_GP4Character::ContinueSliding()
{
	CurrentAngle = FindCurrentFloorAngleAndDirection();

	float InterpFloat = FMath::FInterpTo(CurrentAngle, FindCurrentFloorAngleAndDirection(), GetWorld()->GetDeltaSeconds(), 0.4f);

	// Use the FinterpTo return value if you want a smoother check. Use CurrrentAngle for a sharper stopping.
	if (InterpFloat < 3.0f) // might not work, might need to be changed to greater than.
	{
		if (UKismetMathLibrary::VSize(GetCharacterMovement()->Velocity) < SpeedToStopSliding)
		{
			GetWorld()->GetTimerManager().ClearTimer(ContinueSlidingHandle); // this might not work
			PlayGettingUpEvent();
		}
	}
	else 
	{
		AddMovementInput(UKismetMathLibrary::GetForwardVector(FRotator(0.0f, 0.0f, GetActorRotation().Yaw)));
		GetCharacterMovement()->MaxWalkSpeed = 1500.0f; /// speed you want the player to slide at.
		GetCharacterMovement()->MaxAcceleration = 500.0f; // for how slow or fast the player speeds up.

		CheckIfHitSurface();
		
	}

}



/// <summary>
/// Reset the players rotation once sliding finishes.
/// </summary>
void Aparkour_GP4Character::ResetXYRotation()
{
	UE_LOG(LogTemp, Warning, TEXT("7ResetXYRotation!!!"))

	FRotator DefaultYawRotation(0.0f, 0.0f, GetActorRotation().Yaw);
	// Timeline Equivalent - ResetSlideRotation.

	float deltaTime = GetWorld()->GetDeltaSeconds();

	float InterpSpeed = 5.0f;

	FRotator interpRotate = FMath::RInterpTo(GetActorRotation(), DefaultYawRotation, deltaTime, InterpSpeed);

	SetActorRotation(interpRotate);

	GetCharacterMovement()->MaxWalkSpeed = 500.0f; /// speed you want the player to slide at.
	GetCharacterMovement()->MaxAcceleration = 1500.0f; // for how slow or fast the player speeds up.
}


#pragma endregion


#pragma region Vaulting

void Aparkour_GP4Character::Vaulting()
{
	if (UKismetMathLibrary::VSize(GetCharacterMovement()->Velocity) > 450.0f && !GetCharacterMovement()->IsFalling())
	{
		VaultTrace(380.0f, 100.0f, 45.0f, 100.0f);

		if (CanVault)
		{
			Vault(VaultStartLocation, VaultMiddleLocation, VaultLandLocation, VaultDistance);
		}
	}

}

void Aparkour_GP4Character::VaultTrace(float InitialTraceLength, float SecondaryTraceZOffset, float SecondaryTraceGap, float LandingPositionForwardOffset)
{

	/*
		Do a line trace to trace for the object to vault over. If an object is found, the script continues to find the target locations.
		Vault distance is set to 0 so it can be incremented to find the vaulting distance and play different montages based on it.
	*/

	FVector StartVector = GetActorLocation();

	FVector MultipliedVector = GetActorForwardVector() * InitialTraceLength;

	FVector EndVector = GetActorLocation() + MultipliedVector;

	FHitResult OutHit;
	ETraceTypeQuery TraceChannel = UEngineTypes::ConvertToTraceType(ECC_Visibility); // Trace channel to use
	TArray<AActor*> ActorsArray;
	//EDrawDebugTrace::Type DrawDebugType = EDrawDebugTrace::ForDuration;
	bool bSingleHit = UKismetSystemLibrary::LineTraceSingle(GetWorld(), StartVector, EndVector, TraceChannel, false, ActorsArray, EDrawDebugTrace::ForDuration, OutHit, true);

	if (bSingleHit)
	{
		VaultDistance = 0;
		for (int i = 0; i < 10; i++)
		{
			/*
			* Sphere Traces used to determine the length of the object and height.
			*/

			VaultDistance++;
			FVector MultiVector = GetActorForwardVector() * i * SecondaryTraceGap;
			FVector HitLocation = OutHit.Location + MultiVector;
			FVector AddedVector = HitLocation + (0.0f, 0.0f, SecondaryTraceZOffset);
			TArray<AActor*> ActorsArray2;
			FHitResult OutHit2;

			bool bSphereHit = UKismetSystemLibrary::SphereTraceSingle(GetWorld(), AddedVector, HitLocation, 10.0f, TraceChannel, false, ActorsArray2, EDrawDebugTrace::ForDuration, OutHit2, true);

			if (bSphereHit)
			{
				if (i == 0)
				{
					/*
					* If it is the first/initial trace then the vault starting location is set and a sphere trace is used to check if there is anything blocking so the vault can be cancelled if there is.
					*/

					VaultStartLocation = OutHit2.ImpactPoint;
					FVector AddToVaultStartVector = VaultStartLocation + (0.0f, 0.0f, 20.0f);
					TArray<AActor*> ActorsArray3;
					FHitResult OutHit3;

					bool bSphereHit2 = UKismetSystemLibrary::SphereTraceSingle(GetWorld(), AddToVaultStartVector, AddToVaultStartVector, 10.0f, TraceChannel, false, ActorsArray3, EDrawDebugTrace::ForDuration, OutHit3, true, FLinearColor::Blue);
					if (bSphereHit2)
					{
						CanVault = false;
						break;
					}
				}
				else
				{

					/*
					* if the Trace is not the initial one then the vault middle/height location is set by setting it every time a trace is done, making the final trace the target middle location.
					*/

					VaultMiddleLocation = OutHit2.ImpactPoint;
					FVector AddToVaultStartVector = VaultStartLocation + (0.0f, 0.0f, 20.0f);
					TArray<AActor*> ActorsArray4;
					FHitResult OutHit4;

					bool bSphereHit3 = UKismetSystemLibrary::SphereTraceSingle(GetWorld(), OutHit2.TraceStart, OutHit2.TraceStart, 10.0f, TraceChannel, false, ActorsArray4, EDrawDebugTrace::ForDuration, OutHit4, true, FLinearColor::Blue);
					if (bSphereHit3)
					{
						CanVault = false;
					}
				}
			}
			else
			{
				CanVault = true;

				/*
				* Find the landing location by doing a line trace downwards from an offset so it is not directly tracing down to the object but to the floor.
				*/

				FHitResult OutHit5;
				TArray<AActor*> ActorsArray5;

				FVector MultiplyForwardVector = GetActorForwardVector() * LandingPositionForwardOffset;

				FVector StartTraceEndAddVector = OutHit2.TraceEnd + MultiplyForwardVector;

				FVector EndTraceEndAddVector = StartTraceEndAddVector - (0.0f, 0.0f, 100.0f);

				bool bSingleHit4 = UKismetSystemLibrary::LineTraceSingle(GetWorld(), StartTraceEndAddVector, EndTraceEndAddVector, TraceChannel, false, ActorsArray5, EDrawDebugTrace::ForDuration, OutHit5, true);

				TArray<AActor*> ActorsArray6;
				FHitResult OutHit6;
				bool bSphereHit5 = UKismetSystemLibrary::SphereTraceSingle(GetWorld(), StartTraceEndAddVector, StartTraceEndAddVector, 20.0f, TraceChannel, false, ActorsArray6, EDrawDebugTrace::ForDuration, OutHit6, true, FLinearColor::Blue);

				if (bSphereHit5)
				{
					CanVault = false;
				}
				else
				{
					VaultLandLocation = OutHit5.Location;

				}

				break;
			}
		}
	}
}

void Aparkour_GP4Character::Vault(FVector InputVaultStartLocation, FVector InputVaultMiddleLocation, FVector InputVaultLandLocation, int InputVaultDistance)
{
	CanVault = false;

	GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Flying);

}

#pragma endregion



void Aparkour_GP4Character::MantleTrace(float InitialTraceLength, float SecondaryTraceZOffset, float FallingHeightMultiplier)
{
	CanMantle = false;

	/*
		Trace to check for an object.
	*/

	FVector StartVector = GetActorLocation();
	FVector MultipliedVector = GetActorForwardVector() * InitialTraceLength;
	FVector EndVector = GetActorLocation() + MultipliedVector;

	FHitResult OutHit;
	ETraceTypeQuery TraceChannel = UEngineTypes::ConvertToTraceType(ECC_Visibility); // Trace channel to use
	TArray<AActor*> ActorsArray;
	//EDrawDebugTrace::Type DrawDebugType = EDrawDebugTrace::ForDuration;
	bool bSingleHit = UKismetSystemLibrary::LineTraceSingle(GetWorld(), StartVector, EndVector, TraceChannel, false, ActorsArray, EDrawDebugTrace::ForDuration, OutHit, true);

	if (bSingleHit)
	{
		/*
			If an object is found then trace for the object height. If player is falling then the maximum reachable height is lowered.
		*/

		float SelectedFloat = UKismetMathLibrary::SelectFloat(FallingHeightMultiplier, 1.0f, GetCharacterMovement()->IsFalling());
		float Multiplingfloat = SecondaryTraceZOffset * SelectedFloat;
		FVector StartVectorForSphere = OutHit.Location + (0.0f, 0.0f, Multiplingfloat);

		TArray<AActor*> ActorsArray2;
		FHitResult OutHit2;

		bool bSphereHit = UKismetSystemLibrary::SphereTraceSingle(GetWorld(), StartVectorForSphere, OutHit.Location, 10.0f, TraceChannel, false, ActorsArray2, EDrawDebugTrace::ForDuration, OutHit2, true);
		if (bSphereHit)
		{

			/*
				Find the positions to motion warp to using the detected points from the trace.
			*/
			MantlePosition1 = OutHit2.ImpactPoint + (GetActorForwardVector() * -50.0f);
			MantlePosition2 = (GetActorForwardVector() * 120.0f) + OutHit2.ImpactPoint;


			CanMantle = true;

			FVector VectorForSphereTrace = MantlePosition2 + (0.0f, 0.0f, 20.0f);
			TArray<AActor*> ActorsArray3;
			FHitResult OutHit3;

			/*
				Do a sphere trace to check if the player has enough space to land at the target location once mantled. This deduces the second motion warp location.
			*/
			bool bSphereHit2 = UKismetSystemLibrary::SphereTraceSingle(GetWorld(), VectorForSphereTrace, VectorForSphereTrace, 10.0f, TraceChannel, false, ActorsArray3, EDrawDebugTrace::ForDuration, OutHit3, true);
			if (bSphereHit2)
			{
				CanMantle = false;

				if (MantlePosition1 == FVector(0.0f, 0.0f, 0.0f) || MantlePosition2 == FVector(0.0f, 0.0f, 0.0f))
				{
					CanMantle = false;
				}
				else
				{
					FVector EndVectorForSphere4 = MantlePosition2 + (0.0f, 0.0f, 100.0f);
					FVector MakeVectorMantle1(MantlePosition1.X, MantlePosition1.Y, EndVectorForSphere4.Z);

					TArray<AActor*> ActorsArray4;
					FHitResult OutHit4;
					/*
						Do a final trace to check if the path from the first and second mantle position is clear.
					*/
					bool bSphereHit3 = UKismetSystemLibrary::SphereTraceSingle(GetWorld(), MakeVectorMantle1, EndVectorForSphere4, 20.0f, TraceChannel, false, ActorsArray4, EDrawDebugTrace::ForDuration, OutHit4, true);

					if (bSphereHit3)
					{
						CanMantle = false;
						// return mantle1Pos and mantle2Pos
					}
					else
					{
						// return mantle1Pos and mantle2Pos
					}
				}
			}
			else
			{
				MantlePosition2 = (GetActorForwardVector() * 50.0f) + OutHit2.ImpactPoint;

				if (MantlePosition1 == FVector(0.0f, 0.0f, 0.0f) || MantlePosition2 == FVector(0.0f, 0.0f, 0.0f))
				{
					CanMantle = false;
				}
				else
				{
					FVector EndVectorForSphere5 = MantlePosition2 + (0.0f, 0.0f, 100.0f);
					FVector MakeVectorMantle1_2(MantlePosition1.X, MantlePosition1.Y, EndVectorForSphere5.Z);

					TArray<AActor*> ActorsArray4;
					FHitResult OutHit4;
					/*
						Do a final trace to check if the path from the first and second mantle position is clear.
					*/
					bool bSphereHit3 = UKismetSystemLibrary::SphereTraceSingle(GetWorld(), MakeVectorMantle1_2, EndVectorForSphere5, 20.0f, TraceChannel, false, ActorsArray4, EDrawDebugTrace::ForDuration, OutHit4, true);

					if (bSphereHit3)
					{
						CanMantle = false;
						// return mantle1Pos and mantle2Pos
					}
					else
					{
						// return mantle1Pos and mantle2Pos
					}
				}
			}
		}
	}
	else
	{
		CanMantle = false;

	}
}
