// Client for the Python Stellar Forge server (server/omnispace_server).
// Loads charted systems, asks the AI to forge new ones, and streams the
// cartography log while the pilot sits in hyperspace.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/IHttpRequest.h"
#include "Data/StarSystemData.h"
#include "ForgeSubsystem.generated.h"

USTRUCT(BlueprintType)
struct FForgeStatus
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) bool bOnline = false;
	UPROPERTY(BlueprintReadOnly) bool bAi = false;
	UPROPERTY(BlueprintReadOnly) FString Model;
	UPROPERTY(BlueprintReadOnly) FString Reason;
	UPROPERTY(BlueprintReadOnly) FString Auth;
	UPROPERTY(BlueprintReadOnly) FString Account;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnForgeStatus, const FForgeStatus&, Status);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnForgeLog, const FString&, SystemId, const TArray<FString>&, Lines);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSystemLoaded, const FString&, SystemId, bool, bSuccess, const FStarSystemData&, System);
/** The Forge finished dressing a system that was already delivered as a survey skeleton. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSystemDressed, const FString&, SystemId, const FStarSystemData&, System);

UCLASS()
class OMNISPACE_API UForgeSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Base URL, e.g. http://127.0.0.1:8000. Read from OMNISPACE_API env or defaults. */
	UPROPERTY(BlueprintReadWrite, Category = "OmniSpace|Forge")
	FString BaseUrl = TEXT("http://127.0.0.1:8000");

	UPROPERTY(BlueprintReadOnly, Category = "OmniSpace|Forge")
	FForgeStatus Status;

	UPROPERTY(BlueprintAssignable) FOnForgeStatus OnStatus;
	UPROPERTY(BlueprintAssignable) FOnForgeLog OnLog;
	UPROPERTY(BlueprintAssignable) FOnSystemLoaded OnSystemLoaded;
	UPROPERTY(BlueprintAssignable) FOnSystemDressed OnSystemDressed;

	UFUNCTION(BlueprintCallable, Category = "OmniSpace|Forge")
	void CheckHealth();

	/**
	 * Load a system: cache -> authored (Kelvar) -> server record -> AI generation
	 * (polling the job) -> OnSystemLoaded. Neighbours give the AI continuity context.
	 */
	UFUNCTION(BlueprintCallable, Category = "OmniSpace|Forge")
	void LoadSystem(const FStarDescriptor& Star, const TArray<FStarDescriptor>& Neighbors, const FString& Commander);

	/** Start AI generation early (route plotted); result lands in the server record. */
	UFUNCTION(BlueprintCallable, Category = "OmniSpace|Forge")
	void Prefetch(const FStarDescriptor& Star, const TArray<FStarDescriptor>& Neighbors, const FString& Commander);

	/** Persist a client-generated system so later visits stay consistent. */
	UFUNCTION(BlueprintCallable, Category = "OmniSpace|Forge")
	void LockIn(const FStarSystemData& System);

	UFUNCTION(BlueprintPure, Category = "OmniSpace|Forge")
	bool GetCached(const FString& SystemId, FStarSystemData& Out) const;

	/** The hand-authored home system bundled with the game (Content/Data/kelvar.json). */
	UFUNCTION(BlueprintCallable, Category = "OmniSpace|Forge")
	bool LoadAuthored(const FString& SystemId, FStarSystemData& Out) const;

private:
	TMap<FString, FStarSystemData> Cache;
	TSet<FString> Pending;

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> MakeRequest(const FString& Path, const FString& Verb) const;
	void PollJob(const FString& SystemId, double StartedAt);
	/** After a survey skeleton arrived: keep polling (slowly) until the Forge dresses it. */
	void PollDressing(const FString& SystemId, double StartedAt);
	void HandleReady(const FString& SystemId, const TSharedPtr<FJsonObject>& J, double StartedAt);
	void Finish(const FString& SystemId, bool bOk, const FStarSystemData& System);
	TSharedRef<FJsonObject> GenerateBody(const FStarDescriptor& Star, const TArray<FStarDescriptor>& Neighbors, const FString& Commander, bool bPrefetch) const;
};
