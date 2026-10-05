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

double AStationActor::Rand()
{
	// mulberry32, same generator as the galaxy so station looks are reproducible
	Seed += 0x6D2B79F5u;
	uint32 T = Seed;
	T = (T ^ (T >> 15)) * (T | 1u);
	T ^= T + (T ^ (T >> 7)) * (T | 61u);
	return ((T ^ (T >> 14)) >> 0) / 4294967296.0;
}

UStaticMeshComponent* AStationActor::AddStrut(const FVector& AM, const FVector& BM, double DiamM, UMaterialInterface* Mat, const FLinearColor& Color)
{
	FVector D = BM - AM;
	double L = D.Size();
	FRotator Rot = FRotationMatrix::MakeFromZ(D.GetSafeNormal()).Rotator();
	return AddPart(OmniAssets::CylinderMesh(), (AM + BM) * 0.5, Rot, FVector(DiamM, DiamM, L), Mat, Color, true, 0.f, 2.f);
}

void AStationActor::Greeble(int32 Count, double RMin, double RMax, double Z, double Up, double SMin, double SMax, UMaterialInterface* Hull, const FLinearColor& A, const FLinearColor& B)
{
	UStaticMesh* Cube = OmniAssets::CubeMesh();
	UStaticMesh* Cyl = OmniAssets::CylinderMesh();
	for (int i = 0; i < Count; i++)
	{
		double Ang = Rand() * 2 * PI;
		double R = RandRange(RMin, RMax);
		FVector P(FMath::Cos(Ang) * R, FMath::Sin(Ang) * R, 0);
		double Kind = Rand();
		FLinearColor C = Rand() < 0.7 ? A : B;
		if (Kind < 0.55)
		{
			FVector S(RandRange(SMin, SMax), RandRange(SMin, SMax), RandRange(SMin * 0.4, SMax * 0.7));
			AddPart(Cube, P + FVector(0, 0, Z + Up * S.Z * 0.5), FRotator(0, Rand() < 0.5 ? FMath::RadiansToDegrees(Ang) : Rand() * 360, 0), S, Hull, C, true, 0.25f, 2.f);
		}
		else if (Kind < 0.85)
		{
			double Dm = RandRange(SMin * 0.5, SMax * 0.6), H = RandRange(SMin, SMax * 1.4);
			AddPart(Cyl, P + FVector(0, 0, Z + Up * H * 0.5), FRotator::ZeroRotator, FVector(Dm, Dm, H), Hull, C, true, 0.f, 2.f);
		}
		else
		{
			double H = RandRange(SMax * 2, SMax * 5);
			AddPart(Cyl, P + FVector(0, 0, Z + Up * H * 0.5), FRotator::ZeroRotator, FVector(2, 2, H), Hull, FLinearColor(0.12f, 0.13f, 0.15f), false, 0.f, 1.f);
			AddPart(Cube, P + FVector(0, 0, Z + Up * H), FRotator::ZeroRotator, FVector(1.5, 1.5, 1.5), OmniAssets::EmissiveMaterial(), FLinearColor(6, 0.6, 0.4), false);
		}
	}
}

UStaticMeshComponent* AStationActor::AddPart(UStaticMesh* Mesh, const FVector& PosM, const FRotator& Rot, const FVector& SizeM, UMaterialInterface* Mat, const FLinearColor& Color, bool bCollide, float Windows, float PanelM)
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
		if (Mat == OmniAssets::HullMaterial())
		{
			MID->SetScalarParameterValue(TEXT("Windows"), Windows);
			MID->SetScalarParameterValue(TEXT("PanelM"), PanelM);
			MID->SetScalarParameterValue(TEXT("Seed"), static_cast<float>(Rand() * 50.0));
			HullMids.Add(MID);
		}
	}
	C->RegisterComponent();
	return C;
}

void AStationActor::Setup(const FStationData& InStation)
{
	Station = InStation;
	Seed = GetTypeHash(Station.Id);
	if (Station.Kind == TEXT("ring")) BuildRing();
	else if (Station.Kind == TEXT("outpost")) BuildOutpost();
	else BuildSpire();
}

void AStationActor::BuildSpire()
{
	UMaterialInterface* Hull = OmniAssets::HullMaterial();
	UMaterialInterface* Em = OmniAssets::EmissiveMaterial();
	const FLinearColor Grey(0.36f, 0.38f, 0.42f);
	const FLinearColor Light(0.55f, 0.56f, 0.58f);
	const FLinearColor Dark(0.10f, 0.11f, 0.13f);
	const FLinearColor Orange(3.0f, 1.6f, 0.4f);
	const FLinearColor Cyan(0.5f, 2.0f, 3.0f);
	const double D = 1150; // disc radius (m)
	UStaticMesh* Cyl = OmniAssets::CylinderMesh();
	UStaticMesh* Cube = OmniAssets::CubeMesh();
	// main disc: stacked decks with a lit rim band
	AddPart(Cyl, FVector(0, 0, 0), FRotator::ZeroRotator, FVector(D * 2, D * 2, 190), Hull, Grey, true, 0.7f, 8.f);
	AddPart(Cyl, FVector(0, 0, 190), FRotator::ZeroRotator, FVector(1440, 1440, 130), Hull, Light, true, 0.5f, 6.f);
	AddPart(Cyl, FVector(0, 0, 300), FRotator::ZeroRotator, FVector(900, 900, 90), Hull, Grey, true, 0.4f, 5.f);
	AddPart(Cyl, FVector(0, 0, -200), FRotator::ZeroRotator, FVector(D * 1.6, D * 1.6, 180), Hull, Dark, true, 0.2f, 8.f);
	AddPart(Cyl, FVector(0, 0, -330), FRotator::ZeroRotator, FVector(D * 1.1, D * 1.1, 80), Hull, Grey, true, 0.3f, 6.f);
	// radial ribs across the top deck
	for (int i = 0; i < 12; i++)
	{
		double A = i / 12.0 * 360.0;
		AddPart(Cube, FVector(FMath::Cos(FMath::DegreesToRadians(A)) * D * 0.6, FMath::Sin(FMath::DegreesToRadians(A)) * D * 0.6, 100), FRotator(0, A, 0), FVector(D * 0.75, 26, 14), Hull, Light, true, 0.f, 3.f);
	}
	// spire: tapering segments with light rings, crown and comms masts
	const double SpireZ[] = {200, 1100, 1900, 2500};
	const double SpireD[] = {340, 300, 240, 170};
	for (int i = 0; i < 4; i++)
	{
		double H = (i < 3 ? SpireZ[i + 1] : 3100) - SpireZ[i];
		AddPart(Cyl, FVector(0, 0, SpireZ[i] + H * 0.5), FRotator::ZeroRotator, FVector(SpireD[i], SpireD[i], H), Hull, i % 2 ? Dark : Grey, true, 0.6f, 5.f);
		AddPart(Cyl, FVector(0, 0, SpireZ[i] + 6), FRotator::ZeroRotator, FVector(SpireD[i] + 40, SpireD[i] + 40, 12), Hull, Light, true, 0.f, 3.f);
		AddPart(Cyl, FVector(0, 0, SpireZ[i] + 14), FRotator::ZeroRotator, FVector(SpireD[i] + 46, SpireD[i] + 46, 3), Em, Orange, false);
	}
	AddPart(Cyl, FVector(0, 0, 3150), FRotator::ZeroRotator, FVector(260, 260, 100), Hull, Light, true, 0.3f, 4.f);
	AddPart(Cyl, FVector(0, 0, 3380), FRotator::ZeroRotator, FVector(14, 14, 360), Hull, Dark, false, 0.f, 1.f);
	AddPart(Cube, FVector(0, 0, 3570), FRotator::ZeroRotator, FVector(3, 3, 3), Em, FLinearColor(8, 1, 0.6), false);
	// struts from the spire to the disc rim
	for (int i = 0; i < 6; i++)
	{
		double A = i / 6.0 * 2 * PI + PI / 6;
		FVector Rim(FMath::Cos(A) * (D - 60), FMath::Sin(A) * (D - 60), 95);
		FVector Top(FMath::Cos(A) * 150, FMath::Sin(A) * 150, 1050);
		AddStrut(Rim, Top, 24, Hull, Light);
	}
	// fuel tanks around the base of the spire
	for (int i = 0; i < 8; i++)
	{
		double A = i / 8.0 * 2 * PI;
		AddPart(Cyl, FVector(FMath::Cos(A) * 330, FMath::Sin(A) * 330, 420), FRotator::ZeroRotator, FVector(90, 90, 260), Hull, i % 2 ? Light : Grey, true, 0.f, 3.f);
	}
	// radiator fins and a comms dish
	for (int i = 0; i < 3; i++)
	{
		double A = i / 3.0 * 360.0 + 40;
		AddPart(Cube, FVector(FMath::Cos(FMath::DegreesToRadians(A)) * 700, FMath::Sin(FMath::DegreesToRadians(A)) * 700, 230), FRotator(0, A, 0), FVector(240, 6, 160), Hull, Dark, true, 0.f, 4.f);
	}
	AddPart(Cyl, FVector(600, -500, 380), FRotator(35, 20, 0), FVector(220, 220, 10), Hull, Light, true, 0.f, 4.f);
	AddPart(Cyl, FVector(600, -500, 300), FRotator::ZeroRotator, FVector(20, 20, 160), Hull, Dark, false, 0.f, 2.f);
	// clutter on the decks
	Greeble(70, 200, D - 90, 95, 1, 8, 36, Hull, Grey, Dark);
	Greeble(30, 300, D * 0.8 - 60, -290, -1, 10, 30, Hull, Dark, Grey);
	// rim light rings
	AddPart(Cyl, FVector(0, 0, -70), FRotator::ZeroRotator, FVector(D * 2 + 20, D * 2 + 20, 8), Em, Orange, false);
	AddPart(Cyl, FVector(0, 0, 70), FRotator::ZeroRotator, FVector(D * 2 + 20, D * 2 + 20, 5), Em, Orange, false);
	// docking bays: recessed mouths in the rim with lit edges and approach markers
	const int NB = 4;
	for (int i = 0; i < NB; i++)
	{
		double A = (i / (double)NB) * 2 * PI + PI / 4;
		FVector N(FMath::Cos(A), FMath::Sin(A), 0);
		FVector Base = N * (D + 20);
		FRotator R(0, FMath::RadiansToDegrees(A), 0);
		AddPart(Cube, Base, R, FVector(60, 260, 86), Hull, Dark, true, 0.f, 3.f);
		AddPart(Cube, Base + N * 31, R, FVector(2, 230, 70), Em, FLinearColor(0.05f, 0.08f, 0.12f), false); // dark mouth
		AddPart(Cube, Base + N * 32 + FVector(0, 0, 46), R, FVector(6, 280, 6), Em, Cyan, false);
		AddPart(Cube, Base + N * 32 - FVector(0, 0, 46), R, FVector(6, 280, 6), Em, Cyan, false);
		for (int k = 1; k <= 4; k++)
			AddPart(Cube, Base + N * (40 + k * 90), FRotator::ZeroRotator, FVector(3, 3, 3), Em, k % 2 ? Orange : Cyan, false);
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
		AddPart(Cube, P, FRotator(0, FMath::RadiansToDegrees(A) + 90, 0), FVector(480, 340, 340), Hull, Grey, true, 0.8f, 6.f);
	}
	Greeble(40, R - 150, R + 150, 170, 1, 6, 24, Hull, Grey, FLinearColor(0.10f, 0.11f, 0.13f));
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
	if (SpinRateRadPerSec != 0)
		for (UMaterialInstanceDynamic* M : HullMids)
			if (M) M->SetScalarParameterValue(TEXT("Spin"), static_cast<float>(Angle));
}
