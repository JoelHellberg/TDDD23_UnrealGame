// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "interfaces/OnlineSessionInterface.h"
//#include "OnlineSubsystem/OnlineSessionInterface.h"
#include "BaseGameInstance.generated.h"

/**
 * 
 */
UCLASS()
class COOLGAME_API UBaseGameInstance : public UGameInstance
{
	GENERATED_BODY()
	UBaseGameInstance();
	


public:
	// Host an online session.
	UFUNCTION(BlueprintCallable, Category = "Networking")
	void HostSession();

	// Search for an online session.
	UFUNCTION(BlueprintCallable, Category = "Networking")
	void SearchForSessions();
	void SearchForSessionsCompleted(bool _searchCompleted);

	FOnFindSessionsCompleteDelegate SearchForSessionsCompletedDelegate;
	FDelegateHandle SearchForSessionsCompletedHandle;
	TSharedPtr<FOnlineSessionSearch> searchSettings;

	// Join an online session.
	UFUNCTION(BlueprintCallable, Category = "Networking")
	void JoinSession();
	void JoinSessionCompleted(FName _sessionName, EOnJoinSessionCompleteResult::Type _joinResult);

	FOnJoinSessionCompleteDelegate JoinSessionCompletedDelegate;
	FDelegateHandle JoinSessionCompletedHandle;

	// Travel to the joined session (returns true if successful).
	bool TravelToSession();

};