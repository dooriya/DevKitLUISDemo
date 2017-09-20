#include "Arduino.h"
#include "AudioClassV2.h"
#include "AZ3166WiFi.h"
#include "AzureIotHub.h"
#include "IoTHubMQTTClient.h"
#include "OledDisplay.h"
#include "Sensor.h"
#include "http_client.h"
//#include "TestFile.h"

#define RGB_LED_BRIGHTNESS  32
#define LOOP_DELAY          100

static volatile int status;

static struct _tagRGB
{
  int red;
  int green;
  int blue;
} _rgb[] =
{
  { 0,   0,   0 },
  { 255,   0,   0 },
  {   0, 255,   0 },
  {   0,   0, 255 },
};

static RGB_LED rgbLed;
static int color = 0;
static int led = 0;

static bool hasWifi;
//static char action[16];
//static char actionParam[128];
static AudioClass& Audio = AudioClass::getInstance();
const int RECORD_DURATION = 3;
const int AUDIO_BUFFER_SIZE = 32000 * RECORD_DURATION + 44;
char *audioBuffer = NULL;
int waveFileSize = 0;
bool startPlay = false;
int responseAudioSize = 0;

char * pAudioWriter = NULL;
char * pAudioReader = NULL;
int playedSize= 0;
int theta = 0;
const int playChunk = 256;
static char emptyAudio[playChunk];

static int lastButtonAState;
static int buttonAState;
static int lastButtonBState;
static int buttonBState;

//const char* BotServiceUrl = "http://devkitdemobotapp.azurewebsites.net";
char* conversationId;

void initWiFi()
{
  Screen.print("IoT DevKit\r\n \r\nConnecting...\r\n");

  if (WiFi.begin() == WL_CONNECTED)
  {
    IPAddress ip = WiFi.localIP();
    Screen.print(1, ip.get_address());
    hasWifi = true;
    Screen.print(2, "Running... \r\n");
  }
  else
  {
    Screen.print(1, "No Wi-Fi\r\n ");
  }
}

void freeWavFile()
{
    if (audioBuffer != NULL)
    {
        free(audioBuffer);
        audioBuffer = NULL;
    }
}

void record()
{  
    memset(audioBuffer, 0x0, AUDIO_BUFFER_SIZE);
    Audio.format(8000, 16);
    
    // Start to record audio data
    Screen.print(0, "Recording...");
    Audio.startRecord(audioBuffer, AUDIO_BUFFER_SIZE, RECORD_DURATION);
}

void getWave()
{
    Screen.print(0, "Record Finish!");
    Audio.getWav(&waveFileSize);
    waveFileSize = Audio.convertToMono(audioBuffer, waveFileSize, 16);
    Serial.print("Recorded mono size: ");
    Serial.println(waveFileSize);
}

void play()
{
  Screen.clean();
  Screen.print(0, "playing...");

  Audio.attach(playCallback);
  Audio.format(8000, 16);
  Audio.startPlay();
  startPlay = true;

  /*
  while(playedSize < 70000)
  {
    delay(100);
  }

  Serial.println("Stop playing. ");
  Screen.print(0, "Stop playing");
  Audio.stop();
  */
}

void convertToStereo(char* data, int totalSize)
{
    uint16_t* pReader = (uint16_t *)data;    
    uint16_t* pWriter = (uint16_t *)malloc(totalSize*2);
    if (pWriter == NULL)
    {
        Serial.println("Memory allocation failed.");
    }
    
    while ((char*)pReader < data + totalSize)
    {
        *(pWriter++) = *pReader;
        *pWriter = *pReader;
        
        pReader++;
        pWriter++; 
    }
}

void playCallback(void)
{
    Serial.print("### playCallback, theta: ");
    Serial.println(theta);

    if (theta < playChunk)
    {
        Audio.write(emptyAudio, playChunk);
        return;
    }
        
    Audio.write(pAudioReader, playChunk);       
    playedSize += playChunk;
    theta -= playChunk;   
    pAudioReader += playChunk;
    
    if (pAudioReader > audioBuffer + AUDIO_BUFFER_SIZE - 45)
    {
        pAudioReader = audioBuffer;
    }    
}

void setResponseBodyCallback(const char* data, size_t dataSize)
{
    //Serial.print("*** Response callback, theta: ");
    //Serial.println(theta);
    
    if (status == 2)
    {
        enterReceivingState();
    }
    
    if (!startPlay && theta > 16000)
    {
      play();
    }
    
    int remaining = AUDIO_BUFFER_SIZE - (pAudioWriter - audioBuffer);
    if (remaining < dataSize)
    {
        memcpy(pAudioWriter, data, remaining);        
        memcpy(audioBuffer, data + remaining, dataSize - remaining);
        pAudioWriter = audioBuffer + dataSize - remaining;
        theta += dataSize;
        return;
    }
  
    memcpy(pAudioWriter, data, dataSize);
    pAudioWriter += dataSize;
    theta += dataSize;
    responseAudioSize += dataSize;
}

int sendVoiceCommand(const char *audioBinary, int audioSize)
{
    if (audioBinary == NULL || audioSize == 0)
    {
        Serial.println("Invalid audio binary.");
        return -1;
    }

    char requestUrl[128];
    sprintf(requestUrl, "http://demobotapp-sandbox.azurewebsites.net/conversation/%s", conversationId);
    Serial.println(requestUrl);

    // Setup response body buffer and callback
    pAudioWriter = audioBuffer;
    pAudioReader = audioBuffer;
    theta = 0;
    startPlay = false;
    playedSize = 0;
    responseAudioSize = 0;

    HTTPClient client = HTTPClient(HTTP_POST, requestUrl, setResponseBodyCallback);
    const Http_Response *response = client.send(audioBinary, audioSize);
    
    Serial.println("Got response from bot.");
    if (response != NULL && response->status_code == 200)
    {
      int contentLength = 0;
      
      // Get audio size from 'Content-Length' header
      KEYVALUE* headers = (KEYVALUE*)response->headers;
      while (headers != NULL)
      {
          KEYVALUE* prev = headers->prev;
          Serial.print(prev->key);
          Serial.print(": ");
          Serial.println(headers->value);
          
          if (strcmp(prev->key, "Content-Length") == 0)
          {         
            contentLength = atoi(headers->value);
          }
                 
          if (prev != NULL)
          {
            headers = prev->prev;
          }
      }

      Serial.print("Response body size: ");
      Serial.println(contentLength);

      if (!startPlay)
      {
          play();         
      }

      while (playedSize <= contentLength - playChunk)
      {
          delay(100);
      }

      Serial.println("Stop playing. ");
      Screen.print(0, "Stop playing");
      Audio.stop();

      return 0;
  }
  else
  {
      Serial.println(response->status_message);
      Serial.println(response->status_code);
      Serial.println(response->body);
      return -1;
  }
}

void enterIdleState()
{
    status = 0;
    Screen.clean();
    Screen.print(0, "Hold A to talk   ");
    Serial.println("Hold A to talk   ");
}

void enterRecordState()
{
    status = 1;
    Screen.clean();
    Screen.print(0, "Release B to send\r\nMax duraion: 3 sec");
    Serial.println("Release B to send    ");
}

void enterProcessingState()
{
    status = 2;
    Screen.clean();
    Screen.print(0, "Processing...");
    Screen.print(1, "Uploading...");
}

void enterReceivingState()
{
    status = 3;
    Screen.clean();
    Screen.print(0, "Processing...");
    Screen.print(1, "Receiving...");
}

void listenVoiceCommand()
{
    switch (status)
    {
        // Idle
        case 0:
            buttonAState = digitalRead(USER_BUTTON_A);
            if (buttonAState == LOW)
            {
                audioBuffer = (char *)malloc(AUDIO_BUFFER_SIZE + 1);
                if (audioBuffer == NULL)
                {
                    Serial.println("Not enough Memory! ");
                    return;
                }
                
                record();                
                status = 1;
                Screen.clean();
                Screen.print(0, "Release A to send\r\nMax duraion: 3 sec");  
            }           
            break;
        // Finish recording
        case 1:
            buttonAState = digitalRead(USER_BUTTON_A);
            if (buttonAState == HIGH)
            {
                getWave();
                if (waveFileSize > 0)
                {                   
                    enterProcessingState();
                }
                else
                {
                    Serial.println("No Data Recorded! ");
                    freeWavFile();
                    enterIdleState();
                }
            }
            break;
        case 2:
            sendVoiceCommand(audioBuffer, waveFileSize);
            freeWavFile();
            enterIdleState();
            break;
    }
}

void setup()
{
    Screen.init();
    Screen.print(0, "IoT DevKit");
    Screen.print(2, "Initializing...");
    
    Screen.print(3, " > Serial");
    Serial.begin(115200);
  
    // Initialize the WiFi module
    Screen.print(3, " > WiFi");
    hasWifi = false;
    initWiFi();
    if (!hasWifi)
    {
      return;
    }

    pinMode(USER_BUTTON_A, INPUT);
    lastButtonAState = digitalRead(USER_BUTTON_A);
    pinMode(USER_BUTTON_B, INPUT);
    lastButtonBState = digitalRead(USER_BUTTON_B);
    status = 0;

    memset(emptyAudio, 0x0, playChunk);
    
    // start a new conversation
    HTTPClient client = HTTPClient(HTTP_POST, "http://demobotapp-sandbox.azurewebsites.net/conversation");
    const Http_Response *response = client.send(NULL, NULL);
    if (response != NULL && response->status_code == 200)
    {
        conversationId = (char*)malloc(strlen(response->body) + 1);
        strcpy(conversationId, (char *)response->body);

        Serial.print("Initialize conversation successfully.\r\nConversationId: ");
        Serial.println(conversationId);
    }
    else
    {
        Serial.println("Initialize conversation failed.");
        return;
    }

    if (response){
      delete(response);
    }
}

void loop()
{
    if (hasWifi && conversationId != NULL)
    {
      listenVoiceCommand();
    }
    
    delay(LOOP_DELAY);
}

