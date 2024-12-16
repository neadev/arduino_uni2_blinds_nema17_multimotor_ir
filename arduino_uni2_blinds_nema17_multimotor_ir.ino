// библиотека ИК приемника
#include "IRremote.hpp"
// Подключаем библиотеку AccelStepper
#include <AccelStepper.h>

#include <EEPROM.h>

bool debugConsole = true;

// количество шагов за 1 нажатие кнопки
byte stepsPerClick = 112;

// 0 - все моторы
byte activeMotor = 0;

// 1 общий enable pin для всех моторов
byte enablePin = 2;

// флаг движение без остановки или по шагам
bool moveBySteps = true;

// ПИН ИК-приемника
#define IR_RECEIVE_PIN A5

// количество моторов
const byte motorAmount = 5;


// Параметры моторов
//старые:
// 0 dirPin, 1 stepPin, 2 enabledPin, 3 moving(0,1), 4 direction(1 вверх,-1 вниз), 5 speed, 6 stopPin(пин концевика), 7 topPosition, 

// также понадобятся: верхняя позиция по-умолчанию defaultTopPosition
// большая штора верхнее положение 75000 на 2-х контактах делителя на схеме
long motorsParams[motorAmount][8] = {
  {16, 17, 2, 0, 1, 700, 10, 8000},
  {4, 3, 2, 0, 1, 700, 10, 8000},
  {7, 6, 2, 0, 1, 700, 10, 8000},
  {10, 9, 2, 0, 1, 700, 10, 8000},
  {13, 12, 2, 0, 1, 700, 10, 8000},
};

AccelStepper motors[] = { 
  {1, motorsParams[0][1], motorsParams[0][0]}, 
  {1, motorsParams[1][1], motorsParams[1][0]},
  {1, motorsParams[2][1], motorsParams[2][0]},
  {1, motorsParams[3][1], motorsParams[3][0]},
  {1, motorsParams[4][1], motorsParams[4][0]},
};

// определяем коды команд ИК приемника
// подъем без остановки
// кнопка Вверх
byte moveUpCmd   = 22;
// опускание без остановки
// кнопка Вниз
byte moveDownCmd = 26;
// подъем вверх на заданное количество шагов stepsPerClick
// кнопка влево
byte moveUpByStepCmd   = 81;
// опускание вниз на заданное количество шагов stepsPerClick
// кнопка вправо
byte moveDownByStepCmd = 80;
// команда остановить двигатель - OK
byte stopCmd = 19;
// запомнить верхнюю позицию
// Кнопка VOL+
byte setTopPositionCmd = 24;
// запомнить нижнюю позицию
// Кнопка VOL-
byte setHomePositionCmd = 16;
// кнопка APP
// отображает текущую позицию мотора + все параметры массива motor1...n
byte showMotorPositionsCmd = 15;
// установка активными моторами все моторы
// кнопка Home
byte setActiveMotorAllCmd = 17;
// установка активным мотором №1
byte setActiveMotor1Cmd = 78;
// установка активным мотором №2
byte setActiveMotor2Cmd = 13;
// установка активным мотором №3
byte setActiveMotor3Cmd = 12;
// установка активным мотором №4
byte setActiveMotor4Cmd = 74;
// установка активным мотором №5
byte setActiveMotor5Cmd = 9;
// установка активным мотором №6
byte setActiveMotor6Cmd = 8;

// таймер нахождения моторов без движения
long saveTimer = 0;

// массив с последними позициями двигателей
// 6 двигателей(0 1 2 3 4 5) + счетчик записей EEPROM(6 или последняя ячейка)
long currentPositions[motorAmount+1];

// Serial.println wrapper
// for debug usage, in final compilation commented
void consoleLog(String str, bool ignoreDebug=false) {
  if(debugConsole || ignoreDebug) {
    Serial.println(str);
  }
}


void setup() {

  Serial.begin(9600);
  consoleLog(F("serial init"));

  /*
  for(int y=0; y<=30; y++) {
    //EEPROM.update(y, 255);
    consoleLog( "EEPROM.read("+String(y)+") = " + String(EEPROM[y]) );
  }
  */
  

  // запись байта по адресу
  //EEPROM.update(50, 177);
  // чтение байта по адресу
  //consoleLog( "EEPROM.read(1050) = " + String(EEPROM[1050]) );


  // читаем из памяти массив с последней позицией для каждого мотора
  EEPROM.get(0, currentPositions);

  for(int positionNum = 0; positionNum <= motorAmount; positionNum++) {
    currentPositions[positionNum] = currentPositions[positionNum] < 0 ? 0 : currentPositions[positionNum];

    consoleLog("position from memory["+String(positionNum)+"] = " + String(currentPositions[positionNum]));
  }


  //отключаем питание на двигателях изначально
  // перебираем все классы и устанавливаем для каждого двигателя enabledPin в HIGH

  // устанавливаем общий для всех enable pin в HIGH, т.е. движки отключены
  pinMode(enablePin, OUTPUT);
  digitalWrite(enablePin, HIGH);

  // Устанавливаем максимальную скорость, коэффициент ускорения,
  // начальную скорость 
  // для каждого двигателя
  for (int i = 0; i < motorAmount; i++) {
    /*
    // устанавливаем enabledPin в HIGH)
    // у всех движков 
    digitalWrite(motorsParams[i][2], HIGH);
    */

    // устанавливаем скорость и макс.скорость
    // setSpeed(direction 1\-1 * motorSpeed)
    motors[i].setMaxSpeed(1000);
    motors[i].setSpeed(motorsParams[i][4]*motorsParams[i][5]);

    // устанавливаем текущее положение из памяти
    motors[i].setCurrentPosition(currentPositions[i]==-1 ? 0 : currentPositions[i]);

    //consoleLog("current position setup motor["+String(i)+"] = "+String(motors[i].currentPosition()));

  }

  // запускаем ИК приемник
  IrReceiver.begin(IR_RECEIVE_PIN);  
  consoleLog(F("IR Init"));

}

void loop() {

  // Читаем ИК приемник
  int irCmd = 0;
  if (IrReceiver.decode()) {
    
    // отладка ИК приемника
    //consoleLog(F("IrReceiver.decode()IrReceiver.decode():"));
    //IrReceiver.printIRResultShort(&Serial);

    irCmd = IrReceiver.decodedIRData.command;
    //consoleLog("IR command = " + String(irCmd) + "\n");

    IrReceiver.resume();
  }

  String consoleCmd = "0";

  if (Serial.available()) {
    consoleCmd = Serial.readString();
    consoleCmd.trim();
  }

  // если получена команда из консоли или с пульта
  if(consoleCmd != "0" || irCmd > 0) {

    //consoleLog("comand = " + consoleCmd + " (" + String(consoleCmd.length()) + ")");
    //consoleLog("consoleCmd.substring(0, 8) = " + consoleCmd.substring(0, 8));
    

    /*
      Консольные команды
      up
      down
      stepup
      stepdown
      stop
      sethome
      settop
      showpositions
      active1...6...all
      setspeed
    */

    // поднимаем вверх на позицию topPosition, иначе говоря 100% открытие окна
    if(consoleCmd == "up" || irCmd == moveUpCmd) {
      consoleLog(F("up"));

      moveBySteps = false;
      moveMotors2Position(1, 100);
    }
    // опускаем вниз на позицию 0 (home), или 0% открытие окна
    else if ( consoleCmd == "down" || irCmd == moveDownCmd) {
      consoleLog(F("down"));

      moveBySteps = false;
      moveMotors2Position(-1, 0);
    }
    // опускаем вниз на заданное количество шагов (работает пока нажата кнопка)
    else if ( irCmd == moveDownByStepCmd || consoleCmd == "stepdown") {
      consoleLog(F("move 1 step down"));

      moveBySteps = true;
      moveMotors2Position(-1, -1);
    }
    // поднимаем вверх на заданное количество шагов (работает пока нажата кнопка)
    else if ( irCmd == moveUpByStepCmd || consoleCmd == "stepup") {
      consoleLog(F("move 1 step up"));

      moveBySteps = true;
      moveMotors2Position(1, -1);
    }
    // устанавливаем текущую позицию мотора в 0 (home), соответствует низу шторы
    else if ( irCmd == setHomePositionCmd || consoleCmd == "sethome") {
      consoleLog(F("set home position currentPosition=0"));

      setHomePosition();
    }
    // записываем верхнюю позицию мотора, соответствует верху поднятой шторы
    else if ( irCmd == setTopPositionCmd || consoleCmd == "settop") {
      consoleLog(F("set top position"));

      setTopPosition();
    }
    // останавливаем и выключаем все моторы
    else if ( consoleCmd == "stop" || irCmd == stopCmd) {
      consoleLog(F("stop"));
      stopMotor(-1);
    }
    // устанавливаем активным мотор №1
    else if ( consoleCmd == "active1" || irCmd == setActiveMotor1Cmd) {
      consoleLog(F("active motor = 1"));
      activeMotor = 1;
    }
    // устанавливаем активным мотор №2
    else if ( consoleCmd == "active2" || irCmd == setActiveMotor2Cmd) {
      consoleLog(F("active motor = 2"));
      activeMotor = 2;
    }
    // устанавливаем активным мотор №3
    else if ( consoleCmd == "active3" || irCmd == setActiveMotor3Cmd) {
      consoleLog(F("active motor = 3"));
      activeMotor = 3;
    }
    // устанавливаем активным мотор №4
    else if ( consoleCmd == "active4" || irCmd == setActiveMotor4Cmd) {
      consoleLog(F("active motor = 1"));
      activeMotor = 1;
    }
    // устанавливаем активным мотор №5
    else if ( consoleCmd == "active5" || irCmd == setActiveMotor5Cmd) {
      consoleLog(F("active motor = 4"));
      activeMotor = 4;
    }
    // устанавливаем активным мотор №6
    else if ( consoleCmd == "active6" || irCmd == setActiveMotor6Cmd) {
      consoleLog(F("active motor = 5"));
      activeMotor = 5;
    }
    // отображаем текущую позицию мотора + все его параметры из массива
    else if ( consoleCmd == "showpositions" || irCmd == showMotorPositionsCmd) {
      consoleLog(F("showMotorPositionConsole():"), true);
      showMotorPositionConsole();
    }
    // устанавливаем все моторы активными
    else if ( consoleCmd == "activeall" || irCmd == setActiveMotorAllCmd) {
      consoleLog(F("active motor all"));
      activeMotor = 0;
    }
    // стираем всю постоянную память.
    else if( consoleCmd == "format" ) {
      formatEEPROM();
    }
    // читаем значения из памяти
    else if( consoleCmd == "readmem" ) {
      readMemory();
    }
    // устанавливаем скорость через консоль
    else if ( consoleCmd.substring(0, 8) == "setspeed" ) {
      int speed2set = consoleCmd.substring(8).toInt();

      consoleLog("set speed = "+ String( speed2set ));

      setSpeed(speed2set);
    }
  }

  //крутим активные моторы
  moveMotor();

  // сохраняем текущие позиции моторов
  saveCurrentPositions();
}

// отображаем текущую позицию мотора + все его параметры из массива
void showMotorPositionConsole() {

  int fromMotor = 0;
  int toMotor = motorAmount;

  if(activeMotor > 0) {
    fromMotor = activeMotor - 1;
    toMotor = activeMotor;
  }
  
  // перебираем все моторы
  for (int i = fromMotor; i < toMotor; i++) {
    consoleLog("\tMotor["+String(i)+"].currentPosition() = "+String(motors[i].currentPosition()), true);
    consoleLog("\tMotor["+String(i)+"][topPosition] = "+String(motorsParams[i][7]), true);
    consoleLog("\tMotor["+String(i)+"].distanceToGo() = "+String(motors[i].distanceToGo()), true);

    consoleLog(F("\t----------------------------------------------------------"), true);
  }

}

// устанавливаем нулевую, нижнюю позицию положения шторы (закрыто)
void setHomePosition() {

  int fromMotor = 0;
  int toMotor = motorAmount;

  if(activeMotor > 0) {
    fromMotor = activeMotor - 1;
    toMotor = activeMotor;
  }
  // перебираем все моторы
  for (int i = fromMotor; i < toMotor; i++) {
    motors[i].setCurrentPosition(0);
  }
}

// записываем позицию верхнего положения шторы (открыто)
void setTopPosition() {

  int fromMotor = 0;
  int toMotor = motorAmount;

  if(activeMotor > 0) {
    fromMotor = activeMotor - 1;
    toMotor = activeMotor;
  }
  // перебираем все моторы
  for (int i = fromMotor; i < toMotor; i++) {
    motorsParams[i][7] = motors[i].currentPosition();
  }
}

// передвигаем мотор на заданную позицию
// targetPositionPercent позиция в процентах, 0 - 0% закрыто, 100 - 100% открыто
// если целевая позиция targetPositionPercent = -1, то двигаем на дефолтное количество шагов stepsPerClick 
// величина шага в переменной глобальной stepsPerClick
void moveMotors2Position(int direction, int targetPositionPercent) {

  setDirection(direction);


  // варианты:
  // Если targetPositionPercent = -1
  // 1. Двигаем по шагам, пока нажата кнопка в соответствии с направлением direction

  // Если targetPositionPercent = заданное кол-во процентов%
  // 2. Двигаем на заданное количество процентов открытия, вычисляется по позиции motorsParams[][7]topPosition
  // 3. Если motorsParams[][7]topPosition не задано, то двигаем по шагам, п.1

  // Если targetPositionPercent = 0%
  // 4. Если двигаем вниз, всегда всегда двигаем до позиции 0 .moveTo(0);

  int fromMotor = 0;
  int toMotor = motorAmount;

  if(activeMotor > 0) {
    fromMotor = activeMotor - 1;
    toMotor = activeMotor;
  }
  // перебираем все моторы
  for (int i = fromMotor; i < toMotor; i++) {

    long targetPosition = 0;

    // если задано количество процентов% открытия шторы
    if(targetPositionPercent > -1) {

      targetPosition = (targetPositionPercent/100) * motorsParams[i][7];

      // если текущая позиция больше целевой позиции и движение идет вниз
      // или текущая позиция меньше целевой и движение идет вверх
      if( (motors[i].currentPosition() > targetPosition && direction == -1 ) || ( motors[i].currentPosition() < targetPosition && direction == 1 )) {
        motors[i].moveTo(targetPosition);
      }
      // иначе остаемся на текущей позиции
      else {
        motors[i].moveTo(motors[i].currentPosition());
      }

      /*
      consoleLog("direction = "+String(direction));
      consoleLog("targetPosition = "+String(targetPosition));
      consoleLog("currentPosition = "+String(motors[i].currentPosition()));
      consoleLog("distanceToGo = "+String(motors[i].distanceToGo()));
      */
    }

    // если задано движение по шагам
    if(targetPositionPercent == -1) {
      motors[i].moveTo(motors[i].currentPosition()+direction*stepsPerClick);
    }

  }
}

// устанавливаем направление для выбранного двигателя или всех сразу
void setDirection(int direction) {
  int fromMotor = 0;
  int toMotor = motorAmount;

  if(activeMotor > 0) {
    fromMotor = activeMotor - 1;
    toMotor = activeMotor;
  }
  // перебираем все моторы
  for (int i = fromMotor; i < toMotor; i++) {
    motorsParams[i][4] = direction;
    motorsParams[i][3] = 1;
    // активируем питание обмоток
    digitalWrite(motorsParams[i][2], LOW);
  }
}

// двигаем все моторы, на которых есть питание
// ВОТ ТУТ ПРОВЕРИТЬ ИЗМЕНЯЕТСЯ ЛИ ПОЗИЦИЯ, ЕСЛИ МОТОР ОБЕСТОЧЕН, НО ОДИН ХУЙ ЕГО ДЕРГАЮТ motor.run();
void moveMotor() {

  // перебираем все моторы, проверяем, если активны, то крутим
  for (int i = 0; i < motorAmount; i++) {
    if(motorsParams[i][3] == 1) {
      motors[i].setSpeed(motorsParams[i][4]*motorsParams[i][5]);

      //if(( ((motors[i].currentPosition() > 0 && motorsParams[i][4] == -1 ) || (motors[i].currentPosition() <= motorsParams[i][7] && motorsParams[i][4] == 1 )) && !moveBySteps ) || (moveBySteps && motors[i].distanceToGo() != 0 )) {
      if( motors[i].distanceToGo() != 0 ) {
        // активируем все моторы общим enable pin
        digitalWrite(enablePin, LOW);

        motors[i].run();

        // сохраняем время последнего движения
        saveTimer = millis();
      }
      else {
        //consoleLog("\stopMotor(), i = "+String(i));
        stopMotor(i);
      }
    }
  }
}

// останавливаем (обесточиваем) все моторы или только активный мотор
void stopMotor(int motorNum) {

  int fromMotor = 0;
  int toMotor = motorAmount;

  if(activeMotor > 0) {
    fromMotor = activeMotor - 1;
    toMotor = activeMotor;
  }
  // перебираем все моторы
  for (int i = fromMotor; i < toMotor; i++) {
    if( i == motorNum || motorNum == -1 ) {

      /*
      consoleLog("\motorNum param = "+String(motorNum));
      consoleLog("\tMotor["+String(i)+"].currentPosition() = "+String(motors[i].currentPosition()));
      consoleLog("\tMotor["+String(i)+"].distanceToGo() = "+String(motors[i].distanceToGo()));
      consoleLog("\tMotor["+String(i)+"][topPosition] = "+String(motorsParams[i][7]));

      consoleLog(F("\t----------------------------------------------------------"));
      */

      motorsParams[i][3] = 0;
      //digitalWrite(motorsParams[i][2], HIGH);
    }
  }
}

// устанавливаем скорость для выбранного двигателя или всех сразу
void setSpeed(int speed) {

  int fromMotor = 0;
  int toMotor = motorAmount;

  if(activeMotor > 0) {
    fromMotor = activeMotor - 1;
    toMotor = activeMotor;
  }
  // перебираем все моторы
  for (int i = fromMotor; i < toMotor; i++) {
    motorsParams[i][5] = speed;
  }
}

// сохраняем текущие позиции моторов в EEPROM
void saveCurrentPositions() {

  // задержка 5000мс(5сек) после последнего движения
  long diff_countdown = saveTimer + 5000 - millis();


  if( diff_countdown <= 0 && saveTimer > 0 ) {

    //consoleLog("diff_countdown = " + String(diff_countdown));
    //consoleLog("saveTimer = " + String(saveTimer));

    consoleLog(F("saving current positions after 5sec without moving"));
    
    // сбрасываем таймер
    saveTimer = 0;

    // перебираем все моторы, складывает значения текущих позиция в массив для записи
    for (int i = 0; i < motorAmount; i++) {
      currentPositions[i] = motors[i].currentPosition();
    }

    // увеличиваем счетчик сохранений
    currentPositions[motorAmount] += 1;
    
    
    consoleLog(F("Positions to save:"));
    for(int i = 0; i < motorAmount; i++) {
      consoleLog("motor["+String(i)+"] position to save = "+String(currentPositions[i]));
    }
    consoleLog("save counter = "+String(currentPositions[motorAmount]));
    

    // записываем значение в EEPROM
    EEPROM.put(0, currentPositions);

    // обесточиваем все моторы общим enable pin = HIGH
    digitalWrite(enablePin, HIGH);
  }

  /*
  else {
    //consoleLog("saveTimer = " + String(saveTimer));
    //consoleLog("millis() = " + String(millis()));
    if(saveTimer > 0) {
      consoleLog("wating for move timeOut..." + String(diff_countdown));
    }
  }
  */

}

// стираем все ячейки постоянной памяти
void formatEEPROM() {

  //consoleLog(F("Formating EEPROM memory..."));

  for(int i = 0; i < EEPROM.length(); i++) {

    if(EEPROM[i] != 255) {

      EEPROM.update(i, 255);

      //consoleLog("erasing addr " + String(i));

    }

  }

  //consoleLog(F("Formating EEPROM memory FINISHED"));
}


// читаем и выводим данные из памяти
void readMemory() {

  // читаем из памяти массив с последней позицией для каждого мотора
  EEPROM.get(0, currentPositions);

  for(int positionNum = 0; positionNum <= motorAmount; positionNum++) {
    currentPositions[positionNum] = currentPositions[positionNum] < 0 ? 0 : currentPositions[positionNum];

    consoleLog("position from memory["+String(positionNum)+"] = " + String(currentPositions[positionNum]), true);
  }

}