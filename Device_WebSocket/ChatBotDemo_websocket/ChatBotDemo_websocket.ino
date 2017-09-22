#include "mbed.h"
#include "AZ3166WiFi.h"
#include "http_client.h"
#include "AudioClassV2.h"
#include "Websocket.h"
#include "RingBuffer.h"

static bool hasWifi;
static bool connect_state;
static int lastButtonAState;
static int buttonAState;
static int lastButtonBState;
static int buttonBState;
static volatile int status;

static AudioClass& Audio = AudioClass::getInstance();
const int RING_BUFFER_SIZE = 15000;
RingBuffer ringBuffer(RING_BUFFER_SIZE);
char readBuffer[2048];
char websocketBuffer[5000];
bool startPlay = false;
static char emptyAudio[256];
Websocket *websocket;

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


void play()
{
  printf("start play\r\n");
  Audio.attachRecord(NULL);
  Audio.attachPlay(playCallback);
  Audio.format(8000, 16);
  Audio.startPlay();
  startPlay = true;
}

void record()
{   
  ringBuffer.clear();
  Audio.format(8000, 16);
  Audio.attachPlay(NULL);
  Audio.attachRecord(recordCallback);
  Audio.startRecord(NULL, NULL, 0);
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
  if (ringBuffer.use() < 256)
  {
    Audio.write(emptyAudio, 256);
    return;
  }
  ringBuffer.get((uint8_t*)readBuffer, 256);
  Audio.write(readBuffer, 256);
}

void recordCallback(void)
{
  Audio.read(readBuffer, 2048);
  ringBuffer.put((uint8_t*)readBuffer, 2048);
}

void setResponseBodyCallback(const char* data, size_t dataSize)
{
  while(ringBuffer.available() < dataSize) {
    printf("ringBuffer ava %d\r\n", ringBuffer.available());
    delay(10);
  }
  ringBuffer.put((uint8_t*)data, dataSize);
  if (ringBuffer.use() > RING_BUFFER_SIZE / 2 && startPlay == false) {
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
  snprintf(url, 300, "%s%s", "ws://yiribot.azurewebsites.net/chat?nickName=", _response->body);
  printf("url: %s\r\n", url);
  return url;
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

    memset(emptyAudio, 0x0, 256);
    char *url = getUrl();
    websocket = new Websocket(url);
    connect_state = (*websocket).connect();
    printf("connect_state %d\r\n", connect_state);
    enterIdleState();
}

void enterIdleState()
{
    status = 0;
    Screen.clean();
    Screen.print(0, "Hold A to talk   ");
    Serial.println("Hold A to talk   ");
}

void enterRecordingState()
{
    status = 1;
    Screen.clean();
    Screen.print(0, "Release A to send\r\nMax duraion: 3 sec");
    Serial.println("Release A to send    ");
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
    int sendBytes;
    switch (status)
    {
        // Idle state
        case 0:
            buttonAState = digitalRead(USER_BUTTON_A);
            if (buttonAState == LOW)
            {
                (*websocket).send("pcmstart", 4, 0x02);
                record();
                enterRecordingState();
            }

          break;
        // Recording state
        case 1:        
            sendBytes = ringBuffer.get((uint8_t*)websocketBuffer, 2048);
            if (sendBytes > 0)
            {
              (*websocket).send(websocketBuffer, sendBytes, 0x00);
              printf("send bytes %d\r\n", sendBytes);
            }
            
            buttonAState = digitalRead(USER_BUTTON_A);
            
            if (buttonAState == HIGH)
            {
                stop();
                printf("send remaining %d \r\n", ringBuffer.use());
                // send remainig recorded audio
                while(ringBuffer.use() > 0)
                {
                  sendBytes = ringBuffer.get((uint8_t*)websocketBuffer, 2048);
                  if (sendBytes > 0) {
                    (*websocket).send(websocketBuffer, sendBytes, 0x00);
                    printf("send bytes 2 %d\r\n", sendBytes);
                  }
                }

                (*websocket).send("pcmend", 4, 0x80);
                enterReceivingState();
            }
          break;

        // receiving state
        case 2:
            unsigned char opcode = 0;
            int len = 0;
            bool first = true;
            
            while ((opcode & 0x80) == 0x00)
            {
              int chunk = (*websocket).read(websocketBuffer, &len, &opcode, first);
              printf("chunk %d recv len %d opcode %d\r\n", chunk, len, opcode);
              first = false;
              if (chunk == 0)
              {
                break;
              }
              setResponseBodyCallback(websocketBuffer, len);
            }
            
            if (startPlay == false) play();
            while(ringBuffer.use() >= 256) delay(100);
            stop();
            enterIdleState();
            break;
    }
}

void loop()
{
  // put your main code here, to run repeatedly:
  printf("you can start a new question now\r\n");
  (*websocket).send("pcmstart", 4, 0x02);
  enterIdleState();
  while(digitalRead(USER_BUTTON_A) == HIGH) delay(10);
  enterRecordingState();
  record();
  while(digitalRead(USER_BUTTON_A) == LOW || ringBuffer.use() > 0)
  {
    if (digitalRead(USER_BUTTON_A) == HIGH) stop();
    int sz = ringBuffer.get((uint8_t*)websocketBuffer, 2048);
    if (sz > 0) {
      (*websocket).send(websocketBuffer, sz, 0x00);
    }
  }
  stop();
  (*websocket).send("pcmend", 4, 0x80);
  printf("your question send\r\n");
  enterReceivingState();
  unsigned char opcode = 0;
  int len = 0;
  bool first = true;
  while ((opcode & 0x80) == 0x00) {
    int tmp = (*websocket).read(websocketBuffer, &len, &opcode, first);
    printf("tmp %d recv len %d opcode %d\r\n", tmp, len, opcode);
    first = false;
    if (tmp == 0) break;
    setResponseBodyCallback(websocketBuffer, len);
  }
  if (startPlay == false) play();
  while(ringBuffer.use() >= 256) delay(100);
  stop();
     /*if (connect_state)
      {
        listenVoiceCommand();
      }*/    
}
