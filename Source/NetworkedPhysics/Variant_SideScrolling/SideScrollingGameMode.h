// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SideScrollingGameMode.generated.h"

class USideScrollingUI;

/**
 *  Simple Side Scrolling Game Mode
 *  Spawns and manages the game UI
 *  Counts pickups collected by the player
 */
UCLASS(abstract)
class ASideScrollingGameMode : public AGameModeBase
{
	GENERATED_BODY()
	
protected:

	/** Class of UI widget to spawn when the game starts */
	UPROPERTY(EditAnywhere, Category="UI")
	TSubclassOf<USideScrollingUI> UserInterfaceClass;

	/** User interface widget for the game */
	UPROPERTY(BlueprintReadOnly, Category="UI")
	TObjectPtr<USideScrollingUI> UserInterface;

	/** Number of pickups collected by the player */
	UPROPERTY(BlueprintReadOnly, Category="Pickups")
	int32 PickupsCollected = 0;

protected:

	/** Initialization */
	virtual void BeginPlay() override;

public:

	/** Receives an interaction event from another actor */
	virtual void ProcessPickup();
};
