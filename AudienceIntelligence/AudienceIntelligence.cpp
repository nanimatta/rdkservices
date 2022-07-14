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

#include "AudienceIntelligence.h"
#include "utils.h"
#include <string>
#include <memory>
#include <iostream>
#include <mutex>
#include <thread>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <syscall.h>

bool ACRModeEnabled = false;
bool LARModeEnabled = false;

namespace WPEFramework
{
    namespace Plugin
    {

	AudienceIntelligence::AudienceIntelligence()
        : AbstractPlugin()
        {
	    LOGINFO("ctor");
            AudienceIntelligence::_instance = this;

            registerMethod("getLogLevel", &AudienceIntelligence::getLogLevelWrapper, this);
            registerMethod("setLogLevel", &AudienceIntelligence::setLogLevelWrapper, this);
            registerMethod("enableLAR", &AudienceIntelligence::enableLAR, this);
            registerMethod("enableACR", &AudienceIntelligence::enableACR, this);
            registerMethod("setACRFrequency", &AudienceIntelligence::setACRFrequency, this);
        }

	// Registered methods begin
        uint32_t AudienceIntelligence::getLogLevelWrapper(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
	    bool result = true;
            std::string logLevel = "INFO";
            //todo
            if (false == result) {
                response["message"] = "failed to get log level";
            }
            else {
                response["logLevel"] = logLevel;
            }
            returnResponse(result);
        }
        
	uint32_t AudienceIntelligence::setLogLevelWrapper(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
	    bool result = true;
            if (!parameters.HasLabel("logLevel"))
            {
                result = false;
                response["message"] = "please specify log level (logLevel = DEBUG/INFO/WARN/ERROR/FATAL)";
            }
            if (result)
            {
                std::string logLevel  = parameters["logLevel"].String();
                //todo
                if (false == result) {
                    response["message"] = "failed to set log level";
                }
                else
                {
                    response["logLevel"] = logLevel;
                }
            }
	}
        
	uint32_t AudienceIntelligence::enableACR(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
	    bool result = true;

            if (!parameters.HasLabel("enableACR"))
            {
                result = false;
                response["message"] = "please specify enable parameter";
            }
            if (result)
            {
                const bool enable  = parameters["enableACR"].Boolean();
                ACRModeEnabled = result = enable;
            }
            returnResponse(result);
        }
        
	uint32_t AudienceIntelligence::enableLAR(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
	    bool result = true;

            if (!parameters.HasLabel("enableLAR"))
            {
                result = false;
                response["message"] = "please specify enable parameter";
            }
            if (result)
            {
                const bool enable  = parameters["enableLAR"].Boolean();
                LARModeEnabled = result = enable;
            }
            returnResponse(result);
        }
        
	uint32_t AudienceIntelligence::setACRFrequency(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
        }

       AudienceIntelligence::~AudienceIntelligence()
        {
        }

       void AudienceIntelligence::Deinitialize(PluginHost::IShell* /* service */)
        {
            AudienceIntelligence::_instance = nullptr;

        }

    } // namespace Plugin
} // namespace WPEFramework
 
