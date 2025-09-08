// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "BaseGameInstance.generated.h"

/**
 * 
 */
UCLASS()
class COOLGAME_API UBaseGameInstance : public UGameInstance
{
	GENERATED_BODY()
	


public:
	UFUNCTION(BlueprintCallable, Category = "Networking")
	void HostSession();


};