// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "AnimGraphNode_CustomProperty.h"
#include "WnpNodes/Public/AnimNode_CRPA.h"
#include "AnimGraphNode_CRPA.generated.h"

struct FVariableMappingInfo;

/**
 * 
 */
UCLASS(MinimalAPI)
class UAnimGraphNode_CRPA : public UAnimGraphNode_CustomProperty
{
	GENERATED_UCLASS_BODY()
	UPROPERTY(EditAnywhere, Category = Settings)
	FAnimNode_CRPA Node;

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostReconstructNode() override;

private:
	// UEdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual void CustomizePinData(UEdGraphPin* Pin, FName SourcePropertyName, int32 ArrayIndex) const override;
	virtual FText GetTooltipText() const override;

	virtual FAnimNode_CustomProperty* GetCustomPropertyNode() override { return &Node; }
	virtual const FAnimNode_CustomProperty* GetCustomPropertyNode() const override { return &Node; }

	// property related things
	void GetVariables(bool bInput, TMap<FName, FRigVMExternalVariable>& OutParameters) const;

	TMap<FName, FRigVMExternalVariable> InputVariables;
	TMap<FName, FRigVMExternalVariable> OutputVariables;

	void RebuildExposedProperties();
	virtual void CreateCustomPins(TArray<UEdGraphPin*>* OldPins) override;
	FString GetControlValueAsString(const FRigControlCopy& Pose, const FRigControlElement* InControlElement) const;
	virtual void ValidateAnimNodeDuringCompilation(USkeleton* ForSkeleton, FCompilerResultsLog& MessageLog) override;

	// pin option related
	void SetPinForProperty(ECheckBoxState NewState, FName PropertyName);

	bool IsInputProperty(const FName& PropertyName) const;
	FRigControlCopy& GetRigControlCopy(const FName& PropertyName, class UControlRigPoseAsset* PoseAsset) const;
	FRigControlElement* FindControlElement(const FName& InControlName) const;
};
