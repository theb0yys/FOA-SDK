namespace TaintedGrailModdingSDK
{
    namespace
    {
        thread_local const SourceEvidenceRegistry* s_verifierReviewSourceRegistry = nullptr;

        void ValidateVerifierReview(
            const AdapterDeploymentWorkOrder& workOrder,
            const AdapterPostDeploymentVerifierResultEnvelope& envelope,
            AdapterPostDeploymentVerifierEvidenceReturn& result,
            VerifierValidationFlags& flags)
        {
            if (!s_verifierReviewSourceRegistry)
            {
                AddVerifierIssue(
                    result,
                    flags.m_verifierUnreviewed,
                    "post_deployment_verifier.review_registry_missing",
                    "Verifier review validation requires the exact active source/evidence registry; unbound review evidence is rejected.");
                return;
            }
            ValidateVerifierReview(
                workOrder,
                *s_verifierReviewSourceRegistry,
                envelope,
                result,
                flags);
        }

        class VerifierReviewRegistryScope final
        {
        public:
            explicit VerifierReviewRegistryScope(
                const SourceEvidenceRegistry& sourceRegistry)
                : m_previous(s_verifierReviewSourceRegistry)
            {
                s_verifierReviewSourceRegistry = &sourceRegistry;
            }

            ~VerifierReviewRegistryScope()
            {
                s_verifierReviewSourceRegistry = m_previous;
            }

        private:
            const SourceEvidenceRegistry* m_previous = nullptr;
        };
    } // namespace

    AdapterPostDeploymentVerifierEvidenceReturn
    AdapterPostDeploymentVerifierEvidenceService::BuildEvidenceReturn(
        const AdapterDeploymentWorkOrder& workOrder,
        const AdapterDeploymentExecutionResultEnvelope& executionEnvelope,
        const AdapterPostDeploymentVerificationReport& report,
        const SourceEvidenceRegistry& sourceRegistry,
        const AdapterPostDeploymentVerifierResultEnvelope& verifierEnvelope) const
    {
        const VerifierReviewRegistryScope scope(sourceRegistry);
        return BuildEvidenceReturn(
            workOrder,
            executionEnvelope,
            report,
            verifierEnvelope);
    }
} // namespace TaintedGrailModdingSDK
