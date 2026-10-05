#include "Forge/ForgeSubsystem.h"
#include "OmniSpace.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "TimerManager.h"
#include "Engine/World.h"

namespace
{
	FString JsonToString(const TSharedRef<FJsonObject>& J)
	{
		FString Out;
		TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> W = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
		FJsonSerializer::Serialize(J, W);
		return Out;
	}
	TSharedPtr<FJsonObject> ParseObject(const FString& Text)
	{
		TSharedPtr<FJsonObject> J;
		TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(Text);
		if (FJsonSerializer::Deserialize(R, J)) return J;
		return nullptr;
	}
	TArray<FString> LogLines(const TSharedPtr<FJsonObject>& J)
	{
		TArray<FString> Lines;
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (J.IsValid() && J->TryGetArrayField(TEXT("log"), Arr))
			for (const auto& V : *Arr) if (V->Type == EJson::String) Lines.Add(V->AsString());
		return Lines;
	}
}

void UForgeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	FString Env = FPlatformMisc::GetEnvironmentVariable(TEXT("OMNISPACE_API"));
	if (!Env.IsEmpty()) BaseUrl = Env;
	FString Cmd;
	if (FParse::Value(FCommandLine::Get(), TEXT("ForgeUrl="), Cmd)) BaseUrl = Cmd;
	CheckHealth();
}

TSharedRef<IHttpRequest, ESPMode::ThreadSafe> UForgeSubsystem::MakeRequest(const FString& Path, const FString& Verb) const
{
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
	Req->SetURL(BaseUrl + Path);
	Req->SetVerb(Verb);
	Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Req->SetTimeout(20.0f);
	return Req;
}

void UForgeSubsystem::CheckHealth()
{
	auto Req = MakeRequest(TEXT("/api/health"), TEXT("GET"));
	Req->SetTimeout(4.0f);
	TWeakObjectPtr<UForgeSubsystem> Weak(this);
	Req->OnProcessRequestComplete().BindLambda([Weak](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
	{
		if (!Weak.IsValid()) return;
		UForgeSubsystem* Self = Weak.Get();
		FForgeStatus S;
		if (bOk && Resp.IsValid() && Resp->GetResponseCode() == 200)
		{
			TSharedPtr<FJsonObject> J = ParseObject(Resp->GetContentAsString());
			const TSharedPtr<FJsonObject>* Ai = nullptr;
			if (J.IsValid() && J->TryGetObjectField(TEXT("ai"), Ai))
			{
				S.bOnline = true;
				(*Ai)->TryGetBoolField(TEXT("enabled"), S.bAi);
				(*Ai)->TryGetStringField(TEXT("model"), S.Model);
				(*Ai)->TryGetStringField(TEXT("reason"), S.Reason);
				(*Ai)->TryGetStringField(TEXT("auth"), S.Auth);
				(*Ai)->TryGetStringField(TEXT("account"), S.Account);
			}
		}
		else S.Reason = TEXT("Stellar Forge server offline");
		Self->Status = S;
		Self->OnStatus.Broadcast(S);
		UE_LOG(LogOmniSpace, Log, TEXT("Forge health: online=%d ai=%d model=%s reason=%s"), S.bOnline ? 1 : 0, S.bAi ? 1 : 0, *S.Model, *S.Reason);
	});
	Req->ProcessRequest();
}

bool UForgeSubsystem::GetCached(const FString& SystemId, FStarSystemData& Out) const
{
	if (const FStarSystemData* S = Cache.Find(SystemId)) { Out = *S; return true; }
	return false;
}

bool UForgeSubsystem::LoadAuthored(const FString& SystemId, FStarSystemData& Out) const
{
	FString Path = FPaths::ProjectContentDir() / TEXT("Data") / (SystemId + TEXT(".json"));
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path)) return false;
	return FStarSystemData::FromJsonString(Text, Out);
}

TSharedRef<FJsonObject> UForgeSubsystem::GenerateBody(const FStarDescriptor& Star, const TArray<FStarDescriptor>& Neighbors, const FString& Commander, bool bPrefetch) const
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetObjectField(TEXT("star"), Star.ToJson());
	TArray<TSharedPtr<FJsonValue>> N;
	for (const FStarDescriptor& S : Neighbors)
	{
		if (S.Id == Star.Id) continue;
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("id"), S.Id);
		O->SetStringField(TEXT("name"), S.Name);
		O->SetNumberField(TEXT("distanceLy"), FVector::Dist(S.Pos, Star.Pos));
		O->SetStringField(TEXT("cls"), S.Cls);
		N.Add(MakeShared<FJsonValueObject>(O));
		if (N.Num() >= 10) break;
	}
	Body->SetArrayField(TEXT("neighbors"), N);
	Body->SetStringField(TEXT("commander"), Commander);
	Body->SetBoolField(TEXT("prefetch"), bPrefetch);
	return Body;
}

void UForgeSubsystem::Finish(const FString& SystemId, bool bOk, const FStarSystemData& System)
{
	Pending.Remove(SystemId);
	if (bOk) Cache.Add(SystemId, System);
	OnSystemLoaded.Broadcast(SystemId, bOk, System);
}

void UForgeSubsystem::LoadSystem(const FStarDescriptor& Star, const TArray<FStarDescriptor>& Neighbors, const FString& Commander)
{
	const FString Id = Star.Id;
	FStarSystemData Found;
	if (GetCached(Id, Found)) { OnSystemLoaded.Broadcast(Id, true, Found); return; }
	if (LoadAuthored(Id, Found)) { Finish(Id, true, Found); return; }
	if (Pending.Contains(Id)) return;
	Pending.Add(Id);

	// Don't gate on the health check (it may not have answered yet at startup): just try;
	// a dead server fails the request and Finish() reports it.
	auto Req = MakeRequest(FString::Printf(TEXT("/api/systems/%s/generate"), *FGenericPlatformHttp::UrlEncode(Id)), TEXT("POST"));
	Req->SetContentAsString(JsonToString(GenerateBody(Star, Neighbors, Commander, false)));
	TWeakObjectPtr<UForgeSubsystem> Weak(this);
	const double Started = FPlatformTime::Seconds();
	Req->OnProcessRequestComplete().BindLambda([Weak, Id, Started](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
	{
		if (!Weak.IsValid()) return;
		UForgeSubsystem* Self = Weak.Get();
		TSharedPtr<FJsonObject> J = (bOk && Resp.IsValid()) ? ParseObject(Resp->GetContentAsString()) : nullptr;
		if (!J.IsValid()) { Self->Finish(Id, false, FStarSystemData()); return; }
		FString St;
		J->TryGetStringField(TEXT("status"), St);
		Self->OnLog.Broadcast(Id, LogLines(J));
		if (St == TEXT("ready"))
		{
			FStarSystemData Sys;
			const TSharedPtr<FJsonObject>* SysObj = nullptr;
			if (J->TryGetObjectField(TEXT("system"), SysObj) && FStarSystemData::FromJson(*SysObj, Sys)) Self->Finish(Id, true, Sys);
			else Self->Finish(Id, false, FStarSystemData());
		}
		else if (St == TEXT("generating")) Self->PollJob(Id, Started);
		else Self->Finish(Id, false, FStarSystemData()); // unavailable / failed
	});
	Req->ProcessRequest();
}

void UForgeSubsystem::PollJob(const FString& SystemId, double StartedAt)
{
	if (FPlatformTime::Seconds() - StartedAt > 300.0) { Finish(SystemId, false, FStarSystemData()); return; }
	TWeakObjectPtr<UForgeSubsystem> Weak(this);
	UWorld* World = GetWorld();
	if (!World) { Finish(SystemId, false, FStarSystemData()); return; }
	FTimerHandle Handle;
	World->GetTimerManager().SetTimer(Handle, [Weak, SystemId, StartedAt]()
	{
		if (!Weak.IsValid()) return;
		UForgeSubsystem* Self = Weak.Get();
		auto Req = Self->MakeRequest(FString::Printf(TEXT("/api/systems/%s/job"), *FGenericPlatformHttp::UrlEncode(SystemId)), TEXT("GET"));
		Req->OnProcessRequestComplete().BindLambda([Weak, SystemId, StartedAt](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
		{
			if (!Weak.IsValid()) return;
			UForgeSubsystem* Inner = Weak.Get(); // not "Self": would shadow the outer lambda's local
			TSharedPtr<FJsonObject> J = (bOk && Resp.IsValid()) ? ParseObject(Resp->GetContentAsString()) : nullptr;
			if (!J.IsValid()) { Inner->Finish(SystemId, false, FStarSystemData()); return; }
			FString St;
			J->TryGetStringField(TEXT("status"), St);
			Inner->OnLog.Broadcast(SystemId, LogLines(J));
			if (St == TEXT("ready"))
			{
				FStarSystemData Sys;
				const TSharedPtr<FJsonObject>* SysObj = nullptr;
				if (J->TryGetObjectField(TEXT("system"), SysObj) && FStarSystemData::FromJson(*SysObj, Sys)) Inner->Finish(SystemId, true, Sys);
				else Inner->Finish(SystemId, false, FStarSystemData());
			}
			else if (St == TEXT("generating")) Inner->PollJob(SystemId, StartedAt);
			else
			{
				FString Err;
				J->TryGetStringField(TEXT("error"), Err);
				UE_LOG(LogOmniSpace, Warning, TEXT("Forge failed for %s: %s"), *SystemId, *Err);
				Inner->Finish(SystemId, false, FStarSystemData());
			}
		});
		Req->ProcessRequest();
	}, 0.7f, false);
}

void UForgeSubsystem::Prefetch(const FStarDescriptor& Star, const TArray<FStarDescriptor>& Neighbors, const FString& Commander)
{
	if (Cache.Contains(Star.Id) || Pending.Contains(Star.Id) || !Status.bOnline || !Status.bAi) return;
	FStarSystemData Authored;
	if (LoadAuthored(Star.Id, Authored)) return;
	auto Req = MakeRequest(FString::Printf(TEXT("/api/systems/%s/generate"), *FGenericPlatformHttp::UrlEncode(Star.Id)), TEXT("POST"));
	Req->SetContentAsString(JsonToString(GenerateBody(Star, Neighbors, Commander, true)));
	Req->ProcessRequest();
}

void UForgeSubsystem::LockIn(const FStarSystemData& System)
{
	if (!Status.bOnline) return;
	auto Req = MakeRequest(FString::Printf(TEXT("/api/systems/%s"), *FGenericPlatformHttp::UrlEncode(System.Id)), TEXT("PUT"));
	Req->SetContentAsString(JsonToString(System.ToJson()));
	Req->ProcessRequest();
}
