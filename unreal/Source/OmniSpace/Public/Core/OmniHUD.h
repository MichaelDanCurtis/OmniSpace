// Vector-drawn holographic HUD (no UI assets needed).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "OmniHUD.generated.h"

UCLASS()
class OMNISPACE_API AOmniHUD : public AHUD
{
	GENERATED_BODY()
public:
	virtual void DrawHUD() override;

private:
	void Bar(float X, float Y, float W, float H, double Frac, const FLinearColor& C);
	void Bracket(float X, float Y, float S, const FLinearColor& C, float Thick);
	void Diamond(float X, float Y, float S, const FLinearColor& C, float Thick);
	void Circle(float X, float Y, float R, const FLinearColor& C, int32 Segs, float Thick, float Squash = 1.f);
};
