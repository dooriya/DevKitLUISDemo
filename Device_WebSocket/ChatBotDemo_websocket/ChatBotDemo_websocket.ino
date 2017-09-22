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
  ringBuffer.clear();
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
       // printf("stop\r\n");
       // Audio.stop();
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

void SendBinary(Websocket *ws, char *message, int size){
  int sendTime = (size - 1) / 1024 + 1;
    for (int i = 0; i < sendTime; ++i) {
      char opcode = ((i == 0) ? 0x02 : 0x00);
      if (i == sendTime - 1) 
      {
        opcode = opcode | 0x80;
      }
      printf("opcode %d  send %d\r\n", opcode, (*ws).send(message + 1024 * i, (i == sendTime - 1) ? (size % 1024) : 1024, opcode));
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
//Websocket ws("ws://yiribot.azurewebsites.net/ws/ws?nickName=hah");
  int connect_error = ws.connect();
  printf("connect_error %d\r\n", connect_error);
  
while(true){
  
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
  //ringBuffer.clear();
    unsigned char opcode = 0;
    int len = 0;
    int first = true;
    while ((opcode & 0x80) == 0x00) {
      int tmp = ws.read(s, &len, &opcode, first);
      first = false;
      printf("tmp %d  len %d opecode %d\r\n", tmp, len, opcode);
      if (tmp == 0) break;
      setResponseBodyCallback(s, len);
    }
    while(ringBuffer.use() >= 256) delay(100);
    ringBuffer.clear();
    stop();
 }
}

void loop() {
  // put your main code here, to run repeatedly:

}
