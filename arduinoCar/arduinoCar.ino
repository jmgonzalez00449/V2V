/*
  Joe's car
  
*/
#include "EB_UltrasonicSensor.h"
#include "EB_MotorDriver.h"
#include "EB_LineFinder.h"
#include <SoftwareSerial.h>

//M1_EN,M1_IN2,M1_IN1, M2_EN,M2_IN2,M2_IN1
//M2 is on the left, M1 is on the right
EB_MotorDriver myMotorDriver(3,8,4, 5,6,7);

//Connect the Ultrasonic Sensor Trig to A2(D16, Echo to A3(D17)
EB_UltrasonicSensor myUltrasonicSensor(16,17);

//Connect Line Finder Sensor1 D14(A0), Sensor2 to D15(A1)
//Sensor 2 is on the left, Sensor 1 on the right
EB_LineFinder myLineFinder(14,15);

int speed = 130;
bool sensor1 = false;
bool sensor2 = false;
bool bothSensors = false;
unsigned char lastSensor = 0; // Used to know which sensor was active before both were
unsigned char correctionFactor = 0; // The longer off course, the more getting on course needs a currection
bool betweenLines = false;
bool isCorrecting; // This is true while getting off a line.

// Color sensors
#define SIO_BAUD 4800
#define UNUSED_PIN 255
#define WAIT_DELAY 200
#define COLOR_DIFF 30 // The difference between colors that are still considered a match
unsigned int red;
unsigned int green;
unsigned int blue;

#define LEFT_SENSOR_PIN 9
SoftwareSerial leftSensorIn(LEFT_SENSOR_PIN, UNUSED_PIN);
SoftwareSerial leftSensorOut(UNUSED_PIN, LEFT_SENSOR_PIN);

#define RIGHT_SENSOR_PIN 2
SoftwareSerial rightSensorIn(RIGHT_SENSOR_PIN, UNUSED_PIN);
SoftwareSerial rightSensorOut(UNUSED_PIN, RIGHT_SENSOR_PIN);

#define GET_SENSOR_IN( pin ) (pin == LEFT_SENSOR_PIN ? leftSensorIn : rightSensorIn)

void setup()
{
  myMotorDriver.begin();
  myUltrasonicSensor.begin();
  
  Serial.begin(9600);
  Serial.println("SETUP2!");

  // Init left sensor
  reset(LEFT_SENSOR_PIN);
  leftSensorOut.begin(SIO_BAUD);
  pinMode(LEFT_SENSOR_PIN, OUTPUT);
  leftSensorOut.print("= (00 $ m) !"); // Loop print values, see ColorPAL documentation
  leftSensorOut.end();

  leftSensorIn.begin(SIO_BAUD); // Set up serial port for receiving
  pinMode(LEFT_SENSOR_PIN, INPUT);

  // Init right sensor
  reset(RIGHT_SENSOR_PIN);
  rightSensorOut.begin(SIO_BAUD);
  pinMode(RIGHT_SENSOR_PIN, OUTPUT);
  rightSensorOut.print("= (00 $ m) !"); // Loop print values, see ColorPAL documentation
  rightSensorOut.end();

  rightSensorIn.begin(SIO_BAUD); // Set up serial port for receiving
  pinMode(RIGHT_SENSOR_PIN, INPUT);


}

// Reset ColorPAL; see ColorPAL documentation for sequence
void reset(unsigned char pin) {
  delay(200);
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
  pinMode(pin, INPUT);
  while (digitalRead(pin) != HIGH);
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
  delay(80);
  pinMode(pin, INPUT);
  delay(WAIT_DELAY);
}

void turnLeft(int sharpTurn) {
  // Move left by decreasing speed of left wheel
  Serial.print("TURN LEFT");


  int turnSpeed = speed+sharpTurn;
  if (turnSpeed > 255) turnSpeed = 255;
  myMotorDriver.setM2Speed(0);
  myMotorDriver.setM1Speed(turnSpeed);
  //delay(250);

}

void turnRight(int sharpTurn) {
  // Move right by decreasing speed of right wheel
  Serial.print("TURN RIGHT");


  int turnSpeed = speed+sharpTurn;
  if (turnSpeed > 255) turnSpeed = 255;
  myMotorDriver.setM1Speed(0);
  myMotorDriver.setM2Speed(turnSpeed);
  //delay(250);
} 

void stayBetweenLines() {
  
  if (myUltrasonicSensor.distance() < 20) {
    myMotorDriver.stop();
    return;
  } 
  
  // Stay between lines
  /*
  if (myLineFinder.sensor1() && myLineFinder.sensor2()) {
    if (betweenLines) {
      betweenLines = false;
      correctionFactor = 0;
    }
    
    if (lastSensor == 1) {
      // Sensor1 is on the right, so move left
      // true means make a sharp turn
      turnLeft(0);
      correctionFactor++;
    } else if (lastSensor == 2) {
      // Sensor2 is on the left, so move right
      // true means make a sharp turn
      turnRight(0);
      correctionFactor++;
    } else {
      // Back up
      myMotorDriver.setM1Speed(-speed);
      myMotorDriver.setM2Speed(-speed);
    }
  }
  else*/ 
  bool rightSensorOnLine = sensorOnLine(RIGHT_SENSOR_PIN);
  bool leftSensorOnLine = sensorOnLine(LEFT_SENSOR_PIN);
  if (rightSensorOnLine) {
    /*if (isCorrecting) {
      Serial.print("isCorrecting");
      myMotorDriver.setM1Speed(speed);
      myMotorDriver.setM2Speed(speed);  
    } else {*/
      // Move left
      turnLeft(0);
      correctionFactor += 1;
      lastSensor = RIGHT_SENSOR_PIN;
      isCorrecting = true;
    //}
  } else if (leftSensorOnLine) {
    /*if (isCorrecting) {
      Serial.print("isCorrecting");
      myMotorDriver.setM1Speed(speed);
      myMotorDriver.setM2Speed(speed);  
    } else {*/
      // Move right
      turnRight(0);
      correctionFactor += 1;
      lastSensor = LEFT_SENSOR_PIN;
      isCorrecting = true;
    //}
  } else {
    betweenLines = true;
    /*if (lastSensor == RIGHT_SENSOR_PIN) {
      // Correct back
      Serial.print("Correct Right");
      turnRight(0);
    } else if (lastSensor == LEFT_SENSOR_PIN) {
      // Correct back
      Serial.print("Correct Left");
      turnLeft(0);
    } else*/ {  
      Serial.print("Straight");
      myMotorDriver.setM1Speed(speed);
      myMotorDriver.setM2Speed(speed);
    }
    isCorrecting = false;
    lastSensor = 0;
    correctionFactor = 0;
  }

  return;
}


void loop()
{

  stayBetweenLines();
  //stayOnLine();

  Serial.println();
    
  //delay(10);

}

bool sensorOnLine( unsigned char pin ) {

  
  //Serial.print("READ DATA PIN: ");
  //Serial.print(pin);
  if (!readData(pin)) {
    return false;
  }

  if (  (green < COLOR_DIFF || green - COLOR_DIFF <= red) && ( 255-green < COLOR_DIFF || red <= green + COLOR_DIFF) &&
        (blue < COLOR_DIFF || blue - COLOR_DIFF <= green) && ( 255-blue < COLOR_DIFF || green <= blue + COLOR_DIFF) &&
        (red < COLOR_DIFF || red - COLOR_DIFF <= blue) && ( 255-red < COLOR_DIFF || blue <= red + COLOR_DIFF) 
  ) {
    return true;
  }

  return false;
}

// Returns true if the red, green, blue values were read and false otherwise.
bool readData(unsigned char pin) {
  char localBuffer[32];

  GET_SENSOR_IN(pin).listen();

  // Wait until the begginning of the next sequence
  do {
    localBuffer[0] = GET_SENSOR_IN(pin).read();
    //Serial.print(localBuffer[0]);
  } while( localBuffer[0] != '$' );

  //Serial.print(" | ");

  for(int i = 0; i < 9; i++) {
    while (GET_SENSOR_IN(pin).available() == 0);     // Wait for next input character
    localBuffer[i] = GET_SENSOR_IN(pin).read();
    //Serial.print(localBuffer[i]);
    if (localBuffer[i] == '$')               // Return early if $ character encountered
      return false;
 }
 
 parse(localBuffer);

 return true;
 
}

// Parse the hex data into integers
void parse(char * data) {
  sscanf (data, "%3x%3x%3x", &red, &green, &blue);
}

