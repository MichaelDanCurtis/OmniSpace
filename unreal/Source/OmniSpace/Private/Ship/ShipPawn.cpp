#include "Ship/ShipPawn.h"
#include "World/SystemDirector.h"
#include "World/StationActor.h"
#include "OmniSpace.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "CollisionQueryParams.h"
#include "Engine/EngineTypes.h"

namespace
{
	constexpr double DOCK_RANGE = 7500.0;
	constexpr double MASS_LOCK = 6000.0;
	inline double Clamp(double V, double A, double B) { return V < A ? A : (V > B ? B : V); }
}

AShipPawn::AShipPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Root);
	Camera->FieldOfView = 70.f;
	Camera->SetRelativeLocation(FVector(0, 0, 0));
	AutoPossessPlayer = EAutoReceiveInput::Player0;
}

void AShipPawn::BeginPlay()
{
	Super::BeginPlay();
	Orientation = GetActorQuat();
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->bShowMouseCursor = false;
		FInputModeGameOnly Mode;
		PC->SetInputMode(Mode);
	}
}

void AShipPawn::SetupPlayerInputComponent(UInputComponent* IC)
{
	Super::SetupPlayerInputComponent(IC);
	// Keyboard (legacy key bindings: no asset setup required)
	IC->BindAxisKey(EKeys::Up, this, &AShipPawn::OnPitchUp);
	IC->BindAxisKey(EKeys::Down, this, &AShipPawn::OnPitchDown);
	IC->BindAxisKey(EKeys::Left, this, &AShipPawn::OnYawL);
	IC->BindAxisKey(EKeys::Right, this, &AShipPawn::OnYawR);
	IC->BindAxisKey(EKeys::A, this, &AShipPawn::OnRollL);
	IC->BindAxisKey(EKeys::D, this, &AShipPawn::OnRollR);
	IC->BindAxisKey(EKeys::Q, this, &AShipPawn::OnStrafeL);
	IC->BindAxisKey(EKeys::E, this, &AShipPawn::OnStrafeR);
	IC->BindAxisKey(EKeys::R, this, &AShipPawn::OnVertUp);
	IC->BindAxisKey(EKeys::F, this, &AShipPawn::OnVertDown);
	IC->BindAxisKey(EKeys::W, this, &AShipPawn::OnThrUp);
	IC->BindAxisKey(EKeys::S, this, &AShipPawn::OnThrDown);
	IC->BindAxisKey(EKeys::MouseX, this, &AShipPawn::OnMouseX);
	IC->BindAxisKey(EKeys::MouseY, this, &AShipPawn::OnMouseY);
	IC->BindKey(EKeys::Tab, IE_Pressed, this, &AShipPawn::OnBoostPressed);
	IC->BindKey(EKeys::Tab, IE_Released, this, &AShipPawn::OnBoostReleased);
	IC->BindKey(EKeys::SpaceBar, IE_Pressed, this, &AShipPawn::OnBoostPressed);
	IC->BindKey(EKeys::SpaceBar, IE_Released, this, &AShipPawn::OnBoostReleased);
	IC->BindKey(EKeys::X, IE_Pressed, this, &AShipPawn::OnFullStop);
	IC->BindKey(EKeys::C, IE_Pressed, this, &AShipPawn::OnCruiseThrottle);
	IC->BindKey(EKeys::Z, IE_Pressed, this, &AShipPawn::OnToggleFA);
	IC->BindKey(EKeys::J, IE_Pressed, this, &AShipPawn::OnToggleSupercruise);
	IC->BindKey(EKeys::L, IE_Pressed, this, &AShipPawn::OnDock);
	IC->BindKey(EKeys::T, IE_Pressed, this, &AShipPawn::OnCycleTarget);
	IC->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AShipPawn::OnMouseLock);
}

void AShipPawn::OnMouseX(float V)
{
	Stick.X = Clamp(Stick.X + V * 0.012 * MouseSensitivity, -1, 1);
}
void AShipPawn::OnMouseY(float V)
{
	// Unreal MouseY is positive when moving up; TS used movementY (down positive) with pitch = -stick.y
	Stick.Y = Clamp(Stick.Y - V * 0.012 * MouseSensitivity * (bInvertY ? -1 : 1), -1, 1);
}
void AShipPawn::OnMouseLock()
{
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		FInputModeGameOnly M;
		PC->SetInputMode(M);
		PC->bShowMouseCursor = false;
	}
}
void AShipPawn::OnFullStop() { Throttle = 0; LastThrottleInput = Time; }
void AShipPawn::OnCruiseThrottle() { Throttle = 0.75; LastThrottleInput = Time; }
void AShipPawn::OnToggleFA()
{
	bFlightAssist = !bFlightAssist;
	Msg(bFlightAssist ? TEXT("Flight assist ON") : TEXT("Flight assist OFF"));
}

void AShipPawn::Msg(const FString& Text)
{
	Messages.Add(Text);
	if (Messages.Num() > 40) Messages.RemoveAt(0);
	UE_LOG(LogOmniSpace, Log, TEXT("[ship] %s"), *Text);
}

void AShipPawn::AttachDirector(ASystemDirector* InDirector) { Director = InDirector; }

void AShipPawn::Place(const FVector& UniversePosM, const FVector& LookAtM)
{
	if (Director) Director->SetShipUniversePos(UniversePosM);
	FVector Dir = (LookAtM - UniversePosM).GetSafeNormal();
	Orientation = FRotationMatrix::MakeFromXZ(Dir, FVector::UpVector).ToQuat();
	SetActorRotation(Orientation);
	Velocity = FVector::ZeroVector;
	AngVel = FVector::ZeroVector;
}

AStationActor* AShipPawn::FindStation(const FString& Id) const
{
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AStationActor::StaticClass(), Found);
	for (AActor* A : Found)
		if (AStationActor* S = Cast<AStationActor>(A))
			if (S->Station.Id == Id) return S;
	return nullptr;
}

double AShipPawn::CurrentSpeed() const
{
	return Mode == EFlightMode::Supercruise ? SupercruiseSpeed : Velocity.Size();
}

bool AShipPawn::IsMassLocked() const
{
	if (!Director) return false;
	for (const FString& Id : Director->GetEntityIds())
	{
		FVector P; double R;
		if (!Director->GetEntityPos(Id, P, R)) continue;
		if (R <= 4000 && FVector::Dist(P, Director->GetShipUniversePos()) < MASS_LOCK && FindStation(Id)) return true;
	}
	return false;
}

void AShipPawn::SetTarget(const FString& Id)
{
	TargetId = Id;
	if (!Id.IsEmpty()) Msg(FString::Printf(TEXT("Target: %s"), *Id));
}

void AShipPawn::CycleTarget()
{
	if (!Director) return;
	TArray<FString> Ids = Director->GetEntityIds();
	if (Ids.Num() == 0) return;
	const FVector Ship = Director->GetShipUniversePos();
	Ids.Sort([&](const FString& A, const FString& B)
	{
		FVector PA, PB; double RA, RB;
		Director->GetEntityPos(A, PA, RA);
		Director->GetEntityPos(B, PB, RB);
		return FVector::Dist(PA, Ship) < FVector::Dist(PB, Ship);
	});
	int32 I = Ids.IndexOfByKey(TargetId);
	SetTarget(Ids[(I + 1) % Ids.Num()]);
}

void AShipPawn::CancelFsd(const FString& Text)
{
	FsdKind = EFsdKind::None;
	FsdTime = 0;
	Msg(Text);
}

void AShipPawn::ToggleSupercruise()
{
	if (FsdKind == EFsdKind::Supercruise) { CancelFsd(TEXT("Supercruise cancelled")); return; }
	if (Mode == EFlightMode::Supercruise) { DropFromSupercruise(false); return; }
	if (Mode != EFlightMode::Normal) return;
	if (IsMassLocked()) { Msg(TEXT("Mass locked - move away from the station")); return; }
	FsdKind = EFsdKind::Supercruise;
	FsdTime = 0;
	FsdDuration = 3;
	Msg(TEXT("Frame shift drive charging"));
}

void AShipPawn::RequestDocking()
{
	if (!Director || Mode != EFlightMode::Normal) return;
	const FVector Ship = Director->GetShipUniversePos();
	FString Best;
	double BestD = DOCK_RANGE;
	for (const FString& Id : Director->GetEntityIds())
	{
		FVector P; double R;
		if (!Director->GetEntityPos(Id, P, R)) continue;
		AStationActor* St = FindStation(Id);
		if (!St) continue;
		double D = FVector::Dist(P, Ship);
		if (D < BestD || (Id == TargetId && D < DOCK_RANGE)) { BestD = D; Best = Id; }
	}
	if (Best.IsEmpty()) { Msg(TEXT("No station within 7.5 km")); return; }
	AStationActor* St = FindStation(Best);
	DockingStationId = Best;
	DockingBay = St->BayPositions.Num() ? FMath::RandRange(0, St->BayPositions.Num() - 1) : 0;
	TargetId = Best;
	Msg(FString::Printf(TEXT("%s: docking granted, bay %d"), *St->Station.Name, DockingBay + 1));
}

void AShipPawn::DockAt(const FString& StationId, int32 Bay)
{
	Mode = EFlightMode::Docked;
	Velocity = FVector::ZeroVector;
	AngVel = FVector::ZeroVector;
	Throttle = 0;
	DockingStationId = StationId;
	DockingBay = Bay;
	Msg(TEXT("Docked. Press L to launch."));
}

void AShipPawn::Launch()
{
	if (Mode != EFlightMode::Docked) return;
	Mode = EFlightMode::Launching;
	LaunchTime = 0;
	Msg(TEXT("Launching"));
}

void AShipPawn::DropFromSupercruise(bool bAuto)
{
	Mode = EFlightMode::Normal;
	SupercruiseSpeed = 0;
	bool bPlaced = false;
	if (Director && !TargetId.IsEmpty())
	{
		FVector TP; double TR;
		if (Director->GetEntityPos(TargetId, TP, TR))
		{
			const FVector Ship = Director->GetShipUniversePos();
			double D = FVector::Dist(TP, Ship);
			if (FindStation(TargetId) && D < 120000.0)
			{
				FVector Dir = (Ship - TP).GetSafeNormal();
				Director->SetShipUniversePos(TP + Dir * 7000.0);
				Orientation = FRotationMatrix::MakeFromXZ(-Dir, FVector::UpVector).ToQuat();
				bPlaced = true;
				Msg(FString::Printf(TEXT("Arrived at %s"), *TargetId));
			}
		}
	}
	if (!bPlaced && !bAuto) Msg(TEXT("Supercruise disengaged"));
	Velocity = Orientation.GetForwardVector() * (bPlaced ? 120.0 : 200.0);
	Throttle = bPlaced ? 0.35 : 0.5;
}

void AShipPawn::Rotate(float Dt, double Pitch, double Yaw, double Roll, const FVector& Rates)
{
	double Blue = (Mode == EFlightMode::Normal) ? ((Throttle > 0.45 && Throttle < 0.8) ? 1.0 : 0.8) : 1.0;
	FVector Target(Pitch * Rates.X * Blue, Yaw * Rates.Y * Blue, Roll * Rates.Z);
	if (bFlightAssist || Mode != EFlightMode::Normal)
		AngVel = FMath::Lerp(AngVel, Target, 1.0 - FMath::Exp(-Dt * 5.0));
	else
	{
		const double Acc = 2.2;
		if (FMath::Abs(Pitch) > 0.02) AngVel.X += Clamp(Target.X - AngVel.X, -Acc * Dt, Acc * Dt);
		if (FMath::Abs(Yaw) > 0.02) AngVel.Y += Clamp(Target.Y - AngVel.Y, -Acc * Dt, Acc * Dt);
		if (FMath::Abs(Roll) > 0.02) AngVel.Z += Clamp(Target.Z - AngVel.Z, -Acc * Dt, Acc * Dt);
	}
	// Local axes: X forward, Y right, Z up. Pitch about Y, yaw about Z, roll about X.
	// Signs follow FRotator conventions: +pitch nose up, +yaw turn right, +roll right wing down.
	FQuat DQ = FQuat(FVector::RightVector, AngVel.X * Dt) * FQuat(FVector::UpVector, AngVel.Y * Dt) * FQuat(FVector::ForwardVector, AngVel.Z * Dt);
	Orientation = (Orientation * DQ).GetNormalized();
	SetActorRotation(Orientation);
}

void AShipPawn::FlyNormal(float Dt)
{
	// stick self-centres so the mouse behaves like relative aim
	double K = FMath::Exp(-Dt * 1.6);
	Stick *= K;
	double Pitch = Clamp(-Stick.Y + InPitch, -1, 1);
	double Yaw = Clamp(Stick.X + InYaw, -1, 1);
	Rotate(Dt, Pitch, Yaw, InRoll, FVector(0.85, 0.5, 1.7));

	if (bFlightAssist && BoostTimer <= 0 && Time - LastThrottleInput > 0.8 && Throttle != 0)
	{
		double Step = 0.22 * Dt;
		Throttle = FMath::Abs(Throttle) <= Step ? 0 : Throttle - FMath::Sign(Throttle) * Step;
	}
	if (bBoostHeld && BoostCharge >= 1 && BoostTimer <= 0) { BoostTimer = 3; BoostCharge = 0; }
	if (BoostTimer > 0) BoostTimer -= Dt; else BoostCharge = FMath::Min(1.0, BoostCharge + Dt / 6.0);
	double MaxV = BoostTimer > 0 ? BoostSpeed : MaxSpeed;
	FVector Local = Orientation.UnrotateVector(Velocity); // X fwd, Y right, Z up
	double FwdTarget = BoostTimer > 0 ? BoostSpeed : Throttle * MaxSpeed;
	if (bFlightAssist)
	{
		double Ax = 90 * Dt;
		Local.Y += Clamp(InStrafe * 100 - Local.Y, -Ax, Ax);
		Local.Z += Clamp(InVert * 100 - Local.Z, -Ax, Ax);
		Local.X += Clamp(FwdTarget - Local.X, -150 * Dt, (BoostTimer > 0 ? 260 : 150) * Dt);
	}
	else
	{
		Local.Y += InStrafe * 90 * Dt;
		Local.Z += InVert * 90 * Dt;
		Local.X += (BoostTimer > 0 ? 260 : Throttle * 150) * Dt;
		double Sp = Local.Size();
		if (Sp > MaxV) Local *= MaxV / Sp;
	}
	Velocity = Orientation.RotateVector(Local);
	FVector Move = Velocity * Dt;
	Collide(Dt, Move);
	if (Director) Director->MoveShip(Velocity, Dt);

	// docking trigger
	if (!DockingStationId.IsEmpty())
	{
		if (AStationActor* St = FindStation(DockingStationId))
		{
			FVector BayPos, BayN;
			St->GetBayWorld(DockingBay, BayPos, BayN);
			FVector Mouth = BayPos + BayN * 6000.0; // cm
			double D = Mouth.Size() / 100.0;       // ship is at origin
			if (D < 160 && Velocity.Size() < 140)
			{
				Mode = EFlightMode::Docking;
				DockT = 0;
				DockFrom = -St->GetActorLocation() / 100.0; // ship relative to station, m
				DockFromQ = Orientation;
				Msg(TEXT("Docking computer engaged"));
			}
		}
	}
}

void AShipPawn::Collide(float Dt, const FVector& MoveM)
{
	UWorld* World = GetWorld();
	if (!World || MoveM.IsNearlyZero()) return;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ShipSweep), false, this);
	const FVector Start = FVector::ZeroVector;
	const FVector End = MoveM * 100.0;
	if (World->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_WorldDynamic, FCollisionShape::MakeSphere(ShipRadiusM * 100.0), Params))
	{
		FVector N = Hit.ImpactNormal;
		if (Director) Director->SetShipUniversePos(Director->GetShipUniversePos() + N * (ShipRadiusM - Hit.Distance / 100.0 + 1.0) * 0.0);
		Impact(N);
		// stop motion into the surface this frame
		double Vn = FVector::DotProduct(Velocity, N);
		if (Vn < 0) Velocity -= N * Vn;
	}
}

void AShipPawn::Impact(const FVector& N, double Mult)
{
	double Vn = FVector::DotProduct(Velocity, N);
	if (Vn >= 0) return;
	double Speed = -Vn;
	Velocity -= N * Vn * 1.4;
	Velocity *= 0.6;
	if (Speed < 15 || Time - LastImpact < 0.6) return;
	LastImpact = Time;
	Damage(Speed * 0.0022 * Mult);
}

void AShipPawn::Damage(double Amount)
{
	LastHit = Time;
	double Absorbed = FMath::Min(Shields, Amount);
	Shields -= Absorbed;
	double Rest = Amount - Absorbed;
	if (Rest > 0) Hull = FMath::Max(0.0, Hull - Rest);
	if (Amount > 0.02) Msg(Rest > 0 ? FString::Printf(TEXT("Hull damage! %.0f%%"), Hull * 100) : TEXT("Shields absorbing impact"));
	if (Hull <= 0 && Mode != EFlightMode::Destroyed) { Mode = EFlightMode::Destroyed; Msg(TEXT("Ship destroyed")); }
}

void AShipPawn::FlySupercruise(float Dt)
{
	if (!Director) return;
	double K = FMath::Exp(-Dt * 1.6);
	Stick *= K;
	Rotate(Dt, Clamp(-Stick.Y + InPitch, -1, 1), Clamp(Stick.X + InYaw, -1, 1), InRoll, FVector(0.6, 0.38, 1.2));
	const FVector Ship = Director->GetShipUniversePos();
	double Cap = SupercruiseMax;
	FString NearId;
	double NearD = Director->NearestBody(NearId);
	// gravity wells
	for (const FString& Id : Director->GetEntityIds())
	{
		FVector P; double R;
		if (!Director->GetEntityPos(Id, P, R) || R < 5000) continue;
		double D = FVector::Dist(P, Ship) - R;
		double Well = SupercruiseMin + FMath::Max(0.0, D) * (0.3 + 0.5 * FMath::Min(1.0, 6e6 / R));
		Cap = FMath::Min(Cap, Well);
	}
	// approach cap toward the target
	double TargetDist = TNumericLimits<double>::Max();
	bool bTargetStation = false;
	if (!TargetId.IsEmpty())
	{
		FVector TP; double TR;
		if (Director->GetEntityPos(TargetId, TP, TR))
		{
			TargetDist = FVector::Dist(TP, Ship);
			bTargetStation = FindStation(TargetId) != nullptr;
			double Surf = bTargetStation ? TargetDist : TargetDist - TR;
			double Facing = FVector::DotProduct(Orientation.GetForwardVector(), (TP - Ship).GetSafeNormal());
			if (Facing > 0.6) Cap = FMath::Min(Cap, SupercruiseMin + FMath::Max(0.0, Surf) / 5.5);
		}
	}
	double TargetSpeed = FMath::Max(SupercruiseMin, Throttle * Cap);
	if (TargetSpeed > SupercruiseSpeed) SupercruiseSpeed = FMath::Min(TargetSpeed, SupercruiseSpeed * FMath::Exp(Dt * 1.05) + 4000 * Dt);
	else SupercruiseSpeed = TargetSpeed + (SupercruiseSpeed - TargetSpeed) * FMath::Exp(-Dt / 0.9);
	Velocity = Orientation.GetForwardVector() * SupercruiseSpeed;
	Director->MoveShip(Velocity, Dt);
	if (bTargetStation && TargetDist < 60000.0) DropFromSupercruise(true);
	// emergency drop near a body
	FVector NP; double NR;
	if (!NearId.IsEmpty() && Director->GetEntityPos(NearId, NP, NR) && NearD < FMath::Max(NR * 0.04, 30000.0))
	{
		Msg(FString::Printf(TEXT("Emergency drop! Too close to %s"), *NearId));
		Damage(0.12);
		DropFromSupercruise(true);
		FVector N = (Director->GetShipUniversePos() - NP).GetSafeNormal();
		Director->SetShipUniversePos(NP + N * (NR + FMath::Max(NR * 0.05, 40000.0)));
	}
	// heat near the star
	double StarR = Director->GetSystem().Star.RadiusSolar * 695700000.0;
	double Ratio = Ship.Size() / StarR;
	Heat = Clamp(0.2 + FMath::Max(0.0, 3.0 - Ratio) * 0.35, 0, 1.2);
	if (Ratio < 4 && Fuel < 16) Fuel = FMath::Min(16.0, Fuel + 0.9 * FMath::Pow(1.2 / FMath::Max(Ratio, 1.2), 2) * Dt);
}

void AShipPawn::FlyDocking(float Dt)
{
	AStationActor* St = FindStation(DockingStationId);
	if (!St || !Director) { Mode = EFlightMode::Normal; return; }
	DockT += Dt / 6.0;
	double K = DockT * DockT * (3 - 2 * DockT);
	FVector BayPos, BayN;
	St->GetBayWorld(DockingBay, BayPos, BayN);
	FVector StationU; double R;
	Director->GetEntityPos(DockingStationId, StationU, R);
	// bay in universe space (metres) = station + (bay world - station world)/100
	FVector BayU = StationU + (BayPos - St->GetActorLocation()) / 100.0;
	FVector EndU = BayU + BayN * 25.0;
	FVector StartU = StationU + DockFrom;
	Director->SetShipUniversePos(FMath::Lerp(StartU, EndU, FMath::Min(1.0, K)));
	FQuat EndQ = FRotationMatrix::MakeFromXZ(BayN, FVector::UpVector).ToQuat();
	Orientation = FQuat::Slerp(DockFromQ, EndQ, FMath::Min(1.0, K));
	SetActorRotation(Orientation);
	if (DockT >= 1) DockAt(DockingStationId, DockingBay);
}

void AShipPawn::FlyLaunch(float Dt)
{
	AStationActor* St = FindStation(DockingStationId);
	if (!St || !Director) { Mode = EFlightMode::Normal; return; }
	LaunchTime += Dt;
	FVector BayPos, BayN;
	St->GetBayWorld(DockingBay, BayPos, BayN);
	FVector StationU; double R;
	Director->GetEntityPos(DockingStationId, StationU, R);
	FVector BayU = StationU + (BayPos - St->GetActorLocation()) / 100.0;
	Orientation = FRotationMatrix::MakeFromXZ(BayN, FVector::UpVector).ToQuat();
	SetActorRotation(Orientation);
	Director->SetShipUniversePos(BayU + BayN * (25.0 + LaunchTime * LaunchTime * 40.0));
	Velocity = BayN * LaunchTime * 80.0;
	if (LaunchTime > 3.5)
	{
		Mode = EFlightMode::Normal;
		DockingStationId.Empty();
		Throttle = 0.3;
		LastThrottleInput = Time;
		Msg(TEXT("Launch complete. You have control."));
	}
}

void AShipPawn::Tick(float Dt)
{
	Super::Tick(Dt);
	Time += Dt;
	if (!Director) return;
	Dt = FMath::Min(Dt, 0.1f);
	InPitch = KPitchUp - KPitchDown;
	InYaw = KYawR - KYawL;
	InRoll = KRollR - KRollL;
	InStrafe = KStrafeR - KStrafeL;
	InVert = KVertUp - KVertDown;
	InThrottle = KThrUp - KThrDown;
	if (InThrottle != 0)
	{
		Throttle = Clamp(Throttle + InThrottle * Dt * 0.75, Mode == EFlightMode::Supercruise ? 0 : -0.5, 1);
		LastThrottleInput = Time;
	}
	// FSD charge
	if (FsdKind != EFsdKind::None)
	{
		FsdTime += Dt;
		FsdProgress = FMath::Min(1.0, FsdTime / FsdDuration);
		if (FsdTime >= FsdDuration && FsdKind == EFsdKind::Supercruise)
		{
			FsdKind = EFsdKind::None;
			Mode = EFlightMode::Supercruise;
			SupercruiseSpeed = FMath::Max(SupercruiseMin, Velocity.Size());
			Msg(TEXT("Supercruise engaged"));
		}
	}
	else FsdProgress = 0;

	switch (Mode)
	{
	case EFlightMode::Normal: FlyNormal(Dt); break;
	case EFlightMode::Supercruise: FlySupercruise(Dt); break;
	case EFlightMode::Docking: FlyDocking(Dt); break;
	case EFlightMode::Launching: FlyLaunch(Dt); break;
	case EFlightMode::Docked:
	{
		if (AStationActor* St = FindStation(DockingStationId))
		{
			FVector BayPos, BayN;
			St->GetBayWorld(DockingBay, BayPos, BayN);
			FVector StationU; double R;
			Director->GetEntityPos(DockingStationId, StationU, R);
			Director->SetShipUniversePos(StationU + (BayPos - St->GetActorLocation()) / 100.0 + BayN * 25.0);
			Orientation = FRotationMatrix::MakeFromXZ(BayN, FVector::UpVector).ToQuat();
			SetActorRotation(Orientation);
		}
		break;
	}
	default: break;
	}
	if (Time - LastHit > 4) Shields = FMath::Min(1.0, Shields + Dt * 0.04);
	SetActorLocation(FVector::ZeroVector);
}
