#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "OmniGameInstance.generated.h"

UCLASS()
class OMNISPACE_API UOmniGameInstance : public UGameInstance
{
	GENERATED_BODY()
public:
	virtual void Init() override;
};
