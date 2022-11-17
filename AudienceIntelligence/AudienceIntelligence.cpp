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
#include <string.h>
#include <syscall.h>

#undef LOG // we don't need LOG from audiocapturemgr_iarm as we are defining our own LOG

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
//#include "safec_lib.h"
#include "libIBus.h"
#include <pthread.h>

#include "audiocapturemgr_iarm.h"
#include "libIARM.h"

#define API_VERSION_NUMBER_MAJOR 1
#define API_VERSION_NUMBER_MINOR 0
#define API_VERSION_NUMBER_PATCH 0

bool ACRModeEnabled = true;
bool LARModeEnabled = true;
bool keep_running = true;
JsonArray acrevents_arr;
std::string socket_path;
IARM_Result_t ret;
static const char * instance_name = "test";
audiocapturemgr::session_id_t session;
audiocapturemgr::iarmbus_acm_arg_t param;
audiocapturemgr::audio_properties_ifce_t audio_properties_ifce_t;
audiocapturemgr::audio_properties_ifce_t props;

using namespace std;
namespace WPEFramework
{
    namespace Plugin
    {

	static bool verify_result(IARM_Result_t ret, iarmbus_acm_arg_t &param)
{
	if(IARM_RESULT_SUCCESS != ret)
	{
		LOGINFO("Bus call failed.\n");
		return false;
	}
	if(0 != param.result)
	{
		std::cout<<"ACM implementation of bus call failed.\n";
		return false;
	}
	return true;
}
	SERVICE_REGISTRATION(AudienceIntelligence, API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH);
	AudienceIntelligence* AudienceIntelligence::_instance = nullptr;
	std::vector<string> registeredEvtListeners;

	AudienceIntelligence::AudienceIntelligence()
        : PluginHost::JSONRPC(),
	_acrEventListener(nullptr)
        {
	    LOGINFO("ctor");

            Register("getLogLevel", &AudienceIntelligence::getLogLevelWrapper, this);
            Register("setLogLevel", &AudienceIntelligence::setLogLevelWrapper, this);
            Register("enableLAR", &AudienceIntelligence::enableLAR, this);
            Register("enableACR", &AudienceIntelligence::enableACR, this);
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
                    //IARM_Result_t res;
            }
        }

       void AudienceIntelligence::DeinitializeIARM()
        {
           LOGWARN("AudienceIntelligence::DeinitializeIARM");
	   if(Utils::IARM::isConnected())
            {
                    //IARM_Result_t res;
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
			getAudio();
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

        /*void AudienceIntelligence::get_suffix()
        {  
	        std::ostringstream stream;
	        stream << ticker++;
                stream << ".wav";
	        std::string outstring = stream.str();
	        return outstring;
        }*/        
 	
	void open_session()
	{
		param.details.arg_open.source = 0; //primary
				param.details.arg_open.output_type = 1;
				ret = IARM_Bus_Call(IARMBUS_AUDIOCAPTUREMGR_NAME, IARMBUS_AUDIOCAPTUREMGR_OPEN, (void *) &param, sizeof(param));
				if(!verify_result(ret, param))
				{
				     LOGINFO(" Unknown input!");
				}
				session = param.session_id;
				LOGINFO("Opened new session");


	} 

        void get_default_props()
{
	param.session_id = session;
				ret = IARM_Bus_Call(IARMBUS_AUDIOCAPTUREMGR_NAME, IARMBUS_AUDIOCAPTUREMGR_GET_DEFAULT_AUDIO_PROPS, (void *) &param, sizeof(param));
				if(!verify_result(ret, param))
				{
				     LOGINFO(" Unknown input!");
				}
				LOGINFO("Format: 0x%x, delay comp: %dms, fifo size: %d, threshold: %d\n", 
						param.details.arg_audio_properties.format,
						param.details.arg_audio_properties.delay_compensation_ms,
						param.details.arg_audio_properties.fifo_size,
						param.details.arg_audio_properties.threshold);
				props = param.details.arg_audio_properties;

}

	void set_audio_props()
{
		param.session_id = session;
				param.details.arg_audio_properties = props;
				param.details.arg_audio_properties.delay_compensation_ms = 190;
				ret = IARM_Bus_Call(IARMBUS_AUDIOCAPTUREMGR_NAME, IARMBUS_AUDIOCAPTUREMGR_SET_AUDIO_PROPERTIES, (void *) &param, sizeof(param));
				if(!verify_result(ret, param))
				{
				     LOGINFO(" Unknown input!");
				}

}

        void get_output_props()
{
			param.session_id = session;
				ret = IARM_Bus_Call(IARMBUS_AUDIOCAPTUREMGR_NAME, IARMBUS_AUDIOCAPTUREMGR_GET_OUTPUT_PROPS, (void *) &param, sizeof(param));
				if(!verify_result(ret, param))
				{
				     LOGINFO(" Unknown input!");
				}
				socket_path = std::string(param.details.arg_output_props.output.file_path);
				 LOGINFO("Output path is ");

}

void * read_thread(void * data)
{
	std::string socket_path = *(static_cast <std::string *> (data));
	if(socket_path.empty())
	{
		std::cout<<"read thread returning as socket path is empty.\n";
		return NULL;
	}
	std::cout<<"Connecting to socket "<<socket_path<<std::endl;
	struct sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, socket_path.c_str(), (socket_path.size() + 1));
	int read_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if(0 > read_fd)
	{
		std::cout<<"Couldn't create read socket. Exiting.\n";
		return NULL;
	}
	if(0 != connect(read_fd, (const struct sockaddr *) &addr, sizeof(addr)))
	{
		std::cout<<"Couldn't connect to the path. Exiting.\n";
		perror("read_thread");
		close(read_fd);
		return NULL;
	}
	std::cout<<"Connection established.\n";
	unsigned int recvd_bytes = 0;
	char buffer[1024];
	std::string filename = "/opt/acm_ipout_dump_";
	filename += instance_name;
	std::ofstream file_dump(filename.c_str(), std::ios::binary);
	while(true)
	{
		int ret = read(read_fd, buffer, 1024);
		if(0 == ret)
		{
			std::cout<<"Zero bytes read. Exiting.\n";
			break;
		}
		else if(0 > ret)
		{
			std::cout<<"Error reading from socket. Exiting.\n";
			perror("read error");
			break;
		}
		if((1024*1024*2) > recvd_bytes) //Write up to 2 MB to a file
		{
			file_dump.write(buffer, ret);
		}
		recvd_bytes += ret;
	}

	close(read_fd);
	LOGINFO("Number of bytes read: %d",recvd_bytes);
	file_dump.seekp(0, std::ios_base::end);
	std::cout<<file_dump.tellp()<<" bytes written to file.\n";
//	std::cout<<"Exiting read thread.\n";
	return NULL;
}

void connect_and_read_data(std::string &socket_path)
{
	pthread_t thread;
	int ret = pthread_create(&thread, NULL, read_thread, (void *) &socket_path);
	if(0 == ret)
	{
		LOGINFO("Successfully launched read thread.\n");
	}
	else
	{
		LOGINFO("Failed to launch read thread.\n");
	}
}
void start_capture()
{
			if(socket_path.empty())
				{
					LOGINFO("No path to socket available.\n");
				}
				LOGINFO("Launching read thread.\n");
				connect_and_read_data(socket_path);
				param.session_id = session;
				ret = IARM_Bus_Call(IARMBUS_AUDIOCAPTUREMGR_NAME, IARMBUS_AUDIOCAPTUREMGR_START, (void *) &param, sizeof(param));
				if(!verify_result(ret, param))
				{
				     LOGINFO(" Unknown input!");
				}
				LOGINFO("Start procedure complete.\n");

}

void stop_capture()
{
			param.session_id = session;
				ret = IARM_Bus_Call(IARMBUS_AUDIOCAPTUREMGR_NAME, IARMBUS_AUDIOCAPTUREMGR_STOP, (void *) &param, sizeof(param));
				if(!verify_result(ret, param))
				{
				     LOGINFO(" Unknown input!");
				}
				LOGINFO("Stop procedure complete.\n");

}

void close_session()
{
			param.session_id = session;
				ret = IARM_Bus_Call(IARMBUS_AUDIOCAPTUREMGR_NAME, IARMBUS_AUDIOCAPTUREMGR_CLOSE, (void *) &param, sizeof(param));
				if(!verify_result(ret, param))
				{
				     LOGINFO(" Unknown input!");
				}
				LOGINFO("Close procedure complete.\n");
				session = -1;
				socket_path.clear();
				keep_running = false;
}


        void AudienceIntelligence::getAudio()
        {
             if(0 != IARM_Bus_Init("acm_testapp_sample"))
             {
                LOGINFO("Unable to init IARMBus. Try another session name.\n");
                return;
             }
        if(0 != IARM_Bus_Connect())
        {
                LOGINFO("Unable to connect to IARBus\n");
                return;
        }
	open_session();
        get_default_props();
        set_audio_props();
	get_output_props();
	start_capture();
        stop_capture();
	close_session();
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


	void AudienceIntelligenceListener::onCLDSignatureEvent(const std::string& event,uint64_t epochts)
        {
                LOGINFO("CLD event at Audience Intelligence Plugin :%s \n",event.c_str());

                JsonObject params;
		params.FromString(event);
		params["timestamp"] = epochts;
                acrevents_arr.Add(params);
		if(acrevents_arr.Length() == 4)
		{
                	//string json;
                	JsonObject acrevent;
			acrevent["propertyName"]=ACR_EVENTS;
			acrevent["acr_signatures"] = acrevents_arr;		
			//acrevent.ToString(json);
                	//LOGINFO("NOTIFY json is %s\n",json.c_str());
                	maudintelligence.notify("onacrevent",acrevent);
			acrevents_arr.Clear();
			LOGINFO(" length of acrevents_arr now %d \n",acrevents_arr.Length());
		}
        }


    } // namespace Plugin
} // namespace WPEFramework
