/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include "CatalogDatabase.h"

#include <AzCore/Outcome/Outcome.h>
#include <AzCore/std/functional.h>

namespace TaintedGrailModdingSDK
{
    struct CatalogCommitResult
    {
        CatalogDatabase m_catalog;
        AZStd::string m_filePath;
    };

    class CatalogTransactionService
    {
    public:
        using SaveCallback = AZStd::function<AZ::Outcome<AZStd::string, AZStd::string>(
            const CatalogDocument&,
            const AZStd::string&)>;

        AZ::Outcome<CatalogCommitResult, AZStd::string> Commit(
            const CatalogDatabase& candidate,
            const WorkspaceModel& workspace,
            const GameProfile& profile,
            const SaveCallback& save) const;
    };
} // namespace TaintedGrailModdingSDK
