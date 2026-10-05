#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/StarSystemData.h"
#include "StationActor.generated.h"

class UStaticMeshComponent;
class USceneComponent;
class UStaticMesh;
class UMaterialInterface;

/**
 * Placeholder procedural station (spire / ring / outpost) built from engine
 * primitives so docking works before the art kit arrives. Sizes in metres match
 * the web build so the flight model and docking logic carry over.
 */
UCLASS()
class OMNISPACE_API AStationActor : public AActor
{
	GENERATED_BODY()
public:
	AStationActor();
	void Setup(const FStationData& InStation);
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(BlueprintReadOnly, Category = "OmniSpace") FStationData Station;

	/** Docking bay mouth positions (local, cm) and outward normals. */
	UPROPERTY(BlueprintReadOnly, Category = "OmniSpace") TArray<FVector> BayPositions;
	UPROPERTY(BlueprintReadOnly, Category = "OmniSpace") TArray<FVector> BayNormals;

	/** World-space bay transform right now (accounts for spin). */
	UFUNCTION(BlueprintPure, Category = "OmniSpace")
	void GetBayWorld(int32 Index, FVector& OutPos, FVector& OutNormal) const;

	UPROPERTY(BlueprintReadOnly, Category = "OmniSpace") double SpinRateRadPerSec = 0;
	UPROPERTY(BlueprintReadOnly, Category = "OmniSpace") double BoundRadiusM = 3600;

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Spinner;
	double Angle = 0;

	/** Hull MIDs get the station spin angle each tick so their panel pattern turns with the hull. */
	UPROPERTY() TArray<TObjectPtr<class UMaterialInstanceDynamic>> HullMids;
	uint32 Seed = 1;
	double Rand();                          // deterministic 0..1 (mulberry32 on Seed)
	double RandRange(double A, double B) { return A + (B - A) * Rand(); }

	UStaticMeshComponent* AddPart(UStaticMesh* Mesh, const FVector& PosM, const FRotator& Rot, const FVector& SizeM, UMaterialInterface* Mat, const FLinearColor& Color, bool bCollide = true, float Windows = 0.f, float PanelM = 6.f);
	/** A cylinder spanning two points (m) with the given diameter (m). */
	UStaticMeshComponent* AddStrut(const FVector& AM, const FVector& BM, double DiamM, UMaterialInterface* Mat, const FLinearColor& Color);
	/** Scatter industrial clutter (boxes, tanks, masts) on a horizontal annulus. */
	void Greeble(int32 Count, double RMin, double RMax, double Z, double Up, double SMin, double SMax, UMaterialInterface* Hull, const FLinearColor& A, const FLinearColor& B);
	void BuildSpire();
	void BuildRing();
	void BuildOutpost();
};
