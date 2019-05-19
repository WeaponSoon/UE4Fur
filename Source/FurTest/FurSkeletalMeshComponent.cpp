// Fill out your copyright notice in the Description page of Project Settings.


#include "FurSkeletalMeshComponent.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "SkeletalRenderPublic.h"
#include "FurSkeletalMeshSceneProxy.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/TextureRenderTarget2D.h"

USceneCaptureComponent2D * UFurSkeletalMeshComponent::ShadowCaster() const
{
	return InnerShadowCaster;
}

void UFurSkeletalMeshComponent::SetShadowCaster(USceneCaptureComponent2D * newCaster)
{
	InnerShadowCaster = newCaster;
	if (InnerShadowCaster && InnerShadowCaster->TextureTarget)
	{
		for (int i = 0; i < MultiPassMaterial.Num(); ++i)
		{
			if (MultiPassMaterial[i] != nullptr)
			{
				UMaterialInstanceDynamic* dynamicMat = Cast<UMaterialInstanceDynamic>(MultiPassMaterial[i]);
				if (dynamicMat)
				{
					FMatrix mat;
					FIntPoint size;
					size.X = InnerShadowCaster->TextureTarget->SizeX;
					size.Y = InnerShadowCaster->TextureTarget->SizeY;
					BuildProjectionMatrix(size, InnerShadowCaster->ProjectionType, InnerShadowCaster->FOVAngle, InnerShadowCaster->OrthoWidth, mat);
					auto worldToLocal = InnerShadowCaster->GetComponentTransform().ToInverseMatrixWithScale();
					FMatrix finalMat = /*FMatrix(
						FPlane(0, 0, 1, 0),
						FPlane(1, 0, 0, 0),
						FPlane(0, 1, 0, 0),
						FPlane(0, 0, 0, 1)) **/ worldToLocal * mat;
					finalMat.GetColumn(1);
					FVector4 col0(finalMat.M[0][0], finalMat.M[1][0], finalMat.M[2][0], finalMat.M[3][0]);
					FVector4 col1(finalMat.M[0][1], finalMat.M[1][1], finalMat.M[2][1], finalMat.M[3][1]);
					FVector4 col2(finalMat.M[0][2], finalMat.M[1][2], finalMat.M[2][2], finalMat.M[3][2]);
					FVector4 col3(finalMat.M[0][3], finalMat.M[1][3], finalMat.M[2][3], finalMat.M[3][3]);
					dynamicMat->SetTextureParameterValue("DirectShadowMap", InnerShadowCaster->TextureTarget);
					//FLinearColor a;
					dynamicMat->SetVectorParameterValue("ProjCol0", FLinearColor(col0.X, col0.Y, col0.Z, col0.W));
					dynamicMat->SetVectorParameterValue("ProjCol1", FLinearColor(col1.X, col1.Y, col1.Z, col1.W));
					dynamicMat->SetVectorParameterValue("ProjCol2", FLinearColor(col2.X, col2.Y, col2.Z, col2.W));
					dynamicMat->SetVectorParameterValue("ProjCol3", FLinearColor(col3.X, col3.Y, col3.Z, col3.W));
					dynamicMat->SetVectorParameterValue("SourcePos", InnerShadowCaster->GetComponentLocation());
					dynamicMat->SetVectorParameterValue("SourceDir", InnerShadowCaster->GetForwardVector());
				}
			}
		}
	}
}

void UFurSkeletalMeshComponent::BuildProjectionMatrix(FIntPoint RenderTargetSize, ECameraProjectionMode::Type ProjectionType, float FOV, float InOrthoWidth, FMatrix & ProjectionMatrix)
{
	float const XAxisMultiplier = 1.0f;
	float const YAxisMultiplier = RenderTargetSize.X / (float)RenderTargetSize.Y;

	if (ProjectionType == ECameraProjectionMode::Orthographic)
	{
		check((int32)ERHIZBuffer::IsInverted);
		const float OrthoWidth = InOrthoWidth / 2.0f;
		const float OrthoHeight = InOrthoWidth / 2.0f * XAxisMultiplier / YAxisMultiplier;

		const float NearPlane = 0;
		const float FarPlane = WORLD_MAX / 8.0f;

		const float ZScale = 1.0f / (FarPlane - NearPlane);
		const float ZOffset = -NearPlane;

		ProjectionMatrix = FReversedZOrthoMatrix(
			OrthoWidth,
			OrthoHeight,
			ZScale,
			ZOffset
		);
	}
	else
	{
		if ((int32)ERHIZBuffer::IsInverted)
		{
			ProjectionMatrix = FReversedZPerspectiveMatrix(
				FOV,
				FOV,
				XAxisMultiplier,
				YAxisMultiplier,
				GNearClippingPlane,
				GNearClippingPlane
			);
		}
		else
		{
			ProjectionMatrix = FPerspectiveMatrix(
				FOV,
				FOV,
				XAxisMultiplier,
				YAxisMultiplier,
				GNearClippingPlane,
				GNearClippingPlane
			);
		}
	}
}

FPrimitiveSceneProxy * UFurSkeletalMeshComponent::CreateSceneProxy()
{
	ERHIFeatureLevel::Type SceneFeatureLevel = GetWorld()->FeatureLevel;
	FSkeletalMeshSceneProxy* Result = nullptr;
	FSkeletalMeshRenderData* SkelMeshRenderData = GetSkeletalMeshRenderData();

	// Only create a scene proxy for rendering if properly initialized
	if (SkelMeshRenderData &&
		SkelMeshRenderData->LODRenderData.IsValidIndex(PredictedLODLevel) &&
		!bHideSkin &&
		MeshObject)
	{
		// Only create a scene proxy if the bone count being used is supported, or if we don't have a skeleton (this is the case with destructibles)
		int32 MaxBonesPerChunk = SkelMeshRenderData->GetMaxBonesPerSection();
		int32 MaxSupportedNumBones = MeshObject->IsCPUSkinned() ? MAX_int32 : GetFeatureLevelMaxNumberOfBones(SceneFeatureLevel);
		if (MaxBonesPerChunk <= MaxSupportedNumBones)
		{
			Result = ::new FurSkeletalMeshSceneProxy(this, SkelMeshRenderData);
		}
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	SendRenderDebugPhysics(Result);
#endif

	return Result;
}

void UFurSkeletalMeshComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const
{
	Super::Super::GetUsedMaterials(OutMaterials, bGetDebugMaterials);
	OutMaterials.Append(MultiPassMaterial);
}

void UFurSkeletalMeshComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction * ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (InnerShadowCaster)
	{
		
		for (int i = 0; i < MultiPassMaterial.Num(); ++i)
		{
			if (MultiPassMaterial[i] != nullptr)
			{
				UMaterialInstanceDynamic* dynamicMat = Cast<UMaterialInstanceDynamic>(MultiPassMaterial[i]);
				if (dynamicMat)
				{
					FMatrix mat;
					FIntPoint size;
					size.X = InnerShadowCaster->TextureTarget->SizeX;
					size.Y = InnerShadowCaster->TextureTarget->SizeY;
					BuildProjectionMatrix(size, InnerShadowCaster->ProjectionType, InnerShadowCaster->FOVAngle, InnerShadowCaster->OrthoWidth, mat);
					auto worldToLocal = InnerShadowCaster->GetComponentTransform().ToInverseMatrixWithScale();
					InnerShadowCaster->GetForwardVector();

					FMatrix finalMat =  worldToLocal * FMatrix(
						FPlane(0, 0, 1, 0),
						FPlane(1, 0, 0, 0),
						FPlane(0, 1, 0, 0),
						FPlane(0, 0, 0, 1)) * mat;
					finalMat.GetColumn(1);
					FVector4 col0(finalMat.M[0][0], finalMat.M[1][0], finalMat.M[2][0], finalMat.M[3][0]);
					FVector4 col1(finalMat.M[0][1], finalMat.M[1][1], finalMat.M[2][1], finalMat.M[3][1]);
					FVector4 col2(finalMat.M[0][2], finalMat.M[1][2], finalMat.M[2][2], finalMat.M[3][2]);
					FVector4 col3(finalMat.M[0][3], finalMat.M[1][3], finalMat.M[2][3], finalMat.M[3][3]);
					//dynamicMat->SetTextureParameterValue("DirectShadowMap", InnerShadowCaster->TextureTarget);
					//FLinearColor a;
					dynamicMat->SetVectorParameterValue("ProjCol0", FLinearColor(col0.X, col0.Y, col0.Z, col0.W));
					dynamicMat->SetVectorParameterValue("ProjCol1", FLinearColor(col1.X, col1.Y, col1.Z, col1.W));
					dynamicMat->SetVectorParameterValue("ProjCol2", FLinearColor(col2.X, col2.Y, col2.Z, col2.W));
					dynamicMat->SetVectorParameterValue("ProjCol3", FLinearColor(col3.X, col3.Y, col3.Z, col3.W));
					dynamicMat->SetVectorParameterValue("SourcePos", InnerShadowCaster->GetComponentLocation());
					dynamicMat->SetVectorParameterValue("SourceDir", InnerShadowCaster->GetForwardVector());
				}
			}
		}
	}
}
