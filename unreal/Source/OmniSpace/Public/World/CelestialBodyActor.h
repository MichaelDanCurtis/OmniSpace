#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/StarSystemData.h"
#include "CelestialBodyActor.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;

/** A planet or moon: sphere + optional ring disc, driven by material parameters. */
UCLASS()
class OMNISPACE_API ACelestialBodyActor : public AActor
{
	GENERATED_BODY()
public:
	ACelestialBodyActor();
	void Setup(const FBodyData& InBody);
	/** Shrink factor used by the floating-origin far compression. */
	void SetRenderScale(double Scale);
	/** SunWorldPos in cm (world space), Time in seconds. */
	void UpdateUniforms(const FVector& SunWorldPos, double Time);

	UPROPERTY(BlueprintReadOnly, Category = "OmniSpace") FBodyData Body;

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Tilt;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Surface;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Rings;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> SurfaceMID;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> RingMID;
	double RenderScale = 1.0;
	double Spin = 0.0;

	static int32 TypeIndex(const FString& Type);
};
