namespace TaintedGrailModdingSDK
{
    namespace
    {
        template<class EnumType>
        struct EnumName
        {
            EnumType m_value;
            const char* m_name;
        };

        constexpr EnumName<AdapterDeploymentConfirmationDecision> ConfirmationDecisions[] = {
            { AdapterDeploymentConfirmationDecision::Unknown, "unknown" },
            { AdapterDeploymentConfirmationDecision::Confirmed, "confirmed" },
            { AdapterDeploymentConfirmationDecision::Rejected, "rejected" },
        };

        constexpr EnumName<AdapterDeploymentConfirmationScope> ConfirmationScopes[] = {
            { AdapterDeploymentConfirmationScope::AdditionsOnly, "additions_only" },
            { AdapterDeploymentConfirmationScope::AdditionsAndReplacements,
                "additions_and_replacements" },
            { AdapterDeploymentConfirmationScope::FullPreview, "full_preview" },
        };

        constexpr EnumName<AdapterDeploymentPreflightKind> PreflightKinds[] = {
            { AdapterDeploymentPreflightKind::PackageIntegrity, "package_integrity" },
            { AdapterDeploymentPreflightKind::TargetInventory, "target_inventory" },
            { AdapterDeploymentPreflightKind::BackupReadiness, "backup_readiness" },
            { AdapterDeploymentPreflightKind::RollbackReadiness, "rollback_readiness" },
            { AdapterDeploymentPreflightKind::OperatorReadiness, "operator_readiness" },
        };

        constexpr EnumName<AdapterDeploymentPreflightStatus> PreflightStatuses[] = {
            { AdapterDeploymentPreflightStatus::Unknown, "unknown" },
            { AdapterDeploymentPreflightStatus::Passed, "passed" },
            { AdapterDeploymentPreflightStatus::Failed, "failed" },
        };

        constexpr EnumName<AdapterDeploymentWorkOrderStepKind> WorkOrderStepKinds[] = {
            { AdapterDeploymentWorkOrderStepKind::VerifyPreflight, "verify_preflight" },
            { AdapterDeploymentWorkOrderStepKind::ConfirmMaintenanceWindow,
                "confirm_maintenance_window" },
            { AdapterDeploymentWorkOrderStepKind::Backup, "backup" },
            { AdapterDeploymentWorkOrderStepKind::Add, "add" },
            { AdapterDeploymentWorkOrderStepKind::Replace, "replace" },
            { AdapterDeploymentWorkOrderStepKind::Remove, "remove" },
            { AdapterDeploymentWorkOrderStepKind::VerifyDeployment, "verify_deployment" },
            { AdapterDeploymentWorkOrderStepKind::PreserveRollback, "preserve_rollback" },
        };

        constexpr EnumName<AdapterDeploymentChecklistState> ChecklistStates[] = {
            { AdapterDeploymentChecklistState::ContractSatisfied, "contract_satisfied" },
            { AdapterDeploymentChecklistState::OperatorActionRequired,
                "operator_action_required" },
            { AdapterDeploymentChecklistState::Blocked, "blocked" },
        };

        constexpr EnumName<AdapterDeploymentWorkOrderStatus> WorkOrderStatuses[] = {
            { AdapterDeploymentWorkOrderStatus::ReviewReady, "review_ready" },
            { AdapterDeploymentWorkOrderStatus::PreviewNotReady, "preview_not_ready" },
            { AdapterDeploymentWorkOrderStatus::ConfirmationMissing, "confirmation_missing" },
            { AdapterDeploymentWorkOrderStatus::ConfirmationRejected, "confirmation_rejected" },
            { AdapterDeploymentWorkOrderStatus::ConfirmationBindingMismatch,
                "confirmation_binding_mismatch" },
            { AdapterDeploymentWorkOrderStatus::ScopeMismatch, "scope_mismatch" },
            { AdapterDeploymentWorkOrderStatus::ConfirmationExpired, "confirmation_expired" },
            { AdapterDeploymentWorkOrderStatus::MaintenanceWindowInvalid,
                "maintenance_window_invalid" },
            { AdapterDeploymentWorkOrderStatus::OutsideMaintenanceWindow,
                "outside_maintenance_window" },
            { AdapterDeploymentWorkOrderStatus::PreflightMissing, "preflight_missing" },
            { AdapterDeploymentWorkOrderStatus::PreflightFailed, "preflight_failed" },
            { AdapterDeploymentWorkOrderStatus::WorkOrderIncomplete, "work_order_incomplete" },
        };

        template<class EnumType, size_t Count>
        AZStd::string EnumToString(
            EnumType value,
            const EnumName<EnumType> (&names)[Count])
        {
            for (const EnumName<EnumType>& name : names)
            {
                if (name.m_value == value)
                {
                    return name.m_name;
                }
            }
            return "unknown";
        }

        template<class EnumType, size_t Count>
        bool TryParseEnum(
            const AZStd::string& value,
            EnumType& result,
            const EnumName<EnumType> (&names)[Count])
        {
            for (const EnumName<EnumType>& name : names)
            {
                if (value == name.m_name)
                {
                    result = name.m_value;
                    return true;
                }
            }
            return false;
        }

        bool IsLowerHex(char value)
        {
            return (value >= '0' && value <= '9')
                || (value >= 'a' && value <= 'f');
        }

        bool IsLowerAlphaNumeric(char value)
        {
            return (value >= 'a' && value <= 'z')
                || (value >= '0' && value <= '9');
        }

        bool IsSha256Fingerprint(const AZStd::string& value)
        {
            constexpr size_t PrefixLength = 7;
            constexpr size_t DigestLength = 64;
            if (value.size() != PrefixLength + DigestLength
                || value.substr(0, PrefixLength) != "sha256:")
            {
                return false;
            }
            for (size_t index = PrefixLength; index < value.size(); ++index)
            {
                if (!IsLowerHex(value[index]))
                {
                    return false;
                }
            }
            return true;
        }

        bool IsStableNamespacedId(const AZStd::string& value)
        {
            if (value.size() < 3 || value.find('.') == AZStd::string::npos
                || !IsLowerAlphaNumeric(value.front())
                || !IsLowerAlphaNumeric(value.back()))
            {
                return false;
            }
            for (char character : value)
            {
                if (!IsLowerAlphaNumeric(character)
                    && character != '.'
                    && character != '-'
                    && character != '_')
                {
                    return false;
                }
            }
            return true;
        }

        bool IsDigit(char value)
        {
            return value >= '0' && value <= '9';
        }

        int TwoDigitValue(const AZStd::string& value, size_t offset)
        {
            return (value[offset] - '0') * 10 + (value[offset + 1] - '0');
        }

        int FourDigitValue(const AZStd::string& value, size_t offset)
        {
            return (value[offset] - '0') * 1000
                + (value[offset + 1] - '0') * 100
                + (value[offset + 2] - '0') * 10
                + (value[offset + 3] - '0');
        }

        bool IsLeapYear(int year)
        {
            return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
        }

        int DaysInMonth(int year, int month)
        {
            constexpr int Days[] = {
                0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
            };
            if (month < 1 || month > 12)
            {
                return 0;
            }
            if (month == 2 && IsLeapYear(year))
            {
                return 29;
            }
            return Days[month];
        }

        bool IsUtcTimestamp(const AZStd::string& value)
        {
            if (value.size() != 20
                || value[4] != '-'
                || value[7] != '-'
                || value[10] != 'T'
                || value[13] != ':'
                || value[16] != ':'
                || value[19] != 'Z')
            {
                return false;
            }
            constexpr size_t DigitPositions[] = {
                0, 1, 2, 3, 5, 6, 8, 9, 11, 12, 14, 15, 17, 18
            };
            for (size_t index : DigitPositions)
            {
                if (!IsDigit(value[index]))
                {
                    return false;
                }
            }
            const int year = FourDigitValue(value, 0);
            const int month = TwoDigitValue(value, 5);
            const int day = TwoDigitValue(value, 8);
            const int hour = TwoDigitValue(value, 11);
            const int minute = TwoDigitValue(value, 14);
            const int second = TwoDigitValue(value, 17);
            return year >= 1
                && month >= 1 && month <= 12
                && day >= 1 && day <= DaysInMonth(year, month)
                && hour >= 0 && hour <= 23
                && minute >= 0 && minute <= 59
                && second >= 0 && second <= 59;
        }

        AZStd::string ToUnsignedString(AZ::u64 value)
        {
            std::ostringstream stream;
            stream.imbue(std::locale::classic());
            stream << static_cast<unsigned long long>(value);
            return stream.str().c_str();
        }

        void SortUnique(AZStd::vector<AZStd::string>& values)
        {
            AZStd::sort(values.begin(), values.end());
            values.erase(AZStd::unique(values.begin(), values.end()), values.end());
        }

        void AddBlocker(
            AdapterDeploymentWorkOrder& workOrder,
            AZStd::string code,
            AZStd::string subject,
            AZStd::string reason)
        {
            for (const AdapterDeploymentWorkOrderBlocker& blocker :
                 workOrder.m_blockers)
            {
                if (blocker.m_code == code
                    && blocker.m_subject == subject
                    && blocker.m_reason == reason)
                {
                    return;
                }
            }
            AdapterDeploymentWorkOrderBlocker blocker;
            blocker.m_code = AZStd::move(code);
            blocker.m_subject = AZStd::move(subject);
            blocker.m_reason = AZStd::move(reason);
            workOrder.m_blockers.push_back(AZStd::move(blocker));
        }

        AZStd::vector<AZStd::string> CollectEvidence(
            const AdapterDeploymentWorkOrderRequest& request)
        {
            AZStd::vector<AZStd::string> evidence =
                request.m_confirmation.m_evidenceIds;
            evidence.insert(
                evidence.end(),
                request.m_maintenanceWindow.m_evidenceIds.begin(),
                request.m_maintenanceWindow.m_evidenceIds.end());
            for (const AdapterDeploymentPreflightEvidence& preflight :
                 request.m_preflightEvidence)
            {
                evidence.insert(
                    evidence.end(),
                    preflight.m_evidenceIds.begin(),
                    preflight.m_evidenceIds.end());
            }
            SortUnique(evidence);
            return evidence;
        }

        void SortWorkOrderCollections(AdapterDeploymentWorkOrder& workOrder)
        {
            AZStd::sort(
                workOrder.m_steps.begin(),
                workOrder.m_steps.end(),
                [](const AdapterDeploymentWorkOrderStep& left,
                    const AdapterDeploymentWorkOrderStep& right)
                {
                    return left.m_sequence < right.m_sequence;
                });
            AZStd::sort(
                workOrder.m_checklist.begin(),
                workOrder.m_checklist.end(),
                [](const AdapterDeploymentOperatorChecklistItem& left,
                    const AdapterDeploymentOperatorChecklistItem& right)
                {
                    return left.m_sequence < right.m_sequence;
                });
            AZStd::sort(
                workOrder.m_blockers.begin(),
                workOrder.m_blockers.end(),
                [](const AdapterDeploymentWorkOrderBlocker& left,
                    const AdapterDeploymentWorkOrderBlocker& right)
                {
                    if (left.m_code != right.m_code)
                    {
                        return left.m_code < right.m_code;
                    }
                    if (left.m_subject != right.m_subject)
                    {
                        return left.m_subject < right.m_subject;
                    }
                    return left.m_reason < right.m_reason;
                });
        }
    } // namespace

    AZStd::string ToString(AdapterDeploymentConfirmationDecision decision)
    {
        return EnumToString(decision, ConfirmationDecisions);
    }

    AZStd::string ToString(AdapterDeploymentConfirmationScope scope)
    {
        return EnumToString(scope, ConfirmationScopes);
    }

    AZStd::string ToString(AdapterDeploymentPreflightKind kind)
    {
        return EnumToString(kind, PreflightKinds);
    }

    AZStd::string ToString(AdapterDeploymentPreflightStatus status)
    {
        return EnumToString(status, PreflightStatuses);
    }

    AZStd::string ToString(AdapterDeploymentWorkOrderStepKind kind)
    {
        return EnumToString(kind, WorkOrderStepKinds);
    }

    AZStd::string ToString(AdapterDeploymentChecklistState state)
    {
        return EnumToString(state, ChecklistStates);
    }

    AZStd::string ToString(AdapterDeploymentWorkOrderStatus status)
    {
        return EnumToString(status, WorkOrderStatuses);
    }

    bool TryParseAdapterDeploymentConfirmationDecision(
        const AZStd::string& value,
        AdapterDeploymentConfirmationDecision& decision)
    {
        return TryParseEnum(value, decision, ConfirmationDecisions);
    }

    bool TryParseAdapterDeploymentConfirmationScope(
        const AZStd::string& value,
        AdapterDeploymentConfirmationScope& scope)
    {
        return TryParseEnum(value, scope, ConfirmationScopes);
    }

    bool TryParseAdapterDeploymentPreflightKind(
        const AZStd::string& value,
        AdapterDeploymentPreflightKind& kind)
    {
        return TryParseEnum(value, kind, PreflightKinds);
    }

    bool TryParseAdapterDeploymentPreflightStatus(
        const AZStd::string& value,
        AdapterDeploymentPreflightStatus& status)
    {
        return TryParseEnum(value, status, PreflightStatuses);
    }

    bool TryParseAdapterDeploymentWorkOrderStepKind(
        const AZStd::string& value,
        AdapterDeploymentWorkOrderStepKind& kind)
    {
        return TryParseEnum(value, kind, WorkOrderStepKinds);
    }

    bool TryParseAdapterDeploymentChecklistState(
        const AZStd::string& value,
        AdapterDeploymentChecklistState& state)
    {
        return TryParseEnum(value, state, ChecklistStates);
    }

    bool TryParseAdapterDeploymentWorkOrderStatus(
        const AZStd::string& value,
        AdapterDeploymentWorkOrderStatus& status)
    {
        return TryParseEnum(value, status, WorkOrderStatuses);
    }
} // namespace TaintedGrailModdingSDK
