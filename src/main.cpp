#include <Arduino.h>
#include <microDS3231.h>
#include <GyverTM1637.h>
#include <EncButton.h>
#include <TimerMs.h>
#include <EEPROM.h>

#define THRESHOLD 30

// Memory
#define ADDR_HOURS 0
#define ADDR_MINUTES 1
#define ADDR_DURATION 2

// Hardware pins
#define PUMP_PIN 4
#define BTN_PIN 2
#define GERKON_PIN 3

// Potentiometers
#define MAX_DURATION 61
#define DURATION_PIN A3
#define TIME_PIN A2

// Display
#define CLK 5
#define DIO 6
GyverTM1637 disp(CLK, DIO);

// Library Object
EncButton<EB_TICK, BTN_PIN> btn;
MicroDS3231 rtc;

TimerMs clockTimer(500, 0, 0);
TimerMs scheduleTimer(30000, 1, 0);

// Default values
int scheduledHour = 0;
int scheduledMinute = 0;
int storedDuration = 2;

void setup()
{
  pinMode(BTN_PIN, INPUT_PULLUP);
  pinMode(GERKON_PIN, INPUT_PULLUP);
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(9600);

  if (!rtc.begin()) {
    Serial.println("DS3231 not found");
    disp.brightness(6);
    disp.displayByte(_C, _l, _o, _c);
    delay(1000);
    disp.clear();
    disp.brightness(0);
  } else {
    Serial.print(rtc.getDateString());
    Serial.print(" ");
    Serial.println(rtc.getTimeString());
  }

  if (rtc.lostPower()) {
    Serial.println("lost battery");
    disp.brightness(6);
    disp.displayByte(_B, _a, _t, _empty);
    rtc.setTime(BUILD_SEC, BUILD_MIN, BUILD_HOUR, BUILD_DAY, BUILD_MONTH, BUILD_YEAR);
    delay(1000);
    disp.clear();
    disp.brightness(0);
  }

  scheduledHour = EEPROM.read(ADDR_HOURS);
  scheduledMinute = EEPROM.read(ADDR_MINUTES);
  storedDuration = EEPROM.read(ADDR_DURATION);

  Serial.println("Start!");
}

void startWater(){
  byte duration = EEPROM.read(ADDR_DURATION);
  unsigned long seconds = int(duration * (double)MAX_DURATION/1024);
  Serial.print("Duration: ");
  Serial.print(duration);
  Serial.print(" = ");
  Serial.print(seconds);
  Serial.println("s");

  if (digitalRead(GERKON_PIN) == LOW){
    Serial.println("No water");
    disp.brightness(6);
    disp.displayByte(_F, _i, _l, _l);
    delay(1000);
    disp.clear();
    disp.brightness(0);
    return;
  }

  Serial.println("Start pump"); 
  disp.brightness(6);
  digitalWrite(PUMP_PIN, LOW);
  for(int i = seconds; i > 0; i--){
    disp.clear();
    disp.displayInt(i);
    delay(1000);
  }
  disp.clear();
  disp.brightness(0);
  Serial.println("End pump");
}

int convertAnalogValue(int value, int max){
  return (int)(value * (double)max/1024);
}

void setTime(){
  Serial.println("Set time mode");
  int savedHours = analogRead(DURATION_PIN);
  int savedMinutes = analogRead(TIME_PIN);
  bool showPoint = true;
  clockTimer.start();

  while(true){
    btn.tick();
    if (clockTimer.tick()){
      showPoint = !showPoint;
      disp.point(showPoint);
    }
    
    int currentHours = analogRead(DURATION_PIN);
    int currentMinutes = analogRead(TIME_PIN);

    if (abs(currentHours - savedHours) > THRESHOLD || abs(currentMinutes - savedMinutes) > THRESHOLD){
      savedHours = currentHours;
      savedMinutes = currentMinutes;
      disp.displayClock(convertAnalogValue(currentHours, 24), convertAnalogValue(currentMinutes, 60));
    }

    if (btn.held()){
      clockTimer.stop();
      rtc.setHMSDMY(convertAnalogValue(currentHours, 24), convertAnalogValue(currentMinutes, 60), 0, BUILD_DAY, BUILD_MONTH, BUILD_YEAR);
      return;
    }

    if (btn.click()){
      clockTimer.stop();
      return;
    }
  }
}

bool shouldRunByTime(){
  Serial.println(rtc.getTimeString());
  Serial.print(rtc.getHours());
  Serial.print("=");
  Serial.print(scheduledHour);
  Serial.print("; ");
  Serial.print(rtc.getMinutes());
  Serial.print("=");
  Serial.println(scheduledMinute);
  return rtc.getHours() == scheduledHour && rtc.getMinutes() == scheduledMinute;
}

void settingsMode(){
  Serial.println("Settings mode");
  disp.brightness(6);
  disp.displayByte(_S, _e, _t, _empty);
  int savedDuration = analogRead(DURATION_PIN);
  int savedTime = analogRead(TIME_PIN);

  while(true){
    btn.tick();

    int currentDuration = analogRead(DURATION_PIN);
    if (abs(currentDuration - savedDuration) > THRESHOLD){
      savedDuration = currentDuration;
      unsigned long seconds = convertAnalogValue(savedDuration, MAX_DURATION);
      disp.point(false);
      disp.displayInt(seconds);
      delay(10);
    }

    int currentTime = analogRead(TIME_PIN);
    if (abs(currentTime - savedTime) > THRESHOLD){
      savedTime = currentTime;
      unsigned long minutes = convertAnalogValue(savedTime, 1440);
      disp.point(true);
      disp.displayClock((int)minutes/60, (int)minutes%60);
      delay(10);
    }

    if (btn.click()){
      setTime();
      return;
    }

    if (btn.held()){
      scheduledHour = (int)convertAnalogValue(savedTime, 1440)/60;
      scheduledMinute = (int)convertAnalogValue(savedTime, 1440)%60;
      storedDuration = savedDuration;

      EEPROM.write(ADDR_DURATION, storedDuration);
      EEPROM.write(ADDR_HOURS, scheduledHour);
      EEPROM.write(ADDR_MINUTES, scheduledMinute);
      Serial.println("Exit settings mode");
      return;
    }
  }
}

void loop()
{
  btn.tick();
  
  if (scheduleTimer.tick()){
    if (shouldRunByTime()){
      Serial.println("Scheduled run");
      startWater();
    }
  }

  if (btn.click()) {
    startWater();
  }

  if (btn.held()){
    settingsMode();
    disp.clear();
    disp.point(false);
    disp.brightness(0);
  }

  digitalWrite(PUMP_PIN, HIGH);
}