// ZumoSumo - Template application for robot sumo using a Pololu Zumo

// Version 0.11 - 29 Apr 23

// WARNING - Optimized for speed, not size. Delete if total software size is too big.
#pragma GCC optimize ("-Ofast")

#include <Wire.h>
#include <Zumo32U4.h>

// Define operating parameters

// Speeds (Speed --> -400..0..400)
#define FORWARD_SPEED 200
#define REVERSE_SPEED -200
#define REVERSE_OFFSET 150
#define TURN_SPEED 200
#define RAMMING_SPEED 400
#define STOP 0

// Times in milliseconds
#define REVERSE_TIME 200
#define SCAN_TIME_MIN 200
#define WAIT_TIME 5000
#define STALEMATE_TIME 4000

// Edge detector / line sensor threshold
#define LINE_SENSOR_THRESHOLD 500

// Proximiy sensor threshold
#define PROX_SENSOR_THRESHOLD 1


// Define Zumo hardware systems

// Select older LCD display or newer OLED display
Zumo32U4LCD display;
// Zumo32U4OLED display

Zumo32U4ButtonA buttonA;
Zumo32U4Buzzer buzzer;
Zumo32U4LineSensors lineSensors;
Zumo32U4Motors motors;
Zumo32U4ProximitySensors proxSensors;

// Define state type, current state, and next state
typedef enum StateType
{
  stateAttacking,
  stateBacking,
  stateOrienting,
  stateSearching,
  stateStarting,
} State_t;

State_t currState;

// Define 'left', 'center' 'right', and 'none for the edge detector

typedef enum
{
  edgeLeft,
  edgeCenter,
  edgeRight,
  edgeNone,
} Edge_t;

Edge_t g_edgeStatus;

// Define 'left', 'center', 'right' and 'none' for the proximity detector

typedef enum
{
  proxLeft,
  proxCenter,
  proxRight,
  proxNone,
} Prox_t;

Prox_t g_proxStatus;


// Define auxillary functions

Edge_t IsAtEdge()
{
  // Update global edge status and also return edge detection status

  uint16_t edgeValue[3];

  lineSensors.read(edgeValue);

  if (edgeValue[0] < LINE_SENSOR_THRESHOLD)
  {
    motors.setSpeeds(STOP, STOP); // Stop motors
    g_edgeStatus = edgeLeft;
    return edgeLeft;
  }
  else if (edgeValue[1] < LINE_SENSOR_THRESHOLD)
  {
    motors.setSpeeds(STOP, STOP); // Stop motors
    g_edgeStatus = edgeCenter;
    return edgeCenter;
  }
  else if (edgeValue[2] < LINE_SENSOR_THRESHOLD)
  {
    motors.setSpeeds(STOP, STOP); // Stop motors
    g_edgeStatus = edgeRight;
    return edgeRight;
  }
  else
  {
    g_edgeStatus = edgeNone;
    return edgeNone;
  }
}

Prox_t IsSeen()
{
  // Update global proximity status, and also return status value.
  // Proximity counts = 0..5
  
  proxSensors.read();
  uint8_t leftValue = proxSensors.countsFrontWithLeftLeds();
  uint8_t rightValue = proxSensors.countsFrontWithRightLeds();
  uint8_t centerValue = proxSensors.countsFrontWithLeftLeds(); // FIXME

  if ((leftValue < PROX_SENSOR_THRESHOLD) && (rightValue < PROX_SENSOR_THRESHOLD)
    && (centerValue < PROX_SENSOR_THRESHOLD))
  {
  g_proxStatus = proxNone;
  return proxNone;
  }
  else if ((leftValue >= PROX_SENSOR_THRESHOLD) && (rightValue < PROX_SENSOR_THRESHOLD))
  {
    g_proxStatus = proxLeft;
    return proxLeft;
  }
  else if ((leftValue < PROX_SENSOR_THRESHOLD) && (rightValue >= PROX_SENSOR_THRESHOLD))
  {
    g_proxStatus = proxRight;
    return proxRight;
  }
  else if (leftValue == rightValue)
  {
    g_proxStatus = proxCenter;
    return proxCenter;
  }
}


// Define state handling functions

State_t DoAttackingState()
{
  // Attack opponent
  
  display.clear();
  display.print(F("Attack!!"));

  // Attack tactics - Go to full speed and ram

  motors.setSpeeds(RAMMING_SPEED, RAMMING_SPEED);
  
  for(;;)
  {
    if (IsAtEdge() != edgeNone)
    {
      motors.setSpeeds(STOP, STOP);
      return stateBacking;
    }
  }
}


State_t DoBackingState()
{
  // Back away from edge of arena and resume searching for opponent
  // NOTE: Motors are assumed to be stopped on state entry.
  
  display.clear();
  display.print(F("Backing"));

  // Back up and swerve towards ring center
  switch (g_edgeStatus)
  {
    case edgeLeft:
      motors.setSpeeds(REVERSE_SPEED + REVERSE_OFFSET, REVERSE_SPEED - REVERSE_OFFSET);
      break;
    case edgeRight:
      motors.setSpeeds(REVERSE_SPEED - REVERSE_OFFSET, REVERSE_SPEED + REVERSE_OFFSET);
      break;
    case edgeCenter:
      motors.setSpeeds(REVERSE_SPEED, REVERSE_SPEED);
      break;
  }
  delay(100);

  // Turn in place
  motors.setSpeeds(FORWARD_SPEED, -FORWARD_SPEED);
  delay(250);

  // Resume searching for opponent
  return stateSearching;
}


State_t DoOrientingState()
{
  // Orient to face opponent
  
  display.clear();
  display.print(F("Orient"));
  display.gotoXY(0, 1); // DEBUG
 
  //Orient/track strategy
  motors.setSpeeds(STOP, STOP);

  for (;;)
  {
    IsSeen();
    
    switch (g_proxStatus)
    {
      case proxLeft:
        motors.setSpeeds(-TURN_SPEED, TURN_SPEED);
        display.print(F("Left"));
        break;
      case proxRight:
        motors.setSpeeds(TURN_SPEED, -TURN_SPEED);
        display.print(F("Right"));
        break;
      case proxCenter:
        motors.setSpeeds(TURN_SPEED, TURN_SPEED);
        display.print(F("Center"));
        return stateAttacking;
        break;
      case proxNone:
        display.print(F("None"));
        return stateSearching;
    }
  }
}


State_t DoSearchingState()
{
  // Search for opponent
  
  display.clear();
  display.print(F("Search"));

  // Implement search strategy
  
  motors.setSpeeds(FORWARD_SPEED, FORWARD_SPEED);

  for(;;)
  {
    if (IsAtEdge() != edgeNone) // Edge detected - Back up
    {
      motors.setSpeeds(STOP, STOP);
      return stateBacking;      
    }
    
    else if (IsSeen() != proxNone) // Opponent seen, orient towards them
     return stateOrienting;
  }
}


State_t DoStartingState()
{
  // Wait for match start.
  display.clear();
  display.print(F("Press A"));
  
  // When button A is pressed, wait 5 seconds and start.
  buttonA.waitForButton();
  display.clear();
  display.print(F("Starting"));
  buzzer.playFrequency(1000, 500, 10);
  delay(1000);
  buzzer.playFrequency(1000, 500, 10);
  delay(1000);
  buzzer.playFrequency(1000, 500, 10);
  delay(1000);
  buzzer.playFrequency(1000, 500, 10);
  delay(1000);
  buzzer.playFrequency(1000, 500, 10);
  delay(1000);
  buzzer.playFrequency(1000, 1000, 10);

  return stateSearching;
}

//
// ** Standard Setup Function. **
//

void setup()
{
  // Initialize initial state and sensor status values
  currState = stateStarting;
  g_edgeStatus = edgeNone;
  g_proxStatus = proxNone;

  // Initialize Zumo hardware

  // If necessary, uncomment as needed to fix reversed motor issue.
  // motors.flipLeftMotor(true);
  // motors.flipRightMotor(true);

  motors.setSpeeds(STOP, STOP); // Stop motors
  
  lineSensors.initThreeSensors();
  proxSensors.initThreeSensors();
  
  display.clear();
  display.print(F("ZumoSumo"));
  display.gotoXY(0, 1);
  display.print(F("Ver 0.11"));
  delay(2000);
}

//
// ** Standard loop function. **
//

void loop() {
  switch (currState)
  {
      case stateAttacking:
        currState = DoAttackingState();
        break;
        
      case stateBacking:
        currState = DoBackingState();
        break;
        
      case stateOrienting:
        currState = DoOrientingState();
        break;

      case stateSearching:
        currState = DoSearchingState();
        break;
        
      case stateStarting:
        currState = DoStartingState();
        break;
        
  }
}
