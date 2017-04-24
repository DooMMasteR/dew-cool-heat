#include <Adafruit_LiquidCrystal.h>

#include <AM2320.h>
#include <BME280I2C.h>

#define GOVERN_INTERVAL 1000
#define DISPLAY_INTERVAL 1000
#define SCREEN_INTERVALL 4000
#define MOTOR_PIN A1
#define MOTOR_ENABLE_PIN 1

boolean auto_display = true;
boolean dirty_display = false;
byte display_pos = 0x00;
unsigned long last_display_cycle = 0;
unsigned long last_display_refresh = 0;
unsigned long last_govern = 0;
boolean motor_running = false;
byte motor_speed = 0;
enum class State : byte {
  off,
  manual,
  automatic
};

enum Onreason : byte {
  inactive,
  manual_override,
  dew_point,
  cooling,
  heating
};

Onreason onreason = inactive;
State state = State::off;

class SensorData {
  public:
    float hum = 0;
    float temp = 0;
    float pres = 0;
    float dew = 0;
};

SensorData sensord1;
SensorData sensord2;

//stock BME280 module
BME280I2C sensor1 = new BME280I2C(5, 5, 5, 1, 5, 2, false, 0x76);
//BME280 module with the SDO pin pulled to high (3-3.3V
BME280I2C sensor2 = new BME280I2C(5, 5, 5, 1, 5, 2, false, 0x77);
//LCD with I2C expander
Adafruit_LiquidCrystal lcd(0);


void setup() {
  // put your setup code here, to run once:
  last_display_cycle = millis() + SCREEN_INTERVALL + 1;
  last_display_refresh = millis() + DISPLAY_INTERVAL + 1 ;
  last_govern = millis() + GOVERN_INTERVAL + 1;
  sensor1.begin();
  sensor2.begin();
  lcd.begin(16, 2);
}

void loop() {
  // put your main code here, to run repeatedly:
  //bme280
  if (millis() - last_govern > GOVERN_INTERVAL) {
    updateDisplay();
    governMotor();    
    last_govern = millis();
  }
  if (millis() - last_display_cycle > SCREEN_INTERVALL && auto_display) {
    if (display_pos < 3) {
      display_pos++;
    } else {
      display_pos = 0;
    }
    last_display_cycle = millis();
  }
  if ((millis() - last_display_refresh > DISPLAY_INTERVAL && auto_display) || dirty_display) {
    dirty_display = false;
    updateDisplay();
    last_display_refresh = millis();
  }


}

void updateDisplay() {
  switch (display_pos) {
    case 0: {
        displayTemp();
      }
    case 1: {
        displayHum();
      }
    case 2: {
        displayDew();
      }
    case 3: {
        displayStatus();
      }
    default: {
        displayTemp();
      }
  }
}

void readSensors() {
  sensor1.read(sensord1.temp, sensord1.hum, sensord1.pres, sensord1.dew, true);
  sensor2.read(sensord2.temp, sensord2.hum, sensord2.pres, sensord2.dew, true);
}

void displayTemp() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("in temp: ");
  lcd.println(sensord1.temp);
  lcd.println("°C");
  lcd.setCursor(0, 1);
  lcd.print("out temp: ");
  lcd.println(sensord2.temp);
  lcd.println("°C");
}

void displayHum() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("in hum: ");
  lcd.println("%");
  lcd.println(sensord1.hum);
  lcd.setCursor(0, 1);
  lcd.print("out hum: ");
  lcd.println(sensord2.hum);
  lcd.println("%");
}

void displayDew() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("in dew: ");
  lcd.println("°C");
  lcd.println(sensord1.dew);
  lcd.setCursor(0, 1);
  lcd.print("out dew: ");
  lcd.println(sensord2.dew);
  lcd.println("°C");
}

void displayStatus() {
  
}

void governMotor() {
  if(state != State::automatic){
    return;
  }
  onreason = inactive;
  if(sensord1.dew > sensord2.dew && state == State::automatic){
    onreason = dew_point;
  }
  if(sensord1.temp > sensord2.temp && onreason == dew_point && sensord2.temp < 16){
    onreason = heating;
  }
  if(sensord1.temp < sensord2.temp && onreason == dew_point && sensord2.temp > 22){
    onreason = heating;
  }
  switch (onreason){
    case heating:{
      setMotorSpeed(true, 255);
    }
    case cooling:{
      setMotorSpeed(true, 255);
    }
    case dew_point:{
      setMotorSpeed(true, 64);
    }
    default:{
      setMotorSpeed(false, 0);
    }
  }
}

void setMotorSpeed(boolean set_state, byte set_speed) {
  analogWrite(MOTOR_PIN, set_speed);
  digitalWrite(MOTOR_ENABLE_PIN, set_state && state != State::off);
}

