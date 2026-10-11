// The 3D galaxy map: a star field in a private corner of the world, looked at by its own camera
// with orbit controls. Stars are instanced spheres (one draw call); the grid, stalks, rings, route
// and labels are drawn by the HUD over projected positions, so they stay crisp at any zoom.
// 1 light year = 1 metre of map space; the galactic plane is the map's XY plane, height is Z.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/StarSystemData.h"
#include "GalaxyMapActor.generated.h"

class UCameraComponent;
class UInstancedStaticMeshComponent;
class USceneComponent;

UCLASS()
class OMNISPACE_API AGalaxyMapActor : public AActor
{
	GENERATED_BODY()
public:
	AGalaxyMapActor();
	virtual void Tick(float DeltaSeconds) override;

	static constexpr double LocalRadiusLy = 80.0;
	static constexpr double LyToCm = 100.0;

	/** Open on the current star: focus there (rebuilding the field if the focus moved), camera home. */
	void Open(const FStarDescriptor& Current);
	void Close();
	/** Rebuild the local field around a star. */
	void SetFocus(const FStarDescriptor& Focus);
	/** Map-space (world cm) position of a galactic position (ly). */
	FVector MapPos(const FVector& PosLy) const;
	const TArray<FStarDescriptor>& Local() const { return LocalStars; }
	const FStarDescriptor& Focus() const { return FocusStar; }
	FVector FocusLy() const { return FocusStar.Pos; }

	// camera
	void Orbit(double DYawDeg, double DPitchDeg);
	void Zoom(double Factor);
	void CenterOn(const FVector& PosLy);
	void HomeCamera(const FVector& PosLy);
	void SetGalaxyView(bool bOn);
	bool IsGalaxyView() const { return bGalaxyView; }
	double CameraDistanceM() const { return Dist / LyToCm; }
	FVector TargetLy() const { return TargetLy_; }

	UCameraComponent* GetCamera() const { return Camera; }

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Stars;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Cloud;
	TArray<FStarDescriptor> LocalStars;
	FStarDescriptor FocusStar;
	bool bBuilt = false;
	bool bCloudBuilt = false;
	bool bGalaxyView = false;

	// orbit camera in map cm
	FVector TargetLy_ = FVector::ZeroVector;   // what the camera looks at (ly)
	double Yaw = -90, Pitch = -34, Dist = 7200;  // cm
	double WantYaw = -90, WantPitch = -34, WantDist = 7200;
	FVector WantTargetLy = FVector::ZeroVector;
	void BuildCloud();
	void UpdateCamera(float Dt);
};
