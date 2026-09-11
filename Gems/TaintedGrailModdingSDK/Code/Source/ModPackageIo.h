/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once
#include "ModPackageService.h"
#include "AssetLocalisationService.h"
#include "PathPolicyService.h"
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Serialization/Json/JsonUtils.h>
#include <AzCore/IO/ByteContainerStream.h>
#include <AzCore/Utils/Utils.h>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QTemporaryFile>
namespace TaintedGrailModdingSDK::ModPackageIo
{
    inline QString Q(const AZStd::string& s) { return QString::fromUtf8(s.data(), static_cast<int>(s.size())); }
    inline AZStd::string A(const QString& s) { const auto b=s.toUtf8(); return {b.constData(), static_cast<size_t>(b.size())}; }
    inline QString Hash(const QByteArray& b)
    { return "sha256:" + QString::fromLatin1(QCryptographicHash::hash(b, QCryptographicHash::Sha256).toHex()); }
    inline QByteArray Json(const QJsonObject& o) { return QJsonDocument(o).toJson(QJsonDocument::Compact); }
    inline bool SafePath(const QString& p)
    {
        if (p.isEmpty() || p.size()>240 || p.startsWith('/') || p.contains('\\') || p.contains(':')) { return false; }
        const QRegularExpression component(QStringLiteral("^[A-Za-z0-9_-][A-Za-z0-9_.-]*$"));
        const QRegularExpression device(QStringLiteral("^(con|prn|aux|nul|com[0-9]|lpt[0-9])($|\\.)"), QRegularExpression::CaseInsensitiveOption);
        for (const auto& part : p.split('/'))
        {
            if (!component.match(part).hasMatch() || part.endsWith('.') || device.match(part).hasMatch()) { return false; }
        }
        return true;
    }
    inline QString Direct(const QString& input, bool mustExist)
    {
        if (!QDir::isAbsolutePath(input) || input.contains(QChar(0))) { return {}; }
#ifdef Q_OS_WIN
        // Reject alternate streams and Win32 aliases even when the final path does not exist.
        const auto normalized=QDir::fromNativeSeparators(input);
        if (normalized.mid(2).contains(':')) { return {}; }
        const QRegularExpression device(QStringLiteral("^(con|prn|aux|nul|com[0-9]|lpt[0-9])($|\\.)"),QRegularExpression::CaseInsensitiveOption);
        for (const auto& part:normalized.mid(3).split('/'))
        { if (part.endsWith(' ') || (part!="." && part!=".." && part.endsWith('.')) || device.match(part).hasMatch()) { return {}; } }
#endif
        const auto path=QDir::cleanPath(input); auto probe=path; QStringList suffix;
        while (!QFileInfo::exists(probe))
        {
            if (QFileInfo(probe).isSymLink()) { return {}; }
            const QFileInfo info(probe); suffix.prepend(info.fileName());
            if (info.absolutePath()==probe) { return {}; } probe=info.absolutePath();
        }
        if (mustExist && !suffix.isEmpty()) { return {}; }
        const QFileInfo info(probe);
        if (info.isSymLink()) { return {}; }
#ifdef Q_OS_WIN
        constexpr auto sensitivity=Qt::CaseInsensitive;
#else
        constexpr auto sensitivity=Qt::CaseSensitive;
#endif
        if (info.canonicalFilePath().compare(info.absoluteFilePath(), sensitivity)!=0) { return {}; }
        return path;
    }
    inline bool Inside(const QString& path, const QString& root)
    {
        if (root.isEmpty()) { return false; }
        return PathPolicyService::IsCanonicalPathContained(A(QDir::cleanPath(root)), A(QDir::cleanPath(path)),
#ifdef Q_OS_WIN
            true
#else
            false
#endif
        );
    }
    inline bool Protected(const QString& path, const WorkspaceModel& workspace)
    {
        const auto engine=AZ::Utils::GetEnginePath();
        if (!engine.empty() && Inside(path, QString::fromUtf8(engine.c_str()))) { return true; }
        if (Inside(path, QDir::home().filePath("Saved Games"))) { return true; }
        for (const auto& p : workspace.m_gameProfiles)
        {
            for (const auto* base : {&p.m_installPath,&p.m_managedAssembliesPath,&p.m_pluginPath,&p.m_extractedDataPath})
            {
                if (!base->empty() && Inside(path, QDir(Q(workspace.m_rootPath)).absoluteFilePath(Q(*base)))) { return true; }
            }
        }
        return false;
    }
    inline AZ::Outcome<QByteArray, AZStd::string> Read(const QString& path, qint64 maximum)
    {
        if (Direct(path,true).isEmpty()) { return AZ::Failure(A("File is missing or uses a linked path: "+path)); }
        QFile file(path);
        if (!QFileInfo(file).isFile() || !file.open(QIODevice::ReadOnly) || file.size()<1 || file.size()>maximum)
        { return AZ::Failure(A("Cannot read file within the package size limit: "+path)); }
        const auto bytes=file.read(maximum+1);
        if (bytes.size()!=file.size() || bytes.size()>maximum || file.error()!=QFile::NoError)
        { return AZ::Failure(A("File changed or could not be fully read: "+path)); }
        return AZ::Success(bytes);
    }
    inline bool PortableJson(const QJsonValue& value, int depth=0)
    {
        if (depth>64) { return false; }
        if (value.isString())
        {
            const auto s=value.toString();
            static const QRegularExpression path(QStringLiteral("(^|[\\s\"'=])([A-Za-z]:[\\\\/]|\\\\\\\\|/(home|Users|mnt|tmp)/)"));
            if (s.contains(QChar(0)) || path.match(s).hasMatch()) { return false; }
        }
        if (value.isObject()) { const auto o=value.toObject(); for (auto it=o.begin();it!=o.end();++it) { if (!PortableJson(it.value(),depth+1)) { return false; } } }
        if (value.isArray()) { for (const auto v:value.toArray()) { if (!PortableJson(v,depth+1)) { return false; } } }
        return true;
    }
    inline bool UniqueJson(const rapidjson::Value& v, int depth=0)
    {
        if (depth>64) { return false; }
        if (v.IsObject())
        {
            QSet<QByteArray> keys;
            for (auto it=v.MemberBegin();it!=v.MemberEnd();++it)
            {
                QByteArray key(it->name.GetString(),static_cast<int>(it->name.GetStringLength()));
                if (keys.contains(key) || !UniqueJson(it->value,depth+1)) { return false; } keys.insert(key);
            }
        }
        if (v.IsArray()) { for (const auto& x:v.GetArray()) { if (!UniqueJson(x,depth+1)) { return false; } } }
        return true;
    }
    inline AZ::Outcome<QJsonObject,AZStd::string> Parse(const QByteArray& bytes)
    {
        // Iterative parsing bounds parser stack use before the explicit depth check.
        rapidjson::Document raw;
        raw.Parse<rapidjson::kParseValidateEncodingFlag | rapidjson::kParseIterativeFlag>(bytes.constData(),bytes.size());
        if (raw.HasParseError() || !raw.IsObject() || !UniqueJson(raw))
        { return AZ::Failure(A("Package JSON is malformed, duplicated, too deeply nested or not valid UTF-8.")); }
        const auto doc=QJsonDocument::fromJson(bytes);
        if (!doc.isObject()) { return AZ::Failure(A("Package JSON must be an object.")); }
        return AZ::Success(doc.object());
    }
    template<class T> AZ::Outcome<QByteArray,AZStd::string> Serialize(const T& object)
    {
        AZStd::string bytes; AZ::IO::ByteContainerStream<AZStd::string> stream(&bytes);
        const auto saved=AZ::JsonSerializationUtils::SaveObjectToStream(&object,stream);
        if (!saved.IsSuccess()) { return AZ::Failure(AZStd::string(saved.GetError())); }
        auto parsed=Parse(QByteArray(bytes.data(),static_cast<int>(bytes.size())));
        if (!parsed.IsSuccess()) { return AZ::Failure(parsed.GetError()); }
        return AZ::Success(Json(parsed.GetValue()));
    }
    template<class T> AZ::Outcome<T,AZStd::string> Deserialize(const QByteArray& bytes)
    {
        auto parsed=Parse(bytes); if (!parsed.IsSuccess()) { return AZ::Failure(parsed.GetError()); }
        rapidjson::Document raw; raw.Parse(bytes.constData(),bytes.size());
        if (!raw.HasMember("ClassData") || !raw["ClassData"].IsObject())
        { return AZ::Failure(A("Package object requires its typed serialization envelope.")); }
        AZ::JsonDeserializerSettings settings;
        AZ::ComponentApplicationBus::BroadcastResult(settings.m_serializeContext,&AZ::ComponentApplicationBus::Events::GetSerializeContext);
        AZ::ComponentApplicationBus::BroadcastResult(settings.m_registrationContext,&AZ::ComponentApplicationBus::Events::GetJsonRegistrationContext);
        settings.m_clearContainers=true;
        if (!settings.m_serializeContext || !settings.m_registrationContext) { return AZ::Failure(A("Serialization is unavailable.")); }
        T object;
        const auto loaded=AZ::JsonSerialization::Load(object,raw["ClassData"],settings);
        if (loaded.GetProcessing()!=AZ::JsonSerializationResult::Processing::Completed)
        { return AZ::Failure(A("The package object cannot be loaded by this Editor.")); }
        auto canonical=Serialize(object);
        if (!canonical.IsSuccess() || canonical.GetValue()!=Json(parsed.GetValue()))
        { return AZ::Failure(A("The package object has unknown, noncanonical or unsupported fields.")); }
        return AZ::Success(AZStd::move(object));
    }
    inline AZ::Outcome<void,AZStd::string> Add(ModPackagePreview& result,const QString& path,const QByteArray& bytes)
    {
        if (!SafePath(path) || bytes.isEmpty() || result.m_entries.size()>=ModPackageService::MaximumEntries)
        { return AZ::Failure(A("Invalid or excessive package entries: "+path)); }
        for (auto it=result.m_entries.begin();it!=result.m_entries.end();++it)
        {
            if (it.key().compare(path,Qt::CaseInsensitive)==0)
            {
                if (it.key()==path && it.value()==bytes) { return AZ::Success(); }
                return AZ::Failure(A("Conflicting package paths: "+path));
            }
        }
        if (bytes.size()>ModPackageService::MaximumDecodedBytes-result.m_totalBytes)
        { return AZ::Failure(A("Package exceeds the 64 MiB decoded content limit.")); }
        result.m_entries.insert(path,bytes); result.m_totalBytes+=bytes.size(); return AZ::Success();
    }
    inline bool Keys(const QJsonObject& o,QStringList keys) { auto actual=o.keys(); keys.sort(); return actual==keys; }
    inline QJsonObject Profile(const GameProfile& p)
    {
        return {{"id",Q(p.m_profileId)},{"version",Q(p.m_gameVersion)},{"branch",Q(p.m_branch)},
            {"runtime",Q(p.m_runtimeTarget)},{"unity",Q(p.m_unityVersion)},{"bepinex",Q(p.m_bepInExVersion)}};
    }
    inline WorkspaceModel VirtualWorkspace(const QJsonObject& p)
    {
        WorkspaceModel w; w.m_workspaceId="package.workspace"; w.m_displayName="Imported authoring package"; w.m_rootPath=".";
        GameProfile g; g.m_profileId=A(p["id"].toString()); g.m_displayName="Package target";
        g.m_gameVersion=A(p["version"].toString()); g.m_branch=A(p["branch"].toString()); g.m_runtimeTarget=A(p["runtime"].toString());
        g.m_unityVersion=A(p["unity"].toString()); g.m_bepInExVersion=A(p["bepinex"].toString());
        g.m_installPath="local-profile-required"; g.m_managedAssembliesPath="local-profile-required/Managed"; if (g.m_runtimeTarget=="Mono") { g.m_pluginPath="local-profile-required/plugins"; }
        w.m_activeGameProfileId=g.m_profileId; w.m_gameProfiles={g}; return w;
    }
    AZ::Outcome<void,AZStd::string> ValidatePayload(const ModPackagePreview&);
    AZ::Outcome<void,AZStd::string> WriteNew(const QString&,const QByteArray&,const ModPackageProgress& = {});
    AZ::Outcome<QString,AZStd::string> InputFingerprint(const ModPackageContext&);
}
