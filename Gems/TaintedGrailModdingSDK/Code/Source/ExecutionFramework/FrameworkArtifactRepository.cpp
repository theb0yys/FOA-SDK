/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "FrameworkArtifactRepository.h"
#include <AzCore/std/algorithm.h>
#include <filesystem>
#if AZ_TRAIT_TGSDK_FRAMEWORK_WINDOWS_STAGING
#include <Execution/Platform/Windows/ToolSandbox_Windows.h>
#endif

namespace TaintedGrailModdingSDK::ExecutionFramework
{
    namespace
    {
#if AZ_TRAIT_TGSDK_FRAMEWORK_WINDOWS_STAGING
        bool CopyCancellable(
            const AZStd::string& source,
            const AZStd::string& destination,
            const CE::ArtifactReferenceV1& expected,
            const std::atomic_bool& cancelled)
        {
            ET::Windows::PinnedPath input, parent;
            if (!input.Open(source, false, GENERIC_READ, FILE_SHARE_READ) || input.m_bytes != expected.m_byteSize ||
                !parent.Open(ET::Windows::Utf8(std::filesystem::path(ET::Windows::Wide(destination)).parent_path().wstring()), true))
            {
                return false;
            }
            ET::Windows::Handle output(
                CreateFileW(ET::Windows::Wide(destination).c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
            if (!output)
            {
                return false;
            }
            ET::ToolSha256 hash;
            unsigned char buffer[65536];
            AZ::u64 total = 0;
            while (!cancelled)
            {
                DWORD read = 0, written = 0;
                if (!ReadFile(input.Leaf(), buffer, sizeof(buffer), &read, nullptr))
                {
                    return false;
                }
                if (!read)
                {
                    return total == expected.m_byteSize && hash.Finish() == expected.m_digest && FlushFileBuffers(output.Get());
                }
                if (read > expected.m_byteSize - total)
                {
                    return false;
                }
                total += read;
                hash.Update(buffer, read);
                if (!WriteFile(output.Get(), buffer, read, &written, nullptr) || written != read)
                {
                    return false;
                }
            }
            // Keep any partial file quarantined with its failed transaction; never publish ownership.
            return false;
        }
#endif
    } // namespace
    FrameworkArtifactRepository::FrameworkArtifactRepository(AZStd::string root)
        : m_root(AZStd::move(root))
    {
    }
    AZStd::string FrameworkArtifactRepository::Path(const CE::ArtifactReferenceV1& a) const
    {
        if (!CE::IsStableId(a.m_storageRootId) || !CE::IsRelativeLocator(a.m_relativePath))
        {
            return {};
        }
        return m_root + "/payloads/" + a.m_storageRootId + "/" + a.m_relativePath;
    }
    Result FrameworkArtifactRepository::Verify(const CE::ArtifactRecordV1& a) const
    {
        if (!CE::Validate(a).IsSuccess())
        {
            return { Error::Invalid };
        }
#if AZ_TRAIT_TGSDK_FRAMEWORK_WINDOWS_STAGING
        AZStd::string digest;
        AZ::u64 bytes = 0;
        return { ET::Windows::HashFile(ET::Windows::Wide(Path(a.m_artifact)), digest, bytes) && digest == a.m_artifact.m_digest &&
                         bytes == a.m_artifact.m_byteSize
                     ? Error::None
                     : Error::Drifted };
#else
        return { Error::Unsupported };
#endif
    }
    Result FrameworkArtifactRepository::Capture(
        const CE::CapabilityPhasePlanV1& phase,
        const ET::ToolInvocationRequestV2& request,
        const ET::ToolInvocationRecordV2& tool,
        const AZStd::string& toolStore,
        const AZStd::string& execution,
        const CE::PhaseExtensionReferenceV1& extension,
        const std::atomic_bool& cancelled,
        AZStd::vector<CE::ArtifactRecordV1>& output,
        std::function<bool(const CE::ArtifactRecordV1&)> ownsExisting)
    {
        if (!ET::ToolSucceeded(tool) || tool.m_status.m_attemptId != request.m_attemptId ||
            tool.m_requestFingerprint != request.m_fingerprint || tool.m_outputs.size() != phase.m_expectedOutputs.size() ||
            request.m_outputs.size() != phase.m_expectedOutputs.size())
        {
            return { Error::Invalid };
        }
#if AZ_TRAIT_TGSDK_FRAMEWORK_WINDOWS_STAGING
        AZStd::vector<CE::ArtifactRecordV1> records;
        for (size_t index = 0; index < phase.m_expectedOutputs.size(); ++index)
        {
            const auto& expected = phase.m_expectedOutputs[index];
            if (cancelled)
            {
                return { Error::Cancelled };
            }
            auto found = AZStd::find_if(
                tool.m_outputs.begin(),
                tool.m_outputs.end(),
                [&](const auto& file)
                {
                    return file.m_id == request.m_outputs[index].m_id;
                });
            if (found == tool.m_outputs.end() || found->m_rootId.find("staging.") != 0 || !ET::ToolSafeId(found->m_rootId.substr(8)) ||
                found->m_relativePath != "out/" + expected.m_relativePath || found->m_kind != expected.m_mediaType ||
                found->m_bytes > expected.m_maximumByteSize || found->m_bytes > ET::ToolMaxArtifactBytes ||
                (!expected.m_expectedDigest.empty() && found->m_sha256 != expected.m_expectedDigest))
            {
                return { Error::Invalid };
            }
            if (m_bytes + found->m_bytes > MaximumStore)
            {
                return { Error::StoreFull };
            }
            CE::ArtifactRecordV1 a;
            a.m_id = expected.m_id;
            auto& ref = a.m_artifact;
            ref.m_id = expected.m_id;
            ref.m_payloadContractId = expected.m_payloadContractId;
            ref.m_ownerPackId = expected.m_ownerPackId;
            ref.m_storageRootId = expected.m_storageRootId;
            ref.m_relativePath = expected.m_relativePath;
            ref.m_digest = found->m_sha256;
            ref.m_byteSize = found->m_bytes;
            ref.m_custodianId = expected.m_custodianId;
            ref.m_lifecycle = CE::ArtifactLifecycle::VERIFIED;
            a.m_role = expected.m_role;
            a.m_mediaType = expected.m_mediaType;
            a.m_producerExecutionId = execution;
            a.m_producerPhaseId = phase.m_id;
            a.m_producerId = phase.m_providerId;
            a.m_sourceManifest = CE::Reference(extension).GetValue();
            a.m_redistribution = expected.m_redistribution;
            if (!Seal(ref) || !Seal(a))
            {
                return { Error::Invalid };
            }
            auto destination = Path(ref);
            auto relative = ref.m_storageRootId + "/" + ref.m_relativePath;
            size_t slash = 0;
            while ((slash = relative.find('/', slash)) != AZStd::string::npos)
            {
                auto directory = m_root + "/payloads/" + relative.substr(0, slash++);
                ET::Windows::PinnedPath pin;
                if (!pin.Open(directory, true) && !ET::Windows::CreatePrivateDirectory(ET::Windows::Wide(directory)))
                {
                    return { Error::StorageFailed };
                }
            }
            auto source = toolStore + "/staging/" + found->m_rootId.substr(8) + "/" + found->m_relativePath;
            if (GetFileAttributesW(ET::Windows::Wide(destination).c_str()) != INVALID_FILE_ATTRIBUTES)
            {
                if (!ownsExisting || !ownsExisting(a) || !Verify(a))
                {
                    return { Error::Conflict };
                }
                records.push_back(AZStd::move(a));
                continue;
            }
            // Create-new payload publication; a pre-existing or interrupted copy is never overwritten.
            if (!CopyCancellable(source, destination, ref, cancelled) || !Verify(a))
            {
                return { cancelled ? Error::Cancelled : Error::StorageFailed };
            }
            m_bytes += ref.m_byteSize;
            records.push_back(AZStd::move(a));
        }
        output = AZStd::move(records);
        return {};
#else
        return { Error::Unsupported };
#endif
    }
} // namespace TaintedGrailModdingSDK::ExecutionFramework
