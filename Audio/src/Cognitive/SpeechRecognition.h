// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. 

#ifndef __SPEECH_RECOGNITION_OS5_H__
#define __SPEECH_RECOGNITION_OS5_H__

#include "Arduino.h"
#include "SpeechRecognition.h"
#include "http_client.h"
#include <json.h>

#define TOKEN_SERVICE_URL "https://api.cognitive.microsoft.com/sts/v1.0/issueToken"
#define SPEECH_SERVICE_URL "https://speech.platform.bing.com/speech/recognition/interactive/cognitiveservices/v1?language=en-US&format=simple"

class SpeechRecognition
{
    public:
        SpeechRecognition(const char * subscriptionKey);
        virtual ~SpeechRecognition(void);

        char * convertSpeechToText(const char *wavAudio, int length);
        int getJwtToken();

    private:
        char * _cognitiveSubKey;
        char * _currentToken;
};

#endif
