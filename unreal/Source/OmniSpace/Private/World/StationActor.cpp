#include "World/StationActor.h"
#include "World/OmniAssets.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

AStationActor::AStationActor()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Spinner = CreateDefaultSubobject<USceneComponent>(TEXT("Spinner"));
	Spinner->SetupAttachment(Root);
}

UStaticMeshComponent* AStationActor::AddPart(UStaticMesh* Mesh, const FVector& PosM, const FRotator& Rot, const FVector& SizeM, UMaterialInterface* Mat, const FLinearColor& Color, bool bCollide)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(Mesh);
	C->SetupAttachment(Spinner);
	C->SetRelativeLocation(PosM * 100.0);
	C->SetRelativeRotation(Rot);
	C->SetRelativeScale3D(SizeM); // engine basic shapes are 1 m across
	// Profile first: SetCollisionProfileName also applies the profile's CollisionEnabled,
	// which would otherwise undo NoCollision on the light-strip parts.
	C->SetCollisionProfileName(TEXT("BlockAll"));
	C->SetCollisionEnabled(bCollide ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	C->SetMobility(EComponentMobility::Movable);
	if (UMaterialInstanceDynamic* MID = OmniAssets::MakeMID(C, Mat, this))
	{
		MID->SetVectorParameterValue(TEXT("Color"), Color);
		MID->SetVectorParameterValue(TEXT("EmissiveColor"), Color);
	}
	C->RegisterComponent();
	return C;
}

void AStationActor::Setup(const FStationData& InStation)
{
	Station = InStation;
	if (Station.Kind == TEXT("ring")) BuildRing();
	else if (Station.Kind == TEXT("outpost")) BuildOutpost();
	else BuildSpire();
}

void AStationActor::BuildSpire()
{
	UMaterialInterface* Hull = OmniAssets::HullMaterial();
	UMaterialInterface* Em = OmniAssets::EmissiveMaterial();
	const FLinearColor Grey(0.35f, 0.38f, 0.42f);
	const FLinearColor Dark(0.08f, 0.09f, 0.11f);
	const FLinearColor Orange(3.0f, 1.6f, 0.4f);
	const FLinearColor Cyan(0.5f, 2.0f, 3.0f);
	const double D = 1150; // disc radius (m)
	UStaticMesh* Cyl = OmniAssets::CylinderMesh();
	UStaticMesh* Cube = OmniAssets::CubeMesh();
	AddPart(Cyl, FVector(0, 0, 0), FRotator::ZeroRotator, FVector(D * 2, D * 2, 190), Hull, Grey);
	AddPart(Cyl, FVector(0, 0, 190), FRotator::ZeroRotator, FVector(1440, 1440, 130), Hull, Grey);
	AddPart(Cyl, FVector(0, 0, -200), FRotator::ZeroRotator, FVector(D * 1.6, D * 1.6, 180), Hull, Dark);
	AddPart(Cyl, FVector(0, 0, 200), FRotator::ZeroRotator, FVector(320, 320, 3200), Hull, Grey);
	AddPart(Cyl, FVector(0, 0, 2250), FRotator::ZeroRotator, FVector(200, 200, 900), Hull, Dark);
	// light rings
	AddPart(Cyl, FVector(0, 0, -60), FRotator::ZeroRotator, FVector(D * 2 + 20, D * 2 + 20, 8), Em, Orange, false);
	AddPart(Cyl, FVector(0, 0, 60), FRotator::ZeroRotator, FVector(D * 2 + 20, D * 2 + 20, 5), Em, Orange, false);
	// bays
	const int NB = 4;
	for (int i = 0; i < NB; i++)
	{
		double A = (i / (double)NB) * 2 * PI + PI / 4;
		FVector N(FMath::Cos(A), FMath::Sin(A), 0);
		FVector Base = N * (D + 20);
		FRotator R(0, FMath::RadiansToDegrees(A), 0);
		AddPart(Cube, Base, R, FVector(60, 260, 86), Hull, Dark);
		AddPart(Cube, Base + N * 32 + FVector(0, 0, 46), R, FVector(6, 280, 6), Em, Cyan, false);
		AddPart(Cube, Base + N * 32 - FVector(0, 0, 46), R, FVector(6, 280, 6), Em, Cyan, false);
		BayPositions.Add(N * (D + 40) * 100.0);
		BayNormals.Add(N);
	}
	SpinRateRadPerSec = 2 * PI / 900.0;
	BoundRadiusM = 3600;
}

void AStationActor::BuildRing()
{
	UMaterialInterface* Hull = OmniAssets::HullMaterial();
	UMaterialInterface* Em = OmniAssets::EmissiveMaterial();
	const FLinearColor Grey(0.35f, 0.38f, 0.42f);
	const FLinearColor Orange(3.0f, 1.6f, 0.4f);
	UStaticMesh* Cyl = OmniAssets::CylinderMesh();
	UStaticMesh* Cube = OmniAssets::CubeMesh();
	const double R = 1800;
	// ring approximated by 24 segments
	for (int i = 0; i < 24; i++)
	{
		double A = (i / 24.0) * 2 * PI;
		FVector P(FMath::Cos(A) * R, FMath::Sin(A) * R, 0);
		AddPart(Cube, P, FRotator(0, FMath::RadiansToDegrees(A) + 90, 0), FVector(480, 340, 340), Hull, Grey);
	}
	for (int i = 0; i < 6; i++)
	{
		double A = (i / 6.0) * PI;
		AddPart(Cube, FVector::ZeroVector, FRotator(0, FMath::RadiansToDegrees(A), 0), FVector(R * 2, 70, 70), Hull, Grey);
	}
	AddPart(Cyl, FVector(0, 0, 0), FRotator::ZeroRotator, FVector(560, 560, 900), Hull, Grey);
	AddPart(Cyl, FVector(0, 0, 470), FRotator::ZeroRotator, FVector(660, 660, 80), Hull, Grey);
	AddPart(Cube, FVector(0, 0, 512), FRotator::ZeroRotator, FVector(220, 90, 30), Hull, FLinearColor(0.08f, 0.09f, 0.11f));
	AddPart(Cyl, FVector(0, 0, 0), FRotator::ZeroRotator, FVector(R * 2 + 350, R * 2 + 350, 10), Em, Orange, false);
	BayPositions.Add(FVector(0, 0, 560 * 100.0));
	BayNormals.Add(FVector(0, 0, 1));
	SpinRateRadPerSec = 2 * PI / 240.0;
	BoundRadiusM = 2400;
}

void AStationActor::BuildOutpost()
{
	UMaterialInterface* Hull = OmniAssets::HullMaterial();
	UMaterialInterface* Em = OmniAssets::EmissiveMaterial();
	const FLinearColor Grey(0.35f, 0.38f, 0.42f);
	const FLinearColor Cyan(0.5f, 2.0f, 3.0f);
	UStaticMesh* Cyl = OmniAssets::CylinderMesh();
	UStaticMesh* Cube = OmniAssets::CubeMesh();
	AddPart(Cube, FVector::ZeroVector, FRotator::ZeroRotator, FVector(60, 1400, 60), Hull, Grey);
	AddPart(Cube, FVector::ZeroVector, FRotator::ZeroRotator, FVector(1000, 50, 50), Hull, Grey);
	for (int i = 0; i < 5; i++)
		AddPart(Cyl, FVector(0, -600 + i * 300, 20), FRotator(0, 0, 90), FVector(180, 180, 220), Hull, Grey);
	AddPart(Cube, FVector(420, 0, 0), FRotator::ZeroRotator, FVector(260, 260, 30), Hull, Grey);
	AddPart(Cube, FVector(420, 128, 18), FRotator::ZeroRotator, FVector(250, 4, 4), Em, Cyan, false);
	AddPart(Cube, FVector(420, -128, 18), FRotator::ZeroRotator, FVector(250, 4, 4), Em, Cyan, false);
	BayPositions.Add(FVector(420, 0, 40) * 100.0);
	BayNormals.Add(FVector(0, 0, 1));
	SpinRateRadPerSec = 0;
	BoundRadiusM = 1100;
}

void AStationActor::GetBayWorld(int32 Index, FVector& OutPos, FVector& OutNormal) const
{
	if (BayPositions.Num() == 0) { OutPos = GetActorLocation(); OutNormal = FVector::UpVector; return; }
	Index = FMath::Clamp(Index, 0, BayPositions.Num() - 1);
	const FTransform T = Spinner->GetComponentTransform();
	OutPos = T.TransformPosition(BayPositions[Index]);
	OutNormal = T.TransformVectorNoScale(BayNormals[Index]).GetSafeNormal();
}

void AStationActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Angle += SpinRateRadPerSec * DeltaSeconds;
	Spinner->SetRelativeRotation(FRotator(0, FMath::RadiansToDegrees(Angle), 0));
}
