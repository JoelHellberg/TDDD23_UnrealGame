// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CoolGameTemplateGameMode.generated.h"

/**
 * 
 */
UCLASS()
class COOLGAME_API ACoolGameTemplateGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	// Display session data in menus/widgets (such as the Session List).
	UFUNCTION(BlueprintImplementableEvent)
	void DisplaySessionInfo(const FString& _sessionId);
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

protected:
	UPROPERTY(BlueprintReadOnly)
	int32 ConnectedPlayersCount = 0;
};
