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
	FString StartId = TEXT("kelvar");
	FParse::Value(FCommandLine::Get(), TEXT("StartSystem="), StartId);
	FStarDescriptor Star;
	if (!Galaxy->GetStar(StartId, Star)) Galaxy->GetStar(TEXT("kelvar"), Star);
	PendingSystemId = Star.Id;
	bPendingJump = false;
	Forge->LoadSystem(Star, Galaxy->StarsNear(Star.Pos, 25.0), Commander);
}

void AOmniGameMode::OnSystemLoaded(const FString& SystemId, bool bSuccess, const FStarSystemData& System)
{
	if (SystemId != PendingSystemId) return;
	if (!bSuccess)
	{
		UE_LOG(LogOmniSpace, Error, TEXT("Could not load system %s"), *SystemId);
		return;
	}
	EnterSystem(System, bPendingJump);
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
		double StarR = System.Star.RadiusSolar * 695700000.0;
		FVector Dir = FVector(0.6, 0.75, 0.25).GetSafeNormal();
		FVector Pos = Dir * FMath::Max(StarR * 9.0, 4e8);
		Ship->Place(Pos, FVector::ZeroVector);
		Ship->Mode = EFlightMode::Supercruise;
		Ship->SupercruiseSpeed = 30000;
	}
	Ship->Msg(FString::Printf(TEXT("%s - %s - %s"), *System.Name, *System.Profile.Faction, *System.Profile.Economy));
}
