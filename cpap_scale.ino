//By Scott Clandinin
//2017

#include <Q2HX711.h>
#include <Tone.h>
#include <EEPROM.h> 

#define OCTAVE_OFFSET 0

int notes[] = { 0,
NOTE_C4, NOTE_CS4, NOTE_D4, NOTE_DS4, NOTE_E4, NOTE_F4, NOTE_FS4, NOTE_G4, NOTE_GS4, NOTE_A4, NOTE_AS4, NOTE_B4,
NOTE_C5, NOTE_CS5, NOTE_D5, NOTE_DS5, NOTE_E5, NOTE_F5, NOTE_FS5, NOTE_G5, NOTE_GS5, NOTE_A5, NOTE_AS5, NOTE_B5,
NOTE_C6, NOTE_CS6, NOTE_D6, NOTE_DS6, NOTE_E6, NOTE_F6, NOTE_FS6, NOTE_G6, NOTE_GS6, NOTE_A6, NOTE_AS6, NOTE_B6,
NOTE_C7, NOTE_CS7, NOTE_D7, NOTE_DS7, NOTE_E7, NOTE_F7, NOTE_FS7, NOTE_G7, NOTE_GS7, NOTE_A7, NOTE_AS7, NOTE_B7
};

const byte hx711_data_pin = A2;
const byte hx711_clock_pin = A3;

const int ack = 2;
const int calibration_empty = 4;
const int calibration_full = 3;
const int hanger = 5;

int ack_state;
int calibration_empty_state;
int calibration_full_state;
int hanger_state;


Q2HX711 hx711(hx711_data_pin, hx711_clock_pin);
Tone tone1;

long weight_measure[5];
long weight_moving_average[11];
long empty_weight;
long full_weight;
long result_weight;
long alarm_weight;

const float alarm_percent = 0.15;   //this value represents the percentage of water remaining
                                    //before the alarm goes off

char *song = "scotland:d=4,o=5,b=250:c,32p,2f,f.,8g,a,f,a,c6,2f6,f.6,8f6,f6,c6,a,f,2a#,d.6,8a#,a,c6,a,f,2g,c.6,8d6,8c.6,16p,8c6,8d6,8c6,8b,8a,8g,2f,f.,8g,a,f,a,c6,2f6,f.6,8f6,f6,c6,a,f,2a#,d.6,8a#,a,c6,a,f,2g,f.,32p,8g,32p,2f,32p,f";
char *success_beep = "beep:d=4,o=5,b=250:c,a";
char *fail_beep = "beep:d=4,o=1,b=200:d,c#";

//*************************************************
//  void setup()
//*************************************************
void setup() 
{
  Serial.begin(9600);
  
  tone1.begin(8);
  
  pinMode(ack, INPUT_PULLUP);
  pinMode(calibration_empty, INPUT_PULLUP);
  pinMode(calibration_full, INPUT_PULLUP);
  pinMode(hanger, INPUT_PULLUP);

  empty_weight = EEPROMReadlong(0);
  full_weight = EEPROMReadlong(4);

  Serial.println("CPAP Monitor will sound alarm at 15% water remaining.");
  delay(1500);

  Serial.print("Stored Full Weight: ");
  Serial.println(full_weight);
  
  Serial.print("Stored Empty Weight: ");
  Serial.println(empty_weight);
  Serial.println();
  delay(300);
}


#define isdigit(n) (n >= '0' && n <= '9')

//*************************************************
//  void loop()
//*************************************************
void loop() 
{

  
  //alarm test
  ack_state = digitalRead(ack);
  if (!ack_state)
  {
    delay(500);
    Serial.println("Testing Alarm");
    Serial.println();
    play_rtttl(song);
  }

  //check for calibrate empty button
  calibration_empty_state = digitalRead(calibration_empty);
  if (!calibration_empty_state && !hanger_state)
  {
    empty_weight = measure();  //take measurement
    EEPROMWritelong(0, empty_weight);
    
    Serial.print("Empty weight average: ");
    Serial.println(empty_weight);
    Serial.println("Stored in EEPROM");
    Serial.println();
    delay(200);

    //alert user if the empty weight is more than the full weight
    if (empty_weight >= full_weight)
    {
      play_rtttl(fail_beep);
    }
    else
    {
      play_rtttl(success_beep);
    }
    
  }

  
  //check for calibrate full button
  calibration_full_state = digitalRead(calibration_full);
  if (!calibration_full_state && !hanger_state)
  {
    full_weight = measure();  //take measurement
    EEPROMWritelong(4, full_weight);
    Serial.print("Full weight average: ");
    Serial.println(full_weight);
    Serial.println("Stored in EEPROM");
    Serial.println();
    delay(200);
    
    //alert user if the empty weight is more than the full weight
    if (empty_weight >= full_weight)
    {
      play_rtttl(fail_beep);
    }
    else
    {
      play_rtttl(success_beep);
    }
    
  }


  //if the mask is off the hanger, and the calibration weights have been properly set, begin measurements
  hanger_state = digitalRead(hanger);
  if (hanger_state && (empty_weight < full_weight))
  {
    play_rtttl(success_beep);
    Serial.println("****************");
    Serial.println("  Mask in use");
    Serial.println("****************");
    Serial.println();

    //calculate lowest weight value before alarm, calculated for 10% water remaining
    alarm_weight = ((full_weight - empty_weight) * alarm_percent) + empty_weight;
    
    initial_measurements(); //take inital 9 measurements for the moving average
  }

  //if the calibration weights have not been properly set, alert user
  if (hanger_state && (empty_weight >= full_weight))
  {
    play_rtttl(fail_beep);
    delay(2000);
  }

  //begin moving average calculation if the mask is off the hanger, and the calibration weights have been properly set
  while (hanger_state && (empty_weight < full_weight))
  {
    result_weight = moving_average();  //current weight based on moving average
    delay(1000);
    
    if (result_weight < alarm_weight)
    {
      Serial.println("**************************");
      Serial.println("** Water Reservoir Low! **");
      Serial.println("**************************");
      play_rtttl(song);
      delay(15000);
    } 
    hanger_state = digitalRead(hanger);
  } 
}


//*************************************************
//  long measure() - take weight measurement
//*************************************************
long measure()
{
  long measurement;
  
  weight_measure[0] = hx711.read()/100;
  delay(50);
  weight_measure[1] = hx711.read()/100;
  delay(50);
  weight_measure[2] = hx711.read()/100;
  delay(50);
  weight_measure[3] = hx711.read()/100;
  delay(50);
  weight_measure[4] = hx711.read()/100;
  delay(50);  

  measurement = (weight_measure[0] + weight_measure[1] + weight_measure[2] + weight_measure[3] + weight_measure[4]) / 5; 
  
  return measurement;
}


//*************************************************
//  void initial_measurements() - take initial 9 
//  measurements for moving average
//*************************************************
void initial_measurements()
{
  for (int i=0; i<9; i++)
  {
    weight_moving_average[i] = measure();
  }
  Serial.println("Initial Measurements:");
  delay(500);
  Serial.println(weight_moving_average[0]);
  delay(100);
  Serial.println(weight_moving_average[1]);
  delay(100);
  Serial.println(weight_moving_average[2]);
  delay(100);
  Serial.println(weight_moving_average[3]);
  delay(100);
  Serial.println(weight_moving_average[4]);
  delay(100);
  Serial.println(weight_moving_average[5]);
  delay(100);
  Serial.println(weight_moving_average[6]);
  delay(100);
  Serial.println(weight_moving_average[7]);
  delay(100);
  Serial.println(weight_moving_average[8]);
  delay(100);
}

//*************************************************
//  long moving_average() - update moving average 
//*************************************************
long moving_average()
{
  long result;
  
  for (int i=9; i>-1; i--)
  {
    weight_moving_average[i+1] = weight_moving_average[i];    //write current value to next position
  }

  //make new measurement for index 0
  weight_moving_average[0] = measure();           //place new measurement in index 0

  //add up values
  for (int j=0; j<10; j++)
  {
    result = result + weight_moving_average[j];
  }
  Serial.println("Calculating moving average");
  delay(500);
  Serial.println(weight_moving_average[0]);
  delay(100);
  Serial.println(weight_moving_average[1]);
  delay(100);
  Serial.println(weight_moving_average[2]);
  delay(100);
  Serial.println(weight_moving_average[3]);
  delay(100);
  Serial.println(weight_moving_average[4]);
  delay(100);
  Serial.println(weight_moving_average[5]);
  delay(100);
  Serial.println(weight_moving_average[6]);
  delay(100);
  Serial.println(weight_moving_average[7]);
  delay(100);
  Serial.println(weight_moving_average[8]);
  delay(100);
  Serial.println(weight_moving_average[9]);
  delay(100);
  
  result = result/10;
  Serial.println();
  Serial.print("Result: ");
  Serial.println(result);
  Serial.print("Alarm Weight: ");
  Serial.println(alarm_weight);
  Serial.println();
  delay(500);
  return result;
}


//*************************************************
//  play_rtttl(char *p) - play alarm song
//*************************************************
void play_rtttl(char *p)
{
  // Absolutely no error checking in here

  byte default_dur = 4;
  byte default_oct = 6;
  int bpm = 63;
  int num;
  long wholenote;
  long duration;
  byte note;
  byte scale;

  // format: d=N,o=N,b=NNN:
  // find the start (skip name, etc)

  while(*p != ':') p++;    // ignore name
  p++;                     // skip ':'

  // get default duration
  if(*p == 'd')
  {
    p++; p++;              // skip "d="
    num = 0;
    while(isdigit(*p))
    {
      num = (num * 10) + (*p++ - '0');
    }
    if(num > 0) default_dur = num;
    p++;                   // skip comma
  }


  // get default octave
  if(*p == 'o')
  {
    p++; p++;              // skip "o="
    num = *p++ - '0';
    if(num >= 3 && num <=7) default_oct = num;
    p++;                   // skip comma
  }


  // get BPM
  if(*p == 'b')
  {
    p++; p++;              // skip "b="
    num = 0;
    while(isdigit(*p))
    {
      num = (num * 10) + (*p++ - '0');
    }
    bpm = num;
    p++;                   // skip colon
  }


  // BPM usually expresses the number of quarter notes per minute
  wholenote = (60 * 1000L / bpm) * 4;  // this is the time for whole note (in milliseconds)


  // now begin note loop
  while(*p)
  {
    // first, get note duration, if available
    num = 0;
    while(isdigit(*p))
    {
      num = (num * 10) + (*p++ - '0');
    }
    
    if(num) duration = wholenote / num;
    else duration = wholenote / default_dur;  // we will need to check if we are a dotted note after

    // now get the note
    note = 0;

    switch(*p)
    {
      case 'c':
        note = 1;
        break;
      case 'd':
        note = 3;
        break;
      case 'e':
        note = 5;
        break;
      case 'f':
        note = 6;
        break;
      case 'g':
        note = 8;
        break;
      case 'a':
        note = 10;
        break;
      case 'b':
        note = 12;
        break;
      case 'p':
      default:
        note = 0;
    }
    p++;

    // now, get optional '#' sharp
    if(*p == '#')
    {
      note++;
      p++;
    }

    // now, get optional '.' dotted note
    if(*p == '.')
    {
      duration += duration/2;
      p++;
    }
  
    // now, get scale
    if(isdigit(*p))
    {
      scale = *p - '0';
      p++;
    }
    else
    {
      scale = default_oct;
    }

    scale += OCTAVE_OFFSET;

    if(*p == ',')
      p++;       // skip comma for next note (or we may be at the end)

    // now play the note

    if(note)
    {
      ack_state = digitalRead(ack);
        if (!ack_state)
        {
          delay(300);
          return;
        }
      tone1.play(notes[(scale - 4) * 12 + note]);
      delay(duration);
      tone1.stop();
    }
    else
    {
      ack_state = digitalRead(ack);
      if (!ack_state)
      {
        delay(300);
        return;
      }
      delay(duration);
    }
  }
}




//*************************************************
//  EEPROMWritelong
//*************************************************
void EEPROMWritelong(int address, long value)
{
      //Decomposition from a long to 4 bytes by using bitshift.
      //One = Most significant -> Four = Least significant byte
      byte four = (value & 0xFF);
      byte three = ((value >> 8) & 0xFF);
      byte two = ((value >> 16) & 0xFF);
      byte one = ((value >> 24) & 0xFF);

      //Write the 4 bytes into the eeprom memory.
      EEPROM.write(address, four);
      EEPROM.write(address + 1, three);
      EEPROM.write(address + 2, two);
      EEPROM.write(address + 3, one);
}



//*************************************************
//  EEPROMReadlong
//*************************************************
long EEPROMReadlong(long address)
{
      //Read the 4 bytes from the eeprom memory.
      long four = EEPROM.read(address);
      long three = EEPROM.read(address + 1);
      long two = EEPROM.read(address + 2);
      long one = EEPROM.read(address + 3);

      //Return the recomposed long by using bitshift.
      return ((four << 0) & 0xFF) + ((three << 8) & 0xFFFF) + ((two << 16) & 0xFFFFFF) + ((one << 24) & 0xFFFFFFFF);
}

            
