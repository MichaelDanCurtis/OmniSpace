// Procedural cockpit interior built from engine primitives and parented to the ship root, so the
// pilot sees a canopy frame, dashboard and consoles around the view (the concept-art framing).
#pragma once

#include "CoreMinimal.h"

class AActor;
class USceneComponent;

namespace OmniCockpit
{
	/** Builds the cockpit under Parent (ship root; camera at the parent origin looking +X). */
	OMNISPACE_API void Build(AActor* Owner, USceneComponent* Parent);
}
