/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "ModPackageIo.h"
#include "CatalogPersistenceService.h"
#include "WorkspacePersistenceService.h"
#include "WorkspaceSchemaService.h"
#include "SourceEvidencePersistenceService.h"
#include "PathPolicyService.h"
#include <QTemporaryDir>
namespace TaintedGrailModdingSDK
{
    using namespace ModPackageIo;
    AZ::Outcome<ModPackagePreview,AZStd::string> ModPackageService::Inspect(
        const QString& archive,const ModPackageProgress& progress)
    {
        if (progress.Cancelled()) { return AZ::Failure(A("Cancelled.")); }
        auto bytes=Read(archive,MaximumArchiveBytes);
        if (!bytes.IsSuccess()) { return AZ::Failure(bytes.GetError()); }
        auto parsed=Parse(bytes.GetValue()); if (!parsed.IsSuccess()) { return AZ::Failure(parsed.GetError()); }
        const auto root=parsed.GetValue();
        if (!Keys(root,{"manifest","manifest_sha256","payloads"}) || !root["manifest"].isObject() || !root["payloads"].isArray())
        { return AZ::Failure(A("Unsupported authoring archive fields.")); }
        const auto manifest=root["manifest"].toObject();
        if (!Keys(manifest,{"format","version","provider","selected_pack","profile","entries"})
            || manifest["format"]!="foa-authoring-package" || manifest["version"]!=1
            || manifest["provider"]!="foa.local-authoring-package/1"
            || !manifest["selected_pack"].isString() || !manifest["profile"].isObject() || !manifest["entries"].isArray()
            || !PortableJson(manifest))
        { return AZ::Failure(A("Unsupported package version, provider or manifest.")); }
        ModPackagePreview result; result.m_manifest=Json(manifest); result.m_inputFingerprint=Hash(bytes.GetValue()); result.m_archiveBytes=bytes.GetValue().size();
        result.m_fingerprint=Hash(result.m_manifest); result.m_selectedPack=manifest["selected_pack"].toString();
        if (root["manifest_sha256"].toString()!=result.m_fingerprint)
        { return AZ::Failure(A("Manifest checksum mismatch.")); }
        const auto inventory=manifest["entries"].toArray(),payloads=root["payloads"].toArray();
        if (inventory.isEmpty() || inventory.size()>MaximumEntries || payloads.size()!=inventory.size())
        { return AZ::Failure(A("Package inventory count is invalid or exceeds 4096 entries.")); }
        QString previous;
        for (int i=0;i<inventory.size();++i)
        {
            if (progress.Cancelled()) { return AZ::Failure(A("Cancelled.")); }
            if (!inventory[i].isObject() || !payloads[i].isObject()) { return AZ::Failure(A("Invalid archive entry.")); }
            const auto info=inventory[i].toObject(),payload=payloads[i].toObject();
            const auto path=info["path"].toString();
            if (!Keys(info,{"path","bytes","sha256"}) || !Keys(payload,{"path","base64"})
                || !info["bytes"].isDouble() || !info["sha256"].isString()
                || !payload["base64"].isString() || payload["path"].toString()!=path
                || !SafePath(path) || (!previous.isEmpty() && path<=previous))
            { return AZ::Failure(A("Invalid, duplicated, unordered or unsafe package entry.")); }
            previous=path;
            const auto count=info["bytes"].toDouble();
            if (count<1 || count>MaximumDecodedBytes || count!=static_cast<qint64>(count))
            { return AZ::Failure(A("Invalid entry length.")); }
            const auto encoded=payload["base64"].toString().toLatin1();
            if (encoded.size()!=4*((static_cast<qint64>(count)+2)/3))
            { return AZ::Failure(A("Invalid base64 entry length.")); }
            const auto decoded=QByteArray::fromBase64(encoded);
            if (decoded.toBase64()!=encoded || decoded.size()!=static_cast<qint64>(count) || Hash(decoded)!=info["sha256"].toString())
            { return AZ::Failure(A("Entry content or checksum mismatch: "+path)); }
            if (path.endsWith(".json"))
            {
                auto json=Parse(decoded);
                if (!json.IsSuccess() || !PortableJson(json.GetValue()))
                { return AZ::Failure(A("Package metadata contains malformed data or private paths: "+path)); }
            }
            auto added=Add(result,path,decoded); if (!added.IsSuccess()) { return AZ::Failure(added.GetError()); }
            if (progress.m_update) { progress.m_update(i+1,QStringLiteral("Verified file %1 of %2").arg(i+1).arg(inventory.size())); }
        }
        auto valid=ValidatePayload(result); if (!valid.IsSuccess()) { return AZ::Failure(valid.GetError()); }
        for (auto it=result.m_entries.begin();it!=result.m_entries.end();++it)
        {
            if (it.key().startsWith("Packs/"))
            {
                auto pack=Deserialize<PackManifest>(it.value());
                result.m_packLabels.append(Q(pack.GetValue().m_displayName)+" "+Q(pack.GetValue().m_version));
            }
        }
        result.m_warnings={"Authoring package only. Review imported content before runtime use."};
        return AZ::Success(AZStd::move(result));
    }
    AZ::Outcome<ModPackageReceipt,AZStd::string> ModPackageService::Import(
        const QString& archive,const QString& expectedFingerprint,const WorkspaceModel& context,
        const QString& destination,const ModPackageProgress& progress)
    {
        const auto target=Direct(destination,false);
        const auto parent=QFileInfo(target).absolutePath();
        if (target.isEmpty() || QFileInfo::exists(target) || Direct(parent,true).isEmpty() || !QFileInfo(parent).isDir()
            || Protected(target,context) || Inside(target,Q(context.m_rootPath))
            || Inside(archive,target) || QFileInfo(target).fileName().startsWith('.'))
        { return AZ::Failure(A("Choose a new workspace folder outside existing workspaces and protected folders.")); }
        auto inspected=Inspect(archive,progress); if (!inspected.IsSuccess()) { return AZ::Failure(inspected.GetError()); }
        const auto& package=inspected.GetValue();
        if (expectedFingerprint.isEmpty() || package.m_fingerprint!=expectedFingerprint)
        { return AZ::Failure(A("The archive changed after inspection. Inspect it again.")); }
        const auto* profile=context.FindActiveGameProfile();
        const auto manifest=QJsonDocument::fromJson(package.m_manifest).object();
        if (!profile || !profile->IsConfigured() || Profile(*profile)!=manifest["profile"].toObject())
        { return AZ::Failure(A("Select the package's matching local game profile before reopening it.")); }
        QTemporaryDir temporary(parent+"/.foa-import-XXXXXX");
        if (!temporary.isValid() || Direct(temporary.path(),true).isEmpty())
        { return AZ::Failure(A("Could not create a private import staging folder.")); }
        const auto stage=temporary.path();
        int index=0;
        for (auto it=package.m_entries.begin();it!=package.m_entries.end();++it)
        {
            if (progress.Cancelled()) { return AZ::Failure(A("Cancelled. The destination was not created.")); }
            const auto path=QDir(stage).filePath(it.key());
            if (!Inside(path,stage) || !QDir().mkpath(QFileInfo(path).absolutePath()) || Direct(path,false).isEmpty())
            { return AZ::Failure(A("Cannot create a safe package entry folder.")); }
            auto written=WriteNew(path,it.value(),progress); if (!written.IsSuccess()) { return AZ::Failure(written.GetError()); }
            if (progress.m_update) { progress.m_update(++index,QStringLiteral("Restored file %1").arg(index)); }
        }
        WorkspaceModel workspace;
        workspace.m_workspaceId="import."+A(package.m_fingerprint.mid(7,32));
        workspace.m_displayName="Imported "+A(package.m_selectedPack);
        workspace.m_rootPath=A(target); workspace.m_outputPath=A(target+"/Build");
        workspace.m_stagingPath=A(target+"/Staging"); workspace.m_deploymentPath=A(target+"/Deployment");
        workspace.m_activeGameProfileId=profile->m_profileId; workspace.m_gameProfiles={*profile};
        // Resolve any caller-local relative profile paths before moving context.
        auto& local=workspace.m_gameProfiles.front();
        for (auto* path:{&local.m_installPath,&local.m_managedAssembliesPath,&local.m_pluginPath})
        { if (!path->empty() && !QDir::isAbsolutePath(Q(*path))) { *path=A(QDir(Q(context.m_rootPath)).absoluteFilePath(Q(*path))); } }
        local.m_diagnosticsPath="Diagnostics"; local.m_extractedDataPath="Extracted";
        for (const auto* folder:{"Build","Staging","Deployment","Diagnostics","Extracted"})
        { if (!QDir().mkpath(QDir(stage).filePath(folder))) { return AZ::Failure(A("Cannot create workspace output folders.")); } }
        auto catalog=Deserialize<CatalogDocument>(package.m_entries.value("Catalog/catalog.tgcatalog.json"));
        catalog.GetValue().m_workspaceId=workspace.m_workspaceId;
        const auto savedCatalog=CatalogPersistenceService{}.Save(catalog.GetValue(),A(stage));
        if (!savedCatalog.IsSuccess()) { return AZ::Failure(savedCatalog.GetError()); }
        const auto workspacePath=stage+"/workspace.tgworkspace.json";
        auto savedWorkspace=WorkspacePersistenceService{}.Save(workspace,A(workspacePath));
        if (!savedWorkspace.IsSuccess()) { return AZ::Failure(savedWorkspace.GetError()); }
        auto reopened=WorkspacePersistenceService{}.Load(A(workspacePath));
        auto reopenedCatalog=CatalogPersistenceService{}.Load(A(stage));
        if (!reopened.IsSuccess() || !reopenedCatalog.IsSuccess()
            || reopenedCatalog.GetValue().m_workspaceId!=workspace.m_workspaceId)
        { return AZ::Failure(A("Reconstructed workspace did not pass persistence verification.")); }
        // Validate the staged storage and rebind every persisted source/evidence/catalog document
        // before publishing. Only the owned workspace/output roots differ after the rename.
        auto stagedWorkspace=workspace; stagedWorkspace.m_rootPath=A(stage);
        stagedWorkspace.m_outputPath="Build"; stagedWorkspace.m_stagingPath="Staging"; stagedWorkspace.m_deploymentPath="Deployment";
        auto paths=PathPolicyService{}.ValidateWorkspacePaths(stagedWorkspace,A(stage));
        if (!paths.IsSuccess()) { return AZ::Failure(paths.GetError()); }
        AZStd::vector<SourceDocument> sources; AZStd::vector<EvidenceDocument> evidence;
        AZStd::vector<ImportIssue> issues; SourceEvidenceRegistry registry; AZStd::string error;
        auto loaded=SourceEvidencePersistenceService{}.LoadWorkspaceDocuments(A(stage),sources,evidence,issues);
        if (!loaded.IsSuccess() || !issues.empty()) { return AZ::Failure(A("Reconstructed source documents did not pass reload validation.")); }
        for (const auto& source:sources)
        { if (!registry.RegisterSource(source.m_source,&error)) { return AZ::Failure(error); } }
        for (const auto& document:evidence)
        { for (const auto& entry:document.m_evidence) { if (!registry.RegisterEvidence(entry,&error)) { return AZ::Failure(error); } } }
        CatalogDatabase verified;
        if (!verified.ReplaceFromBoundDocument(reopenedCatalog.GetValue(),workspace,*workspace.FindActiveGameProfile(),registry,&error))
        { return AZ::Failure(error); }
        if (progress.Cancelled() || QFileInfo::exists(target) || Direct(target,false).isEmpty()
            || Direct(stage,true).isEmpty() || Protected(target,context))
        { return AZ::Failure(A("Cancelled or destination changed. Existing files were preserved.")); }
        if (!QDir().rename(stage,target))
        { return AZ::Failure(A("Could not publish the new workspace without replacing existing content.")); }
        temporary.setAutoRemove(false);
        ModPackageReceipt receipt; receipt.m_path=target+"/workspace.tgworkspace.json";
        receipt.m_fingerprint=package.m_inputFingerprint; receipt.m_bytes=package.m_archiveBytes;
        return AZ::Success(AZStd::move(receipt));
    }
}
