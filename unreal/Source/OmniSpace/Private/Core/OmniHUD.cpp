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
#include "Core/CommanderSubsystem.h"
#include "Data/OutpostData.h"

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
			else { const FMaterials R = M.Remaining(); Line = FString::Printf(TEXT("%s  needs %.0f/%.0f/%.0f t"), S.Name, R.Metals, R.Machinery, R.Electronics); C = HudCyanDim; }
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
			DrawText(FString::Printf(TEXT("%d  %s   %.0f/%.0f/%.0f t"), i + 1, S.Name, S.Cost.Metals, S.Cost.Machinery, S.Cost.Electronics), bSel ? HudWhite : HudCyanDim, PX, Y, Font, 0.85f * U);
			Y += 18 * U;
		}
	}
	DrawText(GM->BuildStatus, GM->bGhostValid ? HudGreen : HudAmber, PX, Y + 4 * U, Font, 0.8f * U);
}

void AOmniHUD::DrawStationMenu(AShipPawn* Ship, UFont* Font, float W, float H, float U)
{
	ASystemDirector* D = Ship->GetDirector();
	UCommanderSubsystem* Co = Ship->Cmdr();
	if (!D || !Co) return;
	const FStarSystemData& Sys = D->GetSystem();
	const FStationData* St = Sys.FindStation(Ship->DockingStationId);
	if (!St) return;
	const float MW = FMath::Min(W * 0.72f, 1100 * U), MH = FMath::Min(H * 0.74f, 660 * U);
	const float MX = W / 2 - MW / 2, MY = H * 0.47f - MH / 2;
	DrawRect(FLinearColor(0.02f, 0.06f, 0.1f, 0.9f), MX, MY, MW, MH);
	DrawLine(MX, MY, MX + MW, MY, HudCyan, 1.5f); DrawLine(MX, MY + MH, MX + MW, MY + MH, HudCyan, 1.5f);
	float X = MX + 20 * U, Y = MY + 14 * U;
	DrawText(St->Name.ToUpper(), HudWhite, X, Y, Font, 1.5f * U);
	DrawText(St->Tagline, HudCyanDim, X, Y + 28 * U, Font, 0.9f * U);
	DrawText(FString::Printf(TEXT("%s  -  %s  -  %s"), *Sys.Name, *St->Faction, *St->Economy), HudGrey, X, Y + 46 * U, Font, 0.85f * U);
	DrawText(FString::Printf(TEXT("%s CR"), *FText::AsNumber(Co->S.Credits).ToString()), HudAmber, MX + MW - 220 * U, Y, Font, 1.3f * U);
	DrawText(FString::Printf(TEXT("CARGO %.0f / %.0f t"), Co->TotalTons(), UCommanderSubsystem::CargoCap), HudCyanDim, MX + MW - 220 * U, Y + 28 * U, Font, 0.9f * U);
	DrawText(FString::Printf(TEXT("FUEL %.1f t   HULL %.0f%%"), Ship->Fuel, Ship->Hull * 100), HudCyanDim, MX + MW - 220 * U, Y + 46 * U, Font, 0.9f * U);
	// tabs
	Y += 74 * U;
	const bool bMarket = St->Services.Contains(TEXT("market")), bCarto = St->Services.Contains(TEXT("cartographics"));
	const TCHAR* Tabs[] = { TEXT("1 CONCOURSE"), TEXT("2 COMMODITIES"), TEXT("3 CONTRACTS"), TEXT("4 REFUEL & REPAIR"), TEXT("5 CARTOGRAPHICS") };
	float TX = X;
	for (int32 i = 0; i < 5; i++)
	{
		const bool bOff = (i == 1 && !bMarket) || (i == 4 && !bCarto);
		const FLinearColor C = i == Ship->MenuTab ? HudWhite : bOff ? FLinearColor(0.4f, 0.45f, 0.5f, 0.6f) : HudCyanDim;
		DrawText(Tabs[i], C, TX, Y, Font, 0.95f * U);
		if (i == Ship->MenuTab) DrawLine(TX, Y + 18 * U, TX + 120 * U, Y + 18 * U, HudAmber, 1.5f);
		TX += 170 * U;
	}
	Y += 34 * U;
	const int32 Day = UCommanderSubsystem::Day();
	auto RowC = [&](int32 i) { return i == Ship->MenuRow ? HudWhite : HudCyanDim; };
	switch (Ship->MenuTab)
	{
	case 0:
	{
		DrawText(St->Description, FLinearColor(0.85f, 0.9f, 1.f, 0.9f), X, Y, Font, 0.9f * U); Y += 40 * U;
		DrawText(Sys.Profile.Description, FLinearColor(0.75f, 0.8f, 0.9f, 0.85f), X, Y, Font, 0.85f * U); Y += 40 * U;
		DrawText(FString::Printf(TEXT("%s   -   %s   -   security %s   -   population %s   -   charted by %s"), *Sys.Profile.Government, *Sys.Profile.Economy, *Sys.Profile.Security, *FText::AsNumber(Sys.Profile.Population).ToString(), Sys.Source == TEXT("ai") ? *FString::Printf(TEXT("the Stellar Forge (%s)"), *Sys.Model) : TEXT("the onboard survey computer")), HudGrey, X, Y, Font, 0.85f * U); Y += 24 * U;
		FString Sv; for (const FString& S : St->Services) Sv += (Sv.IsEmpty() ? TEXT("") : TEXT("  -  ")) + S;
		DrawText(TEXT("SERVICES  ") + Sv.ToUpper(), HudCyanDim, X, Y, Font, 0.85f * U); Y += 30 * U;
		DrawText(TEXT("HOLD"), HudAmber, X, Y, Font, 0.95f * U); Y += 20 * U;
		if (Co->S.Cargo.Num() == 0 && Co->S.MissionCargo.Num() == 0) { DrawText(TEXT("empty"), HudGrey, X, Y, Font, 0.85f * U); Y += 18 * U; }
		for (const FCargoItem& Ci : Co->S.Cargo) { const FCommodityDef* Cd = UCommanderSubsystem::Commodity(Ci.Id); DrawText(FString::Printf(TEXT("%.0f t  %s"), Ci.Tons, Cd ? Cd->Name : *Ci.Id), HudCyanDim, X, Y, Font, 0.85f * U); Y += 18 * U; }
		for (const FCargoItem& Ci : Co->S.MissionCargo) { const FCommodityDef* Cd = UCommanderSubsystem::Commodity(Ci.Id); DrawText(FString::Printf(TEXT("%.0f t  %s  (contract cargo)"), Ci.Tons, Cd ? Cd->Name : *Ci.Id), HudAmber, X, Y, Font, 0.85f * U); Y += 18 * U; }
		break;
	}
	case 1:
	{
		if (!bMarket) { DrawText(TEXT("No commodities market at this station"), HudGrey, X, Y, Font, 0.9f * U); break; }
		const float C1 = X, C2 = X + 260 * U, C3 = X + 380 * U, C4 = X + 470 * U, C5 = X + 560 * U, C6 = X + 650 * U;
		DrawText(TEXT("COMMODITY"), HudGrey, C1, Y, Font, 0.8f * U); DrawText(TEXT("BUY"), HudGrey, C3, Y, Font, 0.8f * U); DrawText(TEXT("SELL"), HudGrey, C4, Y, Font, 0.8f * U); DrawText(TEXT("SUPPLY"), HudGrey, C5, Y, Font, 0.8f * U); DrawText(TEXT("HELD"), HudGrey, C6, Y, Font, 0.8f * U);
		Y += 20 * U;
		const TArray<FMarketRow> Rows = UCommanderSubsystem::Market(*St, Day);
		for (int32 i = 0; i < Rows.Num(); i++)
		{
			const FMarketRow& R = Rows[i];
			if (i == Ship->MenuRow) DrawRect(FLinearColor(HudCyan.R, HudCyan.G, HudCyan.B, 0.12f), X - 6 * U, Y - 2 * U, MW - 28 * U, 20 * U);
			DrawText(R.Name, RowC(i), C1, Y, Font, 0.9f * U);
			if (R.Tag == TEXT("produced")) DrawText(TEXT("PRODUCED - CHEAP"), HudGreen, C2, Y, Font, 0.75f * U);
			else if (R.Tag == TEXT("consumed")) DrawText(TEXT("IN DEMAND"), HudAmber, C2, Y, Font, 0.75f * U);
			DrawText(FString::Printf(TEXT("%d"), R.Buy), RowC(i), C3, Y, Font, 0.9f * U);
			DrawText(FString::Printf(TEXT("%d"), R.Sell), RowC(i), C4, Y, Font, 0.9f * U);
			DrawText(FString::Printf(TEXT("%d"), R.Supply), HudGrey, C5, Y, Font, 0.9f * U);
			const double Held = Co->Held(R.Id);
			if (Held > 0) DrawText(FString::Printf(TEXT("%.0f t"), Held), HudWhite, C6, Y, Font, 0.9f * U);
			Y += 20 * U;
		}
		break;
	}
	case 2:
	{
		DrawText(TEXT("ACTIVE CONTRACTS"), HudAmber, X, Y, Font, 0.95f * U); Y += 20 * U;
		int32 Row = 0;
		if (Co->S.Contracts.Num() == 0) { DrawText(TEXT("none"), HudGrey, X, Y, Font, 0.85f * U); Y += 18 * U; }
		for (const FContract& C : Co->S.Contracts)
		{
			if (Row == Ship->MenuRow) DrawRect(FLinearColor(HudCyan.R, HudCyan.G, HudCyan.B, 0.12f), X - 6 * U, Y - 2 * U, MW - 28 * U, 20 * U);
			DrawText(FString::Printf(TEXT("%s  ->  %s   %d CR"), *C.Title, C.DestStationName.IsEmpty() ? *C.DestSystemName : *C.DestStationName, C.Reward), RowC(Row), X, Y, Font, 0.9f * U);
			Y += 20 * U; Row++;
		}
		Y += 10 * U;
		DrawText(TEXT("MISSION BOARD"), HudAmber, X, Y, Font, 0.95f * U); Y += 20 * U;
		const TArray<FContract> Board = Co->MissionBoard(*St, Sys, Day);
		for (const FContract& C : Board)
		{
			const bool bHave = Co->S.Contracts.ContainsByPredicate([&](const FContract& A) { return A.Id == C.Id; });
			if (Row == Ship->MenuRow) DrawRect(FLinearColor(HudCyan.R, HudCyan.G, HudCyan.B, 0.12f), X - 6 * U, Y - 2 * U, MW - 28 * U, 38 * U);
			DrawText(FString::Printf(TEXT("%s   %d CR   %.1f ly%s"), *C.Title, C.Reward, C.DistanceLy, bHave ? TEXT("   (accepted)") : TEXT("")), bHave ? HudGreen : RowC(Row), X, Y, Font, 0.9f * U);
			DrawText(C.Description, HudGrey, X + 12 * U, Y + 17 * U, Font, 0.78f * U);
			Y += 38 * U; Row++;
		}
		break;
	}
	case 3:
	{
		const double Need = 16.0 - Ship->Fuel, Missing = 1.0 - Ship->Hull;
		if (Ship->MenuRow == 0) DrawRect(FLinearColor(HudCyan.R, HudCyan.G, HudCyan.B, 0.12f), X - 6 * U, Y - 2 * U, MW - 28 * U, 20 * U);
		DrawText(FString::Printf(TEXT("REFUEL   %.1f t needed   %d CR   (55 CR / t)"), Need, FMath::CeilToInt(Need * 55)), RowC(0), X, Y, Font, 0.95f * U); Y += 24 * U;
		if (Ship->MenuRow == 1) DrawRect(FLinearColor(HudCyan.R, HudCyan.G, HudCyan.B, 0.12f), X - 6 * U, Y - 2 * U, MW - 28 * U, 20 * U);
		DrawText(FString::Printf(TEXT("REPAIR   hull %.0f%%   %d CR"), Ship->Hull * 100, FMath::CeilToInt(Missing * 6000)), RowC(1), X, Y, Font, 0.95f * U); Y += 40 * U;
		DrawText(TEXT("OS-7 KESTREL"), HudAmber, X, Y, Font, 0.95f * U); Y += 20 * U;
		DrawText(FString::Printf(TEXT("fuel 16 t   cargo 32 t   jump range %.1f ly at current mass   340 m/s, boost 560 m/s"), Ship->JumpRangeLy), HudCyanDim, X, Y, Font, 0.85f * U);
		break;
	}
	case 4:
	{
		if (!bCarto) { DrawText(TEXT("No cartographics office here - exploration data sells at larger stations"), HudGrey, X, Y, Font, 0.9f * U); break; }
		DrawText(FString::Printf(TEXT("EXPLORATION DATA   %d records   %d CR   (Enter sells all)"), Co->S.Data.Num(), Co->DataValue()), HudAmber, X, Y, Font, 0.95f * U); Y += 22 * U;
		int32 N = 0;
		for (int32 i = Co->S.Data.Num() - 1; i >= 0 && N < 14; i--, N++) { DrawText(FString::Printf(TEXT("%-6d  %s"), Co->S.Data[i].Value, *Co->S.Data[i].Label), HudCyanDim, X, Y, Font, 0.85f * U); Y += 18 * U; }
		break;
	}
	default: break;
	}
	// footer
	const TCHAR* Keys = Ship->MenuTab == 1 ? TEXT("Up/Down select   Enter buy 1   Right buy 8   Backspace sell 1   Left sell all   U quick construction load   L launch")
		: Ship->MenuTab == 2 ? TEXT("Up/Down select   Enter accept   Backspace abandon   L launch")
		: Ship->MenuTab == 3 ? TEXT("Up/Down select   Enter buy   L launch")
		: TEXT("1-5 tabs   Enter confirm   L launch");
	DrawText(Keys, HudGrey, X, MY + MH - 22 * U, Font, 0.8f * U);
	if (!Ship->MenuNote.IsEmpty()) DrawText(Ship->MenuNote, HudGreen, X, MY + MH - 42 * U, Font, 0.9f * U);
}

void AOmniHUD::DrawSystemMap(AShipPawn* Ship, UFont* Font, float W, float H, float U)
{
	ASystemDirector* D = Ship->GetDirector();
	UCommanderSubsystem* Co = Ship->Cmdr();
	if (!D) return;
	const FStarSystemData& Sys = D->GetSystem();
	const float MW = FMath::Min(W * 0.86f, 1300 * U), MH = FMath::Min(H * 0.78f, 700 * U);
	const float MX = W / 2 - MW / 2, MY = H * 0.47f - MH / 2;
	const float PanelW = 320 * U;
	DrawRect(FLinearColor(0.02f, 0.06f, 0.1f, 0.9f), MX, MY, MW, MH);
	DrawLine(MX, MY, MX + MW, MY, HudCyan, 1.5f); DrawLine(MX, MY + MH, MX + MW, MY + MH, HudCyan, 1.5f);
	DrawLine(MX + MW - PanelW, MY, MX + MW - PanelW, MY + MH, FLinearColor(HudCyan.R, HudCyan.G, HudCyan.B, 0.3f), 1.f);
	DrawText(Sys.Name.ToUpper() + TEXT(" SYSTEM"), HudWhite, MX + 16 * U, MY + 12 * U, Font, 1.4f * U);
	DrawText(TEXT("click to target   T cycles targets   P autopilot   I close"), HudGrey, MX + 16 * U, MY + 36 * U, Font, 0.85f * U);
	DrawText(FString::Printf(TEXT("%s  -  %s  -  %s  -  %s"), *Sys.Profile.Faction, *Sys.Profile.Government, *Sys.Profile.Economy, Sys.Source == TEXT("ai") ? TEXT("charted by the Stellar Forge") : TEXT("onboard survey")), HudCyanDim, MX + 16 * U, MY + 56 * U, Font, 0.85f * U);
	// chart: star at the left, orbits on a log scale to the right
	const float X0 = MX + 110 * U, X1 = MX + MW - PanelW - 40 * U, CY = MY + MH * 0.5f;
	double OMin = 1e30, OMax = 1;
	for (const FBodyData& B : Sys.Bodies) if (B.Parent == TEXT("star")) { OMin = FMath::Min(OMin, B.OrbitM); OMax = FMath::Max(OMax, B.OrbitM); }
	for (const FBeltData& B : Sys.Belts) if (B.Parent == TEXT("star")) { OMin = FMath::Min(OMin, B.RadiusM); OMax = FMath::Max(OMax, B.RadiusM); }
	if (OMin > 1e29) { OMin = 1e10; OMax = 1e12; }
	const double LMin = FMath::Loge(OMin * 0.8), LMax = FMath::Loge(OMax * 1.3);
	auto XOf = [&](double OrbitM) { return static_cast<float>(X0 + (X1 - X0) * FMath::Clamp((FMath::Loge(FMath::Max(OrbitM, 1.0)) - LMin) / FMath::Max(1e-6, LMax - LMin), 0.0, 1.0)); };
	// the star
	const FLinearColor SC = Sys.Star.LinearColor();
	Circle(MX + 50 * U, CY, 22 * U, SC, 32, 3.f);
	DrawText(Sys.Star.Name, HudCyanDim, MX + 32 * U, CY + 30 * U, Font, 0.8f * U);
	float MouseX = -1, MouseY = -1;
	if (APlayerController* PC = GetOwningPlayerController()) PC->GetMousePosition(MouseX, MouseY);
	FString Hover; float HoverD = 18 * U;
	auto Consider = [&](const FString& Id, float PX, float PY) { const float MD = FVector2D::Distance(FVector2D(MouseX, MouseY), FVector2D(PX, PY)); if (MD < HoverD) { HoverD = MD; Hover = Id; } };
	TMap<FString, FVector2D> Placed;
	// belts
	for (const FBeltData& B : Sys.Belts)
	{
		if (B.Parent != TEXT("star")) continue;
		const float BX = XOf(B.RadiusM);
		for (float y = CY - 90 * U; y < CY + 90 * U; y += 10 * U) DrawLine(BX, y, BX, y + 5 * U, FLinearColor(0.6f, 0.55f, 0.45f, 0.6f), 1.f);
		DrawText(B.Name, HudGrey, BX - 20 * U, CY + 96 * U, Font, 0.75f * U);
	}
	// planets, then moons under them and stations above
	for (const FBodyData& B : Sys.Bodies)
	{
		if (B.Parent != TEXT("star")) continue;
		const float PX = XOf(B.OrbitM);
		const float R = FMath::Clamp(static_cast<float>(6.0 + 7.0 * FMath::Loge(FMath::Max(B.RadiusM, 1e5) / 1e6)), 5.f, 30.f) * U;
		const FLinearColor C = B.Palette.Num() > 1 ? OmniJson::HexToLinear(B.Palette[1]) : FLinearColor(0.6f, 0.6f, 0.65f);
		Circle(PX, CY, R, C, 32, 2.f);
		if (B.bHasRings) { Circle(PX, CY, R * 1.6f, FLinearColor(0.8f, 0.75f, 0.65f, 0.6f), 32, 1.f); }
		if (!B.Atmosphere.IsEmpty()) Circle(PX, CY, R + 3 * U, FLinearColor(0.5f, 0.75f, 1.f, 0.4f), 32, 1.f);
		const bool bTarget = Ship->TargetId == B.Id;
		if (bTarget) Bracket(PX, CY, R + 8 * U, HudAmber, 1.5f);
		const bool bScanned = Co && Co->IsScanned(Sys.Id, B.Id);
		DrawText(B.Name, bTarget ? HudAmber : HudWhite, PX - R, CY - R - 20 * U, Font, 0.85f * U);
		DrawText(FString::Printf(TEXT("%.2f AU%s"), B.OrbitM / 1.496e11, bScanned ? TEXT("  SCANNED") : TEXT("")), bScanned ? HudGreen : HudGrey, PX - R, CY + R + 6 * U, Font, 0.75f * U);
		Consider(B.Id, PX, CY);
		Placed.Add(B.Id, FVector2D(PX, CY));
		// moons
		float My = CY + R + 40 * U;
		for (const FBodyData& M : Sys.Bodies)
		{
			if (M.Parent != B.Id) continue;
			const float MR = FMath::Clamp(static_cast<float>(3.0 + 4.0 * FMath::Loge(FMath::Max(M.RadiusM, 1e5) / 1e6)), 3.f, 10.f) * U;
			const FLinearColor MC = M.Palette.Num() > 1 ? OmniJson::HexToLinear(M.Palette[1]) : FLinearColor(0.6f, 0.6f, 0.65f);
			DrawLine(PX, My - MR - 4 * U, PX, My - MR - 18 * U, FLinearColor(0.4f, 0.5f, 0.6f, 0.5f), 1.f);
			Circle(PX, My, MR, MC, 20, 1.5f);
			if (Ship->TargetId == M.Id) Bracket(PX, My, MR + 6 * U, HudAmber, 1.5f);
			DrawText(M.Name, Ship->TargetId == M.Id ? HudAmber : HudCyanDim, PX + MR + 6 * U, My - 7 * U, Font, 0.75f * U);
			Consider(M.Id, PX, My);
			Placed.Add(M.Id, FVector2D(PX, My));
			My += MR * 2 + 26 * U;
		}
		// stations
		float Sy = CY - R - 46 * U;
		for (const FStationData& St : Sys.Stations)
		{
			if (St.Parent != B.Id) continue;
			Diamond(PX, Sy, 7 * U, Ship->TargetId == St.Id ? HudAmber : HudGreen, 1.5f);
			DrawText(St.Name, Ship->TargetId == St.Id ? HudAmber : HudGreen, PX + 12 * U, Sy - 7 * U, Font, 0.75f * U);
			Consider(St.Id, PX, Sy);
			Placed.Add(St.Id, FVector2D(PX, Sy));
			Sy -= 24 * U;
		}
	}
	// stations orbiting the star directly
	float Ssy = CY - 120 * U;
	for (const FStationData& St : Sys.Stations)
	{
		if (St.Parent != TEXT("star")) continue;
		const float PX = XOf(St.OrbitM);
		Diamond(PX, Ssy, 7 * U, Ship->TargetId == St.Id ? HudAmber : HudGreen, 1.5f);
		DrawText(St.Name, HudGreen, PX + 12 * U, Ssy - 7 * U, Font, 0.75f * U);
		Consider(St.Id, PX, Ssy);
		Ssy -= 24 * U;
	}
	Ship->SysMapHoverId = Hover;
	if (const FVector2D* HP = Placed.Find(Hover)) Circle(HP->X, HP->Y, 16 * U, FLinearColor(1.f, 1.f, 1.f, 0.5f), 24, 1.f);
	// right panel: the hovered or targeted thing
	const FString ShowId = !Hover.IsEmpty() ? Hover : Ship->TargetId;
	float IX = MX + MW - PanelW + 16 * U, IY = MY + 14 * U;
	auto Line = [&](const FString& T, const FLinearColor& Col) { DrawText(T, Col, IX, IY, Font, 0.85f * U); IY += 18 * U; };
	auto Wrap = [&](const FString& In, int32 Max, const FLinearColor& Col) { FString Desc = In; int32 Lines = 0; while (!Desc.IsEmpty() && Lines < Max) { int32 Cut = FMath::Min(46, Desc.Len()); if (Desc.Len() > 46) { int32 Sp; if (Desc.Left(46).FindLastChar(' ', Sp) && Sp > 20) Cut = Sp; } Line(Desc.Left(Cut).TrimStartAndEnd(), Col); Desc = Desc.Mid(Cut); Lines++; } };
	if (const FBodyData* B = Sys.FindBody(ShowId))
	{
		DrawText(B->Name.ToUpper(), HudWhite, IX, IY, Font, 1.3f * U); IY += 26 * U;
		Line(FString::Printf(TEXT("%s %s"), *B->Type.Replace(TEXT("_"), TEXT(" ")), *B->Kind), HudAmber);
		Line(FString::Printf(TEXT("radius %.0f km   gravity %.2f g"), B->RadiusM / 1000, B->GravityG), HudCyanDim);
		Line(FString::Printf(TEXT("%.0f K   orbit %.2f AU"), B->TemperatureK, B->OrbitM / 1.496e11), HudCyanDim);
		Line(B->Atmosphere.IsEmpty() ? TEXT("no atmosphere") : TEXT("atmosphere"), HudCyanDim);
		if (B->bHasRings) Line(TEXT("ring system"), HudCyanDim);
		Line(Co && Co->IsScanned(Sys.Id, B->Id) ? TEXT("surface scanned") : FString::Printf(TEXT("unscanned  (%d CR)"), UCommanderSubsystem::ScanValue(B->Type)), Co && Co->IsScanned(Sys.Id, B->Id) ? HudGreen : HudGrey);
		IY += 8 * U;
		Wrap(B->Description, 8, HudGrey);
		IY += 8 * U;
		Line(B->IsGiant() ? TEXT("no surface to land on") : TEXT("landable: L within 2.5 radii"), B->IsGiant() ? HudGrey : HudGreen);
	}
	else if (const FStationData* St = Sys.FindStation(ShowId))
	{
		DrawText(St->Name.ToUpper(), HudWhite, IX, IY, Font, 1.3f * U); IY += 26 * U;
		Line(St->Tagline, HudAmber);
		Line(FString::Printf(TEXT("%s   %s   %s"), *St->Faction, *St->Economy, *St->Kind), HudCyanDim);
		FString Sv; for (const FString& S : St->Services) Sv += (Sv.IsEmpty() ? TEXT("") : TEXT(", ")) + S;
		Wrap(Sv, 2, HudCyanDim);
		IY += 8 * U;
		Wrap(St->Description, 8, HudGrey);
	}
	else
	{
		DrawText(Sys.Star.Name.ToUpper(), HudWhite, IX, IY, Font, 1.3f * U); IY += 26 * U;
		Line(UGalaxySubsystem::ClassLabel(Sys.Star.Cls), HudAmber);
		Line(FString::Printf(TEXT("%.0f K   %.2f R   %.2f M   %.1f Gyr"), Sys.Star.Temperature, Sys.Star.RadiusSolar, Sys.Star.MassSolar, Sys.Star.AgeGyr), HudCyanDim);
		IY += 8 * U;
		Wrap(Sys.StarFlavor, 4, HudGrey);
		IY += 8 * U;
		Wrap(Sys.Profile.Description, 8, HudGrey);
		IY += 8 * U;
		Wrap(Sys.Profile.Lore, 8, FLinearColor(0.75f, 0.7f, 0.9f, 0.9f));
	}
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
		if (UCommanderSubsystem* Co = Ship->Cmdr())
			DrawText(FString::Printf(TEXT("%s CR   cargo %.0f t   data %d CR"), *FText::AsNumber(Co->S.Credits).ToString(), Co->TotalTons(), Co->DataValue()), HudCyanDim, PX, PY + 128 * U, Font, 0.8f * U);
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

	// ---------------------------------------------------------------- station services (docked)
	if (Ship->Mode == EFlightMode::Docked && Ship->bStationMenu && !Ship->bShowMap && !Ship->bShowSystemMap && !Ship->bShowHelp) DrawStationMenu(Ship, Font, W, H, U);

	// ---------------------------------------------------------------- galaxy map overlay (M)
	if (Ship->bShowMap && GM && !bHyper)
	{
		UGalaxySubsystem* Galaxy = GetGameInstance() ? GetGameInstance()->GetSubsystem<UGalaxySubsystem>() : nullptr;
		UForgeSubsystem* Forge = GetGameInstance() ? GetGameInstance()->GetSubsystem<UForgeSubsystem>() : nullptr;
		UCommanderSubsystem* Co = Ship->Cmdr();
		if (Galaxy)
		{
			const float MW = FMath::Min(W * 0.86f, 1300 * U), MH = FMath::Min(H * 0.78f, 700 * U);
			const float MX = CX - MW / 2, MY = H * 0.47f - MH / 2;
			const float PanelW = 300 * U;             // info panel on the right
			DrawRect(FLinearColor(0.02f, 0.06f, 0.1f, 0.9f), MX, MY, MW, MH);
			DrawLine(MX, MY, MX + MW, MY, HudCyan, 1.5f); DrawLine(MX, MY + MH, MX + MW, MY + MH, HudCyan, 1.5f);
			DrawLine(MX + MW - PanelW, MY, MX + MW - PanelW, MY + MH, FLinearColor(HudCyan.R, HudCyan.G, HudCyan.B, 0.3f), 1.f);
			DrawText(TEXT("GALAXY MAP"), HudWhite, MX + 16 * U, MY + 12 * U, Font, 1.4f * U);
			DrawText(TEXT("click a star to select   N jump target / route to it   B plot route   wheel zoom   H jump   M close"), HudGrey, MX + 16 * U, MY + 36 * U, Font, 0.85f * U);
			const FStarDescriptor& Cur = GM->CurrentStar;
			const double RangeLy = Ship->MapRangeLy;
			const float MapW = MW - PanelW;
			const float Scale = (FMath::Min(MapW, MH) * 0.46f) / static_cast<float>(RangeLy);
			const float OX = MX + MapW * 0.5f, OY = MY + MH * 0.54f;
			Circle(OX, OY, static_cast<float>(Ship->JumpRangeLy) * Scale, FLinearColor(HudCyan.R, HudCyan.G, HudCyan.B, 0.45f), 48, 1.f);
			for (double R = 10; R < RangeLy; R += 10) Circle(OX, OY, static_cast<float>(R) * Scale, FLinearColor(HudCyan.R, HudCyan.G, HudCyan.B, 0.08f), 48, 1.f);
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
			// mouse: nearest star within reach of the cursor
			float MouseX = -1, MouseY = -1;
			if (APlayerController* PC = GetOwningPlayerController()) PC->GetMousePosition(MouseX, MouseY);
			FString Hover; float HoverD = 14 * U;
			const bool bDense = Stars.Num() > 260;
			for (const FStarDescriptor& S : Stars)
			{
				const float PX = OX + static_cast<float>(S.Pos.X - Cur.Pos.X) * Scale;
				const float PY = OY - static_cast<float>(S.Pos.Z - Cur.Pos.Z) * Scale;
				const float Dy = static_cast<float>(S.Pos.Y - Cur.Pos.Y) * Scale * 0.3f;
				if (PX < MX + 4 || PX > MX + MapW - 4 || PY - Dy < MY + 50 * U || PY - Dy > MY + MH - 30 * U) continue;
				const double DistLy = FVector::Dist(S.Pos, Cur.Pos);
				const bool bCur = S.Id == Cur.Id;
				const bool bTarget = S.Id == Ship->JumpTargetId;
				const bool bDest = S.Id == Ship->RouteDestId;
				const bool bSel = S.Id == Ship->MapSelectedId;
				const bool bVisited = Co && Co->S.Visited.Contains(S.Id);
				FStarSystemData Tmp;
				const bool bCharted = Forge && Forge->GetCached(S.Id, Tmp);
				const float MD = FVector2D::Distance(FVector2D(MouseX, MouseY), FVector2D(PX, PY - Dy));
				if (MD < HoverD) { HoverD = MD; Hover = S.Id; }
				FLinearColor C = S.LinearColor(); C.A = DistLy <= Ship->JumpRangeLy || bCur || bTarget || bDest || bSel ? 1.f : 0.4f;
				if (FMath::Abs(Dy) > 1.f && !bDense) DrawLine(PX, PY, PX, PY - Dy, FLinearColor(C.R, C.G, C.B, 0.2f), 1.f);
				const float Sz = (bCur ? 5.f : 2.f + static_cast<float>(FMath::Clamp(S.RadiusSolar, 0.2, 2.0))) * U;
				DrawRect(C, PX - Sz / 2, PY - Dy - Sz / 2, Sz, Sz);
				if (bCur) Circle(PX, PY - Dy, 10 * U, HudCyan, 16, 1.5f);
				if (bTarget) Diamond(PX, PY - Dy, 10 * U, HudGreen, 1.5f);
				if (bDest) Bracket(PX, PY - Dy, 10 * U, HudAmber, 1.5f);
				if (bSel) Circle(PX, PY - Dy, 13 * U, HudAmber, 20, 1.5f);
				if (bVisited && !bCur) Circle(PX, PY - Dy, 6 * U, FLinearColor(HudGreen.R, HudGreen.G, HudGreen.B, 0.6f), 10, 1.f);
				else if (bCharted && !bCur) Circle(PX, PY - Dy, 6 * U, FLinearColor(0.7f, 0.5f, 1.f, 0.6f), 10, 1.f);
				if (bCur || bTarget || bDest || bSel || (!bDense && (DistLy <= Ship->JumpRangeLy || S.Settlement > 0.8)))
					DrawText(FString::Printf(TEXT("%s  %.1f"), *S.Name, DistLy), bCur ? HudCyan : (bTarget ? HudGreen : FLinearColor(0.85f, 0.9f, 1.f, C.A)), PX + 8 * U, PY - Dy - 6 * U, Font, 0.8f * U);
			}
			Ship->MapHoverId = Hover;
			if (!Hover.IsEmpty() && Hover != Ship->MapSelectedId)
			{
				FStarDescriptor HS;
				if (Galaxy->GetStar(Hover, HS))
				{
					const float PX = OX + static_cast<float>(HS.Pos.X - Cur.Pos.X) * Scale, PY = OY - static_cast<float>(HS.Pos.Z - Cur.Pos.Z) * Scale - static_cast<float>(HS.Pos.Y - Cur.Pos.Y) * Scale * 0.3f;
					Circle(PX, PY, 12 * U, FLinearColor(1.f, 1.f, 1.f, 0.5f), 20, 1.f);
					DrawText(FString::Printf(TEXT("%s  %.1f ly"), *HS.Name, FVector::Dist(HS.Pos, Cur.Pos)), HudWhite, PX + 14 * U, PY + 8 * U, Font, 0.85f * U);
				}
			}
			// info panel: the selected star (or the current one)
			FStarDescriptor Sel;
			const bool bHaveSel = Galaxy->GetStar(Ship->MapSelectedId.IsEmpty() ? Cur.Id : Ship->MapSelectedId, Sel);
			if (bHaveSel)
			{
				float IX = MX + MW - PanelW + 16 * U, IY = MY + 14 * U;
				const double DistLy = FVector::Dist(Sel.Pos, Cur.Pos);
				const bool bInRange = DistLy <= Ship->JumpRangeLy;
				DrawText(Sel.Name.ToUpper(), HudWhite, IX, IY, Font, 1.3f * U); IY += 26 * U;
				DrawText(UGalaxySubsystem::ClassLabel(Sel.Cls), HudAmber, IX, IY, Font, 0.9f * U); IY += 22 * U;
				auto Line = [&](const FString& T, const FLinearColor& Col) { DrawText(T, Col, IX, IY, Font, 0.85f * U); IY += 18 * U; };
				if (Sel.Id == Cur.Id) Line(TEXT("current system"), HudCyan);
				else Line(FString::Printf(TEXT("%.1f ly  %s"), DistLy, bInRange ? TEXT("- in range") : TEXT("- needs a route")), bInRange ? HudGreen : HudCyanDim);
				Line(FString::Printf(TEXT("%.0f K   %.2f L   %.2f R"), Sel.Temperature, Sel.Luminosity, Sel.RadiusSolar), HudCyanDim);
				Line(UGalaxySubsystem::IsScoopable(Sel.Cls) ? TEXT("fuel scoopable") : TEXT("not scoopable"), UGalaxySubsystem::IsScoopable(Sel.Cls) ? HudGreen : HudGrey);
				Line(Sel.Region, HudCyanDim);
				Line(Sel.Settlement > 0.8 ? TEXT("Core worlds") : Sel.Settlement > 0.55 ? TEXT("Inner frontier") : Sel.Settlement > 0.3 ? TEXT("Frontier") : TEXT("Deep space"), HudCyanDim);
				if (bInRange && Sel.Id != Cur.Id) Line(FString::Printf(TEXT("jump fuel %.1f t"), 0.35 + 3.4 * FMath::Square(DistLy / Ship->JumpRangeLy)), HudCyanDim);
				IY += 8 * U;
				FStarSystemData SysD;
				if (Forge && Forge->GetCached(Sel.Id, SysD))
				{
					Line(SysD.Source == TEXT("ai") ? TEXT("CHARTED BY THE STELLAR FORGE") : TEXT("SURVEYED"), FLinearColor(0.7f, 0.5f, 1.f));
					Line(FString::Printf(TEXT("%d bodies   %d stations"), SysD.Bodies.Num(), SysD.Stations.Num()), HudCyanDim);
					Line(FString::Printf(TEXT("%s   %s"), *SysD.Profile.Faction, *SysD.Profile.Economy), HudCyanDim);
					// wrap the description roughly
					FString Desc = SysD.Profile.Description; int32 Lines = 0;
					while (!Desc.IsEmpty() && Lines < 6) { int32 Cut = FMath::Min(44, Desc.Len()); if (Desc.Len() > 44) { int32 Sp; if (Desc.Left(44).FindLastChar(' ', Sp) && Sp > 20) Cut = Sp; } Line(Desc.Left(Cut).TrimStartAndEnd(), HudGrey); Desc = Desc.Mid(Cut); Lines++; }
				}
				else Line(TEXT("UNCHARTED"), HudGrey);
				if (Co && Co->S.Visited.Contains(Sel.Id)) Line(TEXT("visited"), HudGreen);
				IY += 10 * U;
				if (Sel.Id != Cur.Id) Line(bInRange ? TEXT("N  set as jump target") : TEXT("N / B  plot a route here"), HudAmber);
			}
			// legend + next jump
			float LY = MY + MH - 22 * U;
			DrawText(TEXT("ring: current   diamond: next jump   bracket: route end   amber ring: selected   green dot: visited   violet dot: charted"), HudGrey, MX + 16 * U, LY, Font, 0.8f * U);
			if (!Ship->JumpTargetId.IsEmpty())
				DrawText(FString::Printf(TEXT("NEXT JUMP  %s  %.1f ly  %s%s"), *Ship->JumpTarget.Name.ToUpper(), Ship->JumpDistanceLy(), *UGalaxySubsystem::ClassLabel(Ship->JumpTarget.Cls), Ship->Route.Num() > 1 ? *FString::Printf(TEXT("  (%d jumps to go)"), Ship->Route.Num()) : TEXT("")), HudGreen, MX + 16 * U, LY - 22 * U, Font, 1.0f * U);
		}
	}

	// ---------------------------------------------------------------- help (F1)
	if (Ship->bShowHelp && !bHyper)
	{
		const float MW = FMath::Min(W * 0.6f, 820 * U), MH = FMath::Min(H * 0.8f, 640 * U);
		const float MX = CX - MW / 2, MY = H * 0.47f - MH / 2;
		DrawRect(FLinearColor(0.02f, 0.06f, 0.1f, 0.92f), MX, MY, MW, MH);
		DrawLine(MX, MY, MX + MW, MY, HudCyan, 1.5f); DrawLine(MX, MY + MH, MX + MW, MY + MH, HudCyan, 1.5f);
		float Y = MY + 14 * U; const float X1 = MX + 20 * U, X2 = MX + 190 * U;
		DrawText(TEXT("CONTROLS"), HudWhite, X1, Y, Font, 1.4f * U); Y += 34 * U;
		const TCHAR* Rows[][2] = {
			{TEXT("mouse / arrows"), TEXT("pitch and yaw (click the view to capture the mouse)")},
			{TEXT("A / D"), TEXT("roll")},
			{TEXT("W / S"), TEXT("throttle (reverse to -50% in normal flight)   X stop   C 75%")},
			{TEXT("Q / E   R / F"), TEXT("strafe, vertical thrusters (R/F climb and descend in atmosphere)")},
			{TEXT("Tab / Space"), TEXT("boost")},
			{TEXT("Z"), TEXT("flight assist on/off (off: Newtonian drift)")},
			{TEXT("J"), TEXT("supercruise (3 s charge; mass locked near stations)")},
			{TEXT("T"), TEXT("cycle targets by distance   I system map")},
			{TEXT("N / B"), TEXT("next star in jump range / plot a route to a far star")},
			{TEXT("M"), TEXT("galaxy map: click a star, N or B to go there, wheel to zoom")},
			{TEXT("H"), TEXT("hyperspace jump (5 s charge, aligned, out of mass lock)")},
			{TEXT("P"), TEXT("autopilot: flies to the target, docks, jumps along the route")},
			{TEXT("L"), TEXT("request docking / launch / land on a planet (within 2.5 radii) / lift off")},
			{TEXT("K"), TEXT("landing lights")},
			{TEXT("G"), TEXT("deploy the rover when landed (G again near the ship to board)")},
			{TEXT("O   1-6   [ ]   Enter"), TEXT("outpost build mode: choose a module, rotate, place")},
			{TEXT("U"), TEXT("docked: quick construction load   landed: unload materials to the sites")},
			{TEXT("1-5 / arrows / Enter"), TEXT("station services when docked")},
			{TEXT("7 8 9 0"), TEXT("debug: sky, star, bodies, cockpit")},
			{TEXT("F1 or /"), TEXT("this panel")},
		};
		for (const auto& R : Rows) { DrawText(R[0], HudAmber, X1, Y, Font, 0.9f * U); DrawText(R[1], HudCyanDim, X2, Y, Font, 0.9f * U); Y += 22 * U; }
		Y += 10 * U;
		DrawText(TEXT("Tips: turning is best between 45% and 80% throttle. Fuel scoops within 4 radii of a bright star in supercruise. Flying within 5 radii of a planet logs a surface scan worth credits at a cartographics office."), HudGrey, X1, Y, Font, 0.8f * U);
	}

	// ---------------------------------------------------------------- system map overlay (I)
	if (Ship->bShowSystemMap && D && !bHyper) DrawSystemMap(Ship, Font, W, H, U);

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
		DrawText(TEXT("W/S throttle   mouse/arrows steer   A/D roll   Q/E R/F thrusters   Tab boost   J supercruise   T target   N jump target   B plot route   M map   H jump   P autopilot   L dock/land/launch   I system map   G rover   K lights   O build   U cargo   Z FA   X stop   C 75%"), FLinearColor(0.5f, 0.6f, 0.7f, 0.8f), 28 * U, H - 16 * U, Font, 0.8f * U);
	}
}
