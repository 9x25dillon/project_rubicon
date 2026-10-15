#include "RubiconAtmosphereDriverComponent.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Engine/World.h"
#include "RubiconGolgothaGreybox.h"
#include "RubiconNarrativeSubsystem.h"

URubiconAtmosphereDriverComponent::URubiconAtmosphereDriverComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void URubiconAtmosphereDriverComponent::BeginPlay()
{
	Super::BeginPlay();
	if (const ARubiconGolgothaGreybox* G = Cast<ARubiconGolgothaGreybox>(GetOwner()))
	{
		if (USkyAtmosphereComponent* Sky = G->GetSkyAtmosphere())
		{
			DefaultMieScattering = Sky->MieScatteringScale;
			DefaultMieAbsorption = Sky->MieAbsorptionScale;
		}
		if (UExponentialHeightFogComponent* Fog = G->GetHeightFog()) DefaultFogDensity = Fog->FogDensity;
		if (UPostProcessComponent* Pp = G->GetPostProcess())
		{
			FPostProcessSettings& S = Pp->Settings;
			S.bOverride_AutoExposureMethod = true;
			S.AutoExposureMethod = EAutoExposureMethod::AEM_Histogram;
			S.bOverride_AutoExposureMinBrightness = true;
			S.AutoExposureMinBrightness = MinEV100;
			S.bOverride_AutoExposureMaxBrightness = true;
			S.AutoExposureMaxBrightness = MaxEV100;
			S.bOverride_AutoExposureSpeedUp = true;
			S.AutoExposureSpeedUp = AdaptationSpeed;
			S.bOverride_AutoExposureSpeedDown = true;
			S.AutoExposureSpeedDown = AdaptationSpeed;
		}
	}
}

void URubiconAtmosphereDriverComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const ARubiconGolgothaGreybox* G = Cast<ARubiconGolgothaGreybox>(GetOwner());
	const URubiconNarrativeSubsystem* N = GetWorld() ? GetWorld()->GetSubsystem<URubiconNarrativeSubsystem>() : nullptr;
	if (!G || !N) return;
	const FRubiconFrameOutputs& F = N->GetFrameOutputsRef();
	if (!F.bValid || !F.bHasAtmosphere) return;

	const double TargetLogLux = FMath::Loge(FMath::Max(1.0, F.Atmosphere.SunIlluminanceLux));
	if (!bInitialized)
	{
		LogLux = TargetLogLux;
		CctK = F.Atmosphere.SkyCctK;
		Aod = F.Atmosphere.AerosolOpticalDepth;
		bInitialized = true;
	}
	else
	{
		const double K = 1.0 - FMath::Exp(-static_cast<double>(DeltaTime) / FMath::Max(0.1, static_cast<double>(TransitionTauSeconds)));
		LogLux += (TargetLogLux - LogLux) * K;
		CctK += (F.Atmosphere.SkyCctK - CctK) * K;
		Aod += (F.Atmosphere.AerosolOpticalDepth - Aod) * K;
	}

	const double Lux = FMath::Exp(LogLux);
	const bool bHdr = GetWorld()->GetFeatureLevel() >= ERHIFeatureLevel::SM5;
	double Intensity = Lux;
	if (!bHdr)
	{
		const double T = FMath::Clamp((LogLux - FMath::Loge(MinLux)) / (FMath::Loge(MaxLux) - FMath::Loge(MinLux)), 0.0, 1.0);
		Intensity = FMath::Lerp(static_cast<double>(MinLdrIntensity), static_cast<double>(MaxLdrIntensity), T);
	}
	if (UDirectionalLightComponent* Sun = G->GetSun())
	{
		Sun->SetIntensity(static_cast<float>(Intensity));
		Sun->SetTemperature(static_cast<float>(CctK));
	}
	const double AodRatio = FMath::Max(0.0, Aod) / FMath::Max(0.01, static_cast<double>(ReferenceAerosolOpticalDepth));
	if (USkyAtmosphereComponent* Sky = G->GetSkyAtmosphere())
	{
		Sky->SetMieScatteringScale(static_cast<float>(DefaultMieScattering * AodRatio));
		Sky->SetMieAbsorptionScale(static_cast<float>(DefaultMieAbsorption * AodRatio));
	}
	if (UExponentialHeightFogComponent* Fog = G->GetHeightFog())
	{
		Fog->SetFogDensity(static_cast<float>(FMath::Min(0.05, DefaultFogDensity * FMath::Sqrt(AodRatio))));
	}
}
