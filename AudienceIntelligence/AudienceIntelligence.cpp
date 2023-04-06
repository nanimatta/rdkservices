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
#include <cjson/cJSON.h>
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
#include <curl/curl.h>

#define API_VERSION_NUMBER_MAJOR 1
#define API_VERSION_NUMBER_MINOR 0
#define API_VERSION_NUMBER_PATCH 0

bool ACRModeEnabled = true;
bool LARModeEnabled = true;
int Svalue = 0;
JsonArray acrevents_arr;
JsonObject payload;
JsonObject context;
JsonObject sendacr;

using namespace std;
namespace WPEFramework
{
    namespace Plugin
    {

	SERVICE_REGISTRATION(AudienceIntelligence, API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH);
	AudienceIntelligence* AudienceIntelligence::_instance = nullptr;
	std::vector<string> registeredEvtListeners;
        const int curlTimeoutInSeconds = 30;

	static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
        {
          ((std::string*)userp)->append((char*)contents, size * nmemb);
          return size * nmemb;
        }
	AudienceIntelligence::AudienceIntelligence()
        : PluginHost::JSONRPC(),
	_acrEventListener(nullptr)
        {
	    LOGINFO("ctor");

            Register("getLogLevel", &AudienceIntelligence::getLogLevelWrapper, this);
            Register("setLogLevel", &AudienceIntelligence::setLogLevelWrapper, this);
            Register("enableLAR", &AudienceIntelligence::enableLAR, this);
            Register("enableACR", &AudienceIntelligence::enableACR, this);
	    Register("frameSkip", &AudienceIntelligence::frameSkip, this);
            Register("setACRFrequency", &AudienceIntelligence::setACRFrequency, this);

	    Register("registerListeners",&AudienceIntelligence::registerListeners, this);  //Register ACRLAR Events
	    Register("unregisterListeners",&AudienceIntelligence::unregisterListeners, this);
        }

	const string AudienceIntelligence::Initialize(PluginHost::IShell* service)
        {
	    LOGWARN("Initlaizing AudienceIntelligence");
            AudienceIntelligence::_instance = this;
	    _acrClient        = ACRClient::getInstance();
            _acrEventListener = new AudienceIntelligenceListener(this);
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
       //             IARM_Result_t res;
            }
        }

       void AudienceIntelligence::DeinitializeIARM()
        {
           LOGWARN("AudienceIntelligence::DeinitializeIARM");
	   if(Utils::IARM::isConnected())
            {
            //        IARM_Result_t res;
            }

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
                response["level"] = logLevel;
            }
	    returnResponse(result);
        }
        
	uint32_t AudienceIntelligence::setLogLevelWrapper(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
	    bool result = true;
            if (!parameters.HasLabel("level"))
            {
                result = false;
                response["message"] = "please specify log level (level = DEBUG/INFO/WARN/ERROR/FATAL)";
            }
            if (result)
            {
                std::string logLevel  = parameters["level"].String();
                //todo
                if (false == result) {
                    response["message"] = "failed to set log level";
                }
                else
                {
                    response["level"] = logLevel;
                }
            }
	    returnResponse(result);
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
                    if (ACRModeEnabled) {
                        response["message"] = "ACR feature enabled";
			if(_acrClient) {
                                _acrClient->enableAudienceIntelligence(ACRModeEnabled);
                        }
		    }
                    else {
			 _acrClient->enableAudienceIntelligence(ACRModeEnabled);
                        response["message"] = "ACR feature disabled";
		    }
            }
	    returnResponse(result);
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
	    returnResponse(result);
        }
        
	uint32_t AudienceIntelligence::frameSkip(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
	    bool result = true;
            if (!parameters.HasLabel("value"))
            {
                result = false;
                response["message"] = "please specify value parameter";
            }
            if (result)
            {
                Svalue = (unsigned int)parameters["value"].Number();
                    if (Svalue <= 1)
		    {
			if(_acrClient) {
                                _acrClient->updateframeskipvalue(Svalue);
                        }
                        response["message"] = "Updated frameSkip value";
		    }
            }
	    returnResponse(result);
        }

	uint32_t AudienceIntelligence::setACRFrequency(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
		return 0;
        }


	uint32_t AudienceIntelligence::registerListeners(const JsonObject& parameters, JsonObject& response)
        {
                LOGINFOMETHOD();

                bool ret=false;
                string json;
                std::vector<string>::iterator it;
                parameters.ToString(json);
                cJSON *root;
                root=cJSON_Parse(json.c_str());
                if( root ==NULL)
                {
                        LOGWARN("not able to parse json request \n");
                        returnResponse(ret);
                }
                cJSON *items=cJSON_GetObjectItem(root,"PropertyNames");
                if( items == NULL)
                {
                        LOGWARN("not able to fetch property names from request \n");
                        cJSON_Delete(root);
                        returnResponse(ret);
                }
                int arrsize=cJSON_GetArraySize(items);
                if(arrsize!=0)
                {
                        ret = true;
                        cJSON *elem;
                        int i;
                        vector<string> eventname;
                        JsonArray response_arr;
                        JsonObject acrlarevent;
			LOGINFO("Register arrsize : %d ",arrsize);
                        for(i=0;i<arrsize;i++)
                        {
                                elem = cJSON_GetArrayItem(items, i);
                                string evt_str =elem->valuestring;
                                eventname.push_back(evt_str);
                                LOGINFO("Register Event Listener %s ",evt_str.c_str());
                                acrlarevent["propertyName"] = evt_str;
                                response_arr.Add(acrlarevent);
                         }

                         for( it = eventname.begin(); it!= eventname.end(); ++it )
                         {

                                if (std::find(registeredEvtListeners.begin(), registeredEvtListeners.end(), *it) == registeredEvtListeners.end())
                                {
                                        LOGINFO("Event being added to listeners %s",it->c_str());
                                        registeredEvtListeners.push_back(*it);
                                }
			 }
                         _acrClient->RegisterEventListener(eventname,_acrEventListener);
			 cout << "ACR RegisterEventListener after call "<< endl;
                      	 response["properties"]=response_arr;
                         /*string json_str;
                         response.ToString(json_str);
                         LOGINFO("json array of properties is %s\n",json_str.c_str());*/

                }
                LOGTRACEMETHODFIN();
                cJSON_Delete(root);
                returnResponse(ret);
        }

	 uint32_t AudienceIntelligence::unregisterListeners(const JsonObject& parameters, JsonObject& response)
        {
                LOGINFOMETHOD();
                bool ret=false;
                string json;
                parameters.ToString(json);
                cJSON *root;
                root=cJSON_Parse(json.c_str());
                if( root ==NULL)
                {
                        LOGWARN("not able to parse json request \n");
                        returnResponse(ret);
                }
                cJSON *items=cJSON_GetObjectItem(root,"PropertyNames");
                if( items == NULL)
                {
                        LOGWARN("not able to fetch property names from request \n");
                        cJSON_Delete(root);
                        returnResponse(ret);
                }
                int arrsize=cJSON_GetArraySize(items);
                if(arrsize!=0)
                {
                        ret=true;
                        cJSON *elem;
                        vector<string> acrlarevents;
                        for(int i=0;i<arrsize;i++)
                        {
                                elem = cJSON_GetArrayItem(items, i);
                                string prop_str =elem->valuestring;
                                acrlarevents.push_back(prop_str);
                        }
                        for( vector<string>::iterator it = acrlarevents.begin(); it!= acrlarevents.end(); ++it )
                        {
                                vector<string>::iterator itr=find(registeredEvtListeners.begin(), registeredEvtListeners.end(), *it);
                                if(itr!=registeredEvtListeners.end())
                                {
                                        LOGINFO("Event erased : %s",it->c_str());
                                        registeredEvtListeners.erase(itr);
                                }
                        }
                        _acrClient->UnRegisterEventListener(acrlarevents,_acrEventListener);
                }
                LOGTRACEMETHODFIN();
                cJSON_Delete(root);
                returnResponse(ret);
        }


 	void AudienceIntelligence::notify(const std::string& event, const JsonObject& parameters)
        {
                string property_name = parameters["propertyName"].String();
		LOGINFO(" event notification : %s  propertyname : %s \n",event.c_str(),property_name.c_str());
                if(std::find(registeredEvtListeners.begin(), registeredEvtListeners.end(), property_name) != registeredEvtListeners.end())
                {

			 LOGINFO(" Property registered notifying the events \n");
                        sendNotify(event.c_str(),parameters);
                }
	}

	AudienceIntelligence::~AudienceIntelligence()
	{
            Unregister("getLogLevel");
            Unregister("setLogLevel");
            Unregister("enableLAR");
            Unregister("enableACR");
	    Unregister("frameSkip");
            Unregister("setACRFrequency");
	    Unregister("registerListeners");
	    Unregister("unregisterListeners");
            delete _acrEventListener;
            _acrEventListener = nullptr;
            delete _acrClient;
            _acrClient = nullptr;

       }

	AudienceIntelligenceListener::AudienceIntelligenceListener(AudienceIntelligence* audintelligence)
                        : maudintelligence(*audintelligence)
        {
		LOGINFO("Constructor \n");
        }
        AudienceIntelligenceListener::~AudienceIntelligenceListener()
        {
		LOGINFO("Destructor \n");
        }


	void AudienceIntelligenceListener::onCLDSignatureEvent(const std::string& event,uint64_t epochts,unsigned int is_interlaced,unsigned int frame_rate,unsigned int pic_width,unsigned int pic_height,int frame_skip)
        {
                LOGINFO("CLD event at Audience Intelligence Plugin :%s \n",event.c_str());

                JsonObject params;
		params.FromString(event);
		params["timestamp"] = epochts;
		params["is_interlaced"] = is_interlaced;
                params["pic_width"]     = pic_width;
                params["pic_height"]    = pic_height;
                params["frame_skip"]    = frame_skip;
                acrevents_arr.Add(params);

		sendacr["account_id"] = "1234559232997869257";
		sendacr["app_name"] = "acr";
		sendacr["app_ver"] = "1.0";
		sendacr["device_id"] = "1234645798406437895";
		sendacr["device_language"] = "eng";
		sendacr["device_model"] = "TCHxi6";
		sendacr["device_timezone"] = -14400000;
		sendacr["event_id"] = "5e5e386a-e5cb-11ec-8fea-0242ac120002";
		sendacr["event_name"] = "acr_event";
		sendacr["event_schema"] = "acr/acr_event/2";
		sendacr["os_ver"] = "TX061AEI_VBN_23Q1_sprint_20230328150607sdy_ACR_xi6";
		sendacr["partner_id"] = "comcast";
		sendacr["platform"] = "flex";
		sendacr["session_id"] = "1234e567-e89b-12d3-a456-426614174000";
		sendacr["timestamp"] = epochts;
                context["hdmi_input"] = "none"; 
                context["station_id"] = "none"; 
                context["tune_type"] = "none"; 
                context["viewing_mode"] = "none"; 
                context["program_title"] = "none"; 
                context["program_id"] = "none"; 
                context["program_start_time"] = "none"; 
                context["tune_time"] = "none"; 
                payload["acr_signatures"] =  acrevents_arr;
                payload["context"] =  context;
                payload["ip_address"] = "1.2.3.4"; 
		sendacr["event_payload"] = payload;
                string acrjson;
                sendacr.ToString(acrjson);
		LOGINFO("%s: sendacr is %s\n", __FUNCTION__, acrjson.c_str());
                string ajson = "'[" + acrjson + "]'";
		LOGINFO("%s: sendacr final is %s\n", __FUNCTION__, ajson.c_str());

		long http_code = 0;
                std::string response;
                CURL *curl_handle = NULL;
                CURLcode res = CURLE_OK;
                curl_handle = curl_easy_init();
                //create header
                struct curl_slist *chunk = NULL;
                chunk = curl_slist_append(chunk, "Content-Type: application/json");

                if (curl_handle) {

		   LOGINFO("%s: acr entered curl \n", __FUNCTION__);
                   curl_easy_setopt(curl_handle, CURLOPT_URL, "https://collector.pabs.comcast.com/acr/dev");
		   curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, chunk);
                   curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, ajson.c_str());
                   curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDSIZE, ajson.size());
		   curl_easy_setopt(curl_handle, CURLOPT_POST, 1);
                   curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1); //when redirected, follow the redirections
                   curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteCallback);
                   curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &response);
                   curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, curlTimeoutInSeconds);

                   res = curl_easy_perform(curl_handle);
		   LOGINFO("%s: acr sent curl \n", __FUNCTION__);
                   curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &http_code);

                   LOGWARN("Perfomed acr curl call : %d http response: %s code: %ld error:'%s'", res, response.c_str(), http_code, curl_easy_strerror(res));
                   curl_easy_cleanup(curl_handle);
            }
            else {
                LOGWARN("Could not perform acr curl ");
            }


		if(acrevents_arr.Length() == 4)
		{
			string json;
			//string ajson;
                	JsonObject acrevent;
			acrevent["propertyName"]=ACR_EVENTS;
			acrevent["acr_signatures"] = acrevents_arr;		
			//sendacr["acr_signatures"] = acrevents_arr;
			acrevent.ToString(json);
			//sendacr.ToString(ajson);
			LOGINFO("%s: NOTIFY json is %s\n", __FUNCTION__, json.c_str());
			//LOGINFO("%s: sendacr is %s\n", __FUNCTION__, ajson.c_str());
                	maudintelligence.notify("onacrevent",acrevent);
			acrevents_arr.Clear();
			LOGINFO(" length of acrevents_arr now %d \n",acrevents_arr.Length());
		}
        }
    } // namespace Plugin
} // namespace WPEFramework
