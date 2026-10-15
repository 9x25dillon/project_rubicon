#include "RubiconGazeTargetActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/CollisionProfile.h"

namespace RubiconGreyboxAssets
{
	UStaticMesh* Mesh(const TCHAR* Path)
	{
		return LoadObject<UStaticMesh>(nullptr, Path);
	}
	UStaticMesh* Cube() { return Mesh(TEXT("/Engine/BasicShapes/Cube.Cube")); }
	UStaticMesh* Cylinder() { return Mesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")); }
	UStaticMesh* Sphere() { return Mesh(TEXT("/Engine/BasicShapes/Sphere.Sphere")); }
	UMaterialInterface* BaseMaterial() { return LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")); }
}

ARubiconGazeTargetActor::ARubiconGazeTargetActor()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

FName ARubiconGazeTargetActor::TagFor(FName Id)
{
	return FName(*(TEXT("Gaze.Target.") + Id.ToString()));
}

FSphere ARubiconGazeTargetActor::GetGazeBounds() const
{
	const FVector Scale = GetActorScale3D().GetAbs();
	return FSphere(GetActorLocation(), (ProxyExtent * Scale).Size());
}

void ARubiconGazeTargetActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Tags.RemoveAll([](const FName& T) { return T.ToString().StartsWith(TEXT("Gaze.Target.")); });
	if (!TargetId.IsNone()) Tags.Add(TagFor(TargetId));
	RebuildProxies();
}

void ARubiconGazeTargetActor::RebuildProxies()
{
	for (UStaticMeshComponent* P : Proxies)
	{
		if (IsValid(P)) P->DestroyComponent(); // construction-script components may already be gone
	}
	Proxies.Reset();
	if (ProxyShape == ERubiconProxyShape::None) return;

	auto Add = [this](UStaticMesh* Mesh, const FVector& Center, const FVector& HalfExtent)
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
		C->CreationMethod = EComponentCreationMethod::UserConstructionScript;
		C->SetupAttachment(Root);
		C->SetStaticMesh(Mesh);
		// Engine basic shapes are 100 cm across.
		C->SetRelativeLocation(Center);
		C->SetRelativeScale3D(HalfExtent / 50.0);
		C->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
		C->SetCollisionEnabled(Resolution == ERubiconGazeResolution::SkyAbove ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryOnly);
		C->SetCollisionResponseToAllChannels(ECR_Ignore);
		C->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		C->SetCastShadow(true);
		if (UMaterialInterface* Base = RubiconGreyboxAssets::BaseMaterial())
		{
			UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Base, C);
			Mid->SetVectorParameterValue(TEXT("Color"), ProxyColor);
			C->SetMaterial(0, Mid);
		}
		C->ComponentTags.Add(TagFor(TargetId));
		C->RegisterComponent();
		Proxies.Add(C);
	};

	switch (ProxyShape)
	{
	case ERubiconProxyShape::Box: Add(RubiconGreyboxAssets::Cube(), FVector(0, 0, ProxyExtent.Z), ProxyExtent); break;
	case ERubiconProxyShape::Cylinder: Add(RubiconGreyboxAssets::Cylinder(), FVector(0, 0, ProxyExtent.Z), ProxyExtent); break;
	case ERubiconProxyShape::Sphere: Add(RubiconGreyboxAssets::Sphere(), FVector::ZeroVector, ProxyExtent); break;
	case ERubiconProxyShape::Figures:
	{
		// Deterministic scatter of standing figures (about 45 cm wide, height from ProxyExtent.Z) inside the extent.
		const int32 N = FMath::Max(1, FigureCount);
		FRandomStream Rng(static_cast<int32>(GetTypeHash(TargetId)));
		for (int32 I = 0; I < N; ++I)
		{
			const FVector P(Rng.FRandRange(-ProxyExtent.X, ProxyExtent.X), Rng.FRandRange(-ProxyExtent.Y, ProxyExtent.Y), 0);
			const double H = ProxyExtent.Z * Rng.FRandRange(0.92, 1.05);
			Add(RubiconGreyboxAssets::Cylinder(), P + FVector(0, 0, H), FVector(22, 22, H));
			Add(RubiconGreyboxAssets::Sphere(), P + FVector(0, 0, 2 * H + 12), FVector(12, 12, 13));
		}
		break;
	}
	case ERubiconProxyShape::HillRing:
	{
		const int32 N = FMath::Max(3, FigureCount);
		FRandomStream Rng(static_cast<int32>(GetTypeHash(TargetId)) ^ 0x5eed);
		for (int32 I = 0; I < N; ++I)
		{
			const double A = 2.0 * UE_DOUBLE_PI * (I + Rng.FRandRange(-0.3, 0.3)) / N;
			const double R = ProxyExtent.X * Rng.FRandRange(0.9, 1.1);
			const double H = ProxyExtent.Z * Rng.FRandRange(0.4, 1.0);
			const double W = 2.0 * UE_DOUBLE_PI * ProxyExtent.X / N * 0.7;
			const FVector P(FMath::Cos(A) * R, FMath::Sin(A) * R, 0);
			UStaticMeshComponent* Before = Proxies.Num() ? Proxies.Last() : nullptr;
			Add(RubiconGreyboxAssets::Sphere(), P, FVector(W * 0.5, W * 0.5, H));
			if (Proxies.Num() && Proxies.Last() != Before) Proxies.Last()->SetRelativeRotation(FRotator(0, FMath::RadiansToDegrees(A), 0));
		}
		break;
	}
	default: break;
	}
}
