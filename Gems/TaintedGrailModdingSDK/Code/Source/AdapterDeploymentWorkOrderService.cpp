/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterDeploymentWorkOrderService.h"
#include "CanonicalFingerprint.h"

#include <AzCore/std/algorithm.h>
#include <AzCore/std/sort.h>
#include <AzCore/std/utility/move.h>

#include <cstddef>
#include <initializer_list>
#include <locale>
#include <sstream>

#include "AdapterDeploymentWorkOrderServicePart1.inl"
#include "AdapterDeploymentWorkOrderServicePart2.inl"
#include "AdapterDeploymentWorkOrderServicePart3.inl"
