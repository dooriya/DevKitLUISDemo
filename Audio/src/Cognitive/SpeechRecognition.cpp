// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. 

#include "SpeechRecognition.h"

SpeechRecognition::SpeechRecognition(const char * subscriptionKey)
{
    _cognitiveSubKey = (char *)malloc(33);
    memcpy(_cognitiveSubKey, subscriptionKey, 33);

    _currentToken = NULL;
}

SpeechRecognition::~SpeechRecognition(void)
{
    free(_cognitiveSubKey);
    if (!_currentToken)
    {
        free(_currentToken);
    }
}

int SpeechRecognition::getJwtToken()
{
    time_t currentTime = time(NULL);
    if (currentTime == (time_t)-1 || currentTime < 1492333149)
    {
        Serial.println("Invalid system time.");
        return -1;
    }

    HTTPClient tokenRequest = HTTPClient(HTTP_POST, TOKEN_SERVICE_URL);
    tokenRequest.set_header("Host", "api.cognitive.microsoft.com");
    tokenRequest.set_header("Connection", "Keep-Alive");
    tokenRequest.set_header("Content-Length", "0");
    tokenRequest.set_header("Ocp-Apim-Subscription-Key", _cognitiveSubKey);
    const Http_Response *response = tokenRequest.send();

    if (response == NULL)
    {
        Serial.println("Cognitive token retrieval failed.");
        return -1;
    }
    else if (response->status_code == 403)
    {
        Serial.println("Authentication failed, please check the cognitive subscription key.");
        return -1;
    }
    else
    {
        if (_currentToken == NULL)
        {
            _currentToken = (char *)malloc(strlen(response->body));
        }

        strcpy(_currentToken, (char *)response->body);
        return 0;
    }
}

char * SpeechRecognition::convertSpeechToText(const char *wavAudio, int length)
{
    getJwtToken();

    String bearer = "Bearer ";
    String auth = (bearer + _currentToken);
    HTTPClient conversionRequest = HTTPClient(HTTP_POST, SPEECH_SERVICE_URL);
    conversionRequest.set_header("Host", "speech.platform.bing.com");
    conversionRequest.set_header("Accept", "application/json;text/xml");
    conversionRequest.set_header("Content-Type", "audio/wav; codec=audio/pcm; samplerate=1600");
    conversionRequest.set_header("Authorization", auth.c_str());
    const Http_Response *response = conversionRequest.send(wavAudio, length);

    if (response == NULL)
    {
        Serial.println("No response, error");
        return NULL;
    }

    json_object *jsonObject = json_tokener_parse(response->body);
    if (jsonObject != NULL)
    {
        const char *json_Status = json_object_get_string(json_object_object_get(jsonObject, "RecognitionStatus"));
        if (json_Status == NULL)
        {
            Serial.println("Unable to get string from json object for \"Status\"");
        }

        if (strcmp(json_Status, "Success") == 0)
        {
            char *json_DisplayText = (char *)json_object_get_string(json_object_object_get(jsonObject, "DisplayText"));
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
