#include "Ship/ShipAudio.h"
#include "Ship/ShipPawn.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "UObject/UObjectGlobals.h"

USoundBase* UShipAudio::Sound(const TCHAR* Name)
{
	if (TObjectPtr<USoundBase>* Found = Cache.Find(Name)) return *Found;
	USoundBase* S = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Audio/%s.%s"), Name, Name));
	Cache.Add(Name, S);
	return S;
}

UAudioComponent* UShipAudio::Loop(const TCHAR* Name)
{
	USoundBase* S = Sound(Name);
	if (!S || !Owner) return nullptr;
	UAudioComponent* C = UGameplayStatics::SpawnSound2D(Owner->GetWorld(), S, 0.f, 1.f, 0.f, nullptr, true, false);
	if (C) { C->bAutoDestroy = false; C->SetVolumeMultiplier(0.f); }
	return C;
}

void UShipAudio::Init(AShipPawn* Ship)
{
	Owner = Ship;
	Engine = Loop(TEXT("engine_loop"));
	Cruise = Loop(TEXT("supercruise_loop"));
	Hyper = Loop(TEXT("hyperspace_loop"));
}

void UShipAudio::Play(const TCHAR* Name, float Volume, float Pitch)
{
	if (!Owner) return;
	if (USoundBase* S = Sound(Name)) UGameplayStatics::PlaySound2D(Owner->GetWorld(), S, Volume, Pitch);
}

void UShipAudio::StopCharge()
{
	if (Charge) { Charge->Stop(); Charge = nullptr; }
}

void UShipAudio::Update(float Dt)
{
	if (!Owner) return;
	const EFlightMode M = Owner->Mode;
	const bool bNormalish = M == EFlightMode::Normal || M == EFlightMode::Docking || M == EFlightMode::Launching;
	const double Thr = FMath::Abs(Owner->Throttle);
	const double SpeedN = FMath::Clamp(Owner->Velocity.Size() / Owner->MaxSpeed, 0.0, 1.5);
	const bool bBoost = Owner->BoostTimer > 0;
	const float TargetEngine = bNormalish ? static_cast<float>(0.18 + 0.5 * Thr + 0.25 * SpeedN + (bBoost ? 0.3 : 0)) : (M == EFlightMode::Docked ? 0.08f : 0.f);
	const float TargetCruise = M == EFlightMode::Supercruise ? static_cast<float>(0.5 + 0.3 * FMath::Clamp(FMath::LogX(10.0, FMath::Max(1.0, Owner->SupercruiseSpeed / 3e4)) / 7.0, 0.0, 1.0)) : 0.f;
	const float TargetHyper = M == EFlightMode::Hyperspace ? 0.9f : 0.f;
	const float K = 1.f - FMath::Exp(-Dt * 4.f);
	EngineVol += (TargetEngine - EngineVol) * K;
	CruiseVol += (TargetCruise - CruiseVol) * K;
	HyperVol += (TargetHyper - HyperVol) * K;
	if (Engine) { Engine->SetVolumeMultiplier(EngineVol); Engine->SetPitchMultiplier(static_cast<float>(0.85 + 0.45 * SpeedN + (bBoost ? 0.25 : 0))); }
	if (Cruise) Cruise->SetVolumeMultiplier(CruiseVol);
	if (Hyper) Hyper->SetVolumeMultiplier(HyperVol);
	// events
	if (bBoost && !bWasBoosting) Play(TEXT("boost"), 0.9f);
	bWasBoosting = bBoost;
	const bool bCharging = Owner->FsdKind != EFsdKind::None;
	if (bCharging && !bWasCharging)
	{
		if (USoundBase* S = Sound(TEXT("fsd_charge")))
		{
			Charge = UGameplayStatics::SpawnSound2D(Owner->GetWorld(), S, 0.8f, Owner->FsdKind == EFsdKind::Jump ? 1.f : 1.4f);
		}
	}
	if (!bCharging && bWasCharging) StopCharge();
	bWasCharging = bCharging;
	const int32 ModeI = static_cast<int32>(M);
	if (ModeI != LastMode)
	{
		if (M == EFlightMode::Supercruise || M == EFlightMode::Hyperspace) Play(TEXT("jump"), M == EFlightMode::Hyperspace ? 1.f : 0.6f, M == EFlightMode::Hyperspace ? 0.8f : 1.2f);
		if (M == EFlightMode::Docked) Play(TEXT("dock_clunk"), 1.f);
		if (M == EFlightMode::Docking) Play(TEXT("chime"), 0.7f);
		if (LastMode == static_cast<int32>(EFlightMode::Hyperspace)) Play(TEXT("jump"), 0.8f, 1.1f);
		LastMode = ModeI;
	}
	if (Owner->LastImpactTime > LastImpactT + 0.05)
	{
		LastImpactT = Owner->LastImpactTime;
		Play(TEXT("impact"), 0.9f, 0.9f + 0.2f * FMath::FRand());
	}
}
