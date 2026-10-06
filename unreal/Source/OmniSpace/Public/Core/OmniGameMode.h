#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Data/StarSystemData.h"
#include "Data/OutpostData.h"
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

	/** Planetary landing tile: build the surface for a body and move the ship onto it. */
	void EnterSurface(const FString& BodyId, const FVector2D& Site);
	void LeaveSurface();
	void DeployRover();
	void BoardShip();
	UPROPERTY(BlueprintReadOnly, Category = "OmniSpace") TObjectPtr<class ASurfaceTile> Tile;
	UPROPERTY(BlueprintReadOnly, Category = "OmniSpace") TObjectPtr<class ARoverPawn> Rover;
	FString SurfaceBodyId;
	FVector2D SurfaceSite = FVector2D::ZeroVector;
	FVector SurfaceApproachDir = FVector::UpVector;

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

	// ---- outposts: prefab modules on the landing tile, built by delivering materials
	UPROPERTY() TArray<FOutpostData> Outposts;              // every outpost the commander has founded
	UPROPERTY() TObjectPtr<class AOutpostActor> OutpostActor; // the one on the current tile, if any
	int32 CurrentOutpost = -1;                                // index into Outposts
	UPROPERTY(BlueprintReadOnly, Category = "OmniSpace") bool bBuildMode = false;
	UPROPERTY(BlueprintReadOnly, Category = "OmniSpace") EModuleKind BuildKind = EModuleKind::Hab;
	int32 BuildRot = 0;
	int32 GhostGX = 0, GhostGY = 0;
	bool bGhostValid = false;
	UPROPERTY(BlueprintReadOnly, Category = "OmniSpace") FString BuildStatus;
	void ToggleBuildMode();
	void BuildSelect(int32 Slot);      // 1..6: Hab, Power, Storage, Corridor, Lab, Pad
	void BuildRotate(int32 Dir);
	void BuildPlace();
	/** Pawn tick: aim the ghost ahead of the pawn (world cm) and validate it. */
	void UpdateBuildGhost(const FVector& PawnPosCm, const FVector& Facing);
	/** Unload construction materials from a hold into the outpost's sites; returns tons delivered. */
	double DeliverMaterials(FMaterials& Hold, const FVector& ShipPosCm);
	FOutpostData* CurrentOutpostData() { return Outposts.IsValidIndex(CurrentOutpost) ? &Outposts[CurrentOutpost] : nullptr; }
	void SaveOutposts();
	void LoadOutposts();
	FString OutpostSavePath() const;
	void SpawnOutpostActor();
	virtual void Tick(float DeltaSeconds) override;
	TArray<FStarDescriptor> NeighborsOf(const FStarDescriptor& Star) const;
};
