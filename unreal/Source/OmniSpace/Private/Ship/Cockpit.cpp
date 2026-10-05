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

	// --- dashboard: a raked slab under the view with a raised lip
	Part(Owner, Parent, Cube, FVector(1.05, 0, -0.62), FRotator(-18, 0, 0), FVector(0.55, 2.3, 0.10), Hull, Dark, 0.5f);
	Part(Owner, Parent, Cube, FVector(1.30, 0, -0.52), FRotator(-18, 0, 0), FVector(0.06, 2.34, 0.05), Hull, Trim, 0.2f);
	Part(Owner, Parent, Cube, FVector(0.78, 0, -0.74), FRotator(-18, 0, 0), FVector(0.06, 2.3, 0.14), Hull, Frame, 0.2f);
	// instrument screens (dim glass) and lit strips
	for (int i = -1; i <= 1; i++)
	{
		Part(Owner, Parent, Cube, FVector(1.04, i * 0.62, -0.555), FRotator(-18, 0, 0), FVector(0.36, 0.50, 0.012), Em, Screen);
		Part(Owner, Parent, Cube, FVector(1.22, i * 0.62, -0.50), FRotator(-18, 0, 0), FVector(0.01, 0.46, 0.006), Em, i == 0 ? Amber : Cyan);
	}
	Part(Owner, Parent, Cube, FVector(1.31, 0, -0.495), FRotator(-18, 0, 0), FVector(0.01, 2.2, 0.004), Em, Cyan);
	// --- side consoles
	for (int s = -1; s <= 1; s += 2)
	{
		Part(Owner, Parent, Cube, FVector(0.55, s * 1.05, -0.70), FRotator(0, 0, s * 12.0), FVector(0.9, 0.34, 0.12), Hull, Dark, 0.4f);
		Part(Owner, Parent, Cube, FVector(0.55, s * 1.07, -0.63), FRotator(0, 0, s * 12.0), FVector(0.7, 0.22, 0.01), Em, Screen);
		Part(Owner, Parent, Cube, FVector(0.55, s * 1.19, -0.60), FRotator(0, 0, s * 12.0), FVector(0.8, 0.008, 0.006), Em, Amber);
	}
	// --- canopy frame: A-pillars, top rail, centre spine, sills
	const FVector SillL(1.32, -1.17, -0.50), SillR(1.32, 1.17, -0.50);
	const FVector TopL(0.55, -0.95, 0.92), TopR(0.55, 0.95, 0.92);
	const FVector NoseL(1.95, -0.55, 0.05), NoseR(1.95, 0.55, 0.05);
	Beam(Owner, Parent, SillL, NoseL, 0.08, 0.08, Hull, Frame);
	Beam(Owner, Parent, SillR, NoseR, 0.08, 0.08, Hull, Frame);
	Beam(Owner, Parent, NoseL, TopL, 0.07, 0.07, Hull, Frame);
	Beam(Owner, Parent, NoseR, TopR, 0.07, 0.07, Hull, Frame);
	Beam(Owner, Parent, TopL, TopR, 0.09, 0.07, Hull, Frame);
	Beam(Owner, Parent, NoseL, NoseR, 0.06, 0.05, Hull, Frame);
	Beam(Owner, Parent, SillL, FVector(0.2, -1.25, -0.40), 0.10, 0.10, Hull, Frame);
	Beam(Owner, Parent, SillR, FVector(0.2, 1.25, -0.40), 0.10, 0.10, Hull, Frame);
	Beam(Owner, Parent, TopL, FVector(-0.1, -1.1, 1.0), 0.09, 0.09, Hull, Frame);
	Beam(Owner, Parent, TopR, FVector(-0.1, 1.1, 1.0), 0.09, 0.09, Hull, Frame);
	// --- overhead panel with switches
	Part(Owner, Parent, Cube, FVector(0.25, 0, 1.02), FRotator(12, 0, 0), FVector(0.5, 1.2, 0.08), Hull, Dark, 0.4f);
	for (int i = -3; i <= 3; i++)
		Part(Owner, Parent, Cube, FVector(0.3, i * 0.15, 0.975), FRotator(12, 0, 0), FVector(0.03, 0.03, 0.01), Em, (i % 2) ? Cyan : Amber);
	// --- seat/rear bulkhead so turning the head never shows empty space
	Part(Owner, Parent, Cube, FVector(-0.9, 0, 0.1), FRotator::ZeroRotator, FVector(0.3, 2.8, 2.4), Hull, Dark, 0.6f);
	Part(Owner, Parent, Cyl, FVector(-0.2, 0, -0.9), FRotator::ZeroRotator, FVector(0.9, 0.9, 0.5), Hull, Frame, 0.3f);

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
