
/*
   Testing the correctness of wiring of 4x4 matrix switches and joystick

*/

// changes the baud rate to match the Serial Monitor setting
//#define BAUD 115200
#define BAUD 38400

#define Console Serial

// pins for matrix switches
#define MATRIX_ROW_START 6 // pin 6 to 9
#define MATRIX_NROW 4
#define MATRIX_COL_START 2 // pin 2 to 5
#define MATRIX_NCOL 4

// joystick
#define VRX_PIN   A0
#define VRY_PIN   A1
#define SW_PIN    A2

void setup() {
  Console.begin(BAUD);

  while (!Console); // wait for serial port to connect. Needed for native USB port only
  pinMode(SW_PIN, INPUT);
  digitalWrite(SW_PIN, HIGH);

  Console.println("started!");
}

void loop() {
  boolean sw1ButtonState = false;
  static boolean sw1PrevButtonState = true;
  static long outputPeriod = 0;
  int longClick = 0;

  int key = scanmatrix(&longClick);
  int joy_x = map(analogRead(VRY_PIN), 0, 1023, 255, 0);
  int joy_y = map(analogRead(VRX_PIN), 0, 1023, 255, 0);

  sw1ButtonState = digitalRead(SW_PIN);


  if (millis() >= outputPeriod) {
    if (key >= 0) {
      Console.print("Key [" );
      Console.print(key);
      Console.print("] pressed, ");
    } else {
      Console.print("No key pressed, ");
    }
    if (!sw1ButtonState) {
      Console.print("button pressed");
    } else {
      Console.print("button not pressed");
    }
    Console.print(", Joystick X=");
    Console.print(joy_x);
    Console.print(", Y=");
    Console.println(joy_y);

    outputPeriod = millis() + 500;
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
  }

  return -1;
}

