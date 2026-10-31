// Ambiente de cada mundo: sol, cielo físico, nubes volumétricas, niebla, suelo y postproceso.
// Se crea por código para que el juego funcione en un mapa vacío.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HFEnvironment.generated.h"

class FHFMatch;
class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class UPostProcessComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;
class UStaticMeshComponent;
class UVolumetricCloudComponent;

UCLASS()
class HEXAFRONTERA_API AHFEnvironment : public AActor
{
	GENERATED_BODY()

public:
	AHFEnvironment();

	void ApplyScenario(const FHFMatch& Match, float BoardExtent);

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UDirectionalLightComponent> Sun;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USkyAtmosphereComponent> Atmosphere;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USkyLightComponent> SkyLight;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UVolumetricCloudComponent> Clouds;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UExponentialHeightFogComponent> Fog;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPostProcessComponent> PostProcess;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Ground;
};
