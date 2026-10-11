#include "World/GalaxyMapActor.h"
#include "World/OmniAssets.h"
#include "Galaxy/GalaxySubsystem.h"
#include "Galaxy/GalaxyCore.h"
#include "OmniSpace.h"
#include "Camera/CameraComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/GameInstance.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	// star point sizes from the web map (px), reused as sphere diameters in decimetres
	double MapStarSize(const FString& Cls)
	{
		if (Cls == TEXT("O")) return 9.0; if (Cls == TEXT("B")) return 7.5; if (Cls == TEXT("A")) return 6.0; if (Cls == TEXT("F")) return 5.2;
		if (Cls == TEXT("G")) return 5.0; if (Cls == TEXT("K")) return 4.4; if (Cls == TEXT("M")) return 3.6; if (Cls == TEXT("L")) return 3.0;
		return 3.4;
	}
}

AGalaxyMapActor::AGalaxyMapActor()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("MapCamera"));
	Camera->SetupAttachment(Root);
	Camera->FieldOfView = 55.f;
	Stars = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Stars"));
	Stars->SetupAttachment(Root);
	Stars->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Stars->SetCastShadow(false);
	Stars->NumCustomDataFloats = 4;
	Stars->SetMobility(EComponentMobility::Movable);
	Cloud = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Cloud"));
	Cloud->SetupAttachment(Root);
	Cloud->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Cloud->SetCastShadow(false);
	Cloud->SetMobility(EComponentMobility::Movable);
	SetActorHiddenInGame(true);
}

FVector AGalaxyMapActor::MapPos(const FVector& PosLy) const
{
	// galactic (x, y=height, z) -> map (x, z, y): the plane lies flat, height goes up
	const FVector R = PosLy - FocusStar.Pos;
	return GetActorLocation() + FVector(R.X, R.Z, R.Y) * LyToCm;
}

void AGalaxyMapActor::SetFocus(const FStarDescriptor& Focus)
{
	UGalaxySubsystem* Galaxy = GetGameInstance() ? GetGameInstance()->GetSubsystem<UGalaxySubsystem>() : nullptr;
	if (!Galaxy) return;
	FocusStar = Focus;
	LocalStars = Galaxy->StarsNear(Focus.Pos, LocalRadiusLy);
	Stars->ClearInstances();
	if (!Stars->GetStaticMesh())
	{
		Stars->SetStaticMesh(OmniAssets::SphereMesh());
		Stars->SetMaterial(0, OmniAssets::MaterialOr(TEXT("/Game/Materials/M_MapStar.M_MapStar"), TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial")));
	}
	TArray<FTransform> Xf;
	Xf.Reserve(LocalStars.Num());
	for (const FStarDescriptor& S : LocalStars)
	{
		const double Sz = MapStarSize(S.Cls) * 0.09; // sphere mesh is 1 m across
		Xf.Add(FTransform(FQuat::Identity, MapPos(S.Pos), FVector(Sz)));
	}
	Stars->AddInstances(Xf, false, false);
	for (int32 i = 0; i < LocalStars.Num(); i++)
	{
		const FLinearColor C = LocalStars[i].LinearColor();
		Stars->SetCustomDataValue(i, 0, C.R, false);
		Stars->SetCustomDataValue(i, 1, C.G, false);
		Stars->SetCustomDataValue(i, 2, C.B, false);
		Stars->SetCustomDataValue(i, 3, static_cast<float>(FMath::Clamp(LocalStars[i].Luminosity, 0.05, 4.0)), i == LocalStars.Num() - 1);
	}
	Stars->MarkRenderStateDirty();
	bBuilt = true;
	UE_LOG(LogOmniSpace, Log, TEXT("[map] focus %s: %d stars within %.0f ly"), *Focus.Name, LocalStars.Num(), LocalRadiusLy);
}

void AGalaxyMapActor::BuildCloud()
{
	if (bCloudBuilt) return;
	bCloudBuilt = true;
	// the galaxy at large: a thinned sample of the spiral density model, flat sprites seen from above
	Cloud->SetStaticMesh(OmniAssets::PlaneMesh());
	Cloud->SetMaterial(0, OmniAssets::MaterialOr(TEXT("/Game/Materials/M_MapCloud.M_MapCloud"), TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial")));
	omni::galaxy::Rng Rng(777);
	const double Max = omni::galaxy::density(0, 0, 0);
	TArray<FTransform> Xf;
	const int32 N = 24000;
	int32 Guard = 0;
	while (Xf.Num() < N && Guard++ < 2000000)
	{
		const double R = FMath::Sqrt(Rng.next()) * 52000.0;
		const double Th = Rng.next() * 2 * PI;
		const double X = FMath::Cos(Th) * R, Z = FMath::Sin(Th) * R;
		const double Y = Rng.gauss() * (300.0 + R * 0.01) * 2.0;
		const double D = omni::galaxy::density(X, Y, Z);
		if (Rng.next() * Max * 0.35 > D) continue;
		// galactic coordinates are absolute; MapPos wants ly relative to the focus
		Xf.Add(FTransform(FQuat::Identity, MapPos(FVector(X, Y, Z)), FVector(1.8, 1.8, 1.0)));
	}
	Cloud->AddInstances(Xf, false, false);
	Cloud->MarkRenderStateDirty();
	UE_LOG(LogOmniSpace, Log, TEXT("[map] galaxy cloud: %d points"), Xf.Num());
}

void AGalaxyMapActor::Open(const FStarDescriptor& Current)
{
	if (!bBuilt || FVector::Dist(Current.Pos, FocusStar.Pos) > 48.0) SetFocus(Current);
	SetActorHiddenInGame(false);
	SetGalaxyView(false);
	HomeCamera(Current.Pos);
	UpdateCamera(100.f);
}

void AGalaxyMapActor::Close()
{
	SetActorHiddenInGame(true);
}

void AGalaxyMapActor::HomeCamera(const FVector& PosLy)
{
	WantTargetLy = PosLy;
	WantYaw = -90; WantPitch = -34; WantDist = 72 * LyToCm;
	TargetLy_ = WantTargetLy; Yaw = WantYaw; Pitch = WantPitch; Dist = WantDist;
}

void AGalaxyMapActor::CenterOn(const FVector& PosLy)
{
	WantTargetLy = PosLy;
}

void AGalaxyMapActor::Orbit(double DYawDeg, double DPitchDeg)
{
	WantYaw += DYawDeg;
	WantPitch = FMath::Clamp(WantPitch + DPitchDeg, -88.0, 20.0);
}

void AGalaxyMapActor::Zoom(double Factor)
{
	const double Lo = bGalaxyView ? 2000 * LyToCm : 4 * LyToCm;
	const double Hi = bGalaxyView ? 200000 * LyToCm : 400 * LyToCm;
	WantDist = FMath::Clamp(WantDist * Factor, Lo, Hi);
}

void AGalaxyMapActor::SetGalaxyView(bool bOn)
{
	if (bOn && !bCloudBuilt) BuildCloud();
	bGalaxyView = bOn;
	Cloud->SetVisibility(bOn);
	if (bOn)
	{
		// pull back to see the whole disc, centred on the galaxy's core
		WantTargetLy = FVector(0, 0, 0);
		WantDist = 90000 * LyToCm; WantPitch = -62; WantYaw = -90;
	}
	else
	{
		WantTargetLy = FocusStar.Pos;
		WantDist = 72 * LyToCm; WantPitch = -34;
	}
}

void AGalaxyMapActor::UpdateCamera(float Dt)
{
	const double K = 1.0 - FMath::Exp(-Dt * 8.0);
	Yaw += (WantYaw - Yaw) * K;
	Pitch += (WantPitch - Pitch) * K;
	Dist += (WantDist - Dist) * K;
	TargetLy_ += (WantTargetLy - TargetLy_) * K;
	const FVector T = MapPos(TargetLy_);
	const double Cy = FMath::Cos(FMath::DegreesToRadians(Yaw)), Sy = FMath::Sin(FMath::DegreesToRadians(Yaw));
	const double Cp = FMath::Cos(FMath::DegreesToRadians(Pitch)), Sp = FMath::Sin(FMath::DegreesToRadians(Pitch));
	const FVector Dir(Cy * Cp, Sy * Cp, Sp); // camera -> target; pitch negative means the camera sits above
	Camera->SetWorldLocation(T - Dir * Dist);
	Camera->SetWorldRotation(FRotationMatrix::MakeFromXZ(Dir, FVector::UpVector).ToQuat());
}

void AGalaxyMapActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (IsHidden()) return;
	UpdateCamera(DeltaSeconds);
}
