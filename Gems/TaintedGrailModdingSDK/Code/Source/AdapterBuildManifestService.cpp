/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "AdapterBuildManifestService.h"
#include "CanonicalFingerprint.h"
#include "PackagePathValidation.h"

#include <AzCore/std/algorithm.h>
#include <AzCore/std/utility/move.h>

#include <cstddef>
#include <initializer_list>
#include <locale>
#include <sstream>

#define IsSafeRelativePath LegacyBuildManifestIsSafeRelativePath
#define PathIsInsideRoot LegacyBuildManifestPathIsInsideRoot
#define HasDuplicateOutputPaths LegacyBuildManifestHasDuplicateOutputPaths
#define SortOutputs LegacyBuildManifestSortOutputs
#include "AdapterBuildManifestServicePart1.inl"
#undef SortOutputs
#undef HasDuplicateOutputPaths
#undef PathIsInsideRoot
#undef IsSafeRelativePath
#include "AdapterBuildManifestPathPolicy.inl"
#include "AdapterBuildManifestServicePart2.inl"
#include "AdapterBuildManifestServicePart3.inl"
