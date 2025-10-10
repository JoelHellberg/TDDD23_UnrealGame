// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AICommander.generated.h"

UCLASS()
class COOLGAME_API AAICommander : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AAICommander();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void* ReadPipe;
	void* WritePipe;
	bool IsProxyServerRunning();
	FProcHandle ProxyProcHandle;
	FTimerHandle PollTimerHandle;

public:	

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLLMResponse, const FString&, Response);
	UPROPERTY(BlueprintAssignable)
	FOnLLMResponse OnLLMResponse;
	UFUNCTION(BlueprintCallable, Category = "LLM")
	void InitializeProxyServer();
	UFUNCTION(BlueprintCallable, Category = "LLM")
	void SetLLMInstructions(const FString& LLMInstructions);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLLMReady);

	UPROPERTY(BlueprintAssignable)
	FOnLLMReady OnLLMReady;
	UFUNCTION(BlueprintCallable, Category = "LLM")

	void SendPromptToLLM(const FString& Prompt);
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};
