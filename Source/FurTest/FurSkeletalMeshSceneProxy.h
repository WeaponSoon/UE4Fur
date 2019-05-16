// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SkeletalMeshTypes.h"
/**
 * 
 */
class FURTEST_API FurSkeletalMeshSceneProxy : public FSkeletalMeshSceneProxy
{
public:
	virtual SIZE_T GetTypeHash() const;

	FurSkeletalMeshSceneProxy(const USkinnedMeshComponent* Component, FSkeletalMeshRenderData* InSkelMeshRenderData);
	TArray<UMaterialInterface*> MultiPassMaterial;
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily,
		uint32 VisibilityMap, FMeshElementCollector& Collector) const override;
	void GetMeshElementsConditionallySelectable(const TArray<const FSceneView*>& Views, 
		const FSceneViewFamily& ViewFamily, bool bInSelectable, uint32 VisibilityMap, FMeshElementCollector& Collector) const;
};
