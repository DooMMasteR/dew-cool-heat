#include <Adafruit_LiquidCrystal.h>
#include <HashMap.h>

#include <AM2320.h>
#include <BME280I2C.h>

#define GOVERN_INTERVAL 100
#define DISPLAY_INTERVAL 1000
#define SCREEN_INTERVALL 4000
#define MOTOR_PIN 1
#define MOTOR_ENABLE_PIN 2
#define OFF_SWITCH_PIN 3
#define HAND_SWITCH_PIN 4
#define AUTO_SWITCH_PIN 5
#define TOR_SWITCH_PIN 6
#define POTI_PIN A0

CreateHashMap(ledPins, String, int, 4);

boolean auto_display = true;
boolean dirty_display = false;
byte display_pos = 0x00;
unsigned long last_display_cycle = 0;
unsigned long last_display_refresh = 0;
unsigned long last_govern = 0;
boolean motor_running = false;
byte motor_speed = 0;
boolean tor_offen = false;
enum class State : byte {
  off,
  manual,
  automatic
};

enum class OnReason : byte {
  inactive,
  manual_override,
  dew_point,
  cooling,
  heating
};

OnReason onreason = OnReason::inactive;
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
    checkUI();
    readSensors();
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
  onreason = OnReason::inactive;
  if(sensord1.dew > sensord2.dew && state == State::automatic){
    onreason = OnReason::dew_point;
  }
  if(sensord1.temp > sensord2.temp && onreason == OnReason::dew_point && sensord2.temp < 16){
    onreason = OnReason::heating;
  }
  if(sensord1.temp < sensord2.temp && onreason == OnReason::dew_point && sensord2.temp > 22){
    onreason = OnReason::heating;
  }
  switch (onreason){
    case OnReason::heating:{
      setMotorSpeed(255);
    }
    case OnReason::cooling:{
      setMotorSpeed(255);
    }
    case OnReason::dew_point:{
      setMotorSpeed(64);
    }
    default:{
      setMotorSpeed(0);
    }
  }
}

void setMotorSpeed(byte set_speed) {
  if(tor_offen){
    set_speed = set_speed / 2;
  }
  if(state == State::manual){
    set_speed = map(analogRead(POTI_PIN),0,1023,10,255);
  }
  if(motor_speed > set_speed) {
    motor_speed = motor_speed - ((motor_speed - set_speed)/2 - 1);
  } else if (motor_speed < set_speed) {
    motor_speed = motor_speed + ((set_speed - motor_speed)/2 + 1);
  }
  analogWrite(MOTOR_PIN, motor_speed);
  digitalWrite(MOTOR_ENABLE_PIN, state != State::off);
}

void checkUI(){
  boolean s_off = (digitalRead(OFF_SWITCH_PIN) == LOW);
  boolean s_hand = (digitalRead(HAND_SWITCH_PIN) == LOW);
  boolean s_auto = (digitalRead(AUTO_SWITCH_PIN) == LOW);
  boolean tor_offen = (digitalRead(TOR_SWITCH_PIN) == LOW);
  if(s_off) {
    state = State::off;    
    return;
  }
  if(s_hand && !s_off && !s_auto) {
    state = State::manual;
    return;
  }
  if(s_auto && !s_hand && !s_off) {
    state = State::automatic;
  }
  state = State::off;
}

