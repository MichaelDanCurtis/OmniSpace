// Minimal Canvas HUD for the first playable build (replaced by UMG later).
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
};
