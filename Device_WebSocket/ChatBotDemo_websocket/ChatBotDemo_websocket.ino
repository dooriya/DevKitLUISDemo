#include "mbed.h"
#include "AZ3166WiFi.h"
#include "http_client.h"
#include "AudioClassV2.h"
#include "Websocket.h"
#include "RingBuffer.h"
#include "SystemTickCounter.h"

#define HEARTBEAT_INTERVAL  60000
#define RING_BUFFER_SIZE 16000
#define PLAY_CHUNK 256

static bool hasWifi;
static bool connect_state;
static int lastButtonAState;
static int buttonAState;
static int lastButtonBState;
static int buttonBState;
static volatile int status;
static uint64_t hb_interval_ms;

static AudioClass& Audio = AudioClass::getInstance();

RingBuffer ringBuffer(RING_BUFFER_SIZE);
char readBuffer[2048];
char websocketBuffer[5000];

static char emptyAudio[PLAY_CHUNK];
Websocket *websocket;
bool startPlay = false;

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

int connectWebSocket()
{
    Screen.clean();
    Screen.print(0, "Connecting to WS.");
    char *url = getUrl();
    websocket = new Websocket(url);
    connect_state = (*websocket).connect();
    if (connect_state == 1)
    {
        Serial.println("WebSocket connect succeeded.");

        // Trigger heart beat immediately
        hb_interval_ms = -(HEARTBEAT_INTERVAL);
        sendHeartbeat();
        return 0;
    }
    else
    {
        Serial.print("WebSocket connect failed, connect_state: ");
        Serial.println(connect_state);
        return -1;
    }    
}

void sendHeartbeat()
{
    if ((int)(SystemTickCounterRead() - hb_interval_ms) < HEARTBEAT_INTERVAL)
    {
      return;
    }
  
    Serial.println(">>Heartbeat<<");  
    // Send haertbeart message
    (*websocket).send("heartbeat", 9);
    
    // Reset
    hb_interval_ms = SystemTickCounterRead();
}

void record()
{
    ringBuffer.clear();
    Audio.format(8000, 16);
    Audio.attachPlay(NULL);
    Audio.attachRecord(recordCallback);
    Audio.startRecord();
}

void play()
{
    Serial.println("start playing");
    enterPlayingState();
    
    Audio.attachRecord(NULL);
    Audio.attachPlay(playCallback);
    Audio.format(8000, 16);
    Audio.startPlay();
    startPlay = true;
}

void stop()
{
    Audio.stop();
    Audio.attachRecord(NULL);
    Audio.attachPlay(NULL);
    startPlay= false;
}

void playCallback(void)
{
    if (ringBuffer.use() < PLAY_CHUNK)
    {
      Audio.write(emptyAudio, PLAY_CHUNK);
      return;
    }
    ringBuffer.get((uint8_t*)readBuffer, PLAY_CHUNK);
    Audio.write(readBuffer, PLAY_CHUNK);
}

void recordCallback(void)
{
    Audio.read(readBuffer, 2048);
    ringBuffer.put((uint8_t*)readBuffer, 2048);
}

void setResponseBodyCallback(const char* data, size_t dataSize)
{
    while(ringBuffer.available() < dataSize)
    {
        //Serial.print("ringBuffer available:");
        //Serial.println(ringBuffer.available());
        delay(50);
    }
    
    ringBuffer.put((uint8_t*)data, dataSize);
    if (ringBuffer.use() > RING_BUFFER_SIZE / 2 && startPlay == false)
    {
        play();
    }
}

char* getUrl()
{
    char *url;
    url = (char *)malloc(300);
    if (url == NULL)
    {
      return NULL;
    }
    HTTPClient guidRequest = HTTPClient(HTTP_GET, "http://www.fileformat.info/tool/guid.htm?count=1&format=text&hyphen=true");
    const Http_Response* _response = guidRequest.send();
    if (_response == NULL)
    {
      printf("Guid generator HTTP request failed.\r\n");
      return NULL;
    }
    
    snprintf(url, 300, "%s%s", "ws://demobotapp-sandbox.azurewebsites.net/chat?nickName=", _response->body);
    printf("url: %s\r\n", url);
    return url;
}

void enterIdleState()
{
    status = 0;
    Screen.clean();
    Screen.print(0, "Press A to start\r\nconversation");
}

void enterActiveState()
{
    status = 1;
    Screen.clean();
    Screen.print(0, "Hold B to talk   ");
    Screen.print(1, "Or press A to stop conversation", true);
    Serial.println("Hold B to talk or press A to stop conversation");
}

void enterRecordingState()
{
    status = 2;
    Screen.clean();
    Screen.print(0,"recording...");
    Screen.print(1, "Release B to send    ");
    Serial.println("Release B to send    ");
}

void enterReceivingState()
{
    status = 3;
    Screen.clean();
    Screen.print(0, "Processing...");
    Screen.print(1, "Receiving...");
}

void enterPlayingState()
{
    status = 4;
    Screen.clean();
    Screen.print(1, "Playing...");
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

    memset(emptyAudio, 0x0, PLAY_CHUNK);
    //connectWebSocket();
    enterIdleState();
}

void loop()
{
    if (hasWifi)
    {
        doWork();
    }   
}

void doWork()
{
    switch (status)
    {
        // Idle
        case 0:
            buttonAState = digitalRead(USER_BUTTON_A);
            if (buttonAState == LOW)
            {
                if (connectWebSocket() == 0)
                {
                    enterActiveState();
                }
            }
          break;

        // Active state, ready for conversation
        case 1:
            buttonBState = digitalRead(USER_BUTTON_B);
            if (buttonBState == LOW)
            {
                (*websocket).send("pcmstart", 4, 0x02);
                record();
                enterRecordingState();
            }

            buttonAState = digitalRead(USER_BUTTON_A);
            if (buttonAState == LOW)
            {
                if((*websocket).close())
                {
                    Serial.println("WebSocket stop succeeded.");
                    Screen.print("conversation stop.");
                    delay(200);
                    connect_state = 0;
                    enterIdleState();
                }
                else
                {
                    Serial.println("WebSocket close failed.");
                }
            }

            sendHeartbeat();
            
            break;
            
        // Recording state
        case 2:
            while(digitalRead(USER_BUTTON_B) == LOW || ringBuffer.use() > 0)
            {
                if (digitalRead(USER_BUTTON_B) == HIGH)
                {
                    stop();
                }
                
                int sz = ringBuffer.get((uint8_t*)websocketBuffer, 2048);
                if (sz > 0)
                {
                  (*websocket).send(websocketBuffer, sz, 0x00);
                }
            }
    
            if (Audio.getAudioState() == AUDIO_STATE_RECORDING)
            {
                stop();
            }
            
            // Mark binary message end
            (*websocket).send("pcmend", 4, 0x80);
            Serial.println("Your voice message is sent.");
            enterReceivingState();
          break;

        // receiving and playing
        case 3:           
            unsigned char opcode = 0;
            int len = 0;
            bool firstReceived = true;
            while ((opcode & 0x80) == 0x00)
            {
                int receivedSize = (*websocket).read(websocketBuffer, &len, &opcode, firstReceived);
                //printf("receivedSize %d recv len %d opcode %d\r\n", receivedSize, len, opcode);
                
                if (receivedSize == 0){
                    break;
                }
                firstReceived = false;
                setResponseBodyCallback(websocketBuffer, len);
            }
        
            if (startPlay == false)
            {
              play();
            }
            
            while(ringBuffer.use() >= PLAY_CHUNK)
            {
                delay(100);
            }
            stop();
            enterActiveState();
    }
}

