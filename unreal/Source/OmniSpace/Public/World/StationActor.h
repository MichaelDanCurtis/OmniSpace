#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/StarSystemData.h"
#include "StationActor.generated.h"

class UStaticMeshComponent;
class USceneComponent;

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

	UStaticMeshComponent* AddPart(UStaticMesh* Mesh, const FVector& PosM, const FRotator& Rot, const FVector& SizeM, UMaterialInterface* Mat, const FLinearColor& Color, bool bCollide = true);
	void BuildSpire();
	void BuildRing();
	void BuildOutpost();
};
