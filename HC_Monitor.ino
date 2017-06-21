// HC Monitor
// Written By: Robert Vinci includes various BME and AdaFruit libraries
// BME280 sensor captures temperature, humidity and pressure
// B2.00.00 - 06/20/17 Fixed Renamed HC Monitor from Home Zone Control
// B1.10.01 - 06/19/17 Fixed "Scheduled Sleep" bug
// B1.10.00 - 06/19/17 Added Ubidots webhooks, OTA Update, scheduled sleep
// B1.01.00 - 06/14/17 Fixed No Sensor bug that hangs; added systStatus and constants
#undef  DEBUG     //Comment out to debug
#include "Adafruit_Sensor.h"
#include "Adafruit_BME280.h"
#include "TempHumHW.h"
#include "Ubidots.h"

/****************************************
 * Constants
 ****************************************/
#define TOKEN "3qdJ9TCWeflFCtxVToFss2sExfY6FG"  // Put here your Ubidots TOKEN
//Photon 5
#define VAR_ID "59482375762542798733437f"  // Put here your data source name
#define VAR_LABEL "update-otap5" // Put here your variable api label
#define DEVICE_LABEL "35003a001947333438373338" // Put here your device api label
//Photon 6
//#define VAR_ID "59432bc576254271e7ff5652"  // Put here your data source name
//#define VAR_LABEL "update-otap6" // Put here your variable api label
//#define DEVICE_LABEL "56004a001451353432393433" // Put here your device api label

//#define BME_SCK D4
//#define BME_MISO D3
//#define BME_MOSI D2
//#define BME_CS D5
/*#define RED_LED D7
#define GRN_LED D6
#define BLU_LED D5*/
#define BME_ADDR 0x76     //Address of BME device
#define BME_NOSENSOR 0xFF //Status value if no sensor attached or sensor has failed
#define BME_SYSTEM_OK 0
#define BME_SYSTEM_FAIL -1
#define BME_SYSTEM_NOSENSOR -2

#define SEALEVELPRESSURE_HPA (1013.25)
#define cSecs(a,b) ((a*60)+b) * 60

Adafruit_BME280 bme; // I2C

//NOTE: All calculations assume degrees Fahrenheit, conversions to Celcius where necessary for display
#define WAKE 1    //Wakeup heat ON time
#define MORN 2    //Morning setback time
#define EVEN 3    //Evening heat ON time
#define NGHT 4    //Night setback time
#define DEG_F 0
#define DEG_C 1
double setpoint = 60.0; //Setpoint temperature for zone heating
int SleepPeriod = 30;  //seconds
double temperature = -99.0;
String(sTemperature) = "-99.0";
double humidity = -99.0;
String(sHumidity) = "-99.0";
double pressure = -99.0;
String(sPressure) = "-99.0";
double altitude = -99.0;
String (sAltitude) = "-99.0";
double boiler = -99.0;
double overrideTemp = 50.0;
bool remoteOveride = false;
bool sleepOverride = false;
bool sysStatus = false; //Wait until detection of successful BME sensor to change status to True
//String string = "";

//time_t  heatTimes[4] = { "06:00AM","08:00AM", "05:00PM", "11:00PM" };
int wkdy = 0;          //weekday
ulong curTOD = 0;       //Current time of day in seconds
ulong lastTOD = 0;      //Last time of day in seconds
ulong glitchCnt = 0;    //Keep track of CRC errors
ulong zeroCnt = 0;      //Keep track of 0 readings
long wkdayHeatSched[] = {(0), cSecs(5,00), cSecs(7,30), cSecs(20,00), cSecs(22,00)};
long wkendHeatSched[] = {(0), cSecs(6,00), cSecs(7,30), cSecs(20,00), cSecs(22,00)};

//Heating Periods
int wkdayTemps[5]  = {61, 62, 63, 64, 65}; //degrees Fahrenheit
int wkendTemps[5]  = {62, 65, 62, 62, 62};

//Sleep Periods
int wkdaySleepT[5] = {900, 120, 90, 120, 600}; //seconds
int wkendSleepT[5] = {900, 120, 90, 120, 600};

/*boolean midnightCross = false;
boolean MNCross = false;  //Midnight crossing*/

#define TEMPSCALE DEG_F    //temperature scale DEF_F or DEG_C
//Output type
#define RLY_TYPE SPDT01   //Output control device for heating control connects RH to Y terminal
                          //on thermostat so 24V signal requests heat and opens zone valve
#define RLY_SENSOR_PIN D2 //Control pin to enable relay. Power is Vin (3 to 5V) and Ground
RLY rly(RLY_SENSOR_PIN, RLY_TYPE);

//Cloud functions
// SYNTAX
bool s1 = Particle.function("override", TempOverride);
bool s2 = Particle.function("resume", TempResume);

// Cloud functions must return int and take one String
int TempOverride(String extra) {
  overrideTemp = extra.toFloat();
  if ((overrideTemp > 55) && (overrideTemp < 75)){
    setpoint = overrideTemp;
    remoteOveride = true;
    return 0;
  }
  else
    return -1;
}

int TempResume(String extra) {
  remoteOveride = false;
  return 0;
}

int i = 0;

void myHandler(const char *event, const char *data) {
  // Handle the integration response
  i++;
  Serial.println(i);
  Serial.print(event);
  Serial.print(", data: ");

  if (atoi(data) == 1)
    sleepOverride = true;
  else
    sleepOverride = false;

    Serial.print("sleepOverride = "); Serial.println(sleepOverride);

  if (data)
    Serial.println(data);
  else
    Serial.println("NULL");
}

STARTUP(WiFi.selectAntenna(ANT_EXTERNAL));
Ubidots ubidots(TOKEN);
void setup() {

    //Cloud variables
    Particle.variable("Zero_Cnts", zeroCnt);
    Particle.variable("Current_TOD", curTOD);
    Particle.variable("Week_Day", wkdy);
    Particle.variable("Temperature", sTemperature);
    Particle.variable("Humidity", sHumidity);
    Particle.variable("Pressure", sPressure);
    Particle.variable("Altitude", sAltitude);
    Particle.variable("Setpoint", setpoint);
    Particle.variable("Relay_State", rly.getRlyState());
    Particle.variable("Override", remoteOveride);
    Particle.variable("Remote_Temp", overrideTemp);
    Particle.variable("Sleep_Period", SleepPeriod);

    //Subscriptions
    Particle.subscribe("UbidotsWebhook", myHandler);

    //Set Time Zone to EST
    Time.zone(-5);
    lastTOD = cSecs(Time.hour(),Time.minute()) + Time.second();

#ifdef  DEBUG
    .begin(9600);
    Serial.println("BME280 debug");
    String verNo = "";
    //verNo = System.version.c_str();
    Serial.print("Version Number : "); Serial.println(System.version().c_str());
    //verNo = System.versionNumber();
    Serial.print("Version Number Availble: "); Serial.println(System.versionNumber());
#endif

    if (!bme.begin(BME_ADDR)) {
//      Serial.println("Could not find a valid BME280 sensor, check wiring!");
      sysStatus = BME_SYSTEM_NOSENSOR;
    }
    else
      sysStatus = BME_SYSTEM_OK;

// Wait for the sensor to stabilize
//    delay(3000);

//    pinMode(RED_LED, OUTPUT);
//    pinMode(GRN_LED, OUTPUT);
//    pinMode(BLU_LED, OUTPUT);

    pinMode(RLY_SENSOR_PIN, OUTPUT);
    rly.begin();
}

unsigned long firstAvailable = 0;
int counter;

void loop() {
  //Check for OTA Update
  float ota;
  ota = ubidots.getValueWithDatasource(DEVICE_LABEL, VAR_LABEL);
  if (ota > 0)
    sleepOverride = true;
  else
    sleepOverride = false;

#ifdef  DEBUG
    Serial.print("Temperature = ");
    Serial.print(bme.readTemperature());
    Serial.println(" degC");

    Serial.print("Pressure = ");

    Serial.print(bme.readPressure() / 100.0F);
    Serial.println(" mBARS");

    Serial.print("Approx. Altitude = ");
    Serial.print(bme.readAltitude(SEALEVELPRESSURE_HPA));
    Serial.println(" m");

    Serial.print("Humidity = ");
    Serial.print(bme.readHumidity());
    Serial.println(" %RH");

    Serial.println();
  //  delay(2000);
#endif

    // Read Temperature
    if (TEMPSCALE == DEG_F)
        temperature = (bme.readTemperature() * 9)/5 + 32;
    else
        temperature = bme.readTemperature();

    sTemperature = String::format("%0.1f", temperature);
delay(50);

    // Read Humdity
    humidity = bme.readHumidity();
    sHumidity = String::format("%0.1f", humidity);

delay(50);

    // Read Pressure
    pressure = bme.readPressure() / 100.0F;
    sPressure = String::format("%0.1f", pressure);

delay(50);

    // Read Altitude
    altitude = bme.readAltitude(SEALEVELPRESSURE_HPA);
    sAltitude = String::format("%0.1f", altitude);

delay(50);

#ifdef  DEBUG
    // Get the current Time-of-day and check for midnight crossing
    Serial.print("WKDY: ");
    Serial.println(Time.weekday());
    Serial.print("HR: ");
    Serial.println(Time.hour());
    Serial.print("MIN: ");
    Serial.println(Time.minute());
    Serial.print("SEC: ");
    Serial.println(Time.second());
#endif

    curTOD = cSecs(Time.hour(), Time.minute()) + Time.second();
    wkdy = Time.weekday();

    /*if (MNCross == false){
      if (curTOD >= lastTOD){
        MNCross = false;
      }
      else
        MNCross = true;
    } //Midnight Cross
    else {
      MNCross = false;
    }
    lastTOD = curTOD;*/

/*#if defined(DEBUG)
    Serial.print("MNCross: ");
    Serial.println(MNCross);
    Serial.print("curTOD: ");
    Serial.println(curTOD);
    Serial.print("lastTOD: ");
    Serial.println(lastTOD);
#endif*/

  /*if (remoteOveride == true){
    setpoint = overrideTemp;
  }
  else
    if (MNCross == false){*/
      for (int i=0; i < 5; i++){
        if ((wkdy == 1) || (wkdy == 7)) {     //Sun or Sat
          if (curTOD > wkendHeatSched[i])
            setpoint = wkendTemps[i];
            SleepPeriod = wkendSleepT[i];
        }
        else{                                 //Mon - Fri
          if (curTOD > wkdayHeatSched[i]){
            setpoint = wkdayTemps[i];
            SleepPeriod = wkdaySleepT[i];
          }
        }
      }

    /*}
    else
        MNCross = false;*/

    // 2/12/17 Changed for BME version because relay is low enabled
    // version that is a low out is active.
    // Check if call for heat

    if (temperature < setpoint){
        rly.setRelayOut(LOW);    //heat ON
     }
    else {
        rly.setRelayOut(HIGH);   //heat OFF
    }

      // Publish variables to Webhooks
      if (chkVal(temperature, 45, 99)){
        Particle.publish("Temperature", String(temperature), PRIVATE);
#ifdef  DEBUG
        Serial.print("Temperature: ");
        Serial.println(temperature, 1);
#endif
      }
      delay(250);
      if (chkVal(humidity, 5, 95)){
        Particle.publish("Humidity", String(humidity), PRIVATE);
#ifdef  DEBUG
        Serial.print("Humidity: ");
        Serial.println(humidity, 1);
#endif
      }
      delay(250);
      if (chkVal(pressure, 100, 1500)){
        Particle.publish("Pressure", String(pressure), PRIVATE);
#ifdef  DEBUG
      Serial.print("Pressure: ");
      Serial.println(pressure, 1);
#endif
      }
/*
      Particle.publish("Setpoint", String(setpoint), PRIVATE);
      Particle.publish("RelayState", String(rly.getRlyState()), PRIVATE);
*/

//TEMP
Particle.publish("Sleep_Period", String(SleepPeriod), PRIVATE);
//TEMP
      delay(250);

  bool wifiReady = WiFi.ready();
  bool cloudReady = Particle.connected();

	//Serial.printlnf("wifi=%s cloud=%s counter=%d", (wifiReady ? "on" : "off"), (cloudReady ? "on" : "off"), counter++);

	if (wifiReady) {
		if (firstAvailable == 0) {
			firstAvailable = millis();
		}
		if (millis() - firstAvailable > 100) {
			// After we've been up for x seconds, go to sleep
        if (!sleepOverride){
  //			    Serial.println("Calling System.sleep(30)");
//  			    System.sleep(SLEEP_MODE_SOFTPOWEROFF, SleepPeriod);
            System.sleep(SLEEP_MODE_DEEP, SleepPeriod);
//            Serial.println("returned from sleep");
        }
        else
          Serial.println("Sleep Mode Override for OTA");

//SLEEP_MODE_DEEP
  			// System.sleep(SLEEP_MODE_DEEP, long seconds) (current approx 0.8mA)
        // Happens immediately, shuts down everything including network. Stops execution.
        // When returning from sleep, starts code excution from the beginning, network needs to wakeup.

  			// System.sleep(long seconds) (current approx 36mA)
        // Happens immediately but doesn't block and your code keeps running, just with
  			// Wi-Fi and cloud turned off. The LED should breathe white instead of cyan in this state. Then,
  			// after the time expires, reconnection to the cloud occurs
		}
	}
	else {
		firstAvailable = 0;
	}
}

boolean nextPub(String sPub, String vPub){
  Particle.publish(sPub, vPub, PRIVATE);
  return 1;
}

//chkVal()
// Check for value within min/max range
// Returns true if in range otherwise false
boolean chkVal(float val, float min, float max){
  if ((val < min) || (val > max))
    return false;
  else {
    zeroCnt++;
    return true;
  }
}
