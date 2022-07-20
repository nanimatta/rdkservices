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

#include "UtilsJsonRpc.h"
#include "UtilsIarm.h"

#include <string>
#include <memory>
#include <iostream>
#include <mutex>
#include <thread>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <syscall.h>

#define API_VERSION_NUMBER_MAJOR 1
#define API_VERSION_NUMBER_MINOR 0
#define API_VERSION_NUMBER_PATCH 0

bool ACRModeEnabled = false;
bool LARModeEnabled = false;

namespace WPEFramework
{
    namespace Plugin
    {

	SERVICE_REGISTRATION(AudienceIntelligence, API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH);
	AudienceIntelligence* AudienceIntelligence::_instance = nullptr;

	AudienceIntelligence::AudienceIntelligence()
        : PluginHost::JSONRPC()
        {
	    LOGINFO("ctor");

            Register("getLogLevel", &AudienceIntelligence::getLogLevel, this);
            Register("setLogLevel", &AudienceIntelligence::setLogLevel, this);
            Register("enableLAR", &AudienceIntelligence::enableLAR, this);
            Register("enableACR", &AudienceIntelligence::enableACR, this);
            Register("setACRFrequency", &AudienceIntelligence::setACRFrequency, this);
        }

	const string AudienceIntelligence::Initialize(PluginHost::IShell* service)
        {
	    LOGWARN("Initlaizing AudienceIntelligence");
            AudienceIntelligence::_instance = this;
            InitializeIARM();
            return (string());
        }

	void AudienceIntelligence::Deinitialize(PluginHost::IShell * /* service */)
        {
	    LOGWARN("DeInitlaizing AudienceIntelligence");
            DeinitializeIARM();
            AudienceIntelligence::_instance = nullptr;
        }

        string AudienceIntelligence::Information() const
        {
            return (string());
        }

	void AudienceIntelligence::InitializeIARM()
        {
           LOGWARN("AudienceIntelligence::InitializeIARM");
	   if(Utils::IARM::init())
            {
                    IARM_Result_t res;
            }
        }

       void AudienceIntelligence::DeinitializeIARM()
        {
           LOGWARN("AudienceIntelligence::DeinitializeIARM");
	   if(Utils::IARM::isConnected())
            {
                    IARM_Result_t res;
            }

        }

	// Registered methods begin
        uint32_t AudienceIntelligence::getLogLevel(const JsonObject& parameters, JsonObject& response)
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
        
	uint32_t AudienceIntelligence::setLogLevel(const JsonObject& parameters, JsonObject& response)
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

            if (!parameters.HasLabel("enable"))
            {
                result = false;
                response["message"] = "please specify enable parameter";
            }
            if (result)
            {
                ACRModeEnabled = parameters["enable"].Boolean();
                    if (ACRModeEnabled)
                        response["message"] = "ACR feature enabled";
                    else
                        response["message"] = "ACR feature disabled";
            }
            returnResponse(ACRModeEnabled);
        }
        
	uint32_t AudienceIntelligence::enableLAR(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
	    bool result = true;

            if (!parameters.HasLabel("enable"))
            {
                result = false;
                response["message"] = "please specify enable parameter";
            }
            if (result)
            {
                LARModeEnabled = parameters["enable"].Boolean();
                    if (LARModeEnabled)
                        response["message"] = "LAR feature enabled";
                    else
                        response["message"] = "LAR feature disabled";
            }
            returnResponse(LARModeEnabled);
        }
        
	uint32_t AudienceIntelligence::setACRFrequency(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
        }

       AudienceIntelligence::~AudienceIntelligence()
       {
            Unregister("getLogLevel");
            Unregister("setLogLevel");
            Unregister("enableLAR");
            Unregister("enableACR");
            Unregister("setACRFrequency");
       }
    } // namespace Plugin
} // namespace WPEFramework
 
