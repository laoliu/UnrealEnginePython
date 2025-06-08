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

		FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor"); // �����չϵͳ�Ƿ����
		if (!LevelEditorModule.GetToolBarExtensibilityManager().IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("��������չ��������Ч!"));
		}
		LevelEditorModule.GetMenuExtensibilityManager()->AddExtender( Extender );

		// Register a tab spawner so that our tab can be automatically restored from layout files
		FGlobalTabmanager::Get()->RegisterTabSpawner( PythonEditorTabName, FOnSpawnTab::CreateStatic( &Local::SpawnPythonEditorTab ) )
				.SetDisplayName( LOCTEXT( "PythonEditorTabTitle", "Python Editor" ) )
				.SetTooltipText( LOCTEXT( "PythonEditorTooltipText", "Open the Python Editor tab." ) )
				.SetIcon(FSlateIcon(FPythonEditorStyle::Get().GetStyleSetName(), "PythonEditor.TabIcon"));

		// ע�Ṥ������չ

		// ����һ����������չ
		ToolbarExtender = MakeShareable(new FExtender);

		// ��ӹ�������չ
		ToolbarExtender->AddToolBarExtension(
			"Play", // ���뵽�ĸ�����֮��
			EExtensionHook::After, // ����λ�ã�֮ǰ��֮��
			nullptr, // �����б�����Ϊnull��
			FToolBarExtensionDelegate::CreateRaw(this, &FPythonEditor::AddToolbarButton)
		);

		// ע����չ
		LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender(ToolbarExtender);
		UE_LOG(LogTemp, Log, TEXT("Extender added successfully"));


		
	}
	// ��ӹ�������ť�ĺ���
	virtual void AddToolbarButton(FToolBarBuilder& Builder)
	{
		UE_LOG(LogTemp, Warning, TEXT("AddToolbarButton called!"));

		// ��֤ͼ���Ƿ����
		if (!FPythonEditorStyle::Get().GetBrush("PythonEditor.TabIcon"))
		{
			UE_LOG(LogTemp, Error, TEXT("Tab icon not found!"));
		}
		// ���һ����ť
		Builder.AddToolBarButton(
			FUIAction(
				FExecuteAction::CreateRaw(this, &FPythonEditor::OnButtonClicked), // ���ʱִ�еĺ���
				FCanExecuteAction() // ��ť�Ƿ���õ�����������Ϊ�գ�
			),
			NAME_None, // �������
			FText::FromString("Python Editor"), // ��ť�ı�
			FText::FromString("Open the Python Editor tab."), // ������ʾ
			FSlateIcon(FPythonEditorStyle::GetStyleSetName(), "PythonEditor.TabIcon") // ͼ��
		);
	}
	// ��ť���������
	void OnButtonClicked()
	{
		UE_LOG(LogTemp, Log, TEXT("Custom toolbar button clicked!"));
		// �����������İ�ť����߼�
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
