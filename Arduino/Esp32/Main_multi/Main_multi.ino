long currentTime = 0;
long elapsedTime = 0;
long previousTime = 0;
double Force_Current_KF = 0.;
double Force_Current_KF_dt = 0.;
float averageCycleTime = 0.0f;
uint64_t maxCycles = 1000;
uint64_t cycleIdx = 0;
int32_t joystickNormalizedToInt32 = 0;
float delta_t = 0.;
float delta_t_pow2 = 0.;
float delta_t_pow3 = 0.;
float delta_t_pow4 = 0.;
long Position_Next = 0;
long set = 0;
bool checkPosition = 1;

bool absActive = 0;
float absFrequency = 2 * PI * 5;
float absAmplitude = 100;
float absTime = 0;
float stepperAbsOffset = 0;
float absDeltaTimeSinceLastTrigger = 0;

float stepperRange = 0;

bool resetPedalPosition = false;
bool configUpdateAvailable = false;





#include "DiyActivePedal_types.h"
DAP_config_st dap_config_st;
DAP_config_st dap_config_st_local;


int32_t pcnt = 0;

#define RAD_2_DEG 180.0f / PI





//USBCDC USBSerial;

#define MIN_STEPS 5

//#define SUPPORT_ESP32_PULSE_COUNTER


//#define PRINT_CYCLETIME



/**********************************************************************************************/
/*                                                                                            */
/*                         multitasking  definitions                                          */
/*                                                                                            */
/**********************************************************************************************/
#include "soc/rtc_wdt.h"

//rtc_wdt_protect_off();    // Turns off the automatic wdt service
//rtc_wdt_enable();         // Turn it on manually
//rtc_wdt_set_time(RTC_WDT_STAGE0, 20000);  // Define how long you desire to let dog wait.



TaskHandle_t Task1;
TaskHandle_t Task2;
//SemaphoreHandle_t batton;
//SemaphoreHandle_t semaphore_updateJoystick;

static SemaphoreHandle_t semaphore_updateConfig=NULL;
static SemaphoreHandle_t semaphore_updateJoystick=NULL;


/**********************************************************************************************/
/*                                                                                            */
/*                         joystick  definitions                                              */
/*                                                                                            */
/**********************************************************************************************/
#include <BleGamepad.h>

#define JOYSTICK_MIN_VALUE 0
#define JOYSTICK_MAX_VALUE 10000

BleGamepad bleGamepad("DiyActiveBrake", "DiyActiveBrake", 100);




/**********************************************************************************************/
/*                                                                                            */
/*                         endstop definitions                                                */
/*                                                                                            */
/**********************************************************************************************/
#define minPin 34
#define maxPin 35
#define ENDSTOP_MOVEMENT 5 // movement per cycle to find endstop positions

long stepperPosMin = 0;
long stepperPosMax = 0;
long stepperPosMin_global = 0;
long stepperPosMax_global = 0;
bool minEndstopNotTriggered = false;
bool maxEndstopNotTriggered = false;
long stepperPosPrevious = 0;
long stepperPosCurrent = 0;


/**********************************************************************************************/
/*                                                                                            */
/*                         pedal mechanics definitions                                        */
/*                                                                                            */
/**********************************************************************************************/
float startPosRel = 0.35;
float endPosRel = 0.8;

float springStiffnesss = 1;
float springStiffnesssInv = 1;
float Force_Min = 0.1;    //Min Force in lb to activate Movement
float Force_Max = 5.;     //Max Force in lb = Max Travel Position
double conversion = 4000.;


/**********************************************************************************************/
/*                                                                                            */
/*                         Kalman filter definitions                                          */
/*                                                                                            */
/**********************************************************************************************/
#include <Kalman.h>
using namespace BLA;
// Configuration of Kalman filter
// assume constant rate of change 
// observed states:
// x = [force, d force / dt]
// state transition matrix
// x_k+1 = [1, delta_t; 0, 1] * x_k


// Dimensions of the matrices
#define KF_CONST_VEL
#define Nstate 2 // length of the state vector
#define Nobs 1   // length of the measurement vector
#define KF_MODEL_NOISE_FORCE_ACCELERATION (float)500.0f // adjust model noise here

KALMAN<Nstate,Nobs> K; // your Kalman filter
BLA::Matrix<Nobs, 1> obs; // observation vector



/**********************************************************************************************/
/*                                                                                            */
/*                         loadcell definitions                                               */
/*                                                                                            */
/**********************************************************************************************/
#define LOADCELL_STD_MIN 0.001f
#define LOADCELL_VARIANCE_MIN LOADCELL_STD_MIN*LOADCELL_STD_MIN

float loadcellOffset = 0.0f;     //offset value
float varEstimate = 0.0f; // estimated loadcell variance
float stdEstimate = 0.0f;



/**********************************************************************************************/
/*                                                                                            */
/*                         stepper motor definitions                                          */
/*                                                                                            */
/**********************************************************************************************/

#include "FastAccelStepper.h"

// Stepper Wiring
#define dirPinStepper    0//8
#define stepPinStepper   4//9

//no clue what this does
FastAccelStepperEngine engine = FastAccelStepperEngine();
FastAccelStepper *stepper = NULL;


#define TRAVEL_PER_ROTATION_IN_MM (float)5.0f
#define STEPS_PER_MOTOR_REVOLUTION (float)300.0f
#define MAXIMUM_STEPPER_RPM (float)6000.0f
#define MAXIMUM_STEPPER_SPEED (MAXIMUM_STEPPER_RPM/60*STEPS_PER_MOTOR_REVOLUTION)   //needs to be in us per step || 1 sec = 1000000 us
#define SLOW_STEPPER_SPEED (float)(MAXIMUM_STEPPER_SPEED * 0.05f)
#define MAXIMUM_STEPPER_ACCELERATION (float)1e6




/**********************************************************************************************/
/*                                                                                            */
/*                         ADC definitions                                                    */
/*                                                                                            */
/**********************************************************************************************/
#include <SPI.h>
#include <ADS1256.h>
#define NUMBER_OF_SAMPLES_FOR_LOADCELL_OFFFSET_ESTIMATION 1000

float clockMHZ = 7.68; // crystal frequency used on ADS1256
float vRef = 2.5; // voltage reference

// Construct and init ADS1256 object
ADS1256 adc(clockMHZ,vRef,false); // RESETPIN is permanently tied to 3.3v
double loadcellReading;




  





/**********************************************************************************************/
/*                                                                                            */
/*                         helper function                                                    */
/*                                                                                            */
/**********************************************************************************************/



// initialize configuration struct at startup
void initConfig()
{

  dap_config_st.payloadType = 100;
  dap_config_st.version = 0;
  dap_config_st.pedalStartPosition = 35;
  dap_config_st.pedalEndPosition = 80;

  dap_config_st.maxForce = 90;
  dap_config_st.preloadForce = 1;

  dap_config_st.relativeForce_p000 = 0;
  dap_config_st.relativeForce_p020 = 20;
  dap_config_st.relativeForce_p040 = 40;
  dap_config_st.relativeForce_p060 = 60;
  dap_config_st.relativeForce_p080 = 80;
  dap_config_st.relativeForce_p100 = 100;

  dap_config_st.dampingPress = 0;
  dap_config_st.dampingPull = 0;

  dap_config_st.absFrequency = 5;
  dap_config_st.absAmplitude = 100;

  dap_config_st.lengthPedal_AC = 150;
  dap_config_st.horPos_AB = 215;
  dap_config_st.verPos_AB = 80;
  dap_config_st.lengthPedal_CB = 200;
}

// update the local variables used for computation from the config struct
void updateComputationalVariablesFromConfig()
{

  startPosRel = ((float)dap_config_st.pedalStartPosition) / 100.0f;
  endPosRel = ((float)dap_config_st.pedalEndPosition) / 100.0f;

  Force_Min = ((float)dap_config_st.preloadForce) / 10.0f;
  Force_Max = ((float)dap_config_st.maxForce) / 10.0f;

  absFrequency = ((float)dap_config_st.absFrequency);
  absAmplitude = ((float)dap_config_st.absAmplitude);

  springStiffnesss = (Force_Max-Force_Min) / (float)(stepperPosMax-stepperPosMin);
  springStiffnesssInv = 1.0 / springStiffnesss;

}

// compute pedal incline angle
float computePedalInclineAngle(float sledPosInCm)
{

  // see https://de.wikipedia.org/wiki/Kosinussatz
  // A: is lower pedal pivot
  // C: is upper pedal pivot
  // B: is rear pedal pivot
  float a = ((float)dap_config_st.lengthPedal_CB) / 10.0f;
  float b = ((float)dap_config_st.lengthPedal_AC) / 10.0f;
  float c_ver = ((float)dap_config_st.verPos_AB) / 10.0f;
  float c_hor = ((float)dap_config_st.horPos_AB) / 10.0f;
  c_hor += sledPosInCm / 10.0f;
  float c = sqrtf(c_ver * c_ver + c_hor * c_hor);

  /*Serial.print("a: ");
  Serial.print(a);

  Serial.print(", b: ");
  Serial.print(b);

  Serial.print(", c: ");
  Serial.print(c);

  Serial.print(", sledPosInCm: ");
  Serial.print(sledPosInCm);*/

  float nom = b*b + c*c - a*a;
  float den = 2 * b * c;

  float alpha = 0;
   
  if (abs(den) > 0.01)
  {
    alpha = acos( nom / den );
  }

  
  /*Serial.print(", alpha1: ");
  Serial.print(alpha * RAD_2_DEG);*/


  // add incline due to AB incline --> result is incline realtive to horizontal 
  if (abs(c_hor)>0.01)
  {
    alpha += atan(c_ver / c_hor);
  }

  /*Serial.print(", alpha2: ");
  Serial.print(alpha * RAD_2_DEG);
  Serial.println(" ");*/
  
  return alpha * RAD_2_DEG;
  
}


/**********************************************************************************************/
/*                                                                                            */
/*                         setup function                                                     */
/*                                                                                            */
/**********************************************************************************************/
void setup()
{
  //Serial.begin(115200);
  Serial.begin(921600);
  Serial.setTimeout(5);

  


  //batton = xSemaphoreCreateBinary();
  //semaphore_updateJoystick = xSemaphoreCreateBinary();
  semaphore_updateJoystick = xSemaphoreCreateMutex();
  semaphore_updateConfig = xSemaphoreCreateMutex();


  if(semaphore_updateJoystick==NULL)
  {
    Serial.println("Could not create semaphore");
    ESP.restart();
  }

  disableCore0WDT();


  // initialize configuration and update local variables
  initConfig();
  updateComputationalVariablesFromConfig();


  //USBSerial.begin(921600);

  delay(1000);

  //activate bluetooth controller
  /*BleGamepadConfiguration bleGamepadConfig;
  bleGamepadConfig.setControllerType(CONTROLLER_TYPE_MULTI_AXIS); // CONTROLLER_TYPE_JOYSTICK, CONTROLLER_TYPE_GAMEPAD (DEFAULT), CONTROLLER_TYPE_MULTI_AXIS
  bleGamepadConfig.setAxesMin(JOYSTICK_MIN_VALUE); // 0 --> int16_t - 16 bit signed integer - Can be in decimal or hexadecimal
  bleGamepadConfig.setAxesMax(JOYSTICK_MAX_VALUE); // 32767 --> int16_t - 16 bit signed integer - Can be in decimal or hexadecimal 

  //bleGamepadConfig.setWhichSpecialButtons(false, false, false, false, false, false, false, false);
  //bleGamepadConfig.setWhichAxes(false, false, false, false, false, false, false, false);
  bleGamepadConfig.setWhichSimulationControls(false, false, false, true, false); // only brake active 
  bleGamepadConfig.setButtonCount(0);
  bleGamepadConfig.setHatSwitchCount(0);
  bleGamepadConfig.setAutoReport(false);

  bleGamepad.begin(&bleGamepadConfig);*/

  
  //activate bluetooth controller
  BleGamepadConfiguration bleGamepadConfig;
  bleGamepadConfig.setControllerType(CONTROLLER_TYPE_MULTI_AXIS); // CONTROLLER_TYPE_JOYSTICK, CONTROLLER_TYPE_GAMEPAD (DEFAULT), CONTROLLER_TYPE_MULTI_AXIS
  bleGamepadConfig.setAxesMin(JOYSTICK_MIN_VALUE); // 0 --> int16_t - 16 bit signed integer - Can be in decimal or hexadecimal
  bleGamepadConfig.setAxesMax(JOYSTICK_MAX_VALUE); // 32767 --> int16_t - 16 bit signed integer - Can be in decimal or hexadecimal 

  bleGamepadConfig.setAutoReport(false);


  //BleGamepadConfiguration bleGamepadConfig;
  //bleGamepadConfig.setControllerType(CONTROLLER_TYPE_MULTI_AXIS); // CONTROLLER_TYPE_JOYSTICK, CONTROLLER_TYPE_GAMEPAD (DEFAULT), CONTROLLER_TYPE_MULTI_AXIS
  //bleGamepadConfig.setWhichSpecialButtons(false, false, false, false, false, false, false, false);
  //bleGamepadConfig.setWhichAxes(false, false, false, false, false, false, false, false);
  bleGamepadConfig.setWhichSimulationControls(false, false, false, true, false); // only brake active 
  bleGamepadConfig.setButtonCount(0);
  bleGamepadConfig.setHatSwitchCount(0);
  /*bleGamepadConfig.setAutoReport(false);*/

  bleGamepad.begin(&bleGamepadConfig);




  // define endstop switch
  pinMode(minPin, INPUT);
  pinMode(maxPin, INPUT);


  engine.init();
  stepper = engine.stepperConnectToPin(stepPinStepper);
  //stepper = engine.stepperConnectToPin(stepPinStepper, DRIVER_RMT);


  Serial.println("Starting ADC");  
  adc.initSpi(clockMHZ);
  delay(1000);
  Serial.println("ADS: send SDATAC command");
  //adc.sendCommand(ADS1256_CMD_SDATAC);

  
  // start the ADS1256 with data rate of 15kSPS SPS and gain x64
  adc.begin(ADS1256_DRATE_15000SPS,ADS1256_GAIN_64,false); 
  Serial.println("ADC Started");

  adc.waitDRDY(); // wait for DRDY to go low before changing multiplexer register
  adc.setChannel(0,1);   // Set the MUX for differential between ch2 and 3 
  adc.setConversionFactor(conversion);



  Serial.println("ADC: Identify loadcell offset");
  // Due to construction and gravity, the loadcell measures an initial voltage difference.
  // To compensate this difference, the difference is estimated by moving average filter.
  float ival = 0;
  loadcellOffset = 0.0f;
  for (long i = 0; i < NUMBER_OF_SAMPLES_FOR_LOADCELL_OFFFSET_ESTIMATION; i++){
    loadcellReading = adc.readCurrentChannel(); // DOUT arriving here are from MUX AIN0 and 
    ival = loadcellReading / (float)NUMBER_OF_SAMPLES_FOR_LOADCELL_OFFFSET_ESTIMATION;
    //Serial.println(loadcellReading,10);
    loadcellOffset += ival;
  }

  Serial.print("Offset ");
  Serial.println(loadcellOffset,10);



  // automatically identify sensor noise for KF parameterization
  #ifdef ESTIMATE_LOADCELL_VARIANCE
    Serial.println("ADC: Identify loadcell variance");
    float varNormalizer = 1. / (float)(NUMBER_OF_SAMPLES_FOR_LOADCELL_OFFFSET_ESTIMATION - 1);
    varEstimate = 0.0f;
    for (long i = 0; i < NUMBER_OF_SAMPLES_FOR_LOADCELL_OFFFSET_ESTIMATION; i++){
      adc.waitDRDY(); // wait for DRDY to go low before next register read
      loadcellReading = adc.readCurrentChannel(); // DOUT arriving here are from MUX AIN0 and 
      ival = (loadcellReading - loadcellOffset);
      ival *= ival;
      varEstimate += ival * varNormalizer;
      //Serial.println(loadcellReading,10);
    }

    // make sure estimate is nonzero
    if (varEstimate < LOADCELL_VARIANCE_MIN){varEstimate = LOADCELL_VARIANCE_MIN; }
    varEstimate *= 9;
  #else
    varEstimate = 0.2f * 0.2f;
  #endif
  stdEstimate = sqrt(varEstimate);

  Serial.print("stdEstimate:");
  Serial.println(stdEstimate, 6);



  //FastAccelStepper setup
  if (stepper) {

    Serial.println("Setup stepper!");
    stepper->setDirectionPin(dirPinStepper, false);
    stepper->setAutoEnable(true);
    
    //Stepper Parameters
    stepper->setSpeedInHz(MAXIMUM_STEPPER_SPEED);   // steps/s
    stepper->setAcceleration(MAXIMUM_STEPPER_ACCELERATION);  // 100 steps/s²

    stepper->attachToPulseCounter(1, 0, 0);

    delay(5000);
  }





  

  // Find min stepper position
  minEndstopNotTriggered = digitalRead(minPin);
  Serial.print(minEndstopNotTriggered);
  while(minEndstopNotTriggered == true){
    stepper->moveTo(set, true);
    minEndstopNotTriggered = digitalRead(minPin);
    set = set - ENDSTOP_MOVEMENT;
  }
  stepper->forceStopAndNewPosition(0);
  stepper->moveTo(0);
  stepperPosMin_global = (long)stepper->getCurrentPosition();
  stepperPosMin = (long)stepper->getCurrentPosition();

  Serial.println("The limit switch: Min On");
  Serial.print("Min Position is "); 
  Serial.println( stepperPosMin );


  // Find max stepper position
  set = 0;
  maxEndstopNotTriggered = digitalRead(maxPin);
  Serial.print(maxEndstopNotTriggered);
  while(maxEndstopNotTriggered == true){
    stepper->moveTo(set, true);
    maxEndstopNotTriggered = digitalRead(maxPin);
    set = set + ENDSTOP_MOVEMENT;
  } 
  Serial.print(maxEndstopNotTriggered);
  stepperPosMax_global = (long)stepper->getCurrentPosition();
  stepperPosMax = (long)stepper->getCurrentPosition();

  Serial.println("The limit switch: Max On");
  Serial.print("Max Position is "); 
  Serial.println( stepperPosMax );

  

  // correct start and end position as requested from the user
  stepperRange = (stepperPosMax - stepperPosMin);
  stepperPosMin = 0*stepperPosMin + stepperRange * startPosRel;
  stepperPosMax = 0*stepperPosMin + stepperRange * endPosRel;


  // move to initial position
  stepper->moveTo(stepperPosMin, true);
  stepper->clearPulseCounter();
  

  // compute pedal stiffness parameters
  springStiffnesss = (Force_Max-Force_Min) / (float)(stepperPosMax-stepperPosMin);
  springStiffnesssInv = 1.0 / springStiffnesss;

  // obtain current stepper position
  stepperPosPrevious = stepper->getCurrentPosition();

  



    // Kalman filter setup
    // example of evolution matrix. Size is <Nstate,Nstate>
    K.F = {1.0, 0.0,
          0.0, 1.0};
          
    // example of measurement matrix. Size is <Nobs,Nstate>
    K.H = {1.0, 0.0};

    // example of model covariance matrix. Size is <Nstate,Nstate>
    K.Q = {1.0f,   0.0,
            0.0,  1.0};

  // example of measurement covariance matrix. Size is <Nobs,Nobs>
  K.R = {varEstimate};









/*#if defined(SUPPORT_ESP32_PULSE_COUNTER)
  stepper->attachToPulseCounter(1, 0, 0);
#endif*/


  //create a task that will be executed in the Task2code() function, with priority 1 and executed on core 1
  xTaskCreatePinnedToCore(
                    pedalUpdateTask,   /* Task function. */
                    "pedalUpdateTask",     /* name of task. */
                    10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &Task2,      /* Task handle to keep track of created task */
                    0);          /* pin task to core 1 */
    delay(500);

  xTaskCreatePinnedToCore(
                    serialCommunicationTask,   
                    "serialCommunicationTask", 
                    10000,     
                    NULL,      
                    1,         
                    &Task2,    
                    1);     
    delay(500);



  // equalize pedal config for both tasks
  dap_config_st_local = dap_config_st;

  Serial.println("Setup end!");

  previousTime = micros();




}







/**********************************************************************************************/
/*                                                                                            */
/*                         Main function                                                      */
/*                                                                                            */
/**********************************************************************************************/
void loop() {
}


/**********************************************************************************************/
/*                                                                                            */
/*                         pedal update task                                                  */
/*                                                                                            */
/**********************************************************************************************/

long cycleIdx2 = 0;


  //void loop()
  void pedalUpdateTask( void * pvParameters )
  {

    for(;;){
      //delayMicroseconds(100);


  if (configUpdateAvailable == true)
  {
    xSemaphoreTake(semaphore_updateConfig, portMAX_DELAY);
    Serial.println("Update pedal config!");
    configUpdateAvailable = false;
    dap_config_st = dap_config_st_local;
    updateComputationalVariablesFromConfig();
    xSemaphoreGive(semaphore_updateConfig);
  }
  


  // obtain time
  currentTime = micros();
  elapsedTime = currentTime - previousTime;
  if (elapsedTime<1){elapsedTime=1;}
  previousTime = currentTime;


  // if endstop triggered --> recalibrate position
  maxEndstopNotTriggered = digitalRead(maxPin);
  //Serial.println(maxEndstopNotTriggered);
  if (maxEndstopNotTriggered == false)
  {
    Serial.println("Recalibrate due to endstop trigger");
    stepper->forceStopAndNewPosition(stepperPosMax_global);
    stepper->moveTo(stepperPosMax, true);
  }



  if (resetPedalPosition)
  {

    set = 0;//stepperPosMin_global;
    minEndstopNotTriggered = digitalRead(minPin);
    Serial.println(minEndstopNotTriggered);
    while(minEndstopNotTriggered == true){
      stepper->moveTo(set, true);
      minEndstopNotTriggered = digitalRead(minPin);
      set = set - ENDSTOP_MOVEMENT;
      //Serial.println(set);
    }  
    stepper->forceStopAndNewPosition(stepperPosMin_global);

    resetPedalPosition = false;
  }

  

    //#define ABS_OSCILLATION
    #ifdef ABS_OSCILLATION
    
    // compute pedal oscillation, when ABS is active
    if (absActive)
    {
      //Serial.print(2);
      absTime += elapsedTime * 1e-6; 
      absDeltaTimeSinceLastTrigger += elapsedTime * 1e-6; 
      stepperAbsOffset = absAmplitude * sin(absFrequency * absTime);
    }
    
    // reset ABS when trigger is not active anymore
    if (absDeltaTimeSinceLastTrigger > 0.1)
    {
      absTime = 0;
      absActive = false;
    }
    #endif


  //#define COMPUTE_PEDAL_INCLINE_ANGLE
  #ifdef COMPUTE_PEDAL_INCLINE_ANGLE
    float sledPosition = ((float)stepperPosCurrent) / STEPS_PER_MOTOR_REVOLUTION * TRAVEL_PER_ROTATION_IN_MM;
    float pedalInclineAngle = computePedalInclineAngle(sledPosition);

    //Serial.println(pedalInclineAngle);
  #endif
    

  // average execution time averaged over multiple cycles 
  #ifdef PRINT_CYCLETIME
    averageCycleTime += elapsedTime;
    cycleIdx++;
    if (maxCycles< cycleIdx)
    {
      cycleIdx = 0;
      averageCycleTime /= (float)maxCycles; 
      Serial.print("PU cycle time: ");
      Serial.println(averageCycleTime);
      averageCycleTime = 0;
    }
  #endif



    // read ADC value
    adc.waitDRDY(); // wait for DRDY to go low before next register read
    loadcellReading = adc.readCurrentChannel(); // read as voltage according to gain and vref
    loadcellReading -= loadcellOffset;


    // Kalman filter  
    // update state transition and system covariance matrices
    delta_t = (float)elapsedTime / 1000000.0f; // convert to seconds
    delta_t_pow2 = delta_t * delta_t;
    delta_t_pow3 = delta_t_pow2 * delta_t;
    delta_t_pow4 = delta_t_pow2 * delta_t_pow2;

    K.F = {1.0,  delta_t, 
          0.0,  1.0};

    double K_Q_11 = KF_MODEL_NOISE_FORCE_ACCELERATION * 0.5f * delta_t_pow3;
    K.Q = {KF_MODEL_NOISE_FORCE_ACCELERATION * 0.25f * delta_t_pow4,   K_Q_11,
          K_Q_11, KF_MODEL_NOISE_FORCE_ACCELERATION * delta_t_pow2};
          

    // APPLY KALMAN FILTER
    obs(0) = loadcellReading;
    K.update(obs);
    Force_Current_KF = K.x(0,0);
    Force_Current_KF_dt = K.x(0,1);


    // compute target position
    float stepperRange_local = (stepperPosMax - stepperPosMin);
    float posDeltaToTmp = stepperPosCurrent - (stepperPosMin + 0.8 * stepperRange_local) ;

    Position_Next = springStiffnesssInv * (Force_Current_KF-Force_Min) + stepperPosMin ;        //Calculates new position using linear function

    if (posDeltaToTmp > 0) 
    {
      long posCorrention = posDeltaToTmp * 0.95;
      //Serial.print(posDeltaToTmp);
      //Serial.print(", ");
      //Serial.println(posCorrention);
      Position_Next -= posCorrention;
    }
   
    
    //Position_Next -= Force_Current_KF_dt * 0.045f * springStiffnesssInv; // D-gain for stability

    /*cycleIdx2 += 1;
    cycleIdx2 %= JOYSTICK_MAX_VALUE;
    Force_Current_KF = (float)cycleIdx2 / (float)JOYSTICK_MAX_VALUE * Force_Max;*/


  #ifdef ABS_OSCILLATION
    Position_Next += stepperAbsOffset;
  #endif
    if (Position_Next <= stepperPosMin){ Position_Next = stepperPosMin; }
    if (Position_Next >= stepperPosMax){ Position_Next = stepperPosMax; }
    //Position_Next = (int32_t)constrain(Position_Next, stepperPosMin, stepperPosMax);

    


    // get current stepper position
    stepperPosCurrent = stepper->getCurrentPosition();
    /*if (stepperPosCurrent == stepperPosMin)
    {
      Serial.println(stepper->readPulseCounter());
    }*/
    
    //stepperPosCurrent = stepper->getPositionAfterCommandsCompleted();
    long movement = abs( stepperPosCurrent - Position_Next);
    if (movement>MIN_STEPS  )
    {
      stepper->moveTo(Position_Next, false);
    }



    
    float den = (Force_Max-Force_Min);
    if(abs(den)>0.01)
    {     
      int32_t joystickNormalizedToInt32_local = ( Force_Current_KF - Force_Min) / den  * JOYSTICK_MAX_VALUE;
      if(xSemaphoreTake(semaphore_updateJoystick, 1)==pdTRUE)
      {
        joystickNormalizedToInt32 = (int32_t)constrain(joystickNormalizedToInt32_local, JOYSTICK_MIN_VALUE, JOYSTICK_MAX_VALUE);
        xSemaphoreGive(semaphore_updateJoystick);
      }
      
    }
    


  //#define PRINT_DEBUG
  #ifdef PRINT_DEBUG
    //Serial.print("elapsedTime:");
    //Serial.print(elapsedTime);
    Serial.print(",A:");
    Serial.print(loadcellReading,6);
    Serial.print(",B:");
    Serial.print(Force_Current_KF, 6);
    Serial.print(",C:");
    Serial.print(Position_Next, 6);
    Serial.println(" ");
    //delay(100);
  #endif

  

    }
  }

  








/**********************************************************************************************/
/*                                                                                            */
/*                         pedal update task                                                  */
/*                                                                                            */
/**********************************************************************************************/

  unsigned long sc_currentTime = 0;
  unsigned long sc_previousTime = 0;
  unsigned long sc_elapsedTime = 0;
  unsigned long sc_cycleIdx = 0;
  float sc_averageCycleTime = 0;
  int32_t joystickNormalizedToInt32_local = 0;

  void serialCommunicationTask( void * pvParameters )
  {

    for(;;){

      //delayMicroseconds(100);


    // average cycle time averaged over multiple cycles 
    #ifdef PRINT_CYCLETIME

      // obtain time
      sc_currentTime = micros();
      sc_elapsedTime = sc_currentTime - sc_previousTime;
      if (sc_elapsedTime<1){sc_elapsedTime=1;}
      sc_previousTime = sc_currentTime;
      
      sc_averageCycleTime += sc_elapsedTime;
      sc_cycleIdx++;
      if (maxCycles < sc_cycleIdx)
      {
        sc_cycleIdx = 0;
        sc_averageCycleTime /= (float)maxCycles; 
        Serial.print("SC cycle time: ");
        Serial.println(sc_averageCycleTime);
        sc_averageCycleTime = 0;
      }
    #endif





      // read serial input 
      byte n = Serial.available();

      // likely config structure 
      if ( n == sizeof(DAP_config_st) )
      {
        xSemaphoreTake(semaphore_updateConfig, portMAX_DELAY);
        DAP_config_st * dap_config_st_local_ptr;
        dap_config_st_local_ptr = &dap_config_st_local;
        Serial.readBytes((char*)dap_config_st_local_ptr, sizeof(DAP_config_st));

        Serial.println("Config received!");

        // check if data is plausible
        bool structChecker = true;
        if ( dap_config_st_local.payloadType != dap_config_st.payloadType ){ structChecker = false;}
        if ( dap_config_st_local.version != dap_config_st.version ){ structChecker = false;}

        //Serial.print("payloadType: ");
        //Serial.println(dap_config_st_local.payloadType);

        //Serial.print("version: ");
        //Serial.println(dap_config_st_local.version);

        // if checks are successfull, overwrite global configuration struct
        if (structChecker == true)
        {
          configUpdateAvailable = true;          
        }
        xSemaphoreGive(semaphore_updateConfig);
      }
      else
      {
        if (n!=0)
        {
          int menuChoice = Serial.parseInt();
          switch (menuChoice) {
            // resset minimum position
            case 1:
              Serial.println("Reset position!");
              resetPedalPosition = true;
              break;

            // toggle ABS
            case 2:
              //Serial.print("Second case:");
              absActive = true;
              absDeltaTimeSinceLastTrigger = 0;
              break;

            default:
              Serial.println("Default case:");
              break;
          }

        }
      }



      


      // transmit joystick output
      if (bleGamepad.isConnected())
      {
        delay(1);
        if(xSemaphoreTake(semaphore_updateJoystick, 1)==pdTRUE)
        {
          joystickNormalizedToInt32_local = joystickNormalizedToInt32;
          xSemaphoreGive(semaphore_updateJoystick);
        }
        

        //bleGamepad.setBrake(joystickNormalizedToInt32_local);
        bleGamepad.setAxes(joystickNormalizedToInt32_local, 0, 0, 0, 0, 0, 0, 0);
        bleGamepad.sendReport();
        //Serial.println(joystickNormalizedToInt32);   
      }

    }
  }



