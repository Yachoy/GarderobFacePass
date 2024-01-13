#include <CustomStepper.h>            // библиотекa управления шаговым двигателем. По умолчанию настроена на двигатель 28BYJ-48-5V
#include <Arduino.h>
#include "WiFi.h"
#include "AsyncUDP.h"

#define FULLD 390
#define MAX_TRYES_CONNECT_WIFI 15
#define MAX_BUFFER_COMMANDS_FROM_SERVER 10
#define COUNT_ITEMS 8
const char* ssid = "***";
const char* pass = "***";
const uint16_t port = 9669;

struct RGBIndicator{
  private:
    uint8_t r, b, g;
    void Blink(uint8_t pin, int count, int mls){
        for (int i = 0; i < count; i++){
            digitalWrite(pin, HIGH);
            delay(mls);
            digitalWrite(pin, LOW);
            delay(mls);
        }
    }
  public:
    RGBIndicator(uint8_t red, uint8_t green, uint8_t blue){
        r = red;
        b = blue;
        g = green;
        pinMode(r, OUTPUT);
        pinMode(b, OUTPUT);
        pinMode(g, OUTPUT);
    }
    void Red(bool blink = false, int count = 8, int mls = 150){
        digitalWrite(r, HIGH);
        digitalWrite(b, LOW);
        digitalWrite(g, LOW);
        if (blink)
            Blink(r, count, mls);
    }
    void Blue(bool blink = false, int count = 8, int mls = 150){
        digitalWrite(r, LOW);
        digitalWrite(b, HIGH);
        digitalWrite(g, LOW);
        if (blink)
            Blink(b, count, mls);
    }
    void Green(bool blink = false, int count = 8, int mls = 150){
        digitalWrite(r, LOW);
        digitalWrite(b, LOW);
        digitalWrite(g, HIGH);
        if (blink)
            Blink(g, count, mls);
    }
};

struct line{
        String com;
        String arg = "";
        String uid = "";
        line(String command, String arg, String uid) : com(command), arg(arg), uid(uid){}
};

line* lines_udp[MAX_BUFFER_COMMANDS_FROM_SERVER];

CustomStepper stepper(18,19,22, 23); 
RGBIndicator rgb (16,17,21);
AsyncUDP udp;


// Set your Static IP address
IPAddress local_IP(192, 168, 1, 1);
// Set your Gateway IP address
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 0, 0);
IPAddress primaryDNS(8, 8, 8, 8); //optional
IPAddress secondaryDNS(8, 8, 4, 4); //optional

line* MakeLineFromUdpString(String dat){
      const char* packetBuffer = dat.c_str();
      bool writeCommand = false, writeArg = false, writeId = false;
      String arg = "", id = "", command = "";
      for (int i{0};i<dat.length();i++){
        char ch = (char)packetBuffer[i];

        if(ch == '~'){
            writeCommand = true;
            writeArg = false; writeId = false;
            continue;
        }
        if(ch == '`'){
            writeCommand = false;
            writeArg = true; writeId = false;
            continue;
        }
        if(ch == '!'){
            writeCommand = false;
            writeArg = false; writeId = true;
            continue;
        }

        if(writeCommand)
            command += ch;
        if(writeArg)
            arg += ch;
        if(writeId)
            id += ch;
        } // end for

      return new line(command, arg, id);
    }


void stop_rotate(){
  stepper.setDirection(STOP);
}

void start_infinity_rotate(){
  stop_rotate();
  stepper.setDirection(CW);
  stepper.rotate();  // Будет вращать пока не получит команду о смене направления или пока не получит директиву STOP
}

void rotate(float angle){
    stop_rotate();
    stepper.setDirection(CW);
    Serial.println("Rot "+ String(angle * FULLD / 360));
    stepper.rotateDegrees(angle * FULLD / 360);        // Поворачивает вал на заданное кол-во градусов. Можно указывать десятичную точность (например 90.5), но не принимаются отрицательные значения
}


int tryes = MAX_TRYES_CONNECT_WIFI;
void setup()
{
  pinMode(15, INPUT_PULLUP); // BTN
  pinMode(5, INPUT_PULLUP); // Gercon
  stepper.setRPM(12);                 // Устанавливаем кол-во оборотов в минуту
  stepper.setSPR(4075.7728395);       // Устанавливаем кол-во шагов на полный оборот. Максимальное значение 4075.7728395

  Serial.begin(115200);
  
  for(int i{0}; i < MAX_BUFFER_COMMANDS_FROM_SERVER; i++){
      lines_udp[i] = NULL;
    }

  if(!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("STA Failed to configure");
    return;
  }
  WiFi.begin(ssid, pass);

  // Проверяем статус. Если нет соединения, то выводим сообщение о подключении
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print("TryWiFi. \n");
    tryes -= 1;
    if (tryes <= 0){
      Serial.println("FailWIFI\n");
      rgb.Red(true, 8, 100);
      return;
    }
    rgb.Blue(true, 2, 100);
  }

  Serial.print("Local IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("Subnet Mask: " );
  Serial.println(WiFi.subnetMask());
  Serial.print("Gateway IP: ");
  Serial.println(WiFi.gatewayIP());
  Serial.print("DNS 1: ");
  Serial.println(WiFi.dnsIP(0));
  Serial.print("DNS 2: ");
  Serial.println(WiFi.dnsIP(1));

  udp.listen(port);
  udp.onPacket([](AsyncUDPPacket packet) {
    String data = (const char*)packet.data();
    for(int i{0}; i < MAX_BUFFER_COMMANDS_FROM_SERVER; i++){
      if(lines_udp[i] != NULL) continue;
      lines_udp[i] = MakeLineFromUdpString(data);
      break;
    }
    }
  );
  rgb.Red(true, 3);
  rgb.Green(true, 3);
  rgb.Blue(true, 3);
  start_infinity_rotate();
}


bool pressed = false;
int local_index = 0;
const int angle_on_one_item = (360/COUNT_ITEMS);
bool one_time_find_zero = false;
void loop()
{
  if (!one_time_find_zero && !digitalRead(5)){
    stop_rotate();
    one_time_find_zero = true;
  }


  for (int i = 0; i < MAX_BUFFER_COMMANDS_FROM_SERVER; i++)
  {
    if (lines_udp[i] == NULL) continue;
    line* task = new line(lines_udp[i]->com,lines_udp[i]->arg, lines_udp[i]->uid);
    delete lines_udp[i];
    lines_udp[i] = NULL;
    Serial.println("Get task: "+task->com + " "+task->arg +" "+ task->uid);
    switch (task->com[0])
        {
          case 'r':{
            rgb.Red(true, 5);
            break;
          }
          case 'g':{
            rgb.Green(true, 5);
            break;
          }
          case 'b':{
            rgb.Blue(true, 5);
            break;
          }
          case 'm':{
          int id = (float)(task->arg[0]-'0');
            Serial.print("ROTATE (" + task->arg +") ");
            if (stepper.isDone()){
                int angle = id * angle_on_one_item;

                if(local_index < angle){
                  rotate(angle - local_index);
                  Serial.println("from " + String(local_index) +" to " + String(angle) + " -> "+ String(angle - local_index));
                }else if(local_index > angle){
                  rotate(360 - local_index + angle);
                  Serial.println("from " + String(local_index) +" to " + String(angle) + " -> "+ String(360 - local_index + angle));
                }
                local_index = angle;
                break;
            }
          }
          default:{
            Serial.print("Unknow command"); 
            Serial.println(task->com);
            break;
          }
        }
        break;
  }
  rgb.Blue();
  if (!pressed && !digitalRead(15)){
      pressed = true;
      Serial.println("pressed");
      udp.broadcastTo("triggered", port);
  }
  if (digitalRead(15)){
    pressed = false;
  }
  
  
  // if (stepper.isDone())
  // {
  //   delay(1000);
  //   start_infinity_rotate();
  // }
  stepper.run();                      // Этот метод обязателен в блоке loop. Он инициирует работу двигателя, когда это необходимо
}
