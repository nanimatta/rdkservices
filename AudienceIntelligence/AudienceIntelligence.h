/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2019 RDK Management
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
**/

#pragma once

#include <thread>
#include <mutex>
#include "Module.h"
#include "utils.h"
#include "AbstractPlugin.h"

namespace WPEFramework {

    namespace Plugin {

        class AudienceIntelligence : public AbstractPlugin {
        private:

            // We do not allow this plugin to be copied !!
            AudienceIntelligence(const AudienceIntelligence&) = delete;
            AudienceIntelligence& operator=(const AudienceIntelligence&) = delete;

            //Begin methods
            uint32_t getLogLevelWrapper(const JsonObject& parameters, JsonObject& response);
            uint32_t setLogLevelWrapper(const JsonObject& parameters, JsonObject& response);
            uint32_t enableLAR(const JsonObject& parameters, JsonObject& response);
            uint32_t enableACR(const JsonObject& parameters, JsonObject& response);
            uint32_t setACRFrequency(const JsonObject& parameters, JsonObject& response);
            //End methods

            //Begin events
            
	    //End events

        public:
            AudienceIntelligence();
            virtual ~AudienceIntelligence();
            virtual void Deinitialize(PluginHost::IShell* service) override;

        public:
            static AudienceIntelligence* _instance;
        };
	} // namespace Plugin
} // namespace WPEFramework
