// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SkeletalMeshComponent.h"
#include "FurSkeletalMeshComponent.generated.h"

/**
 * 
 */
UCLASS(ClassGroup = (Rendering, Common), hidecategories = Object, config = Engine, editinlinenew, meta = (BlueprintSpawnableComponent))
class FURTEST_API UFurSkeletalMeshComponent : public USkeletalMeshComponent
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Multy Pass Component")
	TArray<UMaterialInterface*> MultiPassMaterial;
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const override;
};
