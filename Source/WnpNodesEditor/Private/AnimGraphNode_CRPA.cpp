// Fill out your copyright notice in the Description page of Project Settings.

#include "AnimGraphNode_CRPA.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SCheckBox.h"
#include "ScopedTransaction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "AnimationGraphSchema.h"
#include "RigVMBlueprintGeneratedClass.h"
#include "ControlRigBlueprint.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimGraphNode_CRPA)

#define LOCTEXT_NAMESPACE "AnimNode_CRPA"

UAnimGraphNode_CRPA::UAnimGraphNode_CRPA(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FText UAnimGraphNode_CRPA::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	// display control rig here
	return LOCTEXT("AnimGraphNode_AnimNode_CRPA_Title", "Control Rig From Pose asset");
}

FText UAnimGraphNode_CRPA::GetTooltipText() const
{
	// display control rig here
	return LOCTEXT("AnimGraphNode_AnimNode_CRPA_Tooltip", "Evaluates a control rig from pose asset");
}

void UAnimGraphNode_CRPA::CreateCustomPins(TArray<UEdGraphPin*>* OldPins)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	// we do this to refresh input variables
	RebuildExposedProperties();

	// Grab the SKELETON class here as when we are reconstructed during during BP compilation
	// the full generated class is not yet present built.
	UClass* TargetClass = GetTargetSkeletonClass();

	if (!TargetClass)
	{
		// Nothing to search for properties
		return;
	}

	// Need the schema to extract pin types
	// const UEdGraphSchema_K2* Schema = CastChecked<UEdGraphSchema_K2>(GetSchema());

	// Default anim schema for util funcions
	const UAnimationGraphSchema* AnimGraphDefaultSchema = GetDefault<UAnimationGraphSchema>();

#if WITH_EDITOR

	// sustain the current set of custom pins - we'll refrain from changing the node until post load is complete
	if (URigVMBlueprintGeneratedClass* GeneratedClass = Cast<URigVMBlueprintGeneratedClass>(GetTargetClass()))
	{
		if (UControlRigBlueprint* RigBlueprint = Cast<UControlRigBlueprint>(GeneratedClass->ClassGeneratedBy))
		{
			if (RigBlueprint->HasAllFlags(RF_NeedPostLoad))
			{
				if (OldPins)
				{
					for (UEdGraphPin* OldPin : *OldPins)
					{
						// do not rebuild sub pins as they will be treated after in UK2Node::RestoreSplitPins
						const bool bIsSubPin = OldPin->ParentPin && OldPin->ParentPin->bHidden;
						if (bIsSubPin)
						{
							continue;
						}

						bool bFound = false;
						for (UEdGraphPin* CurrentPin : Pins)
						{
							if (CurrentPin->GetFName() == OldPin->GetFName())
							{
								if (CurrentPin->PinType == OldPin->PinType ||
									AnimGraphDefaultSchema->ArePinTypesCompatible(CurrentPin->PinType, OldPin->PinType))
								{
									bFound = true;
									break;
								}
							}
						}

						if (!bFound)
						{
							FName PropertyName = OldPin->GetFName();
							UEdGraphPin* NewPin = CreatePin(EEdGraphPinDirection::EGPD_Input, OldPin->PinType,
							                                PropertyName);
							NewPin->PinFriendlyName = OldPin->PinFriendlyName;

							// Newly created pin does not have an auto generated default value, so we need to generate one here
							// Missing the auto-gen default would cause UEdGraphPin::MovePersistentDataFromOldPin to override
							// the actual default value with the empty auto-gen default, causing BP compiler to complain
							// This is similar to how the following two functions create and initialize new pins, create first 
							// and then set an auto-gen default
							// FOptionalPinManager::CreateVisiblePins()
							// FAnimBlueprintNodeOptionalPinManager::PostInitNewPin()
							AnimGraphDefaultSchema->SetPinAutogeneratedDefaultValueBasedOnType(NewPin);

							AnimGraphDefaultSchema->ResetPinToAutogeneratedDefaultValue(NewPin, false);

							CustomizePinData(NewPin, PropertyName, INDEX_NONE);
						}
					}
				}
				return;
			}
		}
	}
#endif

	// We'll track the names we encounter by removing from this list, if anything remains the properties
	// have been removed from the target class and we should remove them too
	TSet<FName> BeginExposableNames;
	TSet<FName> CurrentlyExposedNames;
	for (const FOptionalPinFromProperty& OptionalPin : CustomPinProperties)
	{
		BeginExposableNames.Add(OptionalPin.PropertyName);

		if (OptionalPin.bShowPin)
		{
			CurrentlyExposedNames.Add(OptionalPin.PropertyName);
		}
	}

	for (TPair<FName, FRigVMExternalVariable>& VariablePair : InputVariables)
	{
		FName PropertyName = VariablePair.Key;
		BeginExposableNames.Remove(PropertyName);

		if (CurrentlyExposedNames.Contains(PropertyName))
		{
			FRigVMExternalVariable& Variable = VariablePair.Value;

			FEdGraphPinType PinType = RigVMTypeUtils::PinTypeFromExternalVariable(Variable);
			if (!PinType.PinCategory.IsValid())
			{
				continue;
			}

			UEdGraphPin* NewPin = CreatePin(EEdGraphPinDirection::EGPD_Input, PinType, PropertyName);
			NewPin->PinFriendlyName = FText::FromName(PropertyName);

			// Newly created pin does not have an auto generated default value, so we need to generate one here
			// Missing the auto-gen default would cause UEdGraphPin::MovePersistentDataFromOldPin to override
			// the actual default value with the empty auto-gen default, causing BP compiler to complain
			// This is similar to how the following two functions create and initialize new pins, create first 
			// and then set an auto-gen default
			// FOptionalPinManager::CreateVisiblePins()
			// FAnimBlueprintNodeOptionalPinManager::PostInitNewPin()
			AnimGraphDefaultSchema->SetPinAutogeneratedDefaultValueBasedOnType(NewPin);

			// We cant interrogate CDO here as we may be mid-compile, so we can only really
			// reset to the autogenerated default.
			AnimGraphDefaultSchema->ResetPinToAutogeneratedDefaultValue(NewPin, false);

			// Extract default values from the Target Control Rig if possible
			// Memory could be null if Control Rig is compiling, so only do it if Memory is not null
			if (Variable.IsValid(false))
			{
				if (URigVMBlueprintGeneratedClass* GeneratedClass = Cast<URigVMBlueprintGeneratedClass>(
					GetTargetClass()))
				{
					for (TFieldIterator<FProperty> PropertyIt(GeneratedClass); PropertyIt; ++PropertyIt)
					{
						if (PropertyIt->GetFName() == PropertyName)
						{
							FString DefaultValue;

							// The format the graph pins editor use is different of what property exporter produces, so we use BlueprintEditorUtils to generate the default string
							// Variable.Memory here points to the corresponding property in the Control Rig BP CDO, it was initialized in UAnimGraphNode_AnimNode_CRPA::RebuildExposedProperties 
							FBlueprintEditorUtils::PropertyValueToString_Direct(
								*PropertyIt, Variable.Memory, DefaultValue, this);

							if (!DefaultValue.IsEmpty())
							{
								AnimGraphDefaultSchema->TrySetDefaultValue(*NewPin, DefaultValue);
							}
						}
					}
				}
			}

			// sustain the current set of custom pins - we'll refrain from changing the node until post load is complete 
			CustomizePinData(NewPin, PropertyName, INDEX_NONE);
		}
	}

	if (const UClass* ControlRigClass = GetTargetClass())
	{
		if (UControlRig* CDO = ControlRigClass->GetDefaultObject<UControlRig>())
		{
			if (const URigHierarchy* Hierarchy = CDO->GetHierarchy())
			{
				Hierarchy->ForEach<FRigControlElement>([&](FRigControlElement* ControlElement) -> bool
				{
					if (Hierarchy->IsAnimatable(ControlElement))
					{
						const FName ControlName = ControlElement->GetName();
						BeginExposableNames.Remove(ControlName);

						if (CurrentlyExposedNames.Contains(ControlName))
						{
							const FEdGraphPinType PinType = Hierarchy->GetControlPinType(ControlElement);
							if (!PinType.PinCategory.IsValid())
							{
								return true;
							}

							UEdGraphPin* NewPin = CreatePin(EEdGraphPinDirection::EGPD_Input, PinType, ControlName);
							NewPin->PinFriendlyName = FText::FromName(ControlElement->GetName());

							// Newly created pin does not have an auto generated default value, so we need to generate one here
							// Missing the auto-gen default would cause UEdGraphPin::MovePersistentDataFromOldPin to override
							// the actual default value with the empty auto-gen default, causing BP compiler to complain
							// This is similar to how the following two functions create and initialize new pins, create first 
							// and then set an auto-gen default
							// FOptionalPinManager::CreateVisiblePins()
							// FAnimBlueprintNodeOptionalPinManager::PostInitNewPin()
							AnimGraphDefaultSchema->SetPinAutogeneratedDefaultValueBasedOnType(NewPin);

							// We cant interrogate CDO here as we may be mid-compile, so we can only really
							// reset to the autogenerated default.
							AnimGraphDefaultSchema->ResetPinToAutogeneratedDefaultValue(NewPin, false);

							FString DefaultValue = "";
							// Extract default values from the Target Control Rig if possible
							if (Node.PoseAsset && Node.PoseAsset->Pose.ContainsName(ControlName))
							{
								const int index = Node.PoseAsset->Pose.CopyOfControlsNameToIndex.FindChecked(
									ControlName);
								DefaultValue = GetControlValueAsString(
									Node.PoseAsset->Pose.CopyOfControls[index], ControlElement);
							}
							else
							{
								DefaultValue = Hierarchy->GetControlPinDefaultValue(ControlElement, true);
							}

							if (!DefaultValue.IsEmpty())
							{
								AnimGraphDefaultSchema->TrySetDefaultValue(*NewPin, DefaultValue);
							}

							// sustain the current set of custom pins - we'll refrain from changing the node until post load is complete 
							CustomizePinData(NewPin, ControlName, INDEX_NONE);
						}
					}

					return true;
				});
			}
		}
	}

	// Remove any properties that no longer exist on the target class
	for (FName& RemovedPropertyName : BeginExposableNames)
	{
		CustomPinProperties.RemoveAll([RemovedPropertyName](const FOptionalPinFromProperty& InOptionalPin)
		{
			return InOptionalPin.PropertyName == RemovedPropertyName;
		});
	}
}

FString UAnimGraphNode_CRPA::GetControlValueAsString(const FRigControlCopy& Pose,
                                                     const FRigControlElement* InControlElement) const
{
	// bool bForEdGraph = true;
	switch (Pose.ControlType)
	{
	case ERigControlType::Bool:
		{
			return Pose.Value.ToString<bool>();
		}
	case ERigControlType::Float:
		{
			return Pose.Value.ToString<float>();
		}
	case ERigControlType::Integer:
		{
			return Pose.Value.ToString<int32>();
		}
	case ERigControlType::Vector2D:
		{
			const FVector3f Vector = Pose.Value.Get<FVector3f>();
			const FVector2D Vector2D(Vector.X, Vector.Y);

			// if (bForEdGraph)
			// {
			// 	return Vector2D.ToString();
			// }

			FString Result;
			TBaseStructure<FVector2D>::Get()->ExportText(Result, &Vector2D, nullptr, nullptr, PPF_None, nullptr);
			return Result;
		}
	case ERigControlType::Position:
	case ERigControlType::Scale:
		{
			// if (bForEdGraph)
			// {
			// 	// NOTE: We can not use ToString() here since the FDefaultValueHelper::IsStringValidVector used in
			// 	// EdGraphSchema_K2 expects a string with format '#,#,#', while FVector:ToString is returning the value
			// 	// with format 'X=# Y=# Z=#'				
			// 	const FVector Vector(Pose.Value.Get<FVector3f>());
			// 	return FString::Printf(TEXT("%3.3f,%3.3f,%3.3f"), Vector.X, Vector.Y, Vector.Z);
			// }
			return Pose.Value.ToString<FVector>();
		}
	case ERigControlType::Rotator:
		{
			// if (bForEdGraph)
			// {
			// 	// NOTE: se explanations above for Position/Scale
			// 	const FRotator Rotator = FRotator::MakeFromEuler((FVector)Pose.Value.GetRef<FVector3f>());
			// 	return FString::Printf(TEXT("%3.3f,%3.3f,%3.3f"), Rotator.Pitch, Rotator.Yaw, Rotator.Roll);
			// }
			return Pose.Value.ToString<FRotator>();
		}
	case ERigControlType::Transform:
	case ERigControlType::TransformNoScale:
	case ERigControlType::EulerTransform:
		{
			const FTransform Transform = Pose.Value.GetAsTransform(
				InControlElement->Settings.ControlType,
				InControlElement->Settings.PrimaryAxis);

			// if (bForEdGraph)
			// {
			// 	return Transform.ToString();
			// }

			FString Result;
			TBaseStructure<FTransform>::Get()->ExportText(Result, &Transform, nullptr, nullptr, PPF_None, nullptr);
			return Result;
		}
	}
	return FString();
}

void UAnimGraphNode_CRPA::ValidateAnimNodeDuringCompilation(USkeleton* ForSkeleton,
                                                            FCompilerResultsLog& MessageLog)
{
	if (UClass* TargetClass = GetTargetClass())
	{
		if (UControlRigBlueprint* Blueprint = Cast<UControlRigBlueprint>(TargetClass->ClassGeneratedBy))
		{
			URigHierarchy* Hierarchy = Blueprint->Hierarchy;
			if (ForSkeleton)
			{
				const FReferenceSkeleton& ReferenceSkeleton = ForSkeleton->GetReferenceSkeleton();
				const TArray<FMeshBoneInfo>& BoneInfos = ReferenceSkeleton.GetRefBoneInfo();

				for (const FMeshBoneInfo& BoneInfo : BoneInfos)
				{
					const FRigElementKey BoneKey(BoneInfo.Name, ERigElementType::Bone);
					if (Hierarchy->Contains(BoneKey))
					{
						const FRigElementKey ParentKey = Hierarchy->GetFirstParent(BoneKey);

						FName DesiredParentName = NAME_None;
						if (BoneInfo.ParentIndex != INDEX_NONE)
						{
							DesiredParentName = BoneInfos[BoneInfo.ParentIndex].Name;
						}

						if (DesiredParentName != ParentKey.Name)
						{
							FString Message = FString::Printf(
								TEXT(
									"@@ - Hierarchy discrepancy for bone '%s' - different parents on Control Rig vs SkeletalMesh."),
								*BoneInfo.Name.ToString());
							MessageLog.Warning(*Message, this);
						}
					}
				}
			}
		}
	}

	Super::ValidateAnimNodeDuringCompilation(ForSkeleton, MessageLog);
}

void UAnimGraphNode_CRPA::RebuildExposedProperties()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	if (URigVMBlueprintGeneratedClass* TargetClass = Cast<URigVMBlueprintGeneratedClass>(GetTargetClass()))
	{
		if (UControlRigBlueprint* RigBlueprint = Cast<UControlRigBlueprint>(TargetClass->ClassGeneratedBy))
		{
			if (RigBlueprint->HasAllFlags(RF_NeedPostLoad))
			{
				return;
			}
		}
	}

	TSet<FName> OldOptionalPinNames;
	TSet<FName> OldExposedPinNames;
	for (const FOptionalPinFromProperty& OptionalPin : CustomPinProperties)
	{
		OldOptionalPinNames.Add(OptionalPin.PropertyName);

		if (OptionalPin.bShowPin)
		{
			OldExposedPinNames.Add(OptionalPin.PropertyName);
		}
	}
	CustomPinProperties.Empty();

	// go through exposed properties, and clean up
	GetVariables(true, InputVariables);
	// we still update OUtputvariables
	// we don't want output to be exposed
	GetVariables(false, OutputVariables);

	// clear IO variables that don't exist anymore
	auto ClearInvalidMapping = [](const TMap<FName, FRigVMExternalVariable>& InVariables,
	                              TMap<FName, FName>& InOutMapping)
	{
		TArray<FName> KeyArray;
		InOutMapping.GenerateKeyArray(KeyArray);

		for (int32 Index = 0; Index < KeyArray.Num(); ++Index)
		{
			// if this input doesn't exist anymore
			if (!InVariables.Contains(KeyArray[Index]))
			{
				InOutMapping.Remove(KeyArray[Index]);
			}
		}
	};

	ClearInvalidMapping(InputVariables, Node.InputMapping);
	ClearInvalidMapping(OutputVariables, Node.OutputMapping);

	auto MakeOptionalPin = [&OldExposedPinNames](FName InPinName)-> FOptionalPinFromProperty
	{
		FOptionalPinFromProperty OptionalPin;
		OptionalPin.PropertyName = InPinName;
		OptionalPin.bShowPin = OldExposedPinNames.Contains(InPinName);
		OptionalPin.bCanToggleVisibility = true;
		OptionalPin.bIsOverrideEnabled = false;
		return OptionalPin;
	};

	for (auto Iter = InputVariables.CreateConstIterator(); Iter; ++Iter)
	{
		CustomPinProperties.Add(MakeOptionalPin(Iter.Key()));
	}

	// also add all of the controls
	if (const UClass* TargetClass = GetTargetClass())
	{
		if (UControlRig* CDO = TargetClass->GetDefaultObject<UControlRig>())
		{
			if (const URigHierarchy* Hierarchy = CDO->GetHierarchy())
			{
				Hierarchy->ForEach<FRigControlElement>([&](const FRigControlElement* ControlElement) -> bool
				{
					if (Hierarchy->IsAnimatable(ControlElement))
					{
						const FName ControlName = ControlElement->GetName();
						CustomPinProperties.Add(MakeOptionalPin(ControlName));
					}
					return true;
				});
			}
		}
	}
}

bool UAnimGraphNode_CRPA::IsInputProperty(const FName& PropertyName) const
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	// this is true for both input variables and controls
	return InputVariables.Contains(PropertyName) || !OutputVariables.Contains(PropertyName);
}

FRigControlCopy& UAnimGraphNode_CRPA::GetRigControlCopy(const FName& PropertyName,
                                                        UControlRigPoseAsset* PoseAsset) const
{
	int Index = PoseAsset->Pose.CopyOfControlsNameToIndex.FindChecked(PropertyName);
	return PoseAsset->Pose.GetPoses()[Index];
}

FRigControlElement* UAnimGraphNode_CRPA::FindControlElement(const FName& InControlName) const
{
	if (const UClass* ControlRigClass = GetTargetClass())
	{
		if (UControlRig* CDO = ControlRigClass->GetDefaultObject<UControlRig>())
		{
			if (const URigHierarchy* Hierarchy = CDO->GetHierarchy())
			{
				return (FRigControlElement*)Hierarchy->Find<FRigControlElement>(
					FRigElementKey(InControlName, ERigElementType::Control));
			}
		}
	}
	return nullptr;
}


void UAnimGraphNode_CRPA::SetPinForProperty(ECheckBoxState NewState, FName PropertyName)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	FOptionalPinFromProperty* FoundPin = CustomPinProperties.FindByPredicate(
		[PropertyName](const FOptionalPinFromProperty& InOptionalPin)
		{
			return InOptionalPin.PropertyName == PropertyName;
		});

	UEdGraphPin* Pin = FindPinByPredicate([PropertyName](const UEdGraphPin* InOptionalPin)
	{
		return InOptionalPin->PinName == PropertyName;
	});

	if (FoundPin)
	{
		if (Pin)
			Pin->ResetToDefaults();

		if (NewState == ECheckBoxState::Checked)
			FoundPin->bShowPin = true;
		else
			FoundPin->bShowPin = false;

		FGuardValue_Bitfield(bDisableOrphanPinSaving, true);
		ReconstructNode();

		// see if any of my child has the mapping, and clear them
		if (NewState == ECheckBoxState::Checked)
		{
			FScopedTransaction Transaction(LOCTEXT("PropertyExposedChanged", "Expose Property to Pin"));
			Modify();

			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprint());

			bool bInput = IsInputProperty(PropertyName);
			// if checked, we clear mapping
			// and unclear all children
			Node.SetIOMapping(bInput, PropertyName, NAME_None);
		}
	}
}


void UAnimGraphNode_CRPA::GetVariables(bool bInput, TMap<FName, FRigVMExternalVariable>& OutVariables) const
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	OutVariables.Reset();

	if (URigVMBlueprintGeneratedClass* TargetClass = Cast<URigVMBlueprintGeneratedClass>(GetTargetClass()))
	{
		if (Cast<UControlRigBlueprint>(TargetClass->ClassGeneratedBy))
		{
			//RigBlueprint->CleanupVariables();
			UControlRig* ControlRig = TargetClass->GetDefaultObject<UControlRig>();
			if (ControlRig)
			{
				const TArray<FRigVMExternalVariable>& PublicVariables = ControlRig->GetPublicVariables();
				for (const FRigVMExternalVariable& PublicVariable : PublicVariables)
				{
					if (!bInput || !PublicVariable.bIsReadOnly)
					{
						OutVariables.Add(PublicVariable.Name, PublicVariable);
					}
				}
			}
		}
	}
}

void UAnimGraphNode_CRPA::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	Super::PostEditChangeProperty(PropertyChangedEvent);

	bool bRequiresNodeReconstruct = false;
	FProperty* ChangedProperty = PropertyChangedEvent.Property;

	if (ChangedProperty)
	{
		if (ChangedProperty->GetFName() == GET_MEMBER_NAME_CHECKED(FAnimNode_CRPA, ControlRigClass))
		{
			bRequiresNodeReconstruct = true;
			RebuildExposedProperties();
		}

		if (ChangedProperty->GetFName() == GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_CRPA, AlphaInputType))
		{
			FScopedTransaction Transaction(LOCTEXT("ChangeAlphaInputType", "Change Alpha Input Type"));
			Modify();

			// Break links to pins going away
			for (int32 PinIndex = 0; PinIndex < Pins.Num(); ++PinIndex)
			{
				UEdGraphPin* Pin = Pins[PinIndex];
				if (Pin->PinName == GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_CRPA, Alpha))
				{
					if (Node.AlphaInputType != EAnimAlphaInputType::Float)
					{
						Pin->BreakAllPinLinks();
						PropertyBindings.Remove(Pin->PinName);
					}
				}
				else if (Pin->PinName == GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_CRPA, bAlphaBoolEnabled))
				{
					if (Node.AlphaInputType != EAnimAlphaInputType::Bool)
					{
						Pin->BreakAllPinLinks();
						PropertyBindings.Remove(Pin->PinName);
					}
				}
				else if (Pin->PinName == GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_CRPA, AlphaCurveName))
				{
					if (Node.AlphaInputType != EAnimAlphaInputType::Curve)
					{
						Pin->BreakAllPinLinks();
						PropertyBindings.Remove(Pin->PinName);
					}
				}
			}
		}

		if (ChangedProperty->GetFName() == GET_MEMBER_NAME_CHECKED(FAnimNode_CRPA, PoseAsset) || ChangedProperty->
			GetFName() == GET_MEMBER_NAME_CHECKED(FAnimNode_CRPA, NeutralPoseAsset))
		{
			if (Node.NeutralPoseAsset && Node.PoseAsset)
			{
				FScopedTransaction Transaction(LOCTEXT("ChangeAlphaInputType", "Change Alpha Input Type"));
				Modify();

				for (auto& Pose : Node.PoseAsset->Pose.GetPoses())
				{
					const FRigControlCopy& NeutralPose = GetRigControlCopy(Pose.Name, Node.NeutralPoseAsset);
					constexpr float Tolerance = 0.0001f;
					bool bAddPin = !NeutralPose.LocalTransform.Equals(Pose.LocalTransform, Tolerance);

					if (bAddPin)
						SetPinForProperty(ECheckBoxState::Checked, Pose.Name);
					else
						SetPinForProperty(ECheckBoxState::Unchecked, Pose.Name);
				}
			}

			bRequiresNodeReconstruct = true;
			RebuildExposedProperties();
		}
	}

	if (bRequiresNodeReconstruct)
	{
		ReconstructNode();
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprint());
	}
}

void UAnimGraphNode_CRPA::PostReconstructNode()
{
	// Fix default values that were serialized directly with property exported strings (not valid for pin editors)
#if WITH_EDITOR
	if (!IsTemplate())
	{
		static UScriptStruct* VectorStruct = TBaseStructure<FVector>::Get();
		static UScriptStruct* TransformStruct = TBaseStructure<FTransform>::Get();

		// fix up any pin data if it needs to 
		for (UEdGraphPin* CurrentPin : Pins)
		{
			const FName& PinCategory = CurrentPin->PinType.PinCategory;

			// Target only struct types vector and transform 
			// For rotator, if it was created with incorect format, it was serialized with DefaultValue = L"0, 0, 0", so it can not be corrected without risking changing users value
			if (PinCategory == UEdGraphSchema_K2::PC_Struct)
			{
				const UAnimationGraphSchema* AnimGraphDefaultSchema = GetDefault<UAnimationGraphSchema>();
				// const FName& PinName = CurrentPin->GetFName();

				if (CurrentPin->PinType.PinSubCategoryObject == VectorStruct)
				{
					// If InitFromString succeeds, it has bad format, re-encode
					FVector FixVector = FVector::ZeroVector;
					if (FixVector.InitFromString(CurrentPin->DefaultValue))
					{
						const FString DefaultValue = FString::Printf(
							TEXT("%f,%f,%f"), FixVector.X, FixVector.Y, FixVector.Z);
						AnimGraphDefaultSchema->TrySetDefaultValue(*CurrentPin, DefaultValue);
					}
				}
				else if (CurrentPin->PinType.PinSubCategoryObject == TransformStruct)
				{
					// If pin was serialized with bad format, transforms would stay empty, so try to get the value from ControlRig or set to Identity as last resort
					if (CurrentPin->DefaultValue.IsEmpty())
					{
						FTransform FixTransform = FTransform::Identity;
						FString DefaultValue = FixTransform.ToString();

						const FRigVMExternalVariable* RigVMExternalVariable = (CurrentPin->Direction == EGPD_Input)
							                                                      ? InputVariables.Find(
								                                                      CurrentPin->GetFName())
							                                                      : (CurrentPin->Direction ==
								                                                      EGPD_Output)
							                                                      ? OutputVariables.Find(
								                                                      CurrentPin->GetFName())
							                                                      : nullptr;

						if (RigVMExternalVariable != nullptr && RigVMExternalVariable->IsValid())
						{
							if (URigVMBlueprintGeneratedClass* GeneratedClass = Cast<URigVMBlueprintGeneratedClass>(
								GetTargetClass()))
							{
								const FName& PropertyName = CurrentPin->GetFName();

								for (TFieldIterator<FProperty> PropertyIt(GeneratedClass); PropertyIt; ++PropertyIt)
								{
									if (PropertyIt->GetFName() == PropertyName)
									{
										// The format the graph pins editor use is different of what property exporter produces, so we use BlueprintEditorUtils to generate the default string
										// Variable.Memory here points to the corresponding property in the Control Rig BP CDO, it was initialized in UAnimGraphNode_AnimNode_CRPA::RebuildExposedProperties 
										FBlueprintEditorUtils::PropertyValueToString_Direct(
											*PropertyIt, RigVMExternalVariable->Memory, DefaultValue, this);
										break;
									}
								}
							}
						}

						AnimGraphDefaultSchema->SetPinAutogeneratedDefaultValue(CurrentPin, DefaultValue);
						AnimGraphDefaultSchema->TrySetDefaultValue(*CurrentPin, DefaultValue);
					}
				}
			}
		}
	}
#endif // WITH_EDITOR

	Super::PostReconstructNode();
}

void UAnimGraphNode_CRPA::CustomizePinData(UEdGraphPin* Pin, FName SourcePropertyName, int32 ArrayIndex) const
{
	Super::CustomizePinData(Pin, SourcePropertyName, ArrayIndex);

	if (Pin->PinName == GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_CRPA, Alpha))
	{
		Pin->bHidden = (Node.AlphaInputType != EAnimAlphaInputType::Float);

		if (!Pin->bHidden)
		{
			Pin->PinFriendlyName = Node.AlphaScaleBias.GetFriendlyName(
				Node.AlphaScaleBiasClamp.GetFriendlyName(Pin->PinFriendlyName));
		}
	}

	if (Pin->PinName == GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_CRPA, bAlphaBoolEnabled))
	{
		Pin->bHidden = (Node.AlphaInputType != EAnimAlphaInputType::Bool);
	}

	if (Pin->PinName == GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_CRPA, AlphaCurveName))
	{
		Pin->bHidden = (Node.AlphaInputType != EAnimAlphaInputType::Curve);

		if (!Pin->bHidden)
		{
			Pin->PinFriendlyName = Node.AlphaScaleBiasClamp.GetFriendlyName(Pin->PinFriendlyName);
		}
	}
}
#undef LOCTEXT_NAMESPACE
