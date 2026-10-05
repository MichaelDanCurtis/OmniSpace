#include "Core/OmniGameMode.h"
#include "Core/OmniHUD.h"
#include "Ship/ShipPawn.h"
#include "Ship/RoverPawn.h"
#include "World/SurfaceTile.h"
#include "World/SkyActor.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "World/SystemDirector.h"
#include "Forge/ForgeSubsystem.h"
#include "Galaxy/GalaxySubsystem.h"
#include "OmniSpace.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/WorldSettings.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"

AOmniGameMode::AOmniGameMode()
{
	DefaultPawnClass = AShipPawn::StaticClass();
	HUDClass = AOmniHUD::StaticClass();
}

void AOmniGameMode::BeginPlay()
{
	Super::BeginPlay();
	UWorld* World = GetWorld();
	if (AWorldSettings* WS = World->GetWorldSettings())
	{
		WS->bEnableWorldBoundsChecks = false;
		WS->KillZ = -1e12f; // KillZ is a float
	}
	FActorSpawnParameters P;
	Director = World->SpawnActor<ASystemDirector>(ASystemDirector::StaticClass(), FTransform::Identity, P);
	if (AShipPawn* Ship = Cast<AShipPawn>(UGameplayStatics::GetPlayerPawn(World, 0))) Ship->AttachDirector(Director);

	UGameInstance* GI = GetGameInstance();
	UForgeSubsystem* Forge = GI->GetSubsystem<UForgeSubsystem>();
	UGalaxySubsystem* Galaxy = GI->GetSubsystem<UGalaxySubsystem>();
	Forge->OnSystemLoaded.AddDynamic(this, &AOmniGameMode::OnSystemLoaded);
	Forge->OnLog.AddDynamic(this, &AOmniGameMode::OnForgeLog);
	Forge->OnSystemDressed.AddDynamic(this, &AOmniGameMode::OnSystemDressed);
	FString StartId = TEXT("kelvar");
	FParse::Value(FCommandLine::Get(), TEXT("StartSystem="), StartId);
	FStarDescriptor Star;
	if (!Galaxy->GetStar(StartId, Star)) Galaxy->GetStar(TEXT("kelvar"), Star);
	PendingSystemId = Star.Id;
	CurrentStar = Star;
	bPendingJump = false;
	Forge->LoadSystem(Star, NeighborsOf(Star), Commander);
}

TArray<FStarDescriptor> AOmniGameMode::NeighborsOf(const FStarDescriptor& Star) const
{
	UGalaxySubsystem* Galaxy = GetGameInstance()->GetSubsystem<UGalaxySubsystem>();
	TArray<FStarDescriptor> Near = Galaxy->StarsNear(Star.Pos, 25.0);
	Near.RemoveAll([&](const FStarDescriptor& S) { return S.Id == Star.Id; });
	Near.Sort([&](const FStarDescriptor& A, const FStarDescriptor& B) { return FVector::DistSquared(A.Pos, Star.Pos) < FVector::DistSquared(B.Pos, Star.Pos); });
	if (Near.Num() > 6) Near.SetNum(6);
	return Near;
}

void AOmniGameMode::PrefetchJump(const FStarDescriptor& Dest)
{
	UForgeSubsystem* Forge = GetGameInstance()->GetSubsystem<UForgeSubsystem>();
	FStarSystemData Tmp;
	if (!Forge->GetCached(Dest.Id, Tmp)) Forge->Prefetch(Dest, NeighborsOf(Dest), Commander);
}

void AOmniGameMode::BeginJump(const FStarDescriptor& Dest)
{
	UForgeSubsystem* Forge = GetGameInstance()->GetSubsystem<UForgeSubsystem>();
	PendingSystemId = Dest.Id;
	bPendingJump = true;
	bJumpReady = false;
	bJumpFailed = false;
	JumpLog.Reset();
	PreviousStar = CurrentStar;
	if (Director) PreviousSystem = Director->GetSystem();
	CurrentStar = Dest;
	if (Director) Director->Clear();
	Forge->LoadSystem(Dest, NeighborsOf(Dest), Commander);
}

void AOmniGameMode::CompleteJump()
{
	if (!bJumpReady) return;
	bJumpReady = false;
	bPendingJump = false;
	EnterSystem(ArrivedSystem, true);
}

void AOmniGameMode::AbortJump()
{
	bPendingJump = false;
	bJumpReady = false;
	bJumpFailed = false;
	CurrentStar = PreviousStar;
	PendingSystemId = PreviousStar.Id;
	if (!PreviousSystem.Id.IsEmpty()) EnterSystem(PreviousSystem, true);
}

void AOmniGameMode::OnSystemDressed(const FString& SystemId, const FStarSystemData& System)
{
	if (SystemId != CurrentStar.Id || !Director || bPendingJump) return;
	Director->ApplyDressing(System);
	if (AShipPawn* Ship = Cast<AShipPawn>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0)))
		Ship->Msg(FString::Printf(TEXT("Stellar Forge: %s chronicled - %s, %s"), *System.Name, *System.Profile.Faction, *System.Profile.Economy));
}

void AOmniGameMode::OnForgeLog(const FString& SystemId, const TArray<FString>& Lines)
{
	if (SystemId == PendingSystemId) JumpLog = Lines;
}

void AOmniGameMode::OnSystemLoaded(const FString& SystemId, bool bSuccess, const FStarSystemData& System)
{
	if (SystemId != PendingSystemId) return;
	if (!bSuccess)
	{
		UE_LOG(LogOmniSpace, Error, TEXT("Could not load system %s"), *SystemId);
		bJumpFailed = true;
		return;
	}
	if (bPendingJump)
	{
		// hold until the ship's hyperspace sequence is ready to drop out
		ArrivedSystem = System;
		bJumpReady = true;
		return;
	}
	EnterSystem(System, false);
}

void AOmniGameMode::EnterSystem(const FStarSystemData& System, bool bArrivingFromJump)
{
	if (!Director) return;
	Director->LoadSystem(System);
	AShipPawn* Ship = Cast<AShipPawn>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
	if (!Ship) return;
	if (!bArrivingFromJump && System.Stations.Num())
	{
		// concept-art framing: a few km from the first station, host planet to the left
		const FStationData& St = System.Stations[0];
		FVector StPos; double R;
		Director->GetEntityPos(St.Id, StPos, R);
		FVector HostPos = FVector::ZeroVector; double HR;
		Director->GetEntityPos(St.Parent, HostPos, HR);
		FVector ToHost = (HostPos - StPos).GetSafeNormal();
		FVector Side = FVector::CrossProduct(ToHost, FVector::UpVector).GetSafeNormal();
		FVector Pos = StPos - ToHost * 5200.0 - Side * 2400.0 + FVector(0, 0, 700.0);
		FVector Aim = StPos + (HostPos - StPos).GetSafeNormal() * 2000.0;
		Ship->Place(Pos, Aim);
		Ship->SetTarget(St.Id);
		Ship->Mode = EFlightMode::Normal;
	}
	else
	{
		// hyperspace arrival: near the star, in supercruise, star off to one side, throttle closed
		double StarR = System.Star.RadiusSolar * 695700000.0;
		FVector Dir = FVector(0.6, 0.75, 0.25).GetSafeNormal();
		double Range = FMath::Max(StarR * 9.0, 4e8);
		FVector Pos = Dir * Range;
		// keep clear of any body parked near the star (hot giants hug red dwarfs)
		for (int Try = 0; Try < 16; Try++)
		{
			bool bClear = true;
			for (const FString& Id : Director->GetEntityIds())
			{
				FVector BP; double BR;
				if (Id == TEXT("star") || !Director->GetEntityPos(Id, BP, BR)) continue;
				if (FVector::Dist(BP, Pos) < FMath::Max(BR * 8.0, 3e8)) { bClear = false; break; }
			}
			if (bClear) break;
			Dir = FQuat(FVector::UpVector, FMath::DegreesToRadians(37.0)).RotateVector(Dir);
			Range *= 1.15;
			Pos = Dir * Range;
		}
		FVector Side = FVector::CrossProduct(Dir, FVector::UpVector).GetSafeNormal();
		Ship->Place(Pos, Pos + Side * 1e9 - Dir * 4e8);
		Ship->Mode = EFlightMode::Supercruise;
		Ship->SupercruiseSpeed = 30000;
		Ship->Throttle = 0;
	}
	Ship->Msg(FString::Printf(TEXT("%s - %s - %s"), *System.Name, *System.Profile.Faction, *System.Profile.Economy));
}


// ---------------------------------------------------------------- landing tiles
void AOmniGameMode::EnterSurface(const FString& BodyId, const FVector2D& Site)
{
	AShipPawn* Ship = Cast<AShipPawn>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
	if (!Director || !Ship || Tile) return;
	const FBodyData* B = Director->GetSystem().FindBody(BodyId);
	if (!B) return;
	FVector BP; double BR;
	Director->GetEntityPos(BodyId, BP, BR);
	SurfaceBodyId = BodyId;
	SurfaceApproachDir = (Director->GetShipUniversePos() - BP).GetSafeNormal();
	Director->SetSystemHidden(true);
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Tile = GetWorld()->SpawnActor<ASurfaceTile>(ASurfaceTile::StaticClass(), FTransform::Identity, P);
	const FLinearColor SunC = Director->GetSystem().Star.LinearColor().Desaturate(0.4f);
	Tile->Build(*B, Site, SunC, 38.0);
	// sun: a fixed afternoon elevation from the site azimuth; the director re-aims it on return
	if (ADirectionalLight* Sun = Director->GetSunLight())
		Sun->SetActorRotation(FRotator(-38.0, Site.X * 360.0, 0));
	Ship->StartDescent(Tile);
	UE_LOG(LogOmniSpace, Log, TEXT("Surface: %s at site (%.2f, %.2f)"), *B->Name, Site.X, Site.Y);
}

void AOmniGameMode::LeaveSurface()
{
	AShipPawn* Ship = Cast<AShipPawn>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
	if (!Director || !Tile) return;
	if (Rover) { Rover->Destroy(); Rover = nullptr; }
	Tile->Destroy();
	Tile = nullptr;
	Director->SetSystemHidden(false);
	FVector BP; double BR;
	Director->GetEntityPos(SurfaceBodyId, BP, BR);
	if (Ship)
	{
		Ship->EndSurface();
		Ship->FlashAmount = 1.f;
		// back in orbit above the site, nose along the horizon, drifting outward
		const FVector Up = SurfaceApproachDir;
		const FVector Pos = BP + Up * (BR * 1.35 + 40000.0);
		const FVector Side = FVector::CrossProduct(Up, FVector::UpVector).GetSafeNormal();
		Ship->Place(Pos, Pos + Side * 1e6 + Up * 2e5);
		Ship->Mode = EFlightMode::Normal;
		Ship->Velocity = (Side * 0.9 + Up * 0.3).GetSafeNormal() * 150.0;
		Ship->Throttle = 0.4;
		Ship->Msg(FString::Printf(TEXT("Orbit regained above %s"), *Director->GetEntityName(SurfaceBodyId)));
	}
	SurfaceBodyId.Empty();
}

void AOmniGameMode::DeployRover()
{
	AShipPawn* Ship = Cast<AShipPawn>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!Ship || !Tile || !PC || Ship->Mode != EFlightMode::Landed) return;
	if (!Rover)
	{
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Rover = GetWorld()->SpawnActor<ARoverPawn>(ARoverPawn::StaticClass(), FTransform::Identity, P);
	}
	const FQuat Q = Ship->GetActorQuat();
	const FVector Drop = Ship->GetActorLocation() + Q.GetRightVector() * 1800.0; // 18 m to starboard
	Rover->Deploy(Tile, Ship, FVector(Drop.X, Drop.Y, Drop.Z), Q);
	PC->Possess(Rover);
	Rover->Msg(TEXT("Rover deployed"));
}

void AOmniGameMode::BoardShip()
{
	AShipPawn* Ship = Cast<AShipPawn>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0)) ? nullptr : nullptr;
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC || !Rover) return;
	AShipPawn* TheShip = nullptr;
	for (TActorIterator<AShipPawn> It(GetWorld()); It; ++It) { TheShip = *It; break; }
	if (!TheShip) return;
	PC->Possess(TheShip);
	Rover->Destroy();
	Rover = nullptr;
	TheShip->Msg(TEXT("Back aboard. L or W to lift off."));
	(void)Ship;
}
