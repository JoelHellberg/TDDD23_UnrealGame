// Fill out your copyright notice in the Description page of Project Settings.

#include "AICommander.h"
#include "HAL/PlatformProcess.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Http.h"
#include "Json.h"
#include "JsonUtilities.h"

// Sets default values
AAICommander::AAICommander()
{
    // Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
    PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void AAICommander::BeginPlay()
{
    Super::BeginPlay();
}

void AAICommander::InitializeLLMConnection(const FString& LLMInstructions)
{
    InitializeProxyServer();

    // Bind to a repeating timer
    FTimerDelegate TimerDel;
    TimerDel.BindUFunction(this, FName("PollLLMReady"), LLMInstructions);
    GetWorld()->GetTimerManager().SetTimer(PollTimerHandle, TimerDel, 0.1f, true);
}

void AAICommander::PollLLMReady(const FString& LLMInstructions)
{
    if (IsProxyServerRunning())
    {
        SetLLMInstructions(LLMInstructions);

        OnLLMReady.Broadcast();
        GetWorld()->GetTimerManager().ClearTimer(PollTimerHandle);
    }
}

bool AAICommander::IsProxyServerRunning()
{
    // Simply check if the proxy process is still alive
    return ProxyProcHandle.IsValid() && FPlatformProcess::IsProcRunning(ProxyProcHandle);
}

void AAICommander::InitializeProxyServer()
{
    // Path to your Python executable
    FString PythonExe = TEXT("python");

    // Path to your Flask script
    FString ScriptPath = TEXT("ProxyServer/proxy_server.py");

    // Build the command line
    FString CommandLine = ScriptPath;

    void* ReadPipe = nullptr;
    void* WritePipe = nullptr;
    FPlatformProcess::CreatePipe(ReadPipe, WritePipe);

    ProxyProcHandle = FPlatformProcess::CreateProc(
        *PythonExe,             // Executable
        *CommandLine,           // Arguments
        false,                   // bLaunchDetached
        true,                  // bLaunchHidden
        true,                  // bLaunchReallyHidden
        nullptr,                // OutProcessID
        0,                      // PriorityModifier
        nullptr,                // OptionalWorkingDirectory
        WritePipe               // Pipe to capture output
    );
}

void AAICommander::SetLLMInstructions(const FString& Instructions)
{
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(TEXT("http://127.0.0.1:5000/set_instructions"));
    Request->SetVerb("POST");
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

    // Create proper JSON using Unreal's JSON library
    TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());
    JsonObject->SetStringField(TEXT("instructions"), Instructions);

    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

    Request->SetContentAsString(OutputString);

    Request->OnProcessRequestComplete().BindLambda([](FHttpRequestPtr Req, FHttpResponsePtr Response, bool bWasSuccessful)
        {
            if (bWasSuccessful && Response.IsValid())
            {
                UE_LOG(LogTemp, Log, TEXT("Instructions set successfully: %s"), *Response->GetContentAsString());
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("Failed to set LLM instructions"));
            }
        });

    Request->ProcessRequest();
}

void AAICommander::SendPromptToLLM(const FString& Prompt)
{
    UE_LOG(LogTemp, Warning, TEXT("[AAICommander] Sending prompt to LLM: %s"), *Prompt);

    // Step 1: Quick check if proxy server seems alive
    if (!IsProxyServerRunning())
    {
        UE_LOG(LogTemp, Error, TEXT("[AAICommander] Proxy server not running — cannot send prompt!"));
        return;
    }

    // Step 2: Create the HTTP request
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(TEXT("http://127.0.0.1:5000/send_to_llm"));
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

    // Create proper JSON using Unreal's JSON library
    TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());
    JsonObject->SetStringField(TEXT("prompt"), Prompt);

    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

    Request->SetContentAsString(OutputString);

    UE_LOG(LogTemp, Warning, TEXT("[AAICommander] JSON Payload: %s"), *OutputString);

    // Step 3: Bind a lambda to handle response
    Request->OnProcessRequestComplete().BindLambda(
        [this](FHttpRequestPtr Req, FHttpResponsePtr Response, bool bWasSuccessful)
        {
            if (bWasSuccessful && Response.IsValid())
            {
                const FString RawResp = Response->GetContentAsString();
                UE_LOG(LogTemp, Log, TEXT("[AAICommander] Raw LLM Response: %s"), *RawResp);

                // Parse the JSON response
                TSharedPtr<FJsonObject> JsonObject;
                TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RawResp);

                if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
                {
                    // Extract "llm_response" field
                    FString LLMResponse = JsonObject->GetStringField(TEXT("llm_response"));
                    UE_LOG(LogTemp, Log, TEXT("[AAICommander] Parsed LLM Response: %s"), *LLMResponse);

                    // Broadcast the actual response text, not the raw JSON
                    OnLLMResponse.Broadcast(LLMResponse);
                }
                else
                {
                    UE_LOG(LogTemp, Error, TEXT("[AAICommander] Failed to parse JSON response"));
                    OnLLMResponse.Broadcast(TEXT("Error: Failed to parse response"));
                }
            }
            else
            {
                FString StatusMsg = Response.IsValid()
                    ? FString::Printf(TEXT("HTTP Status: %d, Message: %s"), Response->GetResponseCode(), *Response->GetContentAsString())
                    : TEXT("No valid response received.");

                UE_LOG(LogTemp, Error, TEXT("[AAICommander] Failed to contact LLM. %s"), *StatusMsg);
                OnLLMResponse.Broadcast(TEXT("Error: Failed to contact LLM"));
            }
        });

    // Step 4: Actually send the HTTP request
    UE_LOG(LogTemp, Warning, TEXT("[AAICommander] Sending POST request to http://127.0.0.1:5000/send_to_llm ..."));
    Request->ProcessRequest();
}

// Called every frame
void AAICommander::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

void AAICommander::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);

    // Terminate the proxy server if it was started
    if (ProxyProcHandle.IsValid())
    {
        FPlatformProcess::TerminateProc(ProxyProcHandle, true);
        FPlatformProcess::CloseProc(ProxyProcHandle);
        ProxyProcHandle.Reset();
        UE_LOG(LogTemp, Log, TEXT("Python proxy server terminated."));
    }
}