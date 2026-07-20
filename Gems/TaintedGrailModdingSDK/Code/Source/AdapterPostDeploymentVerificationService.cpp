/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterPostDeploymentVerificationService.h"

#include "AdapterDeploymentExecutionResultCanonical.h"
#include "CanonicalFingerprint.h"
#include "ResearchContractValidation.h"

#include <AzCore/std/algorithm.h>
#include <AzCore/std/sort.h>
#include <AzCore/std/utility/move.h>

#include <cstddef>

#include "AdapterPostDeploymentVerificationServicePart1.inl"
#include "AdapterPostDeploymentVerificationServicePart2.inl"
#include "AdapterPostDeploymentVerificationServicePart3.inl"
