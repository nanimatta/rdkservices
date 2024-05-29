#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string>
#include <fstream>
#include <algorithm>
#include <time.h>
#include <vector>
#include <thread>
#include <chrono>
#include <condition_variable>
#include <atomic>
#include <mutex>
#include <curl/curl.h>
#include "Module.h"
#include "NetworkManagerConnectivity.h"
#include "NetworkManagerImplementation.h"

namespace WPEFramework {
    namespace Plugin {

    extern NetworkManagerImplementation* _instance;

    bool EndpointCache::isEndpointCashFileExist()
    {
        std::ifstream fileStream(CachefilePath);
        return fileStream.is_open();
    }

    void EndpointCache::writeEnpointsToFile(const std::vector<std::string>& endpoints)
    {
        std::ofstream outputFile(CachefilePath);
        if (outputFile.is_open())
        {
            for (const std::string& str : endpoints)
            {
                outputFile << str << '\n';
            }
            outputFile.close();
            NMLOG_INFO("Connectivity endpoints successfully written to a file");
        }
        else
        {
            NMLOG_ERROR("Connectivity endpoints file write error");
        }
    }

    std::vector<std::string> EndpointCache::readEnpointsFromFile()
    {
        std::vector<std::string> readStrings;
        std::ifstream inputFile(CachefilePath);
        if (inputFile.is_open())
        {
            std::string line;
            while (std::getline(inputFile, line))
            {
                readStrings.push_back(line);
            }
            inputFile.close();
        }
        else
        {
            NMLOG_ERROR("Failed to open connectivity endpoint cache file");
        }
        return readStrings;
    }

    bool ConnectivityMonitor::isConnectivityMonitorEndpointSet()
    {
        const std::lock_guard<std::mutex> lock(endpointMutex);
        if(monitorEndpoints.size() > 0)
        {
            return true;
        }
        else if(endpointCache.isEndpointCashFileExist())
        {
            monitorEndpoints = endpointCache.readEnpointsFromFile();
            std::string endpoints_str;
            for (const auto& endpoint : monitorEndpoints)
                endpoints_str.append(endpoint).append(" ");
            NMLOG_INFO("updated endpoints count %d and endpoints:- %s", static_cast<int>(monitorEndpoints.size()), endpoints_str.c_str());
            if(monitorEndpoints.size() > 0)
                return true;
        }
        return false;
    }

    bool ConnectivityMonitor::isConnectedToInternet(nsm_ipversion ipversion)
    {
        if (nsm_internetState::FULLY_CONNECTED == getInternetConnectionState(ipversion))
        {
            NMLOG_INFO("isConnectedToInternet = true");
            return true;
        }

        NMLOG_WARNING("isConnectedToInternet = false");
        return false;
    }

    nsm_internetState ConnectivityMonitor::getInternetConnectionState(nsm_ipversion ipversion)
    {

        nsm_internetState internetState = nsm_internetState::UNKNOWN;

        // If monitor connectivity is running take the cashe value
        if ( isMonitorThreadRunning() && (ipversion == NSM_IPRESOLVE_WHATEVER) && (g_internetState != nsm_internetState::UNKNOWN) )
        {
            internetState = g_internetState;
        }
        else
        {
            if(isConnectivityMonitorEndpointSet())
                internetState = testConnectivity(getConnectivityMonitorEndpoints(), TEST_CONNECTIVITY_DEFAULT_TIMEOUT_MS, ipversion, true);
            else
                internetState = testConnectivity(getConnectivityDefaultEndpoints(), TEST_CONNECTIVITY_DEFAULT_TIMEOUT_MS, ipversion, true);
        }

        return internetState;
    }

    std::string ConnectivityMonitor::getCaptivePortalURI()
    {
        if (g_internetState == nsm_internetState::CAPTIVE_PORTAL)
        {
            return getCaptivePortal();
        }
        else
        {
            std::string captivePortal;
            if(getInternetConnectionState(NSM_IPRESOLVE_WHATEVER) == nsm_internetState::CAPTIVE_PORTAL)
            {
                return getCaptivePortal();
            }
            else
                NMLOG_WARNING("No captive portal found !");
        }
        return std::string("");
    }

    void ConnectivityMonitor::setConnectivityMonitorEndpoints(const std::vector<std::string> &endpoints)
    {
        const std::lock_guard<std::mutex> lock(endpointMutex);
        monitorEndpoints.clear();
        for (auto endpoint : endpoints) {
            if(!endpoint.empty() && endpoint.size() > 3)
                monitorEndpoints.push_back(endpoint.c_str());
            else
                NMLOG_INFO("endpoint not vallied = %s", endpoint.c_str());
        }

        // write the endpoints to a file
        endpointCache.writeEnpointsToFile(monitorEndpoints);

        std::string endpoints_str;
        for (const auto& endpoint : monitorEndpoints)
            endpoints_str.append(endpoint).append(" ");
        NMLOG_INFO("Connectivity monitor endpoints -: %d :- %s", static_cast<int>(monitorEndpoints.size()), endpoints_str.c_str());
    }

    std::vector<std::string> ConnectivityMonitor::getConnectivityMonitorEndpoints()
    {
        const std::lock_guard<std::mutex> lock(endpointMutex);
        std::vector<std::string> endpoints;
        for (auto endpoint : monitorEndpoints) {
            endpoints.push_back(endpoint);
        }
        return endpoints;
    }

    static bool curlVerboseEnabled() {
        std::ifstream fileStream("/tmp/network.plugin.debug");
        return fileStream.is_open();
    }

    static long current_time ()
    {
        struct timespec ts;
        clock_gettime (CLOCK_MONOTONIC, &ts);
        return (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
    }
    static size_t writeFunction(void* ptr, size_t size, size_t nmemb, std::string* data) {
    #ifdef DBG_CURL_GET_RESPONSE
        NMLOG_TRACE("%s",(char*)ptr);
    #endif
        return size * nmemb;
    }

    nsm_internetState Connectivity::testConnectivity(const std::vector<std::string>& endpoints, long timeout_ms, nsm_ipversion ipversion, bool connectOnly)
    {
        long deadline = current_time() + timeout_ms, time_now = 0, time_earlier = 0;
        if(endpoints.size() < 1) {
            NMLOG_ERROR("endpoints size error ! ");
            return NO_INTERNET;
        }

        CURLM *curl_multi_handle = curl_multi_init();
        if (!curl_multi_handle)
        {
            NMLOG_ERROR("curl_multi_init returned NULL");
            return NO_INTERNET;
        }

        CURLMcode mc;
        std::vector<CURL*> curl_easy_handles;
        std::vector<int> http_responses;
        struct curl_slist *chunk = NULL;
        chunk = curl_slist_append(chunk, "Cache-Control: no-cache, no-store");
        chunk = curl_slist_append(chunk, "Connection: close");
        for (const auto& endpoint : endpoints)
        {
            CURL *curl_easy_handle = curl_easy_init();
            if (!curl_easy_handle)
            {
                NMLOG_ERROR("endpoint = <%s> curl_easy_init returned NULL", endpoint.c_str());
                continue;
            }
            curl_easy_setopt(curl_easy_handle, CURLOPT_URL, endpoint.c_str());
            curl_easy_setopt(curl_easy_handle, CURLOPT_PRIVATE, endpoint.c_str());
            /* set our custom set of headers */
            curl_easy_setopt(curl_easy_handle, CURLOPT_HTTPHEADER, chunk);
            curl_easy_setopt(curl_easy_handle, CURLOPT_USERAGENT, "RDKCaptiveCheck/1.0");
            /* set CURLOPT_HTTPGET option insted of CURLOPT_CONNECT_ONLY bcause we need to get the captiveportal URI not just connection only */
            if(connectOnly)
                curl_easy_setopt(curl_easy_handle, CURLOPT_CONNECT_ONLY, 1L);
            else
                curl_easy_setopt(curl_easy_handle, CURLOPT_HTTPGET, 1L); /* HTTPGET request added insted of HTTPHEAD request fix for DELIA-61526 */
            curl_easy_setopt(curl_easy_handle, CURLOPT_WRITEFUNCTION, writeFunction);
            curl_easy_setopt(curl_easy_handle, CURLOPT_TIMEOUT_MS, deadline - current_time());
            if ((ipversion == CURL_IPRESOLVE_V4) || (ipversion == CURL_IPRESOLVE_V6))
                curl_easy_setopt(curl_easy_handle, CURLOPT_IPRESOLVE, ipversion);
            if(curlVerboseEnabled())
                curl_easy_setopt(curl_easy_handle, CURLOPT_VERBOSE, 1L);
            if (CURLM_OK != (mc = curl_multi_add_handle(curl_multi_handle, curl_easy_handle)))
            {
                NMLOG_ERROR("endpoint = <%s> curl_multi_add_handle returned %d (%s)", endpoint.c_str(), mc, curl_multi_strerror(mc));
                curl_easy_cleanup(curl_easy_handle);
                continue;
            }
            curl_easy_handles.push_back(curl_easy_handle);
        }
        int handles, msgs_left;
        char *url = nullptr;
    #if LIBCURL_VERSION_NUM < 0x074200
        int numfds, repeats = 0;
    #endif
        char *endpoint = nullptr;
        while (1)
        {
            if (CURLM_OK != (mc = curl_multi_perform(curl_multi_handle, &handles)))
            {
                NMLOG_ERROR("curl_multi_perform returned %d (%s)", mc, curl_multi_strerror(mc));
                break;
            }
            for (CURLMsg *msg; NULL != (msg = curl_multi_info_read(curl_multi_handle, &msgs_left)); )
            {
                long response_code = -1;
                if (msg->msg != CURLMSG_DONE)
                    continue;
                if (CURLE_OK == msg->data.result) {
                    curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &endpoint);
                    if(connectOnly)
                    response_code = HttpStatus_204_No_Content;
                    else if (curl_easy_getinfo(msg->easy_handle, CURLINFO_RESPONSE_CODE, &response_code) == CURLE_OK)  {
                        if(curlVerboseEnabled())
                            NMLOG_INFO("endpoint = <%s> response code <%d>",endpoint, static_cast<int>(response_code));
                        if (HttpStatus_302_Found == response_code) {
                            if ( (curl_easy_getinfo(msg->easy_handle, CURLINFO_REDIRECT_URL, &url) == CURLE_OK) && url != nullptr) {
                                NMLOG_WARNING("captive portal found !!!");
                                setCaptivePortal(url);
                            }
                        }
                    }
                }
                else
                    NMLOG_TRACE("endpoint = <%s> curl error = %d (%s)", endpoint, msg->data.result, curl_easy_strerror(msg->data.result));
                http_responses.push_back(response_code);
            }
            time_earlier = time_now;
            time_now = current_time();
            if (handles == 0 || time_now >= deadline)
                break;
    #if LIBCURL_VERSION_NUM < 0x074200
            if (CURLM_OK != (mc = curl_multi_wait(curl_multi_handle, NULL, 0, deadline - time_now, &numfds)))
            {
                NMLOG_ERROR("curl_multi_wait returned %d (%s)", mc, curl_multi_strerror(mc));
                break;
            }
            if (numfds == 0)
            {
                repeats++;
                if (repeats > 1)
                    usleep(10*1000); /* sleep 10 ms */
            }
            else
                repeats = 0;
    #else
            if (CURLM_OK != (mc = curl_multi_poll(curl_multi_handle, NULL, 0, deadline - time_now, NULL)))
            {
                NMLOG_ERROR("curl_multi_poll returned %d (%s)", mc, curl_multi_strerror(mc));
                break;
            }
    #endif
        }

        if(curlVerboseEnabled()) {
            NMLOG_INFO("endpoints count = %d response count %d, handles = %d, deadline = %ld, time_now = %ld, time_earlier = %ld",
                static_cast<int>(endpoints.size()), static_cast<int>(http_responses.size()), handles, deadline, time_now, time_earlier);
        }

        for (const auto& curl_easy_handle : curl_easy_handles)
        {
            curl_easy_getinfo(curl_easy_handle, CURLINFO_PRIVATE, &endpoint);
            //NMLOG_TRACE("endpoint = <%s> terminating attempt", endpoint);
            curl_multi_remove_handle(curl_multi_handle, curl_easy_handle);
            curl_easy_cleanup(curl_easy_handle);
        }
        curl_multi_cleanup(curl_multi_handle);
        /* free the custom headers */
        curl_slist_free_all(chunk);
        return checkInternetStateFromResponseCode(http_responses);
    }

    /*
     * verifying Most occurred response code is 50 % or more
     * Example 1 :
     *      if we have 5 endpoints so response also 5 ( 204 302 204 204 200 ) . Here count is 204 :- 3, 302 :- 1, 200 :- 1
     *      Return Internet State: FULLY_CONNECTED - 60 %
     * Example 2 :
     *      if we have 4 endpoints so response also 4 ( 204 204 200 200 ) . Here count is 204 :- 2, 200 :- 2
     *      Return Internet State: FULLY_CONNECTED - 50 %
     */

    nsm_internetState Connectivity::checkInternetStateFromResponseCode(const std::vector<int>& responses)
    {
        nsm_internetState InternetConnectionState = NO_INTERNET;
        nsm_connectivity_httpcode http_response_code = HttpStatus_response_error;

        int max_count = 0;
        for (int element : responses)
        {
            int element_count = count(responses.begin(), responses.end(), element);
            if (element_count > max_count)
            {
                http_response_code = static_cast<nsm_connectivity_httpcode>(element);
                max_count = element_count;
            }
        }

        /* Calculate the percentage of the most frequent code occurrences */
        float percentage = (static_cast<float>(max_count) / responses.size());

        /* 50 % connectivity check */
        if (percentage >= 0.5)
        {
            switch (http_response_code)
            {
                case HttpStatus_204_No_Content:
                    InternetConnectionState = FULLY_CONNECTED;
                    NMLOG_INFO("Internet State: FULLY_CONNECTED - %.1f%%", (percentage*100));
                break;
                case HttpStatus_200_OK:
                    InternetConnectionState = LIMITED_INTERNET;
                    NMLOG_INFO("Internet State: LIMITED_INTERNET - %.1f%%", (percentage*100));
                break;
                case HttpStatus_511_Authentication_Required:
                case HttpStatus_302_Found:
                    InternetConnectionState = CAPTIVE_PORTAL;
                    NMLOG_INFO("Internet State: CAPTIVE_PORTAL -%.1f%%", (percentage*100));
                break;
                default:
                    InternetConnectionState = NO_INTERNET;
                    if(http_response_code == -1)
                        NMLOG_ERROR("Internet State: NO_INTERNET curl error");
                    else
                        NMLOG_INFO("Internet State: NO_INTERNET Received http response code: <%d> %.1f%%", static_cast<int>(http_response_code), percentage * 100);
                    break;
            }
        }
        return InternetConnectionState;
    }

    bool ConnectivityMonitor::doContinuousConnectivityMonitoring(int timeoutInSeconds)
    {
        if(!isConnectivityMonitorEndpointSet())
        {
            NMLOG_INFO("Connectivity monitor endpoints are not set !");
            return false;
        }

        timeout.store(timeoutInSeconds >= MONITOR_TIMEOUT_INTERVAL_MIN ? timeoutInSeconds:defaultTimeoutInSec);

        if (isMonitorThreadRunning() && stopFlag == false)
        {
            isContinuesMonitoringNeeded = true;
            resetTimeout = true;
            cv_.notify_all();
        }
        else
        {
            if (thread_.joinable())
            {
                NMLOG_WARNING("Connectivity Monitor joinable Thread is active");
                stopFlag = true;
                cv_.notify_all();
                thread_.join();
            }

            isContinuesMonitoringNeeded = true;
            stopFlag = false;
            thread_ = std::thread(&ConnectivityMonitor::connectivityMonitorFunction, this);
            NMLOG_INFO("Connectivity monitor started with %d", timeout.load());
        }

        return true;
    }

    bool ConnectivityMonitor::doInitialConnectivityMonitoring(int timeoutInSeconds)
    {
        if(!isConnectivityMonitorEndpointSet())
        {
            NMLOG_INFO("Connectivity monitor endpoints are not set !");
            return false;
        }

        if (isMonitorThreadRunning() && stopFlag == false)
        {
            NMLOG_INFO("Connectivity Monitor Thread is active so notify");
            cv_.notify_all();
        }
        else
        {
            if (thread_.joinable())
            {
                NMLOG_WARNING("Connectivity Monitor joinable Thread is active");
                stopFlag = true;
                cv_.notify_all();
                thread_.join();
            }

            stopFlag = false;
            timeout.store(timeoutInSeconds >= MONITOR_TIMEOUT_INTERVAL_MIN ? timeoutInSeconds:defaultTimeoutInSec);
            thread_ = std::thread(&ConnectivityMonitor::connectivityMonitorFunction, this);
            NMLOG_INFO("Initial Connectivity Monitoring started with %d", timeout.load());
        }

        return true;
    }

    bool ConnectivityMonitor::isMonitorThreadRunning()
    {
        return thread_.joinable();
    }

    bool ConnectivityMonitor::stopInitialConnectivityMonitoring()
    {
        if(isContinuesMonitoringNeeded)
        {
            NMLOG_WARNING("Continuous Connectivity Monitor is running");
            return true;
        }

        stopFlag = true;
        cv_.notify_all();

        if (thread_.joinable())
            thread_.join();

        NMLOG_INFO("Initial Connectivity Monitor stopped");
        return true;
    }

    bool ConnectivityMonitor::stopContinuousConnectivityMonitoring()
    {
        stopFlag = true;
        cv_.notify_all();

        if (thread_.joinable())
            thread_.join();

        isContinuesMonitoringNeeded = false;
        NMLOG_INFO("Continuous Connectivity monitor stopped");
        return true;
    }

    void ConnectivityMonitor::signalConnectivityMonitor()
    {
        if (isMonitorThreadRunning())
        {
            /* Reset the global value to UNKNOWN state so the cache is reset */
            g_internetState = nsm_internetState::UNKNOWN;
            cv_.notify_all();
        }
    }

    void ConnectivityMonitor::connectivityMonitorFunction()
    {
        nsm_internetState InternetConnectionState = nsm_internetState::UNKNOWN;
        int notifyWaitCount = DEFAULT_MONITOR_RETRY_COUNT;
        int tempTimeout = defaultTimeoutInSec;
        do
        {
            if(g_internetState.load() == nsm_internetState::FULLY_CONNECTED)
                /*if previous check was fully connected then do connect only curl check*/
                InternetConnectionState = testConnectivity(getConnectivityMonitorEndpoints(), TEST_CONNECTIVITY_DEFAULT_TIMEOUT_MS, NSM_IPRESOLVE_WHATEVER, true);
            else
                /*curl get request*/
                InternetConnectionState = testConnectivity(getConnectivityMonitorEndpoints(), TEST_CONNECTIVITY_DEFAULT_TIMEOUT_MS, NSM_IPRESOLVE_WHATEVER, false);

            if(g_internetState.load() != InternetConnectionState)
            {
                NMLOG_TRACE("notification count %d ", notifyWaitCount);
                if(InternetConnectionState == nsm_internetState::NO_INTERNET && notifyWaitCount > 0)
                {
                    /* Decrease the notification count to create a delay in posting the 'no internet' state. */
                    notifyWaitCount--;
                    /* change the timeout value to 5 sec so that next check will happens with in 5 sec */
                    tempTimeout = 5;
                    NMLOG_INFO("notification count change to %d timeout %d ", notifyWaitCount, tempTimeout);
                }
                else
                {
                    g_internetState.store(InternetConnectionState);
                    if(_instance != nullptr)
                    {
                        Exchange::INetworkManager::InternetStatus oldState = static_cast<Exchange::INetworkManager::InternetStatus>(g_internetState.load());
                        Exchange::INetworkManager::InternetStatus newstate = static_cast<Exchange::INetworkManager::InternetStatus>(InternetConnectionState);
                        _instance->ReportInternetStatusChangedEvent(oldState , newstate);
                        NMLOG_TRACE("ReportInternetStatusChangedEvent called");
                    }
                    else
                        NMLOG_WARNING("ReportInternetStatusChangedEvent callback not set");
                    notifyWaitCount = DEFAULT_MONITOR_RETRY_COUNT;
                    /* change the timeout value to actual requested value */
                    tempTimeout = timeout.load();
                    NMLOG_TRACE("notification count change to default %d ...", notifyWaitCount);
                }
            }
            else
            {
                notifyWaitCount = DEFAULT_MONITOR_RETRY_COUNT;
                tempTimeout = timeout.load();
            }

            if(!isContinuesMonitoringNeeded && (g_internetState.load() == FULLY_CONNECTED))
            {
                stopFlag = true;
                NMLOG_INFO("Initial Connectivity Monitoring done Exiting ... FULLY_CONNECTED");
                break;
            }

            if(stopFlag)
                break;
            // wait for next timout or conditon signal
            std::unique_lock<std::mutex> lock(mutex_);
            if (cv_.wait_for(lock, std::chrono::seconds(tempTimeout)) != std::cv_status::timeout)
            {
                if(!stopFlag)
                {
                    /*
                    * We don't need to notify immediately when restarting the thread.
                    * Immediate notification should occur only when any connection change happens.
                    */
                    if(resetTimeout)
                    {
                        NMLOG_INFO("Connectivity monitor Restarted with %d", timeout.load());
                        tempTimeout = timeout.load();
                        resetTimeout = false;
                    }
                    else
                    {
                        notifyWaitCount = -1;
                        NMLOG_INFO("Connectivity monitor received trigger");
                    }
                }
            }

        } while (!stopFlag);

        g_internetState = nsm_internetState::UNKNOWN;
        NMLOG_WARNING("Connectivity monitor exiting");
    }

    } // namespace Plugin
} // namespace WPEFramework
