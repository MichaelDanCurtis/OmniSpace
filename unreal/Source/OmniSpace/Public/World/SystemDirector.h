// Spawns and positions everything in the current star system using a floating
// origin: "universe" positions are doubles in metres with the star at (0,0,0);
// the player's ship stays near the Unreal world origin and every other actor is
// placed relative to it each frame. World-bounds checks are disabled in the map
// (WorldSettings) so distant bodies can sit far out.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/StarSystemData.h"
#include "SystemDirector.generated.h"

class ACelestialBodyActor;
class AStarActor;
class AStationActor;
class ADirectionalLight;

/** Anything placed in universe space registers one of these. */
USTRUCT()
struct FUniverseEntity
{
	GENERATED_BODY()
	UPROPERTY() TObjectPtr<AActor> Actor = nullptr;
	/** metres, star-centred */
	FVector Pos = FVector::ZeroVector;
	/** bounding radius in metres (for visibility & collision ranges) */
	double Radius = 0;
	FString Id;
	FString Kind; // star | planet | moon | station | poi
};

UCLASS()
class OMNISPACE_API ASystemDirector : public AActor
{
	GENERATED_BODY()
public:
	ASystemDirector();

	/** Build the scene for a system. Previous system is torn down. */
	UFUNCTION(BlueprintCallable, Category = "OmniSpace|System")
	void LoadSystem(const FStarSystemData& System);

	UFUNCTION(BlueprintCallable, Category = "OmniSpace|System")
	void Clear();
	/** Hide/show every system actor (star, bodies, stations) while the pilot is on a landing tile. */
	void SetSystemHidden(bool bHidden);

	/** Universe position of the ship (metres). The director owns it. */
	UFUNCTION(BlueprintPure, Category = "OmniSpace|System")
	FVector GetShipUniversePos() const { return ShipPos; }

	UFUNCTION(BlueprintCallable, Category = "OmniSpace|System")
	void SetShipUniversePos(const FVector& PosM) { ShipPos = PosM; }

	/** Advance the ship by a velocity (m/s) for dt; also applies the floating-origin shift. */
	void MoveShip(const FVector& VelocityMps, float Dt);

	UFUNCTION(BlueprintPure, Category = "OmniSpace|System")
	const FStarSystemData& GetSystem() const { return System; }
	class ADirectionalLight* GetSunLight() const { return SunLight; }
	/** Frame-shift streak on the sky dome. */
	void SetSkyWarp(float Warp, const FVector& Dir, const FVector& Up, float Flow);
	/** Debug: toggle visibility of the sky dome / star. */
	void DebugToggle(int32 What);

	UFUNCTION(BlueprintPure, Category = "OmniSpace|System")
	bool GetEntityPos(const FString& Id, FVector& OutPosM, double& OutRadiusM) const;

	UFUNCTION(BlueprintPure, Category = "OmniSpace|System")
	TArray<FString> GetEntityIds() const;

	/** Nearest body surface distance (m) and its id; for supercruise speed caps. */
	double NearestBody(FString& OutId) const;
	/** Entity kind ("star" | "planet" | "moon" | "station" | "poi"), empty if unknown. */
	FString GetEntityKind(const FString& Id) const;
	/** Display name for an entity id (falls back to the id). */
	FString GetEntityName(const FString& Id) const;
	/** The Forge dressed the current system: swap names/lore in place, no respawn. */
	void ApplyDressing(const FStarSystemData& Dressed);

	/** Universe->world conversion (cm). */
	FVector ToWorld(const FVector& UniversePosM) const { return (UniversePosM - ShipPos) * 100.0; }
	FVector ToUniverse(const FVector& WorldCm) const { return WorldCm / 100.0 + ShipPos; }

	static FVector OrbitOffset(double R, double Phase, double Inc);

	/** Scale applied to distant bodies so they stay inside render range; 1 = true scale. */
	UPROPERTY(EditAnywhere, Category = "OmniSpace|System")
	double FarDistanceM = 2.0e8;

	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY() FStarSystemData System;
	UPROPERTY() TArray<FUniverseEntity> Entities;
	UPROPERTY() TObjectPtr<AStarActor> Star;
	UPROPERTY() TObjectPtr<ADirectionalLight> SunLight;
	UPROPERTY() TObjectPtr<class ASkyActor> Sky;
	FVector ShipPos = FVector::ZeroVector;
	double Time = 0;

	void PlaceAll();
};
