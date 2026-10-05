#include "Core/OmniGameMode.h"
#include "Core/OmniHUD.h"
#include "Ship/ShipPawn.h"
#include "World/SystemDirector.h"
#include "Forge/ForgeSubsystem.h"
#include "Galaxy/GalaxySubsystem.h"
#include "OmniSpace.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/WorldSettings.h"
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
