#include <LiquidCrystal.h>
#include <SoftwareSerialWithHalfDuplex.h>

LiquidCrystal lcd(0, 1, 2, 3, 4, 5);
SoftwareSerialWithHalfDuplex dlcSerial(10, 10, false, false);

byte dlcdata[20] = {0}; 
byte dlcTimeout = 0, dlcChecksumError = 0;

void setup() {
  lcd.begin(16, 2);  

  dlcSerial.begin(9600);
  
  dlcInit();
}

void loop() {
  page2();

  delay(250);
}

/*
* ECU FUNCTIONS
*/

void dlcInit() {
  lcd.setCursor(0,0);
  lcd.print("INITIALIZING    ");
  dlcSerial.write(0x68);
  dlcSerial.write(0x6a);
  dlcSerial.write(0xf5);
  dlcSerial.write(0xaf);
  dlcSerial.write(0xbf);
  dlcSerial.write(0xb3);
  dlcSerial.write(0xb2);
  dlcSerial.write(0xc1);
  dlcSerial.write(0xdb);
  dlcSerial.write(0xb3);
  dlcSerial.write(0xe9);
  lcd.setCursor(0,0);
  delay(300);
}

int dlcCommand(byte cmd, byte num, byte loc, byte len) {
  byte crc = (0xFF - (cmd + num + loc + len - 0x01)); // checksum FF - (cmd + num + loc + len - 0x01)

  unsigned long timeOut = millis() + 200; // timeout @ 200 ms

  memset(dlcdata, 0, sizeof(dlcdata));

  dlcSerial.listen();

  dlcSerial.write(cmd); // header/cmd read memory ??
  dlcSerial.write(num); // num of bytes to send
  dlcSerial.write(loc); // address
  dlcSerial.write(len); // num of bytes to read
  dlcSerial.write(crc); // checksum

  // [00, Error code?] [Total Length] [Data, Length requested] [Checksum]
  // reply: 00 len+3 data...
  int i = 0; 
  while (i < (len + 3) && millis() < timeOut) {
    if (dlcSerial.available()) {
      dlcdata[i] = dlcSerial.read();
      // if (dlcdata[i] != 0x00 && dlcdata[i+1] != (len+3)) continue; // ignore ?
      i++;
    }
  }
  
  // If we have less than the required length, we timed out
  if (i < (len + 3)) { 
    // timeout
    dlcTimeout++;
    if (dlcTimeout > 255) {
      dlcTimeout = 0;
    }
    return 0; // failed
  }

  // calculate checksum
  crc = 0;
  for (i = 0; i < len + 2; i++) {
    crc = crc + dlcdata[i];
  }

  crc = 0xFF - (crc - 1);

  if (crc != dlcdata[len + 2]) { 
    // checksum failed
    dlcChecksumError++;
    return 0; // failed
  }

  return 1; // success
}

void resetEcu()
{
  // 21 04 01 DA / 01 03 FC
  dlcCommand(0x21, 0x04, 0x01, 0x00); // reset ecu
}

String calculateFuelTrim(byte input) {
  byte fuelTrim = ((input / 128) - 1) * 100;
  return formatIntAsLeftPaddedString(output, 2);
}

String calculateIACV(byte input) {
  byte output = input / 2.55;
  return formatIntAsLeftPaddedString(output, 2);
}

String calculateIgnitionAdvance(byte input) {
  byte output = (input - 128) / 2;
  return formatIntAsLeftPaddedString(output, 2);
}

String calculateInjectorPulseWidth(byte high, byte low) {
  int pulseWidth = (high << 8 | low) / 250;
  return formatIntAsLeftPaddedString(output, 5);
}

String calculateKpa(byte input) {
  byte output = input * 0.716 - 5;
  return formatIntAsLeftPaddedString(output, 3);
}

String calculateO2(byte input) {
  byte output = input / 0.513;
  return formatIntAsLeftPaddedString(output, 3);
}

String calculateRpm(byte high, byte low) {
  int rpm = 1875000 / (high << 8 | low);  
  return formatIntAsLeftPaddedString(output, 4);
}

String calculateTemp(byte input) {  
  byte output = 155.04149 - input * 3.0414878 + pow(input, 2) * 0.03952185 - pow(input, 3) * 0.00029383913 + pow(input, 4) * 0.0000010792568 - pow(input, 5) * 0.0000000015618437;
  return formatIntAsLeftPaddedString(output, 3);
}

String calculateTps(byte input) {
  byte output = (input - 24) / 2;
  return formatIntAsLeftPaddedString(output, 3);
}

/*
* PAGES
*/

void page1() {
  if (dlcCommand(0x20, 0x05, 0x10, 0x06)) {
    lcd.setCursor(0,0);

    // TPS - 4 digits
    lcd.print(calculateTps(dlcdata[6]));
    lcd.print(' ');
    
    // MAP - 4 digits
    lcd.print(calculateKpa(dlcdata[4]));
    lcd.print(' ');
    
    // ECT - 4 digits
    lcd.print(calculateTemp(dlcdata[2]));
    lcd.print(' ');

    // IAT - 4 digits
    lcd.print(calculateTemp(dlcdata[3]));

    lcd.setCursor(0,1);

    // IAT - 4 digits
    lcd.print(calculateO2(dlcdata[7]));
    lcd.print(' ');
  }

  if (dlcCommand(0x20, 0x05, 0x00, 0x03)) {
    // RPM - 5 digits
    lcd.print(calculateRpm(dlcdata[2], dlcdata[3]));
    lcd.print(' ');

    // VSS - 2 digits
    lcd.print(formatIntAsLeftPaddedString(dlcdata[4], 3));
  }
}

void page2() {
  if (dlcCommand(0x20, 0x05, 0x20, 0x10)) {
    lcd.setCursor(0,0);

    // STFT - 3 digits
    lcd.print(calculateFuelTrim(dlcdata[2]));
    lcd.print(' ');
    
    // LTFT - 2 digits
    lcd.print(calculateFuelTrim(dlcdata[4]));
    
    // 6 spaces
    lcd.print("       ");
    
    // Injector Pulse Width - 5 digits
    lcd.print(calculateInjectorPulseWidth(dlcdata[6], dlcdata[7]));  

    lcd.setCursor(0,1);

    // Ignition Advance - 2 digits
    lcd.print(calculateIgnitionAdvance(dlcdata[8]));
    
    // 5 spaces
    lcd.print("     ");

    // IACV -  2 digits
    lcd.print(calculateIACV(dlcdata[10]));
  }
}

/*
* STRING FUNCTIONS
*/

String formatIntAsLeftPaddedString(int input, byte digits) {
  String output = String(input, DEC); 
  boolean isNegative = false;

  // If negative, remove symbol 
  if (output.startsWith("-")) {
    isNegative = true;
    output = output.substring(1);
  }

  // Prepend until we hit the correct length
  while (output.length() < digits) {
    output = '0' + output;
  }

  // if negative, replace the symbol
  if (isNegative) {
    output = '-' + output.substring(1);
  }

  return output;
}

String formatByteAs2DigitHex(byte input) {
  String output = "";
  
  if (input < 0x10) {
    // If the value is less than 0x10, print a leading zero
    output = "0";
  }
  output += String(input, HEX);  
  output.toUpperCase();
  return output;
}

void printResponse() {
  lcd.setCursor(0, 1);

  for (int i=0; i<5; i++){
    String hexValue = formatByteAs2DigitHex(dlcdata[i]);
    lcd.print(hexValue);
    lcd.print(' ');
  }
}

