/*
 * 
 * 
 * 
 */


 #include "EncoderMotorControl.h"

encoderMotorController::encoderMotorController(uint8_t motorA_pin_1 , uint8_t motorA_pin_2 ,uint8_t motorB_pin_1 , uint8_t motorB_pin_2,uint8_t encoderA_pin_1,uint8_t encoderA_pin_2){
  analogWriteFreq(PWMFrequency); //Theoretical max frequency is 80000000/range, range = 1024 so 78Khz here
  motorAPin1 = motorA_pin_1;
  motorAPin2 = motorA_pin_2;
  motorBPin1 = motorB_pin_1;
  motorBPin2 = motorB_pin_2;
  pinMode(motorAPin1,OUTPUT);
  pinMode(motorAPin2,OUTPUT);
  pinMode(motorBPin1,OUTPUT);
  pinMode(motorBPin2,OUTPUT);
  lastX = 0;
  lastY = 0;
  for (int a = 0; a < 2; a++){
   // encoderStepTimingBufferPosition[a] = 0;
    lastSampleDeltaT[a] = 0;
    steps[a] = 0;
    totalSteps[a] = 0;
    lastMicros[a] = micros();
    wheelSpeed[a] = 0;
    motorDirection[a] = forward;
    timeOfFirstStep[a] = 0;
    timeOfLastStep[a] = 0;
    timeOfCurrentStep[a] = 0;
    wheelTargetSpeed[a] = 0;
    boostOn[a] = false;
    debounceMinStepTime[a] = (distancePerStep / (MAX_Speed/3600.0)) * 0.75;
    lastError[a] = 0;
  }

}
 float encoderMotorController::checkNormal(float normal){
  if (normal > 1)normal = 1.0;
  if (normal < 0)normal = 0.0;
  return normal;
 }

 int encoderMotorController::makePositive(int number){
  if (number < 0){
    number = -number;
  }
  return number;
}
 double encoderMotorController::makePositive(double number){
  if (number < 0){
    number = -number;
  }
  return number;
}
void encoderMotorController::playNote(int note,int duration){
  analogWriteFreq(78000);
  delay(1);
  double noteFrequency = 1.0 / note ;
  noteFrequency *= 1000000.00;
  double noteHalfDuration = noteFrequency * 0.5; 
  double a=0;
  while (a<(duration/(noteFrequency/1000.0))){
    a++;
  analogWrite(motorAPin1,0);
  analogWrite(motorAPin2,PWMWriteRange);
  analogWrite(motorBPin1,0);
  analogWrite(motorBPin2,PWMWriteRange);
  delayMicroseconds(noteHalfDuration);
  analogWrite(motorAPin1,PWMWriteRange);
  analogWrite(motorAPin2,0);
  analogWrite(motorBPin1,PWMWriteRange);
  analogWrite(motorBPin2,0);
  delayMicroseconds(noteHalfDuration);
  }
  analogWriteFreq(PWMFrequency);
  analogWrite(motorAPin1,0);
  analogWrite(motorAPin2,0);
  analogWrite(motorBPin1,0);
  analogWrite(motorBPin2,0);
  delay(30);
}

void encoderMotorController::takeStep(int encoder){
  // debounce
  if ((micros() - lastMicros[encoder]) < debounceMinStepTime[encoder]){
//    if ((micros() - lastMicros[encoder]) > 1000){
//    Serial.println();
//    Serial.println((micros() - lastMicros[encoder]));
//    }
    return;                                         // this is a bounce, ignore it
  }
  lastMicros[encoder] = micros();
  // update grid
  updateGrid(encoder);
  
  // update heading + steps
    heading += ((encoder == 1) * motorDirection[encoder] * -anglePerStep ) + ( (encoder == 0 ) * motorDirection[encoder] * anglePerStep );
    if (heading > 360)heading -= 360.0;
    else if (heading < 0)heading += 360.0;
    totalSteps[encoder]++;
    steps[encoder]++;
  // Stop overshoot on start / boost
  if (boostOn[encoder] == true){ // first step after start
      wheelSpeed[encoder] = MIN_Speed;
      if (encoder == 0){
        PWMA = minMotorSpeed * PWMWriteRange;
      }else{
        PWMB = minMotorSpeed * PWMWriteRange;
      }
      boostOn[encoder] = false;
    }

  // update speed buffer / last pulse time 
  if ((micros() - timeOfCurrentStep[encoder]) > minCalculatedSpeedTimePerStep){  // beyond max time so reset minCalculatedSpeedTimePerStep
    timeOfLastStep[encoder] = micros();                   // reset timers
    timeOfCurrentStep[encoder] = timeOfLastStep[encoder];
    timeOfFirstStep[encoder] = timeOfLastStep[encoder];
    wheelSpeed[encoder] = 0;
  }else{
    timeOfLastStep[encoder] = timeOfCurrentStep[encoder];
    timeOfCurrentStep[encoder] = micros();
  }

  // update targets

  // TODO: check bot targets 
  
  if (botTargetDistance > 0){
    volatile long currentDistance = ((totalSteps[0] + totalSteps[1]) * 0.5) * distancePerStep;
    if ((botTargetDistance - currentDistance) >= 100){
      wheelTargetSpeed[0] = botmodeSpeed;
      wheelTargetSpeed[1] = botmodeSpeed;
    }
    if ((botTargetDistance - currentDistance) < 100){/*
      if (PWMA > PWMB){PWMB = PWMA;}else{PWMA = PWMB;}
          if (PWMA > 256){
            PWMA = minMotorSpeed*PWMWriteRange;
            PWMB = minMotorSpeed*PWMWriteRange;
            setMotorSpeed();
          }*/
          double botRemainingDistance = double(botTargetDistance - currentDistance);
      //wheelTargetSpeed[0] = MIN_Speed + (botmodeSpeed - MIN_Speed)*(botRemainingDistance/100);
      //wheelTargetSpeed[1] = MIN_Speed + (botmodeSpeed - MIN_Speed)*(botRemainingDistance/100);
      botTargetSpeed = MIN_Speed + (botmodeSpeed - MIN_Speed)*(botRemainingDistance / 100.0);
    }
    if (currentDistance >= (botTargetDistance - distancePerStep*4.0)){
      botTargetDistance = 0;
      wheelTargetSpeed[0] = 0;
      wheelTargetSpeed[1] = 0;
      PWMA = 0;
      PWMB = 0;
      //targetHeading = heading;
      botTargetSpeed = 0;
      setMotorSpeed();
      commandCompleted = true;
    }
  }
      double headingToTarget = targetHeading - heading;
      if (headingToTarget > 180)headingToTarget-=360;
      if (headingToTarget < -180)headingToTarget+=360;
  if (botTurnDirection == turnLeft || botTurnDirection == turnRight){ // stop overshoot
    if (makePositive(headingToTarget) < (anglePerStep * 0.5) ){
      //Serial.println("\r\nT" + String(targetHeading) + "H" + String(heading));
      PWMA=0;
      PWMB=0;
      setMotorSpeed();
      commandCompleted = true;
    }
  }
  
  if (botTurnDirection == none && botTargetSpeed > 0){
    if (makePositive(headingToTarget) < (anglePerStep * 0.5) ){ // instant nudge
      wheelTargetSpeed[0] = botTargetSpeed;
      wheelTargetSpeed[1] = botTargetSpeed;
      PID();
    }
  }
  
}

void encoderMotorController::manualDrive(int X, int Y){
  if (commandSetHasCommands){
    cancelCommandSet();                                                        // cancel any outstanding turtle / auto mode commands targets etc...
  }
  inBotMode = false;
  botTurnDirection = none;                                                     // turn to closest heading
  if (X > MAX_range)X = MAX_range;
  if (X < -MAX_range)X = -MAX_range;
  if (Y > MAX_range)Y = MAX_range;
  if (Y < -MAX_range)Y = -MAX_range;
  
  // temporary drive steering function
  if(X)targetHeading = heading + (double(X) / double(MAX_range)) * MAX_heading_Change * botTargetDirection;
  
  //if (!lastX && !lastY) targetHeading = heading;                               // start from standstill, based on input not speed
    if (!lastX && !lastX && !Y && !X){// no driving at all
      targetHeading = heading; // dump any incomplete turning
    }
    if (lastX > 0 && X < 0){ // change direction
      targetHeading = heading; // dump any incomplete turning
    }
    if (lastX < 0 && X > 0){
      targetHeading = heading; // dump any incomplete turning
    }
  lastX = X;
  lastY = Y;
  if (!Y)targetHeading = heading; // not moving forward dump incomplete turning
  if (Y < 0){motorDirection[0] = forward; motorDirection[1] = forward; botTargetDirection = forward;}
  else if (Y > 0) {motorDirection[0] = reverse; motorDirection[1] = reverse; botTargetDirection = reverse;} // if Y = 0 coasting in same direction
  botTargetSpeed = 0;
  if (Y)botTargetSpeed = ( double(makePositive(Y)) / double(MAX_range) ) * double(MAX_Speed - MIN_Speed) + double(MIN_Speed);
  wheelTargetSpeed[0] = botTargetSpeed;
  wheelTargetSpeed[1] = wheelTargetSpeed[0];

  //  calculate heading change in degrees per second from X and update target heading
  /*
  double headingToTarget = targetHeading - heading;
      if (headingToTarget > 180)headingToTarget-=360;
      if (headingToTarget < -180)headingToTarget+=360;
      if (makePositive(headingToTarget) < 140.0){
        targetDegreesPerSecond = double(X) / double(MAX_range) * MAX_heading_Change;
      }else{targetDegreesPerSecond = 0;}
  */

}

  void encoderMotorController::reverseMotorA(){    // swap pins
    uint8_t holder;
    holder = motorAPin1;
    motorAPin1 = motorAPin2;
    motorAPin2 = holder;
  }
  
  void encoderMotorController::reverseMotorB(){    // swap pins
    uint8_t holder;
    holder = motorBPin1;
    motorBPin1 = motorBPin2;
    motorBPin2 = holder;
  }

void encoderMotorController::updateGrid(int encoder){
  //gridX += (0.5 * distancePerStep * motorDirection[encoder]) * cos(heading + anglePerStep);
  //gridY += (0.5 * distancePerStep * motorDirection[encoder]) * sin(heading + anglePerStep);
}

void encoderMotorController::startCommandSet(String theCommandSet){
  commandSet = theCommandSet;
  if (commandSet.indexOf("data") == -1){ // error, no data found or invalid string
    cancelCommandSet();
    return;
  }
  commandSet.remove(0,5); // trimm data & comma from front of string
  commandSetHasCommands = true;
  nextCommandMillis = 0;
  //nextCommandMillis = millis() + delaybetweenCommands;
  inBotMode = false; // so command set will set the target to the heading on the first command only
  getNextCommand();
}

boolean encoderMotorController::getNextCommand(){
  allStop();
  String Command;
  float value;
  if (commandSet.length() && commandSetHasCommands){ // has data left
    int indexOfComma = commandSet.indexOf(",");
    Command = commandSet.substring(0,indexOfComma);
    commandSet.remove(0,indexOfComma+1);
    int nextComma =  commandSet.indexOf(",");
    value = commandSet.substring(0,nextComma).toFloat();
    commandSet.remove(0,nextComma+1);
    long t = millis();
    processCommand(Command , value);
  }else{
    cancelCommandSet();
    return false;
  }
  return true;
}
void encoderMotorController::processCommand(String command, double value){
  // process single character commands
  char oneChar[2];
  command.toCharArray(oneChar,2);
//Serial.println("command processed " + String(oneChar));
  switch(int(oneChar[0])){
    case 'F':
    {
      botTargetDistance = value * 10 + ((totalSteps[0] + totalSteps[1]) * 0.5) * distancePerStep;
      //Serial.println("\r\n bot total travel target " + String(botTargetDistance));
      if (!inBotMode){
      targetHeading = heading;
      inBotMode = true;
      }
      botTargetSpeed = botmodeSpeed;
      wheelTargetSpeed[0] = botmodeSpeed;
      wheelTargetSpeed[1] = wheelTargetSpeed[0];
      motorDirection[0] = forward;
      motorDirection[1] = forward;
      botTargetDirection = forward;
      botTurnDirection = none;
    }
    break;

    case 'B':
    {
      botTargetDistance = value * 10 + ((totalSteps[0] + totalSteps[1]) * 0.5) * distancePerStep;
      //Serial.println("\r\n bot total travel target " + String(botTargetDistance));
      if (!inBotMode){
      targetHeading = heading;
      inBotMode = true;
      }
      botTargetSpeed = botmodeSpeed;
      wheelTargetSpeed[0] = botmodeSpeed;
      wheelTargetSpeed[1] = wheelTargetSpeed[0];
      motorDirection[0] = reverse;
      motorDirection[1] = reverse;
      botTargetDirection = reverse;
      botTurnDirection = none;
    }
    break;

    case 'L':
    {
     // Serial.println("\r\n turn Left ");
     if (!inBotMode){
      targetHeading = heading;
      inBotMode = true;
      }
      botTargetSpeed = 0.0;
      targetHeading = targetHeading - value;
      if (targetHeading < 0)targetHeading += 360;
      motorDirection[0] = forward;
      motorDirection[1] = forward;
      botTargetDirection = forward;
      botTurnDirection = turnLeft;
    }
    break;

    case 'R':
    {
     // Serial.println("\r\n turn Right ");
     if (!inBotMode){
      targetHeading = heading;
      inBotMode = true;
      }
      botTargetSpeed = 0.0;
      targetHeading = targetHeading + value;
      if (targetHeading > 360)targetHeading -= 360;
      motorDirection[0] = forward;
      motorDirection[1] = forward;
      botTargetDirection = forward;
      botTurnDirection = turnRight;
    }
    break;
    default:
   // Serial.println("\r\n not single character command");
    break;
  }
  // process longer commands
  if(command.indexOf("AS") != -1){ // all stop
    cancelCommandSet();
  }
}
void encoderMotorController::cancelCommandSet(){
  if (commandSetHasCommands){ // only cancel if running
    allStop();
    commandSet.remove(0);
    commandSetHasCommands = false;
    targetHeading = heading;
  }
}

void encoderMotorController::update(){
  unsigned long lastDeltaT = micros() - lastUpdateMicros;
  unsigned long lastDeltaTA = timeOfCurrentStep[0] - timeOfFirstStep[0];
  unsigned long lastDeltaTB = timeOfCurrentStep[1] - timeOfFirstStep[1];
  lastUpdateMicros = micros();
 // botCurrentSpeed = ( (( double(steps[0] + steps[1]) * distancePerStep) * 0.5) / double(lastDeltaT)) * 3600.0; // km / hr
  if (steps[0] > 0)wheelSpeed[0] = (double(steps[0]) * distancePerStep) / double(lastDeltaTA) * 3600.0;
  if (steps[1] > 0)wheelSpeed[1] = (double(steps[1]) * distancePerStep) / double(lastDeltaTB) * 3600.0;
  //if(!lastDeltaTA)wheelSpeed[0] = 0;
  //if(!lastDeltaTB)wheelSpeed[1] = 0;
  botCurrentSpeed = (wheelSpeed[0] + wheelSpeed[1]) * 0.5;
  //dynamically adjust debounce baced on wheel speed
  //if (wheelSpeed[0]>MIN_Speed)debounceMinStepTime[0] = (distancePerStep / (wheelSpeed[0]/3600.0)) * 0.5;//wheelSpeed[0]/wheelTargetSpeed[0]
  //if (wheelSpeed[1]>MIN_Speed)debounceMinStepTime[1] = (distancePerStep / (wheelSpeed[1]/3600.0)) * 0.5;
 // if (lastDeltaTA == 0 && lastDeltaTB == 0)botCurrentSpeed = 0;
  //Serial.println("A" + String(wheelSpeed[0]) + "B" + String(wheelSpeed[1]) + "Bs" + String(botCurrentSpeed) );
  //Serial.println("PWMA" + String(PWMA) + "PWMB" + String(PWMB) ) ;
  //Serial.println("MSTA" + String(lastDeltaTA)+"SA" + String(steps[0]) );
  //Serial.println("DBT" + String(debounceMinStepTime[0]) + "MSTA" + String(lastDeltaTA) );
  if ((micros() - timeOfCurrentStep[0]) > minCalculatedSpeedTimePerStep){  // beyond max time so reset minCalculatedSpeedTimePerStep
    timeOfLastStep[0] = micros();                   // reset timers
    timeOfCurrentStep[0] = timeOfLastStep[0];
    timeOfFirstStep[0] = timeOfLastStep[0];
    wheelSpeed[0] = 0;
    //Serial.println("STOP0");
  }
    if ((micros() - timeOfCurrentStep[1]) > minCalculatedSpeedTimePerStep){  // beyond max time so reset minCalculatedSpeedTimePerStep
    timeOfLastStep[1] = micros();                   // reset timers
    timeOfCurrentStep[1] = timeOfLastStep[1];
    timeOfFirstStep[1] = timeOfLastStep[1];
    wheelSpeed[1] = 0;
    //Serial.println("STOP1");
  }
if (steps[0] > 0)timeOfFirstStep[0] = timeOfCurrentStep[0];
if (steps[1] > 0)timeOfFirstStep[1] = timeOfCurrentStep[1];
  steps[0] = 0;
  steps[1] = 0;
  
  if (commandCompleted && commandSetHasCommands){
    if (!nextCommandMillis){
      nextCommandMillis = millis() + delaybetweenCommands;
      allStop();
      commandCompleted = true; // just to keep flag
    }
    if (millis() > nextCommandMillis && nextCommandMillis){
      nextCommandMillis = 0;
      getNextCommand();
    }/*
    if ((nextCommandMillis + delaybetweenCommands) < millis()){
      nextCommandMillis = millis() + delaybetweenCommands;
      getNextCommand();
    }else{
      allStop();
      commandCompleted = true; // just to keep flag
    }*/
  }
  updateSteering(lastDeltaT);
  PID();
}

void encoderMotorController::allStop(){
      botTargetDistance = 0;
      wheelTargetSpeed[0] = 0;
      wheelTargetSpeed[1] = 0;
      PWMA = 0;
      PWMB = 0;
      //targetHeading = heading;// keep last heading to correct for any overshoot
      botTargetSpeed = 0;
      botTurnDirection = none;
      setMotorSpeed();
      targetDegreesPerSecond = 0;
      commandCompleted = false;
  // TODO: set all target flags to false
  //       set target speed to 0 so PID can slow to stop.
}

void encoderMotorController::setMotorSpeed(){

  analogWrite(motorAPin1,PWMA * (motorDirection[0] < 0) );
  analogWrite(motorAPin2,PWMA * (motorDirection[0] > 0) );
  analogWrite(motorBPin1,PWMB * (motorDirection[1] < 0) );
  analogWrite(motorBPin2,PWMB * (motorDirection[1] > 0) );
}
void encoderMotorController::setMotorSpeed(int newPWMA, int newPWMB){
  analogWrite(motorAPin1,newPWMA * (motorDirection[0] < 0) );
  analogWrite(motorAPin2,newPWMA * (motorDirection[0] > 0) );
  analogWrite(motorBPin1,newPWMB * (motorDirection[1] < 0) );
  analogWrite(motorBPin2,newPWMB * (motorDirection[1] > 0) );
}

void encoderMotorController::PID(){
  //TODO: calculate PID for motor PWM baced on motor target speed
  // currently being called every 50 mS (20 Hz)
  // wheelSpeed[0];               // is current left motor speed in km / hr
  // wheelSpeed[1];               // is current right motor speed in km / hr
  // wheelTargetSpeed[0];         // is the left motor target speed
  // wheelTargetSpeed[1];         // is the right motor target speed
  // MAX_Speed;                   // is bot maximum speed
  // MIN_Speed;                   // minimum speed to prevent stall, takes about 50% more to overcome starting torque

// The following functions are a temporary fix for testing

      double errorA = wheelTargetSpeed[0] - wheelSpeed[0];
      double errorB = wheelTargetSpeed[1] - wheelSpeed[1];
      double maxPWMChange = 100.0;
      int PWMChangeIncreaseA = int(maxPWMChange * (makePositive(errorA) / MAX_Speed));
      int PWMChangeIncreaseB = int(maxPWMChange * (makePositive(errorB) / MAX_Speed));
      int PWMChangeDecreaseA = int((maxPWMChange * (makePositive(errorA) / MAX_Speed)) * 0.6);
      int PWMChangeDecreaseB = int((maxPWMChange * (makePositive(errorB) / MAX_Speed)) * 0.6);
      if (PWMChangeIncreaseA > PWMWriteRange)PWMChangeIncreaseA = 0;
      if (PWMChangeIncreaseB > PWMWriteRange)PWMChangeIncreaseB = 0;
      if (PWMChangeDecreaseA > PWMWriteRange)PWMChangeDecreaseA = 0;
      if (PWMChangeDecreaseB > PWMWriteRange)PWMChangeDecreaseB = 0;
      lastError[0] = errorA;
      lastError[1] = errorB;
      //PWMChange = int(PWMChange * 0.5) + int( (double(PWMChange)*0.5 * (botTargetSpeed / MAX_Speed)));
      if (botTurnDirection == turnLeft || botTurnDirection == turnRight){
        PWMChangeIncreaseA = 6;
        PWMChangeIncreaseB = 6;
        PWMChangeDecreaseA = 2;
        PWMChangeDecreaseB = 2;
      }
      if (!wheelTargetSpeed[0] && !wheelTargetSpeed[1]){
          if (PWMA > PWMB){PWMB = PWMA;}else{PWMA = PWMB;}
          if (PWMA > 256){
            PWMA = 256;
            PWMB = 256;
          }

      if (PWMA <= 256){
        PWMChangeDecreaseA = 8;
        PWMChangeDecreaseB = 8;
      }
    }
      if ((PWMA == 0) && (wheelTargetSpeed[0] >= MIN_Speed)){
        PWMA = startPWMBoost; // start boost
        timeOfLastStep[0] = micros();
        timeOfCurrentStep[0] = timeOfLastStep[0];
        boostOn[0] = true;
        errorA = 0;
      }
      if ((PWMB == 0) && (wheelTargetSpeed[1] >= MIN_Speed)){
        PWMB = startPWMBoost;
        timeOfLastStep[1] = micros();
        timeOfCurrentStep[1] = timeOfLastStep[1];
        boostOn[1] = true;
        errorB = 0;
      }
   
      if (errorA > 0.05){
        PWMA += PWMChangeIncreaseA;
        if (PWMA > PWMWriteRange)PWMA = PWMWriteRange;
      }
      if (errorA < -0.1){
        PWMA -= PWMChangeDecreaseA;
        if (PWMA < 0)PWMA = 0;
      }
      if (errorB > 0.05){
        PWMB += PWMChangeIncreaseB;
        if (PWMB > PWMWriteRange)PWMB = PWMWriteRange;
      }
      if (errorB < -0.1){
        PWMB -= PWMChangeDecreaseB;
        if (PWMB < 0)PWMB = 0;
      }
   if ((PWMA < minMotorSpeed * PWMWriteRange) && (wheelTargetSpeed[0] < MIN_Speed)){
    PWMA = 0;
   }
   if ((PWMB < minMotorSpeed * PWMWriteRange) && (wheelTargetSpeed[1] < MIN_Speed)){
    PWMB = 0;
   }
//Serial.println("PIA" + String(PWMChangeIncreaseA) + "PIB" + String(PWMChangeIncreaseB) );
  setMotorSpeed();      // output pwm to motors range is 0 - 1023
}
double encoderMotorController::getSpeed(){
  return botCurrentSpeed;
}
double encoderMotorController::getheading(){
  return heading;
}
double encoderMotorController::getTravel(){
  return (totalSteps[0]+totalSteps[1]) / 2.0 * distancePerStep;
}
double encoderMotorController::getAcceleration(){
  return 0;
}
void encoderMotorController::updateSteering(long delatT){
    if (targetDegreesPerSecond != 0){
    double headingchange = (targetDegreesPerSecond / 1000000.0) * delatT;
    //targetDegreesPerSecond = 0;
   // targetHeading += headingchange * double(botTargetDirection);
      //Serial.println("hc"+String(headingchange));
    if (targetHeading > 360)targetHeading -= 360;
    if (targetHeading < 0)targetHeading += 360;
    }
      double headingToTarget = targetHeading - heading;
      if (headingToTarget > 180)headingToTarget-=360;
      if (headingToTarget < -180)headingToTarget+=360;
      wheelTargetSpeed[0] = botTargetSpeed;
      wheelTargetSpeed[1] = botTargetSpeed;
      motorDirection[0] = botTargetDirection;
      motorDirection[1] = botTargetDirection;
      double changeDegreesPerSecond = 0;

      double thisMAXSpeed = distancePerDegreeChange * MAX_heading_Change * 0.0036;
      //if (thisMAXSpeed < MIN_Speed)thisMAXSpeed = MIN_Speed;
      //thisMAXSpeed = MIN_Speed;
      double positiveHeadingToTarget = makePositive(headingToTarget);
      //Serial.println("pHTT"+String(positiveHeadingToTarget));     
   if (positiveHeadingToTarget > anglePerStep * 1.0 && (botTurnDirection != none)){ // change speed to correct heading for bot pivit
      //Serial.println("H" + String(heading) + "TH" + String(targetHeading) + "d" + String(headingToTarget));

      if (botTurnDirection == turnRight){
        //Serial.println("PWMA:" + String(PWMA) + "PWMB" + String(PWMB));
        //Serial.println("WS0"+String(wheelSpeed[0]));
        if (PWMA > startPWMBoost){ // prevent overshoot on start
          PWMA = startPWMBoost;
        }
        if (PWMB > startPWMBoost){
          PWMB = startPWMBoost;
        }

        if (positiveHeadingToTarget >= MAX_heading_Change){ // greater than can change in 1 second
            wheelTargetSpeed[0] = MIN_Speed;
            wheelTargetSpeed[1] = MIN_Speed;
            motorDirection[1] = -botTargetDirection;
        }else{// heading is less than max or is 0 change 
          double thisSpeed = thisMAXSpeed * (positiveHeadingToTarget / MAX_heading_Change);
          //if (thisSpeed < MIN_Speed)thisSpeed = MIN_Speed;
            wheelTargetSpeed[0] = MIN_Speed;
            wheelTargetSpeed[1] = MIN_Speed;
            motorDirection[1] = -botTargetDirection;
        }
      }
      
      if (botTurnDirection == turnLeft){
        //Serial.println("PWMA:" + String(PWMA) + "PWMB" + String(PWMB));
        if (PWMA > startPWMBoost){ // prevent overshoot on start
          PWMA = startPWMBoost;
        }
        if (PWMB > startPWMBoost){
          PWMB = startPWMBoost;
        }
        if (positiveHeadingToTarget >= MAX_heading_Change){ // greater than can change in 1 second
            wheelTargetSpeed[0] = MIN_Speed;
            wheelTargetSpeed[1] = MIN_Speed;
            motorDirection[0] = -botTargetDirection;
        }else{// heading is less than max or is 0 change 
          double thisSpeed = thisMAXSpeed * (positiveHeadingToTarget / MAX_heading_Change);
         // if (thisSpeed < MIN_Speed)thisSpeed = MIN_Speed;
            wheelTargetSpeed[0] = MIN_Speed;
            wheelTargetSpeed[1] = MIN_Speed;
            motorDirection[0] = -botTargetDirection;
        }
      }
    }
//Serial.println("T" + String(positiveHeadingToTarget) + "TH" + String(targetHeading) + "H" + String(heading));
   if ((positiveHeadingToTarget > (anglePerStep * 0.5)) && (botTurnDirection == none)){ // change speed to correct heading for bot drive
    if (botTurnDirection == none && botTargetSpeed){ // only when moving at least min speed
        wheelTargetSpeed[0] = botTargetSpeed;
        wheelTargetSpeed[1] = botTargetSpeed;
        double thisSpeed = botTargetSpeed * 0.22;//0.25;//thisMAXSpeed;
          if (positiveHeadingToTarget < 15 ){ // greater than can change in 1 second
              thisSpeed = botTargetSpeed * 0.22 * 0.3;// (positiveHeadingToTarget/15.0);//0.13;//thisMAXSpeed * 1.0;
            }
        if (headingToTarget > 0 ){ // need to turn right
            wheelTargetSpeed[0] += (thisSpeed * double(botTargetDirection));
            wheelTargetSpeed[1] -= (thisSpeed * double(botTargetDirection));
        }else{
            wheelTargetSpeed[0] -= (thisSpeed * double(botTargetDirection));
            wheelTargetSpeed[1] += (thisSpeed * double(botTargetDirection));
        }
        if (wheelTargetSpeed[0] < MIN_Speed){ // correct speeds
         // wheelTargetSpeed[1] += (MIN_Speed - wheelTargetSpeed[0]);
          wheelTargetSpeed[0] = MIN_Speed;
        }
        if (wheelTargetSpeed[1] < MIN_Speed){ // correct speeds
         // wheelTargetSpeed[0] += MIN_Speed - wheelTargetSpeed[1];
          wheelTargetSpeed[1] = MIN_Speed;
        }
        if (wheelTargetSpeed[0] > MAX_Speed){
          wheelTargetSpeed[1] -= wheelTargetSpeed[0] - MAX_Speed;
          wheelTargetSpeed[0] = MAX_Speed;
        }
        if (wheelTargetSpeed[1] > MAX_Speed){
          wheelTargetSpeed[0] -= wheelTargetSpeed[1] - MAX_Speed;
          wheelTargetSpeed[1] = MAX_Speed;
        }
      }
  }
}

