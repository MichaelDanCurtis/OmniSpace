#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Data/StarSystemData.h"
#include "OmniGameMode.generated.h"

class ASystemDirector;
class AShipPawn;

/** Boots the galaxy: loads the home system through the Forge subsystem and places the ship. */
UCLASS()
class OMNISPACE_API AOmniGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	AOmniGameMode();
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "OmniSpace")
	void EnterSystem(const FStarSystemData& System, bool bArrivingFromJump);

	UPROPERTY(BlueprintReadOnly, Category = "OmniSpace") FString Commander = TEXT("Commander");
	UPROPERTY(BlueprintReadOnly, Category = "OmniSpace") TObjectPtr<ASystemDirector> Director;

	UFUNCTION()
	void OnSystemLoaded(const FString& SystemId, bool bSuccess, const FStarSystemData& System);
	UFUNCTION()
	void OnForgeLog(const FString& SystemId, const TArray<FString>& Lines);
	UFUNCTION()
	void OnSystemDressed(const FString& SystemId, const FStarSystemData& System);

	/** Star we are currently in (galaxy descriptor). */
	UPROPERTY(BlueprintReadOnly, Category = "OmniSpace") FStarDescriptor CurrentStar;
	/** Cartography log lines streamed by the Forge for the system being jumped to. */
	UPROPERTY(BlueprintReadOnly, Category = "OmniSpace") TArray<FString> JumpLog;

	/** Hyperspace: ask the Forge for the destination; the ship completes the jump when ready. */
	void BeginJump(const FStarDescriptor& Dest);
	bool IsJumpReady() const { return bJumpReady; }
	bool JumpFailed() const { return bJumpFailed; }
	void CompleteJump();
	void PrefetchJump(const FStarDescriptor& Dest);

private:
	FString PendingSystemId;
	bool bPendingJump = false;
	bool bJumpReady = false;
	bool bJumpFailed = false;
	FStarSystemData ArrivedSystem;
	FStarSystemData PreviousSystem;
	FStarDescriptor PreviousStar;
public:
	/** Jump failed: put the ship back in the system it left. */
	void AbortJump();
	TArray<FStarDescriptor> NeighborsOf(const FStarDescriptor& Star) const;
};
