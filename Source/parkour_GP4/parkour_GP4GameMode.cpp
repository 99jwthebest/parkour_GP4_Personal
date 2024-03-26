// Copyright Epic Games, Inc. All Rights Reserved.

#include "parkour_GP4GameMode.h"
#include "parkour_GP4Character.h"
#include "UObject/ConstructorHelpers.h"

Aparkour_GP4GameMode::Aparkour_GP4GameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
