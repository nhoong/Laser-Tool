/* Includes ------------------------------------------------------------------*/
#include <Arduino.h>
#include <Wire.h>
#include <vl53l4cd_class.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <Adafruit_LC709203F.h>
#include <Fonts/FreeSans9pt7b.h>

#define DEV_I2C Wire
#define SerialPort Serial
char report[64];
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
GFXcanvas16 canvas(240, 135);


// Replace with your network credentials
const char* ssid = "Laser Bucket Height Tool";
const char* password = "1234";
IPAddress IP;

// Create AsyncWebServer object on port 80
AsyncWebServer server (80);
AsyncEventSource events("/events");

// Timer variables
unsigned long lastTime = 0;  
unsigned long timerDelay = 30000;

//address we will assign if dual sensor is present
uint8_t sensor1add = 0x51;
uint8_t sensor2add = 0x28;

// set the pins to shutdown
#define xshut1 14 //14 is A0
#define xshut2 15 //15 is A1
#define GFX_BL DF_GFX_BL

//this holds the measurement
VL53L4CD_Result_t results1;
VL53L4CD_Result_t results2;
int sensor1, sensor2;

//Components.
VL53L4CD sensor1_vl53l4cd_sat(&DEV_I2C, A0);
VL53L4CD sensor2_vl53l4cd_sat(&DEV_I2C, A1);
Adafruit_LC709203F lc;

int offset;
uint8_t NewDataReady1 = 0;
uint8_t NewDataReady2 = 0;
uint8_t status1;
uint8_t status2;

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <title>Laser Bucket Height Tool</title>
  <style>
    html {
      font-family: Arial;
      display: inline-block;
      margin: 0px auto;
      text-align: center;
    }
    
    h2 {
      font-size: 3.0rem;
    }
    
    p {
      font-size: 3.0rem;
    }
    
    .units {
      font-size: 1.2rem;
    }
    
    .sensor-labels {
      font-size: 1.5rem;
      vertical-align: middle;
      padding-bottom: 15px;
    }
    
    .button {
      display: inline-block;
      background-color: #008CBA;
      border: none;
      border-radius: 4px;
      color: white;
      padding: 16px 40px;
      text-decoration: none;
      font-size: 30px;
      margin: 2px;
      cursor: pointer;
    }
    
    #zero {
      background-color: green;
    }
    
    #reset {
      background-color: red;
    }

    .center-table {
        display: flex;
        justify-content: center;
        align-items: center;
        height: 100vh;
    }

    .table {
        margin: 0 auto;
    }

    #recordData,
    #btnExportToCsv {
    padding: 10px 20px; /* Adjust the padding values as desired */
    font-size: 20px; /* Adjust the font size as desired */
  }
  </style>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="icon" href="data:,">
  <link rel="stylesheet" type="text/css" href="style.css">
  <script src="https://kit.fontawesome.com/a30daafdec.js" crossorigin="anonymous"></script>
</head>

<body>
  <h2>Laser Bucket Height Tool</h2>
  <p>
    <i class="fa-solid fa-ruler-vertical" style="color: #f5e609;"></i>
    <span class="sensor-labels">Distance 1: </span>
    <span id="distance1">%DISTANCE1%</span>
    <span class="units"> mm</span>
  </p>
  <p>
    <i class="fa-solid fa-ruler-vertical" style="color: #f5e609;"></i>
    <span class="sensor-labels">Distance 2: </span>
    <span id="distance2">%DISTANCE2%</span>
    <span class="units"> mm</span>
  </p>
  <p>
    <i class="fa-solid fa-minus" style="color: #f5e609;"></i>
    <span class="sensor-labels">Offset 1: </span>
    <span id="offset1">%OFFSET1%</span>
    <span class="units"> mm</span>
  </p>
  <p>
    <i class="fa-solid fa-minus" style="color: #f5e609;"></i>
    <span class="sensor-labels">Offset 2: </span>
    <span id="offset2">%OFFSET2%</span>
    <span class="units"> mm</span>
  </p>
  <label><input class="button" type="button" value="Zero" onclick="zero()" id="zero"></label>
  <label><input class="button" type="button" value="Reset" onclick="reset()" id="reset"></label>
  <div id="error" style="display: none;">
    <p>
      <i class="fa-solid fa-triangle-exclamation" style="color: red;"></i>
      <span class="sensor-labels">Not connected to sensor!</span>
    </p>
  </div>

  <br></br>
  <label for="dispensestation">Dispense Station:</label>
    <select name="dispensestation" id="dispensestation">
        <option value="IW1">IW1</option>
        <option value="IW2">IW2</option>
        <option value="DB1">DB1</option>
        <option value="DB2">DB2</option>
        <option value="DB3">DB3</option>
        <option value="PBW1">PBW1</option>
        <option value="PBW2">PBW2</option>
        <option value="PBW3">PBW3</option>
        <option value="ActsMods1">ActsMods1</option>
        <option value="Bases1">Bases1</option>
        <option value="ActsMods2">ActsMods2</option>
        <option value="Bases2">Bases2</option>
        <option value="Cap1">Cap1</option>
        <option value="Oxidizer">Oxidizer</option>
        <option value="Cap2">Cap2</option>
    </select>
    <br></br>
    <button id="recordData" type="button" class="button" onclick="recordData()">Record Station</button>
    <button id="btnExportToCsv" type="button" class="button">Export to CSV</button>

    <table id="dataTable" class="table">
        <thead>
            <tr>
                <th>Dispense Tower</th>
                <th>Inner Dial Measurement (mm)</th>
                <th>Outer Dial Measurement (mm)</th>
            </tr>
        </thead>
        <tbody>
            <tr>
                <td>IW1</td>
                <td id="IW1_inner">IW1_innerDial</td>
                <td id="IW1_outer">IW1_outerDial</td>
            </tr>
            <tr>
                <td>IW2</td>
                <td id="IW2_inner">IW2_innerDial</td>
                <td id="IW2_outer">IW2_outerDial</td>
            </tr>
            <tr>
                <td>DB1</td>
                <td id="DB1_inner">DB1_innerDial</td>
                <td id="DB1_outer">DB1_outerDial</td>
            </tr>
            <tr>
                <td>DB2</td>
                <td id="DB2_inner">DB2_innerDial</td>
                <td id="DB2_outer">DB2_outerDial</td>
            </tr>
            <tr>
                <td>DB3</td>
                <td id="DB3_inner">DB3_innerDial</td>
                <td id="DB3_outer">DB3_outerDial</td>
            </tr>
            <tr>
                <td>PBW1</td>
                <td id="PBW1_inner">PBW1_innerDial</td>
                <td id="PBW1_outer">PBW1_outerDial</td>
            </tr>
            <tr>
                <td>PBW2</td>
                <td id="PBW2_inner">PBW2_innerDial</td>
                <td id="PBW2_outer">PBW2_outerDial</td>
            </tr>
            <tr>
                <td>PBW3</td>
                <td id="PBW3_inner">PBW3_innerDial</td>
                <td id="PBW3_outer">PBW3_outerDial</td>
            </tr>
            <tr>
                <td>ActsMods1</td>
                <td id="ActsMods1_inner">ActsMods1_innerDial</td>
                <td id="ActsMods1_outer">ActsMods1_outerDial</td>
            </tr>
            <tr>
                <td>Bases1</td>
                <td id="Bases1_inner">Bases1_innerDial</td>
                <td id="Bases1_outer">Bases1_outerDial</td>
            </tr>
            <tr>
                <td>ActsMods2</td>
                <td id="ActsMods2_inner">ActsMods2_innerDial</td>
                <td id="ActsMods2_outer">ActsMods2_outerDial</td>
            </tr>
            <tr>
                <td>Bases2</td>
                <td id="Bases2_inner">Bases2_innerDial</td>
                <td id="Bases2_outer">Bases2_outerDial</td>
            </tr>
            <tr>
                <td>Cap1</td>
                <td id="Cap1_inner">Cap1_innerDial</td>
                <td id="Cap1_outer">Cap1_outerDial</td>
            </tr>
            <tr>
                <td>Oxidizer</td>
                <td id="Oxidizer_inner">Oxidizer_innerDial</td>
                <td id="Oxidizer_outer">Oxidizer_outerDial</td>
            </tr>
            <tr>
                <td>Cap2</td>
                <td id="Cap2_inner">Cap2_innerDial</td>
                <td id="Cap2_outer">Cap2_outerDial</td>
            </tr>
        </tbody>
    </table>

  <script>
    var distance1 = 0;
    var distance2 = 0;
    var offset1 = 0;
    var offset2 = 0;

    setInterval(function () {
      var xhttp1 = new XMLHttpRequest();
      xhttp1.onreadystatechange = function () {
        if (this.readyState == 4 && this.status == 200) {
          distance1 = parseFloat(this.responseText);
          document.getElementById("distance1").innerHTML = (distance1 - offset1).toFixed(0);
          console.log("distance1", this.responseText);
          document.getElementById('error').style.display = 'none';
        }
      };

      xhttp1.open("GET", "/distance1", true);
      xhttp1.onerror = function () {
        console.log("*** Not connected to sensor 1! ***");
        document.getElementById('error').style.display = 'block';
      };
      xhttp1.send();

      var xhttp2 = new XMLHttpRequest();
      xhttp2.onreadystatechange = function () {
        if (this.readyState == 4 && this.status == 200) {
          distance2 = parseFloat(this.responseText);
          document.getElementById("distance2").innerHTML = (distance2 - offset2).toFixed(0);
          console.log("distance2", this.responseText);
          document.getElementById('error').style.display = 'none';
        }
      };

      xhttp2.open("GET", "/distance2", true);
      xhttp2.onerror = function () {
        console.log("*** Not connected to sensor 2! ***");
        document.getElementById('error').style.display = 'block';
      };
      xhttp2.send();
    }, 100);

    function zero() {
      offset1 = distance1;
      offset2 = distance2;
      document.getElementById("offset1").innerHTML = offset1.toFixed(0);
      document.getElementById("offset2").innerHTML = offset2.toFixed(0);
      console.log("offset1", offset1);
      console.log("offset2", offset2);
    }

    function reset() {
      offset1 = 0;
      offset2 = 0;
      document.getElementById("offset1").innerHTML = offset1.toFixed(0);
      document.getElementById("offset2").innerHTML = offset2.toFixed(0);
      console.log("offset1", offset1);
      console.log("offset2", offset2);
    }

    function recordData() {
        var dispenseStation = document.getElementById("dispensestation").value;
        var innerDial = (distance1 - offset1).toFixed(0);
        var innerDialMeasurement = (distance1 - offset1).toFixed(0);
        var outerDial = (distance2 - offset2).toFixed(0);
        var outerDialMeasurement = (distance2 - offset2).toFixed(0);
        var innerCellID = dispenseStation + "_inner";
        document.getElementById(dispenseStation + "_inner").innerHTML = innerDialMeasurement;
        var outerCellID = dispenseStation + "_outer";
        document.getElementById(dispenseStation + "_outer").innerHTML = outerDialMeasurement;

        document.getElementById(innerCellID).innerHTML = innerDial;
        document.getElementById(outerCellID).innerHTML = outerDial;
    }

    function downloadCSV(csv, filename) {
      var csvFile;
      var downloadLink;
      csvFile = new Blob([csv], { type: "text/csv" });
      downloadLink = document.createElement("a");
      downloadLink.download = filename;
      downloadLink.href = window.URL.createObjectURL(csvFile);
      downloadLink.style.display = "none";
      document.body.appendChild(downloadLink);
      downloadLink.click();
    }

    document.getElementById("btnExportToCsv").addEventListener("click", function () {
      var csv = [];
      var rows = document.querySelectorAll("table tr");
      for (var i = 0; i < rows.length; i++) {
        var row = [];
        var cols = rows[i].querySelectorAll("td, th");
        for (var j = 0; j < cols.length; j++) {
          row.push(cols[j].innerText);
        }
        csv.push(row.join(","));
      }
      var filename = "bucket_height_data.csv";
      downloadCSV(csv.join("\n"), filename);
    });
  </script>
</body>
</html>
)rawliteral";

// Initialize LittleFS
void initLittleFS() {

  if(!LittleFS.begin(true)){
    Serial.println("An Error has occurred while mounting LittleFS");
    return;
  }
  Serial.println("LittleFS mounted successfully");
}

//init Wifi
void initWifi(){
  Serial.print("Setting AP (Access Point)...");
  // No password
  WiFi.softAP(ssid);
  // Start WiFi Access Point
  IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.print(IP);
  Serial.println();
}

//init TFT
void initTFT(){
  // turn on backlite
  pinMode(TFT_BACKLITE, OUTPUT);
  digitalWrite(TFT_BACKLITE, HIGH);

  // turn on the TFT / I2C power supply
  pinMode(TFT_I2C_POWER, OUTPUT);
  digitalWrite(TFT_I2C_POWER, HIGH);
  delay(10);

  // initialize TFT
  tft.init(135, 240); // Init ST7789 240x135
  tft.setRotation(3);
  tft.fillScreen(ST77XX_BLACK);
  canvas.setFont(&FreeSans9pt7b); // Use custom font
  canvas.setTextWrap(false);            // Clip text within canvas
}

//init sensors
void initSensors(VL53L4CD sensorX, VL53L4CD sensorY, uint8_t sensorXAdd, uint8_t sensorYAdd)
{
  VL53L4CD_ERROR err;
  
  sensor1_vl53l4cd_sat.begin();
  sensor1_vl53l4cd_sat.VL53L4CD_Off();

  sensor2_vl53l4cd_sat.begin();
  sensor2_vl53l4cd_sat.VL53L4CD_Off();

  do{
    err = sensor1_vl53l4cd_sat.InitSensor(sensorXAdd);
    delay(10);
    Serial.println("InitSensor 1");
  }while(err != VL53L4CD_ERROR_NONE);

  do{
    err = sensor2_vl53l4cd_sat.InitSensor(sensorYAdd);
    delay(10);
    Serial.println("InitSensor 2");
  }while(err != VL53L4CD_ERROR_NONE);

  sensor1_vl53l4cd_sat.VL53L4CD_SetRangeTiming(500, 0);
  sensor1_vl53l4cd_sat.VL53L4CD_SetOffset(-10);
  sensor1_vl53l4cd_sat.VL53L4CD_StartRanging();

  sensor2_vl53l4cd_sat.VL53L4CD_SetRangeTiming(500, 0);
  sensor2_vl53l4cd_sat.VL53L4CD_SetOffset(-10);
  sensor2_vl53l4cd_sat.VL53L4CD_StartRanging();
}

void initBatteryMonitor()
{
  // For the Feather ESP32-S2, we need to enable I2C power first!
  // this section can be deleted for other boards
#if defined(ARDUINO_ADAFRUIT_FEATHER_ESP32S2)
  // turn on the I2C power by setting pin to opposite of 'rest state'
  pinMode(PIN_I2C_POWER, INPUT);
  delay(1);
  bool polarity = digitalRead(PIN_I2C_POWER);
  pinMode(PIN_I2C_POWER, OUTPUT);
  digitalWrite(PIN_I2C_POWER, !polarity);
#endif

  if (!lc.begin()) {
    Serial.println(F("Couldnt find Adafruit LC709203F?\nMake sure a battery is plugged in!"));
    while (1) delay(10);
  }
  Serial.println(F("Found LC709203F"));
  Serial.print("Version: 0x"); Serial.println(lc.getICversion(), HEX);

  lc.setThermistorB(3950);
  Serial.print("Thermistor B = "); Serial.println(lc.getThermistorB());

  lc.setPackSize(LC709203F_APA_500MAH);

  lc.setAlarmVoltage(3.8);
}

void readBattery(){
  Serial.print("Batt_Voltage:");
  Serial.print(lc.cellVoltage(), 3);
  Serial.print("\t");
  Serial.print("Batt_Percent:");
  Serial.print(lc.cellPercent(), 1);
  Serial.print("\t");
  Serial.print("Batt_Temp:");
  Serial.println(lc.getCellTemperature(), 1);

  delay(2000);  // dont query too often!
}

//getSensor1Reading
void getSensor1Reading(uint8_t status, uint8_t NewDataReady)
{
  digitalWrite(xshut1, HIGH);
  //Serial.println("Sensor 1 is on");
  if ((!status) && (NewDataReady != 0)) {
    // (Mandatory) Clear HW interrupt to restart measurements
    sensor1_vl53l4cd_sat.VL53L4CD_ClearInterrupt();
    // Read measured distance. RangeStatus = 0 means valid data
    sensor1_vl53l4cd_sat.VL53L4CD_GetResult(&results1);
    snprintf(report, sizeof(report), "Status = %3u, Distance = %5u mm, Signal = %6u kcps/spad\r\n",
             results1.range_status,
             results1.distance_mm,
             results1.signal_per_spad_kcps);
    if (results1.distance_mm < 1100){
    sensor1 = results1.distance_mm;}
    else{
      sensor1 = 0;}
    Serial.println((String)"This is Sensor1 " + (String)sensor1 + (String)"mm");
  }
  digitalWrite(xshut1, LOW);
}

//getSensor2Reading
void getSensor2Reading(uint8_t status, uint8_t NewDataReady)
{
  digitalWrite(xshut2, HIGH);
  //Serial.println("Sensor 2 is on");
  if ((!status) && (NewDataReady != 0)) {
    // (Mandatory) Clear HW interrupt to restart measurements
    sensor2_vl53l4cd_sat.VL53L4CD_ClearInterrupt();
    // Read measured distance. RangeStatus = 0 means valid data
    sensor2_vl53l4cd_sat.VL53L4CD_GetResult(&results2);
    snprintf(report, sizeof(report), "Status = %3u, Distance = %5u mm, Signal = %6u kcps/spad\r\n",
             results2.range_status,
             results2.distance_mm,
             results2.signal_per_spad_kcps);
    if (results2.distance_mm < 1100){
    sensor2 = results2.distance_mm;}
    else{
      sensor2 = 0;}
    Serial.println((String)"This is Sensor2 " + (String)sensor2 + (String)"mm");
  }
  digitalWrite(xshut2, LOW);
}

void zero(int newZero)
{
  offset = newZero;
  sensor1_vl53l4cd_sat.VL53L4CD_SetOffset(offset);
  sensor2_vl53l4cd_sat.VL53L4CD_SetOffset(offset);
}

void displayMeasurements() {
  // Clear the display
  canvas.fillScreen(0x0000); // Clear canvas (not display)

  // Print local IP address
  canvas.setCursor(0, 20);
  canvas.println((String)"Local IP: " + IP.toString());

  // Print battery level
  canvas.setCursor(0, 40);
  canvas.println((String)"Batt Voltage: " + (String)lc.cellVoltage() + (String)"v");
  canvas.print("\t");
  canvas.setCursor(0, 55);
  canvas.println((String)"Batt Percent: " + (String)lc.cellPercent() + (String)"%");
  canvas.print("\t");
  canvas.setCursor(0, 70);
  canvas.println((String)"Batt Temp: " + (String)lc.getCellTemperature()+(String)"c");

  // Print sensor values
  canvas.setCursor(0, 90);
  canvas.println((String)"Sensor 1 Reading: " + (String)sensor1 + (String)" mm");

  canvas.setCursor(0, 105);
  canvas.println((String)"Sensor 2 Reading: " + (String)sensor2 + (String)" mm");
  tft.drawRGBBitmap(0, 0, canvas.getBuffer(), canvas.width(), canvas.height());
}

// Replaces placeholder with sensor values
String processor(const String& var){
  do {
    status1 = sensor1_vl53l4cd_sat.VL53L4CD_CheckForDataReady(&NewDataReady1);
    status2 = sensor2_vl53l4cd_sat.VL53L4CD_CheckForDataReady(&NewDataReady2);
  } while (!NewDataReady1 || !NewDataReady2);
  getSensor1Reading(status1, NewDataReady1);
  if(var == "DISTANCE1"){
    return String(sensor1);
  }
  else if(var =="OFFSET"){
    return String(offset);
  }
  return String();
  getSensor2Reading(status2, NewDataReady2);
  if(var == "DISTANCE2"){
    return String(sensor2);
  }
  else if(var =="OFFSET"){
    return String(offset);
  }
  return String();
}

/* Setup ---------------------------------------------------------------------*/
void setup()
{
  // Initialize serial for output.
  SerialPort.begin(115200);
  delay(500);
  SerialPort.println("Starting...");
  LittleFS.begin();
  initLittleFS();
  initWifi(); 
  initTFT();
  initBatteryMonitor();

  // Initialize I2C bus.
  DEV_I2C.begin();
  
  initSensors(sensor1_vl53l4cd_sat, sensor2_vl53l4cd_sat, sensor1add, sensor2add);

  // Route for root / web page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });
  /*server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/index.html", String(), false, processor);
  });*/

  // Route to load style.css file, and script.js file
  server.serveStatic("/style.css", LittleFS, "/text/css");

  server.on("/distance1", HTTP_GET, [](AsyncWebServerRequest *request){
    getSensor1Reading(status1, NewDataReady1);
    request->send_P(200, "text/plain", String(sensor1).c_str());
  });

  server.on("/distance2", HTTP_GET, [](AsyncWebServerRequest *request){
    getSensor2Reading(status1, NewDataReady1);
    request->send_P(200, "text/plain", String(sensor2).c_str());
  });

    server.on("/offset", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(offset).c_str());
  });

  server.on("/zero", HTTP_GET, [] (AsyncWebServerRequest *request){
    zero(offset);
    zero(offset);
    Serial.println("Sensor Zeroed");
  });

  server.on("/reset", HTTP_GET, [] (AsyncWebServerRequest *request){
    offset = 0;
    Serial.println("Offset Reset");
  });

  server.begin();
}

void loop()
{
  do {
    status1 = sensor1_vl53l4cd_sat.VL53L4CD_CheckForDataReady(&NewDataReady1);
    status2 = sensor2_vl53l4cd_sat.VL53L4CD_CheckForDataReady(&NewDataReady2);
  } while (!NewDataReady1 || !NewDataReady2);
  getSensor1Reading(status1, NewDataReady1);
  getSensor2Reading(status2, NewDataReady2);
  Serial.println(IP);

  displayMeasurements();

  delay(100);
}