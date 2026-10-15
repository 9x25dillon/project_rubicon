#include "RubiconEmbodiedBody.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "RubiconGazeTargetActor.h"

namespace RubiconBodyDetail
{
	static const FName OwnHands(TEXT("own_hands"));
	static const FLinearColor Skin(0.42f, 0.33f, 0.27f);
	static const FLinearColor Cloth(0.30f, 0.27f, 0.22f);
}

ARubiconEmbodiedBody::ARubiconEmbodiedBody()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Tags.Add(ARubiconGazeTargetActor::TagFor(RubiconBodyDetail::OwnHands));
}

FRubiconTwoBoneSolution ARubiconEmbodiedBody::SolveTwoBone(const FVector& RootPos, const FVector& Target, double UpperLength, double LowerLength, const FVector& Pole)
{
	FRubiconTwoBoneSolution S;
	S.Root = RootPos;
	const FVector ToTarget = Target - RootPos;
	const double Dist = ToTarget.Size();
	const FVector Dir = Dist > UE_KINDA_SMALL_NUMBER ? ToTarget / Dist : FVector::ForwardVector;
	const double MinReach = FMath::Abs(UpperLength - LowerLength) + 1e-3;
	const double MaxReach = UpperLength + LowerLength - 1e-3;
	const double D = FMath::Clamp(Dist, MinReach, MaxReach);
	S.bReached = Dist >= MinReach && Dist <= MaxReach;

	// Bend plane: pole projected orthogonal to the chain direction.
	FVector Bend = Pole - Dir * FVector::DotProduct(Pole, Dir);
	if (!Bend.Normalize()) Bend = FVector::CrossProduct(Dir, FVector::UpVector).GetSafeNormal(UE_KINDA_SMALL_NUMBER, FVector::RightVector);

	const double CosA = FMath::Clamp((UpperLength * UpperLength + D * D - LowerLength * LowerLength) / (2.0 * UpperLength * D), -1.0, 1.0);
	const double SinA = FMath::Sqrt(FMath::Max(0.0, 1.0 - CosA * CosA));
	S.Joint = RootPos + Dir * (UpperLength * CosA) + Bend * (UpperLength * SinA);
	S.End = RootPos + Dir * D;
	return S;
}

void ARubiconEmbodiedBody::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Rebuild();
}

UStaticMeshComponent* ARubiconEmbodiedBody::AddBlob(const FVector& Center, const FVector& HalfExtent, bool bGazeHands)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
	C->CreationMethod = EComponentCreationMethod::UserConstructionScript;
	C->SetupAttachment(Root);
	C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
	C->SetRelativeLocation(Center);
	C->SetRelativeScale3D(HalfExtent / 50.0);
	if (bGazeHands)
	{
		C->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		C->SetCollisionResponseToAllChannels(ECR_Ignore);
		C->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		C->ComponentTags.Add(ARubiconGazeTargetActor::TagFor(RubiconBodyDetail::OwnHands));
	}
	else
	{
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Base, C);
		Mid->SetVectorParameterValue(TEXT("Color"), RubiconBodyDetail::Skin);
		C->SetMaterial(0, Mid);
	}
	C->RegisterComponent();
	Parts.Add(C);
	return C;
}

UStaticMeshComponent* ARubiconEmbodiedBody::AddSegment(const FVector& A, const FVector& B, double Radius, bool bGazeHands)
{
	const FVector Axis = B - A;
	const double Len = FMath::Max(Axis.Size(), 1.0);
	UStaticMeshComponent* C = AddBlob((A + B) * 0.5, FVector(Radius, Radius, Len * 0.5), bGazeHands);
	C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
	C->SetRelativeRotation(FRotationMatrix::MakeFromZ(Axis.GetSafeNormal()).Rotator());
	return C;
}

void ARubiconEmbodiedBody::Rebuild()
{
	for (UStaticMeshComponent* P : Parts)
	{
		if (IsValid(P)) P->DestroyComponent();
	}
	Parts.Reset();

	const FVector Neck = EyePoint + FVector(-10, 0, -14);
	const FVector ShoulderL = EyePoint + FVector(-12, -ShoulderHalfWidth, -ShoulderDrop);
	const FVector ShoulderR = EyePoint + FVector(-12, ShoulderHalfWidth, -ShoulderDrop);
	const FVector Pelvis = EyePoint + FVector(-6, 0, -ShoulderDrop - TorsoLength);
	const FVector HipL = Pelvis + FVector(0, -10, 0);
	const FVector HipR = Pelvis + FVector(0, 10, 0);

	// Arms: elbows bend down and slightly back; legs: knees bend forward.
	const FRubiconTwoBoneSolution ArmL = SolveTwoBone(ShoulderL, LeftWristOnBeam, UpperArm, Forearm, FVector(-0.3, 0, -1));
	const FRubiconTwoBoneSolution ArmR = SolveTwoBone(ShoulderR, RightWristOnBeam, UpperArm, Forearm, FVector(-0.3, 0, -1));
	const FRubiconTwoBoneSolution LegL = SolveTwoBone(HipL, LeftFoot, Thigh, Shin, FVector(1, 0, 0));
	const FRubiconTwoBoneSolution LegR = SolveTwoBone(HipR, RightFoot, Thigh, Shin, FVector(1, 0, 0));

	AddSegment(Neck, Pelvis, 15, false);                 // torso (camera sits above it; no head proxy in embodiment)
	AddSegment(ShoulderL, ShoulderR, 7, false);           // shoulder line
	AddSegment(ArmL.Root, ArmL.Joint, 4.5, false);
	AddSegment(ArmL.Joint, ArmL.End, 4.0, false);
	AddSegment(ArmR.Root, ArmR.Joint, 4.5, false);
	AddSegment(ArmR.Joint, ArmR.End, 4.0, false);
	AddBlob(ArmL.End + FVector(0, -7, 0), FVector(4, 8, 2.5), true); // hands, open on the beam
	AddBlob(ArmR.End + FVector(0, 7, 0), FVector(4, 8, 2.5), true);
	AddSegment(Pelvis + FVector(0, 0, 8), Pelvis + FVector(0, 0, -14), 14, false); // loincloth region
	AddSegment(LegL.Root, LegL.Joint, 6.5, false);
	AddSegment(LegL.Joint, LegL.End, 5.0, false);
	AddSegment(LegR.Root, LegR.Joint, 6.5, false);
	AddSegment(LegR.Joint, LegR.End, 5.0, false);
}
