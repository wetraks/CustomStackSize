#include "CustomStackSize.h"
#include "Modules/ModuleManager.h"
#include "FGItemDescriptor.h"
#include "FGRecipe.h"
#include "FGBuildableManufacturer.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#include "Engine/Engine.h"
#include "Logging/LogMacros.h"
#include "CoreDelegates.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetData.h"
#include "Engine/World.h"
#include "LargeFluidOutputBuffersConfigurationStruct.h"
#include "Configuration/ConfigProperty.h"

DEFINE_LOG_CATEGORY_STATIC(LogCustomStackSize, Log, All);

#define LOCTEXT_NAMESPACE "FCustomStackSizeModule"

// Global registry for custom stack Size
static TMap<UClass*, int32> GCustomStackSize;
static TMap<UClass*, EResourceForm> GCustomResourceForms;

static FNativeFuncPtr OriginalGetStackSizeNative = nullptr;
static UFunction* GetStackSizeFunction = nullptr;

static FDelegateHandle WorldInitHandle;

void SetFluidBuffersToDynamicMode(UWorld* World)
{
	if (!World || !IsValid(World))
	{
		UE_LOG(LogCustomStackSize, Error, TEXT("[CustomStackSize] Invalid World pointer in SetFluidBuffersToDynamicMode"));
		return;
	}

	if (!World->bIsWorldInitialized)
	{
		UE_LOG(LogCustomStackSize, Warning, TEXT("[CustomStackSize] World not initialized yet"));
		return;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance || !IsValid(GameInstance))
	{
		UE_LOG(LogCustomStackSize, Error, TEXT("[CustomStackSize] Failed to get GameInstance"));
		return;
	}

	UConfigManager* ConfigManager = GameInstance->GetSubsystem<UConfigManager>();
	if (!ConfigManager || !IsValid(ConfigManager))
	{
		UE_LOG(LogCustomStackSize, Error, TEXT("[CustomStackSize] Failed to get ConfigManager subsystem"));
		return;
	}

	FConfigId ConfigId{ "LargeFluidOutputBuffers", "" };

	// Get the root configuration section
	UConfigPropertySection* RootSection = ConfigManager->GetConfigurationRootSection(ConfigId);
	if (!RootSection || !IsValid(RootSection))
	{
		UE_LOG(LogCustomStackSize, Error, TEXT("[CustomStackSize] Failed to get root configuration section"));
		return;
	}

	UE_LOG(LogCustomStackSize, Log, TEXT("[CustomStackSize] Got root section, modifying properties..."));

	// Set EnableInputAdjustments
	if (UConfigProperty** EnableInputPropPtr = RootSection->SectionProperties.Find(TEXT("EnableInputAdjustments")))
	{
		if (UConfigProperty* EnableInputProp = *EnableInputPropPtr)
		{
			// Try to find the Value property via reflection
			if (FBoolProperty* BoolProp = CastField<FBoolProperty>(EnableInputProp->GetClass()->FindPropertyByName(TEXT("Value"))))
			{
				void* PropPtr = BoolProp->ContainerPtrToValuePtr<void>(EnableInputProp);
				BoolProp->SetPropertyValue(PropPtr, true);
				UE_LOG(LogCustomStackSize, Warning, TEXT("[CustomStackSize] Set EnableInputAdjustments = true"));
			}
		}
	}

	// Set DynamicSettings.AutoSetBuffers and DynamicSettings.ExceedPipeMax
	if (UConfigProperty** DynamicSettingsPropPtr = RootSection->SectionProperties.Find(TEXT("DynamicSettings")))
	{
		if (UConfigPropertySection* DynamicSection = Cast<UConfigPropertySection>(*DynamicSettingsPropPtr))
		{
			// AutoSetBuffers = true
			if (UConfigProperty** AutoSetPropPtr = DynamicSection->SectionProperties.Find(TEXT("AutoSetBuffers")))
			{
				if (UConfigProperty* AutoSetProp = *AutoSetPropPtr)
				{
					if (FBoolProperty* BoolProp = CastField<FBoolProperty>(AutoSetProp->GetClass()->FindPropertyByName(TEXT("Value"))))
					{
						void* PropPtr = BoolProp->ContainerPtrToValuePtr<void>(AutoSetProp);
						BoolProp->SetPropertyValue(PropPtr, true);
						UE_LOG(LogCustomStackSize, Warning, TEXT("[CustomStackSize] Set DynamicSettings.AutoSetBuffers = true"));
					}
				}
			}

			// ExceedPipeMax = true
			if (UConfigProperty** ExceedPipePropPtr = DynamicSection->SectionProperties.Find(TEXT("ExceedPipeMax")))
			{
				if (UConfigProperty* ExceedPipeProp = *ExceedPipePropPtr)
				{
					if (FBoolProperty* BoolProp = CastField<FBoolProperty>(ExceedPipeProp->GetClass()->FindPropertyByName(TEXT("Value"))))
					{
						void* PropPtr = BoolProp->ContainerPtrToValuePtr<void>(ExceedPipeProp);
						BoolProp->SetPropertyValue(PropPtr, true);
						UE_LOG(LogCustomStackSize, Warning, TEXT("[CustomStackSize] Set DynamicSettings.ExceedPipeMax = true"));
					}
				}
			}
		}
	}

	// Set InputDynamicSettings.AutoSetBuffers and InputDynamicSettings.ExceedPipeMax
	if (UConfigProperty** InputDynamicSettingsPropPtr = RootSection->SectionProperties.Find(TEXT("InputDynamicSettings")))
	{
		if (UConfigPropertySection* InputDynamicSection = Cast<UConfigPropertySection>(*InputDynamicSettingsPropPtr))
		{
			// AutoSetBuffers = true
			if (UConfigProperty** AutoSetPropPtr = InputDynamicSection->SectionProperties.Find(TEXT("AutoSetBuffers")))
			{
				if (UConfigProperty* AutoSetProp = *AutoSetPropPtr)
				{
					if (FBoolProperty* BoolProp = CastField<FBoolProperty>(AutoSetProp->GetClass()->FindPropertyByName(TEXT("Value"))))
					{
						void* PropPtr = BoolProp->ContainerPtrToValuePtr<void>(AutoSetProp);
						BoolProp->SetPropertyValue(PropPtr, true);
						UE_LOG(LogCustomStackSize, Warning, TEXT("[CustomStackSize] Set InputDynamicSettings.AutoSetBuffers = true"));
					}
				}
			}

			// ExceedPipeMax = true
			if (UConfigProperty** ExceedPipePropPtr = InputDynamicSection->SectionProperties.Find(TEXT("ExceedPipeMax")))
			{
				if (UConfigProperty* ExceedPipeProp = *ExceedPipePropPtr)
				{
					if (FBoolProperty* BoolProp = CastField<FBoolProperty>(ExceedPipeProp->GetClass()->FindPropertyByName(TEXT("Value"))))
					{
						void* PropPtr = BoolProp->ContainerPtrToValuePtr<void>(ExceedPipeProp);
						BoolProp->SetPropertyValue(PropPtr, true);
						UE_LOG(LogCustomStackSize, Warning, TEXT("[CustomStackSize] Set InputDynamicSettings.ExceedPipeMax = true"));
					}
				}
			}
		}
	}


	ConfigManager->MarkConfigurationDirty(ConfigId);

	ConfigManager->FlushPendingSaves();

	UE_LOG(LogCustomStackSize, Warning, TEXT("[CustomStackSize] *** Fluid buffers config applied and saved! ***"));
}


// ============================================================================
// MODULE IMPLEMENTATION
// ============================================================================

void FCustomStackSizeModule::StartupModule()
{
	UE_LOG(LogCustomStackSize, Log, TEXT("[CustomStackSize] Module starting up"));

	PostEngineInitHandle = FCoreDelegates::OnFEngineLoopInitComplete.AddLambda([this]()
		{
			UE_LOG(LogCustomStackSize, Log, TEXT("[CustomStackSize] Engine loop init complete, initializing hooks..."));
			InitHooks();

			WorldInitHandle = FWorldDelegates::OnWorldTickStart.AddLambda(
				[this](UWorld* World, ELevelTick TickType, float DeltaTime)
				{
					static bool bConfigApplied = false;
					if (!bConfigApplied && World && World->IsGameWorld())
					{
						if (!IsValid(World) || !World->bIsWorldInitialized)
						{
							UE_LOG(LogCustomStackSize, Log, TEXT("[CustomStackSize] World not ready yet, waiting..."));
							return;
						}

						SetFluidBuffersToDynamicMode(World);
						bConfigApplied = true;

						UE_LOG(LogCustomStackSize, Warning, TEXT("[CustomStackSize] *** GLOBAL CONFIG APPLIED ON STARTUP ***"));
					}
				});

			WorldTickHandle = FWorldDelegates::OnWorldTickStart.AddLambda(
				[this](UWorld* World, ELevelTick TickType, float DeltaTime)
				{
					static float TimeSinceLastCheck = 0.0f;
					static bool bLastKnownState[5] = { false, false, false, false, false };
					// [0] EnableInputAdjustments
					// [1] DynamicSettings.AutoSetBuffers
					// [2] InputDynamicSettings.AutoSetBuffers
					// [3] DynamicSettings.ExceedPipeMax
					// [4] InputDynamicSettings.ExceedPipeMax

					TimeSinceLastCheck += DeltaTime;

					if (TimeSinceLastCheck >= 1.5f)
					{
						TimeSinceLastCheck = 0.0f;

						if (World && World->IsGameWorld() && World->GetGameInstance())
						{
							UConfigManager* ConfigManager = World->GetGameInstance()->GetSubsystem<UConfigManager>();
							if (ConfigManager && IsValid(ConfigManager))
							{
								FConfigId ConfigId{ "LargeFluidOutputBuffers", "" };
								UConfigPropertySection* RootSection = ConfigManager->GetConfigurationRootSection(ConfigId);

								if (RootSection && IsValid(RootSection))
								{
									bool bCurrentStates[5] = { false, false, false, false, false };

									// 0: EnableInputAdjustments
									if (UConfigProperty** Prop1 = RootSection->SectionProperties.Find(TEXT("EnableInputAdjustments")))
									{
										if (UConfigProperty* P = *Prop1)
										{
											if (FBoolProperty* BP = CastField<FBoolProperty>(P->GetClass()->FindPropertyByName(TEXT("Value"))))
											{
												void* Ptr = BP->ContainerPtrToValuePtr<void>(P);
												bCurrentStates[0] = BP->GetPropertyValue(Ptr);
											}
										}
									}

									// 1: DynamicSettings.AutoSetBuffers
									// 3: DynamicSettings.ExceedPipeMax
									if (UConfigProperty** Prop2 = RootSection->SectionProperties.Find(TEXT("DynamicSettings")))
									{
										if (UConfigPropertySection* Section = Cast<UConfigPropertySection>(*Prop2))
										{
											// AutoSetBuffers
											if (UConfigProperty** Prop3 = Section->SectionProperties.Find(TEXT("AutoSetBuffers")))
											{
												if (UConfigProperty* P = *Prop3)
												{
													if (FBoolProperty* BP = CastField<FBoolProperty>(P->GetClass()->FindPropertyByName(TEXT("Value"))))
													{
														void* Ptr = BP->ContainerPtrToValuePtr<void>(P);
														bCurrentStates[1] = BP->GetPropertyValue(Ptr);
													}
												}
											}

											// ExceedPipeMax
											if (UConfigProperty** PropExceed = Section->SectionProperties.Find(TEXT("ExceedPipeMax")))
											{
												if (UConfigProperty* P = *PropExceed)
												{
													if (FBoolProperty* BP = CastField<FBoolProperty>(P->GetClass()->FindPropertyByName(TEXT("Value"))))
													{
														void* Ptr = BP->ContainerPtrToValuePtr<void>(P);
														bCurrentStates[3] = BP->GetPropertyValue(Ptr);
													}
												}
											}
										}
									}

									// 2: InputDynamicSettings.AutoSetBuffers
									// 4: InputDynamicSettings.ExceedPipeMax
									if (UConfigProperty** Prop4 = RootSection->SectionProperties.Find(TEXT("InputDynamicSettings")))
									{
										if (UConfigPropertySection* Section = Cast<UConfigPropertySection>(*Prop4))
										{
											// AutoSetBuffers
											if (UConfigProperty** Prop5 = Section->SectionProperties.Find(TEXT("AutoSetBuffers")))
											{
												if (UConfigProperty* P = *Prop5)
												{
													if (FBoolProperty* BP = CastField<FBoolProperty>(P->GetClass()->FindPropertyByName(TEXT("Value"))))
													{
														void* Ptr = BP->ContainerPtrToValuePtr<void>(P);
														bCurrentStates[2] = BP->GetPropertyValue(Ptr);
													}
												}
											}

											// ExceedPipeMax
											if (UConfigProperty** PropExceed = Section->SectionProperties.Find(TEXT("ExceedPipeMax")))
											{
												if (UConfigProperty* P = *PropExceed)
												{
													if (FBoolProperty* BP = CastField<FBoolProperty>(P->GetClass()->FindPropertyByName(TEXT("Value"))))
													{
														void* Ptr = BP->ContainerPtrToValuePtr<void>(P);
														bCurrentStates[4] = BP->GetPropertyValue(Ptr);
													}
												}
											}
										}
									}

									// Detect if any tracked value has changed or is false
									bool bNeedsReapply = false;
									for (int i = 0; i < 5; ++i)
									{
										if (!bCurrentStates[i] && bLastKnownState[i])
											bNeedsReapply = true;
										if (!bCurrentStates[i])
											bNeedsReapply = true;
									}

									if (bNeedsReapply)
									{
										UE_LOG(LogCustomStackSize, Warning, TEXT("[CustomStackSize] Config was changed, re-applying..."));
										SetFluidBuffersToDynamicMode(World);
									}

									// Enforce all to true after reapplication
									for (int i = 0; i < 5; ++i)
										bLastKnownState[i] = true;
								}
							}
						}
					}
				});


			UE_LOG(LogCustomStackSize, Log, TEXT("[CustomStackSize] Registered config monitoring system."));
		});
}

void FCustomStackSizeModule::ShutdownModule()
{
	UE_LOG(LogCustomStackSize, Log, TEXT("[CustomStackSize] Module shutting down"));

	if (PostEngineInitHandle.IsValid())
	{
		FCoreDelegates::OnFEngineLoopInitComplete.Remove(PostEngineInitHandle);
		PostEngineInitHandle.Reset();
	}

	if (WorldInitHandle.IsValid())
	{
		FWorldDelegates::OnWorldTickStart.Remove(WorldInitHandle);
		WorldInitHandle.Reset();
	}

	if (WorldTickHandle.IsValid())
	{
		FWorldDelegates::OnWorldTickStart.Remove(WorldTickHandle);
		WorldTickHandle.Reset();
	}

	// Clear global maps
	GCustomStackSize.Empty();
	GCustomResourceForms.Empty();

	UE_LOG(LogCustomStackSize, Log, TEXT("[CustomStackSize] Module shutdown complete"));
}

// Custom GetStackSize implementation
void CustomGetStackSize_Native(UObject* Context, FFrame& Stack, void* const Z_Param__Result)
{
	// Check for null result pointer
	if (!Z_Param__Result)
	{
		UE_LOG(LogCustomStackSize, Error, TEXT("[CustomStackSize] Null result pointer!"));
		return;
	}

	UE_LOG(LogCustomStackSize, Log, TEXT("[CustomStackSize] GetStackSize called on: %s"),
		Context ? *Context->GetClass()->GetName() : TEXT("NULL"));

	UClass* ItemClass = nullptr;

	if (Context)
	{
		if (Context->IsA<UClass>())
		{
			ItemClass = Cast<UClass>(Context);
		}
		else
		{
			ItemClass = Context->GetClass();
		}
	}

	if (!ItemClass)
	{
		P_GET_OBJECT(UClass, InItemClass);
		ItemClass = InItemClass;
	}

	if (ItemClass && GCustomStackSize.Contains(ItemClass))
	{
		int32 CustomSize = GCustomStackSize[ItemClass];

		*(int32*)Z_Param__Result = CustomSize;

		UE_LOG(LogCustomStackSize, Warning, TEXT("[CustomStackSize] >>> RETURNING CUSTOM STACK SIZE %d for %s <<<"),
			CustomSize, *ItemClass->GetName());

		return;
	}

	UE_LOG(LogCustomStackSize, Log, TEXT("[CustomStackSize] No custom size found, calling original"));

	if (OriginalGetStackSizeNative)
	{
		OriginalGetStackSizeNative(Context, Stack, Z_Param__Result);

		UE_LOG(LogCustomStackSize, Log, TEXT("[CustomStackSize] Original returned: %d"), *(int32*)Z_Param__Result);
	}
	else
	{
		// Fallback: calculate from enum
		UFGItemDescriptor* Descriptor = Cast<UFGItemDescriptor>(Context);
		if (Descriptor && ItemClass)
		{
			FProperty* StackSizeProp = ItemClass->FindPropertyByName(TEXT("mStackSize"));
			if (StackSizeProp)
			{
				if (FEnumProperty* EnumProp = CastField<FEnumProperty>(StackSizeProp))
				{
					void* PropPtr = EnumProp->ContainerPtrToValuePtr<void>(Descriptor);
					if (PropPtr)
					{
						FNumericProperty* UnderlyingProp = EnumProp->GetUnderlyingProperty();
						if (UnderlyingProp)
						{
							int64 EnumValue = UnderlyingProp->GetSignedIntPropertyValue(PropPtr);

							// Default game mappings
							int32 StackSize = 1;
							switch (EnumValue)
							{
							case 0: StackSize = 1; break;      // SS_ONE
							case 1: StackSize = 50; break;     // SS_SMALL
							case 2: StackSize = 100; break;    // SS_MEDIUM
							case 3: StackSize = 200; break;    // SS_BIG
							case 4: StackSize = 500; break;    // SS_HUGE
							case 5: StackSize = 50000; break;  // SS_FLUID
							default: StackSize = 1; break;
							}

							*(int32*)Z_Param__Result = StackSize;
							UE_LOG(LogCustomStackSize, Log, TEXT("[CustomStackSize] Fallback returned: %d"), StackSize);
							return;
						}
					}
				}
			}
		}

		// Final fallback
		*(int32*)Z_Param__Result = 1;
		UE_LOG(LogCustomStackSize, Warning, TEXT("[CustomStackSize] Ultimate fallback: returning 1"));
	}
}

void FCustomStackSizeModule::InitHooks()
{
	UE_LOG(LogCustomStackSize, Log, TEXT("[CustomStackSize] Initializing hooks..."));

	UClass* ItemDescClass = LoadObject<UClass>(nullptr, TEXT("/Script/FactoryGame.FGItemDescriptor"));
	if (!ItemDescClass)
	{
		UE_LOG(LogCustomStackSize, Error, TEXT("[CustomStackSize] Failed to load FGItemDescriptor class!"));
		return;
	}

	GetStackSizeFunction = ItemDescClass->FindFunctionByName(TEXT("GetStackSize"));
	if (!GetStackSizeFunction)
	{
		UE_LOG(LogCustomStackSize, Error, TEXT("[CustomStackSize] GetStackSize function not found!"));

		UE_LOG(LogCustomStackSize, Log, TEXT("[CustomStackSize] Available functions:"));
		for (TFieldIterator<UFunction> FuncIt(ItemDescClass); FuncIt; ++FuncIt)
		{
			UFunction* Func = *FuncIt;
			UE_LOG(LogCustomStackSize, Log, TEXT("  - %s (Static: %d, Native: %d)"),
				*Func->GetName(),
				!!(Func->FunctionFlags & FUNC_Static),
				!!(Func->FunctionFlags & FUNC_Native));
		}
		return;
	}

	UE_LOG(LogCustomStackSize, Log, TEXT("[CustomStackSize] Found GetStackSize function: %s"),
		*GetStackSizeFunction->GetName());
	UE_LOG(LogCustomStackSize, Log, TEXT("[CustomStackSize] Function flags - Static: %d, Native: %d, BlueprintCallable: %d"),
		!!(GetStackSizeFunction->FunctionFlags & FUNC_Static),
		!!(GetStackSizeFunction->FunctionFlags & FUNC_Native),
		!!(GetStackSizeFunction->FunctionFlags & FUNC_BlueprintCallable));

	if (GetStackSizeFunction->FunctionFlags & FUNC_Native)
	{
		UE_LOG(LogCustomStackSize, Log, TEXT("[CustomStackSize] GetStackSize is native - hooking native function pointer"));

		// Save the original native function pointer
		OriginalGetStackSizeNative = GetStackSizeFunction->GetNativeFunc();

		if (OriginalGetStackSizeNative)
		{
			UE_LOG(LogCustomStackSize, Log, TEXT("[CustomStackSize] Original native function saved at %p"),
				OriginalGetStackSizeNative);
		}
		else
		{
			UE_LOG(LogCustomStackSize, Error, TEXT("[CustomStackSize] Failed to get original native function pointer!"));
			return;
		}

		// Replace with custom implementation
		GetStackSizeFunction->SetNativeFunc(&CustomGetStackSize_Native);

		// Verify the hook was installed
		FNativeFuncPtr CurrentFunc = GetStackSizeFunction->GetNativeFunc();
		if (CurrentFunc == &CustomGetStackSize_Native)
		{
			UE_LOG(LogCustomStackSize, Warning, TEXT("[CustomStackSize] *** HOOK INSTALLED SUCCESSFULLY! ***"));
		}
		else
		{
			UE_LOG(LogCustomStackSize, Error, TEXT("[CustomStackSize] Hook installation FAILED - pointer mismatch!"));
		}

		UE_LOG(LogCustomStackSize, Log, TEXT("[CustomStackSize] Successfully hooked GetStackSize native function!"));
	}
	else
	{
		UE_LOG(LogCustomStackSize, Warning, TEXT("[CustomStackSize] GetStackSize is not native - Blueprint override recommended"));
	}

	UE_LOG(LogCustomStackSize, Log, TEXT("[CustomStackSize] Hook initialization complete!"));
}

// Static helper function to apply form changes
static void ApplyFormToCDO(UClass* ItemClass, EResourceForm Form)
{
	if (!ItemClass || !IsValid(ItemClass))
		return;

	UObject* CDO = ItemClass->GetDefaultObject();
	if (!CDO || !IsValid(CDO))
	{
		UE_LOG(LogCustomStackSize, Error, TEXT("[CustomStackSize] Failed to get CDO for %s"), *ItemClass->GetName());
		return;
	}

	// Ensure we're on the game thread
	if (!IsInGameThread())
	{
		UE_LOG(LogCustomStackSize, Error, TEXT("[CustomStackSize] Attempted to modify CDO from non-game thread!"));
		return;
	}

	// Set resource form (solid/liquid/gas)
	FProperty* FormProp = ItemClass->FindPropertyByName(TEXT("mForm"));
	if (FormProp)
	{
		if (FEnumProperty* EnumProp = CastField<FEnumProperty>(FormProp))
		{
			void* PropPtr = EnumProp->ContainerPtrToValuePtr<void>(CDO);
			if (PropPtr)
			{
				FNumericProperty* UnderlyingProp = EnumProp->GetUnderlyingProperty();
				if (UnderlyingProp)
				{
					UnderlyingProp->SetIntPropertyValue(PropPtr, (int64)Form);

					UE_LOG(LogCustomStackSize, Log, TEXT("[CustomStackSize] Set form to %d for %s"),
						(int32)Form, *ItemClass->GetName());
				}
			}
		}
	}

	// Try to find and override any cached stack size value
	FProperty* CachedStackProp = ItemClass->FindPropertyByName(TEXT("mCachedStackSize"));
	if (CachedStackProp)
	{
		if (FIntProperty* IntProp = CastField<FIntProperty>(CachedStackProp))
		{
			if (int32* CustomSize = GCustomStackSize.Find(ItemClass))
			{
				void* PropPtr = IntProp->ContainerPtrToValuePtr<void>(CDO);
				if (PropPtr)
				{
					IntProp->SetPropertyValue(PropPtr, *CustomSize);
					UE_LOG(LogCustomStackSize, Warning, TEXT("[CustomStackSize] Set cached stack size to %d"), *CustomSize);
				}
			}
		}
	}

	CDO->MarkPackageDirty();
	#if WITH_EDITOR
		CDO->PostEditChange();
	#endif
}

// Public: Register a custom stack size
void FCustomStackSizeModule::RegisterCustomStackSize(UClass* ItemClass, int32 StackSize, EResourceForm Form)
{
	if (!ItemClass || !IsValid(ItemClass))
	{
		UE_LOG(LogCustomStackSize, Warning, TEXT("[CustomStackSize] Attempted to register null or invalid item class"));
		return;
	}

	GCustomStackSize.Add(ItemClass, StackSize);
	GCustomResourceForms.Add(ItemClass, Form);

	UE_LOG(LogCustomStackSize, Log, TEXT("[CustomStackSize] Registered custom stack size: %s -> %d (Form: %d)"),
		*ItemClass->GetName(), StackSize, (int32)Form);

	// Apply form changes to CDO
	ApplyFormToCDO(ItemClass, Form);
}

void FCustomStackSizeModule::RegisterCustomStackSize(const FString& ItemPath, int32 StackSize, EResourceForm Form)
{
	UE_LOG(LogCustomStackSize, Log, TEXT("[CustomStackSize] Attempting to register: %s"), *ItemPath);

	UClass* ItemClass = LoadObject<UClass>(nullptr, *ItemPath);
	if (!ItemClass || !IsValid(ItemClass))
	{
		UE_LOG(LogCustomStackSize, Error, TEXT("[CustomStackSize] Failed to load item class: %s"), *ItemPath);
		return;
	}

	UE_LOG(LogCustomStackSize, Log, TEXT("[CustomStackSize] Successfully loaded class: %s"), *ItemClass->GetName());

	RegisterCustomStackSize(ItemClass, StackSize, Form);
}

int32 FCustomStackSizeModule::GetCustomStackSize(UClass* ItemClass)
{
	if (!ItemClass || !IsValid(ItemClass))
		return -1;

	if (int32* FoundSize = GCustomStackSize.Find(ItemClass))
	{
		return *FoundSize;
	}
	return -1; // Not found
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FCustomStackSizeModule, CustomStackSize)