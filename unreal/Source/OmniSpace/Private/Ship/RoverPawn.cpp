#include "Ship/RoverPawn.h"
#include "Ship/ShipPawn.h"
#include "World/SurfaceTile.h"
#include "World/OmniAssets.h"
#include "Core/OmniGameMode.h"
#include "OmniSpace.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InputComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "InputCoreTypes.h"

namespace
{
	UStaticMeshComponent* Part(AActor* Owner, USceneComponent* Parent, UStaticMesh* Mesh, const FVector& PosM, const FRotator& Rot, const FVector& SizeM, const FLinearColor& Color, bool bEmissive = false)
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Owner);
		C->SetStaticMesh(Mesh);
		C->SetupAttachment(Parent);
		C->SetRelativeLocation(PosM * 100.0);
		C->SetRelativeRotation(Rot);
		C->SetRelativeScale3D(SizeM);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetMobility(EComponentMobility::Movable);
		if (UMaterialInstanceDynamic* MID = OmniAssets::MakeMID(C, bEmissive ? OmniAssets::EmissiveMaterial() : OmniAssets::HullMaterial(), Owner))
		{
			MID->SetVectorParameterValue(TEXT("Color"), Color);
			MID->SetVectorParameterValue(TEXT("EmissiveColor"), Color);
			MID->SetScalarParameterValue(TEXT("PanelM"), 0.6f);
			MID->SetScalarParameterValue(TEXT("Metal"), 0.3f);
		}
		C->RegisterComponent();
		return C;
	}
}

ARoverPawn::ARoverPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Chassis = CreateDefaultSubobject<USceneComponent>(TEXT("Chassis"));
	Chassis->SetupAttachment(Root);
	Arm = CreateDefaultSubobject<USpringArmComponent>(TEXT("Arm"));
	Arm->SetupAttachment(Root);
	Arm->TargetArmLength = 900.f;
	Arm->SetRelativeLocation(FVector(0, 0, 220));
	Arm->bDoCollisionTest = false;
	Arm->bEnableCameraLag = true;
	Arm->CameraLagSpeed = 6.f;
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Arm);
	Camera->FieldOfView = 75.f;
	FPostProcessSettings& PP = Camera->PostProcessSettings;
	PP.bOverride_AutoExposureMinBrightness = true; PP.bOverride_AutoExposureMaxBrightness = true;
	PP.AutoExposureMinBrightness = 1.0f; PP.AutoExposureMaxBrightness = 1.0f;
	AutoPossessPlayer = EAutoReceiveInput::Disabled;
}

void ARoverPawn::BuildBody()
{
	UStaticMesh* Cube = OmniAssets::CubeMesh();
	UStaticMesh* Cyl = OmniAssets::CylinderMesh();
	const FLinearColor Body(0.55f, 0.48f, 0.35f), Dark(0.12f, 0.12f, 0.13f), Cyan(0.5f, 2.f, 3.f), Amber(3.f, 1.5f, 0.4f);
	Part(this, Chassis, Cube, FVector(0, 0, 0.75), FRotator::ZeroRotator, FVector(3.2, 2.0, 0.7), Body);
	Part(this, Chassis, Cube, FVector(-0.6, 0, 1.35), FRotator::ZeroRotator, FVector(1.6, 1.7, 0.6), Body);     // cabin
	Part(this, Chassis, Cube, FVector(0.25, 0, 1.4), FRotator(-20, 0, 0), FVector(0.1, 1.5, 0.5), FLinearColor(0.05f, 0.2f, 0.35f), true); // windscreen
	Part(this, Chassis, Cube, FVector(0.4, 0, 1.0), FRotator::ZeroRotator, FVector(0.9, 1.9, 0.25), Dark);       // nose
	Part(this, Chassis, Cube, FVector(-1.3, 0, 1.0), FRotator::ZeroRotator, FVector(0.6, 1.9, 0.5), Dark);      // cargo
	Part(this, Chassis, Cyl, FVector(-1.2, 0.5, 1.9), FRotator::ZeroRotator, FVector(0.05, 0.05, 1.2), Dark);  // mast
	Part(this, Chassis, Cube, FVector(-1.2, 0.5, 2.5), FRotator::ZeroRotator, FVector(0.1, 0.1, 0.1), Amber, true);
	Part(this, Chassis, Cube, FVector(1.62, 0.6, 0.95), FRotator::ZeroRotator, FVector(0.04, 0.3, 0.1), Cyan, true); // headlights
	Part(this, Chassis, Cube, FVector(1.62, -0.6, 0.95), FRotator::ZeroRotator, FVector(0.04, 0.3, 0.1), Cyan, true);
	Part(this, Chassis, Cube, FVector(-1.62, 0.7, 0.95), FRotator::ZeroRotator, FVector(0.04, 0.25, 0.08), FLinearColor(4.f, 0.3f, 0.2f), true);
	Part(this, Chassis, Cube, FVector(-1.62, -0.7, 0.95), FRotator::ZeroRotator, FVector(0.04, 0.25, 0.08), FLinearColor(4.f, 0.3f, 0.2f), true);
	const FVector WheelPos[4] = {FVector(1.1, 1.1, 0.4), FVector(1.1, -1.1, 0.4), FVector(-1.1, 1.1, 0.4), FVector(-1.1, -1.1, 0.4)};
	for (int i = 0; i < 4; i++)
	{
		UStaticMeshComponent* W = Part(this, Chassis, Cyl, WheelPos[i], FRotator(0, 0, 90), FVector(0.8, 0.8, 0.4), Dark);
		Wheels.Add(W);
	}
	USpotLightComponent* Lamp = NewObject<USpotLightComponent>(this);
	Lamp->SetupAttachment(Chassis);
	Lamp->SetRelativeLocation(FVector(160, 0, 100));
	Lamp->SetRelativeRotation(FRotator(-10, 0, 0));
	Lamp->SetIntensityUnits(ELightUnits::Candelas);
	Lamp->SetIntensity(2500.f); // headlamp: lights the ground ~60 m out at the pinned exposure
	Lamp->SetInnerConeAngle(20.f);
	Lamp->SetOuterConeAngle(36.f);
	Lamp->SetAttenuationRadius(12000.f);
	Lamp->SetCastShadows(true);
	Lamp->SetLightColor(FLinearColor(0.8f, 0.9f, 1.f));
	Lamp->SetMobility(EComponentMobility::Movable);
	Lamp->RegisterComponent();
}

void ARoverPawn::SetupPlayerInputComponent(UInputComponent* IC)
{
	Super::SetupPlayerInputComponent(IC);
	auto BindHold = [IC, this](const FKey& Key, float& Held)
	{
		float* Slot = &Held;
		FInputKeyBinding Down(FInputChord(Key), IE_Pressed);
		Down.bConsumeInput = true;
		Down.KeyDelegate.GetDelegateForManualSet().BindWeakLambda(this, [Slot]() { *Slot = 1.f; });
		IC->KeyBindings.Add(Down);
		FInputKeyBinding Up(FInputChord(Key), IE_Released);
		Up.bConsumeInput = true;
		Up.KeyDelegate.GetDelegateForManualSet().BindWeakLambda(this, [Slot]() { *Slot = 0.f; });
		IC->KeyBindings.Add(Up);
	};
	BindHold(EKeys::W, KFwd); BindHold(EKeys::Up, KFwd);
	BindHold(EKeys::S, KBack); BindHold(EKeys::Down, KBack);
	BindHold(EKeys::A, KLeft); BindHold(EKeys::Left, KLeft);
	BindHold(EKeys::D, KRight); BindHold(EKeys::Right, KRight);
	IC->BindKey(EKeys::SpaceBar, IE_Pressed, this, &ARoverPawn::OnBrakeDown);
	IC->BindKey(EKeys::SpaceBar, IE_Released, this, &ARoverPawn::OnBrakeUp);
	IC->BindKey(EKeys::G, IE_Pressed, this, &ARoverPawn::OnBoard);
	IC->BindAxisKey(EKeys::MouseX, this, &ARoverPawn::OnMouseX);
	IC->BindAxisKey(EKeys::MouseY, this, &ARoverPawn::OnMouseY);
}

void ARoverPawn::Msg(const FString& Text)
{
	Messages.Add(Text);
	if (Messages.Num() > 20) Messages.RemoveAt(0);
	UE_LOG(LogOmniSpace, Log, TEXT("[rover] %s"), *Text);
}

void ARoverPawn::Deploy(ASurfaceTile* InTile, AShipPawn* InShip, const FVector& WorldPosCm, const FQuat& Facing)
{
	Tile = InTile;
	Ship = InShip;
	if (Wheels.Num() == 0) BuildBody();
	Pos = WorldPosCm;
	Heading = FMath::Atan2(Facing.GetForwardVector().Y, Facing.GetForwardVector().X);
	Speed = 0;
	CamYaw = 0;
	SetActorLocation(Pos);
	Msg(TEXT("Rover deployed. W/S drive, A/D steer, Space brake, mouse looks, G near the ship to board."));
}

void ARoverPawn::OnBoard()
{
	if (!Ship) return;
	if (DistanceToShipM > 25.0) { Msg(FString::Printf(TEXT("Drive closer to the ship to board (%.0f m)"), DistanceToShipM)); return; }
	if (AOmniGameMode* GM = Cast<AOmniGameMode>(GetWorld()->GetAuthGameMode())) GM->BoardShip();
}

void ARoverPawn::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!Tile) return;
	Dt = FMath::Min(Dt, 0.1f);
	const double G = Tile->Gravity();
	// --- drive
	const double Throttle = KFwd - KBack;
	const double Steer = KRight - KLeft;
	const double MaxV = 24.0;
	double Accel = Throttle * 9.0;
	if (bBrake) Accel -= FMath::Sign(Speed) * 14.0;
	// slope: gravity along the heading
	const FVector Fwd(FMath::Cos(Heading), FMath::Sin(Heading), 0);
	const FVector N = Tile->NormalAt(Pos.X / 100.0, Pos.Y / 100.0);
	const double Slope = -FVector::DotProduct(N, Fwd); // >0 uphill ahead
	Accel -= Slope * G * 0.6;
	Speed += Accel * Dt;
	Speed -= Speed * (0.35 + (bBrake ? 2.0 : 0.0)) * Dt;
	Speed = FMath::Clamp(Speed, -MaxV * 0.4, MaxV);
	Heading += Steer * Dt * 1.6 * FMath::Clamp(FMath::Abs(Speed) / 6.0, 0.15, 1.0) * (Speed < 0 ? -1 : 1);
	Pos += FVector(FMath::Cos(Heading), FMath::Sin(Heading), 0) * Speed * Dt * 100.0;
	// --- suspension: four wheel contacts define the chassis plane
	const double Hx = 1.1, Hy = 1.1;
	const FVector Rgt(-FMath::Sin(Heading), FMath::Cos(Heading), 0);
	auto GroundAt = [&](double Fx, double Fy)
	{
		const FVector W = Pos / 100.0 + Fwd * Fx + Rgt * Fy;
		return FMath::Max(Tile->HeightAt(W.X, W.Y), Tile->WaterLevel() - 0.3);
	};
	const double FL = GroundAt(Hx, Hy), FR = GroundAt(Hx, -Hy), BL = GroundAt(-Hx, Hy), BR = GroundAt(-Hx, -Hy);
	const double Centre = (FL + FR + BL + BR) * 0.25;
	const double PitchA = FMath::Atan2((FL + FR) * 0.5 - (BL + BR) * 0.5, 2 * Hx);
	const double RollA = FMath::Atan2((FL + BL) * 0.5 - (FR + BR) * 0.5, 2 * Hy);
	Pos.Z = FMath::Lerp(Pos.Z, (Centre + 0.0) * 100.0, 1.0 - FMath::Exp(-Dt * 12.0));
	SetActorLocation(Pos);
	const FQuat Q = FQuat(FVector::UpVector, Heading) * FQuat(FVector::RightVector, -PitchA) * FQuat(FVector::ForwardVector, RollA);
	Chassis->SetWorldRotation(Q);
	WheelSpin += Speed / 0.4 * Dt;
	for (UStaticMeshComponent* W : Wheels) if (W) W->SetRelativeRotation(FRotator(FMath::RadiansToDegrees(-WheelSpin), 0, 90));
	// --- camera: orbit behind, mouse look
	Arm->SetWorldRotation(FRotator(CamPitch, FMath::RadiansToDegrees(Heading) + CamYaw, 0));
	SpeedMS = FMath::Abs(Speed);
	if (Ship) DistanceToShipM = FVector::Dist(Pos, Ship->GetActorLocation()) / 100.0;
}
