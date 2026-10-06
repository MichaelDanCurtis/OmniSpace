// Player ship: Elite-style flight model ported from client/src/game/engine.ts.
// The pawn stays at the Unreal world origin; its motion is integrated into the
// SystemDirector's universe-space ship position (floating origin).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Data/StarSystemData.h"
#include "ShipPawn.generated.h"

class UCameraComponent;
class USceneComponent;
class ASystemDirector;
class AStationActor;

UENUM(BlueprintType)
enum class EFlightMode : uint8 { Normal, Supercruise, Hyperspace, Docking, Docked, Launching, Destroyed, Surface, Landed };

UENUM(BlueprintType)
enum class EFsdKind : uint8 { None, Supercruise, Jump };

UCLASS()
class OMNISPACE_API AShipPawn : public APawn
{
	GENERATED_BODY()
public:
	AShipPawn();
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void BeginPlay() override;

	// ---- tunables (match the web build)
	UPROPERTY(EditAnywhere, Category = "Ship") double MaxSpeed = 340;      // m/s
	UPROPERTY(EditAnywhere, Category = "Ship") double BoostSpeed = 560;    // m/s
	UPROPERTY(EditAnywhere, Category = "Ship") double ShipRadiusM = 22;
	UPROPERTY(EditAnywhere, Category = "Ship") double SupercruiseMin = 30000; // m/s
	UPROPERTY(EditAnywhere, Category = "Ship") double SupercruiseMax = 2001.0 * 299792458.0;
	UPROPERTY(EditAnywhere, Category = "Ship") double MouseSensitivity = 1.0;
	UPROPERTY(EditAnywhere, Category = "Ship") bool bInvertY = false;

	// ---- state (read by the HUD)
	UPROPERTY(BlueprintReadOnly, Category = "Ship") EFlightMode Mode = EFlightMode::Normal;
	UPROPERTY(BlueprintReadOnly, Category = "Ship") double Throttle = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Ship") FVector Velocity = FVector::ZeroVector; // m/s, world axes
	UPROPERTY(BlueprintReadOnly, Category = "Ship") double SupercruiseSpeed = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Ship") bool bFlightAssist = true;
	UPROPERTY(BlueprintReadOnly, Category = "Ship") double BoostTimer = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Ship") double BoostCharge = 1;
	UPROPERTY(BlueprintReadOnly, Category = "Ship") double Shields = 1;
	UPROPERTY(BlueprintReadOnly, Category = "Ship") double Hull = 1;
	UPROPERTY(BlueprintReadOnly, Category = "Ship") double Fuel = 16;
	UPROPERTY(BlueprintReadOnly, Category = "Ship") double Heat = 0.2;
	UPROPERTY(BlueprintReadOnly, Category = "Ship") EFsdKind FsdKind = EFsdKind::None;
	UPROPERTY(BlueprintReadOnly, Category = "Ship") double FsdProgress = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Ship") FString TargetId;
	UPROPERTY(BlueprintReadOnly, Category = "Ship") FString DockingStationId;
	UPROPERTY(BlueprintReadOnly, Category = "Ship") int32 DockingBay = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Ship") TArray<FString> Messages;
	UPROPERTY(BlueprintReadOnly, Category = "Ship") bool bAutopilot = false;
	/** Next hyperspace destination (galaxy star id) and its descriptor. */
	UPROPERTY(BlueprintReadOnly, Category = "Ship") FString JumpTargetId;
	UPROPERTY(BlueprintReadOnly, Category = "Ship") FStarDescriptor JumpTarget;
	UPROPERTY(BlueprintReadOnly, Category = "Ship") double JumpRangeLy = 18.0;
	UPROPERTY(BlueprintReadOnly, Category = "Ship") double HyperT = 0;
	/** Multi-jump route (star ids, next hop first) and its final destination. */
	UPROPERTY(BlueprintReadOnly, Category = "Ship") TArray<FString> Route;
	UPROPERTY(BlueprintReadOnly, Category = "Ship") FString RouteDestId;
	UPROPERTY(BlueprintReadOnly, Category = "Ship") bool bShowMap = false;
	UPROPERTY(BlueprintReadOnly, Category = "Ship") double LastImpactTime = -100;
	UPROPERTY() TObjectPtr<class UShipAudio> Audio;
	/** Landing tile we are flying over (set by the game mode while on the surface). */
	UPROPERTY() TObjectPtr<class ASurfaceTile> Tile;
	UPROPERTY(BlueprintReadOnly, Category = "Ship") double AltitudeM = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Ship") bool bGear = false;
	UPROPERTY(BlueprintReadOnly, Category = "Ship") bool bLandingLights = false;
	void SetLandingLights(bool bOn, bool bAnnounce = true);
	/** World position (cm) on the tile; the floating origin is paused while on the surface. */
	FVector SurfacePos = FVector::ZeroVector;

	UFUNCTION(BlueprintCallable, Category = "Ship") void Msg(const FString& Text);
	UFUNCTION(BlueprintCallable, Category = "Ship") void SetTarget(const FString& Id);
	UFUNCTION(BlueprintCallable, Category = "Ship") void CycleTarget();
	UFUNCTION(BlueprintCallable, Category = "Ship") void ToggleSupercruise();
	UFUNCTION(BlueprintCallable, Category = "Ship") void RequestDocking();
	UFUNCTION(BlueprintCallable, Category = "Ship") void Launch();
	UFUNCTION(BlueprintCallable, Category = "Ship") void ToggleAutopilot();
	/** Land on the targeted planet (must be close); returns false with a message if not possible. */
	UFUNCTION(BlueprintCallable, Category = "Ship") bool BeginLanding();
	/** Called by the game mode once the tile exists: start the descent. */
	void StartDescent(class ASurfaceTile* InTile);
	/** Called by the game mode when we leave the tile. */
	void EndSurface();
	void FlySurface(float Dt);
	UFUNCTION(BlueprintCallable, Category = "Ship") void CycleJumpTarget();
	/** Cycle through stars beyond jump range (up to 60 ly) and plot a multi-jump route. */
	UFUNCTION(BlueprintCallable, Category = "Ship") void CycleFarTarget();
	/** Called on arrival: advance the route to its next hop. */
	void AdvanceRoute();
	UFUNCTION(BlueprintCallable, Category = "Ship") bool EngageJump();
	/** Direction (world) toward the jump target star; false if none. */
	UFUNCTION(BlueprintPure, Category = "Ship") bool JumpDirection(FVector& OutDir) const;
	UFUNCTION(BlueprintPure, Category = "Ship") double JumpDistanceLy() const;
	UFUNCTION(BlueprintCallable, Category = "Ship") bool IsMassLocked() const;
	UFUNCTION(BlueprintPure, Category = "Ship") double CurrentSpeed() const;
	UFUNCTION(BlueprintPure, Category = "Ship") ASystemDirector* GetDirector() const;

	/** Called by the game mode once the system is up. */
	void AttachDirector(ASystemDirector* InDirector);
	/** Place the ship in universe space looking at a point. */
	void Place(const FVector& UniversePosM, const FVector& LookAtM);

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
	UPROPERTY() TObjectPtr<ASystemDirector> Director;

	// input
	double InPitch = 0, InYaw = 0, InRoll = 0, InStrafe = 0, InVert = 0, InThrottle = 0;
	bool bBoostHeld = false;
	FVector2D Stick = FVector2D::ZeroVector;
	double LastThrottleInput = -100;
	double FsdTime = 0, FsdDuration = 0;
	double LaunchTime = 0;
	double DockT = 0;
	FVector DockFrom;
	FQuat DockFromQ;
	double LastHit = -100, LastImpact = -100;
	int32 ApStage = 0;
	double Time = 0;
	FQuat Orientation = FQuat::Identity;
	FVector AngVel = FVector::ZeroVector;

	// key axes: each key delivers 1 while held; positive/negative pairs are summed in Tick
	float KPitchUp = 0, KPitchDown = 0, KYawL = 0, KYawR = 0, KRollL = 0, KRollR = 0, KStrafeL = 0, KStrafeR = 0, KVertUp = 0, KVertDown = 0, KThrUp = 0, KThrDown = 0;
	void OnPitchUp(float V) { KPitchUp = V; }
	void OnPitchDown(float V) { KPitchDown = V; }
	void OnYawL(float V) { KYawL = V; }
	void OnYawR(float V) { KYawR = V; }
	void OnRollL(float V) { KRollL = V; }
	void OnRollR(float V) { KRollR = V; }
	void OnStrafeL(float V) { KStrafeL = V; }
	void OnStrafeR(float V) { KStrafeR = V; }
	void OnVertUp(float V) { KVertUp = V; }
	void OnVertDown(float V) { KVertDown = V; }
	void OnThrUp(float V) { KThrUp = V; }
	void OnThrDown(float V) { KThrDown = V; }
	void OnMouseX(float V);
	void OnMouseY(float V);
	void OnBoostPressed() { bBoostHeld = true; }
	void OnBoostReleased() { bBoostHeld = false; }
	void OnFullStop();
	void OnCruiseThrottle();
	void OnToggleFA();
	void OnToggleSupercruise() { ToggleSupercruise(); }
	void OnDock();
	void OnRover();
	void OnToggleLights() { SetLandingLights(!bLandingLights); }
	void OnCycleTarget() { CycleTarget(); }
	void OnMouseLock();
	void OnToggleAutopilot() { ToggleAutopilot(); }
	void OnCycleJump() { CycleJumpTarget(); }
	void OnCycleFar() { CycleFarTarget(); }
	void OnToggleMap() { bShowMap = !bShowMap; }
	void OnDebugSky();
	void OnDebugStar();
	void OnDebugBodies();
	void OnDebugCockpit();
	void OnEngageJump() { EngageJump(); }
	void BeginHyperspace();
	void FlyHyperspace(float Dt);
	double FsdWaitAlign = 0;
public:
	UPROPERTY() TObjectPtr<class UStaticMeshComponent> Tunnel;
	UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> TunnelMID;
	UPROPERTY() TObjectPtr<class UStaticMeshComponent> Flash;
	UPROPERTY() TArray<TObjectPtr<class USpotLightComponent>> LandingLights;
	UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> FlashMID;
	float WarpAmount = 0;      // sky streak 0..1
	float WarpFlow = 0;        // accumulated streak scroll (never runs backwards)
	float FlashAmount = 0;     // white-out 0..1
	double ArrivalT = -100;    // time of the last hyperspace exit (stars rush back in)
	FQuat HyperBaseQ = FQuat::Identity;

	struct FApControls { double Pitch = 0, Yaw = 0, Throttle = 0; bool bEngageSC = false, bDropSC = false; };
	bool AutopilotControls(FApControls& Out);

	void Rotate(float Dt, double Pitch, double Yaw, double Roll, const FVector& Rates);
	void FlyNormal(float Dt);
	void FlySupercruise(float Dt);
	void FlyDocking(float Dt);
	void FlyLaunch(float Dt);
	void Collide(float Dt, const FVector& MoveM);
	void Impact(const FVector& Normal, double Mult = 1.0);
	void Damage(double Amount);
	void DropFromSupercruise(bool bAuto);
	void DockAt(const FString& StationId, int32 Bay);
	AStationActor* FindStation(const FString& Id) const;
	void CancelFsd(const FString& Text);
};
