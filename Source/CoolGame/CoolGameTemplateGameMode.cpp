// Fill out your copyright notice in the Description page of Project Settings.


#include "CoolGameTemplateGameMode.h"
#include "GameFramework/PlayerState.h"

void ACoolGameTemplateGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);
    ConnectedPlayersCount++;

    UE_LOG(LogTemp, Log, TEXT("Player joined! Total now: %d"), ConnectedPlayersCount);
}

void ACoolGameTemplateGameMode::Logout(AController* Exiting)
{
    Super::Logout(Exiting);
    ConnectedPlayersCount--;

    UE_LOG(LogTemp, Log, TEXT("Player left! Total now: %d"), ConnectedPlayersCount);
}
