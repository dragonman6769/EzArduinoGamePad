//#define __DEBUG__
//#define __CC2540_MASTER__

// baud rate must match the setting of UART modules(HC-05,BT-05,HM-10 and etc)
#define BAUD 115200
//#define BAUD 38400

#define Console Serial
#ifdef __DEBUG__
#include <SoftwareSerial.h>
SoftwareSerial BlueTooth(A5, A4);
#else
#define BlueTooth Serial
#endif

//
#define DEFAULTPACKLENGTH  10
//
#define BUTTON_PRESSED_INDEX 3
#define BUTTON_ID_INDEX 5
#define JOY_X_INDEX 7
#define JOY_Y_INDEX 6
#define CHECK_SUM_INDEX 10
//
// pins for matrix switches
#define MATRIX_ROW_START 6 // pin 6 to 9
#define MATRIX_NROW 4
#define MATRIX_COL_START 2 // pin 2 to 5
#define MATRIX_NCOL 4

// joystick
#define VRX_PIN   A0
#define VRY_PIN   A1
#define SW_PIN    A2
//
#define BEEPER_PIN A3

void setup() {
  Console.begin(BAUD);
  while (!Console); // wait for serial port to connect. Needed for native USB port only
#ifdef __DEBUG__
  Console.println("started!");
#endif

  //
  pinMode(SW_PIN, INPUT);
  digitalWrite(SW_PIN, HIGH);
  //
#ifdef __DEBUG__
  BlueTooth.begin(BAUD);
#endif
  //
#ifdef __CC2540_MASTER__
  bleConnect();
#endif
  tone(BEEPER_PIN, 300);
  delay(100);
  noTone(BEEPER_PIN);
}

void loop() {
  boolean sw1ButtonState = false;
  static boolean sw1PrevButtonState = true;
  static long cmdPeriod = 0;
  int row = -1, col = -1;
  int longClick = 0;
  int joy_x;
  int joy_y;
  byte packet[DEFAULTPACKLENGTH] = {
    0x55, 0xAA, // header
    0x11, // address, unused.
    0x00, // is a button pressed?
    0x00, // unused
    0x00, // button id
    128,  // joystick X
    128,  // joystick Y
    0, 0 // unused
  };

#ifdef __CC2540_MASTER__
  // reconstruct packet due to somehow it is corrupted after calling bleConnect(),
  packet[0] = 0x55;
  packet[1] = 0xAA;
  packet[2] = 0x11;
  packet[3] = 0x00;
  packet[4] = 0x00;
#endif

  int key = scanmatrix(&longClick);
  if (key >= 0) {
    packet[BUTTON_PRESSED_INDEX] = 1;
    switch (key) {
      case 0:
      case 4:
      case 8: packet[BUTTON_ID_INDEX] = 1;
        break;
      case 1:
      case 5:
      case 9: packet[BUTTON_ID_INDEX] = 2;
        break;
      case 2:
      case 6:
      case 10: packet[BUTTON_ID_INDEX] = 3;
        break ;
      case 3:
      case 7:
      case 11: packet[BUTTON_ID_INDEX] = 4;
        break;
      case 12: packet[BUTTON_ID_INDEX] = 5;
        break;
      case 13: packet[BUTTON_ID_INDEX] = 6;
        break;
    }
  }
  joy_x = map(analogRead(VRY_PIN), 0, 1023, 255, 0);
  joy_y = map(analogRead(VRX_PIN), 0, 1023, 255, 0);
  packet[JOY_X_INDEX] = abs(joy_x - 128) < 10 ? 128 : joy_x;
  packet[JOY_Y_INDEX] = abs(joy_y - 128) < 10 ? 128 : joy_y;
  sw1ButtonState = digitalRead(SW_PIN);
  if (!sw1ButtonState) {
    packet[BUTTON_ID_INDEX] = 6;
    packet[BUTTON_PRESSED_INDEX] = 1;
  }
  //  if (sw1ButtonState != sw1PrevButtonState) {
  //    if (!sw1ButtonState) {
  //      packet[BUTTON_ID_INDEX] = 5;
  //      packet[BUTTON_PRESSED_INDEX] = 1;
  //      cmdPeriod=0;
  //    }
  //    sw1PrevButtonState = sw1ButtonState;
  //  }
  if (millis() >= cmdPeriod) {
    sendPacket(packet);
    cmdPeriod = millis() + 200;
  }
}

void sendPacket(byte *packet) {
  unsigned int checksum = 0;
  static byte prev_packet[DEFAULTPACKLENGTH];

  if (memcmp(packet, prev_packet, DEFAULTPACKLENGTH) == 0) {
    return;
  }
  memcpy(prev_packet, packet, DEFAULTPACKLENGTH);
  for (int i = 0; i < DEFAULTPACKLENGTH; i++) {
    if (i == BUTTON_PRESSED_INDEX && packet[i] == 0) continue;
    BlueTooth.write(packet[i]);
    checksum += packet[i];
  }
  checksum = (checksum % 256);
  BlueTooth.write(checksum);
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
  }

  return -1;
}

#ifdef __CC2540_MASTER__
// for CC2540 BLE to UART modules such as BT05,HM-10, in master mode
boolean bleConnect() {
  char buf[32];
  int c = 0;
  bool connected = false;
  long timeout = millis() + 6000;
  long beepTimeout = millis() + 1000;

#ifdef __DEBUG__
  Console.println("Init BLE data mode...");
#endif
  BlueTooth.print("AT+INQ\r\n");
  while (!connected) {
    if (millis() >= beepTimeout) {
      beepTimeout = millis() + 1000;
      Console.print(".");
    }
    if (millis() >= timeout) {
      break;
    }
    if (!BlueTooth.available()) continue;
    int ch = BlueTooth.read();
    if (ch == 0x0d) continue;
    if (ch == 0x0a) { // NL char, recevied end of string
      buf[c] = 0; c = 0;
#ifdef __DEBUG__
      Console.println(buf);
#endif
      if (strcmp(buf, "+INQE") == 0) {
        BlueTooth.print("AT+CONN1\r\n");
      } else if (strcmp(buf, "+Connected") == 0) {
        connected = true;
      }
    } else
      buf[c++] = ch;
  }
  return connected;
}
#endif
