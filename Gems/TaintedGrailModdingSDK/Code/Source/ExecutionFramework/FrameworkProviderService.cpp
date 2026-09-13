/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "FrameworkProviderService.h"
#include "ExecutionSynthetic/FrameworkSyntheticTarget.h"
#include <AzCore/std/algorithm.h>
#include <AzCore/std/containers/set.h>
#include <AzCore/std/sort.h>
#if AZ_TRAIT_TGSDK_FRAMEWORK_WINDOWS_STAGING
#include <Execution/Platform/Windows/ToolSandbox_Windows.h>
#endif

namespace TaintedGrailModdingSDK::ExecutionFramework
{
    namespace
    {
        bool Effects(const AZStd::vector<CE::SideEffect>& effects)
        {
            return AZStd::all_of(
                effects.begin(),
                effects.end(),
                [](auto e)
                {
                    return e == CE::SideEffect::READ_ONLY || e == CE::SideEffect::PROCESS_LAUNCH || e == CE::SideEffect::STAGING_WRITE;
                });
        }
        AZStd::string Identity(const HostBinding& h)
        {
            return h.m_binding.m_fingerprint + "/" + ET::CanonicalToolCommand(h.m_command).m_fingerprint + "/" + h.m_previewContractId;
        }
        bool Capture(const HostBinding& h, ET::ToolResolvedConfiguration& c, AZStd::vector<AZStd::string>& roots)
        {
            if (!h.m_resolve(c) || c.m_discoveryStatus != ET::DiscoveryStatus::Installed || c.m_providerId != h.m_binding.m_providerId ||
                c.m_providerVersion != h.m_binding.m_providerVersion || c.m_probeId != h.m_command.m_probeId ||
                !CE::IsExactVersion(c.m_toolVersion) || !CE::IsDigest(c.m_configurationFingerprint) || c.m_roots.empty() ||
                c.m_roots.size() > CE::MaximumCollection)
            {
                return false;
            }
#if AZ_TRAIT_TGSDK_FRAMEWORK_WINDOWS_STAGING
            AZStd::string digest;
            AZ::u64 bytes = 0;
            if (!ET::Windows::HashFile(ET::Windows::Wide(c.m_executablePath), digest, bytes) || digest != c.m_executableDigest)
            {
                return false;
            }
            AZStd::set<AZStd::string> rootIds;
            for (const auto& root : c.m_roots)
            {
                ET::Windows::PinnedPath pin;
                if (!ET::ToolSafeId(root.m_rootId) || !rootIds.insert(root.m_rootId).second || !pin.Open(root.m_absolutePath, true))
                {
                    return false;
                }
                roots.push_back(root.m_rootId + "/" + pin.m_identity);
            }
            AZStd::sort(roots.begin(), roots.end());
            return AZStd::adjacent_find(roots.begin(), roots.end()) == roots.end();
#else
            return false;
#endif
        }
        AZStd::string Environment(const ET::ToolResolvedConfiguration& c, const AZStd::vector<AZStd::string>& roots)
        {
            AZStd::string bytes = "foa-framework-environment-v1";
            auto append = [&](const AZStd::string& value)
            {
                bytes += AZStd::string::format("/%llu:", static_cast<unsigned long long>(value.size())) + value;
            };
            append(c.m_executableDigest);
            append(c.m_executablePath);
            append(c.m_toolVersion);
            append(c.m_probeId);
            for (const auto& root : roots)
            {
                append(root);
            }
            for (const auto& companion : c.m_companions)
            {
                append(companion.m_id);
                append(companion.m_rootId);
                append(companion.m_relativePath);
                append(companion.m_sha256);
                append(companion.m_kind);
                append(AZStd::string::format("%llu", static_cast<unsigned long long>(companion.m_bytes)));
            }
            return ET::ToolDigest(bytes);
        }
    } // namespace
    AZStd::string FrameworkProviderService::DescriptorFingerprint(const ET::ExternalToolProviderDescriptor& p)
    {
        AZStd::string bytes = "foa-framework-provider-descriptor-v1";
        bool valid = true;
        auto text = [&](const AZStd::string& value)
        {
            if (!ET::ToolSafeText(value, 4096) || bytes.size() + value.size() > CE::MaximumEmbeddedBytes)
            {
                valid = false;
                return;
            }
            bytes += AZStd::string::format("/%llu:", static_cast<unsigned long long>(value.size())) + value;
        };
        auto number = [&](AZ::u64 n)
        {
            text(AZStd::string::format("%llu", static_cast<unsigned long long>(n)));
        };
        auto strings = [&](const AZStd::vector<AZStd::string>& values)
        {
            if (values.size() > CE::MaximumCollection)
            {
                valid = false;
                return;
            }
            number(values.size());
            for (const auto& value : values)
            {
                text(value);
            }
        };
        text(p.m_providerId);
        text(p.m_displayName);
        text(p.m_providerVersion);
        number(p.m_minimumHostApiVersion.m_major);
        number(p.m_minimumHostApiVersion.m_minor);
        number(p.m_minimumHostApiVersion.m_patch);
        number(static_cast<AZ::u64>(p.m_toolFamily));
        strings(p.m_platforms);
        number(p.m_capabilities.m_supportsInteractive);
        number(p.m_capabilities.m_supportsBatch);
        number(p.m_capabilities.m_supportsHeadless);
        number(p.m_capabilities.m_producesAssetSources);
        number(p.m_capabilities.m_supportsStructuredIpc);
        if (p.m_commands.size() > CE::MaximumCollection || p.m_configuration.size() > CE::MaximumCollection ||
            p.m_discoveryProbes.size() > CE::MaximumCollection)
        {
            return {};
        }
        number(p.m_commands.size());
        for (const auto& c : p.m_commands)
        {
            text(c.m_commandId);
            text(c.m_displayName);
            number(static_cast<AZ::u64>(c.m_mode));
            strings(c.m_inputKinds);
            strings(c.m_outputKinds);
            number(c.m_supportsCancellation);
            number(c.m_requiresProject);
            number(c.m_requiresSelection);
        }
        number(p.m_configuration.size());
        for (const auto& c : p.m_configuration)
        {
            text(c.m_key);
            text(c.m_displayName);
            number(static_cast<AZ::u64>(c.m_kind));
            text(c.m_defaultValue);
            number(c.m_required);
            number(c.m_sensitive);
        }
        number(p.m_discoveryProbes.size());
        for (const auto& d : p.m_discoveryProbes)
        {
            text(d.m_probeId);
            number(static_cast<AZ::u64>(d.m_kind));
            text(d.m_pathConfigurationKey);
            text(d.m_versionConfigurationKey);
            text(d.m_minimumSupportedVersion);
            text(d.m_maximumSupportedVersion);
            strings(d.m_platforms);
            number(d.m_timeoutMilliseconds);
            number(d.m_required);
        }
        number(p.m_enabledByDefault);
        return valid ? ET::ToolDigest(bytes) : AZStd::string{};
    }
    bool FrameworkProviderService::Supported(const CE::CapabilityDescriptorV1& d)
    {
        return CE::Validate(d).IsSuccess() && !d.m_runtimeRequired && !d.m_requiredPhases.empty() &&
            d.m_terminalPhase <= CE::Phase::PACKAGE && d.m_saveImpact == CE::SideEffect::READ_ONLY && Effects(d.m_sideEffects) &&
            (d.m_rollbackRequired == CE::RollbackSupport::NONE || d.m_rollbackRequired == CE::RollbackSupport::CLEANUP_ONLY) &&
            AZStd::all_of(
                   d.m_requiredPhases.begin(),
                   d.m_requiredPhases.end(),
                   [](auto p)
                   {
                       return p >= CE::Phase::MATERIALIZE && p <= CE::Phase::PACKAGE;
                   });
    }
    bool FrameworkProviderService::Supports(const CE::CapabilityDescriptorV1& d) const
    {
        if (!m_synthetic)
            return Supported(d);
        return CE::Validate(d).IsSuccess() && d.m_capabilityId == FrameworkSyntheticTarget::Capability && d.m_exactProfileRequired &&
            !d.m_runtimeRequired && d.m_saveImpact == CE::SideEffect::READ_ONLY && Effects(d.m_sideEffects) &&
            d.m_rollbackRequired == CE::RollbackSupport::EXACT_RESTORE && d.m_terminalPhase == CE::Phase::ROLLBACK &&
            d.m_requiredPhases == AZStd::vector<CE::Phase>{ CE::Phase::BUILD,  CE::Phase::PACKAGE, CE::Phase::DEPLOY,
                                                            CE::Phase::LAUNCH, CE::Phase::VERIFY,  CE::Phase::ROLLBACK };
    }
    Result FrameworkProviderService::Register(HostBinding h)
    {
        std::lock_guard lock(m_mutex);
        if (m_finalized)
        {
            return { Error::Closed };
        }
        auto command = ET::CanonicalToolCommand(h.m_command);
        const auto& b = h.m_binding;
        if (!CE::Validate(b).IsSuccess() || !command || !h.m_resolve || !h.m_preview || !CE::IsStableId(h.m_previewContractId) ||
            !Effects(b.m_sideEffects) || b.m_phase < CE::Phase::MATERIALIZE ||
            b.m_phase > (m_synthetic ? CE::Phase::ROLLBACK : CE::Phase::PACKAGE) ||
            (m_synthetic &&
             (b.m_capabilityId != FrameworkSyntheticTarget::Capability || b.m_profileFingerprint != FrameworkSyntheticTarget::Profile())) ||
            b.m_providerFingerprint != DescriptorFingerprint(h.m_provider) || b.m_providerId != h.m_provider.m_providerId ||
            b.m_providerVersion != h.m_provider.m_providerVersion || b.m_providerId != h.m_command.m_providerId ||
            b.m_providerVersion != h.m_command.m_providerVersion || b.m_commandId != h.m_command.m_commandId ||
            h.m_command.m_profile != ET::ToolExecutionProfile)
        {
            return { Error::Invalid };
        }
        for (const auto& existing : m_bindings)
        {
            if (existing.m_binding.m_providerId == b.m_providerId && existing.m_binding.m_providerFingerprint != b.m_providerFingerprint)
            {
                return { Error::Collision };
            }
            if (existing.m_binding.m_id == b.m_id)
            {
                return { Identity(existing) == Identity(h) ? Error::None : Error::Collision };
            }
        }
        if (m_bindings.size() == CE::MaximumCollection)
        {
            return { Error::StoreFull };
        }
        m_bindings.push_back(AZStd::move(h));
        return {};
    }
    void FrameworkProviderService::Finalize()
    {
        std::lock_guard lock(m_mutex);
        m_finalized = true;
    }
    Result FrameworkProviderService::ReviewQualification(Qualification q)
    {
        std::lock_guard lock(m_mutex);
        if (!CE::IsDigest(q.m_bindingFingerprint) || !CE::IsDigest(q.m_executableDigest) || !CE::IsDigest(q.m_profileFingerprint) ||
            !CE::IsStableId(q.m_observationId) || q.m_evidenceIds.empty() || q.m_evidenceIds.size() > CE::MaximumCollection ||
            q.m_from > Clock::now() || q.m_until <= Clock::now() ||
            !AZStd::all_of(q.m_evidenceIds.begin(), q.m_evidenceIds.end(), CE::IsStableId))
        {
            return { Error::Invalid };
        }
        auto binding = AZStd::find_if(
            m_bindings.begin(),
            m_bindings.end(),
            [&](const auto& h)
            {
                return h.m_binding.m_fingerprint == q.m_bindingFingerprint && h.m_binding.m_profileFingerprint == q.m_profileFingerprint;
            });
        if (binding == m_bindings.end())
        {
            return { Error::MissingProvider };
        }
        for (const auto& evidence : binding->m_binding.m_qualificationEvidence)
        {
            if (AZStd::find(q.m_evidenceIds.begin(), q.m_evidenceIds.end(), evidence) == q.m_evidenceIds.end())
            {
                return { Error::Unqualified };
            }
        }
        q.m_revision = ++m_revision;
        for (auto& old : m_qualifications)
        {
            if (old.m_bindingFingerprint == q.m_bindingFingerprint)
            {
                old = AZStd::move(q);
                return {};
            }
        }
        m_qualifications.push_back(AZStd::move(q));
        return {};
    }
    void FrameworkProviderService::RevokeQualification(const AZStd::string& fingerprint)
    {
        std::lock_guard lock(m_mutex);
        ++m_revision;
        m_qualifications.erase(
            AZStd::remove_if(
                m_qualifications.begin(),
                m_qualifications.end(),
                [&](const auto& q)
                {
                    return q.m_bindingFingerprint == fingerprint;
                }),
            m_qualifications.end());
    }
    Result FrameworkProviderService::Resolve(
        const CE::CapabilityDescriptorV1& d,
        const CE::CapabilityExecutionRequestV1& r,
        CE::Phase phase,
        const AZStd::string& workspaceDefault,
        HostBinding& output) const
    {
        std::lock_guard lock(m_mutex);
        if (!m_finalized)
        {
            return { Error::Closed };
        }
        if (!Supports(d) || !CE::Validate(r, d).IsSuccess())
        {
            return { Error::Unsupported };
        }
        AZStd::vector<const HostBinding*> candidates;
        AZStd::string preference;
        for (const auto& ref : r.m_preferredBindings)
        {
            CE::CapabilityProviderBindingV1 b;
            if (!Decode(ref.m_canonicalJson, b) || b.m_fingerprint != ref.m_fingerprint)
            {
                return { Error::Invalid };
            }
            if (b.m_phase == phase)
            {
                if (!preference.empty())
                {
                    return { Error::AmbiguousProvider };
                }
                preference = b.m_fingerprint;
            }
        }
        for (const auto& h : m_bindings)
        {
            const auto& b = h.m_binding;
            if (b.m_phase == phase && b.m_profileFingerprint == r.m_profileFingerprint && CE::Validate(b, d).IsSuccess() &&
                (preference.empty() || b.m_fingerprint == preference) &&
                (!preference.empty() || workspaceDefault.empty() || b.m_fingerprint == workspaceDefault))
            {
                candidates.push_back(&h);
            }
        }
        if (candidates.empty())
        {
            return { Error::MissingProvider };
        }
        if (candidates.size() != 1)
        {
            return { Error::AmbiguousProvider };
        }
        output = *candidates.front();
        return {};
    }
    Result FrameworkProviderService::Prepare(const HostBinding& h, const CE::CapabilityExecutionRequestV1& r, PreparedPhase& output) const
    {
        PreparedPhase p;
        p.m_binding = h.m_binding;
        if (!Capture(h, p.m_configuration, p.m_rootIdentities))
        {
            return { Error::Drifted };
        }
        {
            std::lock_guard lock(m_mutex);
            auto q = AZStd::find_if(
                m_qualifications.begin(),
                m_qualifications.end(),
                [&](const auto& value)
                {
                    return value.m_bindingFingerprint == h.m_binding.m_fingerprint &&
                        value.m_profileFingerprint == r.m_profileFingerprint &&
                        value.m_executableDigest == p.m_configuration.m_executableDigest && value.m_from <= Clock::now() &&
                        value.m_until > Clock::now();
                });
            if (q == m_qualifications.end())
            {
                return { Error::Unqualified };
            }
            p.m_qualificationRevision = q->m_revision;
        }
        auto result = h.m_preview(r, p.m_preview);
        if (!result)
        {
            return result;
        }
        auto& invocation = p.m_preview.m_invocation;
        auto& phase = p.m_preview.m_phase;
        invocation.m_attemptId = "preview.intent";
        invocation.m_commandFingerprint = ET::CanonicalToolCommand(h.m_command).m_fingerprint;
        auto encoded = ET::CanonicalToolRequest(invocation);
        if (!encoded)
        {
            return { Error::Invalid };
        }
        invocation.m_fingerprint = encoded.m_fingerprint;
        if (!ET::ValidateToolRequest(invocation, h.m_command) ||
            (!phase.m_mutations.empty() && (!m_synthetic || phase.m_phase != CE::Phase::DEPLOY)) ||
            phase.m_inputs.size() != invocation.m_inputs.size() || phase.m_expectedOutputs.size() != invocation.m_outputs.size())
        {
            return { Error::Invalid };
        }
        for (size_t i = 0; i < phase.m_inputs.size(); ++i)
        {
            const auto& a = phase.m_inputs[i];
            const auto& file = invocation.m_inputs[i];
            if (a.m_id != file.m_id || a.m_storageRootId != file.m_rootId || a.m_relativePath != file.m_relativePath ||
                a.m_digest != file.m_sha256 || a.m_byteSize != file.m_bytes)
            {
                return { Error::Invalid };
            }
        }
        for (size_t i = 0; i < phase.m_expectedOutputs.size(); ++i)
        {
            const auto& a = phase.m_expectedOutputs[i];
            const auto& file = invocation.m_outputs[i];
            if (a.m_relativePath != file.m_relativePath || a.m_mediaType != file.m_kind || a.m_maximumByteSize != file.m_maxBytes ||
                a.m_maximumByteSize > ET::ToolMaxArtifactBytes)
            {
                return { Error::Invalid };
            }
        }
        phase.m_configurationFingerprint =
            ET::ToolDigest(Identity(h) + "/" + encoded.m_fingerprint + "/" + p.m_configuration.m_configurationFingerprint);
        phase.m_environmentFingerprint = Environment(p.m_configuration, p.m_rootIdentities);
        if (!Seal(phase) || !CE::Validate(phase, h.m_binding).IsSuccess())
        {
            return { Error::Invalid };
        }
        output = AZStd::move(p);
        return {};
    }
    Result FrameworkProviderService::Recheck(const PreparedPhase& phase) const
    {
        HostBinding h;
        {
            std::lock_guard lock(m_mutex);
            auto found = AZStd::find_if(
                m_bindings.begin(),
                m_bindings.end(),
                [&](const auto& b)
                {
                    return b.m_binding.m_fingerprint == phase.m_binding.m_fingerprint;
                });
            if (found == m_bindings.end())
            {
                return { Error::MissingProvider };
            }
            h = *found;
            auto q = AZStd::find_if(
                m_qualifications.begin(),
                m_qualifications.end(),
                [&](const auto& value)
                {
                    return value.m_bindingFingerprint == phase.m_binding.m_fingerprint &&
                        value.m_revision == phase.m_qualificationRevision &&
                        value.m_executableDigest == phase.m_configuration.m_executableDigest &&
                        value.m_profileFingerprint == phase.m_binding.m_profileFingerprint && value.m_from <= Clock::now() &&
                        value.m_until > Clock::now();
                });
            if (q == m_qualifications.end())
            {
                return { Error::Unqualified };
            }
        }
        ET::ToolResolvedConfiguration current;
        AZStd::vector<AZStd::string> roots;
        if (!Capture(h, current, roots) || current.m_configurationFingerprint != phase.m_configuration.m_configurationFingerprint ||
            Environment(current, roots) != phase.m_preview.m_phase.m_environmentFingerprint)
        {
            return { Error::Drifted };
        }
        return {};
    }
    AZStd::vector<HostBinding> FrameworkProviderService::Bindings() const
    {
        std::lock_guard lock(m_mutex);
        return m_bindings;
    }
    bool FrameworkProviderService::CurrentConfiguration(
        const ET::ToolExecutionCommandV2& command, ET::ToolResolvedConfiguration& output) const
    {
        auto bindings = Bindings();
        for (const auto& h : bindings)
        {
            if (ET::CanonicalToolCommand(command).m_fingerprint == ET::CanonicalToolCommand(h.m_command).m_fingerprint)
            {
                return h.m_resolve(output);
            }
        }
        return false;
    }
} // namespace TaintedGrailModdingSDK::ExecutionFramework
