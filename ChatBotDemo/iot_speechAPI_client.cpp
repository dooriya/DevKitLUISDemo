#include "iot_speechAPI_client.h"
#include "Arduino.h"
#include "http_client.h"
#include "AZ3166Wifi.h"
#include <json.h>
#include <stdlib.h>
#include <string.h>

static char *current_token = NULL;

///Utility methods begin
const char *_json_object_get_string(json_object *obj, const char *name);
///Utility methods end

int iot_client_get_token(const char *key1)
{
    time_t currentTime = time(NULL);
    if (currentTime == (time_t)-1 || currentTime < 1492333149)
    {
        Serial.println("Time does not appear to be working.");
        return -1;
    }

    HTTPClient tokenRequest = HTTPClient(HTTP_POST, COGNITIVE_API_TOKEN_URL);
    tokenRequest.set_header("Host", "api.cognitive.microsoft.com");
    tokenRequest.set_header("Connection", "Keep-Alive");
    tokenRequest.set_header("Content-Length", "0");
    tokenRequest.set_header("Ocp-Apim-Subscription-Key", key1);
    const Http_Response *response = tokenRequest.send();

    if (response == NULL)
    {
        Serial.println("Token retrieval failed");
        return -1;
    }
    else if (response->status_code == 403)
    {
        Serial.println("Authentication failed, please check variable key1");
        return -1;
    }
    else
    {
        current_token = (char *)response->body;
        printf(current_token);
        return 0;
    }
}

const char *_json_object_get_string(json_object *obj, const char *name)
{
    return json_object_get_string(json_object_object_get(obj, name));
}

char *iot_client_send_audio(const char *content, int length)
{
    String bearer = "Bearer ";
    String auth = (bearer + current_token);
    HTTPClient conversionRequest = HTTPClient(HTTP_POST, INTERATIVE_SPEECH_URL);
    conversionRequest.set_header("Host", "speech.platform.bing.com");
    conversionRequest.set_header("Accept", "application/json;text/xml");
    conversionRequest.set_header("Content-Type", "audio/wav; codec="
                                                 "audio/pcm"
                                                 "; samplerate=1600");
    conversionRequest.set_header("Authorization", auth.c_str());
    const Http_Response *response = conversionRequest.send(content, length);

    if (response == NULL)
    {
        Serial.println("No response, error");
        return NULL;
    }

    json_object *jsonObject = json_tokener_parse(response->body);
    if (jsonObject != NULL)
    {
        const char *json_Status = _json_object_get_string(jsonObject, "RecognitionStatus");
        if (json_Status == NULL)
        {
            Serial.println("Unable to get string from json object for \"Status\"");
        }

        if (strcmp(json_Status, "Success") == 0)
        {
            char *json_DisplayText = (char *)_json_object_get_string(jsonObject, "DisplayText");
            if (json_DisplayText == NULL)
            {
                Serial.println("Unable to get string from json object for \"DisplayText\"");
            }
            else
            {
                json_DisplayText[strlen(json_DisplayText) - 1] = 0;
                Serial.print("Word: ");
                Serial.println(json_DisplayText);
                return json_DisplayText;
            }
        }
        else
        {
            Serial.println("RecognitionStatus = Failed");
        }
    }
    return NULL;
}
