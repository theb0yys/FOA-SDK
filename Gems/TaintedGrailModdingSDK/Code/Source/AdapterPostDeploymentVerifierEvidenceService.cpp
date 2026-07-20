/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterPostDeploymentVerifierEvidenceService.h"

#include "AdapterContractRegistry.h"
#include "AdapterDeploymentExecutionResultCanonical.h"
#include "AdapterPostDeploymentVerifierResultCanonical.h"
#include "CanonicalFingerprint.h"
#include "ResearchContractValidation.h"

#include <AzCore/std/algorithm.h>
#include <AzCore/std/sort.h>
#include <AzCore/std/utility/move.h>

#include <cstddef>

#include "AdapterPostDeploymentVerifierEvidenceServicePart1.inl"
#include "AdapterPostDeploymentVerifierEvidenceServicePart2.inl"
#include "AdapterPostDeploymentVerifierEvidenceServicePart3.inl"
#include "AdapterPostDeploymentVerifierEvidenceServicePart4.inl"
#include "AdapterPostDeploymentVerifierEvidenceServicePart6.inl"
#include "AdapterPostDeploymentVerifierEvidenceServicePart5.inl"
