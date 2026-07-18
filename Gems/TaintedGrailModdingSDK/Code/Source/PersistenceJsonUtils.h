/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Outcome/Outcome.h>
#include <AzCore/Serialization/Json/JsonSerialization.h>
#include <AzCore/Serialization/Json/JsonUtils.h>
#include <AzCore/std/string/string.h>

namespace TaintedGrailModdingSDK::PersistenceJsonUtils
{
    template<class ObjectType>
    AZ::Outcome<void, AZStd::string> LoadObjectFromFile(
        ObjectType& object,
        const AZStd::string& filePath)
    {
        const auto jsonResult = AZ::JsonSerializationUtils::ReadJsonFile(filePath);
        if (!jsonResult.IsSuccess())
        {
            return AZ::Failure(AZStd::string(jsonResult.GetError()));
        }

        const rapidjson::Document& document = jsonResult.GetValue();
        if (document.IsObject() && document.HasMember("Type"))
        {
            return AZ::JsonSerializationUtils::LoadObjectFromFile(object, filePath);
        }

        AZ::JsonDeserializerSettings settings;
        AZ::ComponentApplicationBus::BroadcastResult(
            settings.m_serializeContext,
            &AZ::ComponentApplicationBus::Events::GetSerializeContext);
        AZ::ComponentApplicationBus::BroadcastResult(
            settings.m_registrationContext,
            &AZ::ComponentApplicationBus::Events::GetJsonRegistrationContext);
        settings.m_clearContainers = true;
        if (!settings.m_serializeContext || !settings.m_registrationContext)
        {
            return AZ::Failure(AZStd::string(
                "The O3DE serialization contexts are unavailable for persistence loading."));
        }

        const AZ::JsonSerializationResult::ResultCode result =
            AZ::JsonSerialization::Load(object, document, settings);
        if (result.GetProcessing() != AZ::JsonSerializationResult::Processing::Completed)
        {
            return AZ::Failure(result.ToString(filePath));
        }
        return AZ::Success();
    }
} // namespace TaintedGrailModdingSDK::PersistenceJsonUtils
