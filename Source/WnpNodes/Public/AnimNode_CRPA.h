// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ControlRig.h"
#include "Animation/InputScaleBias.h"
#include "AnimNode_ControlRigBase.h"
#include "ControlRig/Public/Tools/ControlRigPose.h"
#include "AnimNode_CRPA.generated.h"

USTRUCT()
struct WNPNODES_API FAnimNode_CRPA : public FAnimNode_ControlRigBase
{
	GENERATED_BODY()

	FAnimNode_CRPA();
	~FAnimNode_CRPA();

	virtual UControlRig* GetControlRig() const override { return ControlRig; }
	virtual TSubclassOf<UControlRig> GetControlRigClass() const override { return ControlRigClass; }

	// FAnimNode_Base interface
	virtual void
	OnInitializeAnimInstance(const FAnimInstanceProxy* InProxy, const UAnimInstance* InAnimInstance) override;
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual int32 GetLODThreshold() const override { return LODThreshold; }
	void SetIOMapping(bool bInput, const FName& SourceProperty, const FName& TargetCurve);
	FName GetIOMapping(bool bInput, const FName& SourceProperty) const;

	virtual void InitializeProperties(const UObject* InSourceInstance, UClass* InTargetClass) override;
	virtual void PropagateInputProperties(const UObject* InSourceInstance) override;

private:
	
	void HandleOnInitialized_AnyThread(URigVMHost*, const FName&);
#if WITH_EDITOR
	virtual void HandleObjectsReinstanced_Impl(UObject* InSourceObject, UObject* InTargetObject,
	                                           const TMap<UObject*, UObject*>& OldToNewInstanceMap) override;
#endif

private:
	/** Cached ControlRig */
	UPROPERTY(EditAnywhere, Category = ControlRig)
	TSubclassOf<UControlRig> ControlRigClass;

	/** This will be used for comparision and add only required pins*/
	UPROPERTY(EditAnywhere, Category = ControlRig)
	UControlRigPoseAsset* NeutralPoseAsset;

	UPROPERTY(EditAnywhere, Category = ControlRig)
	UControlRigPoseAsset* PoseAsset;

	UPROPERTY(transient)
	TObjectPtr<UControlRig> ControlRig;


	// alpha value handler
	UPROPERTY(EditAnywhere, Category = Settings, meta = (PinShownByDefault))
	float Alpha;

	UPROPERTY(EditAnywhere, Category = Settings)
	EAnimAlphaInputType AlphaInputType;

	UPROPERTY(EditAnywhere, Category = Settings,
		meta = (PinShownByDefault, DisplayName = "bEnabled", DisplayAfter = "AlphaScaleBias"))
	uint8 bAlphaBoolEnabled : 1;

	// Override the initial transforms with those taken from the mesh component
	UPROPERTY(EditAnywhere, Category=Settings, meta = (DisplayName = "Set Initial Transforms From Mesh"))
	uint8 bSetRefPoseFromSkeleton : 1;

	UPROPERTY(EditAnywhere, Category=Settings)
	FInputScaleBias AlphaScaleBias;

	UPROPERTY(EditAnywhere, Category = Settings, meta = (DisplayName = "Blend Settings"))
	FInputAlphaBoolBlend AlphaBoolBlend;

	UPROPERTY(EditAnywhere, Category = Settings, meta = (PinShownByDefault))
	FName AlphaCurveName;

	UPROPERTY(EditAnywhere, Category = Settings)
	FInputScaleBiasClamp AlphaScaleBiasClamp;

	// we only save mapping, 
	// we have to query control rig when runtime 
	// to ensure type and everything is still valid or not
	UPROPERTY()
	TMap<FName, FName> InputMapping;

	UPROPERTY()
	TMap<FName, FName> OutputMapping;

	TMap<FName, FName> InputTypes;
	TMap<FName, FName> OutputTypes;
	TArray<uint8*> DestParameters;

	/*
	 * Max LOD that this node is allowed to run
	 * For example if you have LODThreadhold to be 2, it will run until LOD 2 (based on 0 index)
	 * when the component LOD becomes 3, it will stop update/evaluate
	 * currently transition would be issue and that has to be re-visited
	 */
	UPROPERTY(EditAnywhere, Category = Performance, meta = (DisplayName = "LOD Threshold"))
	int32 LODThreshold;

protected:
	virtual UClass* GetTargetClass() const override { return *ControlRigClass; }
	virtual void UpdateInput(UControlRig* InControlRig, const FPoseContext& InOutput) override;
	virtual void UpdateOutput(UControlRig* InControlRig, FPoseContext& InOutput) override;

	// Helper function to update the initial ref pose within the Control Rig if needed
	void UpdateControlRigRefPoseIfNeeded(const FAnimInstanceProxy* InProxy, bool bIncludePoseInHash = false);

	// A hash to encode the pointer to anim instance, an
	TOptional<int32> RefPoseSetterHash;

public:
	void PostSerialize(const FArchive& Ar);

	friend class UAnimGraphNode_CRPA;
};

template <>
struct TStructOpsTypeTraits<FAnimNode_CRPA> : public TStructOpsTypeTraitsBase2<FAnimNode_CRPA>
{
	enum
	{
		WithPostSerialize = true,
	};
};
