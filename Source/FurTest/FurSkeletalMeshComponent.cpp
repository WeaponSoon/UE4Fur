// Fill out your copyright notice in the Description page of Project Settings.


#include "FurSkeletalMeshComponent.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "SkeletalRenderPublic.h"
#include "FurSkeletalMeshSceneProxy.h"

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
