// Landing tile: a procedural patch of a planet's surface (Starfield-style). The planet stays a
// shader sphere from orbit; choosing a landing site builds this tile - heightfield terrain from
// the planet type and seed, water where the type calls for it, a sky (atmosphere + fog + sun)
// and a horizon skirt so the edge is never seen. Everything is built from the FBodyData so the
// same site looks the same on every visit.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/StarSystemData.h"
#include "SurfaceTile.generated.h"

class UProceduralMeshComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class USkyAtmosphereComponent;
class UExponentialHeightFogComponent;
class UDirectionalLightComponent;

UCLASS()
class OMNISPACE_API ASurfaceTile : public AActor
{
	GENERATED_BODY()
public:
	ASurfaceTile();

	/** Build the tile for a body; Site picks the landing region on the planet (0..1, 0..1). */
	void Build(const FBodyData& InBody, const FVector2D& Site, const FLinearColor& SunColor, double SunElevationDeg);

	/** Terrain height (m) at a tile-local XY (m). Matches the mesh exactly. */
	double HeightAt(double X, double Y) const;
	/** Surface normal at XY. */
	FVector NormalAt(double X, double Y) const;
	/** Gravity in m/s^2 for this body. */
	double Gravity() const { return GravityMS2; }
	bool HasAtmosphere() const { return bAtmosphere; }
	double WaterLevel() const { return WaterM; }
	const FBodyData& GetBody() const { return Body; }

	UPROPERTY(BlueprintReadOnly, Category = "OmniSpace") double TileSizeM = 12000.0;

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Terrain;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Water;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Skirt;
	UPROPERTY() TObjectPtr<USkyAtmosphereComponent> Atmosphere;
	UPROPERTY() TObjectPtr<UExponentialHeightFogComponent> Fog;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> TerrainMID;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> WaterMID;

	FBodyData Body;
	double Seed = 0;
	double Amplitude = 300;   // m
	double Frequency = 1.0;   // cycles per km
	double WaterM = -1e9;
	double GravityMS2 = 9.8;
	bool bAtmosphere = false;
	int32 Profile = 0;        // 0 rolling, 1 mountains, 2 dunes, 3 lava, 4 ocean, 5 ice
	FVector2D SiteOffset = FVector2D::ZeroVector;
	double HeightPad = 0;      // flattened landing pad level (m)
	bool bPadEnabled = false;

	void ChooseProfile();
	void BuildMesh();
	void BuildSky(const FLinearColor& SunColor, double SunElevationDeg);
};
