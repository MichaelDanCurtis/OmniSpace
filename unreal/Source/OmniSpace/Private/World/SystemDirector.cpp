#include "World/SystemDirector.h"
#include "World/CelestialBodyActor.h"
#include "World/StarActor.h"
#include "World/StationActor.h"
#include "OmniSpace.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/World.h"

ASystemDirector::ASystemDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
}

FVector ASystemDirector::OrbitOffset(double R, double Phase, double Inc)
{
	// Matches client/src/game/layout.ts: X = r cos, Y = r sin * sin(inc), Z = r sin * cos(inc)
	// (TS uses Y-up; Unreal is Z-up, so swap the vertical axis.)
	double x = R * FMath::Cos(Phase);
	double yUp = R * FMath::Sin(Phase) * FMath::Sin(Inc);
	double zFwd = R * FMath::Sin(Phase) * FMath::Cos(Inc);
	return FVector(x, zFwd, yUp);
}

void ASystemDirector::Clear()
{
	for (FUniverseEntity& E : Entities)
		if (E.Actor) E.Actor->Destroy();
	Entities.Reset();
	if (Star) { Star->Destroy(); Star = nullptr; }
	if (SunLight) { SunLight->Destroy(); SunLight = nullptr; }
}

void ASystemDirector::LoadSystem(const FStarSystemData& InSystem)
{
	Clear();
	System = InSystem;
	UWorld* World = GetWorld();
	if (!World) return;
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// star
	Star = World->SpawnActor<AStarActor>(AStarActor::StaticClass(), FTransform::Identity, P);
	Star->Setup(System.Star);
	FUniverseEntity SE;
	SE.Actor = Star;
	SE.Pos = FVector::ZeroVector;
	SE.Radius = System.Star.RadiusSolar * 695700000.0;
	SE.Id = TEXT("star");
	SE.Kind = TEXT("star");
	Entities.Add(SE);

	SunLight = World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), FTransform::Identity, P);
	if (UDirectionalLightComponent* L = Cast<UDirectionalLightComponent>(SunLight->GetLightComponent()))
	{
		L->SetMobility(EComponentMobility::Movable); // we re-aim it every frame as the ship moves
		L->SetIntensity(8.0f);
		L->SetLightColor(System.Star.LinearColor().Desaturate(0.4f));
		L->SetAtmosphereSunLight(true);
		L->SetCastShadows(true);
	}

	// bodies (parents first)
	TMap<FString, FVector> Resolved;
	Resolved.Add(TEXT("star"), FVector::ZeroVector);
	TArray<const FBodyData*> Pending;
	for (const FBodyData& B : System.Bodies) Pending.Add(&B);
	int Guard = 0;
	while (Pending.Num() && Guard++ < 16)
	{
		for (int i = Pending.Num() - 1; i >= 0; i--)
		{
			const FBodyData* B = Pending[i];
			const FVector* Parent = Resolved.Find(B->Parent);
			if (!Parent && Guard < 8) continue;
			FVector Base = Parent ? *Parent : FVector::ZeroVector;
			FVector Pos = Base + OrbitOffset(B->OrbitM, B->Phase, B->Inclination);
			Resolved.Add(B->Id, Pos);
			ACelestialBodyActor* A = World->SpawnActor<ACelestialBodyActor>(ACelestialBodyActor::StaticClass(), FTransform::Identity, P);
			A->Setup(*B);
			FUniverseEntity E;
			E.Actor = A;
			E.Pos = Pos;
			E.Radius = B->RadiusM;
			E.Id = B->Id;
			E.Kind = B->Kind;
			Entities.Add(E);
			Pending.RemoveAt(i);
		}
	}
	for (const FStationData& S : System.Stations)
	{
		const FVector* Host = Resolved.Find(S.Parent);
		FVector Pos = (Host ? *Host : FVector::ZeroVector) + OrbitOffset(S.OrbitM, S.Phase, S.Inclination);
		AStationActor* A = World->SpawnActor<AStationActor>(AStationActor::StaticClass(), FTransform::Identity, P);
		A->Setup(S);
		FUniverseEntity E;
		E.Actor = A;
		E.Pos = Pos;
		E.Radius = 3500;
		E.Id = S.Id;
		E.Kind = TEXT("station");
		Entities.Add(E);
	}
	UE_LOG(LogOmniSpace, Log, TEXT("System %s loaded: %d bodies, %d stations"), *System.Name, System.Bodies.Num(), System.Stations.Num());
	PlaceAll();
}

void ASystemDirector::MoveShip(const FVector& VelocityMps, float Dt)
{
	ShipPos += VelocityMps * Dt;
}

bool ASystemDirector::GetEntityPos(const FString& Id, FVector& OutPosM, double& OutRadiusM) const
{
	for (const FUniverseEntity& E : Entities)
		if (E.Id == Id) { OutPosM = E.Pos; OutRadiusM = E.Radius; return true; }
	return false;
}

TArray<FString> ASystemDirector::GetEntityIds() const
{
	TArray<FString> Out;
	for (const FUniverseEntity& E : Entities) Out.Add(E.Id);
	return Out;
}

double ASystemDirector::NearestBody(FString& OutId) const
{
	double Best = TNumericLimits<double>::Max();
	for (const FUniverseEntity& E : Entities)
	{
		if (E.Kind != TEXT("planet") && E.Kind != TEXT("moon") && E.Kind != TEXT("star")) continue;
		double D = FVector::Dist(E.Pos, ShipPos) - E.Radius;
		if (D < Best) { Best = D; OutId = E.Id; }
	}
	return Best;
}

void ASystemDirector::PlaceAll()
{
	for (FUniverseEntity& E : Entities)
	{
		if (!E.Actor) continue;
		FVector Rel = E.Pos - ShipPos; // metres
		double Dist = Rel.Size();
		double Scale = 1.0;
		// Keep very distant bodies inside a stable render range by shrinking them
		// along the view ray (angular size is preserved so it looks identical).
		if (Dist > FarDistanceM)
		{
			// Monotonic compression keeps depth ordering between distant bodies.
			double Placed = FarDistanceM * (1.0 + FMath::Loge(Dist / FarDistanceM));
			Scale = Placed / Dist;
			Rel *= Scale;
		}
		E.Actor->SetActorLocation(Rel * 100.0, false, nullptr, ETeleportType::TeleportPhysics);
		if (ACelestialBodyActor* B = Cast<ACelestialBodyActor>(E.Actor)) B->SetRenderScale(Scale);
		else if (AStarActor* S = Cast<AStarActor>(E.Actor)) S->SetRenderScale(Scale, Dist);
		else if (AStationActor* St = Cast<AStationActor>(E.Actor))
		{
			St->SetActorScale3D(FVector(Scale));
			St->SetActorHiddenInGame(Dist > 3.0e7);
		}
	}
	if (SunLight)
	{
		FVector ToStar = (-ShipPos).GetSafeNormal();
		SunLight->SetActorRotation((-ToStar).Rotation());
	}
	if (Star) Star->UpdateUniforms(Time);
	for (FUniverseEntity& E : Entities)
		if (ACelestialBodyActor* B = Cast<ACelestialBodyActor>(E.Actor)) B->UpdateUniforms(-ShipPos * 100.0, Time);
}

void ASystemDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Time += DeltaSeconds;
	PlaceAll();
}
