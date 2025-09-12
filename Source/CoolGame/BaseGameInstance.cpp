// Fill out your copyright notice in the Description page of Project Settings.


#include "BaseGameInstance.h"
#include "CoolGameTemplateGameMode.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
//#include "interfaces/OnlineSessionInterface.h"

UBaseGameInstance::UBaseGameInstance() {
	// Instantiate the delegate for finding/searching for sessions (call the corresponding function upon completion).
	SearchForSessionsCompletedDelegate = FOnFindSessionsCompleteDelegate::CreateUObject(this, &UBaseGameInstance::SearchForSessionsCompleted);

	// Instatiate the delegate for joining sessions (call the corresponding function upon completion).
	JoinSessionCompletedDelegate = FOnJoinSessionCompleteDelegate::CreateUObject(this, &UBaseGameInstance::JoinSessionCompleted);
}

void UBaseGameInstance::Init()
{
	Super::Init();

	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get(TEXT("EOS"));
	if (!OnlineSubsystem) return;

	IOnlineIdentityPtr Identity = OnlineSubsystem->GetIdentityInterface();
	if (!Identity.IsValid()) return;

	Identity->OnLoginCompleteDelegates->AddUObject(this, &UBaseGameInstance::OnLoginComplete);

	// DevAuth login istället för anonymous
	FOnlineAccountCredentials DevCreds(TEXT("developer"), TEXT("localhost:8081"), TEXT("TDDD23-Developer"));
	Identity->Login(0, DevCreds);
}


void UBaseGameInstance::OnLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error)
{
	if (bWasSuccessful)
	{
		bIsLoggedIn = true;
		UE_LOG(LogTemp, Log, TEXT("Login Succeeded! LocalUserNum=%d UserId=%s"), LocalUserNum, *UserId.ToString());
	}
	else
	{
		bIsLoggedIn = false;
		UE_LOG(LogTemp, Error, TEXT("Login Failed: %s"), *Error);
	}

	// Om du vill: ta bort delegaten om du inte behöver fler login events
	// Identity->OnLoginCompleteDelegates->RemoveAll(this); // alternativt spara handle och ta bort
}



void UBaseGameInstance::HostSession() {
	if(bIsLoggedIn) {
	if (IOnlineSubsystem* onlineSubsystem = IOnlineSubsystem::Get(TEXT("EOS"))) {
		if (IOnlineSessionPtr onlineSessionInterface = onlineSubsystem->GetSessionInterface()) {
			TSharedPtr<FOnlineSessionSettings> sessionSettings = MakeShareable(new FOnlineSessionSettings());
			sessionSettings->bAllowInvites = true;
			sessionSettings->bAllowJoinInProgress = true;
			sessionSettings->bAllowJoinViaPresence = true;
			sessionSettings->bAllowJoinViaPresenceFriendsOnly = false;
			sessionSettings->bIsDedicated = false;
			sessionSettings->bUsesPresence = true;
			sessionSettings->bIsLANMatch = false;
			sessionSettings->bShouldAdvertise = true;
			sessionSettings->NumPrivateConnections = 0;
			sessionSettings->NumPublicConnections = 2;

			const ULocalPlayer* localPlayer = GetWorld()->GetFirstLocalPlayerFromController();
			if (!localPlayer)
			{
				UE_LOG(LogTemp, Error, TEXT("No local player available!"));
				return;
			}

			FUniqueNetIdRepl userIdRepl = localPlayer->GetPreferredUniqueNetId();
			if (!userIdRepl.IsValid())
			{
				UE_LOG(LogTemp, Error, TEXT("UniqueNetId not ready yet"));
				return;
			}
			// Få det som TSharedPtr<const FUniqueNetId>
			TSharedPtr<const FUniqueNetId> userIdPtr = userIdRepl.GetUniqueNetId();
			if (!userIdPtr.IsValid())
			{
				UE_LOG(LogTemp, Error, TEXT("Failed to convert UniqueNetId to TSharedPtr"));
				return;
			}
			if (onlineSessionInterface->CreateSession(*userIdPtr, NAME_GameSession, *sessionSettings)) {
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
	else {
		UE_LOG(LogTemp, Log, TEXT("Can't host, user not yet initalized"));
	}
}

void UBaseGameInstance::SearchForSessions() {
	if (IOnlineSubsystem* onlineSubsystem = IOnlineSubsystem::Get(TEXT("EOS")))
	{
		if (IOnlineSessionPtr onlineSessionInterface = onlineSubsystem->GetSessionInterface())
		{
			// Initialize listening for the completion of the search operation.
			SearchForSessionsCompletedHandle = onlineSessionInterface->AddOnFindSessionsCompleteDelegate_Handle(SearchForSessionsCompletedDelegate);

			searchSettings = MakeShareable(new FOnlineSessionSearch());
			searchSettings->bIsLanQuery = false;
			searchSettings->MaxSearchResults = 5;
			searchSettings->PingBucketSize = 30;
			searchSettings->TimeoutInSeconds = 10.0f;

			const ULocalPlayer* localPlayer = GetWorld()->GetFirstLocalPlayerFromController();
			if (onlineSessionInterface->FindSessions(*localPlayer->GetPreferredUniqueNetId(), searchSettings.ToSharedRef())) {
				GEngine->AddOnScreenDebugMessage(1, 30.0f, FColor::Cyan, FString::Printf(TEXT("Search Started.")));
				UE_LOG(LogTemp, Warning, TEXT("Search Started."));
			}
			else {
				GEngine->AddOnScreenDebugMessage(1, 30.0f, FColor::Cyan, FString::Printf(TEXT("Search failed to start.")));
				UE_LOG(LogTemp, Warning, TEXT("Search failed to start."));
			}
		}
	}
}

void UBaseGameInstance::SearchForSessionsCompleted(bool _searchCompleted) {
	if (IOnlineSubsystem* onlineSubsystem = IOnlineSubsystem::Get(TEXT("EOS")))
	{
		if (IOnlineSessionPtr onlineSessionInterface = onlineSubsystem->GetSessionInterface())
		{
			// Clear the handle and stop listening for the completion of the search operation.
			onlineSessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(SearchForSessionsCompletedHandle);

			GEngine->AddOnScreenDebugMessage(2, 30.0f, FColor::Cyan, FString::Printf(TEXT("Search found %d sessions after completing search."), searchSettings->SearchResults.Num()));
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

void UBaseGameInstance::JoinSession() {
	if (IOnlineSubsystem* onlineSubsystem = IOnlineSubsystem::Get(TEXT("EOS")))
	{
		if (IOnlineSessionPtr onlineSessionInterface = onlineSubsystem->GetSessionInterface())
		{
			if (searchSettings->SearchResults.Num() > 0) {
				GEngine->AddOnScreenDebugMessage(3, 30.0f, FColor::Cyan, FString::Printf(TEXT("Joining Session")));
				UE_LOG(LogTemp, Warning, TEXT("Joining Session."));
				const ULocalPlayer* localPlayer = GetWorld()->GetFirstLocalPlayerFromController();
				JoinSessionCompletedHandle = onlineSessionInterface->AddOnJoinSessionCompleteDelegate_Handle(JoinSessionCompletedDelegate);
				onlineSessionInterface->JoinSession(*localPlayer->GetPreferredUniqueNetId(), NAME_GameSession, searchSettings->SearchResults[0]);
			}
		}
	}
}

void UBaseGameInstance::JoinSessionCompleted(FName _sessionName, EOnJoinSessionCompleteResult::Type _joinResult) {
	GEngine->AddOnScreenDebugMessage(4, 30.0f, FColor::Cyan, FString::Printf(TEXT("Join result: %d."), (int32)(_joinResult)));
	UE_LOG(LogTemp, Warning, TEXT("Join result: %d."), (int32)(_joinResult));
	if (IOnlineSubsystem* onlineSubsystem = IOnlineSubsystem::Get(TEXT("EOS")))
	{
		if (IOnlineSessionPtr onlineSessionInterface = onlineSubsystem->GetSessionInterface())
		{
			// Clear the handle and stop listening for the completion of the "Join" operation.
			onlineSessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompletedHandle);

			// Attempt to join the session.
			bool wasTravelSuccessful = TravelToSession();

			GEngine->AddOnScreenDebugMessage(5, 30.0f, FColor::Cyan, FString::Printf(TEXT("Travel successful: %d"), wasTravelSuccessful));
			UE_LOG(LogTemp, Warning, TEXT("Travel successful: %d."), wasTravelSuccessful);
		}
	}
}

bool UBaseGameInstance::TravelToSession()
{
	if (IOnlineSubsystem* onlineSubsystem = IOnlineSubsystem::Get(TEXT("EOS")))
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
}