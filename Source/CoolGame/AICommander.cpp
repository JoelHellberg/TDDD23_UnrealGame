// Fill out your copyright notice in the Description page of Project Settings.

#include "AICommander.h"
#include "HAL/PlatformProcess.h"
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

void AAICommander::InitializeProxyServer()
{
    // Path to your Python executable
    FString PythonExe = TEXT("python");

    // Get the project directory to build absolute path
    FString ProjectDir = FPaths::ProjectDir();
    FString ScriptFullPath = FPaths::Combine(ProjectDir, TEXT("ProxyServer/proxy_server.py"));

    UE_LOG(LogTemp, Log, TEXT("LLMProcess: Project Directory: %s"), *ProjectDir);
    UE_LOG(LogTemp, Log, TEXT("LLMProcess: Script Path: %s"), *ScriptFullPath);
    UE_LOG(LogTemp, Log, TEXT("LLMProcess: Script Exists: %s"), FPaths::FileExists(ScriptFullPath) ? TEXT("YES") : TEXT("NO"));

    // Build command with -u for unbuffered output
    FString CommandLine = FString::Printf(TEXT("-u \"%s\""), *ScriptFullPath);

    UE_LOG(LogTemp, Log, TEXT("LLMProcess: Full Command: %s %s"), *PythonExe, *CommandLine);

    // Create pipe to READ from child process stdout
    ReadPipe = nullptr;
    WritePipe = nullptr;
    FPlatformProcess::CreatePipe(ReadPipe, WritePipe);

    uint32 ProcessID = 0;
    ProxyProcHandle = FPlatformProcess::CreateProc(
        *PythonExe,             // Executable
        *CommandLine,           // Arguments
        true,                  // bLaunchDetached
        true,                  // bLaunchHidden - CHANGED to see window for debugging
        true,                  // bLaunchReallyHidden - CHANGED
        &ProcessID,             // OutProcessID - capture this
        0,                      // PriorityModifier
        *ProjectDir,            // OptionalWorkingDirectory - set to project dir
        nullptr,              // StdOutPipe - child writes here
        nullptr                // StdInPipe - we would write here (not used)
    );

    if (!ProxyProcHandle.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("LLMProcess: Failed to launch proxy server!"));
        FPlatformProcess::ClosePipe(ReadPipe, WritePipe);
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("LLMProcess: Process created with PID: %d"), ProcessID);

    UE_LOG(LogTemp, Log, TEXT("LLMProcess: Proxy server process started. Polling for readiness..."));

    // Poll the /health endpoint until server is ready
    Async(EAsyncExecution::Thread, [this]()
        {
            const int MaxAttempts = 30; // 30 seconds max
            int Attempts = 0;
            bool bServerReady = false;

            while (Attempts < MaxAttempts && FPlatformProcess::IsProcRunning(ProxyProcHandle))
            {
                Attempts++;

                // Try to connect to health endpoint
                TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HealthRequest = FHttpModule::Get().CreateRequest();
                HealthRequest->SetURL(TEXT("http://127.0.0.1:5000/health"));
                HealthRequest->SetVerb(TEXT("GET"));
                HealthRequest->SetTimeout(1.0f);

                // Use a promise to make this synchronous in the background thread
                TSharedPtr<bool> bRequestComplete = MakeShared<bool>(false);
                TSharedPtr<bool> bRequestSuccess = MakeShared<bool>(false);

                HealthRequest->OnProcessRequestComplete().BindLambda(
                    [bRequestComplete, bRequestSuccess](FHttpRequestPtr Req, FHttpResponsePtr Response, bool bWasSuccessful)
                    {
                        *bRequestSuccess = (bWasSuccessful && Response.IsValid() && Response->GetResponseCode() == 200);
                        *bRequestComplete = true;
                    });

                HealthRequest->ProcessRequest();

                // Wait for request to complete (with timeout)
                float WaitTime = 0.0f;
                while (!(*bRequestComplete) && WaitTime < 1.0f)
                {
                    FPlatformProcess::Sleep(0.1f);
                    WaitTime += 0.1f;
                }

                if (*bRequestSuccess)
                {
                    bServerReady = true;
                    UE_LOG(LogTemp, Log, TEXT("LLMProcess: Health check passed! Server is ready."));

                    // Switch back to game thread
                    AsyncTask(ENamedThreads::GameThread, [this]()
                        {
                            UE_LOG(LogTemp, Log, TEXT("LLMProcess: Proxy server reported ready!"));
                            OnLLMReady.Broadcast();
                        });
                    break;
                }
                else
                {
                    UE_LOG(LogTemp, Verbose, TEXT("LLMProcess: Health check attempt %d/%d failed, retrying..."), Attempts, MaxAttempts);
                    FPlatformProcess::Sleep(1.0f);
                }
            }

            if (!bServerReady)
            {
                UE_LOG(LogTemp, Error, TEXT("LLMProcess: Server failed to become ready after %d attempts"), MaxAttempts);
            }
        });
}

bool AAICommander::IsProxyServerRunning()
{
    // Simply check if the proxy process is still alive
    return ProxyProcHandle.IsValid() && FPlatformProcess::IsProcRunning(ProxyProcHandle);
}

void AAICommander::SetLLMInstructions(const FString& Instructions)
{
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(TEXT("http://127.0.0.1:5000/set_instructions"));
    Request->SetVerb(TEXT("POST"));
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
                UE_LOG(LogTemp, Log, TEXT("LLM Instructions set successfully: %s"), *Response->GetContentAsString());
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
    if (!IsValid(this)) return;

    // Create request (local shared ptr)
    TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(TEXT("http://127.0.0.1:5000/send_to_llm"));
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetTimeout(30.0f);

    // Build JSON...
    TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());
    JsonObject->SetStringField(TEXT("prompt"), Prompt);
    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
    Request->SetContentAsString(OutputString);

    // Keep it alive
    ActiveRequests.Add(Request);

    // Weak pointer to this to avoid using a destroyed UObject
    TWeakObjectPtr<AAICommander> WeakThis(this);

    // Bind; note we capture Request (shared ptr) by value to keep it alive until callback fires
    Request->OnProcessRequestComplete().BindLambda(
        [WeakThis, Request](FHttpRequestPtr Req, FHttpResponsePtr Response, bool bWasSuccessful)
        {
            // Always marshal to GameThread for any UObject or delegate access
            AsyncTask(ENamedThreads::GameThread, [WeakThis, Request, Response, bWasSuccessful]()
                {
                    // Check actor still exists
                    if (!WeakThis.IsValid())
                    {
                        // Actor gone: nothing to do (Request will be released when ActiveRequests cleaned up)
                        return;
                    }

                    AAICommander* Self = WeakThis.Get();

                    // Remove request from list (modify container on game thread)
                    Self->ActiveRequests.Remove(Request);

                    if (bWasSuccessful && Response.IsValid() && EHttpResponseCodes::IsOk(Response->GetResponseCode()))
                    {
                        const FString RawResp = Response->GetContentAsString();
                        // parse JSON safely...
                        TSharedPtr<FJsonObject> JsonObj;
                        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RawResp);
                        if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
                        {
                            FString LLMResponse = JsonObj->GetStringField(TEXT("llm_response"));
                            // Safe to broadcast on game thread
                            Self->OnLLMResponse.Broadcast(LLMResponse);
                        }
                        else
                        {
                            Self->OnLLMResponse.Broadcast(TEXT("Error: Failed to parse response"));
                        }
                    }
                    else
                    {
                        Self->OnLLMResponse.Broadcast(TEXT("Error: Failed to contact LLM"));
                    }
                });
        });

    // Fire it off
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
        UE_LOG(LogTemp, Log, TEXT("LLMProcess: Python proxy server terminated."));
    }

    // Clean up pipes
    if (ReadPipe || WritePipe)
    {
        FPlatformProcess::ClosePipe(ReadPipe, WritePipe);
        ReadPipe = nullptr;
        WritePipe = nullptr;
    }
}