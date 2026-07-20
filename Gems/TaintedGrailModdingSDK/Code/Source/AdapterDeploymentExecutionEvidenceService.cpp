/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterDeploymentExecutionEvidenceService.h"

#include "AdapterContractRegistry.h"

#include <AzCore/std/algorithm.h>
#include <AzCore/std/sort.h>
#include <AzCore/std/utility/move.h>

#include <cstddef>

#include "AdapterDeploymentExecutionEvidenceServicePart1.inl"
#include "AdapterDeploymentExecutionEvidenceServicePart2.inl"

// Part 1 owns the validation subject helpers. Part 3 builds returned evidence
// and intentionally uses distinct internal symbols so all split implementation
// files compile in one translation unit without duplicate definitions.
#define ResultSubject ExecutionEvidenceResultSubject
#define WorkOrderSubject ExecutionEvidenceWorkOrderSubject
#define StepSubject ExecutionEvidenceStepSubject
#define RollbackSubject ExecutionEvidenceRollbackSubject
#include "AdapterDeploymentExecutionEvidenceServicePart3.inl"
#undef RollbackSubject
#undef StepSubject
#undef WorkOrderSubject
#undef ResultSubject
