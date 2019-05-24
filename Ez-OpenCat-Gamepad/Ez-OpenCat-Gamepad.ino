
#define __DEBUG__
//#define __RF24__

// Baud rate must match the setting of UART modules(HC-05,HM-10 and etc),
// set module to master mode if requires
#define BAUD 38400

#define Console Serial
#define BlueTooth Serial

#ifdef __DEBUG__
#include <SoftwareSerial.h>
#undef BlueTooth
SoftwareSerial BlueTooth(A5, A4);
#endif

#ifdef __RF24__
/*
  library  - https://github.com/tmrh20/RF24/
  tutorial - https://howtomechatronics.com/tutorials/arduino/arduino-wireless-communication-nrf24l01-tutorial/
*/
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#define CE  2   // ce pin
#define CSN 3   // csn pin
const char const address[]  = {0x01, 0xff, 0xff, 0xff, 0xff, 0xff}; // change if needs
RF24 radio(CE, CSN);
#endif

// joystick pins
#define VRX_PIN   A0
#define VRY_PIN   A1
#define SW_PIN    A2

//
#define BEEPER_PIN A3

// pins for matrix switches
#define MATRIX_ROW_START 6 // pin 6 to 9
#define MATRIX_NROW 4
#define MATRIX_COL_START 2 // pin 2 to 5
#define MATRIX_NCOL 4

const char *walks1[] {
  "wkR", "wkL", "wkF", "bk"
};

const char *walks2[] {
  "trR", "trL", "tr", "bk",
};

const char *walks3[] {
  "crR", "crL", "cr", "bk"
};

const char *walks4[] {
  "bkR", "bkL",  "tr", "bd",
};

const char **walkings[] {
  walks1, walks2, walks3, walks4,
};

char **walkptr;

const char *row1[] {
  "rest", "sit", "buttUp", "pee",
};

const char *row2[] {
  "sleep", "pee1", "pu2", "pu1"
};

// disabled
const char *row3[] {
  "vt", nullptr, nullptr, nullptr, "str", "ly", "dropped", "lifted", "stair",
};

char *cmdptr,prev_cmd[24] = "rest";;
boolean is_gait = true;

char **instincts[] {
  walkings[0], row1, row2, row3
};

void setup() {
  Console.begin(BAUD);
  while (!Console); // wait for serial port to connect. Needed for native USB port only
#ifdef __DEBUG__
  Console.println("started!");
  BlueTooth.begin(BAUD);
#endif

  //
  pinMode(SW_PIN, INPUT);
  digitalWrite(SW_PIN, HIGH);
  //
#ifdef __RF24__
  radio.begin();
  radio.openWritingPipe(address);
  radio.setPALevel(RF24_PA_MIN);
  radio.stopListening();
#endif
  tone(BEEPER_PIN, 300);
  delay(100);
  noTone(BEEPER_PIN);

}

void loop() {
  boolean sw1 = false;
  static long cmdPeriod = 0;
  int longClick = 0;
  int joy_x;
  int joy_y;
  int row = -1, col = -1;
  char token = 'k';

  int matrix = scanmatrix(&longClick);
  if (matrix >= 0) {
    row = (matrix) / 4;
    col = matrix % 4;
    if (row == 0) {
      instincts[0] = walkings[col];
      col = 2;
    } else {
      cmdptr = instincts[row][col];
      is_gait = false;
      goto __send;
    }
  }

  joy_x = analogRead(VRY_PIN);
  joy_y = analogRead(VRX_PIN);
  if (joy_x < 300) {
    col = 0;
    is_gait = true;
  } else if (joy_x > 900) {
    col = 1;
    is_gait = true;
  } else if (joy_y < 300) {
    col = 2;
    is_gait = true;
  } else if (joy_y > 900) {
    col = 3;
    is_gait = true;
  } else {
    if (is_gait)
      cmdptr = "balance";
  }
  sw1 = digitalRead(SW_PIN);
  if (!sw1) {
    // disable servo
    token = 'd';
    cmdptr = "";
  }

  if (col >= 0) {
    walkptr = instincts[0];
    cmdptr = walkptr[col];
  }

__send:
  if (millis() >= cmdPeriod && strcmp(cmdptr, prev_cmd) != 0) {
    BlueTooth.write(token);
#ifdef __RF24__
    radio.write(&token, 1);
#endif
    if (token == 'k' && cmdptr != nullptr) {
      BlueTooth.print(cmdptr);
      //BlueTooth.write(cmd, strlen(cmd));
#ifdef __RF24__
      radio.write(cmd, strlen(cmd));
#endif
      strcpy(prev_cmd, cmdptr);
#ifdef __DEBUG__
      Console.write(token);
      Console.println(cmdptr);
#endif
    }
    cmdPeriod = millis() + 50;
  }

}

int scanmatrix(int *longClick) {
  static int priorMatrix = -1;
  static long curMatrixStartTime = 0;

  *longClick = 0;
  // we will energize row lines then read column lines
  // first set all rows to high impedance mode
  for (int row = 0; row < MATRIX_NROW; row++) {
    pinMode(MATRIX_ROW_START + row, INPUT);
  }
  // set all columns to pullup inputs
  for (int col = 0; col < MATRIX_NCOL; col++) {
    pinMode(MATRIX_COL_START + col, INPUT_PULLUP);
  }

  // read each row/column combo until we find one that is active
  for (int row = 0; row < MATRIX_NROW; row++) {
    // set only the row we're looking at output low
    pinMode(MATRIX_ROW_START + row, OUTPUT);
    digitalWrite(MATRIX_ROW_START + row, LOW);

    for (int col = 0; col < MATRIX_NCOL; col++) {
      delayMicroseconds(100);
      if (digitalRead(MATRIX_COL_START + col) != LOW) {
        continue;
      }
      // we found the first pushed button
      int curmatrix = row * MATRIX_NROW + col;
      //
      int clicktime = millis() - curMatrixStartTime;
      if (curmatrix != priorMatrix) {
        curMatrixStartTime = millis();
        priorMatrix = curmatrix;
        *longClick = 0;
      } else if (clicktime > 500) {
        // User has been holding down the same button continuously for a long time
        if (clicktime > 3000) {
          *longClick = 2;
        } else {
          *longClick = 1;
        }

      }
      return curmatrix;
    }
    pinMode(MATRIX_ROW_START + row, INPUT); // set back to high impedance
    //delay(1);
  }

  return -1;
}

