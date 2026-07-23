#include <Arduino.h>
#include <AccelStepper.h>
#include <Wire.h>

// ================= PIN DEFINITIONS =================
#define STEP_PIN1 27
#define DIR_PIN1 26
#define STEP_PIN2 18  
#define DIR_PIN2 32
#define STEP_PIN3 2
#define DIR_PIN3 15
#define STEP_PIN4 19
#define DIR_PIN4 3

#define STEP_PIN5 4
#define DIR_PIN5 5

#define GYRO_POWER 13
#define LED_READY 22
#define LED_FINISHED 12
#define LIMIT_SWITCH 34   // Input only (OK)
#define SOLENOID 33
#define START_BUTTON 25
#define IR_SENSOR_S 14
#define IR_SENSOR_L 39    // Input only (OK)
#define V_LIFE 36         // Input only (OK)

// ================= OBJECTS & QUEUES =================
AccelStepper lf_stepper(AccelStepper::DRIVER, STEP_PIN1, DIR_PIN1);
AccelStepper lr_stepper(AccelStepper::DRIVER, STEP_PIN2, DIR_PIN2);
AccelStepper rf_stepper(AccelStepper::DRIVER, STEP_PIN3, DIR_PIN3);
AccelStepper rr_stepper(AccelStepper::DRIVER, STEP_PIN4, DIR_PIN4);
AccelStepper arm_stepper(AccelStepper::DRIVER, STEP_PIN5, DIR_PIN5);

QueueHandle_t drive_queue;
QueueHandle_t arm_queue;

// ================= ENUMS & GLOBALS =================
enum Command {
    FORWARD = 10,
    BACKWARD = 11,
    DIAGONAL_BACK_LEFT = 13,
    LEFT = 14,
    LEFT_5_DEGREES = 15,
    ACCEL_LEFT = 16,
    ACCEL_FORWARD = 17,
    ACCEL_BACK_LEFT = 18,
    DECEL_LEFT = 19,
    DECEL_FORWARD = 20,
    DECEL_BACK_LEFT = 21,
    STOP = 3
};

// Constants for movement calculations
float wheel_circum = 251.33;
int total_d = 4000;
float one_step_d = wheel_circum / 200.0;
int num_steps = round(total_d / one_step_d);

float threshold = 14.728;

// ================= FUNCTION DECLARATIONS =================
void TaskDrive(void* pvParameters);
void TaskSensors(void* pvParameters);
void TaskArm(void* pvParameters);
void gyro_signals(void);
float IR_Sensor_Signals(int pin_num);

// ================= SETUP =================
void setup() {
    Serial.begin(115200);

    // Configure IO
    pinMode(LED_READY, OUTPUT);
    pinMode(LED_FINISHED, OUTPUT);
    pinMode(SOLENOID, OUTPUT);
    pinMode(START_BUTTON, INPUT_PULLUP);
    pinMode(GYRO_POWER, OUTPUT);

    // Set Stepper Parameters
    lf_stepper.setMaxSpeed(1000);
    lf_stepper.setAcceleration(500);
    lr_stepper.setMaxSpeed(1000);
    lr_stepper.setAcceleration(500);
    rf_stepper.setMaxSpeed(1000);
    rf_stepper.setAcceleration(500);
    rr_stepper.setMaxSpeed(1000);
    rr_stepper.setAcceleration(500);
    arm_stepper.setMaxSpeed(1000);
    arm_stepper.setAcceleration(500);

    // Gyro Initialization
    digitalWrite(GYRO_POWER, HIGH);
    Wire.setClock(400000);
    Wire.begin();
    delay(250);
    Wire.beginTransmission(0x68);
    Wire.write(0x6B);
    Wire.write(0x00);
    Wire.endTransmission();

    // Create Queues
    drive_queue = xQueueCreate(10, sizeof(Command));
    arm_queue = xQueueCreate(1, sizeof(int));

    // Create Tasks
    xTaskCreatePinnedToCore(TaskDrive, "Drive Task", 2000, NULL, 2, NULL, 0);

    // Sensor and Arm tasks
    xTaskCreatePinnedToCore(TaskSensors, "Sensor Task", 2000, NULL, 3, NULL, 1);
    xTaskCreatePinnedToCore(TaskArm, "Arm Task", 1000, NULL, 1, NULL, 1);

    // Indicate ready
    digitalWrite(LED_READY, HIGH);
    Serial.println("System Ready. Waiting for START button.");
}

void loop() {
    // FreeRTOS handles all execution. Loop does nothing.
    vTaskDelay(pdMS_TO_TICKS(1000));
}

// ================= TASK: SENSORS (STATE MACHINE) =================
void TaskSensors(void* pvParameters) {
    Command cmd = STOP;
    float check = 0.0;

    // Wait for Start Button Press
    while (digitalRead(START_BUTTON) == HIGH) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    Serial.println("Started!");
    digitalWrite(LED_READY, LOW);

    // Trigger Arm
    int arm_trigger = 1;
    xQueueSend(arm_queue, &arm_trigger, portMAX_DELAY);

    // 1. Move forwards to the edge
    cmd = FORWARD;
    while (check < threshold) {
        check = IR_Sensor_Signals(IR_SENSOR_L);
        xQueueSend(drive_queue, &cmd, portMAX_DELAY); // Keep sending forward command
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    // 2. Go back and sideways 5° then straight sideways
    cmd = LEFT_5_DEGREES;
    xQueueSend(drive_queue, &cmd, portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(200));

    cmd = LEFT;
    for (int count = 0; count < 51; count++) {
        check = IR_Sensor_Signals(IR_SENSOR_L);
        gyro_signals();

        // Update command depending on sensor status
        if (check >= threshold || (check > 4 && check < 30)) {
            cmd = LEFT_5_DEGREES;
        }
        else {
            cmd = LEFT;
        }

        xQueueSend(drive_queue, &cmd, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // 3. Go backwards diagonal
    cmd = DIAGONAL_BACK_LEFT;
    xQueueSend(drive_queue, &cmd, portMAX_DELAY);

    // Continue until limit switch hits or sensor reads finished
    while (digitalRead(LIMIT_SWITCH) != HIGH && check < threshold) {
        check = IR_Sensor_Signals(IR_SENSOR_S);
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    // 4. Stop robot
    cmd = STOP;
    xQueueSend(drive_queue, &cmd, portMAX_DELAY);

    // 5. Drop off balls and signal finished
    digitalWrite(SOLENOID, HIGH);
    digitalWrite(LED_FINISHED, HIGH);
    Serial.println("Sequence Complete.");

    vTaskDelete(NULL);
}

// ================= TASK: DRIVE (MECANUM KINEMATICS) =================
void TaskDrive(void* pvParameters) {
    Command current_cmd = STOP;

    while (1) {

        if (xQueueReceive(drive_queue, &current_cmd, portMAX_DELAY) == pdPASS) {

            switch (current_cmd) {
            case FORWARD:

                lf_stepper.move(1);
                lr_stepper.move(-1);
                rf_stepper.move(-1);
                rr_stepper.move(1);
                break;

            case BACKWARD:
                lf_stepper.move(-1);
                lr_stepper.move(1);
                rf_stepper.move(1);
                rr_stepper.move(-1);
                break;

            case DIAGONAL_BACK_LEFT:

                lf_stepper.stop();
                lr_stepper.move(-1);
                rf_stepper.stop();
                rr_stepper.move(-1);
                break;

            case LEFT:
                lf_stepper.move(1);
                lr_stepper.move(1);
                rf_stepper.move(-1);
                rr_stepper.move(-1);
                break;

            case LEFT_5_DEGREES:
  
                lf_stepper.stop();
                lr_stepper.stop();
                rf_stepper.stop();
                rr_stepper.stop();
                break;

            case STOP:
            default:
                lf_stepper.stop();
                lr_stepper.stop();
                rf_stepper.stop();
                rr_stepper.stop();
                break;
            }
        }

        // CRITICAL FIX: AccelStepper requires .run() to be called continuously 
        // to actually step the motors. Without this, they will never move.
        lf_stepper.run();
        lr_stepper.run();
        rf_stepper.run();
        rr_stepper.run();

        // Yield to the RTOS scheduler
        vTaskDelay(1);
    }
}

// ================= TASK: ARM =================
void TaskArm(void* pvParameters) {
    int arm_start = 0;

    // Block indefinitely until the sensor task sends the trigger
    xQueueReceive(arm_queue, &arm_start, portMAX_DELAY);

    if (arm_start == 1) {
        Serial.println("Arm Activating...");
        // Lift up 90° 
        arm_stepper.moveTo(arm_stepper.currentPosition() - 50);

        // Must run the arm stepper until it reaches its target
        while (arm_stepper.distanceToGo() != 0) {
            arm_stepper.run();
            vTaskDelay(1);
        }
        Serial.println("Arm in position.");
    }

    // Task is done, delete it to free memory
    vTaskDelete(NULL);
}

// ================= HELPER FUNCTIONS =================

void gyro_signals(void) {
    Wire.beginTransmission(0x68);
    Wire.write(0x1A);
    Wire.write(0x05);
    Wire.endTransmission();
    Wire.beginTransmission(0x68);
    Wire.write(0x1B);
    Wire.write(0x8);
    Wire.endTransmission();
    Wire.beginTransmission(0x68);
    Wire.write(0x43);
    Wire.endTransmission();
    Wire.requestFrom(0x68, 6);
    int16_t GyroX = Wire.read() << 8 | Wire.read();
    int16_t GyroY = Wire.read() << 8 | Wire.read();
    int16_t GyroZ = Wire.read() << 8 | Wire.read();
    // Note: Variables RateRoll/Pitch/Yaw were removed from globals 
    // to save memory since they weren't being used to control anything yet.
}

float IR_Sensor_Signals(int pin_num) {
    // FIX: Removed delay(100) and Serial.print() from here. 
    // Printing and delaying inside a high-priority sensor loop destroys RTOS timing.
    // Only do debug prints if strictly necessary, preferably via a separate debug task.

    float volts = analogRead(pin_num) * (3.3 / 4096.0);
    float distance = 9.5 * pow(volts, -0.7225);

    // Cap the return value to prevent infinite loops if sensor disconnects
    if (distance <= 30 && distance > 0) {
        return distance;
    }
    return 999.0; // Return an out-of-bounds value indicating sensor error
}