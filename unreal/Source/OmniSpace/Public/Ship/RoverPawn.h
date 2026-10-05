// Surface rover: a simple kinematic ground vehicle that hugs the landing tile's heightfield.
// Four virtual wheels sample the terrain, the chassis follows their plane, drive and steering
// are arcade-simple, gravity pulls it down slopes. Built from engine primitives like the rest.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "RoverPawn.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class USpringArmComponent;
class UCameraComponent;
class ASurfaceTile;
class AShipPawn;

UCLASS()
class OMNISPACE_API ARoverPawn : public APawn
{
	GENERATED_BODY()
public:
	ARoverPawn();
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	void Deploy(ASurfaceTile* InTile, AShipPawn* InShip, const FVector& WorldPosCm, const FQuat& Facing);

	UPROPERTY(BlueprintReadOnly, Category = "Rover") double SpeedMS = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Rover") double DistanceToShipM = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Rover") TArray<FString> Messages;
	void Msg(const FString& Text);

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Chassis;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USpringArmComponent> Arm;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Wheels;
	UPROPERTY() TObjectPtr<ASurfaceTile> Tile;
	UPROPERTY() TObjectPtr<AShipPawn> Ship;

	FVector Pos = FVector::ZeroVector; // cm
	double Heading = 0;                 // radians, yaw
	double Speed = 0;                   // m/s along heading
	double WheelSpin = 0;
	double CamYaw = 0, CamPitch = -12;
	float KFwd = 0, KBack = 0, KLeft = 0, KRight = 0;
	bool bBrake = false;
	void OnBoard();
	void OnBrakeDown() { bBrake = true; }
	void OnBrakeUp() { bBrake = false; }
	void OnMouseX(float V) { CamYaw += V * 0.6; }
	void OnMouseY(float V) { CamPitch = FMath::Clamp(CamPitch + V * 0.4, -40.0, 20.0); }
	void BuildBody();
};
