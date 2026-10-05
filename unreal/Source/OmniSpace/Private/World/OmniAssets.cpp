#include "World/OmniAssets.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/PrimitiveComponent.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	template <class T> T* LoadPath(const TCHAR* Path)
	{
		// LoadObject is a FindObject when already loaded, so this is cheap on repeat calls.
		return LoadObject<T>(nullptr, Path);
	}
}

UStaticMesh* OmniAssets::SphereMesh() { return LoadPath<UStaticMesh>(TEXT("/Engine/BasicShapes/Sphere.Sphere")); }
UStaticMesh* OmniAssets::PlaneMesh() { return LoadPath<UStaticMesh>(TEXT("/Engine/BasicShapes/Plane.Plane")); }
UStaticMesh* OmniAssets::CubeMesh() { return LoadPath<UStaticMesh>(TEXT("/Engine/BasicShapes/Cube.Cube")); }
UStaticMesh* OmniAssets::CylinderMesh() { return LoadPath<UStaticMesh>(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")); }

UMaterialInterface* OmniAssets::MaterialOr(const TCHAR* ProjectPath, const TCHAR* FallbackPath)
{
	if (UMaterialInterface* M = LoadPath<UMaterialInterface>(ProjectPath)) return M;
	return LoadPath<UMaterialInterface>(FallbackPath);
}

UMaterialInterface* OmniAssets::PlanetMaterial() { return MaterialOr(TEXT("/Game/Materials/M_Planet.M_Planet"), TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")); }
UMaterialInterface* OmniAssets::StarMaterial() { return MaterialOr(TEXT("/Game/Materials/M_Star.M_Star"), TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial")); }
UMaterialInterface* OmniAssets::RingMaterial() { return MaterialOr(TEXT("/Game/Materials/M_Rings.M_Rings"), TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")); }
UMaterialInterface* OmniAssets::HullMaterial() { return MaterialOr(TEXT("/Game/Materials/M_Hull.M_Hull"), TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")); }
UMaterialInterface* OmniAssets::SkyMaterial() { return MaterialOr(TEXT("/Game/Materials/M_Sky.M_Sky"), TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial")); }
UMaterialInterface* OmniAssets::EmissiveMaterial() { return MaterialOr(TEXT("/Game/Materials/M_Emissive.M_Emissive"), TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial")); }

UMaterialInstanceDynamic* OmniAssets::MakeMID(UPrimitiveComponent* Comp, UMaterialInterface* Base, UObject* Outer)
{
	if (!Comp || !Base) return nullptr;
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, Outer);
	Comp->SetMaterial(0, MID);
	return MID;
}
