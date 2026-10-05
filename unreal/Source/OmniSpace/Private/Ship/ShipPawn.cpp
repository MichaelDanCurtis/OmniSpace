#include "Ship/ShipPawn.h"
#include "Ship/Cockpit.h"
#include "Core/OmniGameMode.h"
#include "Galaxy/GalaxySubsystem.h"
#include "World/OmniAssets.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "World/SystemDirector.h"
#include "World/StationActor.h"
#include "OmniSpace.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Engine/EngineTypes.h"
#include "Engine/HitResult.h"
#include "InputCoreTypes.h"

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
	// Space is mostly black, so histogram auto-exposure cranks the gain and blows out anything lit.
	// Pin the exposure to a fixed EV100 tuned for the 8 lux "sun" the director spawns.
	FPostProcessSettings& PP = Camera->PostProcessSettings;
	PP.bOverride_AutoExposureMinBrightness = true;
	PP.bOverride_AutoExposureMaxBrightness = true;
	PP.AutoExposureMinBrightness = 1.0f;
	PP.AutoExposureMaxBrightness = 1.0f;
	PP.bOverride_AutoExposureBias = true;
	PP.AutoExposureBias = 0.0f;
	PP.bOverride_BloomIntensity = true;
	PP.BloomIntensity = 0.35f;
	AutoPossessPlayer = EAutoReceiveInput::Player0;
}

void AShipPawn::BeginPlay()
{
	Super::BeginPlay();
	Orientation = GetActorQuat();
	OmniCockpit::Build(this, Root);
	// hyperspace tunnel: a long cylinder around the camera along +X, shown only while jumping
	Tunnel = NewObject<UStaticMeshComponent>(this);
	Tunnel->SetStaticMesh(OmniAssets::CylinderMesh());
	Tunnel->SetupAttachment(Root);
	Tunnel->SetRelativeRotation(FRotator(90, 0, 0)); // cylinder axis Z -> X
	Tunnel->SetRelativeScale3D(FVector(600, 600, 6000)); // 600 m across, 6 km long
	Tunnel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Tunnel->SetCastShadow(false);
	Tunnel->SetVisibility(false);
	Tunnel->RegisterComponent();
	TunnelMID = OmniAssets::MakeMID(Tunnel, OmniAssets::MaterialOr(TEXT("/Game/Materials/M_Tunnel.M_Tunnel"), TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial")), this);
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->bShowMouseCursor = false;
		FInputModeGameOnly InputMode; // not "Mode": that would shadow the EFlightMode member
		PC->SetInputMode(InputMode);
	}
}

ASystemDirector* AShipPawn::GetDirector() const { return Director; }

void AShipPawn::SetupPlayerInputComponent(UInputComponent* IC)
{
	Super::SetupPlayerInputComponent(IC);
	// Keyboard (legacy key bindings: no asset setup required). Keyboard keys are not 1D axes,
	// so BindAxisKey ensures on them; bind press/release instead and treat the key as a held axis.
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
	BindHold(EKeys::Up, KPitchUp);
	BindHold(EKeys::Down, KPitchDown);
	BindHold(EKeys::Left, KYawL);
	BindHold(EKeys::Right, KYawR);
	BindHold(EKeys::A, KRollL);
	BindHold(EKeys::D, KRollR);
	BindHold(EKeys::Q, KStrafeL);
	BindHold(EKeys::E, KStrafeR);
	BindHold(EKeys::R, KVertUp);
	BindHold(EKeys::F, KVertDown);
	BindHold(EKeys::W, KThrUp);
	BindHold(EKeys::S, KThrDown);
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
	IC->BindKey(EKeys::P, IE_Pressed, this, &AShipPawn::OnToggleAutopilot);
	IC->BindKey(EKeys::N, IE_Pressed, this, &AShipPawn::OnCycleJump);
	IC->BindKey(EKeys::F5, IE_Pressed, this, &AShipPawn::OnDebugSky);
	IC->BindKey(EKeys::F6, IE_Pressed, this, &AShipPawn::OnDebugStar);
	IC->BindKey(EKeys::F7, IE_Pressed, this, &AShipPawn::OnDebugBodies);
	IC->BindKey(EKeys::F8, IE_Pressed, this, &AShipPawn::OnDebugCockpit);
	IC->BindKey(EKeys::H, IE_Pressed, this, &AShipPawn::OnEngageJump);
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
	if (!Id.IsEmpty()) JumpTargetId.Empty();
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

void AShipPawn::ToggleAutopilot()
{
	if (!bAutopilot && TargetId.IsEmpty() && JumpTargetId.IsEmpty())
	{
		Msg(TEXT("Autopilot needs a target (T) or a jump destination (N)"));
		return;
	}
	bAutopilot = !bAutopilot;
	ApStage = 0;
	Stick = FVector2D::ZeroVector; // residual mouse motion must not count as a manual override
	Msg(bAutopilot ? TEXT("Autopilot engaged") : TEXT("Autopilot disengaged"));
}

bool AShipPawn::AutopilotControls(FApControls& Out)
{
	if (!Director) return false;
	if (TargetId.IsEmpty() || FsdKind == EFsdKind::Jump)
	{
		// no local target: align with the jump destination and engage
		FVector Dir;
		if (!JumpDirection(Dir)) return false;
		const FVector L = Orientation.UnrotateVector(Dir);
		const double YawE = FMath::Atan2(L.Y, L.X), PitchE = FMath::Atan2(L.Z, FMath::Sqrt(L.X * L.X + L.Y * L.Y));
		const bool bLocked = Mode == EFlightMode::Normal && IsMassLocked();
		if (FMath::Sqrt(YawE * YawE + PitchE * PitchE) < 0.08 && FsdKind == EFsdKind::None && !bLocked && !EngageJump()) return false;
		Out.Pitch = Clamp(PitchE * 3.0, -1, 1);
		Out.Yaw = Clamp(YawE * 3.0, -1, 1);
		Out.Throttle = bLocked ? 1.0 : (Mode == EFlightMode::Supercruise ? 0.0 : 0.5);
		return true;
	}
	FVector TP; double TR;
	if (!Director->GetEntityPos(TargetId, TP, TR)) return false;
	const FVector Ship = Director->GetShipUniversePos();
	const double Dist = FVector::Dist(TP, Ship);
	AStationActor* St = FindStation(TargetId);
	const bool bStar = TargetId == TEXT("star");
	const bool bBody = !St && !bStar;
	FVector AimDir = (TP - Ship).GetSafeNormal(); // world axes (universe directions == world directions)
	double ThrottleOut = 0.75;
	bool bEngageSC = false, bDropSC = false;
	if (Mode == EFlightMode::Supercruise)
	{
		if (bBody && Dist - TR < TR * 2.5) bDropSC = true;
		if (bStar && Dist < TR * 6) bDropSC = true;
		if (bBody)
		{
			// aim at the limb so we glide past rather than into it
			FVector ToT = TP - Ship;
			FVector Side = FVector::CrossProduct(ToT, FVector::UpVector).GetSafeNormal();
			AimDir = (ToT + Side * TR * 1.6).GetSafeNormal();
		}
	}
	else
	{
		if (St)
		{
			if (Dist < DOCK_RANGE && DockingStationId.IsEmpty()) RequestDocking();
			if (DockingStationId == TargetId)
			{
				FVector BayPos, BayN; // world cm, ship at origin
				St->GetBayWorld(DockingBay, BayPos, BayN);
				const FVector P1 = BayPos + BayN * 180000.0;
				const FVector P2 = BayPos + BayN * 30000.0;
				const FVector P3 = BayPos + BayN * 5500.0;
				if (ApStage == 0 && P1.Size() < 35000.0) ApStage = 1;
				if (ApStage == 1 && P2.Size() < 12000.0) ApStage = 2;
				FVector Aim = ApStage == 0 ? P1 : ApStage == 1 ? P2 : P3;
				if (ApStage == 0)
				{
					// steer around the station if the straight line to the approach point crosses the hull
					FHitResult Hit;
					FCollisionQueryParams Params(SCENE_QUERY_STAT(ApTrace), false, this);
					if (GetWorld()->SweepSingleByChannel(Hit, FVector::ZeroVector, P1, FQuat::Identity, ECC_WorldDynamic, FCollisionShape::MakeSphere(ShipRadiusM * 100.0 * 4.0), Params) && Hit.GetActor() == St)
					{
						const FVector StW = St->GetActorLocation();
						FVector Rel = -StW; // ship relative to station
						FVector Perp = Rel - BayN * FVector::DotProduct(Rel, BayN);
						if (Perp.SizeSquared() < 1.0) Perp = FVector::CrossProduct(BayN, FMath::Abs(BayN.Z) < 0.9 ? FVector::UpVector : FVector::ForwardVector);
						Aim = StW + Perp.GetSafeNormal() * (St->BoundRadiusM + 600.0) * 100.0;
					}
				}
				const double D = Aim.Size() / 100.0; // m
				ThrottleOut = Clamp(D / 1200.0, 0.12, ApStage == 0 ? 1.0 : 0.4);
				AimDir = Aim.GetSafeNormal();
			}
			else ThrottleOut = Clamp(Dist / 20000.0, 0.2, 1.0);
			if (Dist > 40000.0 && !IsMassLocked()) bEngageSC = true;
		}
		else
		{
			if (Dist > 30000.0) bEngageSC = true;
			ThrottleOut = Clamp(Dist / 8000.0, 0.0, 1.0);
			if (bStar && Dist < TR * 8) { bEngageSC = false; ThrottleOut = 0; }
			if (bBody && Dist - TR < TR * 3) { bEngageSC = false; ThrottleOut = 0; }
		}
	}
	const FVector Local = Orientation.UnrotateVector(AimDir); // X fwd, Y right, Z up
	const double YawErr = FMath::Atan2(Local.Y, Local.X);
	const double PitchErr = FMath::Atan2(Local.Z, FMath::Sqrt(Local.X * Local.X + Local.Y * Local.Y));
	const double Misalign = FMath::Sqrt(YawErr * YawErr + PitchErr * PitchErr);
	if (Misalign > 0.5 && Mode == EFlightMode::Normal) ThrottleOut = FMath::Min(ThrottleOut, 0.55);
	if (Misalign > 0.3) bEngageSC = false;
	Out.Pitch = Clamp(PitchErr * 3.0, -1, 1);
	Out.Yaw = Clamp(YawErr * 3.0, -1, 1);
	Out.Throttle = ThrottleOut;
	Out.bEngageSC = bEngageSC;
	Out.bDropSC = bDropSC;
	return true;
}

void AShipPawn::OnDebugSky() { if (Director) Director->DebugToggle(0); }
void AShipPawn::OnDebugStar() { if (Director) Director->DebugToggle(1); }
void AShipPawn::OnDebugBodies() { if (Director) Director->DebugToggle(2); }
void AShipPawn::OnDebugCockpit()
{
	TArray<USceneComponent*> Kids;
	Root->GetChildrenComponents(true, Kids);
	for (USceneComponent* K : Kids)
		if (K != Camera && K != Tunnel) K->SetHiddenInGame(!K->bHiddenInGame);
	Msg(TEXT("debug: cockpit toggled"));
}

bool AShipPawn::JumpDirection(FVector& OutDir) const
{
	AOmniGameMode* GM = Cast<AOmniGameMode>(GetWorld()->GetAuthGameMode());
	if (!GM || JumpTargetId.IsEmpty()) return false;
	// galaxy positions are TS-style (x, yUp, zFwd); the system frame maps that to UE (x, zFwd, yUp)
	FVector D = JumpTarget.Pos - GM->CurrentStar.Pos;
	OutDir = FVector(D.X, D.Z, D.Y).GetSafeNormal();
	return !OutDir.IsNearlyZero();
}

double AShipPawn::JumpDistanceLy() const
{
	AOmniGameMode* GM = Cast<AOmniGameMode>(GetWorld()->GetAuthGameMode());
	if (!GM || JumpTargetId.IsEmpty()) return 0;
	return FVector::Dist(JumpTarget.Pos, GM->CurrentStar.Pos);
}

void AShipPawn::CycleJumpTarget()
{
	AOmniGameMode* GM = Cast<AOmniGameMode>(GetWorld()->GetAuthGameMode());
	UGalaxySubsystem* Galaxy = GetGameInstance()->GetSubsystem<UGalaxySubsystem>();
	if (!GM || !Galaxy) return;
	const FStarDescriptor& Cur = GM->CurrentStar;
	TArray<FStarDescriptor> Near = Galaxy->StarsNear(Cur.Pos, JumpRangeLy);
	Near.RemoveAll([&](const FStarDescriptor& S) { return S.Id == Cur.Id; });
	if (Near.Num() == 0) { Msg(TEXT("No stars within jump range")); return; }
	Near.Sort([&](const FStarDescriptor& A, const FStarDescriptor& B) { return FVector::DistSquared(A.Pos, Cur.Pos) < FVector::DistSquared(B.Pos, Cur.Pos); });
	int32 I = Near.IndexOfByPredicate([&](const FStarDescriptor& S) { return S.Id == JumpTargetId; });
	const FStarDescriptor& Next = Near[(I + 1) % Near.Num()];
	JumpTargetId = Next.Id;
	JumpTarget = Next;
	TargetId.Empty(); // navigation is one thing at a time: a jump destination or a local target
	Msg(FString::Printf(TEXT("Jump target: %s  %.1f ly  (%s)"), *Next.Name, FVector::Dist(Next.Pos, Cur.Pos), *UGalaxySubsystem::ClassLabel(Next.Cls)));
	GM->PrefetchJump(Next); // start the Forge early so the jump is quick
}

bool AShipPawn::EngageJump()
{
	if (Mode != EFlightMode::Normal && Mode != EFlightMode::Supercruise) return false;
	if (JumpTargetId.IsEmpty()) { Msg(TEXT("No hyperspace destination (press N to pick a nearby star)")); return false; }
	if (FsdKind != EFsdKind::None) { CancelFsd(TEXT("FSD charge cancelled")); return false; }
	const double D = JumpDistanceLy();
	if (D > JumpRangeLy) { Msg(FString::Printf(TEXT("%s is out of range (%.1f ly > %.1f ly)"), *JumpTarget.Name, D, JumpRangeLy)); return false; }
	const double Need = 0.35 + 3.4 * FMath::Square(FMath::Min(1.0, D / JumpRangeLy));
	if (Fuel < Need) { Msg(FString::Printf(TEXT("Insufficient fuel for jump (%.1f t needed)"), Need)); return false; }
	if (Mode == EFlightMode::Normal && IsMassLocked()) { Msg(TEXT("Mass locked - move away from the station")); return false; }
	FsdKind = EFsdKind::Jump;
	FsdTime = 0;
	FsdDuration = 5;
	FsdWaitAlign = 0;
	Msg(FString::Printf(TEXT("Frame shift drive charging - jump to %s"), *JumpTarget.Name));
	return true;
}

void AShipPawn::BeginHyperspace()
{
	AOmniGameMode* GM = Cast<AOmniGameMode>(GetWorld()->GetAuthGameMode());
	if (!GM) return;
	const double D = JumpDistanceLy();
	Fuel = FMath::Max(0.0, Fuel - (0.35 + 3.4 * FMath::Square(FMath::Min(1.0, D / JumpRangeLy))));
	Mode = EFlightMode::Hyperspace;
	HyperT = 0;
	bAutopilot = false;
	DockingStationId.Empty();
	TargetId.Empty();
	Velocity = FVector::ZeroVector;
	AngVel = FVector::ZeroVector;
	if (Tunnel) { Tunnel->SetHiddenInGame(false); Tunnel->SetVisibility(true); }
	if (TunnelMID) TunnelMID->SetVectorParameterValue(TEXT("Color"), JumpTarget.LinearColor());
	Msg(FString::Printf(TEXT("Hyperspace jump - %s - %.2f ly"), *JumpTarget.Name, D));
	GM->BeginJump(JumpTarget);
}

void AShipPawn::FlyHyperspace(float Dt)
{
	AOmniGameMode* GM = Cast<AOmniGameMode>(GetWorld()->GetAuthGameMode());
	HyperT += Dt;
	if (TunnelMID) TunnelMID->SetScalarParameterValue(TEXT("Time"), static_cast<float>(HyperT));
	// gentle drift so the tunnel feels alive
	Orientation = (Orientation * FQuat(FVector::ForwardVector, 0.03 * Dt)).GetNormalized();
	SetActorRotation(Orientation);
	if (!GM) return;
	const bool bRevealDone = HyperT > 0.8 + GM->JumpLog.Num() * 0.55;
	if (GM->JumpFailed())
	{
		Msg(TEXT("Jump failed - the Forge could not chart the system. Frame shift reverted."));
		if (Tunnel) Tunnel->SetVisibility(false);
		JumpTargetId.Empty();
		GM->AbortJump(); // re-enters the system we left, in supercruise
		return;
	}
	if (GM->IsJumpReady() && HyperT > 6.5 && (bRevealDone || HyperT > 14))
	{
		if (Tunnel) Tunnel->SetVisibility(false);
		JumpTargetId.Empty();
		GM->CompleteJump(); // -> EnterSystem(..., true): places us near the star in supercruise
	}
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
	// FQuat(Axis, Angle) is a right-handed rotation, so about +Y a positive angle is nose DOWN and
	// about +X it is left wing down; negate those so +pitch = nose up, +roll = right wing down,
	// +yaw = turn right (FRotator conventions, verified: FQuat(RightVector,a).Rotator().Pitch == -a).
	FQuat DQ = FQuat(FVector::RightVector, -AngVel.X * Dt) * FQuat(FVector::UpVector, AngVel.Y * Dt) * FQuat(FVector::ForwardVector, -AngVel.Z * Dt);
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
	if (Ratio < 4 && Fuel < 16) Fuel = FMath::Min(16.0, Fuel + 0.9 * FMath::Pow(1.2 / FMath::Max(Ratio, 1.2), 2.0) * Dt);
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
	// autopilot: manual override disengages; otherwise it drives pitch/yaw/throttle
	if (bAutopilot && Mode != EFlightMode::Docking && FMath::Abs(Stick.X) + FMath::Abs(Stick.Y) + FMath::Abs(InRoll) + FMath::Abs(InPitch) + FMath::Abs(InYaw) > 0.4)
	{
		bAutopilot = false;
		Msg(TEXT("Autopilot disengaged - manual override"));
	}
	if (bAutopilot && (Mode == EFlightMode::Normal || Mode == EFlightMode::Supercruise))
	{
		FApControls Ap;
		if (AutopilotControls(Ap))
		{
			Stick = FVector2D::ZeroVector;
			InPitch = Ap.Pitch;
			InYaw = Ap.Yaw;
			InRoll = InStrafe = InVert = 0;
			bBoostHeld = false;
			Throttle = Ap.Throttle;
			LastThrottleInput = Time; // keep flight assist from bleeding the autopilot's throttle
			if (Ap.bEngageSC && FsdKind == EFsdKind::None && Mode == EFlightMode::Normal && !IsMassLocked()) ToggleSupercruise();
			if (Ap.bDropSC && Mode == EFlightMode::Supercruise) DropFromSupercruise(true);
		}
		else bAutopilot = false;
	}
	if (bAutopilot && (Mode == EFlightMode::Docked || Mode == EFlightMode::Docking)) bAutopilot = false;
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
		else if (FsdTime >= FsdDuration && FsdKind == EFsdKind::Jump)
		{
			FVector Dir;
			if (!JumpDirection(Dir)) FsdKind = EFsdKind::None;
			else if (FVector::DotProduct(Orientation.GetForwardVector(), Dir) > FMath::Cos(0.2))
			{
				FsdKind = EFsdKind::None;
				BeginHyperspace();
			}
			else
			{
				FsdWaitAlign += Dt;
				if (FsdWaitAlign > 20) CancelFsd(TEXT("Jump aborted - not aligned with destination"));
			}
		}
	}
	else FsdProgress = 0;

	switch (Mode)
	{
	case EFlightMode::Normal: FlyNormal(Dt); break;
	case EFlightMode::Supercruise: FlySupercruise(Dt); break;
	case EFlightMode::Hyperspace: FlyHyperspace(Dt); break;
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
	if (Tunnel && Mode != EFlightMode::Hyperspace && Tunnel->IsVisible()) { Tunnel->SetVisibility(false); Tunnel->SetHiddenInGame(true); }
	SetActorLocation(FVector::ZeroVector);
}
