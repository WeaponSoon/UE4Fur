// Fill out your copyright notice in the Description page of Project Settings.


#include "FurSkeletalMeshSceneProxy.h"
#include "FurSkeletalMeshComponent.h"
#include "SkeletalRenderPublic.h"
#include "Rendering/SkeletalMeshRenderData.h"

class FSkeletalMeshSectionIter
{
public:
	FSkeletalMeshSectionIter(const int32 InLODIdx, const FSkeletalMeshObject& InMeshObject, const FSkeletalMeshLODRenderData& InLODData, const FSkeletalMeshSceneProxy::FLODSectionElements& InLODSectionElements)
		: SectionIndex(0)
		, MeshObject(InMeshObject)
		, LODSectionElements(InLODSectionElements)
		, Sections(InLODData.RenderSections)
#if WITH_EDITORONLY_DATA
		, SectionIndexPreview(InMeshObject.SectionIndexPreview)
		, MaterialIndexPreview(InMeshObject.MaterialIndexPreview)
#endif
	{
		while (NotValidPreviewSection())
		{
			SectionIndex++;
		}
	}
	FORCEINLINE FSkeletalMeshSectionIter& operator++()
	{
		do
		{
			SectionIndex++;
		} while (NotValidPreviewSection());
		return *this;
	}
	FORCEINLINE operator bool() const
	{
		return ((SectionIndex < Sections.Num()) && LODSectionElements.SectionElements.IsValidIndex(GetSectionElementIndex()));
	}
	FORCEINLINE const FSkelMeshRenderSection& GetSection() const
	{
		return Sections[SectionIndex];
	}
	FORCEINLINE const int32 GetSectionElementIndex() const
	{
		return SectionIndex;
	}
	FORCEINLINE const FSkeletalMeshSceneProxy::FSectionElementInfo& GetSectionElementInfo() const
	{
		int32 SectionElementInfoIndex = GetSectionElementIndex();
		return LODSectionElements.SectionElements[SectionElementInfoIndex];
	}
	FORCEINLINE bool NotValidPreviewSection()
	{
#if WITH_EDITORONLY_DATA
		if (MaterialIndexPreview == INDEX_NONE)
		{
			int32 ActualPreviewSectionIdx = SectionIndexPreview;

			return	(SectionIndex < Sections.Num()) &&
				((ActualPreviewSectionIdx >= 0) && (ActualPreviewSectionIdx != SectionIndex));
		}
		else
		{
			int32 ActualPreviewMaterialIdx = MaterialIndexPreview;
			int32 ActualPreviewSectionIdx = INDEX_NONE;
			if (ActualPreviewMaterialIdx != INDEX_NONE && Sections.IsValidIndex(SectionIndex))
			{
				const FSkeletalMeshSceneProxy::FSectionElementInfo& SectionInfo = LODSectionElements.SectionElements[SectionIndex];
				if (SectionInfo.UseMaterialIndex == ActualPreviewMaterialIdx)
				{
					ActualPreviewSectionIdx = SectionIndex;
				}
			}

			return	(SectionIndex < Sections.Num()) &&
				((ActualPreviewMaterialIdx >= 0) && (ActualPreviewSectionIdx != SectionIndex));
		}
#else
		return false;
#endif
	}
private:
	int32 SectionIndex;
	const FSkeletalMeshObject& MeshObject;
	const FSkeletalMeshSceneProxy::FLODSectionElements& LODSectionElements;
	const TArray<FSkelMeshRenderSection>& Sections;
#if WITH_EDITORONLY_DATA
	const int32 SectionIndexPreview;
	const int32 MaterialIndexPreview;
#endif
};

SIZE_T FurSkeletalMeshSceneProxy::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

FurSkeletalMeshSceneProxy::FurSkeletalMeshSceneProxy(const USkinnedMeshComponent* Component, FSkeletalMeshRenderData* InSkelMeshRenderData)
	:FSkeletalMeshSceneProxy(Component, InSkelMeshRenderData)
{
	auto* tem = Cast<UFurSkeletalMeshComponent>(Component);
	MultiPassMaterial = tem->MultiPassMaterial;
}

void FurSkeletalMeshSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily & ViewFamily, uint32 VisibilityMap, FMeshElementCollector & Collector) const
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FSkeletalMeshSceneProxy_GetMeshElements);
	GetMeshElementsConditionallySelectable(Views, ViewFamily, true, VisibilityMap, Collector);
}
bool FSkeletalMeshObject::IsMaterialHidden(int32 InLODIndex, int32 MaterialIdx) const
{
	check(LODInfo.IsValidIndex(InLODIndex));
	return LODInfo[InLODIndex].HiddenMaterials.IsValidIndex(MaterialIdx) && LODInfo[InLODIndex].HiddenMaterials[MaterialIdx];
}
void FSkeletalMeshObject::UpdateMinDesiredLODLevel(const FSceneView* View, const FBoxSphereBounds& Bounds, int32 FrameNumber)
{
	static const auto* SkeletalMeshLODRadiusScale = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.SkeletalMeshLODRadiusScale"));
	float LODScale = FMath::Clamp(SkeletalMeshLODRadiusScale->GetValueOnRenderThread(), 0.25f, 1.0f);

	const float ScreenRadiusSquared = ComputeBoundsScreenRadiusSquared(Bounds.Origin, Bounds.SphereRadius, *View) * LODScale * LODScale;

	checkf(SkeletalMeshLODInfo.Num() == SkeletalMeshRenderData->LODRenderData.Num(), TEXT("Mismatched LOD arrays. SkeletalMeshLODInfo.Num() = %d, SkeletalMeshRenderData->LODRenderData.Num() = %d"), SkeletalMeshLODInfo.Num(), SkeletalMeshRenderData->LODRenderData.Num());

	// Need the current LOD
	const int32 CurrentLODLevel = GetLOD();
	const float HysteresisOffset = 0.f;

	int32 NewLODLevel = 0;

	// Look for a lower LOD if the EngineShowFlags is enabled - Thumbnail rendering disables LODs
	if (View->Family && 1 == View->Family->EngineShowFlags.LOD)
	{
		// Iterate from worst to best LOD
		for (int32 LODLevel = SkeletalMeshRenderData->LODRenderData.Num() - 1; LODLevel > 0; LODLevel--)
		{
			// Get ScreenSize for this LOD
			float ScreenSize = SkeletalMeshLODInfo[LODLevel].ScreenSize.Default;

			// If we are considering shifting to a better (lower) LOD, bias with hysteresis.
			if (LODLevel <= CurrentLODLevel)
			{
				ScreenSize += SkeletalMeshLODInfo[LODLevel].LODHysteresis;
			}

			// If have passed this boundary, use this LOD
			if (FMath::Square(ScreenSize * 0.5f) > ScreenRadiusSquared)
			{
				NewLODLevel = LODLevel;
				break;
			}
		}
	}

	// Different path for first-time vs subsequent-times in this function (ie splitscreen)
	if (FrameNumber != LastFrameNumber)
	{
		// Copy last frames value to the version that will be read by game thread
		MaxDistanceFactor = WorkingMaxDistanceFactor;
		MinDesiredLODLevel = WorkingMinDesiredLODLevel;
		LastFrameNumber = FrameNumber;

		WorkingMaxDistanceFactor = ScreenRadiusSquared;
		WorkingMinDesiredLODLevel = NewLODLevel;
	}
	else
	{
		WorkingMaxDistanceFactor = FMath::Max(WorkingMaxDistanceFactor, ScreenRadiusSquared);
		WorkingMinDesiredLODLevel = FMath::Min(WorkingMinDesiredLODLevel, NewLODLevel);
	}
}

void FurSkeletalMeshSceneProxy::GetMeshElementsConditionallySelectable(const TArray<const FSceneView*>& Views, const FSceneViewFamily & ViewFamily, bool bInSelectable, uint32 VisibilityMap, FMeshElementCollector & Collector) const
{
	if (!MeshObject)
	{
		return;
	}
	MeshObject->PreGDMECallback(ViewFamily.Scene->GetGPUSkinCache(), ViewFamily.FrameNumber);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (VisibilityMap & (1 << ViewIndex))
		{
			const FSceneView* View = Views[ViewIndex];
			MeshObject->UpdateMinDesiredLODLevel(View, GetBounds(), ViewFamily.FrameNumber);
		}
	}

	const FEngineShowFlags& EngineShowFlags = ViewFamily.EngineShowFlags;

	const int32 LODIndex = MeshObject->GetLOD();
	check(LODIndex < SkeletalMeshRenderData->LODRenderData.Num());
	const FSkeletalMeshLODRenderData& LODData = SkeletalMeshRenderData->LODRenderData[LODIndex];

	if (LODSections.Num() > 0)
	{
		const FLODSectionElements& LODSection = LODSections[LODIndex];

		check(LODSection.SectionElements.Num() == LODData.RenderSections.Num());

		for (FSkeletalMeshSectionIter Iter(LODIndex, *MeshObject, LODData, LODSection); Iter; ++Iter)
		{
			const FSkelMeshRenderSection& Section = Iter.GetSection();
			const int32 SectionIndex = Iter.GetSectionElementIndex();
			const FSectionElementInfo& SectionElementInfo = Iter.GetSectionElementInfo();

			bool bSectionSelected = false;

#if WITH_EDITORONLY_DATA
			// TODO: This is not threadsafe! A render command should be used to propagate SelectedEditorSection to the scene proxy.
			if (MeshObject->SelectedEditorMaterial != INDEX_NONE)
			{
				bSectionSelected = (MeshObject->SelectedEditorMaterial == SectionElementInfo.UseMaterialIndex);
			}
			else
			{
				bSectionSelected = (MeshObject->SelectedEditorSection == SectionIndex);
			}

#endif
			// If hidden skip the draw
			if (MeshObject->IsMaterialHidden(LODIndex, SectionElementInfo.UseMaterialIndex) || Section.bDisabled)
			{
				continue;
			}
			
			GetDynamicElementsSection(Views, ViewFamily, VisibilityMap, LODData, LODIndex, SectionIndex, bSectionSelected, SectionElementInfo, bInSelectable, Collector);
			for (int i = 0; i < MultiPassMaterial.Num(); ++i)
			{
				if (MultiPassMaterial[i] == nullptr)
					continue;
				FSectionElementInfo info(SectionElementInfo);
				info.Material = MultiPassMaterial[i];
				
				GetDynamicElementsSection(Views, ViewFamily, VisibilityMap, LODData, LODIndex, SectionIndex, bSectionSelected, info, bInSelectable, Collector);
			}
		}
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (VisibilityMap & (1 << ViewIndex))
		{
			if (PhysicsAssetForDebug)
			{
				DebugDrawPhysicsAsset(ViewIndex, Collector, ViewFamily.EngineShowFlags);
			}

			if (EngineShowFlags.MassProperties && DebugMassData.Num() > 0)
			{
				FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
				if (MeshObject->GetComponentSpaceTransforms())
				{
					const TArray<FTransform>& ComponentSpaceTransforms = *MeshObject->GetComponentSpaceTransforms();

					for (const FDebugMassData& DebugMass : DebugMassData)
					{
						if (ComponentSpaceTransforms.IsValidIndex(DebugMass.BoneIndex))
						{
							const FTransform BoneToWorld = ComponentSpaceTransforms[DebugMass.BoneIndex] * FTransform(GetLocalToWorld());
							DebugMass.DrawDebugMass(PDI, BoneToWorld);
						}
					}
				}
			}

			if (ViewFamily.EngineShowFlags.SkeletalMeshes)
			{
				RenderBounds(Collector.GetPDI(ViewIndex), ViewFamily.EngineShowFlags, GetBounds(), IsSelected());
			}

			if (ViewFamily.EngineShowFlags.Bones)
			{
				DebugDrawSkeleton(ViewIndex, Collector, ViewFamily.EngineShowFlags);
			}
		}
	}
#endif
}

