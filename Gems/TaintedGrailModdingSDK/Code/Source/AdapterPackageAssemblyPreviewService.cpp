/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterPackageAssemblyPreviewService.h"

#include "CanonicalFingerprint.h"
#include "PackagePathValidation.h"

#include <AzCore/std/algorithm.h>
#include <AzCore/std/utility/move.h>

#include <cstddef>
#include <locale>
#include <sstream>

#include "AdapterPackageAssemblyPreviewServicePart1.inl"
#include "AdapterPackageAssemblyPreviewServicePart2.inl"
#include "AdapterPackageAssemblyPreviewServicePart3.inl"
