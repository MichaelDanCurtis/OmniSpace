// Holographic canvas HUD: brand/location block, flight data panels, scanner, target
// brackets, jump marker, hyperspace cartography. Everything is vector-drawn so it scales
// with the window and needs no UI assets.
#include "Core/OmniHUD.h"
#include "Ship/ShipPawn.h"
#include "World/SystemDirector.h"
#include "Core/OmniGameMode.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "Galaxy/GalaxySubsystem.h"
#include "Forge/ForgeSubsystem.h"
#include "World/SurfaceTile.h"
#include "Ship/RoverPawn.h"

namespace
{
	const double LS = 299792458.0;
	const double AU = 1.496e11;
	FString FmtDist(double M)
	{
		if (M < 1000) return FString::Printf(TEXT("%.0f m"), M);
		if (M < 1e6) return FString::Printf(TEXT("%.1f km"), M / 1000);
		if (M < LS * 0.5) return FString::Printf(TEXT("%.1f Mm"), M / 1e6);
		if (M < AU * 0.5) return FString::Printf(TEXT("%.1f ls"), M / LS);
		return FString::Printf(TEXT("%.2f AU"), M / AU);
	}
	FString FmtSpeed(double V)
	{
		if (V < 1000) return FString::Printf(TEXT("%.0f m/s"), V);
		if (V < LS * 0.5) return FString::Printf(TEXT("%.0f km/s"), V / 1000);
		return FString::Printf(TEXT("%.2f c"), V / LS);
	}
	const FLinearColor HudCyan(0.45f, 0.85f, 1.f, 0.95f);
	const FLinearColor HudCyanDim(0.45f, 0.85f, 1.f, 0.35f);
	const FLinearColor HudAmber(1.f, 0.7f, 0.28f, 0.95f);
	const FLinearColor HudRed(1.f, 0.35f, 0.3f, 0.95f);
	const FLinearColor HudGreen(0.5f, 1.f, 0.6f, 0.95f);
	const FLinearColor HudWhite(0.92f, 0.96f, 1.f, 0.95f);
	const FLinearColor HudPanel(0.05f, 0.14f, 0.22f, 0.45f);
	const FLinearColor HudGrey(0.6f, 0.68f, 0.76f, 0.9f);
}

void AOmniHUD::DrawBuildPanel(AOmniGameMode* GM, UFont* Font, float W, float H, float U)
{
	if (!GM || !GM->Tile) return;
	FOutpostData* D = GM->CurrentOutpostData();
	const float PW = 300 * U, PX = W - 28 * U - PW;
	float Y = 24 * U;
	if (D)
	{
		DrawRect(HudPanel, PX - 8 * U, Y - 6 * U, PW + 16 * U, (34 + 18 * FMath::Min(6, D->Modules.Num())) * U);
		DrawText(D->Name.ToUpper(), HudWhite, PX, Y, Font, 1.05f * U); Y += 20 * U;
		int32 Shown = 0;
		for (const FOutpostModule& M : D->Modules)
		{
			if (Shown++ >= 6) break;
			const FModuleSpec& S = Outpost::Spec(M.Kind);
			FString Line;
			FLinearColor C = HudGreen;
			if (M.bComplete) Line = FString::Printf(TEXT("%s  built"), S.Name);
			else if (M.Supplied()) { Line = FString::Printf(TEXT("%s  building %.0f%%"), S.Name, M.Progress * 100); C = HudAmber; }
			else { const FMaterials R = M.Remaining(); Line = FString::Printf(TEXT("%s  needs %.0f/%.0f/%.0f t"), S.Name, R.Alloy, R.Composites, R.Electronics); C = HudCyanDim; }
			DrawText(Line, C, PX, Y, Font, 0.85f * U); Y += 18 * U;
		}
		Y += 10 * U;
	}
	if (!GM->bBuildMode) return;
	const bool bBeacon = !D || !D->HasBeacon();
	const int32 Rows = bBeacon ? 1 : 6;
	DrawRect(HudPanel, PX - 8 * U, Y - 6 * U, PW + 16 * U, (46 + 18 * Rows) * U);
	DrawText(TEXT("BUILD"), HudAmber, PX, Y, Font, 1.05f * U); Y += 20 * U;
	if (bBeacon) { DrawText(TEXT("Outpost beacon  (Enter to place)"), GM->BuildKind == EModuleKind::Beacon ? HudWhite : HudCyanDim, PX, Y, Font, 0.85f * U); Y += 18 * U; }
	else
	{
		static const EModuleKind Slots[] = { EModuleKind::Hab, EModuleKind::Power, EModuleKind::Storage, EModuleKind::Corridor, EModuleKind::Lab, EModuleKind::Pad };
		for (int32 i = 0; i < 6; i++)
		{
			const FModuleSpec& S = Outpost::Spec(Slots[i]);
			const bool bSel = GM->BuildKind == Slots[i];
			DrawText(FString::Printf(TEXT("%d  %s   %.0f/%.0f/%.0f t"), i + 1, S.Name, S.Cost.Alloy, S.Cost.Composites, S.Cost.Electronics), bSel ? HudWhite : HudCyanDim, PX, Y, Font, 0.85f * U);
			Y += 18 * U;
		}
	}
	DrawText(GM->BuildStatus, GM->bGhostValid ? HudGreen : HudAmber, PX, Y + 4 * U, Font, 0.8f * U);
}

void AOmniHUD::Bar(float X, float Y, float W, float H, double Frac, const FLinearColor& C)
{
	DrawRect(FLinearColor(C.R, C.G, C.B, 0.12f), X, Y, W, H);
	DrawRect(C, X, Y, W * FMath::Clamp(static_cast<float>(Frac), 0.f, 1.f), H);
	DrawLine(X, Y + H, X + W, Y + H, FLinearColor(C.R, C.G, C.B, 0.5f), 1.f);
}

void AOmniHUD::Bracket(float X, float Y, float S, const FLinearColor& C, float Thick)
{
	const float L = S * 0.35f;
	DrawLine(X - S, Y - S, X - S + L, Y - S, C, Thick); DrawLine(X - S, Y - S, X - S, Y - S + L, C, Thick);
	DrawLine(X + S, Y - S, X + S - L, Y - S, C, Thick); DrawLine(X + S, Y - S, X + S, Y - S + L, C, Thick);
	DrawLine(X - S, Y + S, X - S + L, Y + S, C, Thick); DrawLine(X - S, Y + S, X - S, Y + S - L, C, Thick);
	DrawLine(X + S, Y + S, X + S - L, Y + S, C, Thick); DrawLine(X + S, Y + S, X + S, Y + S - L, C, Thick);
}

void AOmniHUD::Diamond(float X, float Y, float S, const FLinearColor& C, float Thick)
{
	DrawLine(X - S, Y, X, Y - S, C, Thick); DrawLine(X, Y - S, X + S, Y, C, Thick);
	DrawLine(X + S, Y, X, Y + S, C, Thick); DrawLine(X, Y + S, X - S, Y, C, Thick);
}

void AOmniHUD::Circle(float X, float Y, float R, const FLinearColor& C, int32 Segs, float Thick, float Squash)
{
	for (int32 i = 0; i < Segs; i++)
	{
		const float A0 = 2 * PI * i / Segs, A1 = 2 * PI * (i + 1) / Segs;
		DrawLine(X + FMath::Cos(A0) * R, Y + FMath::Sin(A0) * R * Squash, X + FMath::Cos(A1) * R, Y + FMath::Sin(A1) * R * Squash, C, Thick);
	}
}

void AOmniHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas) return;
	UFont* Font = GEngine->GetMediumFont();
	if (ARoverPawn* Rover = Cast<ARoverPawn>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0)))
	{
		const float W = Canvas->SizeX, H = Canvas->SizeY;
		const float U = FMath::Clamp(H / 900.f, 0.7f, 1.6f);
		DrawText(TEXT("ROVER"), HudWhite, 28 * U, 24 * U, Font, 1.9f * U);
		DrawText(FString::Printf(TEXT("SPEED %.0f m/s"), Rover->SpeedMS), HudCyan, 28 * U, 60 * U, Font, 1.2f * U);
		DrawText(FString::Printf(TEXT("SHIP %s"), *FmtDist(Rover->DistanceToShipM)), Rover->DistanceToShipM < 25 ? HudGreen : HudAmber, 28 * U, 84 * U, Font, 1.1f * U);
		const int32 N = FMath::Min(4, Rover->Messages.Num());
		float Y = H - 40 * U - 19 * U * N;
		for (int32 i = Rover->Messages.Num() - N; i < Rover->Messages.Num(); i++) { DrawText(Rover->Messages[i], FLinearColor(0.85f, 0.92f, 1.f, 0.95f), 28 * U, Y, Font, 0.95f * U); Y += 19 * U; }
		DrawText(TEXT("W/S drive   A/D steer   Space brake   mouse look   G board the ship (within 25 m)   O build   1-6 module   [ ] rotate   Enter place"), FLinearColor(0.5f, 0.6f, 0.7f, 0.8f), 28 * U, H - 16 * U, Font, 0.8f * U);
		DrawBuildPanel(Cast<AOmniGameMode>(GetWorld()->GetAuthGameMode()), Font, W, H, U);
		return;
	}
	AShipPawn* Ship = Cast<AShipPawn>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
	if (!Ship) return;
	const float W = Canvas->SizeX, H = Canvas->SizeY;
	const float U = FMath::Clamp(H / 900.f, 0.7f, 1.6f); // ui scale
	ASystemDirector* D = Ship->GetDirector();
	AOmniGameMode* GM = Cast<AOmniGameMode>(GetWorld()->GetAuthGameMode());
	const bool bHyper = Ship->Mode == EFlightMode::Hyperspace;
	const FQuat Q = Ship->GetActorQuat();

	// ---------------------------------------------------------------- brand + location
	{
		float X = 28 * U, Y = 24 * U;
		DrawText(TEXT("OMNISPACE"), HudWhite, X, Y, Font, 1.9f * U);
		DrawText(TEXT("NEXUS ASTRA"), HudCyanDim, X + 2, Y + 30 * U, Font, 0.9f * U);
		DrawLine(X, Y + 48 * U, X + 220 * U, Y + 48 * U, HudCyanDim, 1.f);
		Y += 58 * U;
		if (D) DrawText(D->GetSystem().Name.ToUpper() + TEXT(" SYSTEM"), HudCyan, X, Y, Font, 1.35f * U);
		if (D) DrawText(FString::Printf(TEXT("%s  -  %s"), *D->GetSystem().Profile.Faction, *D->GetSystem().Profile.Economy), HudGrey, X, Y + 24 * U, Font, 0.95f * U);
		static const TCHAR* ModeNames[] = {TEXT("NORMAL FLIGHT"), TEXT("SUPERCRUISE"), TEXT("HYPERSPACE"), TEXT("DOCKING"), TEXT("DOCKED"), TEXT("LAUNCHING"), TEXT("DESTROYED"), TEXT("ATMOSPHERIC FLIGHT"), TEXT("LANDED")};
		DrawText(ModeNames[static_cast<int>(Ship->Mode)], Ship->Mode == EFlightMode::Supercruise ? HudAmber : HudCyan, X, Y + 46 * U, Font, 1.1f * U);
		float SY = Y + 72 * U;
		auto Status = [&](const FString& S, const FLinearColor& C) { DrawText(S, C, X, SY, Font, 1.0f * U); SY += 20 * U; };
		if (Ship->FsdKind != EFsdKind::None) Status(FString::Printf(TEXT("FSD CHARGING %3.0f%%%s"), Ship->FsdProgress * 100, Ship->FsdKind == EFsdKind::Jump ? TEXT("  JUMP") : TEXT("")), HudAmber);
		if (Ship->IsMassLocked() && Ship->Mode == EFlightMode::Normal) Status(TEXT("MASS LOCKED"), HudAmber);
		if (Ship->bAutopilot) Status(TEXT("AUTOPILOT"), HudGreen);
		if (!Ship->bFlightAssist) Status(TEXT("FLIGHT ASSIST OFF"), HudAmber);
		if (!Ship->DockingStationId.IsEmpty() && Ship->Mode == EFlightMode::Normal) Status(FString::Printf(TEXT("DOCKING GRANTED  BAY %d"), Ship->DockingBay + 1), HudGreen);
		if (Ship->Mode == EFlightMode::Docked) Status(TEXT("DOCKED  -  L TO LAUNCH"), HudGreen);
		if (Ship->BoostTimer > 0) Status(TEXT("BOOST"), HudAmber);
		if (Ship->Mode == EFlightMode::Surface) Status(FString::Printf(TEXT("ALTITUDE %s   %s"), *FmtDist(FMath::Max(0.0, Ship->AltitudeM)), Ship->Tile ? *Ship->Tile->GetBody().Name.ToUpper() : TEXT("")), HudCyan);
		if (Ship->Mode == EFlightMode::Landed) Status(TEXT("G DEPLOY ROVER   L LIFT OFF"), HudGreen);
		if (Ship->bLandingLights) Status(TEXT("LANDING LIGHTS"), HudWhite);
	}

	// ---------------------------------------------------------------- left: speed / throttle
	if (!bHyper)
	{
		const float PX = 28 * U, PY = H * 0.5f - 90 * U, PW = 170 * U;
		DrawRect(HudPanel, PX - 8 * U, PY - 8 * U, PW + 16 * U, 150 * U);
		DrawText(TEXT("SPEED"), HudCyanDim, PX, PY, Font, 0.85f * U);
		DrawText(FmtSpeed(Ship->CurrentSpeed()), HudWhite, PX, PY + 14 * U, Font, 1.6f * U);
		const double MaxV = Ship->Mode == EFlightMode::Supercruise ? Ship->SupercruiseMax : Ship->BoostSpeed;
		const double SpeedFrac = Ship->Mode == EFlightMode::Supercruise ? FMath::LogX(10.0, FMath::Max(1.0, Ship->CurrentSpeed() / Ship->SupercruiseMin)) / FMath::LogX(10.0, MaxV / Ship->SupercruiseMin) : Ship->CurrentSpeed() / MaxV;
		Bar(PX, PY + 48 * U, PW, 6 * U, SpeedFrac, HudCyan);
		DrawText(TEXT("THROTTLE"), HudCyanDim, PX, PY + 64 * U, Font, 0.85f * U);
		const double T = Ship->Throttle;
		if (T >= 0) Bar(PX, PY + 80 * U, PW, 10 * U, T, (T > 0.45 && T < 0.8) ? HudCyan : HudAmber);
		else Bar(PX, PY + 80 * U, PW, 10 * U, -T, HudRed);
		DrawText(FString::Printf(TEXT("%3.0f%%"), T * 100), HudWhite, PX + PW - 34 * U, PY + 62 * U, Font, 1.0f * U);
		// blue zone marker
		DrawLine(PX + PW * 0.45f, PY + 78 * U, PX + PW * 0.45f, PY + 92 * U, HudCyanDim, 1.f);
		DrawLine(PX + PW * 0.8f, PY + 78 * U, PX + PW * 0.8f, PY + 92 * U, HudCyanDim, 1.f);
		DrawText(Ship->bFlightAssist ? TEXT("FA ON") : TEXT("FA OFF"), Ship->bFlightAssist ? HudCyanDim : HudAmber, PX, PY + 100 * U, Font, 0.9f * U);
		DrawText(FString::Printf(TEXT("BOOST %3.0f%%"), Ship->BoostCharge * 100), HudCyanDim, PX + 70 * U, PY + 100 * U, Font, 0.9f * U);
		Bar(PX, PY + 118 * U, PW, 4 * U, Ship->BoostCharge, HudAmber);
	}

	// ---------------------------------------------------------------- right: ship status
	if (!bHyper)
	{
		const float PW = 170 * U, PX = W - 28 * U - PW, PY = H * 0.5f - 90 * U;
		DrawRect(HudPanel, PX - 8 * U, PY - 8 * U, PW + 16 * U, 150 * U);
		auto Row = [&](const TCHAR* Label, double Frac, const FLinearColor& C, const FString& Val, float RowY)
		{
			DrawText(Label, HudCyanDim, PX, RowY, Font, 0.85f * U);
			DrawText(Val, HudWhite, PX + PW - 48 * U, RowY, Font, 0.9f * U);
			Bar(PX, RowY + 15 * U, PW, 6 * U, Frac, C);
		};
		Row(TEXT("HULL"), Ship->Hull, Ship->Hull < 0.3 ? HudRed : HudCyan, FString::Printf(TEXT("%3.0f%%"), Ship->Hull * 100), PY);
		Row(TEXT("SHIELDS"), Ship->Shields, HudCyan, FString::Printf(TEXT("%3.0f%%"), Ship->Shields * 100), PY + 32 * U);
		Row(TEXT("FUEL"), Ship->Fuel / 16.0, Ship->Fuel < 3 ? HudRed : HudAmber, FString::Printf(TEXT("%4.1f t"), Ship->Fuel), PY + 64 * U);
		Row(TEXT("HEAT"), Ship->Heat, Ship->Heat > 0.8 ? HudRed : HudAmber, FString::Printf(TEXT("%3.0f%%"), Ship->Heat * 100), PY + 96 * U);
		if (Ship->Cargo.Total() > 0.01)
			DrawText(FString::Printf(TEXT("CARGO %.0f t   alloy %.0f  comp %.0f  elec %.0f"), Ship->Cargo.Total(), Ship->Cargo.Alloy, Ship->Cargo.Composites, Ship->Cargo.Electronics), HudCyanDim, PX, PY + 128 * U, Font, 0.8f * U);
		if (Ship->Mode == EFlightMode::Landed || Ship->Mode == EFlightMode::Surface) DrawBuildPanel(GM, Font, W, H, U);
	}

	// ---------------------------------------------------------------- centre: reticle, flight vector, markers
	const float CX = W / 2, CY = H * 0.47f;
	if (!bHyper)
	{
		DrawLine(CX - 18 * U, CY, CX - 7 * U, CY, HudCyan, 1.5f); DrawLine(CX + 7 * U, CY, CX + 18 * U, CY, HudCyan, 1.5f);
		DrawLine(CX, CY - 18 * U, CX, CY - 7 * U, HudCyan, 1.5f); DrawLine(CX, CY + 7 * U, CX, CY + 18 * U, HudCyan, 1.5f);
		// flight vector (where the ship is actually going)
		if (Ship->Mode == EFlightMode::Normal && Ship->Velocity.Size() > 5)
		{
			FVector S = Project(Ship->Velocity.GetSafeNormal() * 1.0e8);
			if (S.Z > 0) Circle(S.X, S.Y, 7 * U, HudCyanDim, 12, 1.f);
		}
		// jump destination marker
		FVector JD;
		if (!Ship->JumpTargetId.IsEmpty() && Ship->JumpDirection(JD))
		{
			FVector S = Project(JD * 1.0e9);
			const bool bAligned = FVector::DotProduct(Q.GetForwardVector(), JD) > FMath::Cos(0.2);
			if (S.Z > 0)
			{
				Diamond(S.X, S.Y, 11 * U, bAligned ? HudGreen : HudAmber, 1.5f);
				DrawText(FString::Printf(TEXT("%s  %.1f ly"), *Ship->JumpTarget.Name.ToUpper(), Ship->JumpDistanceLy()), bAligned ? HudGreen : HudAmber, S.X + 16 * U, S.Y - 8 * U, Font, 1.0f * U);
			}
			else
			{
				// off-screen: arrow at the edge pointing the way
				FVector L = Q.UnrotateVector(JD);
				float Ang = FMath::Atan2(-L.Z, L.Y);
				float EX = CX + FMath::Cos(Ang) * (W * 0.42f), EY = CY + FMath::Sin(Ang) * (H * 0.40f);
				Diamond(EX, EY, 8 * U, HudAmber, 1.5f);
				DrawText(Ship->JumpTarget.Name.ToUpper(), HudAmber, EX - 30 * U, EY + 12 * U, Font, 0.9f * U);
			}
		}
		// target bracket
		if (D && !Ship->TargetId.IsEmpty())
		{
			FVector P; double R;
			if (D->GetEntityPos(Ship->TargetId, P, R))
			{
				const double Dist = FVector::Dist(P, D->GetShipUniversePos()) - (R > 5000 ? R : 0);
				FVector S = Project(D->ToWorld(P));
				const FString Label = FString::Printf(TEXT("%s  %s"), *D->GetEntityName(Ship->TargetId).ToUpper(), *FmtDist(FMath::Max(0.0, Dist)));
				if (S.Z > 0)
				{
					Bracket(S.X, S.Y, 16 * U, HudAmber, 1.5f);
					DrawText(Label, HudAmber, S.X + 22 * U, S.Y - 8 * U, Font, 1.0f * U);
				}
				else
				{
					FVector L = Q.UnrotateVector((P - D->GetShipUniversePos()).GetSafeNormal());
					float Ang = FMath::Atan2(-L.Z, L.Y);
					float EX = CX + FMath::Cos(Ang) * (W * 0.42f), EY = CY + FMath::Sin(Ang) * (H * 0.40f);
					Bracket(EX, EY, 8 * U, HudAmber, 1.2f);
					DrawText(Label, HudAmber, EX - 40 * U, EY + 12 * U, Font, 0.85f * U);
				}
			}
		}
	}

	// ---------------------------------------------------------------- bottom centre: scanner
	if (!bHyper && D)
	{
		const float SX = CX, SY = H - 118 * U, SR = 95 * U, Squash = 0.42f;
		DrawRect(HudPanel, SX - SR - 12 * U, SY - SR * Squash - 26 * U, SR * 2 + 24 * U, SR * Squash * 2 + 52 * U);
		Circle(SX, SY, SR, HudCyanDim, 40, 1.f, Squash);
		Circle(SX, SY, SR * 0.5f, FLinearColor(HudCyan.R, HudCyan.G, HudCyan.B, 0.2f), 32, 1.f, Squash);
		DrawLine(SX - SR, SY, SX + SR, SY, FLinearColor(HudCyan.R, HudCyan.G, HudCyan.B, 0.2f), 1.f);
		DrawLine(SX, SY - SR * Squash, SX, SY + SR * Squash, FLinearColor(HudCyan.R, HudCyan.G, HudCyan.B, 0.2f), 1.f);
		DrawText(TEXT("SCANNER"), HudCyanDim, SX - SR, SY - SR * Squash - 20 * U, Font, 0.8f * U);
		// log-scaled range: 10 km .. 1 AU
		const FVector Ship0 = D->GetShipUniversePos();
		for (const FString& Id : D->GetEntityIds())
		{
			FVector P; double R;
			if (!D->GetEntityPos(Id, P, R)) continue;
			FVector Rel = Q.UnrotateVector(P - Ship0); // X fwd, Y right, Z up (m)
			const double Dist = Rel.Size();
			if (Dist < 1) continue;
			const double Rad = FMath::Clamp(FMath::LogX(10.0, FMath::Max(Dist / 1e4, 1.0)) / 7.2, 0.0, 1.0); // 1e4..1.5e11
			FVector Dir = Rel / Dist;
			const float PX = SX + Dir.Y * Rad * SR, PY = SY - Dir.X * Rad * SR * Squash;
			const float ZH = -Dir.Z * Rad * SR * Squash * 0.9f; // stalk for up/down
			const FString Kind = D->GetEntityKind(Id);
			const FLinearColor C = Kind == TEXT("star") ? HudAmber : Kind == TEXT("station") ? HudGreen : Kind == TEXT("moon") ? HudGrey : HudCyan;
			DrawLine(PX, PY, PX, PY + ZH, FLinearColor(C.R, C.G, C.B, 0.5f), 1.f);
			if (Kind == TEXT("station")) Diamond(PX, PY + ZH, 4 * U, C, 1.5f);
			else DrawRect(C, PX - 2 * U, PY + ZH - 2 * U, 4 * U, 4 * U);
			if (Id == Ship->TargetId) Circle(PX, PY + ZH, 7 * U, HudAmber, 10, 1.f);
		}
		DrawRect(HudWhite, SX - 2 * U, SY - 2 * U, 4 * U, 4 * U);
	}

	// ---------------------------------------------------------------- hyperspace
	if (bHyper)
	{
		DrawText(FString::Printf(TEXT("HYPERSPACE   >>   %s"), *Ship->JumpTarget.Name.ToUpper()), HudWhite, CX - 180 * U, H * 0.28f, Font, 1.7f * U);
		if (GM)
		{
			float LY = H * 0.28f + 44 * U;
			const int32 Shown = FMath::Min(GM->JumpLog.Num(), FMath::FloorToInt32((Ship->HyperT - 0.8) / 0.55) + 1);
			for (int32 i = 0; i < Shown; i++) { DrawText(GM->JumpLog[i], HudCyan, CX - 180 * U, LY, Font, 1.0f * U); LY += 20 * U; }
			if (!GM->IsJumpReady()) DrawText(TEXT("Stellar Forge charting the system..."), HudGrey, CX - 180 * U, LY + 10 * U, Font, 1.0f * U);
		}
	}

	// ---------------------------------------------------------------- galaxy map overlay (M)
	if (Ship->bShowMap && GM && !bHyper)
	{
		UGalaxySubsystem* Galaxy = GetGameInstance() ? GetGameInstance()->GetSubsystem<UGalaxySubsystem>() : nullptr;
		UForgeSubsystem* Forge = GetGameInstance() ? GetGameInstance()->GetSubsystem<UForgeSubsystem>() : nullptr;
		if (Galaxy)
		{
			const float MW = FMath::Min(W * 0.62f, 980 * U), MH = FMath::Min(H * 0.70f, 620 * U);
			const float MX = CX - MW / 2, MY = H * 0.47f - MH / 2;
			DrawRect(FLinearColor(0.02f, 0.06f, 0.1f, 0.88f), MX, MY, MW, MH);
			DrawLine(MX, MY, MX + MW, MY, HudCyan, 1.5f); DrawLine(MX, MY + MH, MX + MW, MY + MH, HudCyan, 1.5f);
			DrawText(TEXT("GALAXY MAP"), HudWhite, MX + 16 * U, MY + 12 * U, Font, 1.4f * U);
			DrawText(TEXT("N next star in range   B plot route to a far star   H jump   M close"), HudGrey, MX + 16 * U, MY + 36 * U, Font, 0.9f * U);
			const FStarDescriptor& Cur = GM->CurrentStar;
			const double RangeLy = 42.0;
			const float Scale = (FMath::Min(MW, MH) * 0.46f) / static_cast<float>(RangeLy);
			const float OX = MX + MW * 0.5f, OY = MY + MH * 0.54f;
			// galactic plane top-down: TS x -> right, TS z -> up on the map; TS y (height) as a stalk
			Circle(OX, OY, static_cast<float>(Ship->JumpRangeLy) * Scale, FLinearColor(HudCyan.R, HudCyan.G, HudCyan.B, 0.45f), 48, 1.f);
			Circle(OX, OY, 30.f * Scale, FLinearColor(HudCyan.R, HudCyan.G, HudCyan.B, 0.12f), 48, 1.f);
			DrawText(FString::Printf(TEXT("%.0f ly"), Ship->JumpRangeLy), HudCyanDim, OX + static_cast<float>(Ship->JumpRangeLy) * Scale + 4 * U, OY - 6 * U, Font, 0.8f * U);
			TArray<FStarDescriptor> Stars = Galaxy->StarsNear(Cur.Pos, RangeLy);
			// route lines
			FVector Prev = Cur.Pos;
			for (const FString& Hop : Ship->Route)
			{
				FStarDescriptor HS;
				if (!Galaxy->GetStar(Hop, HS)) break;
				DrawLine(OX + (Prev.X - Cur.Pos.X) * Scale, OY - (Prev.Z - Cur.Pos.Z) * Scale, OX + (HS.Pos.X - Cur.Pos.X) * Scale, OY - (HS.Pos.Z - Cur.Pos.Z) * Scale, HudAmber, 1.5f);
				Prev = HS.Pos;
			}
			for (const FStarDescriptor& S : Stars)
			{
				const float PX = OX + static_cast<float>(S.Pos.X - Cur.Pos.X) * Scale;
				const float PY = OY - static_cast<float>(S.Pos.Z - Cur.Pos.Z) * Scale;
				const float Dy = static_cast<float>(S.Pos.Y - Cur.Pos.Y) * Scale * 0.3f;
				const double DistLy = FVector::Dist(S.Pos, Cur.Pos);
				const bool bCur = S.Id == Cur.Id;
				const bool bTarget = S.Id == Ship->JumpTargetId;
				const bool bDest = S.Id == Ship->RouteDestId;
				FStarSystemData Tmp;
				const bool bCharted = Forge && Forge->GetCached(S.Id, Tmp);
				FLinearColor C = S.LinearColor(); C.A = DistLy <= Ship->JumpRangeLy || bCur || bTarget || bDest ? 1.f : 0.35f;
				if (FMath::Abs(Dy) > 1.f) DrawLine(PX, PY, PX, PY - Dy, FLinearColor(C.R, C.G, C.B, 0.25f), 1.f);
				const float Sz = (bCur ? 5.f : 2.f + static_cast<float>(FMath::Clamp(S.RadiusSolar, 0.2, 2.0))) * U;
				DrawRect(C, PX - Sz / 2, PY - Dy - Sz / 2, Sz, Sz);
				if (bCur) Circle(PX, PY - Dy, 10 * U, HudCyan, 16, 1.5f);
				if (bTarget) Diamond(PX, PY - Dy, 10 * U, HudGreen, 1.5f);
				if (bDest) Bracket(PX, PY - Dy, 10 * U, HudAmber, 1.5f);
				if (bCharted && !bCur) Circle(PX, PY - Dy, 6 * U, FLinearColor(HudWhite.R, HudWhite.G, HudWhite.B, 0.5f), 10, 1.f);
				if (bCur || bTarget || bDest || DistLy <= Ship->JumpRangeLy || S.Settlement > 0.8)
					DrawText(FString::Printf(TEXT("%s  %.1f"), *S.Name, DistLy), bCur ? HudCyan : (bTarget ? HudGreen : FLinearColor(0.85f, 0.9f, 1.f, C.A)), PX + 8 * U, PY - Dy - 6 * U, Font, 0.8f * U);
			}
			// legend
			float LY = MY + MH - 22 * U;
			DrawText(TEXT("ring: current   diamond: next jump   bracket: route destination   dot ring: charted   stalk: height above/below the galactic plane"), HudGrey, MX + 16 * U, LY, Font, 0.8f * U);
			if (!Ship->JumpTargetId.IsEmpty())
				DrawText(FString::Printf(TEXT("NEXT JUMP  %s  %.1f ly  %s%s"), *Ship->JumpTarget.Name.ToUpper(), Ship->JumpDistanceLy(), *UGalaxySubsystem::ClassLabel(Ship->JumpTarget.Cls), Ship->Route.Num() > 1 ? *FString::Printf(TEXT("  (%d jumps to go)"), Ship->Route.Num()) : TEXT("")), HudGreen, MX + 16 * U, LY - 22 * U, Font, 1.0f * U);
		}
	}

	// ---------------------------------------------------------------- messages + key help
	{
		const int32 N = FMath::Min(5, Ship->Messages.Num());
		float Y = H - 40 * U - 19 * U * N;
		for (int32 i = Ship->Messages.Num() - N; i < Ship->Messages.Num(); i++)
		{
			const float Age = static_cast<float>(Ship->Messages.Num() - 1 - i);
			DrawText(Ship->Messages[i], FLinearColor(0.85f, 0.92f, 1.f, 0.95f - Age * 0.15f), 28 * U, Y, Font, 0.95f * U);
			Y += 19 * U;
		}
		DrawText(TEXT("W/S throttle   mouse/arrows steer   A/D roll   Q/E R/F thrusters   Tab boost   J supercruise   T target   N jump target   B plot route   M map   H jump   P autopilot   L dock/land/launch   G rover   K lights   O build   U cargo   Z FA   X stop   C 75%"), FLinearColor(0.5f, 0.6f, 0.7f, 0.8f), 28 * U, H - 16 * U, Font, 0.8f * U);
	}
}
