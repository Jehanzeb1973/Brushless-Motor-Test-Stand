#include <Wire.h>
#include <U8g2lib.h>
#include <Adafruit_MLX90614.h>
#include "HX711.h"

#include <Preferences.h>//EEPROM library

// Libraries for SD card
#include <FS.h>
#include <SPI.h>
#include <SD.h>

//Current Sensor pins

#define SD_CS 32  //Chip select for SD card
#define LED_Pin 02 // LED on DevKit
#define OLED_CS 25 //Chip Select for SPI OLED Display
#define OLED_DC 26 //D/C line for SPI OLED Display
#define OLED_Reset 27// REset Line for SPI OLED Display
#define RC_Pin 15 //GPIO 15 for RC signal
#define LOADCELL_DOUT_PIN  17
#define LOADCELL_SCK_PIN  16
#define RPMsensorPin  4    
#define voltsensorPin  13
#define currentsensorPin  12 


//RPM Variables
unsigned int revs=0;
int RPM=0;
int propblades=2;
long millisold=0;
long timeold=0;//for calculating RPM

//Voltage Sensor variables
   
int voltsensorValue = 0;  // variable to store the value coming from the sensor
float voltagefactor=15.40/1.0; //use actual values as measured by multimeter to avoid error from resistor tolerance
float voltage=0;//Calculated voltage value
//Current Sensor Variables
     
float currentsensorValue = 0;  // variable to store the value coming from the sensor
float currentfactor=0.02;//20mA/volt for ACS 758 100ECB
float nominalvoltage=0;  //nominal voltage measured at zero current from ACS 758. This value will be subtracted from measured voltage to calculate current
float currentsensorvoltage=0;  //voltage measured at ACS758



void IRAM_ATTR isr() {
  if((millis()-millisold)>4)
  {
 revs++;
  }
  millisold=millis();
  }



bool sdavailable=false;

String Data="";//Data line to be logged to SD crd


Adafruit_MLX90614 tempSensor1 = Adafruit_MLX90614(); //Standard MLX90614 sensor
Adafruit_MLX90614 tempSensor2 = Adafruit_MLX90614(0x5B); //MLX90614 with changed I2C address to be used on common I2C Bus
float motorTemp=0;
float escTemp=0;
float ambTemp=0;

//Buffers to store Strings for display
char buf1[16];
char buf2[16];
char buf3[16];
char buf4[16];

// HX711 circuit wiring
HX711 scale;
int loadcellValue=0;


unsigned long rcSignal=0;


//1.3" I2C OLED display
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
//1.5" SPI OLED Display
U8G2_SSD1309_128X64_NONAME0_F_4W_HW_SPI u8g2_2(U8G2_R0, /* cs=*/OLED_CS, /* dc=*/ OLED_DC, /* reset=*/ OLED_Reset);  

byte incoming;
String inBuffer;//Buffer to handle commands sent over serial port
bool calibratesensor=false;//will go into weght calibration mode if this value is true

// Calibration constants stored in EEPROM
Preferences preferences;
bool taredone=false;
bool scalingdone=false;
long tareconst=0;
float scaleconst=0;

 int fileseq=0;//Sequence in filename
 char filename[13];


void setup(void) {
  
Serial.begin(115200);

 int sum=0;
  for(int x=0;x<500;x++)
  
  {
    sum=sum+analogRead(currentsensorPin);
  }
  sum=sum/500; //Current sensor out put has high variance. To reduce this variance, average of a lot of readings is used
 nominalvoltage = 3.3*(float(sum)/4095);//ESP32 has 12 bit ADC thus 4095 max value and this is corresponding to 3.3v
//RPM Setup
 pinMode(RPMsensorPin, INPUT_PULLDOWN);
 attachInterrupt(RPMsensorPin, isr, FALLING);
 millisold=millis();
 timeold=millis();

tempSensor1.begin();
tempSensor2.begin();
pinMode(RC_Pin,INPUT);

  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  readSettings();//read constants from EEPROM if available
  if (taredone) scale.set_offset(tareconst);
  if (scalingdone)  scale.set_scale(scaleconst);
  u8g2.setBusClock(100000); //important to set bus clock at 1MHz otherwise other devices on I2C bus may not work
  u8g2.begin();
  u8g2_2.begin();
 // u8g2.setFont(u8g2_font_9x15_mr);  // choose a suitable font
  u8g2.setFont(u8g2_font_6x12_mr);  // choose a suitable font
  u8g2_2.setFont(u8g2_font_6x12_mr);  // choose a suitable font

  // Initialize SD card for saving data to file
  SD.begin(SD_CS);  
  if(!SD.begin(SD_CS)) {
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();
  if(cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }
  Serial.println("Initializing SD card...");
  if (!SD.begin(SD_CS)) {
    Serial.println("ERROR - SD card initialization failed!");
    return;    // init failed
    
  }

  // If the data.txt file doesn't exist
  // Create a file on the SD card 
 
  File file;
  fileseq=0;
 
 do {
  fileseq++;
  char seq[4];
  itoa(fileseq,seq,10);
   filename[0] = '\0';
  strcat(filename,"/Data");
 strcat(filename,seq);
  strcat(filename,".txt");
 
file = SD.open(filename);


  } while(file);
 
   filename[0] = '\0';
  char seq2[4];
  itoa(fileseq,seq2,10);
  strcat(filename,"/Data");
 strcat(filename,seq2);
  strcat(filename,".txt");
  
  u8g2.clearBuffer();
    u8g2.drawStr(1,16,"Creating file...");
    u8g2.drawStr(1,32,filename);
      u8g2.sendBuffer();
    writeFile(SD, filename, "Reading Data \r\n");
   
  file.close();
  sdavailable=true;
  pinMode(LED_Pin,OUTPUT);
  digitalWrite(LED_Pin,HIGH);
delay(2000);
digitalWrite(LED_Pin,LOW);

}

void loop(void) {
readSerialCommand();//read if a command is being sent through serial port
if (calibratesensor) 
{
  Serial.print("Raw Value ");
  Serial.print(scale.read());
   Serial.print(" Tared Value ");
  Serial.print(scale.get_value(5));//average of 5
   Serial.print(" Tared and Scaled Value ");
  Serial.println(scale.get_units(5));//average of 5
}
else
{
readCurrent();
readVoltage();
readScale();
readRC();
readTempSensors();
readRPM();

     itoa(loadcellValue,buf1,10);
     itoa(rcSignal,buf2,10);
     itoa(escTemp,buf3,10);
     itoa(motorTemp,buf4,10);
 SaveData();

Serial.print(Data);

  u8g2.clearBuffer();	// clear the internal memory
   u8g2.drawStr(2,12,"Volt "); // write something to the internal memory
  u8g2.setCursor(72,12);
  u8g2.print(String(voltage,2));
   u8g2.drawStr(02,25,"Current(A) ");
    u8g2.setCursor(72,25);
  u8g2.print(String(currentsensorValue,2));
    u8g2.drawStr(02,38,"Temp1 ");
  u8g2.setCursor(72,38);
  u8g2.print(String(escTemp,2));
    u8g2.drawStr(02,51,"Temp2 ");
    u8g2.setCursor(72,51);
  u8g2.print(String(motorTemp,2));
   u8g2.drawStr(02,64,"Nom Volt");
    u8g2.setCursor(72,64);
  u8g2.print(String(nominalvoltage,2));
     u8g2.sendBuffer();					// transfer internal memory to the display

   u8g2_2.clearBuffer();  // clear the internal memory
    u8g2_2.drawStr(2,12,"Thrust(g) "); // write something to the internal memory
 u8g2_2.drawStr(72,12,buf1);
  u8g2_2.drawStr(02,25,"RPM ");
     u8g2_2.setCursor(72,25);
  u8g2_2.print(RPM);
  u8g2_2.drawStr(2,38,"Thrt(%)");
  u8g2_2.drawStr(72,38,buf2);
   u8g2_2.drawStr(02,51,"Power(W) ");
     u8g2_2.setCursor(72,51);
  u8g2_2.print(String(voltage*currentsensorValue));
     u8g2_2.sendBuffer();          // transfer internal memory to the display
  Data=""; //Empty data string
}
  delay(300);  
}
void readCurrent()
{
   int sum=0;
  for(int x=0;x<500;x++)
  
  {
    sum=sum+analogRead(currentsensorPin);
  }
 currentsensorValue=sum/500;
  
currentsensorvoltage=3.3*float(currentsensorValue)/4095;
currentsensorValue=  (currentsensorvoltage-nominalvoltage)/currentfactor;
}

void readVoltage()
{
  // read the value from the sensor:
   voltsensorValue = analogRead(voltsensorPin);
 voltage= voltagefactor*3.3*(float(voltsensorValue)/4095);
}

void readRPM()
{
  RPM=0;
  // read the value from the sensor:
     RPM=(revs*60000)/(propblades*(millis()-timeold));
     revs=0;
timeold=millis();
}

void readRC()
{
  rcSignal=pulseIn(RC_Pin,HIGH,30000);
if(rcSignal<1000) rcSignal=1000;
rcSignal=(rcSignal-1000)/10;//RC signal in %
}

void readScale()
{
  if (scale.wait_ready_timeout(1000))  loadcellValue = scale.get_units(5);
if (loadcellValue<0) loadcellValue=0;
}

void readTempSensors()
{
 motorTemp=tempSensor1.readObjectTempC(); 
ambTemp=tempSensor2.readAmbientTempC(); 
escTemp=tempSensor2.readObjectTempC(); 
 // Serial.print("Ambient = "); Serial.print(mlx.readAmbientTempF()); 
 // Serial.print("*F\tObject = "); Serial.print(mlx.readObjectTempF()); Serial.println("*F"); 
 
}


// Write the sensor readings on the SD card
void logSDCard() { 
 appendFile(SD,filename,Data.c_str());
  
}

// Write to the SD card (DON'T MODIFY THIS FUNCTION)
void writeFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if(!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  if(file.print(message)) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}

// Append data to the SD card (DON'T MODIFY THIS FUNCTION)
void appendFile(fs::FS &fs, const char * path, const char * message) {
//  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if(!file) {
  //  Serial.println("Failed to open file for appending");
    return;
  }
  if(file.print(message)) {
    Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
}

void SaveData()
{
Data+="Motor Thrust";
Data+=",";
Data+=loadcellValue;
Data+=",";
Data+="RPM";
Data+=",";
Data+=RPM;
Data+=",";
Data+="Throttle %";
Data+=",";
Data+=rcSignal;
Data+=",";
Data+="Voltage";
Data+=",";
Data+=voltage;
Data+=",";
Data+="Current(A)";
Data+=",";
Data+=currentsensorValue;
Data+=",";
Data+="Power(W)";
Data+=",";
Data+=(voltage*currentsensorValue);
Data+=",";
Data+="Amb Temp(C)";
Data+=",";
Data+=ambTemp;
Data+=",";
Data+="ESC Temp(C)";
Data+=",";
Data+=escTemp;
Data+=",";
Data+="Motor Temp(C)";
Data+=",";
Data+=motorTemp;
Data+='\n';


if(sdavailable) logSDCard();
  
}

void readSerialCommand()
{
  // setup as non-blocking code
    while(Serial.available() > 0) {
        inBuffer = Serial.readString();
        
      //  if(incoming == '\n') {  // newline, carriage return, both, or custom character
        
            // handle the incoming command
        if(inBuffer.length()>2)    handle_command();

            // Clear the string for the next command
            inBuffer = "";
        }
    }


        void handle_command() {
          Serial.println("Handling Command");
          
    // expect something like 'pin 3 high'
    String command = inBuffer.substring(0, inBuffer.indexOf(' '));
    String parameters = inBuffer.substring(inBuffer.indexOf(' ') + 1);
    
    if(command.indexOf("tare")>=0){
      Serial.println("Taring Now");
      for (int x=1;x<10;x++)
      {
        Serial.println(scale.read_average(20));    // print the average of 20 readings from the ADC
      }
     int tareconst= scale.read_average(20);
      scale.tare();
       taredone=true;
//Save values to EEPROM
  preferences.begin("settings",false);
  taredone=preferences.putBool("TareDone",taredone);
  tareconst=preferences.putLong("TareConst",tareconst);
  preferences.end();
              }

        if(command.equalsIgnoreCase("calibrate")){
     calibratesensor=true;
        // parse the rest of the information
        int calweight = parameters.substring(0, parameters.indexOf(' ')).toInt();
Serial.println("Entering calibration mode");
delay(3000);
scale.set_scale();
scale.tare();
Serial.println("Place the weight within 10 seconds on scale to calibrate");
delay(10000);

      int scaleconst=  scale.get_value(10)/calweight;
       scale.set_scale(scaleconst);
       scalingdone=true;
    //Save values to EEPROM
    preferences.begin("settings",false);
    scalingdone=preferences.putBool("ScalingDone",scalingdone);
    scaleconst=preferences.putFloat("ScaleConst",scaleconst);
    preferences.end();
    
        }
    } 


  void readSettings()
{

   preferences.begin("settings",true);
  taredone=preferences.getBool("TareDone",taredone);
  scalingdone=preferences.getBool("ScalingDone",scalingdone);
tareconst=preferences.getLong("TareConst",tareconst);
scaleconst=preferences.getFloat("ScaleConst",scaleconst);
preferences.end();

}   
