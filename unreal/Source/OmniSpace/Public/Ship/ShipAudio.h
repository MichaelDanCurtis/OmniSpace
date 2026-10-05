// Ship sound: engine/supercruise/hyperspace loops driven by flight state, plus one-shots.
// Sounds are the generated waves in /Game/Audio (see Tools/ue_setup.py, RawAudio/).
#pragma once

#include "CoreMinimal.h"
#include "ShipAudio.generated.h"

class AShipPawn;
class UAudioComponent;
class USoundBase;

UCLASS()
class OMNISPACE_API UShipAudio : public UObject
{
	GENERATED_BODY()
public:
	void Init(AShipPawn* Ship);
	void Update(float Dt);
	/** One-shots: "blip" "deny" "chime" "dock_clunk" "impact" "boost" "fsd_charge" "jump". */
	void Play(const TCHAR* Name, float Volume = 1.f, float Pitch = 1.f);
	void StopCharge();

private:
	UPROPERTY() TObjectPtr<AShipPawn> Owner;
	UPROPERTY() TObjectPtr<UAudioComponent> Engine;
	UPROPERTY() TObjectPtr<UAudioComponent> Cruise;
	UPROPERTY() TObjectPtr<UAudioComponent> Hyper;
	UPROPERTY() TObjectPtr<UAudioComponent> Charge;
	UPROPERTY() TMap<FString, TObjectPtr<USoundBase>> Cache;
	USoundBase* Sound(const TCHAR* Name);
	UAudioComponent* Loop(const TCHAR* Name);
	float EngineVol = 0, CruiseVol = 0, HyperVol = 0;
	int32 LastMode = -1;
	bool bWasBoosting = false, bWasCharging = false;
	double LastImpactT = -10;
};
