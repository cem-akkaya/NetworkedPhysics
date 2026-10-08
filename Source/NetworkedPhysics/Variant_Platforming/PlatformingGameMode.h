// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PlatformingGameMode.generated.h"

/**
 *  Simple GameMode for a third person platforming game
 */
UCLASS()
class APlatformingGameMode : public AGameModeBase
{
	GENERATED_BODY()
	
public:

	/** Constructor */
	APlatformingGameMode();
};
