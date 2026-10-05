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
	const FLinearColor Cyan(0.45f, 0.85f, 1.f, 0.95f);
	const FLinearColor CyanDim(0.45f, 0.85f, 1.f, 0.35f);
	const FLinearColor Amber(1.f, 0.7f, 0.28f, 0.95f);
	const FLinearColor Red(1.f, 0.35f, 0.3f, 0.95f);
	const FLinearColor Green(0.5f, 1.f, 0.6f, 0.95f);
	const FLinearColor White(0.92f, 0.96f, 1.f, 0.95f);
	const FLinearColor Panel(0.05f, 0.14f, 0.22f, 0.45f);
	const FLinearColor Grey(0.6f, 0.68f, 0.76f, 0.9f);
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
	AShipPawn* Ship = Cast<AShipPawn>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
	if (!Ship || !Canvas) return;
	UFont* Font = GEngine->GetMediumFont();
	const float W = Canvas->SizeX, H = Canvas->SizeY;
	const float U = FMath::Clamp(H / 900.f, 0.7f, 1.6f); // ui scale
	ASystemDirector* D = Ship->GetDirector();
	AOmniGameMode* GM = Cast<AOmniGameMode>(GetWorld()->GetAuthGameMode());
	const bool bHyper = Ship->Mode == EFlightMode::Hyperspace;
	const FQuat Q = Ship->GetActorQuat();

	// ---------------------------------------------------------------- brand + location
	{
		float X = 28 * U, Y = 24 * U;
		DrawText(TEXT("OMNISPACE"), White, X, Y, Font, 1.9f * U);
		DrawText(TEXT("NEXUS ASTRA"), CyanDim, X + 2, Y + 30 * U, Font, 0.9f * U);
		DrawLine(X, Y + 48 * U, X + 220 * U, Y + 48 * U, CyanDim, 1.f);
		Y += 58 * U;
		if (D) DrawText(D->GetSystem().Name.ToUpper() + TEXT(" SYSTEM"), Cyan, X, Y, Font, 1.35f * U);
		if (D) DrawText(FString::Printf(TEXT("%s  -  %s"), *D->GetSystem().Profile.Faction, *D->GetSystem().Profile.Economy), Grey, X, Y + 24 * U, Font, 0.95f * U);
		static const TCHAR* ModeNames[] = {TEXT("NORMAL FLIGHT"), TEXT("SUPERCRUISE"), TEXT("HYPERSPACE"), TEXT("DOCKING"), TEXT("DOCKED"), TEXT("LAUNCHING"), TEXT("DESTROYED")};
		DrawText(ModeNames[static_cast<int>(Ship->Mode)], Ship->Mode == EFlightMode::Supercruise ? Amber : Cyan, X, Y + 46 * U, Font, 1.1f * U);
		float SY = Y + 72 * U;
		auto Status = [&](const FString& S, const FLinearColor& C) { DrawText(S, C, X, SY, Font, 1.0f * U); SY += 20 * U; };
		if (Ship->FsdKind != EFsdKind::None) Status(FString::Printf(TEXT("FSD CHARGING %3.0f%%%s"), Ship->FsdProgress * 100, Ship->FsdKind == EFsdKind::Jump ? TEXT("  JUMP") : TEXT("")), Amber);
		if (Ship->IsMassLocked() && Ship->Mode == EFlightMode::Normal) Status(TEXT("MASS LOCKED"), Amber);
		if (Ship->bAutopilot) Status(TEXT("AUTOPILOT"), Green);
		if (!Ship->bFlightAssist) Status(TEXT("FLIGHT ASSIST OFF"), Amber);
		if (!Ship->DockingStationId.IsEmpty() && Ship->Mode == EFlightMode::Normal) Status(FString::Printf(TEXT("DOCKING GRANTED  BAY %d"), Ship->DockingBay + 1), Green);
		if (Ship->Mode == EFlightMode::Docked) Status(TEXT("DOCKED  -  L TO LAUNCH"), Green);
		if (Ship->BoostTimer > 0) Status(TEXT("BOOST"), Amber);
	}

	// ---------------------------------------------------------------- left: speed / throttle
	if (!bHyper)
	{
		const float PX = 28 * U, PY = H * 0.5f - 90 * U, PW = 170 * U;
		DrawRect(Panel, PX - 8 * U, PY - 8 * U, PW + 16 * U, 150 * U);
		DrawText(TEXT("SPEED"), CyanDim, PX, PY, Font, 0.85f * U);
		DrawText(FmtSpeed(Ship->CurrentSpeed()), White, PX, PY + 14 * U, Font, 1.6f * U);
		const double MaxV = Ship->Mode == EFlightMode::Supercruise ? Ship->SupercruiseMax : Ship->BoostSpeed;
		const double SpeedFrac = Ship->Mode == EFlightMode::Supercruise ? FMath::LogX(10.0, FMath::Max(1.0, Ship->CurrentSpeed() / Ship->SupercruiseMin)) / FMath::LogX(10.0, MaxV / Ship->SupercruiseMin) : Ship->CurrentSpeed() / MaxV;
		Bar(PX, PY + 48 * U, PW, 6 * U, SpeedFrac, Cyan);
		DrawText(TEXT("THROTTLE"), CyanDim, PX, PY + 64 * U, Font, 0.85f * U);
		const double T = Ship->Throttle;
		if (T >= 0) Bar(PX, PY + 80 * U, PW, 10 * U, T, (T > 0.45 && T < 0.8) ? Cyan : Amber);
		else Bar(PX, PY + 80 * U, PW, 10 * U, -T, Red);
		DrawText(FString::Printf(TEXT("%3.0f%%"), T * 100), White, PX + PW - 34 * U, PY + 62 * U, Font, 1.0f * U);
		// blue zone marker
		DrawLine(PX + PW * 0.45f, PY + 78 * U, PX + PW * 0.45f, PY + 92 * U, CyanDim, 1.f);
		DrawLine(PX + PW * 0.8f, PY + 78 * U, PX + PW * 0.8f, PY + 92 * U, CyanDim, 1.f);
		DrawText(Ship->bFlightAssist ? TEXT("FA ON") : TEXT("FA OFF"), Ship->bFlightAssist ? CyanDim : Amber, PX, PY + 100 * U, Font, 0.9f * U);
		DrawText(FString::Printf(TEXT("BOOST %3.0f%%"), Ship->BoostCharge * 100), CyanDim, PX + 70 * U, PY + 100 * U, Font, 0.9f * U);
		Bar(PX, PY + 118 * U, PW, 4 * U, Ship->BoostCharge, Amber);
	}

	// ---------------------------------------------------------------- right: ship status
	if (!bHyper)
	{
		const float PW = 170 * U, PX = W - 28 * U - PW, PY = H * 0.5f - 90 * U;
		DrawRect(Panel, PX - 8 * U, PY - 8 * U, PW + 16 * U, 150 * U);
		auto Row = [&](const TCHAR* Label, double Frac, const FLinearColor& C, const FString& Val, float RowY)
		{
			DrawText(Label, CyanDim, PX, RowY, Font, 0.85f * U);
			DrawText(Val, White, PX + PW - 48 * U, RowY, Font, 0.9f * U);
			Bar(PX, RowY + 15 * U, PW, 6 * U, Frac, C);
		};
		Row(TEXT("HULL"), Ship->Hull, Ship->Hull < 0.3 ? Red : Cyan, FString::Printf(TEXT("%3.0f%%"), Ship->Hull * 100), PY);
		Row(TEXT("SHIELDS"), Ship->Shields, Cyan, FString::Printf(TEXT("%3.0f%%"), Ship->Shields * 100), PY + 32 * U);
		Row(TEXT("FUEL"), Ship->Fuel / 16.0, Ship->Fuel < 3 ? Red : Amber, FString::Printf(TEXT("%4.1f t"), Ship->Fuel), PY + 64 * U);
		Row(TEXT("HEAT"), Ship->Heat, Ship->Heat > 0.8 ? Red : Amber, FString::Printf(TEXT("%3.0f%%"), Ship->Heat * 100), PY + 96 * U);
	}

	// ---------------------------------------------------------------- centre: reticle, flight vector, markers
	const float CX = W / 2, CY = H * 0.47f;
	if (!bHyper)
	{
		DrawLine(CX - 18 * U, CY, CX - 7 * U, CY, Cyan, 1.5f); DrawLine(CX + 7 * U, CY, CX + 18 * U, CY, Cyan, 1.5f);
		DrawLine(CX, CY - 18 * U, CX, CY - 7 * U, Cyan, 1.5f); DrawLine(CX, CY + 7 * U, CX, CY + 18 * U, Cyan, 1.5f);
		// flight vector (where the ship is actually going)
		if (Ship->Mode == EFlightMode::Normal && Ship->Velocity.Size() > 5)
		{
			FVector S = Project(Ship->Velocity.GetSafeNormal() * 1.0e8);
			if (S.Z > 0) Circle(S.X, S.Y, 7 * U, CyanDim, 12, 1.f);
		}
		// jump destination marker
		FVector JD;
		if (!Ship->JumpTargetId.IsEmpty() && Ship->JumpDirection(JD))
		{
			FVector S = Project(JD * 1.0e9);
			const bool bAligned = FVector::DotProduct(Q.GetForwardVector(), JD) > FMath::Cos(0.2);
			if (S.Z > 0)
			{
				Diamond(S.X, S.Y, 11 * U, bAligned ? Green : Amber, 1.5f);
				DrawText(FString::Printf(TEXT("%s  %.1f ly"), *Ship->JumpTarget.Name.ToUpper(), Ship->JumpDistanceLy()), bAligned ? Green : Amber, S.X + 16 * U, S.Y - 8 * U, Font, 1.0f * U);
			}
			else
			{
				// off-screen: arrow at the edge pointing the way
				FVector L = Q.UnrotateVector(JD);
				float Ang = FMath::Atan2(-L.Z, L.Y);
				float EX = CX + FMath::Cos(Ang) * (W * 0.42f), EY = CY + FMath::Sin(Ang) * (H * 0.40f);
				Diamond(EX, EY, 8 * U, Amber, 1.5f);
				DrawText(Ship->JumpTarget.Name.ToUpper(), Amber, EX - 30 * U, EY + 12 * U, Font, 0.9f * U);
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
				const FString Label = FString::Printf(TEXT("%s  %s"), *Ship->TargetId.ToUpper(), *FmtDist(FMath::Max(0.0, Dist)));
				if (S.Z > 0)
				{
					Bracket(S.X, S.Y, 16 * U, Amber, 1.5f);
					DrawText(Label, Amber, S.X + 22 * U, S.Y - 8 * U, Font, 1.0f * U);
				}
				else
				{
					FVector L = Q.UnrotateVector((P - D->GetShipUniversePos()).GetSafeNormal());
					float Ang = FMath::Atan2(-L.Z, L.Y);
					float EX = CX + FMath::Cos(Ang) * (W * 0.42f), EY = CY + FMath::Sin(Ang) * (H * 0.40f);
					Bracket(EX, EY, 8 * U, Amber, 1.2f);
					DrawText(Label, Amber, EX - 40 * U, EY + 12 * U, Font, 0.85f * U);
				}
			}
		}
	}

	// ---------------------------------------------------------------- bottom centre: scanner
	if (!bHyper && D)
	{
		const float SX = CX, SY = H - 118 * U, SR = 95 * U, Squash = 0.42f;
		DrawRect(Panel, SX - SR - 12 * U, SY - SR * Squash - 26 * U, SR * 2 + 24 * U, SR * Squash * 2 + 52 * U);
		Circle(SX, SY, SR, CyanDim, 40, 1.f, Squash);
		Circle(SX, SY, SR * 0.5f, FLinearColor(Cyan.R, Cyan.G, Cyan.B, 0.2f), 32, 1.f, Squash);
		DrawLine(SX - SR, SY, SX + SR, SY, FLinearColor(Cyan.R, Cyan.G, Cyan.B, 0.2f), 1.f);
		DrawLine(SX, SY - SR * Squash, SX, SY + SR * Squash, FLinearColor(Cyan.R, Cyan.G, Cyan.B, 0.2f), 1.f);
		DrawText(TEXT("SCANNER"), CyanDim, SX - SR, SY - SR * Squash - 20 * U, Font, 0.8f * U);
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
			const FLinearColor C = Kind == TEXT("star") ? Amber : Kind == TEXT("station") ? Green : Kind == TEXT("moon") ? Grey : Cyan;
			DrawLine(PX, PY, PX, PY + ZH, FLinearColor(C.R, C.G, C.B, 0.5f), 1.f);
			if (Kind == TEXT("station")) Diamond(PX, PY + ZH, 4 * U, C, 1.5f);
			else DrawRect(C, PX - 2 * U, PY + ZH - 2 * U, 4 * U, 4 * U);
			if (Id == Ship->TargetId) Circle(PX, PY + ZH, 7 * U, Amber, 10, 1.f);
		}
		DrawRect(White, SX - 2 * U, SY - 2 * U, 4 * U, 4 * U);
	}

	// ---------------------------------------------------------------- hyperspace
	if (bHyper)
	{
		DrawText(FString::Printf(TEXT("HYPERSPACE   >>   %s"), *Ship->JumpTarget.Name.ToUpper()), White, CX - 180 * U, H * 0.28f, Font, 1.7f * U);
		if (GM)
		{
			float LY = H * 0.28f + 44 * U;
			const int32 Shown = FMath::Min(GM->JumpLog.Num(), FMath::FloorToInt32((Ship->HyperT - 0.8) / 0.55) + 1);
			for (int32 i = 0; i < Shown; i++) { DrawText(GM->JumpLog[i], Cyan, CX - 180 * U, LY, Font, 1.0f * U); LY += 20 * U; }
			if (!GM->IsJumpReady()) DrawText(TEXT("Stellar Forge charting the system..."), Grey, CX - 180 * U, LY + 10 * U, Font, 1.0f * U);
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
			DrawLine(MX, MY, MX + MW, MY, Cyan, 1.5f); DrawLine(MX, MY + MH, MX + MW, MY + MH, Cyan, 1.5f);
			DrawText(TEXT("GALAXY MAP"), White, MX + 16 * U, MY + 12 * U, Font, 1.4f * U);
			DrawText(TEXT("N next star in range   B plot route to a far star   H jump   M close"), Grey, MX + 16 * U, MY + 36 * U, Font, 0.9f * U);
			const FStarDescriptor& Cur = GM->CurrentStar;
			const double RangeLy = 60.0;
			const float Scale = (FMath::Min(MW, MH) * 0.46f) / static_cast<float>(RangeLy);
			const float OX = MX + MW * 0.5f, OY = MY + MH * 0.54f;
			// galactic plane top-down: TS x -> right, TS z -> up on the map; TS y (height) as a stalk
			Circle(OX, OY, static_cast<float>(Ship->JumpRangeLy) * Scale, FLinearColor(Cyan.R, Cyan.G, Cyan.B, 0.45f), 48, 1.f);
			Circle(OX, OY, 30.f * Scale, FLinearColor(Cyan.R, Cyan.G, Cyan.B, 0.12f), 48, 1.f);
			DrawText(FString::Printf(TEXT("%.0f ly"), Ship->JumpRangeLy), CyanDim, OX + static_cast<float>(Ship->JumpRangeLy) * Scale + 4 * U, OY - 6 * U, Font, 0.8f * U);
			TArray<FStarDescriptor> Stars = Galaxy->StarsNear(Cur.Pos, RangeLy);
			// route lines
			FVector Prev = Cur.Pos;
			for (const FString& Hop : Ship->Route)
			{
				FStarDescriptor HS;
				if (!Galaxy->GetStar(Hop, HS)) break;
				DrawLine(OX + (Prev.X - Cur.Pos.X) * Scale, OY - (Prev.Z - Cur.Pos.Z) * Scale, OX + (HS.Pos.X - Cur.Pos.X) * Scale, OY - (HS.Pos.Z - Cur.Pos.Z) * Scale, Amber, 1.5f);
				Prev = HS.Pos;
			}
			for (const FStarDescriptor& S : Stars)
			{
				const float PX = OX + static_cast<float>(S.Pos.X - Cur.Pos.X) * Scale;
				const float PY = OY - static_cast<float>(S.Pos.Z - Cur.Pos.Z) * Scale;
				const float Dy = static_cast<float>(S.Pos.Y - Cur.Pos.Y) * Scale * 0.5f;
				const double DistLy = FVector::Dist(S.Pos, Cur.Pos);
				const bool bCur = S.Id == Cur.Id;
				const bool bTarget = S.Id == Ship->JumpTargetId;
				const bool bDest = S.Id == Ship->RouteDestId;
				FStarSystemData Tmp;
				const bool bCharted = Forge && Forge->GetCached(S.Id, Tmp);
				FLinearColor C = S.LinearColor(); C.A = DistLy <= Ship->JumpRangeLy || bCur ? 1.f : 0.55f;
				if (FMath::Abs(Dy) > 1.f) DrawLine(PX, PY, PX, PY - Dy, FLinearColor(C.R, C.G, C.B, 0.25f), 1.f);
				const float Sz = (bCur ? 5.f : 2.5f + static_cast<float>(FMath::Clamp(S.RadiusSolar, 0.2, 3.0))) * U;
				DrawRect(C, PX - Sz / 2, PY - Dy - Sz / 2, Sz, Sz);
				if (bCur) Circle(PX, PY - Dy, 10 * U, Cyan, 16, 1.5f);
				if (bTarget) Diamond(PX, PY - Dy, 10 * U, Green, 1.5f);
				if (bDest) Bracket(PX, PY - Dy, 10 * U, Amber, 1.5f);
				if (bCharted && !bCur) Circle(PX, PY - Dy, 6 * U, FLinearColor(White.R, White.G, White.B, 0.5f), 10, 1.f);
				if (bCur || bTarget || bDest || DistLy <= Ship->JumpRangeLy || S.Settlement > 0.5)
					DrawText(FString::Printf(TEXT("%s  %.1f"), *S.Name, DistLy), bCur ? Cyan : (bTarget ? Green : FLinearColor(0.85f, 0.9f, 1.f, C.A)), PX + 8 * U, PY - Dy - 6 * U, Font, 0.8f * U);
			}
			// legend
			float LY = MY + MH - 22 * U;
			DrawText(TEXT("ring: current   diamond: next jump   bracket: route destination   dot ring: charted   stalk: height above/below the galactic plane"), Grey, MX + 16 * U, LY, Font, 0.8f * U);
			if (!Ship->JumpTargetId.IsEmpty())
				DrawText(FString::Printf(TEXT("NEXT JUMP  %s  %.1f ly  %s%s"), *Ship->JumpTarget.Name.ToUpper(), Ship->JumpDistanceLy(), *UGalaxySubsystem::ClassLabel(Ship->JumpTarget.Cls), Ship->Route.Num() > 1 ? *FString::Printf(TEXT("  (%d jumps to go)"), Ship->Route.Num()) : TEXT("")), Green, MX + 16 * U, LY - 22 * U, Font, 1.0f * U);
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
		DrawText(TEXT("W/S throttle   mouse/arrows steer   A/D roll   Q/E R/F thrusters   Tab boost   J supercruise   T target   N jump target   B plot route   M map   H jump   P autopilot   L dock/launch   Z FA   X stop   C 75%"), FLinearColor(0.5f, 0.6f, 0.7f, 0.8f), 28 * U, H - 16 * U, Font, 0.8f * U);
	}
}
