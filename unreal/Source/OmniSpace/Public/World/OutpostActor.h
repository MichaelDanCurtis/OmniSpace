// A player outpost on a landing tile: the beacon, the modules (built or under construction) and
// the placement ghost. Visuals are engine primitives on the hull/emissive materials, like the
// stations; the data lives in the game mode and is saved between sessions.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/OutpostData.h"
#include "OutpostActor.generated.h"

class ASurfaceTile;
class USceneComponent;
class UStaticMeshComponent;

UCLASS()
class OMNISPACE_API AOutpostActor : public AActor
{
	GENERATED_BODY()
public:
	AOutpostActor();
	virtual void Tick(float DeltaSeconds) override;

	void Init(ASurfaceTile* InTile, FOutpostData* InData);
	FOutpostData* GetData() const { return Data; }

	/** Rebuild every module's visuals from the data (after placement, delivery or completion). */
	void Rebuild();
	void RebuildModule(int32 Index);

	/** Can this module go here? Why not, if not. Grid coordinates are cells from the beacon. */
	bool Validate(EModuleKind Kind, int32 GX, int32 GY, int32 Rot, FString& Why) const;
	/** Ghost preview at a grid cell; green when valid, red when not. */
	void SetGhost(EModuleKind Kind, int32 GX, int32 GY, int32 Rot, bool bValid);
	void ClearGhost();

	/** Tile metres of a cell corner. */
	FVector2D CellM(int32 GX, int32 GY) const { return Data ? Data->OriginM + FVector2D(GX, GY) * Outpost::CellM : FVector2D(GX, GY) * Outpost::CellM; }
	/** Foundation level (m) for a footprint: the highest terrain under it. */
	double FootZ(int32 GX, int32 GY, int32 W, int32 H) const;
	/** Highest slope (1 - N.z) under a footprint. */
	double FootSlope(int32 GX, int32 GY, int32 W, int32 H) const;
	bool FootDry(int32 GX, int32 GY, int32 W, int32 H) const;

	/** Called by the game mode while construction runs; returns true when something finished. */
	bool AdvanceConstruction(float Dt);
	/** Modules still waiting for materials, nearest first. */
	TArray<int32> SitesNeedingMaterials() const;

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY() TArray<TObjectPtr<USceneComponent>> Nodes;   // one per module
	UPROPERTY() TArray<TObjectPtr<USceneComponent>> Risers;  // the box that grows during construction (or null)
	UPROPERTY() TObjectPtr<USceneComponent> GhostNode;
	UPROPERTY() TObjectPtr<ASurfaceTile> Tile;
	FOutpostData* Data = nullptr;
	TArray<int32> VisState; // -1 none, 0 site, 1 complete

	USceneComponent* MakeNode(const FVector& PosCm);
	void BuildModuleVisual(USceneComponent* Node, const FOutpostModule& M, bool bGhost, bool bValid, USceneComponent*& OutRiser);
	void DestroyNode(USceneComponent* Node);
};
