#include "mbed.h"
#include "AZ3166WiFi.h"
#include "http_client.h"
#include "AudioClassV2.h"
#include "Websocket.h"
#include "RingBuffer.h"

void InitWiFi()
{
  Screen.print("WiFi \r\n \r\nConnecting...\r\n             \r\n");
  
  if(WiFi.begin() == WL_CONNECTED)
  {
    printf("connected \r\\n");
  }
}
AudioClass& Audio = AudioClass::getInstance();
const int RING_BUFFER_SIZE = 15000;
const int playChunk = 4096;
char readBuffer[2048];
bool startPlay = false;
static char emptyAudio[256];
RingBuffer ringBuffer(RING_BUFFER_SIZE);

void play()
{
  printf("start play\r\n");
  Audio.attachPlay(playCallback);
  Audio.format(8000, 16);
  Audio.startPlay();
  startPlay = true;
}

void record()
{   
    ringBuffer.clear();
    Audio.format(8000, 16);
    Audio.attachRecord(recordCallback);
    Audio.startRecord(NULL, NULL, 0);
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

void stop()
{
    Audio.stop();
    startPlay= false;
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
    delay(100);
  }
  ringBuffer.put((uint8_t*)data, dataSize);
  if (ringBuffer.use() > RING_BUFFER_SIZE / 2 && startPlay == false) {
    play();
  }
}

void setup() {
  
  pinMode(USER_BUTTON_A, INPUT);
  if (WiFi.begin() == WL_CONNECTED)
  {
    printf("connected\r\n");
  }

  char url[300] = "";
    HTTPClient guidRequest = HTTPClient(HTTP_GET, "http://www.fileformat.info/tool/guid.htm?count=1&format=text&hyphen=true");
    const Http_Response* _response = guidRequest.send();
    if (_response == NULL)
    {
        printf("Guid generator HTTP request failed.\r\n");
      return;
    }
    snprintf(url, 300, "%s%s", "ws://yiribot.azurewebsites.net/chat?nickName=", _response->body);
    printf("%s\r\n", url);
    
  char *s;
  s = (char *)malloc(5000);
  if (s == NULL)
  {
      printf("s is null\r\n");  
  }
  memset(emptyAudio, 0x0, 256);
  Websocket ws(url);
  int connect_state = ws.connect();
  printf("connect_state %d\r\n", connect_state);
  
while(true){
  printf("you can start a new question now\r\n");
  ws.send("pcmstart", 4, 0x02);
  while(digitalRead(USER_BUTTON_A) == HIGH) delay(10);
  record();
  while(digitalRead(USER_BUTTON_A) == LOW || ringBuffer.use() > 0)
  {
    if (digitalRead(USER_BUTTON_A) == HIGH) stop();
    int sz = ringBuffer.get((uint8_t*)s, 2048);
    if (sz > 0) {
      ws.send(s, sz, 0x00);
    }
  }
  ws.send("pcmend", 4, 0x80);
  delay(100);
  printf("your question send\r\n");
    ringBuffer.clear();
    unsigned char opcode = 0;
    int len = 0;
    bool first = true;
    while ((opcode & 0x80) == 0x00) {
      int tmp = ws.read(s, &len, &opcode, first);
      printf("tmp %d recv len %d opcode %d\r\n", tmp, len, opcode);
      first = false;
      if (tmp == 0) break;
      setResponseBodyCallback(s, len);
    }
    if (startPlay == false) play();
    while(ringBuffer.use() >= 256) delay(100);
    ringBuffer.clear();
    stop();
 }
}

void loop() {
  // put your main code here, to run repeatedly:

}
