// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Runtime/Core/Public/Modules/ModuleManager.h"
#include "AssetToolsModule.h"
#include "Editor/UnrealEd/Public/Toolkits/AssetEditorToolkit.h"
#include "LevelEditor.h"
#include "PythonEditorStyle.h"
#include "PythonProjectEditor.h"
#include "PythonProject.h"
#if ENGINE_MAJOR_VERSION == 5 || (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 24)
#include "Subsystems/AssetEditorSubsystem.h"
#endif
#include "Runtime/Slate/Public/Framework/MultiBox/MultiBoxBuilder.h"

static const FName PythonEditorTabName( TEXT( "PythonEditor" ) );

#define LOCTEXT_NAMESPACE "PythonEditor"

class FPythonEditor : public IModuleInterface
{
public:

	struct Local
	{
		static TSharedRef<SDockTab> SpawnPythonEditorTab(const FSpawnTabArgs& TabArgs)
		{
			TSharedRef<FPythonProjectEditor> NewPythonProjectEditor(new FPythonProjectEditor());
			NewPythonProjectEditor->InitPythonEditor(EToolkitMode::Standalone, TSharedPtr<class IToolkitHost>(), GetMutableDefault<UPythonProject>());

			return FGlobalTabmanager::Get()->GetMajorTabForTabManager(NewPythonProjectEditor->GetTabManager().ToSharedRef()).ToSharedRef();
		}

		static void OpenPythonEditor()
		{
			SpawnPythonEditorTab(FSpawnTabArgs(TSharedPtr<SWindow>(), FTabId()));
		}

		static void ExtendMenu(class FMenuBuilder& MenuBuilder)
		{
			MenuBuilder.AddMenuEntry
			(
				LOCTEXT("PythonEditorTabTitle", "Python Editor"),
				LOCTEXT("PythonEditorTooltipText", "Open the Python Editor tab."),
				FSlateIcon(FPythonEditorStyle::Get().GetStyleSetName(), "PythonEditor.TabIcon"),
				FUIAction
				(
					FExecuteAction::CreateStatic(&Local::OpenPythonEditor)
				)
			);
		}
	};
	/** IModuleInterface implementation */
	virtual void StartupModule() override
	{
		FPythonEditorStyle::Initialize();

		Extender = MakeShareable(new FExtender());
	
		// Add Python editor extension to main menu
		Extender->AddMenuExtension(
			"WindowLayout",
			EExtensionHook::After,
			TSharedPtr<FUICommandList>(),
			FMenuExtensionDelegate::CreateStatic( &Local::ExtendMenu ) );

		FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor"); // 检查扩展系统是否可用
		if (!LevelEditorModule.GetToolBarExtensibilityManager().IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("工具栏扩展管理器无效!"));
		}
		LevelEditorModule.GetMenuExtensibilityManager()->AddExtender( Extender );

		// Register a tab spawner so that our tab can be automatically restored from layout files
		FGlobalTabmanager::Get()->RegisterTabSpawner( PythonEditorTabName, FOnSpawnTab::CreateStatic( &Local::SpawnPythonEditorTab ) )
				.SetDisplayName( LOCTEXT( "PythonEditorTabTitle", "Python Editor" ) )
				.SetTooltipText( LOCTEXT( "PythonEditorTooltipText", "Open the Python Editor tab." ) )
				.SetIcon(FSlateIcon(FPythonEditorStyle::Get().GetStyleSetName(), "PythonEditor.TabIcon"));

		// 注册工具栏扩展

		// 创建一个工具栏扩展
		ToolbarExtender = MakeShareable(new FExtender);

		// 添加工具栏扩展
		ToolbarExtender->AddToolBarExtension(
			"Play", // 插入到哪个部分之后
			EExtensionHook::After, // 插入位置（之前或之后）
			nullptr, // 命令列表（可以为null）
			FToolBarExtensionDelegate::CreateRaw(this, &FPythonEditor::AddToolbarButton)
		);

		// 注册扩展
		LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender(ToolbarExtender);
		UE_LOG(LogTemp, Log, TEXT("Extender added successfully"));


		
	}
	// 添加工具栏按钮的函数
	virtual void AddToolbarButton(FToolBarBuilder& Builder)
	{
		UE_LOG(LogTemp, Warning, TEXT("AddToolbarButton called!"));

		// 验证图标是否存在
		if (!FPythonEditorStyle::Get().GetBrush("PythonEditor.TabIcon"))
		{
			UE_LOG(LogTemp, Error, TEXT("Tab icon not found!"));
		}
		// 添加一个按钮
		Builder.AddToolBarButton(
			FUIAction(
				FExecuteAction::CreateRaw(this, &FPythonEditor::OnButtonClicked), // 点击时执行的函数
				FCanExecuteAction() // 按钮是否可用的条件（可以为空）
			),
			NAME_None, // 插槽名称
			FText::FromString("Python Editor"), // 按钮文本
			FText::FromString("Open the Python Editor tab."), // 工具提示
			FSlateIcon(FPythonEditorStyle::GetStyleSetName(), "PythonEditor.TabIcon") // 图标
		);
	}
	// 按钮点击处理函数
	void OnButtonClicked()
	{
		UE_LOG(LogTemp, Log, TEXT("Custom toolbar button clicked!"));
		// 在这里添加你的按钮点击逻辑
		Local::OpenPythonEditor();
	}

	virtual void ShutdownModule() override
	{
		// Unregister the tab spawner
		FGlobalTabmanager::Get()->UnregisterTabSpawner( PythonEditorTabName );

		if(FModuleManager::Get().IsModuleLoaded("LevelEditor"))
		{
			FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
			Extender.Reset();
			//LevelEditorModule.GetMenuExtensibilityManager()->RemoveExtender(Extender);
			ToolbarExtender.Reset();
			//LevelEditorModule.GetMenuExtensibilityManager()->RemoveExtender(ToolbarExtender);
		}

		FPythonEditorStyle::Shutdown();
	}

private:
	TSharedPtr<FExtender> Extender; 
	TSharedPtr<FExtender> ToolbarExtender;
};

IMPLEMENT_MODULE( FPythonEditor, PythonEditor )

#undef LOCTEXT_NAMESPACE
