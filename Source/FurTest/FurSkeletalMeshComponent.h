// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraTypes.h"
#include "FurSkeletalMeshComponent.generated.h"

/**
 * 
 */
UCLASS(ClassGroup = (Rendering, Common), hidecategories = Object, config = Engine, editinlinenew, meta = (BlueprintSpawnableComponent))
class FURTEST_API UFurSkeletalMeshComponent : public USkeletalMeshComponent
{
	GENERATED_BODY()
private:
	UPROPERTY()
	class USceneCaptureComponent2D* InnerShadowCaster;

public:
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "FurSkeletal")
	class USceneCaptureComponent2D* ShadowCaster() const;
	UFUNCTION(BlueprintCallable, Category = "FurSkeletal")
	void SetShadowCaster(class USceneCaptureComponent2D* newCaster);

	UFUNCTION(BlueprintCallable, Category = "FurSkeletal|Help")
	static void BuildProjectionMatrix(FIntPoint RenderTargetSize, ECameraProjectionMode::Type ProjectionType, float FOV, float InOrthoWidth, FMatrix& ProjectionMatrix);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Multy Pass Component")
	TArray<UMaterialInterface*> MultiPassMaterial;
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
};
