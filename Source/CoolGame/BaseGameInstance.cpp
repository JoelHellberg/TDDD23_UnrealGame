// Fill out your copyright notice in the Description page of Project Settings.


#include "BaseGameInstance.h"
#include "CoolGameTemplateGameMode.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
//#include "interfaces/OnlineSessionInterface.h"

UBaseGameInstance::UBaseGameInstance() {
	// Instantiate the delegate for finding/searching for sessions (call the corresponding function upon completion).
	SearchForSessionsCompletedDelegate = FOnFindSessionsCompleteDelegate::CreateUObject(this, &UBaseGameInstance::SearchForSessionsCompleted);
}

void UBaseGameInstance::HostSession() {
	if (IOnlineSubsystem* onlineSubsystem = IOnlineSubsystem::Get()) {
		if (IOnlineSessionPtr onlineSessionInterface = onlineSubsystem->GetSessionInterface()) {
			TSharedPtr<FOnlineSessionSettings> sessionSettings = MakeShareable(new FOnlineSessionSettings());
			sessionSettings->bAllowInvites = true;
			sessionSettings->bAllowJoinInProgress = true;
			sessionSettings->bAllowJoinViaPresence = true;
			sessionSettings->bAllowJoinViaPresenceFriendsOnly = false;
			sessionSettings->bIsDedicated = false;
			sessionSettings->bUsesPresence = true;
			sessionSettings->bIsLANMatch = true;
			sessionSettings->bShouldAdvertise = true;
			sessionSettings->NumPrivateConnections = 0;
			sessionSettings->NumPublicConnections = 2;

			const ULocalPlayer* localPlayer = GetWorld()->GetFirstLocalPlayerFromController();
			if (onlineSessionInterface->CreateSession(*localPlayer->GetPreferredUniqueNetId(), NAME_GameSession, *sessionSettings)) {
				GEngine->AddOnScreenDebugMessage(0, 30.0f, FColor::Cyan, FString::Printf(TEXT("A session has been created!")));
				UE_LOG(LogTemp, Warning, TEXT("A session has been created!"));
			}
			else {
				GEngine->AddOnScreenDebugMessage(0, 30.0f, FColor::Cyan, FString::Printf(TEXT("A session has failed to be created!")));
				UE_LOG(LogTemp, Warning, TEXT("A session has failed to be created!"));
			}
		}
	}
}

void UBaseGameInstance::SearchForSessions() {
	if (IOnlineSubsystem* onlineSubsystem = IOnlineSubsystem::Get())
	{
		if (IOnlineSessionPtr onlineSessionInterface = onlineSubsystem->GetSessionInterface())
		{
			// Initialize listening for the completion of the search operation.
			SearchForSessionsCompletedHandle = onlineSessionInterface->AddOnFindSessionsCompleteDelegate_Handle(SearchForSessionsCompletedDelegate);

			searchSettings = MakeShareable(new FOnlineSessionSearch());
			searchSettings->bIsLanQuery = true;
			searchSettings->MaxSearchResults = 5;
			searchSettings->PingBucketSize = 30;
			searchSettings->TimeoutInSeconds = 10.0f;

			const ULocalPlayer* localPlayer = GetWorld()->GetFirstLocalPlayerFromController();
			if (onlineSessionInterface->FindSessions(*localPlayer->GetPreferredUniqueNetId(), searchSettings.ToSharedRef())) {
				GEngine->AddOnScreenDebugMessage(0, 30.0f, FColor::Cyan, FString::Printf(TEXT("Search Started.")));
				UE_LOG(LogTemp, Warning, TEXT("Search Started."));
			}
			else {
				GEngine->AddOnScreenDebugMessage(0, 30.0f, FColor::Cyan, FString::Printf(TEXT("Search failed to start.")));
				UE_LOG(LogTemp, Warning, TEXT("Search failed to start."));
			}
		}
	}
}

void UBaseGameInstance::SearchForSessionsCompleted(bool _searchCompleted) {
	if (IOnlineSubsystem* onlineSubsystem = IOnlineSubsystem::Get())
	{
		if (IOnlineSessionPtr onlineSessionInterface = onlineSubsystem->GetSessionInterface())
		{
			// Clear the handle and stop listening for the completion of the search operation.
			onlineSessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(SearchForSessionsCompletedHandle);

			GEngine->AddOnScreenDebugMessage(0, 30.0f, FColor::Cyan, FString::Printf(TEXT("Search found %d sessions after completing search."), searchSettings->SearchResults.Num()));
			UE_LOG(LogTemp, Warning, TEXT("Search found %d sessions after completing search."), searchSettings->SearchResults.Num());

			if (auto gameMode = Cast<ACoolGameTemplateGameMode>(GetWorld()->GetAuthGameMode()))
			{
				for (int i = 0; i < searchSettings->SearchResults.Num(); ++i)
				{
					gameMode->DisplaySessionInfo(searchSettings->SearchResults[i].Session.GetSessionIdStr());
				}
			}
		}
	}
}

/*bool UBaseGameInstance::TravelToSession()
{
	if (IOnlineSubsystem* onlineSubsystem = IOnlineSubsystem::Get()) 
	{
		if (IOnlineSessionPtr onlineSessionInterface = onlineSubsystem->GetSessionInterface()) 
		{
			FString connectionInfo;
			if (onlineSessionInterface->GetResolvedConnectString(NAME_GameSession, connectionInfo)) {
				// Travel the client to the server.
				APlayerController* playerController = GetWorld()->GetFirstPlayerController();
				playerController->ClientTravel(connectionInfo, TRAVEL_Absolute);
				return true;
			}
			else {
				// The connection information could not be obtained.
				return false;
			}
		}
	}

	// The client was unable to travel.
	return false;
}*/