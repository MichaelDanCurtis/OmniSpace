// Asset lookups with engine fallbacks, so the C++ runs before any project
// content exists (materials are created later with the editor Python tooling).
#pragma once

#include "CoreMinimal.h"

class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UPrimitiveComponent;

namespace OmniAssets
{
	OMNISPACE_API UStaticMesh* SphereMesh();
	OMNISPACE_API UStaticMesh* PlaneMesh();
	OMNISPACE_API UStaticMesh* CubeMesh();
	OMNISPACE_API UStaticMesh* CylinderMesh();
	OMNISPACE_API UMaterialInterface* PlanetMaterial();
	OMNISPACE_API UMaterialInterface* StarMaterial();
	OMNISPACE_API UMaterialInterface* RingMaterial();
	OMNISPACE_API UMaterialInterface* HullMaterial();
	OMNISPACE_API UMaterialInterface* EmissiveMaterial();
	/** Load /Game/... first, else the engine fallback. */
	OMNISPACE_API UMaterialInterface* MaterialOr(const TCHAR* ProjectPath, const TCHAR* FallbackPath);
	OMNISPACE_API UMaterialInstanceDynamic* MakeMID(UPrimitiveComponent* Comp, UMaterialInterface* Base, UObject* Outer);
}
