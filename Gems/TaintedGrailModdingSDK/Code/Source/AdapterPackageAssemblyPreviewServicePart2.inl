namespace TaintedGrailModdingSDK
{
    namespace
    {
        void AppendEscapedJsonString(
            AZStd::string& json,
            const AZStd::string& value)
        {
            json.push_back('"');
            for (char character : value)
            {
                switch (character)
                {
                case '"':
                    json += "\\\"";
                    break;
                case '\\':
                    json += "\\\\";
                    break;
                case '\b':
                    json += "\\b";
                    break;
                case '\f':
                    json += "\\f";
                    break;
                case '\n':
                    json += "\\n";
                    break;
                case '\r':
                    json += "\\r";
                    break;
                case '\t':
                    json += "\\t";
                    break;
                default:
                    if (static_cast<unsigned char>(character) < 0x20)
                    {
                        static constexpr char Hex[] = "0123456789abcdef";
                        json += "\\u00";
                        json.push_back(Hex[(character >> 4) & 0x0f]);
                        json.push_back(Hex[character & 0x0f]);
                    }
                    else
                    {
                        json.push_back(character);
                    }
                    break;
                }
            }
            json.push_back('"');
        }

        AZStd::string UnsignedToString(AZ::u64 value)
        {
            std::ostringstream stream;
            stream.imbue(std::locale::classic());
            stream << value;
            return stream.str().c_str();
        }

        void AppendJsonKey(AZStd::string& json, const char* key)
        {
            AppendEscapedJsonString(json, key);
            json.push_back(':');
        }

        void AppendJsonBool(AZStd::string& json, bool value)
        {
            json += value ? "true" : "false";
        }

        void AppendLayoutArray(
            AZStd::string& json,
            const AZStd::vector<AdapterPackageLayoutEntry>& entries)
        {
            json.push_back('[');
            for (size_t index = 0; index < entries.size(); ++index)
            {
                if (index > 0)
                {
                    json.push_back(',');
                }
                const AdapterPackageLayoutEntry& entry = entries[index];
                json.push_back('{');
                AppendJsonKey(json, "InventoryEntryId");
                AppendEscapedJsonString(json, entry.m_inventoryEntryId);
                json += ",";
                AppendJsonKey(json, "StagingPath");
                AppendEscapedJsonString(json, entry.m_stagingPath);
                json += ",";
                AppendJsonKey(json, "PackagePath");
                AppendEscapedJsonString(json, entry.m_packagePath);
                json += ",";
                AppendJsonKey(json, "Role");
                AppendEscapedJsonString(json, entry.m_role);
                json += ",";
                AppendJsonKey(json, "MediaType");
                AppendEscapedJsonString(json, entry.m_mediaType);
                json += ",";
                AppendJsonKey(json, "OutputDigest");
                AppendEscapedJsonString(json, entry.m_outputDigest);
                json += ",";
                AppendJsonKey(json, "ByteSize");
                json += UnsignedToString(entry.m_byteSize);
                json += ",";
                AppendJsonKey(json, "ProjectOwned");
                AppendJsonBool(json, entry.m_projectOwned);
                json += ",";
                AppendJsonKey(json, "Redistributable");
                AppendJsonBool(json, entry.m_redistributable);
                json.push_back('}');
            }
            json.push_back(']');
        }

        void AppendOmissionArray(
            AZStd::string& json,
            const AZStd::vector<AdapterPackageAssemblyOmission>& omissions)
        {
            json.push_back('[');
            for (size_t index = 0; index < omissions.size(); ++index)
            {
                if (index > 0)
                {
                    json.push_back(',');
                }
                const AdapterPackageAssemblyOmission& omission = omissions[index];
                json.push_back('{');
                AppendJsonKey(json, "ExpectedPath");
                AppendEscapedJsonString(json, omission.m_expectedPath);
                json += ",";
                AppendJsonKey(json, "Role");
                AppendEscapedJsonString(json, omission.m_role);
                json += ",";
                AppendJsonKey(json, "Reason");
                AppendEscapedJsonString(json, omission.m_reason);
                json.push_back('}');
            }
            json.push_back(']');
        }

        void AppendStringArray(
            AZStd::string& json,
            const AZStd::vector<AZStd::string>& values)
        {
            json.push_back('[');
            for (size_t index = 0; index < values.size(); ++index)
            {
                if (index > 0)
                {
                    json.push_back(',');
                }
                AppendEscapedJsonString(json, values[index]);
            }
            json.push_back(']');
        }

        void AppendCollisionArray(
            AZStd::string& json,
            const AZStd::vector<AdapterPackageAssemblyCollision>& collisions)
        {
            json.push_back('[');
            for (size_t index = 0; index < collisions.size(); ++index)
            {
                if (index > 0)
                {
                    json.push_back(',');
                }
                const AdapterPackageAssemblyCollision& collision = collisions[index];
                json.push_back('{');
                AppendJsonKey(json, "PackagePath");
                AppendEscapedJsonString(json, collision.m_packagePath);
                json += ",";
                AppendJsonKey(json, "InventoryEntryIds");
                AppendStringArray(json, collision.m_inventoryEntryIds);
                json.push_back('}');
            }
            json.push_back(']');
        }

        void AppendBlockerArray(
            AZStd::string& json,
            const AZStd::vector<AdapterPackageAssemblyBlocker>& blockers)
        {
            json.push_back('[');
            for (size_t index = 0; index < blockers.size(); ++index)
            {
                if (index > 0)
                {
                    json.push_back(',');
                }
                const AdapterPackageAssemblyBlocker& blocker = blockers[index];
                json.push_back('{');
                AppendJsonKey(json, "Code");
                AppendEscapedJsonString(json, blocker.m_code);
                json += ",";
                AppendJsonKey(json, "PackagePath");
                AppendEscapedJsonString(json, blocker.m_packagePath);
                json += ",";
                AppendJsonKey(json, "Reason");
                AppendEscapedJsonString(json, blocker.m_reason);
                json.push_back('}');
            }
            json.push_back(']');
        }
    } // namespace

    AZStd::string AdapterPackageAssemblyPreviewService::SerializeCanonicalPreview(
        const AdapterPackageAssemblyPreview& preview) const
    {
        AZStd::string json;
        json.reserve(2048);
        json.push_back('{');
        AppendJsonKey(json, "FormatVersion");
        json += UnsignedToString(preview.m_formatVersion);
        json += ",";
        AppendJsonKey(json, "PreviewId");
        AppendEscapedJsonString(json, preview.m_previewId);
        json += ",";
        AppendJsonKey(json, "ManifestId");
        AppendEscapedJsonString(json, preview.m_manifestId);
        json += ",";
        AppendJsonKey(json, "ManifestFingerprint");
        AppendEscapedJsonString(json, preview.m_manifestFingerprint);
        json += ",";
        AppendJsonKey(json, "PackId");
        AppendEscapedJsonString(json, preview.m_packId);
        json += ",";
        AppendJsonKey(json, "PackVersion");
        AppendEscapedJsonString(json, preview.m_packVersion);
        json += ",";
        AppendJsonKey(json, "AdapterId");
        AppendEscapedJsonString(json, preview.m_adapterId);
        json += ",";
        AppendJsonKey(json, "AdapterVersion");
        AppendEscapedJsonString(json, preview.m_adapterVersion);
        json += ",";
        AppendJsonKey(json, "PlanId");
        AppendEscapedJsonString(json, preview.m_planId);
        json += ",";
        AppendJsonKey(json, "InventoryId");
        AppendEscapedJsonString(json, preview.m_inventoryId);
        json += ",";
        AppendJsonKey(json, "PackageRoot");
        AppendEscapedJsonString(json, preview.m_packageRoot);
        json += ",";
        AppendJsonKey(json, "Status");
        AppendEscapedJsonString(json, ToString(preview.m_status));
        json += ",";
        AppendJsonKey(json, "AssemblyAllowed");
        AppendJsonBool(json, preview.m_assemblyAllowed);
        json += ",";
        AppendJsonKey(json, "ArchiveAllowed");
        AppendJsonBool(json, preview.m_archiveAllowed);
        json += ",";
        AppendJsonKey(json, "DeploymentAllowed");
        AppendJsonBool(json, preview.m_deploymentAllowed);
        json += ",";
        AppendJsonKey(json, "Layout");
        AppendLayoutArray(json, preview.m_layout);
        json += ",";
        AppendJsonKey(json, "Omissions");
        AppendOmissionArray(json, preview.m_omissions);
        json += ",";
        AppendJsonKey(json, "Collisions");
        AppendCollisionArray(json, preview.m_collisions);
        json += ",";
        AppendJsonKey(json, "Blockers");
        AppendBlockerArray(json, preview.m_blockers);
        json.push_back('}');
        return json;
    }
} // namespace TaintedGrailModdingSDK
