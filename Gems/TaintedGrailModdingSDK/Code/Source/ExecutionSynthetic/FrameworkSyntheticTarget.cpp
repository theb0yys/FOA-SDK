/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include "FrameworkSyntheticTarget.h"
#if AZ_TRAIT_TGSDK_FRAMEWORK_WINDOWS_STAGING
#include <Execution/Platform/Windows/ToolSandbox_Windows.h>
#endif
#include <mutex>

namespace TaintedGrailModdingSDK::ExecutionFramework
{
    namespace
    {
        constexpr size_t Limit = 65536;
        const CE::CapabilityPhasePlanV1* Deployment(const CE::CapabilityExecutionPlanV1& p)
        {
            for (const auto& phase : p.m_phases)
            {
                if (phase.m_phase == CE::Phase::DEPLOY)
                    return &phase;
            }
            return nullptr;
        }
#if AZ_TRAIT_TGSDK_FRAMEWORK_WINDOWS_STAGING
        namespace W = ET::Windows;
        bool ReadHandle(HANDLE file, AZStd::string& bytes)
        {
            LARGE_INTEGER size{}, zero{};
            if (!GetFileSizeEx(file, &size) || size.QuadPart < 0 || size.QuadPart > Limit ||
                !SetFilePointerEx(file, zero, nullptr, FILE_BEGIN))
                return false;
            bytes.resize(static_cast<size_t>(size.QuadPart));
            DWORD read = 0;
            return ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr) && read == bytes.size();
        }
#endif
    } // namespace
    struct FrameworkSyntheticTarget::Impl
    {
        AZStd::string m_parent, m_root, m_owner, m_identity, m_files, m_plan, m_journal, m_state = "baseline";
        mutable std::mutex m_mutex;
#if AZ_TRAIT_TGSDK_FRAMEWORK_WINDOWS_STAGING
        W::PinnedPath m_pin, m_lease;
        AZStd::string Files(const W::PinnedPath& payload, const W::PinnedPath& backup, const W::PinnedPath& canary) const
        {
            return ET::ToolDigest(payload.m_identity + "/" + backup.m_identity + "/" + canary.m_identity);
        }
        bool CaptureFiles()
        {
            W::PinnedPath payload, backup, canary;
            if (!payload.Open(m_root + "/payload.txt", false) || !backup.Open(m_root + "/backup.txt", false) ||
                !canary.Open(m_root + "/canary.txt", false))
                return false;
            m_files = Files(payload, backup, canary);
            return true;
        }
        bool ObservationCapacity(bool writing = true) const
        {
            W::PinnedPath root;
            if (!root.Open(m_parent + "/synthetic-observations", true))
                return false;
            WIN32_FIND_DATAW data{};
            HANDLE find = FindFirstFileW(W::Wide(m_parent + "/synthetic-observations/*").c_str(), &data);
            if (find == INVALID_HANDLE_VALUE)
                return GetLastError() == ERROR_FILE_NOT_FOUND;
            size_t count = 0;
            bool valid = true;
            do
            {
                std::wstring name = data.cFileName;
                if (name == L"." || name == L"..")
                    continue;
                if (++count > MaximumAttempts || (writing && count == MaximumAttempts) ||
                    (data.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)))
                {
                    valid = false;
                    break;
                }
            } while (FindNextFileW(find, &data));
            auto error = GetLastError();
            FindClose(find);
            return valid && error == ERROR_NO_MORE_FILES;
        }
        bool Record(const CE::RollbackReceiptV1& receipt)
        {
            auto encoded = CE::Canonicalize(receipt);
            if (!encoded.IsSuccess())
                return false;
            auto path = W::Wide(m_parent + "/synthetic-observations/" + receipt.m_fingerprint.substr(7) + ".json");
            AZStd::string old;
            if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES)
            {
                if (!W::ReadFileBounded(path, CE::MaximumCanonicalBytes, old) || old != encoded.GetValue().m_json)
                    return false;
            }
            else if (!W::WriteFileAtomic(path, encoded.GetValue().m_json))
                return false;
            return W::WriteFileAtomic(W::Wide(m_parent + "/synthetic-rollback.json"), encoded.GetValue().m_json);
        }
        bool Read(const char* path, AZStd::string& bytes) const
        {
            return W::ReadFileBounded(W::Wide(m_root + "/" + path), Limit, bytes);
        }
        bool Save(const char* state)
        {
            AZStd::string current;
            if (!m_journal.empty() && (!Read("transaction.state", current) || current != m_journal))
                return false;
            // This is a separate synthetic profile journal, not a modification of the M3 store contract.
            AZStd::string body =
                "foa-synthetic-target-v1\n" + m_identity + "\n" + m_owner + "\n" + m_files + "\n" + m_plan + "\n" + state + "\n";
            if (!W::WriteFileAtomic(W::Wide(m_root + "/transaction.state"), body + ET::ToolDigest(body)))
                return false;
            m_journal = body + ET::ToolDigest(body);
            m_state = state;
            return true;
        }
        Result Check(bool deployed) const
        {
            W::PinnedPath pin, payloadPin, backupPin, canaryPin;
            AZStd::string payload, backup, canary;
            if (!pin.Open(m_root, true) || pin.m_identity != m_identity ||
                !payloadPin.Open(m_root + "/payload.txt", false, GENERIC_READ, FILE_SHARE_READ) ||
                !backupPin.Open(m_root + "/backup.txt", false, GENERIC_READ, FILE_SHARE_READ) ||
                !canaryPin.Open(m_root + "/canary.txt", false, GENERIC_READ, FILE_SHARE_READ) ||
                Files(payloadPin, backupPin, canaryPin) != m_files || !ReadHandle(payloadPin.Leaf(), payload) ||
                !ReadHandle(backupPin.Leaf(), backup) || !ReadHandle(canaryPin.Leaf(), canary) || backup != Baseline || canary != Canary ||
                payload != (deployed ? Payload : Baseline))
                return { Error::Drifted };
            // Fixed inventory: extra files, subdirectories, case aliases and links are refused.
            WIN32_FIND_DATAW data{};
            HANDLE find = FindFirstFileW(W::Wide(m_root + "/*").c_str(), &data);
            if (find == INVALID_HANDLE_VALUE)
                return { Error::Drifted };
            size_t count = 0;
            bool valid = true;
            do
            {
                std::wstring name = data.cFileName;
                if (name == L"." || name == L"..")
                    continue;
                if (++count > 5 || (data.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) ||
                    (name != L"payload.txt" && name != L"backup.txt" && name != L"canary.txt" && name != L"lease.lock" &&
                     name != L"transaction.state"))
                {
                    valid = false;
                    break;
                }
            } while (FindNextFileW(find, &data));
            const auto last = GetLastError();
            FindClose(find);
            if (!valid || count != 5 || last != ERROR_NO_MORE_FILES)
                return { Error::Drifted };
            return {};
        }
        Result Replace(const char* expected, const char* desired, const char* intent, const char* done)
        {
            W::PinnedPath payload, backupPin, canaryPin;
            AZStd::string current, backup, canary;
            if (!payload.Open(m_root + "/payload.txt", false, GENERIC_READ | GENERIC_WRITE, 0) ||
                !backupPin.Open(m_root + "/backup.txt", false, GENERIC_READ, FILE_SHARE_READ) ||
                !canaryPin.Open(m_root + "/canary.txt", false, GENERIC_READ, FILE_SHARE_READ) ||
                Files(payload, backupPin, canaryPin) != m_files || !ReadHandle(backupPin.Leaf(), backup) || backup != Baseline ||
                !ReadHandle(canaryPin.Leaf(), canary) || canary != Canary || !ReadHandle(payload.Leaf(), current) || current != expected)
                return { Error::Drifted };
            if (!Save(intent))
                return { Error::StorageFailed };
            DWORD written = 0;
            LARGE_INTEGER zero{};
            const auto length = static_cast<DWORD>(strlen(desired));
            if (!SetFilePointerEx(payload.Leaf(), zero, nullptr, FILE_BEGIN) ||
                !WriteFile(payload.Leaf(), desired, length, &written, nullptr) || written != length || !SetEndOfFile(payload.Leaf()) ||
                !FlushFileBuffers(payload.Leaf()) || !ReadHandle(payload.Leaf(), current) || current != desired)
                return { Error::Quarantined };
            return Save(done) ? Result{} : Result{ Error::StorageFailed };
        }
#endif
    };
    FrameworkSyntheticTarget::FrameworkSyntheticTarget()
        : m_impl(std::make_unique<Impl>())
    {
    }
    FrameworkSyntheticTarget::~FrameworkSyntheticTarget() = default;
    AZStd::string FrameworkSyntheticTarget::Profile()
    {
        return ET::ToolDigest("foa-isolated-synthetic-spine-v1");
    }
    AZStd::string FrameworkSyntheticTarget::Root() const
    {
        return m_impl->m_root;
    }
    AZStd::string FrameworkSyntheticTarget::Inventory() const
    {
        return ET::ToolDigest(
            "foa-synthetic-inventory-v1/" + m_impl->m_identity + "/" + m_impl->m_owner + "/" + m_impl->m_files + "/" + Baseline + Canary);
    }
    CE::ArtifactReferenceV1 FrameworkSyntheticTarget::Backup() const
    {
        CE::ArtifactReferenceV1 a;
        a.m_id = "artifact.synthetic-backup";
        a.m_payloadContractId = "data.synthetic";
        a.m_ownerPackId = m_impl->m_owner;
        a.m_storageRootId = "target.synthetic";
        a.m_relativePath = "backup.txt";
        a.m_digest = ET::ToolDigest(Baseline);
        a.m_byteSize = strlen(Baseline);
        a.m_custodianId = "custodian.synthetic";
        a.m_lifecycle = CE::ArtifactLifecycle::BACKUP;
        Seal(a);
        return a;
    }
    Result FrameworkSyntheticTarget::Create(
        const AZStd::string& root, const AZStd::string& owner, std::shared_ptr<FrameworkSyntheticTarget>& out)
    {
#if AZ_TRAIT_TGSDK_FRAMEWORK_WINDOWS_STAGING
        if (!CE::IsStableId(owner))
            return { Error::Invalid };
        auto t = std::shared_ptr<FrameworkSyntheticTarget>(new FrameworkSyntheticTarget);
        auto& i = *t->m_impl;
        W::PinnedPath parent;
        if (!parent.Open(root, true))
            return { Error::Invalid };
        i.m_parent = root;
        i.m_root = root + "/synthetic-target";
        i.m_owner = owner;
        if (GetFileAttributesW(W::Wide(i.m_root).c_str()) != INVALID_FILE_ATTRIBUTES || !W::CreatePrivateDirectory(W::Wide(i.m_root)) ||
            !i.m_pin.Open(i.m_root, true))
            return { Error::Conflict };
        i.m_identity = i.m_pin.m_identity;
        if (!W::CreatePrivateDirectory(W::Wide(root + "/synthetic-observations")))
            return { Error::Conflict };
        if (!W::WriteFileAtomic(W::Wide(i.m_root + "/lease.lock"), "synthetic-exclusive-v1") ||
            !i.m_lease.Open(i.m_root + "/lease.lock", false, GENERIC_READ, FILE_SHARE_READ) ||
            !W::WriteFileAtomic(W::Wide(i.m_root + "/backup.txt"), Baseline) ||
            !W::WriteFileAtomic(W::Wide(i.m_root + "/payload.txt"), Baseline) ||
            !W::WriteFileAtomic(W::Wide(i.m_root + "/canary.txt"), Canary) || !i.CaptureFiles() || !i.Save("baseline"))
            return { Error::StorageFailed };
        out = AZStd::move(t);
        return {};
#else
        (void)root;
        (void)owner;
        (void)out;
        return { Error::Unsupported };
#endif
    }
    Result FrameworkSyntheticTarget::Reopen(
        const AZStd::string& root, const AZStd::string& owner, std::shared_ptr<FrameworkSyntheticTarget>& out)
    {
#if AZ_TRAIT_TGSDK_FRAMEWORK_WINDOWS_STAGING
        auto t = std::shared_ptr<FrameworkSyntheticTarget>(new FrameworkSyntheticTarget);
        auto& i = *t->m_impl;
        i.m_parent = root;
        i.m_root = root + "/synthetic-target";
        i.m_owner = owner;
        // Share-read lets inventory inspect the lock but a second writer cannot acquire it.
        if (!CE::IsStableId(owner) || !i.m_pin.Open(i.m_root, true) ||
            !i.m_lease.Open(i.m_root + "/lease.lock", false, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ))
            return { Error::Busy };
        i.m_identity = i.m_pin.m_identity;
        AZStd::string state;
        if (!i.CaptureFiles() || !i.Read("transaction.state", state))
            return { Error::CorruptStore };
        AZStd::string prefix = "foa-synthetic-target-v1\n" + i.m_identity + "\n" + owner + "\n" + i.m_files + "\n";
        if (state.substr(0, prefix.size()) != prefix)
            return { Error::CorruptStore };
        auto end = state.find('\n', prefix.size());
        if (end == AZStd::string::npos)
            return { Error::CorruptStore };
        i.m_plan = state.substr(prefix.size(), end - prefix.size());
        auto next = state.find('\n', end + 1);
        if (next == AZStd::string::npos || state.substr(next + 1) != ET::ToolDigest(state.substr(0, next + 1)))
            return { Error::CorruptStore };
        i.m_state = state.substr(end + 1, next - end - 1);
        i.m_journal = state;
        if ((!i.m_plan.empty() && !CE::IsDigest(i.m_plan)) ||
            (i.m_state != "baseline" && i.m_state != "intent" && i.m_state != "deployed" && i.m_state != "restore-intent" &&
             i.m_state != "restored"))
            return { Error::CorruptStore };
        if (!i.ObservationCapacity(false))
            return { Error::StoreFull };
        if (!t->Pending() && !i.Check(false))
            return { Error::Drifted };
        out = AZStd::move(t);
        return {};
#else
        (void)root;
        (void)owner;
        (void)out;
        return { Error::Unsupported };
#endif
    }
    bool FrameworkSyntheticTarget::Accepts(const CE::CapabilityPhasePlanV1& p) const
    {
        if (p.m_capabilityId != Capability || p.m_profileFingerprint != Profile() || p.m_targetInventoryFingerprint != Inventory())
            return false;
        if (p.m_phase != CE::Phase::DEPLOY)
            return p.m_mutations.empty();
        if (p.m_mutations.size() != 1 || p.m_rollback.m_steps.size() != 1)
            return false;
        const auto& m = p.m_mutations[0];
        const auto& s = p.m_rollback.m_steps[0];
        return m.m_targetRootId == "target.synthetic" && m.m_relativePath == "payload.txt" && m.m_ownerPackId == m_impl->m_owner &&
            m.m_operation == CE::MutationOperation::REPLACE && m.m_preimagePresence == CE::PreimagePresence::PRESENT &&
            m.m_preimageFingerprint == ET::ToolDigest(Baseline) && m.m_preimageOwnerId == m_impl->m_owner && m.m_backupArtifact &&
            m.m_backupArtifact->m_fingerprint == Backup().m_fingerprint && m.m_desiredArtifact &&
            m.m_desiredArtifact->m_digest == ET::ToolDigest(Payload) && m.m_desiredArtifact->m_byteSize == strlen(Payload) &&
            m.m_desiredArtifact->m_ownerPackId == m_impl->m_owner && s.m_action == CE::RollbackAction::RESTORE_BACKUP &&
            s.m_restoreFingerprint == ET::ToolDigest(Baseline);
    }
    Result FrameworkSyntheticTarget::Check(bool deployed) const
    {
        std::lock_guard lock(m_impl->m_mutex);
#if AZ_TRAIT_TGSDK_FRAMEWORK_WINDOWS_STAGING
        return m_impl->Check(deployed);
#else
        (void)deployed;
        return { Error::Unsupported };
#endif
    }
    bool FrameworkSyntheticTarget::Pending() const
    {
        std::lock_guard lock(m_impl->m_mutex);
        return m_impl->m_state == "intent" || m_impl->m_state == "deployed" || m_impl->m_state == "restore-intent";
    }
    Result FrameworkSyntheticTarget::Begin(const CE::CapabilityExecutionPlanV1& p)
    {
        std::lock_guard lock(m_impl->m_mutex);
#if AZ_TRAIT_TGSDK_FRAMEWORK_WINDOWS_STAGING
        if (m_impl->m_state != "baseline" && m_impl->m_state != "restored")
            return { Error::Quarantined };
        // Reserve bounded recovery capacity before any forward work can mutate this target.
        if (!m_impl->ObservationCapacity())
            return { Error::StoreFull };
        const auto* d = Deployment(p);
        if (!CE::Validate(p).IsSuccess() || !d || !Accepts(*d))
            return { Error::Invalid };
        if (!m_impl->Check(false))
            return { Error::Drifted };
        m_impl->m_plan = p.m_fingerprint;
        return m_impl->Save("baseline") ? Result{} : Result{ Error::StorageFailed };
#else
        (void)p;
        return { Error::Unsupported };
#endif
    }
    Result FrameworkSyntheticTarget::Apply(
        const CE::CapabilityExecutionPlanV1& p, const AZStd::string& artifact, AZStd::vector<CE::TargetObservationV1>& observations)
    {
        std::lock_guard lock(m_impl->m_mutex);
#if AZ_TRAIT_TGSDK_FRAMEWORK_WINDOWS_STAGING
        const auto* d = Deployment(p);
        AZStd::string bytes;
        if (!CE::Validate(p).IsSuccess() || !d || !Accepts(*d) || m_impl->m_plan != p.m_fingerprint || m_impl->m_state != "baseline" ||
            !W::ReadFileBounded(W::Wide(artifact), Limit, bytes) || bytes != Payload)
            return { Error::Invalid };
        if (!m_impl->ObservationCapacity())
            return { Error::StoreFull };
        auto result = m_impl->Check(false);
        if (result)
            result = m_impl->Replace(Baseline, Payload, "intent", "deployed");
        if (!result)
            return result;
        const auto& m = d->m_mutations[0];
        CE::TargetObservationV1 o;
        o.m_id = "observation.synthetic-deploy";
        o.m_mutationId = m.m_id;
        o.m_targetRootId = m.m_targetRootId;
        o.m_relativePath = m.m_relativePath;
        o.m_presence = CE::PreimagePresence::PRESENT;
        o.m_contentFingerprint = ET::ToolDigest(Payload);
        o.m_ownerPackId = m_impl->m_owner;
        o.m_verification = CE::VerificationState::PASSED;
        if (!Seal(o))
            return { Error::Invalid };
        observations.push_back(AZStd::move(o));
        return {};
#else
        (void)p;
        (void)artifact;
        (void)observations;
        return { Error::Unsupported };
#endif
    }
    Result FrameworkSyntheticTarget::Rollback(const CE::CapabilityExecutionPlanV1& p, CE::RollbackReceiptV1& receipt)
    {
        std::lock_guard lock(m_impl->m_mutex);
#if AZ_TRAIT_TGSDK_FRAMEWORK_WINDOWS_STAGING
        const auto* d = Deployment(p);
        if (!CE::Validate(p).IsSuccess() || !d || !Accepts(*d) || m_impl->m_plan != p.m_fingerprint)
            return { Error::Invalid };
        if (!m_impl->ObservationCapacity())
            return { Error::StoreFull };
        receipt = {};
        receipt.m_id = NewIdentity("restore");
        receipt.m_plan = CE::Reference(p).GetValue();
        receipt.m_rollbackPlan = CE::Reference(d->m_rollback).GetValue();
        CE::RollbackStepReceiptV1 row;
        row.m_id = "receipt.synthetic-restore";
        row.m_stepId = d->m_rollback.m_steps[0].m_id;
        row.m_startedAt = UtcNow();
        auto result = m_impl->Check(true);
        if (result)
            result = m_impl->Replace(Payload, Baseline, "restore-intent", "restored");
        else if (
            (m_impl->m_state == "intent" || m_impl->m_state == "restore-intent" || m_impl->m_state == "restored") && m_impl->Check(false))
            result = m_impl->Save("restored") ? Result{} : Result{ Error::StorageFailed };
        row.m_finishedAt = UtcNow();
        row.m_outcome = result ? CE::Outcome::SUCCEEDED : CE::Outcome::FAILED;
        receipt.m_state = result ? CE::RollbackState::SUCCEEDED : CE::RollbackState::FAILED;
        if (result)
        {
            row.m_observedFingerprint = ET::ToolDigest(Baseline);
            row.m_observedOwnerId = m_impl->m_owner;
        }
        else
        {
            CE::FailureRecordV1 f;
            f.m_id = "failure.synthetic-restore";
            f.m_code = "error.synthetic-drift";
            f.m_message = "The owned target or backup changed. Retain the target for inspection.";
            Seal(f);
            row.m_failures.push_back(f);
            receipt.m_failures.push_back(f);
        }
        Seal(row);
        receipt.m_steps.push_back(row);
        if (!Seal(receipt))
            return { Error::Invalid };
        if (!m_impl->Record(receipt))
            return { Error::StorageFailed };
        return result;
#else
        (void)p;
        (void)receipt;
        return { Error::Unsupported };
#endif
    }
} // namespace TaintedGrailModdingSDK::ExecutionFramework
