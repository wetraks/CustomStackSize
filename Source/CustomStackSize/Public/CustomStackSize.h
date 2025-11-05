#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Resources/FGItemDescriptor.h"

class UClass;

class CUSTOMSTACKSIZE_API FCustomStackSizeModule : public IModuleInterface
{
public:

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	static void RegisterCustomStackSize(UClass* ItemClass, int32 StackSize, EResourceForm Form = EResourceForm::RF_SOLID);
	static void RegisterCustomStackSize(const FString& ItemPath, int32 StackSize, EResourceForm Form = EResourceForm::RF_SOLID);
	static int32 GetCustomStackSize(UClass* ItemClass);

private:
	void InitHooks();

	FDelegateHandle PostEngineInitHandle;
	FDelegateHandle WorldTickHandle;
};