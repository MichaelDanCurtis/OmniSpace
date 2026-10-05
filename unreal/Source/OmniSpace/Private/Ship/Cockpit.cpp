#include "Ship/Cockpit.h"
#include "World/OmniAssets.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	UStaticMeshComponent* Part(AActor* Owner, USceneComponent* Parent, UStaticMesh* Mesh, const FVector& PosM, const FRotator& Rot, const FVector& SizeM, UMaterialInterface* Mat, const FLinearColor& Color, float PanelM = 0.4f)
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Owner);
		C->SetStaticMesh(Mesh);
		C->SetupAttachment(Parent);
		C->SetRelativeLocation(PosM * 100.0);
		C->SetRelativeRotation(Rot);
		C->SetRelativeScale3D(SizeM);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetMobility(EComponentMobility::Movable);
		C->bCastDynamicShadow = true;
		C->SetReceivesDecals(false);
		if (UMaterialInstanceDynamic* MID = OmniAssets::MakeMID(C, Mat, Owner))
		{
			MID->SetVectorParameterValue(TEXT("Color"), Color);
			MID->SetVectorParameterValue(TEXT("EmissiveColor"), Color);
			MID->SetScalarParameterValue(TEXT("PanelM"), PanelM);
			MID->SetScalarParameterValue(TEXT("Windows"), 0.f);
		}
		C->RegisterComponent();
		return C;
	}

	/** Box beam from A to B (metres) with cross-section W x H. */
	UStaticMeshComponent* Beam(AActor* Owner, USceneComponent* Parent, const FVector& A, const FVector& B, double W, double H, UMaterialInterface* Mat, const FLinearColor& Color)
	{
		FVector D = B - A;
		FRotator Rot = FRotationMatrix::MakeFromX(D.GetSafeNormal()).Rotator();
		return Part(Owner, Parent, OmniAssets::CubeMesh(), (A + B) * 0.5, Rot, FVector(D.Size(), W, H), Mat, Color, 0.3f);
	}
}

void OmniCockpit::Build(AActor* Owner, USceneComponent* Parent)
{
	UMaterialInterface* Hull = OmniAssets::HullMaterial();
	UMaterialInterface* Em = OmniAssets::EmissiveMaterial();
	UStaticMesh* Cube = OmniAssets::CubeMesh();
	UStaticMesh* Cyl = OmniAssets::CylinderMesh();
	const FLinearColor Dark(0.045f, 0.05f, 0.06f);
	const FLinearColor Frame(0.11f, 0.12f, 0.14f);
	const FLinearColor Trim(0.22f, 0.21f, 0.2f);
	const FLinearColor Cyan(0.25f, 1.4f, 2.2f);
	const FLinearColor Amber(2.6f, 1.3f, 0.35f);
	const FLinearColor Screen(0.05f, 0.25f, 0.4f);

	// Camera sits at the origin looking +X with a 70 deg horizontal FOV (~42 deg vertical), so
	// anything that should be on screen must lie within ~35 deg sideways and ~21 deg up/down.
	// --- dashboard: a raked slab across the bottom of the view with three screens
	Part(Owner, Parent, Cube, FVector(1.25, 0, -0.50), FRotator(-16, 0, 0), FVector(0.55, 2.4, 0.08), Hull, Dark, 0.5f);
	Part(Owner, Parent, Cube, FVector(1.52, 0, -0.42), FRotator(-16, 0, 0), FVector(0.05, 2.44, 0.05), Hull, Trim, 0.2f);
	for (int i = -1; i <= 1; i++)
	{
		Part(Owner, Parent, Cube, FVector(1.22, i * 0.62, -0.455), FRotator(-16, 0, 0), FVector(0.34, 0.50, 0.012), Em, Screen);
		Part(Owner, Parent, Cube, FVector(1.40, i * 0.62, -0.405), FRotator(-16, 0, 0), FVector(0.01, 0.46, 0.006), Em, i == 0 ? Amber : Cyan);
	}
	Part(Owner, Parent, Cube, FVector(1.53, 0, -0.395), FRotator(-16, 0, 0), FVector(0.01, 2.3, 0.004), Em, Cyan);
	// --- side consoles at the edges of the view
	for (int s = -1; s <= 1; s += 2)
	{
		Part(Owner, Parent, Cube, FVector(0.95, s * 0.85, -0.52), FRotator(0, 0, s * 14.0), FVector(0.8, 0.30, 0.10), Hull, Dark, 0.4f);
		Part(Owner, Parent, Cube, FVector(0.95, s * 0.86, -0.465), FRotator(0, 0, s * 14.0), FVector(0.6, 0.20, 0.01), Em, Screen);
		Part(Owner, Parent, Cube, FVector(0.95, s * 0.97, -0.44), FRotator(0, 0, s * 14.0), FVector(0.7, 0.008, 0.006), Em, Amber);
	}
	// --- canopy frame: sills, A-pillars up the sides, top rail, nose bar across the lower third
	const FVector SillL(1.45, -1.02, -0.46), SillR(1.45, 1.02, -0.46);
	const FVector TopL(1.25, -0.78, 0.42), TopR(1.25, 0.78, 0.42);
	const FVector NoseL(1.95, -0.58, -0.44), NoseR(1.95, 0.58, -0.44);
	Beam(Owner, Parent, SillL, NoseL, 0.07, 0.07, Hull, Frame);
	Beam(Owner, Parent, SillR, NoseR, 0.07, 0.07, Hull, Frame);
	Beam(Owner, Parent, SillL, TopL, 0.06, 0.06, Hull, Frame);
	Beam(Owner, Parent, SillR, TopR, 0.06, 0.06, Hull, Frame);
	Beam(Owner, Parent, TopL, TopR, 0.07, 0.05, Hull, Frame);
	Beam(Owner, Parent, NoseL, NoseR, 0.05, 0.04, Hull, Frame);
	Beam(Owner, Parent, TopL, FVector(0.4, -1.0, 0.75), 0.08, 0.08, Hull, Frame);
	Beam(Owner, Parent, TopR, FVector(0.4, 1.0, 0.75), 0.08, 0.08, Hull, Frame);
	Beam(Owner, Parent, SillL, FVector(0.4, -1.15, -0.5), 0.09, 0.09, Hull, Frame);
	Beam(Owner, Parent, SillR, FVector(0.4, 1.15, -0.5), 0.09, 0.09, Hull, Frame);
	// --- overhead panel just above the top rail with a row of switch lights
	Part(Owner, Parent, Cube, FVector(1.0, 0, 0.55), FRotator(20, 0, 0), FVector(0.45, 1.3, 0.07), Hull, Dark, 0.4f);
	for (int i = -3; i <= 3; i++)
		Part(Owner, Parent, Cube, FVector(1.02, i * 0.16, 0.505), FRotator(20, 0, 0), FVector(0.03, 0.03, 0.01), Em, (i % 2) ? Cyan : Amber);
	// --- hull shell behind and around the pilot so turning never shows empty space
	Part(Owner, Parent, Cube, FVector(-0.9, 0, 0.1), FRotator::ZeroRotator, FVector(0.3, 2.8, 2.4), Hull, Dark, 0.6f);
	Part(Owner, Parent, Cube, FVector(0.3, 0, -1.1), FRotator::ZeroRotator, FVector(2.6, 2.8, 0.2), Hull, Dark, 0.6f);
	Part(Owner, Parent, Cube, FVector(0.2, 0, 1.3), FRotator::ZeroRotator, FVector(2.2, 2.8, 0.2), Hull, Dark, 0.6f);
	for (int s = -1; s <= 1; s += 2)
		Part(Owner, Parent, Cube, FVector(0.3, s * 1.45, 0.1), FRotator::ZeroRotator, FVector(2.6, 0.2, 2.4), Hull, Dark, 0.6f);

	// soft interior light so the dashboard reads when the sun is behind us
	UPointLightComponent* L = NewObject<UPointLightComponent>(Owner);
	L->SetupAttachment(Parent);
	L->SetRelativeLocation(FVector(40, 0, 60));
	L->SetIntensity(120.f);
	L->SetLightColor(FLinearColor(0.75f, 0.85f, 1.f));
	L->SetAttenuationRadius(400.f);
	L->SetCastShadows(false);
	L->SetMobility(EComponentMobility::Movable);
	L->RegisterComponent();
}
