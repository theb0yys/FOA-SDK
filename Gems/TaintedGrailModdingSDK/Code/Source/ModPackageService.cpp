/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "ModPackageIo.h"
#include "PackPersistenceService.h"
#include "ProjectImageService.h"
#include <QBuffer>
#include <QImageReader>
#include <QDirIterator>
namespace TaintedGrailModdingSDK
{
    using namespace ModPackageIo;
    namespace
    {
        AZ::Outcome<AZStd::vector<PackManifest>,AZStd::string> ReadPacks(const ModPackageContext& context)
        {
            const auto root=Q(context.m_root);
            const QString path=QDir(root).filePath("Packs");
            if (Direct(path,true).isEmpty()) { return AZ::Failure(A("The workspace Packs folder is missing or linked.")); }
            AZStd::vector<PackManifest> packs;
            QDirIterator it(path,QDir::Dirs|QDir::NoDotAndDotDot);
            while (it.hasNext())
            {
                const auto directory=it.next();
                if (packs.size()>=512) { return AZ::Failure(A("The workspace exceeds 512 pack folders.")); }
                const auto file=QDir(directory).filePath("pack.tgpack.json");
                const auto bytes=Read(file,1024*1024);
                if (!bytes.IsSuccess()) { return AZ::Failure(bytes.GetError()); }
                auto pack=PackPersistenceService{}.Load(A(file));
                if (!pack.IsSuccess()) { return AZ::Failure(pack.GetError()); }
                if (QFileInfo(directory).fileName()!=Q(pack.GetValue().m_packId))
                { return AZ::Failure(A("Pack folders must match their exact pack IDs: "+directory)); }
                packs.push_back(pack.TakeValue());
            }
            return AZ::Success(AZStd::move(packs));
        }
        template<class T> AZ::Outcome<void,AZStd::string> AddObject(ModPackagePreview& p,const QString& path,const T& object)
        {
            auto bytes=Serialize(object); if (!bytes.IsSuccess()) { return AZ::Failure(bytes.GetError()); }
            auto json=Parse(bytes.GetValue());
            if (!json.IsSuccess() || !PortableJson(json.GetValue()))
            { return AZ::Failure(A("Remove private machine paths or unsupported metadata from "+path)); }
            return Add(p,path,bytes.GetValue());
        }
        AZ::Outcome<void,AZStd::string> CheckImage(const ProjectAssetProfile& asset,const QByteArray& bytes)
        {
            if (bytes.size()!=static_cast<qint64>(asset.m_byteSize) || Hash(bytes)!=Q(asset.m_fingerprint))
            { return AZ::Failure(A("Image size or checksum mismatch: "+Q(asset.m_sourcePath))); }
            QBuffer buffer; buffer.setData(bytes); buffer.open(QIODevice::ReadOnly);
            QImageReader reader(&buffer); reader.setDecideFormatFromContent(true);
            const auto size=reader.size(); const auto type=reader.format();
            if ((type!="png" && type!="jpeg") || !size.isValid()
                || size.width()!=static_cast<int>(asset.m_width) || size.height()!=static_cast<int>(asset.m_height)
                || reader.imageCount()>1 || reader.read().isNull()
                || Q(asset.m_mediaType)!=(type=="png" ? "image/png" : "image/jpeg"))
            { return AZ::Failure(A("Image decoding or declared format mismatch: "+Q(asset.m_sourcePath))); }
            return AZ::Success();
        }
    }
    namespace ModPackageIo
    {
        AZ::Outcome<QString,AZStd::string> InputFingerprint(const ModPackageContext& c)
        {
            auto catalog=Read(QDir(Q(c.m_root)).filePath("Catalog/catalog.tgcatalog.json"),ModPackageService::MaximumDecodedBytes);
            if (!catalog.IsSuccess()) { return AZ::Failure(catalog.GetError()); }
            const auto* profile=c.m_workspace.FindActiveGameProfile();
            if (!profile) { return AZ::Failure(A("Select a game profile.")); }
            auto packs=ReadPacks(c); if (!packs.IsSuccess()) { return AZ::Failure(packs.GetError()); }
            QCryptographicHash hash(QCryptographicHash::Sha256);
            hash.addData(catalog.GetValue()); hash.addData(Json(Profile(*profile)));
            for (const auto& p:packs.GetValue())
            {
                auto data=Serialize(p); if (!data.IsSuccess()) { return AZ::Failure(data.GetError()); }
                hash.addData(data.GetValue());
            }
            return AZ::Success(QStringLiteral("sha256:")+QString::fromLatin1(hash.result().toHex()));
        }
        AZ::Outcome<void,AZStd::string> ValidatePayload(const ModPackagePreview& p)
        {
            auto manifest=Parse(p.m_manifest); if (!manifest.IsSuccess()) { return AZ::Failure(manifest.GetError()); }
            const auto profile=manifest.GetValue()["profile"].toObject();
            if (!Keys(profile,{"id","version","branch","runtime","unity","bepinex"}))
            { return AZ::Failure(A("Unsupported package profile fields.")); }
            auto workspace=VirtualWorkspace(profile);
            const auto* game=workspace.FindActiveGameProfile();
            if (!game || !game->IsConfigured()) { return AZ::Failure(A("The package target profile is invalid.")); }
            auto catalog=Deserialize<CatalogDocument>(p.m_entries.value("Catalog/catalog.tgcatalog.json"));
            if (!catalog.IsSuccess()) { return AZ::Failure(catalog.GetError()); }
            if (catalog.GetValue().m_schemaVersion!=AssetLocalisationCatalogSchemaVersion
                || catalog.GetValue().m_workspaceId!="package.workspace")
            { return AZ::Failure(A("This package requires the exact supported catalog schema and portable workspace binding.")); }
            AZStd::vector<PackManifest> packs; SourceEvidenceRegistry registry;
            AZStd::vector<EvidenceDocument> evidence;
            QSet<QString> expected{"Catalog/catalog.tgcatalog.json"};
            AZStd::string error;
            for (auto it=p.m_entries.begin();it!=p.m_entries.end();++it)
            {
                if (it.key().startsWith("Packs/"))
                {
                    auto pack=Deserialize<PackManifest>(it.value());
                    if (!pack.IsSuccess()) { return AZ::Failure(pack.GetError()); }
                    const auto path="Packs/"+Q(pack.GetValue().m_packId)+"/pack.tgpack.json";
                    if (it.key()!=path) { return AZ::Failure(A("Pack manifest path and identity differ.")); }
                    expected.insert(path); packs.push_back(pack.TakeValue());
                }
                if (it.key().endsWith("/source.tgsource.json"))
                {
                    auto doc=Deserialize<SourceDocument>(it.value());
                    if (!doc.IsSuccess()) { return AZ::Failure(doc.GetError()); }
                    const auto path="Sources/"+Q(doc.GetValue().m_source.m_sourceId)+"/source.tgsource.json";
                    if (path!=it.key() || !doc.GetValue().UsesSupportedSchema() || !doc.GetValue().m_issues.empty()
                        || !registry.RegisterSource(doc.GetValue().m_source,&error))
                    { return AZ::Failure(A("Invalid package source metadata. ")+error); }
                    expected.insert(path);
                }
                if (it.key().endsWith("/evidence.tgevidence.json"))
                {
                    auto doc=Deserialize<EvidenceDocument>(it.value());
                    if (!doc.IsSuccess()) { return AZ::Failure(doc.GetError()); }
                    const auto path="Sources/"+Q(doc.GetValue().m_sourceId)+"/evidence.tgevidence.json";
                    if (path!=it.key() || !doc.GetValue().UsesSupportedSchema() || !doc.GetValue().m_issues.empty())
                    { return AZ::Failure(A("Invalid package evidence metadata.")); }
                    expected.insert(path); evidence.push_back(doc.TakeValue());
                }
            }
            for (const auto& doc:evidence)
            {
                const auto* source=registry.FindSource(doc.m_sourceId);
                if (!source || doc.m_sourceFingerprint!=source->m_fingerprint || doc.m_profileId!=source->m_profileId
                    || doc.m_gameVersion!=source->m_gameVersion || doc.m_branch!=source->m_branch)
                { return AZ::Failure(A("Package evidence headers do not match their source.")); }
                for (const auto& e:doc.m_evidence)
                {
                    if (e.m_sourceId!=doc.m_sourceId || !registry.RegisterEvidence(e,&error))
                    { return AZ::Failure(A("Package evidence is inconsistent. ")+error); }
                }
            }
            CatalogDatabase database;
            if (!database.ReplaceFromBoundDocument(catalog.GetValue(),workspace,*game,registry,&error))
            { return AZ::Failure(A("Package definitions failed validation. ")+error); }
            auto selected=ModPackagePlan::Select(workspace,packs,A(p.m_selectedPack),database,registry);
            if (!selected.IsSuccess()) { return AZ::Failure(selected.GetError()); }
            auto selectedCatalog=Serialize(selected.GetValue().m_catalog);
            if (!selectedCatalog.IsSuccess() || selectedCatalog.GetValue()!=p.m_entries.value("Catalog/catalog.tgcatalog.json")
                || selected.GetValue().m_packs.size()!=packs.size()
                || selected.GetValue().m_evidence.GetEvidence().size()!=registry.GetEvidence().size()
                || selected.GetValue().m_evidence.GetSources().size()!=registry.GetSources().size())
            { return AZ::Failure(A("Package contains unowned, unused, noncanonical or permission-bearing content.")); }
            for (const auto& source:selected.GetValue().m_evidence.GetSources())
            {
                SourceDocument sd; sd.m_source=source;
                EvidenceDocument ed; ed.m_sourceId=source.m_sourceId; ed.m_sourceFingerprint=source.m_fingerprint;
                ed.m_profileId=source.m_profileId; ed.m_gameVersion=source.m_gameVersion; ed.m_branch=source.m_branch;
                ed.m_evidence=selected.GetValue().m_evidence.FindEvidenceForSource(source.m_sourceId);
                auto sb=Serialize(sd),eb=Serialize(ed);
                const auto base="Sources/"+Q(source.m_sourceId)+"/";
                if (!sb.IsSuccess() || !eb.IsSuccess() || sb.GetValue()!=p.m_entries.value(base+"source.tgsource.json")
                    || eb.GetValue()!=p.m_entries.value(base+"evidence.tgevidence.json"))
                { return AZ::Failure(A("Source/evidence metadata is not the portable, unprivileged projection.")); }
            }
            for (const auto& asset:selected.GetValue().m_catalog.m_projectAssets)
            {
                if (asset.m_redistribution!="declared_permitted")
                { return AZ::Failure(A("Review redistribution in Assets and text before packaging "+Q(asset.m_recordId))); }
                const auto path=Q(asset.m_sourcePath);
                auto checked=CheckImage(asset,p.m_entries.value(path));
                if (!checked.IsSuccess()) { return checked; } expected.insert(path);
            }
            for (const auto& actor:selected.GetValue().m_catalog.m_actorProfiles)
            {
                if (actor.m_portraitAssetRef.empty()) { continue; }
                const auto ref=Q(actor.m_portraitAssetRef);
                if (!ref.startsWith("$workspace/") || !expected.contains(ref.mid(11)))
                { return AZ::Failure(A("Move this actor portrait into the Asset Manager and review redistribution: "+Q(actor.m_recordId))); }
            }
            for (const auto& item:selected.GetValue().m_catalog.m_economyItems)
            {
                for (const auto* reference:{&item.m_iconRef,&item.m_assetRef})
                {
                    const auto value=Q(*reference);
                    if (value.startsWith("$workspace/") && !expected.contains(value.mid(11)))
                    { return AZ::Failure(A("Missing local item visual; register it in Assets and text: "+Q(item.m_recordId))); }
                }
            }
            for (const auto& pack:packs)
            {
                for (const auto* paths:{&pack.m_contentDefinitionPaths,&pack.m_assetPaths,&pack.m_localisationPaths})
                {
                    for (const auto& path:*paths)
                    {
                        if (!expected.contains(Q(path)))
                        { return AZ::Failure(A("Manifest declaration is missing or unsupported; use managed authoring data: "+Q(path))); }
                    }
                }
            }
            if (expected.size()!=p.m_entries.size()) { return AZ::Failure(A("The archive contains unsupported or unreferenced files.")); }
            return AZ::Success();
        }
        AZ::Outcome<void,AZStd::string> WriteNew(const QString& path,const QByteArray& bytes,const ModPackageProgress& progress)
        {
            if (Direct(path,false).isEmpty() || QFileInfo::exists(path) || !QFileInfo(QFileInfo(path).absolutePath()).isDir())
            { return AZ::Failure(A("Choose a new output name in an existing, unlinked folder.")); }
            QTemporaryFile temp(QFileInfo(path).absolutePath()+"/.foa-package-XXXXXX");
            if (!temp.open() || temp.write(bytes)!=bytes.size() || !temp.flush())
            { return AZ::Failure(A("Could not write the temporary package. Existing files were preserved.")); }
            temp.close();
            auto verified=Read(temp.fileName(),ModPackageService::MaximumArchiveBytes);
            if (!verified.IsSuccess() || verified.GetValue()!=bytes || Direct(path,false).isEmpty() || QFileInfo::exists(path))
            { return AZ::Failure(A("Package output verification failed or the destination changed.")); }
            if (progress.Cancelled()) { return AZ::Failure(A("Cancelled. Existing files were preserved.")); }
            if (!temp.rename(path)) { return AZ::Failure(A("Could not publish the package without replacing an existing file.")); }
            temp.setAutoRemove(false); return AZ::Success();
        }
    }
    AZ::Outcome<ModPackagePreview,AZStd::string> ModPackageService::Preview(
        const ModPackageContext& context,const ModPackageProgress& progress)
    {
        if (progress.Cancelled()) { return AZ::Failure(A("Cancelled.")); }
        const auto root=Direct(Q(context.m_root),true);
        if (root.isEmpty() || Protected(root,context.m_workspace))
        { return AZ::Failure(A("Choose a local authoring workspace outside protected folders.")); }
        auto before=InputFingerprint(context); if (!before.IsSuccess()) { return AZ::Failure(before.GetError()); }
        auto packs=ReadPacks(context); if (!packs.IsSuccess()) { return AZ::Failure(packs.GetError()); }
        auto selection=ModPackagePlan::Select(context.m_workspace,packs.GetValue(),context.m_selectedPack,context.m_catalog,context.m_evidence);
        if (!selection.IsSuccess()) { return AZ::Failure(selection.GetError()); }
        auto& chosen=selection.GetValue();
        ModPackagePreview result; result.m_selectedPack=Q(context.m_selectedPack); result.m_inputFingerprint=before.GetValue();
        for (const auto& warning:chosen.m_warnings) { result.m_warnings.append(Q(warning)); }
        chosen.m_catalog.m_workspaceId="package.workspace";
        auto added=AddObject(result,"Catalog/catalog.tgcatalog.json",chosen.m_catalog);
        if (!added.IsSuccess()) { return AZ::Failure(added.GetError()); }
        for (const auto& pack:chosen.m_packs)
        {
            added=AddObject(result,"Packs/"+Q(pack.m_packId)+"/pack.tgpack.json",pack);
            if (!added.IsSuccess()) { return AZ::Failure(added.GetError()); }
            result.m_packLabels.append(Q(pack.m_displayName)+" "+Q(pack.m_version));
        }
        for (const auto& source:chosen.m_evidence.GetSources())
        {
            SourceDocument s; s.m_source=source;
            EvidenceDocument e; e.m_sourceId=source.m_sourceId; e.m_sourceFingerprint=source.m_fingerprint;
            e.m_profileId=source.m_profileId; e.m_gameVersion=source.m_gameVersion; e.m_branch=source.m_branch;
            e.m_evidence=chosen.m_evidence.FindEvidenceForSource(source.m_sourceId);
            const auto base="Sources/"+Q(source.m_sourceId)+"/";
            added=AddObject(result,base+"source.tgsource.json",s); if (!added.IsSuccess()) { return AZ::Failure(added.GetError()); }
            added=AddObject(result,base+"evidence.tgevidence.json",e); if (!added.IsSuccess()) { return AZ::Failure(added.GetError()); }
        }
        int index=0;
        for (const auto& asset:chosen.m_catalog.m_projectAssets)
        {
            if (progress.Cancelled()) { return AZ::Failure(A("Cancelled.")); }
            if (asset.m_redistribution!="declared_permitted")
            { return AZ::Failure(A("Review image redistribution in Assets and text: "+Q(asset.m_recordId))); }
            const auto* record=context.m_catalog.FindByRecordId(asset.m_recordId);
            auto image=ProjectImageService::ReadManaged(asset,record->m_ownerPackId,context.m_workspace,context.m_root);
            if (!image.IsSuccess()) { return AZ::Failure(image.GetError()); }
            added=Add(result,Q(asset.m_sourcePath),image.GetValue().m_bytes);
            if (!added.IsSuccess()) { return AZ::Failure(added.GetError()); }
            if (progress.m_update) { progress.m_update(++index,QStringLiteral("Verified image %1").arg(index)); }
        }
        QJsonArray entries;
        for (auto it=result.m_entries.begin();it!=result.m_entries.end();++it)
        {
            entries.append(QJsonObject{{"path",it.key()},{"bytes",it.value().size()},{"sha256",Hash(it.value())}});
        }
        result.m_manifest=Json({{"format","foa-authoring-package"},{"version",1},{"provider","foa.local-authoring-package/1"},
            {"selected_pack",result.m_selectedPack},{"profile",Profile(*context.m_workspace.FindActiveGameProfile())},{"entries",entries}});
        result.m_fingerprint=Hash(result.m_manifest);
        auto valid=ValidatePayload(result); if (!valid.IsSuccess()) { return AZ::Failure(valid.GetError()); }
        auto after=InputFingerprint(context);
        if (!after.IsSuccess() || after.GetValue()!=result.m_inputFingerprint)
        { return AZ::Failure(A("Workspace files changed during preview. Refresh the preview.")); }
        if (progress.Cancelled()) { return AZ::Failure(A("Cancelled.")); }
        return AZ::Success(AZStd::move(result));
    }
    AZ::Outcome<ModPackageReceipt,AZStd::string> ModPackageService::Export(
        const ModPackageContext& context,const ModPackagePreview& preview,const QString& output,const ModPackageProgress& progress)
    {
        if (Direct(output,false).isEmpty() || Protected(output,context.m_workspace)
            || Inside(output,Q(context.m_root)) || !output.endsWith(".tgmod",Qt::CaseInsensitive))
        { return AZ::Failure(A("Choose a new .tgmod file outside the workspace and protected folders.")); }
        auto fresh=Preview(context,progress); if (!fresh.IsSuccess()) { return AZ::Failure(fresh.GetError()); }
        if (fresh.GetValue().m_fingerprint!=preview.m_fingerprint || fresh.GetValue().m_inputFingerprint!=preview.m_inputFingerprint
            || fresh.GetValue().m_manifest!=preview.m_manifest || fresh.GetValue().m_entries!=preview.m_entries)
        { return AZ::Failure(A("Content changed after preview. Refresh and review the package again.")); }
        QJsonArray payloads;
        for (auto it=preview.m_entries.begin();it!=preview.m_entries.end();++it)
        {
            if (progress.Cancelled()) { return AZ::Failure(A("Cancelled.")); }
            payloads.append(QJsonObject{{"path",it.key()},{"base64",QString::fromLatin1(it.value().toBase64())}});
        }
        const auto bytes=Json({{"manifest",QJsonDocument::fromJson(preview.m_manifest).object()},
            {"manifest_sha256",preview.m_fingerprint},{"payloads",payloads}});
        if (bytes.size()>MaximumArchiveBytes) { return AZ::Failure(A("Archive exceeds the 96 MiB limit.")); }
        auto current=InputFingerprint(context);
        if (!current.IsSuccess() || current.GetValue()!=preview.m_inputFingerprint || progress.Cancelled())
        { return AZ::Failure(A("Cancelled or workspace changed during export. Refresh the preview.")); }
        auto saved=WriteNew(output,bytes,progress); if (!saved.IsSuccess()) { return AZ::Failure(saved.GetError()); }
        ModPackageReceipt receipt; receipt.m_path=output; receipt.m_fingerprint=Hash(bytes); receipt.m_bytes=bytes.size();
        return AZ::Success(AZStd::move(receipt));
    }
}
