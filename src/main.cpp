#include <Arduino.h>
#include <microDS3231.h>
#include <GyverTM1637.h>
#include <EncButton.h>
#include <TimerMs.h>
#include <EEPROM.h>

#define THRESHOLD 20

// Memory
#define ADDR_HOURS 3
#define ADDR_MINUTES 5
#define ADDR_DURATION 7

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
TimerMs scheduleTimer(35000, 1, 0);

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
  unsigned long seconds = int(storedDuration * (double)MAX_DURATION/1024);
  Serial.print("Duration: ");
  Serial.print(storedDuration);
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

void displaySave(){
    Serial.println("Display save");
    disp.displayByte(_S, _A, _U, _E);
    delay(1000);
    disp.clear();
}

void setTime(bool isSchedule){
  disp.brightness(6);

  if (isSchedule){
    Serial.println("Set schedule mode");
    disp.displayByte(_S, _C, _E, _D);
    clockTimer.stop();
  } else {
    Serial.println("Set time mode");
    disp.displayByte(_C, _l, _O, _C);
    clockTimer.start();
  }

  int savedHours = analogRead(DURATION_PIN);
  int savedMinutes = analogRead(TIME_PIN);
  bool showPoint = true;

  while(true){
    btn.tick();
    if (isSchedule){
      disp.point(true);
    }
    
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
      if (isSchedule){
        scheduledHour = convertAnalogValue(currentHours, 24);
        scheduledMinute = convertAnalogValue(currentMinutes, 60);        
        EEPROM.write(ADDR_HOURS, scheduledHour);
        EEPROM.write(ADDR_MINUTES, scheduledMinute);
      } else {
        rtc.setHMSDMY(convertAnalogValue(currentHours, 24), convertAnalogValue(currentMinutes, 60), 0, BUILD_DAY, BUILD_MONTH, BUILD_YEAR);
      }

      displaySave();
      return;
    }

    if (btn.click()){
      clockTimer.stop();
      if (isSchedule) {
        setTime(false);
        return;
      }
      return;
    }
  }
}

void setDuration() {
    Serial.println("Set duration");
    disp.brightness(6);
    disp.displayByte(_D, _U, _r, _empty);
    int savedDuration = analogRead(DURATION_PIN);
    
    while (true){
      btn.tick();

      int currentDuration = analogRead(DURATION_PIN);
      if (abs(currentDuration - savedDuration) > THRESHOLD){
        savedDuration = currentDuration;
        unsigned long seconds = convertAnalogValue(savedDuration, MAX_DURATION);
        disp.point(false);
        disp.displayInt(seconds);
        delay(10);
      }

      if (btn.held()){
        storedDuration = currentDuration;
        EEPROM.write(ADDR_DURATION, storedDuration);
        displaySave();
        return;
      }

      if (btn.click()){
        setTime(true);
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
  setDuration();
  Serial.println("Exit settings mode");
  delay(300);
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