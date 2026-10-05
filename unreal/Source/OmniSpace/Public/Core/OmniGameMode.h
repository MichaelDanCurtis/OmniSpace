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

private:
	FString PendingSystemId;
	bool bPendingJump = false;
};
