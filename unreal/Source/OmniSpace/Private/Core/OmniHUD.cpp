#include "Core/OmniHUD.h"
#include "Ship/ShipPawn.h"
#include "World/SystemDirector.h"
#include "Core/OmniGameMode.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	FString FmtDist(double M)
	{
		const double LS = 299792458.0;
		if (M < 1000) return FString::Printf(TEXT("%.0f m"), M);
		if (M < 1e6) return FString::Printf(TEXT("%.1f km"), M / 1000);
		if (M < LS * 0.5) return FString::Printf(TEXT("%.1f Mm"), M / 1e6);
		if (M < 1.496e11 * 0.5) return FString::Printf(TEXT("%.1f ls"), M / LS);
		return FString::Printf(TEXT("%.2f AU"), M / 1.496e11);
	}
	FString FmtSpeed(double V)
	{
		const double C = 299792458.0;
		if (V < 1000) return FString::Printf(TEXT("%.0f m/s"), V);
		if (V < C * 0.5) return FString::Printf(TEXT("%.0f km/s"), V / 1000);
		return FString::Printf(TEXT("%.2f c"), V / C);
	}
}

void AOmniHUD::DrawHUD()
{
	Super::DrawHUD();
	AShipPawn* Ship = Cast<AShipPawn>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
	if (!Ship || !Canvas) return;
	UFont* Font = GEngine->GetMediumFont();
	const FLinearColor Cyan(0.56f, 0.82f, 1.f);
	const FLinearColor Amber(1.f, 0.7f, 0.28f);
	float W = Canvas->SizeX, H = Canvas->SizeY;

	// reticle
	DrawLine(W / 2 - 18, H * 0.46f, W / 2 - 8, H * 0.46f, Cyan, 1.5f);
	DrawLine(W / 2 + 8, H * 0.46f, W / 2 + 18, H * 0.46f, Cyan, 1.5f);
	DrawLine(W / 2, H * 0.46f - 18, W / 2, H * 0.46f - 8, Cyan, 1.5f);
	DrawLine(W / 2, H * 0.46f + 8, W / 2, H * 0.46f + 18, Cyan, 1.5f);

	ASystemDirector* D = Ship->GetDirector();
	float Y = 30;
	auto Line = [&](const FString& S, const FLinearColor& C, float X = 30)
	{
		DrawText(S, C, X, Y, Font, 1.2f);
		Y += 22;
	};
	if (D) Line(D->GetSystem().Name.ToUpper() + TEXT(" SYSTEM"), FLinearColor::White);
	static const TCHAR* ModeNames[] = {TEXT("NORMAL FLIGHT"), TEXT("SUPERCRUISE"), TEXT("HYPERSPACE"), TEXT("DOCKING"), TEXT("DOCKED"), TEXT("LAUNCHING"), TEXT("DESTROYED")};
	Line(ModeNames[static_cast<int>(Ship->Mode)], Cyan);
	Line(FString::Printf(TEXT("SPEED %s   THR %3.0f%%   %s"), *FmtSpeed(Ship->CurrentSpeed()), Ship->Throttle * 100, Ship->bFlightAssist ? TEXT("FA ON") : TEXT("FA OFF")), Cyan);
	Line(FString::Printf(TEXT("HULL %3.0f%%  SHLD %3.0f%%  FUEL %4.1f t  HEAT %3.0f%%"), Ship->Hull * 100, Ship->Shields * 100, Ship->Fuel, Ship->Heat * 100), Cyan);
	if (Ship->FsdKind != EFsdKind::None) Line(FString::Printf(TEXT("FSD CHARGING %3.0f%%%s"), Ship->FsdProgress * 100, Ship->FsdKind == EFsdKind::Jump ? TEXT("  (JUMP)") : TEXT("")), Amber);
	if (!Ship->JumpTargetId.IsEmpty())
	{
		FVector JD;
		bool bAligned = Ship->JumpDirection(JD) && FVector::DotProduct(Ship->GetActorQuat().GetForwardVector(), JD) > FMath::Cos(0.2);
		Line(FString::Printf(TEXT("JUMP: %s  %.1f ly  %s"), *Ship->JumpTarget.Name.ToUpper(), Ship->JumpDistanceLy(), bAligned ? TEXT("ALIGNED - H to jump") : TEXT("align with marker")), bAligned ? FLinearColor::Green : Amber);
		if (Ship->Mode != EFlightMode::Hyperspace)
		{
			FVector Screen = Project(JD * 1.0e9);
			if (Screen.Z > 0)
			{
				// diamond marker for the destination star
				DrawLine(Screen.X - 10, Screen.Y, Screen.X, Screen.Y - 10, Amber, 1.5f);
				DrawLine(Screen.X, Screen.Y - 10, Screen.X + 10, Screen.Y, Amber, 1.5f);
				DrawLine(Screen.X + 10, Screen.Y, Screen.X, Screen.Y + 10, Amber, 1.5f);
				DrawLine(Screen.X, Screen.Y + 10, Screen.X - 10, Screen.Y, Amber, 1.5f);
				DrawText(Ship->JumpTarget.Name.ToUpper(), Amber, Screen.X + 14, Screen.Y - 8, Font, 1.0f);
			}
		}
	}
	if (Ship->Mode == EFlightMode::Hyperspace)
	{
		DrawText(FString::Printf(TEXT("HYPERSPACE  >>  %s"), *Ship->JumpTarget.Name.ToUpper()), FLinearColor::White, W / 2 - 160, H * 0.3f, Font, 1.6f);
		if (AOmniGameMode* GM = Cast<AOmniGameMode>(GetWorld()->GetAuthGameMode()))
		{
			float LY = H * 0.3f + 40;
			int32 Shown = FMath::Min(GM->JumpLog.Num(), FMath::FloorToInt32((Ship->HyperT - 0.8) / 0.55) + 1);
			for (int32 i = 0; i < Shown; i++) { DrawText(GM->JumpLog[i], Cyan, W / 2 - 160, LY, Font, 1.0f); LY += 20; }
			if (!GM->IsJumpReady()) DrawText(TEXT("Stellar Forge charting the system..."), FLinearColor(0.6f, 0.65f, 0.7f), W / 2 - 160, LY + 10, Font, 1.0f);
		}
	}
	if (Ship->IsMassLocked() && Ship->Mode == EFlightMode::Normal) Line(TEXT("MASS LOCKED"), Amber);
	if (Ship->bAutopilot) Line(TEXT("AUTOPILOT"), FLinearColor::Green);
	if (D)
	{
		FString NearId;
		double NearD = D->NearestBody(NearId);
		if (!NearId.IsEmpty()) Line(FString::Printf(TEXT("NEAREST %s  %s   STAR %s   tunnel=%d"), *NearId.ToUpper(), *FmtDist(NearD), *FmtDist(D->GetShipUniversePos().Size()), Ship->Tunnel && Ship->Tunnel->IsVisible() ? 1 : 0), FLinearColor(0.5f, 0.6f, 0.7f));
	}
	if (!Ship->DockingStationId.IsEmpty() && Ship->Mode == EFlightMode::Normal) Line(FString::Printf(TEXT("DOCKING: %s BAY %d"), *Ship->DockingStationId, Ship->DockingBay + 1), FLinearColor::Green);

	// target
	if (D && !Ship->TargetId.IsEmpty())
	{
		FVector P; double R;
		if (D->GetEntityPos(Ship->TargetId, P, R))
		{
			double Dist = FVector::Dist(P, D->GetShipUniversePos()) - (R > 5000 ? R : 0);
			FVector Screen = Project(D->ToWorld(P));
			if (Screen.Z > 0)
			{
				DrawRect(Amber, Screen.X - 6, Screen.Y - 6, 12, 12);
				DrawText(FString::Printf(TEXT("%s  %s"), *Ship->TargetId.ToUpper(), *FmtDist(FMath::Max(0.0, Dist))), Amber, Screen.X + 14, Screen.Y - 8, Font, 1.1f);
			}
		}
	}
	// messages
	Y = H - 30 - 20 * FMath::Min(6, Ship->Messages.Num());
	for (int i = FMath::Max(0, Ship->Messages.Num() - 6); i < Ship->Messages.Num(); i++) Line(Ship->Messages[i], FLinearColor(0.85f, 0.92f, 1.f));
	DrawText(TEXT("W/S throttle  mouse/arrows steer  A/D roll  Q/E R/F thrusters  Tab boost  J supercruise  T target  N jump target  H jump  P autopilot  L dock/launch  Z FA  X stop  C 75%"), FLinearColor(0.5f, 0.6f, 0.7f), 30, H - 12, Font, 0.9f);
}
