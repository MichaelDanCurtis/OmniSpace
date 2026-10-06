#include "Core/OmniGameMode.h"
#include "Core/OmniHUD.h"
#include "Ship/ShipPawn.h"
#include "Ship/RoverPawn.h"
#include "Ship/ShipAudio.h"
#include "World/SurfaceTile.h"
#include "World/OutpostActor.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
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
	PrimaryActorTick.bCanEverTick = true;
}

void AOmniGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (OutpostActor && OutpostActor->AdvanceConstruction(DeltaSeconds))
	{
		SaveOutposts();
		if (AShipPawn* Ship = Cast<AShipPawn>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0))) Ship->Msg(TEXT("Outpost: construction complete"));
	}
}

void AOmniGameMode::BeginPlay()
{
	Super::BeginPlay();
	LoadOutposts();
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
	SurfaceSite = Site;
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
	// an outpost founded at this site comes back with it
	CurrentOutpost = -1;
	for (int32 i = 0; i < Outposts.Num(); i++)
		if (Outposts[i].BodyId == BodyId && Outposts[i].SystemId == Director->GetSystem().Id && (Outposts[i].Site - Site).Size() < 0.04) { CurrentOutpost = i; break; }
	if (CurrentOutpost >= 0) SpawnOutpostActor();
	UE_LOG(LogOmniSpace, Log, TEXT("Surface: %s at site (%.2f, %.2f)%s"), *B->Name, Site.X, Site.Y, CurrentOutpost >= 0 ? TEXT(" - outpost here") : TEXT(""));
}

void AOmniGameMode::SpawnOutpostActor()
{
	FOutpostData* D = CurrentOutpostData();
	if (!Tile || !D) return;
	if (!OutpostActor)
	{
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		OutpostActor = GetWorld()->SpawnActor<AOutpostActor>(AOutpostActor::StaticClass(), FTransform::Identity, P);
	}
	OutpostActor->Init(Tile, D);
}

void AOmniGameMode::LeaveSurface()
{
	AShipPawn* Ship = Cast<AShipPawn>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
	if (!Director || !Tile) return;
	if (Rover) { Rover->Destroy(); Rover = nullptr; }
	if (OutpostActor) { OutpostActor->Destroy(); OutpostActor = nullptr; }
	bBuildMode = false;
	CurrentOutpost = -1;
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

// ---------------------------------------------------------------- outposts
FString AOmniGameMode::OutpostSavePath() const
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("OmniSpace"), TEXT("outposts.json"));
}

void AOmniGameMode::SaveOutposts()
{
	FOutpostSave S;
	S.Outposts = Outposts;
	FString Json;
	if (FJsonObjectConverter::UStructToJsonObjectString(S, Json))
	{
		FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(OutpostSavePath()));
		FFileHelper::SaveStringToFile(Json, *OutpostSavePath());
	}
}

void AOmniGameMode::LoadOutposts()
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *OutpostSavePath())) return;
	FOutpostSave S;
	if (FJsonObjectConverter::JsonObjectStringToUStruct(Json, &S)) Outposts = S.Outposts;
	UE_LOG(LogOmniSpace, Log, TEXT("[outpost] loaded %d outposts"), Outposts.Num());
}

void AOmniGameMode::ToggleBuildMode()
{
	AShipPawn* Ship = Cast<AShipPawn>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
	APawn* Pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	auto Say = [&](const FString& T) { if (Ship) Ship->Msg(T); else if (Rover) Rover->Msg(T); };
	if (!Tile) { Say(TEXT("Outposts are built on a planet's surface - land first")); return; }
	if (Ship && Ship->Mode != EFlightMode::Landed) { Say(TEXT("Set down first, then build (or take the rover out)")); return; }
	bBuildMode = !bBuildMode;
	if (bBuildMode)
	{
		FOutpostData* D = CurrentOutpostData();
		BuildKind = (D && D->HasBeacon()) ? EModuleKind::Hab : EModuleKind::Beacon;
		BuildRot = 0;
		BuildStatus = D && D->HasBeacon() ? TEXT("1-6 choose module, [ ] rotate, Enter place, O done") : TEXT("Place the outpost beacon: Enter to set it down, O to cancel");
		Say(BuildStatus);
	}
	else
	{
		if (OutpostActor) OutpostActor->ClearGhost();
		BuildStatus.Empty();
		Say(TEXT("Build mode off"));
	}
	(void)Pawn;
}

void AOmniGameMode::BuildSelect(int32 Slot)
{
	if (!bBuildMode) return;
	FOutpostData* D = CurrentOutpostData();
	if (!D || !D->HasBeacon()) { BuildStatus = TEXT("Place the beacon first"); return; }
	static const EModuleKind Slots[] = { EModuleKind::Hab, EModuleKind::Power, EModuleKind::Storage, EModuleKind::Corridor, EModuleKind::Lab, EModuleKind::Pad };
	if (Slot < 1 || Slot > 6) return;
	BuildKind = Slots[Slot - 1];
	const FModuleSpec& S = Outpost::Spec(BuildKind);
	BuildStatus = FString::Printf(TEXT("%s - %s. Needs %.0f t alloy, %.0f t composites, %.0f t electronics"), S.Name, S.Blurb, S.Cost.Alloy, S.Cost.Composites, S.Cost.Electronics);
}

void AOmniGameMode::BuildRotate(int32 Dir)
{
	if (!bBuildMode) return;
	BuildRot = (BuildRot + Dir + 4) % 4;
}

void AOmniGameMode::UpdateBuildGhost(const FVector& PawnPosCm, const FVector& Facing)
{
	if (!bBuildMode || !Tile) return;
	FOutpostModule M; M.Kind = BuildKind; M.Rot = BuildRot;
	const int32 W = M.FootW(), H = M.FootH();
	// the ghost sits ahead of the pawn, its footprint centred there, snapped to the grid
	const FVector FwdH = FVector(Facing.X, Facing.Y, 0).GetSafeNormal();
	const FVector AimM = (PawnPosCm + FwdH * (1400.0 + FMath::Max(W, H) * Outpost::CellM * 50.0)) / 100.0;
	FOutpostData* D = CurrentOutpostData();
	const FVector2D Origin = D ? D->OriginM : FVector2D(AimM.X, AimM.Y);
	GhostGX = FMath::RoundToInt((AimM.X - Origin.X) / Outpost::CellM - W * 0.5);
	GhostGY = FMath::RoundToInt((AimM.Y - Origin.Y) / Outpost::CellM - H * 0.5);
	if (!D)
	{
		// no outpost yet: a temporary record so the actor can validate and draw the beacon ghost
		FOutpostData Tmp;
		Tmp.OriginM = Origin;
		if (!OutpostActor)
		{
			FActorSpawnParameters P;
			P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			OutpostActor = GetWorld()->SpawnActor<AOutpostActor>(AOutpostActor::StaticClass(), FTransform::Identity, P);
		}
		static FOutpostData Scratch;
		Scratch = Tmp;
		OutpostActor->Init(Tile, &Scratch);
		GhostGX = 0; GhostGY = 0;
	}
	FString Why;
	bGhostValid = OutpostActor->Validate(BuildKind, GhostGX, GhostGY, BuildRot, Why);
	if (BuildKind == EModuleKind::Beacon && bGhostValid && FVector2D(AimM.X, AimM.Y).Size() > 1500.0) { bGhostValid = false; Why = TEXT("Outposts go within 1.5 km of the landing site"); }
	OutpostActor->SetGhost(BuildKind, GhostGX, GhostGY, BuildRot, bGhostValid);
	if (!bGhostValid) BuildStatus = Why;
	else if (BuildKind == EModuleKind::Beacon) BuildStatus = TEXT("Enter to found the outpost here");
	else BuildStatus = FString::Printf(TEXT("%s: Enter to place the construction site  ([ ] rotate)"), Outpost::Spec(BuildKind).Name);
}

void AOmniGameMode::BuildPlace()
{
	if (!bBuildMode || !OutpostActor || !Tile || !Director) return;
	AShipPawn* Ship = Cast<AShipPawn>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
	auto Say = [&](const FString& T) { if (Ship) Ship->Msg(T); else if (Rover) Rover->Msg(T); };
	if (!bGhostValid) { Say(BuildStatus); return; }
	FOutpostData* D = CurrentOutpostData();
	if (!D)
	{
		// founding: the beacon's cell becomes grid (0,0)
		FOutpostData N;
		N.BodyId = SurfaceBodyId;
		N.SystemId = Director->GetSystem().Id;
		N.Site = FVector2D::ZeroVector;
		if (const FBodyData* B = Director->GetSystem().FindBody(SurfaceBodyId)) N.Name = B->Name + TEXT(" Outpost");
		const FOutpostData* Scratch = OutpostActor->GetData();
		N.OriginM = Scratch ? Scratch->OriginM : FVector2D::ZeroVector;
		// remember the site we landed at so the outpost comes back with this tile
		N.Site = SurfaceSite;
		Outposts.Add(N);
		CurrentOutpost = Outposts.Num() - 1;
		D = CurrentOutpostData();
		OutpostActor->Init(Tile, D);
	}
	FOutpostModule M;
	M.Kind = BuildKind; M.GX = GhostGX; M.GY = GhostGY; M.Rot = BuildRot;
	if (M.Kind == EModuleKind::Beacon) { M.Delivered = Outpost::Spec(M.Kind).Cost; } // the beacon is carried aboard
	D->Modules.Add(M);
	OutpostActor->ClearGhost();
	OutpostActor->RebuildModule(D->Modules.Num() - 1);
	SaveOutposts();
	const FModuleSpec& S = Outpost::Spec(M.Kind);
	if (M.Kind == EModuleKind::Beacon)
	{
		Say(FString::Printf(TEXT("%s founded. 1-6 choose a module, [ ] rotate, Enter place."), *D->Name));
		BuildKind = EModuleKind::Hab;
	}
	else Say(FString::Printf(TEXT("%s site marked - needs %.0f t alloy, %.0f t composites, %.0f t electronics. Land with cargo within %.0f m to deliver."), S.Name, S.Cost.Alloy, S.Cost.Composites, S.Cost.Electronics, Outpost::DeliverM));
	if (Ship && Ship->Audio) Ship->Audio->Play(TEXT("chime"), 0.6f);
}

double AOmniGameMode::DeliverMaterials(FMaterials& Hold, const FVector& ShipPosCm)
{
	FOutpostData* D = CurrentOutpostData();
	if (!D || !OutpostActor || Hold.Total() < 0.01) return 0;
	const FVector2D ShipM(ShipPosCm.X / 100.0, ShipPosCm.Y / 100.0);
	if ((ShipM - D->OriginM).Size() > Outpost::DeliverM) return 0;
	double Moved = 0;
	for (int32 i : OutpostActor->SitesNeedingMaterials())
	{
		FOutpostModule& M = D->Modules[i];
		FMaterials Need = M.Remaining();
		const double A = FMath::Min(Need.Alloy, Hold.Alloy), C = FMath::Min(Need.Composites, Hold.Composites), E = FMath::Min(Need.Electronics, Hold.Electronics);
		M.Delivered.Alloy += A; M.Delivered.Composites += C; M.Delivered.Electronics += E;
		Hold.Alloy -= A; Hold.Composites -= C; Hold.Electronics -= E;
		Moved += A + C + E;
		if (A + C + E > 0) OutpostActor->RebuildModule(i);
		if (Hold.Total() < 0.01) break;
	}
	if (Moved > 0) SaveOutposts();
	return Moved;
}
