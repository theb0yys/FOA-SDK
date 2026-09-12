/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "CapabilityExecutionContracts.h"
#include <AzCore/std/algorithm.h>
#include <AzCore/std/containers/array.h>

namespace TaintedGrailModdingSDK::CapabilityExecution
{
    bool SizeBudget::Add(size_t count, size_t width)
    {
        if (m_used > m_limit || (width && count > (m_limit - m_used) / width)) { return false; }
        m_used += count * width;
        return true;
    }
    namespace
    {
        bool Alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
        bool Digit(char c) { return c >= '0' && c <= '9'; }
        bool Utf8(AZStd::string_view value)
        {
            for (size_t i = 0; i < value.size();)
            {
                const auto c = static_cast<unsigned char>(value[i++]);
                if (c < 0x20 || c == 0x7f) { return false; }
                if (c < 0x80) { continue; }
                unsigned remaining = 0; AZ::u32 point = 0; AZ::u32 minimum = 0;
                if (c >= 0xc2 && c <= 0xdf) { remaining = 1; point = c & 31; minimum = 0x80; }
                else if (c >= 0xe0 && c <= 0xef) { remaining = 2; point = c & 15; minimum = 0x800; }
                else if (c >= 0xf0 && c <= 0xf4) { remaining = 3; point = c & 7; minimum = 0x10000; }
                else { return false; }
                if (remaining > value.size() - i) { return false; }
                while (remaining--)
                {
                    const auto next = static_cast<unsigned char>(value[i++]);
                    if ((next & 0xc0) != 0x80) { return false; }
                    point = (point << 6) | (next & 63);
                }
                if (point < minimum || point > 0x10ffff || (point >= 0xd800 && point <= 0xdfff)
                    || (point >= 0x80 && point <= 0x9f)) { return false; }
            }
            return true;
        }
    }
    bool IsSafeText(AZStd::string_view value)
    {
        if (value.size() > MaximumEmbeddedBytes || !Utf8(value)) { return false; }
        AZStd::string lower(value);
        for (auto& c : lower) { if (c >= 'A' && c <= 'Z') { c += 'a' - 'A'; } }
        for (AZStd::string_view forbidden : {"://", "\\\\", "/home/", "/users/", "/mnt/", "/tmp/",
            "/bin/", "password=", "password:", "secret=", "token=", "authorization:", "bearer ",
            "ghp_", "sk-", "-----begin ", "path=", "&&", "||", "$(", "`", ";", "|",
            "cmd.exe", "powershell.exe"})
        {
            if (lower.find(forbidden) != AZStd::string::npos) { return false; }
        }
        for (size_t i = 0; i + 2 < value.size(); ++i)
        { if (Alpha(value[i]) && value[i + 1] == ':' && (value[i + 2] == '/' || value[i + 2] == '\\')) { return false; } }
        return true;
    }
    bool IsStableId(AZStd::string_view value)
    {
        if (value.empty() || !Alpha(value.front()) || value.size() > MaximumIdBytes || value.find('.') == value.npos || !IsSafeText(value)) { return false; }
        bool start = true;
        for (char c : value)
        {
            if (c == '.') { if (start) { return false; } start = true; continue; }
            if (start && !Alpha(c) && !Digit(c)) { return false; }
            if (!Alpha(c) && !Digit(c) && c != '_' && c != '-') { return false; }
            start = false;
        }
        return !start;
    }
    bool IsDigest(AZStd::string_view value)
    {
        if (value.size() != 71 || value.substr(0, 7) != "sha256:") { return false; }
        return AZStd::all_of(value.begin() + 7, value.end(), [](char c) { return Digit(c) || (c >= 'a' && c <= 'f'); });
    }
    bool IsRelativeLocator(AZStd::string_view value)
    {
        if (value.empty() || value.size() > MaximumPathBytes || !IsSafeText(value)) { return false; }
        size_t start = 0;
        while (start < value.size())
        {
            auto end = value.find('/', start); if (end == value.npos) { end = value.size(); }
            const auto part = value.substr(start, end - start);
            if (part.empty() || part == "." || part == ".." || part.back() == '.' || part.back() == ' ') { return false; }
            for (char c : part) { if (!Alpha(c) && !Digit(c) && c != '_' && c != '-' && c != '.') { return false; } }
            AZStd::string base(part.substr(0, part.find('.')));
            for (auto& c : base) { if (c >= 'A' && c <= 'Z') { c += 'a' - 'A'; } }
            if (base == "con" || base == "prn" || base == "aux" || base == "nul"
                || (base.size() == 4 && (base.substr(0, 3) == "com" || base.substr(0, 3) == "lpt") && Digit(base[3]))) { return false; }
            if (end == value.size()) { return true; }
            start = end + 1;
        }
        return false;
    }
    bool IsUtcTimestamp(AZStd::string_view value)
    {
        if (value.size() != 20 || value[4] != '-' || value[7] != '-' || value[10] != 'T'
            || value[13] != ':' || value[16] != ':' || value[19] != 'Z') { return false; }
        for (size_t i = 0; i < value.size(); ++i)
        { if (i != 4 && i != 7 && i != 10 && i != 13 && i != 16 && i != 19 && !Digit(value[i])) { return false; } }
        auto number = [&](size_t pos, size_t length) { unsigned n = 0; while (length--) { n = n * 10 + value[pos++] - '0'; } return n; };
        const auto year = number(0,4), month = number(5,2), day = number(8,2);
        if (year < 1 || month < 1 || month > 12 || day < 1 || number(11,2) > 23 || number(14,2) > 59 || number(17,2) > 59) { return false; }
        constexpr unsigned days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
        const bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
        return day <= days[month - 1] + (month == 2 && leap ? 1 : 0);
    }
    bool IsExactVersion(AZStd::string_view value)
    {
        if (value.empty() || value.size() > 32) { return false; }
        unsigned components = 1; size_t digits = 0; char first = 0;
        for (char c : value)
        {
            if (c == '.') { if (!digits || components == 3) { return false; } ++components; digits = 0; continue; }
            if (!Digit(c) || digits >= 9 || (digits && first == '0')) { return false; }
            if (!digits) { first = c; } ++digits;
        }
        return components == 3 && digits > 0;
    }

    AZStd::string_view Token(Phase value)
    {
        switch (value)
        {
        case Phase::MATERIALIZE: return "MATERIALIZE";
        case Phase::BUILD: return "BUILD";
        case Phase::PACKAGE: return "PACKAGE";
        case Phase::DEPLOY: return "DEPLOY";
        case Phase::LAUNCH: return "LAUNCH";
        case Phase::VERIFY: return "VERIFY";
        case Phase::ASSESS: return "ASSESS";
        case Phase::RECONCILE: return "RECONCILE";
        case Phase::ROLLBACK: return "ROLLBACK";
        default: return {};
        }
    }
    bool ParseToken(AZStd::string_view token, Phase& output)
    {
        if (token == "MATERIALIZE") { output = Phase::MATERIALIZE; return true; }
        if (token == "BUILD") { output = Phase::BUILD; return true; }
        if (token == "PACKAGE") { output = Phase::PACKAGE; return true; }
        if (token == "DEPLOY") { output = Phase::DEPLOY; return true; }
        if (token == "LAUNCH") { output = Phase::LAUNCH; return true; }
        if (token == "VERIFY") { output = Phase::VERIFY; return true; }
        if (token == "ASSESS") { output = Phase::ASSESS; return true; }
        if (token == "RECONCILE") { output = Phase::RECONCILE; return true; }
        if (token == "ROLLBACK") { output = Phase::ROLLBACK; return true; }
        return false;
    }

    AZStd::string_view Token(SideEffect value)
    {
        switch (value)
        {
        case SideEffect::READ_ONLY: return "READ_ONLY";
        case SideEffect::WORKSPACE_WRITE: return "WORKSPACE_WRITE";
        case SideEffect::STAGING_WRITE: return "STAGING_WRITE";
        case SideEffect::PROCESS_LAUNCH: return "PROCESS_LAUNCH";
        case SideEffect::INSTALLATION_MUTATION: return "INSTALLATION_MUTATION";
        case SideEffect::RUNTIME_MUTATION: return "RUNTIME_MUTATION";
        case SideEffect::SAVE_MUTATION: return "SAVE_MUTATION";
        case SideEffect::SECRET_USE: return "SECRET_USE";
        case SideEffect::NETWORK_PUBLICATION: return "NETWORK_PUBLICATION";
        case SideEffect::DESTRUCTIVE_DELETE: return "DESTRUCTIVE_DELETE";
        default: return {};
        }
    }
    bool ParseToken(AZStd::string_view token, SideEffect& output)
    {
        if (token == "READ_ONLY") { output = SideEffect::READ_ONLY; return true; }
        if (token == "WORKSPACE_WRITE") { output = SideEffect::WORKSPACE_WRITE; return true; }
        if (token == "STAGING_WRITE") { output = SideEffect::STAGING_WRITE; return true; }
        if (token == "PROCESS_LAUNCH") { output = SideEffect::PROCESS_LAUNCH; return true; }
        if (token == "INSTALLATION_MUTATION") { output = SideEffect::INSTALLATION_MUTATION; return true; }
        if (token == "RUNTIME_MUTATION") { output = SideEffect::RUNTIME_MUTATION; return true; }
        if (token == "SAVE_MUTATION") { output = SideEffect::SAVE_MUTATION; return true; }
        if (token == "SECRET_USE") { output = SideEffect::SECRET_USE; return true; }
        if (token == "NETWORK_PUBLICATION") { output = SideEffect::NETWORK_PUBLICATION; return true; }
        if (token == "DESTRUCTIVE_DELETE") { output = SideEffect::DESTRUCTIVE_DELETE; return true; }
        return false;
    }

    AZStd::string_view Token(RollbackSupport value)
    {
        switch (value)
        {
        case RollbackSupport::NONE: return "NONE";
        case RollbackSupport::CLEANUP_ONLY: return "CLEANUP_ONLY";
        case RollbackSupport::COMPENSATING: return "COMPENSATING";
        case RollbackSupport::EXACT_RESTORE: return "EXACT_RESTORE";
        default: return {};
        }
    }
    bool ParseToken(AZStd::string_view token, RollbackSupport& output)
    {
        if (token == "NONE") { output = RollbackSupport::NONE; return true; }
        if (token == "CLEANUP_ONLY") { output = RollbackSupport::CLEANUP_ONLY; return true; }
        if (token == "COMPENSATING") { output = RollbackSupport::COMPENSATING; return true; }
        if (token == "EXACT_RESTORE") { output = RollbackSupport::EXACT_RESTORE; return true; }
        return false;
    }

    AZStd::string_view Token(SupportState value)
    {
        switch (value)
        {
        case SupportState::SUPPORTED: return "SUPPORTED";
        case SupportState::UNSUPPORTED: return "UNSUPPORTED";
        default: return {};
        }
    }
    bool ParseToken(AZStd::string_view token, SupportState& output)
    {
        if (token == "SUPPORTED") { output = SupportState::SUPPORTED; return true; }
        if (token == "UNSUPPORTED") { output = SupportState::UNSUPPORTED; return true; }
        return false;
    }

    AZStd::string_view Token(QualificationState value)
    {
        switch (value)
        {
        case QualificationState::QUALIFIED: return "QUALIFIED";
        case QualificationState::UNQUALIFIED: return "UNQUALIFIED";
        case QualificationState::STALE: return "STALE";
        case QualificationState::UNKNOWN: return "UNKNOWN";
        default: return {};
        }
    }
    bool ParseToken(AZStd::string_view token, QualificationState& output)
    {
        if (token == "QUALIFIED") { output = QualificationState::QUALIFIED; return true; }
        if (token == "UNQUALIFIED") { output = QualificationState::UNQUALIFIED; return true; }
        if (token == "STALE") { output = QualificationState::STALE; return true; }
        if (token == "UNKNOWN") { output = QualificationState::UNKNOWN; return true; }
        return false;
    }

    AZStd::string_view Token(EnvironmentState value)
    {
        switch (value)
        {
        case EnvironmentState::AVAILABLE: return "AVAILABLE";
        case EnvironmentState::UNAVAILABLE: return "UNAVAILABLE";
        case EnvironmentState::DRIFTED: return "DRIFTED";
        case EnvironmentState::UNKNOWN: return "UNKNOWN";
        default: return {};
        }
    }
    bool ParseToken(AZStd::string_view token, EnvironmentState& output)
    {
        if (token == "AVAILABLE") { output = EnvironmentState::AVAILABLE; return true; }
        if (token == "UNAVAILABLE") { output = EnvironmentState::UNAVAILABLE; return true; }
        if (token == "DRIFTED") { output = EnvironmentState::DRIFTED; return true; }
        if (token == "UNKNOWN") { output = EnvironmentState::UNKNOWN; return true; }
        return false;
    }

    AZStd::string_view Token(PolicyState value)
    {
        switch (value)
        {
        case PolicyState::ALLOWED: return "ALLOWED";
        case PolicyState::CONFIRMATION_REQUIRED: return "CONFIRMATION_REQUIRED";
        case PolicyState::DENIED: return "DENIED";
        default: return {};
        }
    }
    bool ParseToken(AZStd::string_view token, PolicyState& output)
    {
        if (token == "ALLOWED") { output = PolicyState::ALLOWED; return true; }
        if (token == "CONFIRMATION_REQUIRED") { output = PolicyState::CONFIRMATION_REQUIRED; return true; }
        if (token == "DENIED") { output = PolicyState::DENIED; return true; }
        return false;
    }

    AZStd::string_view Token(AuthorizationState value)
    {
        switch (value)
        {
        case AuthorizationState::NOT_REQUIRED: return "NOT_REQUIRED";
        case AuthorizationState::PENDING: return "PENDING";
        case AuthorizationState::GRANTED: return "GRANTED";
        case AuthorizationState::EXPIRED: return "EXPIRED";
        case AuthorizationState::REVOKED: return "REVOKED";
        case AuthorizationState::SCOPE_MISMATCH: return "SCOPE_MISMATCH";
        default: return {};
        }
    }
    bool ParseToken(AZStd::string_view token, AuthorizationState& output)
    {
        if (token == "NOT_REQUIRED") { output = AuthorizationState::NOT_REQUIRED; return true; }
        if (token == "PENDING") { output = AuthorizationState::PENDING; return true; }
        if (token == "GRANTED") { output = AuthorizationState::GRANTED; return true; }
        if (token == "EXPIRED") { output = AuthorizationState::EXPIRED; return true; }
        if (token == "REVOKED") { output = AuthorizationState::REVOKED; return true; }
        if (token == "SCOPE_MISMATCH") { output = AuthorizationState::SCOPE_MISMATCH; return true; }
        return false;
    }

    AZStd::string_view Token(Outcome value)
    {
        switch (value)
        {
        case Outcome::NOT_ATTEMPTED: return "NOT_ATTEMPTED";
        case Outcome::RUNNING: return "RUNNING";
        case Outcome::SUCCEEDED: return "SUCCEEDED";
        case Outcome::FAILED: return "FAILED";
        case Outcome::SKIPPED: return "SKIPPED";
        case Outcome::BLOCKED: return "BLOCKED";
        case Outcome::CANCELLED: return "CANCELLED";
        case Outcome::PARTIAL: return "PARTIAL";
        default: return {};
        }
    }
    bool ParseToken(AZStd::string_view token, Outcome& output)
    {
        if (token == "NOT_ATTEMPTED") { output = Outcome::NOT_ATTEMPTED; return true; }
        if (token == "RUNNING") { output = Outcome::RUNNING; return true; }
        if (token == "SUCCEEDED") { output = Outcome::SUCCEEDED; return true; }
        if (token == "FAILED") { output = Outcome::FAILED; return true; }
        if (token == "SKIPPED") { output = Outcome::SKIPPED; return true; }
        if (token == "BLOCKED") { output = Outcome::BLOCKED; return true; }
        if (token == "CANCELLED") { output = Outcome::CANCELLED; return true; }
        if (token == "PARTIAL") { output = Outcome::PARTIAL; return true; }
        return false;
    }

    AZStd::string_view Token(VerificationState value)
    {
        switch (value)
        {
        case VerificationState::NOT_CHECKED: return "NOT_CHECKED";
        case VerificationState::PASSED: return "PASSED";
        case VerificationState::FAILED: return "FAILED";
        case VerificationState::UNKNOWN: return "UNKNOWN";
        default: return {};
        }
    }
    bool ParseToken(AZStd::string_view token, VerificationState& output)
    {
        if (token == "NOT_CHECKED") { output = VerificationState::NOT_CHECKED; return true; }
        if (token == "PASSED") { output = VerificationState::PASSED; return true; }
        if (token == "FAILED") { output = VerificationState::FAILED; return true; }
        if (token == "UNKNOWN") { output = VerificationState::UNKNOWN; return true; }
        return false;
    }

    AZStd::string_view Token(AssessmentState value)
    {
        switch (value)
        {
        case AssessmentState::NOT_ASSESSED: return "NOT_ASSESSED";
        case AssessmentState::ACCEPTED: return "ACCEPTED";
        case AssessmentState::REJECTED: return "REJECTED";
        case AssessmentState::NEEDS_REVIEW: return "NEEDS_REVIEW";
        default: return {};
        }
    }
    bool ParseToken(AZStd::string_view token, AssessmentState& output)
    {
        if (token == "NOT_ASSESSED") { output = AssessmentState::NOT_ASSESSED; return true; }
        if (token == "ACCEPTED") { output = AssessmentState::ACCEPTED; return true; }
        if (token == "REJECTED") { output = AssessmentState::REJECTED; return true; }
        if (token == "NEEDS_REVIEW") { output = AssessmentState::NEEDS_REVIEW; return true; }
        return false;
    }

    AZStd::string_view Token(PromotionState value)
    {
        switch (value)
        {
        case PromotionState::NOT_PROMOTED: return "NOT_PROMOTED";
        case PromotionState::CANDIDATE: return "CANDIDATE";
        case PromotionState::PROMOTED: return "PROMOTED";
        case PromotionState::REJECTED: return "REJECTED";
        default: return {};
        }
    }
    bool ParseToken(AZStd::string_view token, PromotionState& output)
    {
        if (token == "NOT_PROMOTED") { output = PromotionState::NOT_PROMOTED; return true; }
        if (token == "CANDIDATE") { output = PromotionState::CANDIDATE; return true; }
        if (token == "PROMOTED") { output = PromotionState::PROMOTED; return true; }
        if (token == "REJECTED") { output = PromotionState::REJECTED; return true; }
        return false;
    }

    AZStd::string_view Token(ReleaseDecisionState value)
    {
        switch (value)
        {
        case ReleaseDecisionState::NOT_DECIDED: return "NOT_DECIDED";
        case ReleaseDecisionState::APPROVED: return "APPROVED";
        case ReleaseDecisionState::REJECTED: return "REJECTED";
        default: return {};
        }
    }
    bool ParseToken(AZStd::string_view token, ReleaseDecisionState& output)
    {
        if (token == "NOT_DECIDED") { output = ReleaseDecisionState::NOT_DECIDED; return true; }
        if (token == "APPROVED") { output = ReleaseDecisionState::APPROVED; return true; }
        if (token == "REJECTED") { output = ReleaseDecisionState::REJECTED; return true; }
        return false;
    }

    AZStd::string_view Token(ArtifactLifecycle value)
    {
        switch (value)
        {
        case ArtifactLifecycle::DECLARED: return "DECLARED";
        case ArtifactLifecycle::PRODUCED: return "PRODUCED";
        case ArtifactLifecycle::VERIFIED: return "VERIFIED";
        case ArtifactLifecycle::STAGED: return "STAGED";
        case ArtifactLifecycle::DEPLOYED: return "DEPLOYED";
        case ArtifactLifecycle::RETIRED: return "RETIRED";
        case ArtifactLifecycle::BACKUP: return "BACKUP";
        default: return {};
        }
    }
    bool ParseToken(AZStd::string_view token, ArtifactLifecycle& output)
    {
        if (token == "DECLARED") { output = ArtifactLifecycle::DECLARED; return true; }
        if (token == "PRODUCED") { output = ArtifactLifecycle::PRODUCED; return true; }
        if (token == "VERIFIED") { output = ArtifactLifecycle::VERIFIED; return true; }
        if (token == "STAGED") { output = ArtifactLifecycle::STAGED; return true; }
        if (token == "DEPLOYED") { output = ArtifactLifecycle::DEPLOYED; return true; }
        if (token == "RETIRED") { output = ArtifactLifecycle::RETIRED; return true; }
        if (token == "BACKUP") { output = ArtifactLifecycle::BACKUP; return true; }
        return false;
    }

    AZStd::string_view Token(RedistributionState value)
    {
        switch (value)
        {
        case RedistributionState::UNKNOWN: return "UNKNOWN";
        case RedistributionState::PERMITTED: return "PERMITTED";
        case RedistributionState::FORBIDDEN: return "FORBIDDEN";
        default: return {};
        }
    }
    bool ParseToken(AZStd::string_view token, RedistributionState& output)
    {
        if (token == "UNKNOWN") { output = RedistributionState::UNKNOWN; return true; }
        if (token == "PERMITTED") { output = RedistributionState::PERMITTED; return true; }
        if (token == "FORBIDDEN") { output = RedistributionState::FORBIDDEN; return true; }
        return false;
    }

    AZStd::string_view Token(PreimagePresence value)
    {
        switch (value)
        {
        case PreimagePresence::ABSENT: return "ABSENT";
        case PreimagePresence::PRESENT: return "PRESENT";
        default: return {};
        }
    }
    bool ParseToken(AZStd::string_view token, PreimagePresence& output)
    {
        if (token == "ABSENT") { output = PreimagePresence::ABSENT; return true; }
        if (token == "PRESENT") { output = PreimagePresence::PRESENT; return true; }
        return false;
    }

    AZStd::string_view Token(MutationOperation value)
    {
        switch (value)
        {
        case MutationOperation::CREATE: return "CREATE";
        case MutationOperation::REPLACE: return "REPLACE";
        case MutationOperation::REMOVE: return "REMOVE";
        default: return {};
        }
    }
    bool ParseToken(AZStd::string_view token, MutationOperation& output)
    {
        if (token == "CREATE") { output = MutationOperation::CREATE; return true; }
        if (token == "REPLACE") { output = MutationOperation::REPLACE; return true; }
        if (token == "REMOVE") { output = MutationOperation::REMOVE; return true; }
        return false;
    }

    AZStd::string_view Token(RollbackAction value)
    {
        switch (value)
        {
        case RollbackAction::REMOVE_CREATED: return "REMOVE_CREATED";
        case RollbackAction::RESTORE_BACKUP: return "RESTORE_BACKUP";
        case RollbackAction::COMPENSATE: return "COMPENSATE";
        default: return {};
        }
    }
    bool ParseToken(AZStd::string_view token, RollbackAction& output)
    {
        if (token == "REMOVE_CREATED") { output = RollbackAction::REMOVE_CREATED; return true; }
        if (token == "RESTORE_BACKUP") { output = RollbackAction::RESTORE_BACKUP; return true; }
        if (token == "COMPENSATE") { output = RollbackAction::COMPENSATE; return true; }
        return false;
    }

    AZStd::string_view Token(CleanupState value)
    {
        switch (value)
        {
        case CleanupState::NOT_REQUIRED: return "NOT_REQUIRED";
        case CleanupState::PENDING: return "PENDING";
        case CleanupState::SUCCEEDED: return "SUCCEEDED";
        case CleanupState::FAILED: return "FAILED";
        case CleanupState::SKIPPED: return "SKIPPED";
        default: return {};
        }
    }
    bool ParseToken(AZStd::string_view token, CleanupState& output)
    {
        if (token == "NOT_REQUIRED") { output = CleanupState::NOT_REQUIRED; return true; }
        if (token == "PENDING") { output = CleanupState::PENDING; return true; }
        if (token == "SUCCEEDED") { output = CleanupState::SUCCEEDED; return true; }
        if (token == "FAILED") { output = CleanupState::FAILED; return true; }
        if (token == "SKIPPED") { output = CleanupState::SKIPPED; return true; }
        return false;
    }

    AZStd::string_view Token(RollbackState value)
    {
        switch (value)
        {
        case RollbackState::NOT_REQUIRED: return "NOT_REQUIRED";
        case RollbackState::NOT_ATTEMPTED: return "NOT_ATTEMPTED";
        case RollbackState::SUCCEEDED: return "SUCCEEDED";
        case RollbackState::FAILED: return "FAILED";
        case RollbackState::PARTIAL: return "PARTIAL";
        case RollbackState::SKIPPED: return "SKIPPED";
        case RollbackState::CANCELLED: return "CANCELLED";
        default: return {};
        }
    }
    bool ParseToken(AZStd::string_view token, RollbackState& output)
    {
        if (token == "NOT_REQUIRED") { output = RollbackState::NOT_REQUIRED; return true; }
        if (token == "NOT_ATTEMPTED") { output = RollbackState::NOT_ATTEMPTED; return true; }
        if (token == "SUCCEEDED") { output = RollbackState::SUCCEEDED; return true; }
        if (token == "FAILED") { output = RollbackState::FAILED; return true; }
        if (token == "PARTIAL") { output = RollbackState::PARTIAL; return true; }
        if (token == "SKIPPED") { output = RollbackState::SKIPPED; return true; }
        if (token == "CANCELLED") { output = RollbackState::CANCELLED; return true; }
        return false;
    }

    AZStd::string_view Token(ExecutionState value)
    {
        switch (value)
        {
        case ExecutionState::DRAFT: return "DRAFT";
        case ExecutionState::VALIDATED: return "VALIDATED";
        case ExecutionState::RESOLVED: return "RESOLVED";
        case ExecutionState::QUALIFIED: return "QUALIFIED";
        case ExecutionState::PLANNED: return "PLANNED";
        case ExecutionState::AWAITING_AUTHORIZATION: return "AWAITING_AUTHORIZATION";
        case ExecutionState::READY: return "READY";
        case ExecutionState::EXECUTING: return "EXECUTING";
        case ExecutionState::VERIFYING: return "VERIFYING";
        case ExecutionState::SUCCEEDED: return "SUCCEEDED";
        case ExecutionState::REJECTED: return "REJECTED";
        case ExecutionState::RESOLUTION_FAILED: return "RESOLUTION_FAILED";
        case ExecutionState::QUALIFICATION_FAILED: return "QUALIFICATION_FAILED";
        case ExecutionState::POLICY_DENIED: return "POLICY_DENIED";
        case ExecutionState::AUTHORIZATION_EXPIRED: return "AUTHORIZATION_EXPIRED";
        case ExecutionState::ENVIRONMENT_DRIFTED: return "ENVIRONMENT_DRIFTED";
        case ExecutionState::FAILED: return "FAILED";
        case ExecutionState::PARTIAL: return "PARTIAL";
        case ExecutionState::CANCELLATION_REQUESTED: return "CANCELLATION_REQUESTED";
        case ExecutionState::CANCELLED: return "CANCELLED";
        case ExecutionState::ROLLBACK_REQUIRED: return "ROLLBACK_REQUIRED";
        case ExecutionState::ROLLING_BACK: return "ROLLING_BACK";
        case ExecutionState::ROLLED_BACK: return "ROLLED_BACK";
        case ExecutionState::ROLLBACK_FAILED: return "ROLLBACK_FAILED";
        case ExecutionState::SUPERSEDED: return "SUPERSEDED";
        case ExecutionState::ARCHIVED: return "ARCHIVED";
        default: return {};
        }
    }
    bool ParseToken(AZStd::string_view token, ExecutionState& output)
    {
        if (token == "DRAFT") { output = ExecutionState::DRAFT; return true; }
        if (token == "VALIDATED") { output = ExecutionState::VALIDATED; return true; }
        if (token == "RESOLVED") { output = ExecutionState::RESOLVED; return true; }
        if (token == "QUALIFIED") { output = ExecutionState::QUALIFIED; return true; }
        if (token == "PLANNED") { output = ExecutionState::PLANNED; return true; }
        if (token == "AWAITING_AUTHORIZATION") { output = ExecutionState::AWAITING_AUTHORIZATION; return true; }
        if (token == "READY") { output = ExecutionState::READY; return true; }
        if (token == "EXECUTING") { output = ExecutionState::EXECUTING; return true; }
        if (token == "VERIFYING") { output = ExecutionState::VERIFYING; return true; }
        if (token == "SUCCEEDED") { output = ExecutionState::SUCCEEDED; return true; }
        if (token == "REJECTED") { output = ExecutionState::REJECTED; return true; }
        if (token == "RESOLUTION_FAILED") { output = ExecutionState::RESOLUTION_FAILED; return true; }
        if (token == "QUALIFICATION_FAILED") { output = ExecutionState::QUALIFICATION_FAILED; return true; }
        if (token == "POLICY_DENIED") { output = ExecutionState::POLICY_DENIED; return true; }
        if (token == "AUTHORIZATION_EXPIRED") { output = ExecutionState::AUTHORIZATION_EXPIRED; return true; }
        if (token == "ENVIRONMENT_DRIFTED") { output = ExecutionState::ENVIRONMENT_DRIFTED; return true; }
        if (token == "FAILED") { output = ExecutionState::FAILED; return true; }
        if (token == "PARTIAL") { output = ExecutionState::PARTIAL; return true; }
        if (token == "CANCELLATION_REQUESTED") { output = ExecutionState::CANCELLATION_REQUESTED; return true; }
        if (token == "CANCELLED") { output = ExecutionState::CANCELLED; return true; }
        if (token == "ROLLBACK_REQUIRED") { output = ExecutionState::ROLLBACK_REQUIRED; return true; }
        if (token == "ROLLING_BACK") { output = ExecutionState::ROLLING_BACK; return true; }
        if (token == "ROLLED_BACK") { output = ExecutionState::ROLLED_BACK; return true; }
        if (token == "ROLLBACK_FAILED") { output = ExecutionState::ROLLBACK_FAILED; return true; }
        if (token == "SUPERSEDED") { output = ExecutionState::SUPERSEDED; return true; }
        if (token == "ARCHIVED") { output = ExecutionState::ARCHIVED; return true; }
        return false;
    }

    AZStd::string_view Token(PhaseState value)
    {
        switch (value)
        {
        case PhaseState::NOT_PLANNED: return "NOT_PLANNED";
        case PhaseState::PENDING: return "PENDING";
        case PhaseState::READY: return "READY";
        case PhaseState::RUNNING: return "RUNNING";
        case PhaseState::SUCCEEDED: return "SUCCEEDED";
        case PhaseState::FAILED: return "FAILED";
        case PhaseState::SKIPPED: return "SKIPPED";
        case PhaseState::BLOCKED: return "BLOCKED";
        case PhaseState::CANCELLED: return "CANCELLED";
        case PhaseState::ROLLBACK_PENDING: return "ROLLBACK_PENDING";
        case PhaseState::ROLLED_BACK: return "ROLLED_BACK";
        case PhaseState::ROLLBACK_FAILED: return "ROLLBACK_FAILED";
        default: return {};
        }
    }
    bool ParseToken(AZStd::string_view token, PhaseState& output)
    {
        if (token == "NOT_PLANNED") { output = PhaseState::NOT_PLANNED; return true; }
        if (token == "PENDING") { output = PhaseState::PENDING; return true; }
        if (token == "READY") { output = PhaseState::READY; return true; }
        if (token == "RUNNING") { output = PhaseState::RUNNING; return true; }
        if (token == "SUCCEEDED") { output = PhaseState::SUCCEEDED; return true; }
        if (token == "FAILED") { output = PhaseState::FAILED; return true; }
        if (token == "SKIPPED") { output = PhaseState::SKIPPED; return true; }
        if (token == "BLOCKED") { output = PhaseState::BLOCKED; return true; }
        if (token == "CANCELLED") { output = PhaseState::CANCELLED; return true; }
        if (token == "ROLLBACK_PENDING") { output = PhaseState::ROLLBACK_PENDING; return true; }
        if (token == "ROLLED_BACK") { output = PhaseState::ROLLED_BACK; return true; }
        if (token == "ROLLBACK_FAILED") { output = PhaseState::ROLLBACK_FAILED; return true; }
        return false;
    }

    AZStd::string_view Token(ContractKind value)
    {
        switch (value)
        {
        case ContractKind::CAPABILITY_DESCRIPTOR: return "CAPABILITY_DESCRIPTOR";
        case ContractKind::CAPABILITY_PROVIDER_BINDING: return "CAPABILITY_PROVIDER_BINDING";
        case ContractKind::ARTIFACT_REFERENCE: return "ARTIFACT_REFERENCE";
        case ContractKind::EXPECTED_ARTIFACT: return "EXPECTED_ARTIFACT";
        case ContractKind::ARTIFACT_RECORD: return "ARTIFACT_RECORD";
        case ContractKind::OPTION: return "OPTION";
        case ContractKind::CAPABILITY_EXECUTION_REQUEST: return "CAPABILITY_EXECUTION_REQUEST";
        case ContractKind::CAPABILITY_SUPPORT_DECISION: return "CAPABILITY_SUPPORT_DECISION";
        case ContractKind::CAPABILITY_QUALIFICATION_DECISION: return "CAPABILITY_QUALIFICATION_DECISION";
        case ContractKind::CAPABILITY_ENVIRONMENT_DECISION: return "CAPABILITY_ENVIRONMENT_DECISION";
        case ContractKind::CAPABILITY_POLICY_DECISION: return "CAPABILITY_POLICY_DECISION";
        case ContractKind::CAPABILITY_AUTHORIZATION_RECEIPT: return "CAPABILITY_AUTHORIZATION_RECEIPT";
        case ContractKind::TARGET_MUTATION_CLAIM: return "TARGET_MUTATION_CLAIM";
        case ContractKind::ROLLBACK_STEP: return "ROLLBACK_STEP";
        case ContractKind::ROLLBACK_PLAN: return "ROLLBACK_PLAN";
        case ContractKind::CAPABILITY_PHASE_PLAN: return "CAPABILITY_PHASE_PLAN";
        case ContractKind::CAPABILITY_EXECUTION_PLAN: return "CAPABILITY_EXECUTION_PLAN";
        case ContractKind::FAILURE_RECORD: return "FAILURE_RECORD";
        case ContractKind::DIAGNOSTIC_REFERENCE: return "DIAGNOSTIC_REFERENCE";
        case ContractKind::TARGET_OBSERVATION: return "TARGET_OBSERVATION";
        case ContractKind::PHASE_EXTENSION_REFERENCE: return "PHASE_EXTENSION_REFERENCE";
        case ContractKind::CAPABILITY_PHASE_RECEIPT: return "CAPABILITY_PHASE_RECEIPT";
        case ContractKind::ROLLBACK_STEP_RECEIPT: return "ROLLBACK_STEP_RECEIPT";
        case ContractKind::ROLLBACK_RECEIPT: return "ROLLBACK_RECEIPT";
        case ContractKind::CAPABILITY_EXECUTION_RECEIPT: return "CAPABILITY_EXECUTION_RECEIPT";
        default: return {};
        }
    }
    bool ParseToken(AZStd::string_view token, ContractKind& output)
    {
        if (token == "CAPABILITY_DESCRIPTOR") { output = ContractKind::CAPABILITY_DESCRIPTOR; return true; }
        if (token == "CAPABILITY_PROVIDER_BINDING") { output = ContractKind::CAPABILITY_PROVIDER_BINDING; return true; }
        if (token == "ARTIFACT_REFERENCE") { output = ContractKind::ARTIFACT_REFERENCE; return true; }
        if (token == "EXPECTED_ARTIFACT") { output = ContractKind::EXPECTED_ARTIFACT; return true; }
        if (token == "ARTIFACT_RECORD") { output = ContractKind::ARTIFACT_RECORD; return true; }
        if (token == "OPTION") { output = ContractKind::OPTION; return true; }
        if (token == "CAPABILITY_EXECUTION_REQUEST") { output = ContractKind::CAPABILITY_EXECUTION_REQUEST; return true; }
        if (token == "CAPABILITY_SUPPORT_DECISION") { output = ContractKind::CAPABILITY_SUPPORT_DECISION; return true; }
        if (token == "CAPABILITY_QUALIFICATION_DECISION") { output = ContractKind::CAPABILITY_QUALIFICATION_DECISION; return true; }
        if (token == "CAPABILITY_ENVIRONMENT_DECISION") { output = ContractKind::CAPABILITY_ENVIRONMENT_DECISION; return true; }
        if (token == "CAPABILITY_POLICY_DECISION") { output = ContractKind::CAPABILITY_POLICY_DECISION; return true; }
        if (token == "CAPABILITY_AUTHORIZATION_RECEIPT") { output = ContractKind::CAPABILITY_AUTHORIZATION_RECEIPT; return true; }
        if (token == "TARGET_MUTATION_CLAIM") { output = ContractKind::TARGET_MUTATION_CLAIM; return true; }
        if (token == "ROLLBACK_STEP") { output = ContractKind::ROLLBACK_STEP; return true; }
        if (token == "ROLLBACK_PLAN") { output = ContractKind::ROLLBACK_PLAN; return true; }
        if (token == "CAPABILITY_PHASE_PLAN") { output = ContractKind::CAPABILITY_PHASE_PLAN; return true; }
        if (token == "CAPABILITY_EXECUTION_PLAN") { output = ContractKind::CAPABILITY_EXECUTION_PLAN; return true; }
        if (token == "FAILURE_RECORD") { output = ContractKind::FAILURE_RECORD; return true; }
        if (token == "DIAGNOSTIC_REFERENCE") { output = ContractKind::DIAGNOSTIC_REFERENCE; return true; }
        if (token == "TARGET_OBSERVATION") { output = ContractKind::TARGET_OBSERVATION; return true; }
        if (token == "PHASE_EXTENSION_REFERENCE") { output = ContractKind::PHASE_EXTENSION_REFERENCE; return true; }
        if (token == "CAPABILITY_PHASE_RECEIPT") { output = ContractKind::CAPABILITY_PHASE_RECEIPT; return true; }
        if (token == "ROLLBACK_STEP_RECEIPT") { output = ContractKind::ROLLBACK_STEP_RECEIPT; return true; }
        if (token == "ROLLBACK_RECEIPT") { output = ContractKind::ROLLBACK_RECEIPT; return true; }
        if (token == "CAPABILITY_EXECUTION_RECEIPT") { output = ContractKind::CAPABILITY_EXECUTION_RECEIPT; return true; }
        return false;
    }

}
