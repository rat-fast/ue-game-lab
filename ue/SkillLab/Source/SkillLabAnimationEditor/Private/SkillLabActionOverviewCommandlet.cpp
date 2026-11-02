#include "SkillLabActionOverviewCommandlet.h"
#include "SkillLabActionOverview.h"

#include "HAL/FileManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	const TCHAR* Section = TEXT("SkillLab.ActionOverview");
	bool Writable(const FString& Root)
	{
		if (!IFileManager::Get().MakeDirectory(*Root, true)) return false;
		const FString Probe = Root / (TEXT(".write-probe-") + FGuid::NewGuid().ToString());
		if (!FFileHelper::SaveStringToFile(TEXT("probe"), *Probe)) return false;
		return IFileManager::Get().Delete(*Probe);
	}
}

USkillLabActionOverviewCommandlet::USkillLabActionOverviewCommandlet()
{
	// UE 5.8 AllocateScene 需要 GIsClient；否则只有无 SkeletalMeshUpdater 的 dummy scene。
	IsClient = true;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 USkillLabActionOverviewCommandlet::Main(const FString& Params)
{
	using namespace SkillLabActionOverview;
	FString Root;
	if (FParse::Value(*Params, TEXT("SetOutputRoot="), Root))
	{
		if (Root.IsEmpty() || FPaths::IsRelative(Root) || !Writable(Root))
		{
			UE_LOG(LogTemp, Error, TEXT("ActionOverview: 输出根路径必须是可写的绝对目录: %s"), *Root);
			return 1;
		}
		Root = FPaths::ConvertRelativePathToFull(Root);
		GConfig->SetString(Section, TEXT("OutputRoot"), *Root, GEditorPerProjectIni);
		GConfig->Flush(false, GEditorPerProjectIni);
		FConfigFile ReadBack;
		FString Saved;
		ReadBack.Read(GEditorPerProjectIni);
		if (!ReadBack.GetString(Section, TEXT("OutputRoot"), Saved) || Saved != Root)
		{
			UE_LOG(LogTemp, Error, TEXT("ActionOverview: 设置持久化失败: %s"), *GEditorPerProjectIni);
			return 1;
		}
		UE_LOG(LogTemp, Display, TEXT("ActionOverview OutputRoot=%s Settings=%s"), *Root, *GEditorPerProjectIni);
		return 0;
	}

	FString SpecPath, ResultPath, Input;
	FParse::Value(*Params, TEXT("Spec="), SpecPath);
	FParse::Value(*Params, TEXT("Result="), ResultPath);
	if (!ResultPath.IsEmpty() && IFileManager::Get().FileExists(*ResultPath))
	{
		UE_LOG(LogTemp, Error, TEXT("ActionOverview: 拒绝覆盖已有调用 receipt: %s"), *ResultPath);
		return 1;
	}
	TSharedPtr<FJsonObject> Spec;
	if (!FFileHelper::LoadFileToString(Input, *SpecPath) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Input), Spec) || !Spec)
	{
		UE_LOG(LogTemp, Error, TEXT("ActionOverview: 无法读取 JSON 请求: %s"), *SpecPath);
		return 1;
	}
	for (const auto& Pair : Spec->Values)
	{
		if (Pair.Key != TEXT("assets") && Pair.Key != TEXT("mesh"))
		{
			UE_LOG(LogTemp, Error, TEXT("ActionOverview: 未知请求字段: %s"), *Pair.Key);
			return 1;
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* Assets = nullptr;
	FString Mesh;
	if (!Spec->TryGetArrayField(TEXT("assets"), Assets) || Assets->IsEmpty() || Assets->Num() > 1024 ||
		(Spec->HasField(TEXT("mesh")) && !Spec->TryGetStringField(TEXT("mesh"), Mesh))) return 1;
	for (const auto& Value : *Assets)
	{
		FString Asset;
		if (!Value->TryGetString(Asset) || Asset.IsEmpty()) return 1;
	}
	GConfig->GetString(Section, TEXT("OutputRoot"), Root, GEditorPerProjectIni);
	if (Root.IsEmpty() || FPaths::IsRelative(Root) || !Writable(Root))
	{
		UE_LOG(LogTemp, Error, TEXT("ActionOverview: 已保存根路径未设置或不可写，不回退: %s"), *Root);
		return 1;
	}
	const FString BatchId = FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%S")) + TEXT("-") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString BatchDir = Root / BatchId;
	if (!IFileManager::Get().MakeDirectory(*BatchDir, true)) return 1;
	FObject Batch = MakeShared<FJsonObject>();
	Batch->SetNumberField(TEXT("schemaVersion"), 1);
	Batch->SetStringField(TEXT("batchId"), BatchId);
	Batch->SetStringField(TEXT("outputRoot"), Root);
	Batch->SetStringField(TEXT("directory"), BatchDir);
	Batch->SetStringField(TEXT("status"), TEXT("running"));
	TArray<TSharedPtr<FJsonValue>> Results;
	bool bSuccess = true;
	const FString BatchPath = BatchDir / TEXT("batch.json");
	for (int32 Index = 0; Index < Assets->Num(); ++Index)
	{
		FObject Result = MakeShared<FJsonObject>();
		Result->SetStringField(TEXT("asset"), (*Assets)[Index]->AsString());
		Result->SetStringField(TEXT("directory"), BatchDir / FString::Printf(TEXT("%04d-%s"), Index, *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
		Result->SetStringField(TEXT("status"), TEXT("pending"));
		Results.Add(MakeShared<FJsonValueObject>(Result));
	}
	Batch->SetArrayField(TEXT("items"), Results);
	if (!SaveJson(BatchPath, Batch)) return 1;
	if (!ResultPath.IsEmpty() && !SaveJson(ResultPath, Batch)) return 1;
	UE_LOG(LogTemp, Display, TEXT("ActionOverview Batch=%s Status=running"), *BatchPath);
	for (int32 Index = 0; Index < Assets->Num(); ++Index)
	{
		const FString Asset = (*Assets)[Index]->AsString();
		FObject Result = Results[Index]->AsObject().ToSharedRef();
		const FString Dir = Result->GetStringField(TEXT("directory"));
		Result->SetStringField(TEXT("status"), TEXT("running"));
		if (!SaveJson(BatchPath, Batch)) return 1;
		FObject Item = Observe(Asset, Mesh, Dir, Root, BatchId, FParse::Param(*Params, TEXT("MetadataOnly")));
		const bool bSaved = SaveJson(Dir / TEXT("manifest.json"), Item);
		Result->SetStringField(TEXT("status"), bSaved ? Item->GetStringField(TEXT("status")) : TEXT("manifestWriteFailed"));
		bSuccess &= bSaved && Item->GetStringField(TEXT("status")) == TEXT("framesReady");
		Batch->SetArrayField(TEXT("items"), Results);
		if (!SaveJson(BatchPath, Batch)) return 1;
	}
	Batch->SetStringField(TEXT("status"), bSuccess ? TEXT("framesReady") : TEXT("incomplete"));
	if (!SaveJson(BatchPath, Batch)) return 1;
	if (!ResultPath.IsEmpty())
	{
		if (!SaveJson(ResultPath, Batch)) return 1;
	}
	UE_LOG(LogTemp, Display, TEXT("ActionOverview Batch=%s Status=%s"), *BatchPath, *Batch->GetStringField(TEXT("status")));
	return bSuccess ? 0 : 2;
}
