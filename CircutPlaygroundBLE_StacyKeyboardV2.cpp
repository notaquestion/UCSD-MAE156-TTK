#include <Adafruit_CircuitPlayground.h>
//#include <BLE52_Mouse_and_Keyboard.h> // This one litteraly breaks the Capacative touch. I have no idea why. Can't find anyone talking about it online. 
#include <TinyUSB_Mouse_and_Keyboard.h>

#include <avr/pgmspace.h>
#include <math.h>    // (no semicolon)


///////////////////////USER GLOBAL VARIABLES///////////////////////

//Changeable Menu Settings
int CycleSpeed = 1500; //Delay befor showing a new clickable option.
int GoBackHoldTime = 2000; //Amount of time user must hold input to peform Back Command in AwaitInput.
int InputDelay = 5; //Delay before a new Input can be started after showing a new option.

//Changeable Mouse Settings? - Stole them from Jakes, can we adapt them?
int CircleSpeed = 125; //How fast the mouse circles when input is not being held.
float CircleSize = 5; // Circle Radius
int MouseLinearSpeed = 70; //The Delay between each frame of the mouse moving in a line. Longer is slower.
float RampUpSpeed = 0.05; //Scalar effecting how quickly we ramp from min to max.

bool DebugSerialCapacativeTouch = true;

///////////////////////SYSTEM GLOBAL VARIABLES///////////////////////
float MouseTimer = 0.0;  //Timer used to determine mouse roation and angle in MouseFunctions();

enum Commands {None, Back, Select1}; //The commands we can issue with our hardware. Usually returned by AwaitInput()
Commands NextCommand = None; //What to do next.

int BaselineTouchThreshold = 0; //Touch Threshold when no one touching
int TouchThreshold = 0; //Adjustable threshold, adjusted up based on MaxTouch
int MaxTouch = 0;


///////////////////////FREQUENTLY USED WORD STORAGE///////////////////////
//Stacy has his own verison of this I've edited with him.
const char Words_A[] PROGMEM = "ABOUT/ACTION/ADVENTURE CAPATALIST/AFTER/ALSO/AMANDA";
const char Words_B[] PROGMEM = "BACK/BECAUSE/BRANDON/BURRITO/BIG FISH GAMES";
const char Words_C[] PROGMEM = "CAKE/CHEESEBURGER/CHERIE/CHEST/COME/COMEDY/COMPUTER/CONCERT/COOKIES/COULD";
const char Words_D[] PROGMEM = "DANIEL/DEONTE/DRAMA/DRINK STRAW";
const char Words_E[] PROGMEM = "EVEN";
const char Words_F[] PROGMEM = "FIND/FINGERS/FIRST/FRENCH FRIES/FROM/FROSTWIRE/FUNNY";
const char Words_G[] PROGMEM = "GAMES/GINA/GIVE/GOOD";
const char Words_H[] PROGMEM = "HAND/HAVE/HEAD/HIDDEN OBJECT/HIPS/HURT";
const char Words_I[] PROGMEM = "I WANT/I WANT TO/I WANT TO DO/I WANT TO GET/ICE CREAM SANDWICH/INTO";
const char Words_J[] PROGMEM = "JASON/JIGWORDS/JOSE/JUST";
const char Words_K[] PROGMEM = "KNOW";
const char Words_L[] PROGMEM = "LASAGNA/LEMONADE WATER/LETTERS/LIKE/LOOK/LOVE";
const char Words_M[] PROGMEM = "MACARONI/MAKE/MARCY/MICHAEL/MIKE/MONTI/MOST/MOVIE";
const char Words_N[] PROGMEM = "NECK/NICK";
const char Words_O[] PROGMEM = "ONLY/OTHER/OVER";
const char Words_P[] PROGMEM = "PAINFUL/PAUL/PEOPLE/PILLOW/PINEAPPLE WATER/PLAY/PLEASE";
const char Words_S[] PROGMEM = "SHEET/SOME/STEAM/SWEAT";
const char Words_T[] PROGMEM = "TAKE/TANGELO/THAN/THANK YOU/THAT/THEIR/THEM/THEN/THERE/THESE/THEY/THINK/THIS/TIME/TIRED/TOES/TURN/TV SERIES";
const char Words_W[] PROGMEM = "WANT/WELL/WENT/WHAT/WHEELCHAIR/WHEN/WHICH/WILL/WITH/WORDS/WORK/WOULD/WRITE";
const char Words_Y[] PROGMEM = "YEAR/YOUR";

const char *const AutoSuggestDic[] PROGMEM = {Words_A, Words_B, Words_C, Words_D, Words_E, Words_F, Words_G, Words_H, Words_I, Words_J, Words_K, Words_L, Words_M, Words_N, Words_O, Words_P, Words_S, Words_T, Words_W, Words_Y, };
const int AutoSuggestDic_SIZE = 20;

///////////////////////TYPE LETTER STORAGE///////////////////////
//This is a 2D grid 
const char LetterClumps_1[] PROGMEM = "_/E/T/S/D/W";
const char LetterClumps_2[] PROGMEM = "O/H/I/L/F/K";
const char LetterClumps_3[] PROGMEM = "A/N/U/G/V/Z";
const char LetterClumps_4[] PROGMEM = "R/Y/C/J/1/2";
const char LetterClumps_5[] PROGMEM = "M/B/Q/3/4/5";
const char LetterClumps_6[] PROGMEM = "P/X/6/7/8/9";
const char LetterClumps_7[] PROGMEM = "./?/,/+/!";
const char *const LetterClumps[] PROGMEM = {LetterClumps_1, LetterClumps_2, LetterClumps_3, LetterClumps_4, LetterClumps_5, LetterClumps_6, LetterClumps_7};
const int LetterClumps_SIZE = 7;

///////////////////////MENU STUFF///////////////////////
//All our menus tabbed by menu depth, linke to MenuTreeAsStrings bellow by index/position.
enum MenuTree {
  NoMenu,
  IdleMenu, //This isn't really a menu, more of a waiting state.
  MainMenu, //Our first true Menu.
    KeyboardMenu,
      TypeLetter, //This one's weird, we cick it over to a different system.
      Backspace, //A function, not a menu. If this get's selected, do the function and return to previous menu.
      AutoComplete, //Weird like type letter.
      SpecialKeyMenu,
        Enter, //These are functions, press that special key.
        Tab,
    OptionsMenu,
      FasterCycleSpeed,
      LongerCycleSpeed,
      ShorterGoBackInput,
      LongerGoBackInput,
  MouseMenu
};

MenuTree CurrentMenu = IdleMenu; //The menu we're currently on. Used as the control for a bit switch statment. Sometimes, we stick the name of a command in there, do the command, then update it to go back to the last menu.


String CurrentTypedWord = ""; //Every letter we've typed since space
String AutoCompleteOptions = ""; //A string with 5 words matching our CurrentyTypedWord each sepearted by a /

//Sometimes we need the String version of the enum above.
static const char * MenuTreeAsStrings[] = {
  "None",
  "IdleMenu", 
  "MainMenu",
    "KeyboardMenu",
      "TypeLetter",
      "Backspace",
      "AutoComplete",
      "SpecialKeyMenu",
        "Enter",
        "Tab",
    "OptionsMenu",
      "FasterCycleSpeed",
      "LongerCycleSpeed",
      "ShorterGoBackInput",
      "LongerGoBackInput",
  "MouseMenu"
};

//Converts the Enum to Coresponding string.
String MenuTreeToString(MenuTree menu)
{
  return MenuTreeAsStrings[menu];
}

//What Menus have sub menus? What MenuTree entries are subsidiary to each?
MenuTree M_MainMenu[] = { //Can't just be called MainMenu w/o compiler thinking I meant the enum element.
  KeyboardMenu,
  OptionsMenu,
  NoMenu //Serves as a terminatior when itterating through array.
};
MenuTree M_OptionsMenu[] = {
  FasterCycleSpeed,
  LongerCycleSpeed,
  ShorterGoBackInput,
  LongerGoBackInput,
  NoMenu //Serves as a terminatior when itterating through array.
};
MenuTree M_KeyboardMenu[] = {
  TypeLetter,
  Backspace,
  //AutoComplete,
  SpecialKeyMenu,
  NoMenu //Serves as a terminatior when itterating through array.
};
MenuTree M_SpecialKeyMenu[] = {
  Enter,
  Tab,
  NoMenu //Serves as a terminatior when itterating through array.
};

///////////////////////COLORS///////////////////////
uint32_t ColorWhite = 0xFFFFFF;
uint32_t ColorRed = 0xFF0000;
uint32_t ColorYellow = 0x808000;
uint32_t ColorGreen = 0x00FF00;
uint32_t ColorTeal = 0x008080;
uint32_t ColorBlue = 0x0000FF;

uint32_t StartupColor = ColorTeal;
uint32_t PendingColor = ColorWhite;
uint32_t ConfirmColor = ColorGreen;
uint32_t GoBackColor  = ColorRed;
uint32_t MouseColor   = ColorBlue;

///////////////////////END Variables///////////////////////

void setup() 
{ // initialize the buttons' inputs:
  
  Mouse.begin(); // I think these might sometimes cause the serial monitor to freeze the Circut Playground and IDE, not sure though.
  Keyboard.begin();

  //Some begining fuctions.
  Serial.begin(115200); //Please make sure your Baud rate is set in Serial Monitor to 115200.
  CircuitPlayground.begin();


  //Just an Everythings OK`show.
  delay(1000);

  calibrate();

  Serial.println(MenuTreeToString(MainMenu));
}

void calibrate()
{
  delay(10);
  BaselineTouchThreshold = 0;
  TouchThreshold = 0;
  MaxTouch = 0;
  int MinTouch = 10000000; //We use MaxTouch for other things, but only need min to calibrate starting value
  
    //Calibrate for 1000 frames, get the highest cap in that range and use it to set TouchThreshold and MaxTouch
  int calibrationDelation = 1000;
  for(int i = 0; i < calibrationDelation; ++i)
  {
    fillOverTime(PendingColor, i, calibrationDelation);
    if(CircuitPlayground.readCap(0) > MaxTouch)
      MaxTouch = CircuitPlayground.readCap(0);

    if(CircuitPlayground.readCap(0) < MinTouch)
      MinTouch = CircuitPlayground.readCap(0);

    delay(1);
    Serial.print("BaselineTouchThreshold:"); Serial.println(CircuitPlayground.readCap(0));
  }

  //Fixes: Problem with Min and Max being over 1000 during calibration because user is holding input while caplibrating.
  {
    if(MaxTouch > 1000)
      MaxTouch = 1000;

    if(MinTouch > 800) 
      MinTouch = 800;
  }


  BaselineTouchThreshold = MinTouch + 0.75 * (MaxTouch - MinTouch);
  TouchThreshold = BaselineTouchThreshold + 1; //Just add a bit so we can see it on the graph.

  CircuitPlayground.clearPixels();

  //Now that everythigns connected, rainbows!
  theaterChaseRainbow(20, 1000);
  CircuitPlayground.clearPixels();
}

char buffer[200];  // make sure this is large enough for the largest string it must hold
int bufferSize = 200;

void loop() {

  Commands input = None;

  //Big state machine based off MenuTree enum. 
  switch(CurrentMenu) {
    
    case NoMenu: {
        while(true)
        {
          Serial.println("Error: NO MENU?");
          delay(1);
        }
        break;
    }

    case IdleMenu: { 
        input = AwaitInput(CycleSpeed);

        if(input == Select1)
        {
            // DisplayText("Select Pressed, Hold to Use Mouse");
            // delay(1000);
            // ClearText("Select Pressed, Hold to Use Mouse");
        }
        else if(input == Back)
        {
            // DisplayText("Going to Mouse Menu");
            // delay(1000);
            // ClearText("Going to Mouse Menu");

            //DebugSerialCapacativeTouch = false;
            CurrentMenu = MouseMenu;
        }

        break;
    }

    case MainMenu: { 
        input = DisplayMenuOptions(M_MainMenu, 0xFFFFFF);
        if(input == Back)
        {
          CurrentMenu = MouseMenu;
        }
        break;
    }

      case KeyboardMenu: {
          input = DisplayMenuOptions(M_KeyboardMenu, 0xFF5555);
          if(input == Back)
          {
            CurrentMenu = MainMenu;
          }
          break;
      }

        case TypeLetter: {
            String SelectedCharacters = DispalyGridOptionsAndType(LetterClumps, LetterClumps_SIZE, '/', KeyboardMenu);
            if(SelectedCharacters == "_")
            {
              SelectedCharacters = " ";
              CurrentTypedWord = "";
              AutoCompleteOptions = "";
            }

            if(SelectedCharacters != "")
            {
              if(SelectedCharacters != " ")
              {
                CurrentTypedWord += SelectedCharacters;
              }
              DisplayText(SelectedCharacters);
              CurrentMenu = KeyboardMenu;
              PopulateAutoCompleteDicOptions();
            }

            break;
        }

        case Backspace: { 
            Keyboard.write(KEY_BACKSPACE);
            CurrentTypedWord.remove(CurrentTypedWord.length() - 1);

            bool again = true;
            while(again)
            {
              DisplayText("<--REPEAT?--");
              input = AwaitInput(CycleSpeed);

              if(input == None)
              {
                again = false;
              }
              else
              {
                ClearText("<--REPEAT?--");
                Keyboard.write(KEY_BACKSPACE);

                CurrentTypedWord.remove(CurrentTypedWord.length() - 1);
              }
              delay(1);

            }
            ClearText("<--REPEAT?--");

            CurrentMenu = KeyboardMenu;
            break;
        }

        case SpecialKeyMenu: {
            input = DisplayMenuOptions(M_SpecialKeyMenu, 0x7700FF);
            if(input == Back)
            {
              CurrentMenu = KeyboardMenu;
            }
            break;
        }

          case Enter: { 
              Keyboard.write(KEY_RETURN);
              CurrentMenu = SpecialKeyMenu;
              break;
          }

          case Tab: {
              Keyboard.write(KEY_TAB);
              CurrentMenu = SpecialKeyMenu;
              break;
          }

              //Keyboard.write(KEY_CAPS_LOCK);
              //Keyboard.write(KEY_ESC);

      case OptionsMenu: {
          input = DisplayMenuOptions(M_OptionsMenu, 0x55FFFF);

          if(input == Back)
          {
            CurrentMenu = MainMenu;
          }
          break;
      }

        case FasterCycleSpeed: {
            IncreaseCycleSpeed();
            CurrentMenu = OptionsMenu;
            break;
        }

        case LongerCycleSpeed: {
            DecreaseCycleSpeed();
            CurrentMenu = OptionsMenu;
            break;
        }

        case ShorterGoBackInput: {
            ShortenDelay();
            CurrentMenu = OptionsMenu;
            break;
        }

        case LongerGoBackInput: {
            IncreaseDelay();
            CurrentMenu = OptionsMenu;
            break;
        }

    case MouseMenu: {
        MouseFunctions();
        break;
    }

    default: {
        Serial.print("This Current Menu doesnt exist:"); Serial.println(CurrentMenu);
        break;
    }
  }

}


//////////////////////////////MOUSE/////////////////////////////////////////
//This Function gets evaluated every frame. 
//When the input is not held, MouseTimer is incrimented.
//  //New Sin/Cos movment values are compute from MouseTimer such that the mouse moves in a circle.
//When the input starts, we determine if a hold or a press is occuring.
//  //If a press is occuring, click.
//  //If a hold is occuring, continue moving at your current vector, i.e. a tangent to the circle. (Gotta see it in action)
//



void MouseFunctions()
{
  //doWhat = AwaitInput(1);

      //Base Color
      for(int i=0; i < 10; i++) { // For each pixel in strip...
        CircuitPlayground.setPixelColor(i, 0x110000);         //  Set pixel's color (in RAM)
      }                        

      ChangeMouseSettingsViaButton();

      //MOUSE SETTINGS
      //int CircleSpeed = 200; // Delay between each frame of the rotating circle, lower value, faster movement.
      // int CircleSpeed = 125; 
      // float CircleSize = 5; // Circle Radius
      //int MouseLinearSpeed = 100; //The Delay between each frame of the mouse moving in a line. Longer is slower.

      int MoveDelay = 500; // Hold time/delay before mouse starts moving instead of clicking.
      
            int HoldToGoBackLength = 150; //How many MouseLinearSpeed delays we should wait before going back to the MainMenu from this Mouse Menu
      
      //INTERNAL
      bool xDirection = false;
      float xMax = CircleSize * (float)cos(MouseTimer);
      float yMax = CircleSize * (float)sin(MouseTimer);

      float minSpeed = 0.0;//Minimum mouse speed
      float maxSpeed = 7.5;//Maximum mouse speed
      //float RampUpSpeed = 0.25; //Scalar effecting how quickly we ramp from min to max.

      int xMove = (round(xMax));
      int yMove = (round(yMax));

      Serial.println("");


      //Light up coresponding pixel on circut playgrond. (We have 10 pixels, but we'll treat it like a clock with 12 positions because of how pixels are offset)
      double deg = ((atan2(yMax, xMax) / ( 2* 3.14159)) + 0.5); // 0.0 - 1.0 range based on vector of the mouse;
      int pixelAsDirection = (12 - ceil(deg * 12.0)); //Switch the direction and scale to 12.
      pixelAsDirection = (pixelAsDirection + 0) % 12; //If you mount the Circut playground with the USB at the 5'clock position, then when the top pixel is lit, the mouse will go up. No light on 5 o'clock and 11 o'clock
      if(pixelAsDirection <= 4)
      {
        CircuitPlayground.setPixelColor(pixelAsDirection, 0x0000FF);
      }
      //else if(pixelAsDirection == 5)
      //{} //NO Light on 5
      else if(pixelAsDirection > 5 && pixelAsDirection < 11)
      {
        pixelAsDirection -= 1;
        CircuitPlayground.setPixelColor(pixelAsDirection, 0x0000FF);
      }
      //else if(pixelAsDirection == 11)
      //{} //NO Light on 11

      //Serial.print("Arctan: "); Serial.print(deg); Serial.print(" pixelAsDirection: "); Serial.println(pixelAsDirection);


      //MOVE THE MOUSE
      //Uses a special 3rds technique to make the circle look a little more circular and less like discrete chunks. 
      for(int m = 0; m < 3; ++m)
      {
        if(m < 2)
        {
          Mouse.move(xMove/3, yMove/3);
          //Serial.print(xMove/3); Serial.print("\t"); Serial.println(yMove/3);
        }
        else
        {
          Mouse.move(xMove/3 + xMove % 3, yMove/3 + yMove%3);
          //Serial.print(xMove/3 + xMove % 3); Serial.print("\t"); Serial.println(yMove/3 + yMOve % 3);
        }

        int persistanceOfVisionDelay = 50; //Delay stuff just a little to make it look like a continuous motion.
        for(int d = 0; d < persistanceOfVisionDelay; ++d)
        {
          if(TouchCondition())
          {
            d = persistanceOfVisionDelay; //Break out
            m = 3;
          }
          else
          {
            delay(1);
          }
        }
      }


      delay(CircleSpeed); //Here we have the real option to slow down/spead up the circle. 

      //Clicking and moving the mouse in a straight line.
      int heldTime = 0;
      if(TouchCondition())
      {
        CircuitPlayground.playTone(50, 100, false);

        //While the user TouchCondition(), everything stops, at least for MoveDelay amount of time. Longer than move dealy would be a HOLD action.
        while(TouchCondition() && heldTime < MoveDelay)
        {
          ++heldTime;
          delay(1);
        }
        
        //Are we holding the TouchCondition for greater than MoveDelay? If so, we're holding and we should move the mouse!
        if(heldTime >= MoveDelay) 
        {
          CircuitPlayground.playTone(50, 100, false);
          
          heldTime = 0; //Lets use this again.

          bool signaledGoBack = false;
          //As long as we're still holding this. Move, and accelerate our speed, and fill our HeldTime/fill over time meter.
          //If heldTime Goes over HoldToGoBackLength, we go back to typing Menu.
          while(TouchCondition()) 
          {
            
            float speed = constrain(heldTime * RampUpSpeed,  minSpeed, maxSpeed);
            Mouse.move(speed * float(xMove), speed *  float(yMove)); //Using heldTime as a scaler results in a linear acceleration.
            // delay(MouseLinearSpeed);
            int pause = 0;
            while(pause < MouseLinearSpeed) //If we don't check Touch Condition every single frame, we'll get signifigantly differt values. (Lower, less noisy);
            {
              TouchCondition();
              ++pause;
              delay(1);
            }
            ++heldTime;
            // if(heldTime < HoldToGoBackLength)
            // {
            //   fillOverTime(PendingColor, heldTime, HoldToGoBackLength); //Let us know how long until we trigger a go back to main menu.
            // }
            // else
            // {
            //   if(!signaledGoBack)
            //   {
            //     signaledGoBack = true;
            //     colorWipe(GoBackColor, 25);
            //     CircuitPlayground.playTone(50, 300, false);
            //   }
            // }
          }

          //If you hold for over a certain amount of time (until all the white LEDs are lit up), we assume that's because you want to go back to typing stuff/
          // if(heldTime > HoldToGoBackLength)
          // {
          //   CurrentMenu = MainMenu;
          //   CircuitPlayground.playTone(50, 300, false);
          //   colorWipe(GoBackColor, 50);
          // }
          // else
          // {
            CircuitPlayground.playTone(150, 100, false);
          // }
        }
        else
        {
          CircuitPlayground.playTone(100, 100, false);
          colorWipe(ConfirmColor, 25);
          Mouse.click(MOUSE_LEFT);
        }
      }
      //The other case, just incriment the timer so our rotation continues. 
      else if(!TouchCondition())
      {
        MouseTimer += 0.1;
      }
}


//////////////////////////////INPUT FUNCTIONS/////////////////////////////////
int AverageCap = 0;
int AverageCap2 = 0;

int FramesBellow = 0;

int StartRatio = 2000;


bool TouchCondition()
{

  int cap = CircuitPlayground.readCap(0);

  if(cap > 1000)
    cap = 1000;

  if(cap < AverageCap)
  {
    FramesBellow++;
    AverageCap = (AverageCap * StartRatio + cap * FramesBellow)/(StartRatio + FramesBellow);
  }
  else
  {
    FramesBellow = 0;
    AverageCap = (AverageCap * 9 + cap)/10;
  }

  // Fast Fall Smoothing
  // AverageCap = AverageCap > cap ? //Teranary operator. If this is true
  //       (AverageCap * 7 + cap)/10 // Reading was bellow average
  //       :(AverageCap * 9 + cap)/10; //Reading was above average

  if(AverageCap > MaxTouch)
  {
    MaxTouch = AverageCap;
    TouchThreshold = (MaxTouch + 1 * BaselineTouchThreshold)/2;
    //TouchThreshold = MaxTouch / 4;
  }

  int cap2 = CircuitPlayground.readCap(1);
  //Fast Fall Smoothing
  AverageCap2 = AverageCap2 > cap2 ? //Teranary operator. If this is true
        (AverageCap2 * 9 + cap2)/10 //Reutrn this
        :(AverageCap2 * 9 + cap2)/10; //Otherwise return this

  if(AverageCap2 > 1500)
  {
    calibrate();
    AverageCap2 = 0;
  }

  
  if(DebugSerialCapacativeTouch)
  {
    Serial.print("BaselineTouchThreshold:"); Serial.print(BaselineTouchThreshold); Serial.print('\t');
    Serial.print("TouchThreshold:"); Serial.print(TouchThreshold); Serial.print('\t');
    Serial.print("Raw:"); Serial.print(cap); Serial.print('\t');
    Serial.print("Raw2:"); Serial.print(AverageCap2); Serial.print('\t');
    Serial.print("Smoothed:"); Serial.print(AverageCap);
    //Serial.print("FramesBellowX100:"); Serial.print(FramesBellow * 100);
    Serial.print('\n');


  }


  return AverageCap > TouchThreshold;
}


Commands AwaitInput(int FramesToWait)
{
  if(!DebugSerialCapacativeTouch)
    Serial.println("\nAwaiting Input - ");
  bool hasBeenFalse = false; //Has this ever been false? TO prevent button from being held accidentily as part of last press.

  int i = 0;
  //Serial.println(TouchCondition());
  while (i < FramesToWait)
  {
    if(!TouchCondition()) //No Input
    {
      //Serial.print(".");

      //Seems like a bizare place to put the settings, but this is where we're waiting for input most of the time.
      ChangeSelectionSettingsViaButton();

      i += 100;
      delay(100);
      //++i;
      //delay(1);

      hasBeenFalse = true;
      
    }
    else if(TouchCondition() && hasBeenFalse) 
    {
      CircuitPlayground.playTone(50, 100, false);
      
      if(!DebugSerialCapacativeTouch)
        Serial.print("\nTouch Started...");
      
      int t = 0;


      while(t <= GoBackHoldTime && TouchCondition())
      {
        fillOverTime(PendingColor, t, GoBackHoldTime);
        t += 100;
        delay(100);
      }
      
      if(!DebugSerialCapacativeTouch)
      {
        Serial.print("\nFinished Touch. Duration "); Serial.print(t); Serial.print(" frames. Result: ");
      }

      CircuitPlayground.clearPixels();

      if(t >= GoBackHoldTime)
      {
        CircuitPlayground.playTone(125, 100, false);
        // DisplayText(" --Back-- ");
        colorWipe(GoBackColor, 25); 

        if(!DebugSerialCapacativeTouch)
          Serial.print("Go Back. Awaiting Release:");

        delay(500);
        // ClearText(" --Back-- ");

        while(TouchCondition())
        {
          delay(20);
            if(!DebugSerialCapacativeTouch)
              Serial.print('.');
        }
        if(!DebugSerialCapacativeTouch)
          Serial.println("Released");
        
        CircuitPlayground.playTone(150, 100, false);
        
        CircuitPlayground.clearPixels();
        return Back;
      }
      
      else if (t >= InputDelay)
      {
        CircuitPlayground.playTone(100, 100, false);

        // DisplayText(" --Selected-- ");
        // DisplayText(String(t));
        colorWipe(ConfirmColor, 25); 
  
        if(!DebugSerialCapacativeTouch)
          Serial.println("Select");
  
        delay(500);
        // ClearText(String(t));
        // ClearText(" --Selected-- ");

        CircuitPlayground.clearPixels();
        return Select1;
      }
      
    }

  }
  if(!DebugSerialCapacativeTouch)
    Serial.println("\n - End Await Input");
  
  CircuitPlayground.clearPixels();
  return None;
}


////////////////////////////TEXT DISPLAY//////////////////////////////////////
void DisplayText (String text_)
{
  // Serial.print("Going to print: ");
  // Serial.println(text_);
  for(int c = 0; text_[c] != '\0'; ++c)
  {
    Keyboard.write(text_[c]);
    //delay(10);
  }
  // Serial.println("Finished Print");
}

void ClearText (String text_)
{
  // Serial.print("Clear Text: ");
  // Serial.println(text_);
  for(int c = 0; text_[c] != '\0'; ++c)
  {
    Keyboard.write(KEY_BACKSPACE);
    //delay(10);
  }
  // Serial.print("Clear Text2 ");
  // Serial.println(text_);
}

void AddCurrsor()
{
  Keyboard.write('_');
  Keyboard.write(' ');
  Keyboard.write('<');
  Keyboard.write('-');
  Keyboard.write('-');
  Keyboard.write(' ');
  Keyboard.write(' ');
}

void RemoveCursor()
{
  Keyboard.write(KEY_BACKSPACE);
  Keyboard.write(KEY_BACKSPACE);
  Keyboard.write(KEY_BACKSPACE);
  Keyboard.write(KEY_BACKSPACE);
  Keyboard.write(KEY_BACKSPACE);
  Keyboard.write(KEY_BACKSPACE);
  Keyboard.write(KEY_BACKSPACE);
}

////////////////////////////LETTER/WORD DISPLAY//////////////////////////////////////
//(const char* const* array)

Commands DisplayMenuOptions(MenuTree menu_[], uint32_t menuColor_) 
{
  AddCurrsor();

  Commands doWhat;
  for(int M = 0; menu_[M] != NoMenu; ++M)
    {
      String selectionToDisplay = MenuTreeToString(menu_[M]);
      
      delay(100);

      DisplayText(selectionToDisplay);

      if(M < 10)
      {
          CircuitPlayground.setPixelColor(M, menuColor_);         
                           //  Pause for a moment
      }

      doWhat = AwaitInput(CycleSpeed); //Will return true when input conditoin true, or false in 1000 frames.
      
      delay(100);

      delay(100);

      ClearText(selectionToDisplay);
      
      if(doWhat == Select1)
      {
        CurrentMenu = menu_[M];

        RemoveCursor();
        return doWhat;
      }
      else if(doWhat == Back)
      {
        RemoveCursor();
        return doWhat;
      }
      
    }
    RemoveCursor();
    return doWhat;
}

String DispalyGridOptionsAndType(const char* const* options, int size, char seperator, MenuTree goBackMenu)
{
    String selection = "";
    AddCurrsor();

    for(int g = 0; g < size; ++g)
    {

        String optionsString = String(options[g]); //String cast not needed?
        DisplayText(" >");
        DisplayText(optionsString);
        DisplayText("<");

        Commands doWhatNow = AwaitInput(CycleSpeed);
        
        ClearText(" >");
        ClearText(optionsString);
        ClearText("<");

        if(doWhatNow == Select1)
        {
            selection = ParseStringAndPresentOptions(optionsString, seperator);
           
           if(selection == "")
           {
            //We wanted to go back.
            g = -1; //This'll become 0 in the next go round.
           }
           else
           {
            g = size ;
            //delay(3000);
           }

        }
        else if(doWhatNow == Back)
        {
          CurrentMenu = goBackMenu;
          //g = size;
          RemoveCursor();
          return "";
        }

        if(g == (size - 1))
        {
          g = -1;
        }
    }
    RemoveCursor();

    if(selection.length() > 1)
    {
      selection = " " + selection + " ";
    }

    if(selection != "")
    {
      //DisplayText(selection);
      return selection;
    }
}

String ParseStringAndPresentOptions(String parseMe, char seperator)
{
  String GridSelection;
  Commands doWhatNow;

  int c = 0;
  String chocies[10];

  int p = 0;
  String CurrentString = "";

  while(parseMe[p] != '\0' && c < 10)
  {
    if(parseMe[p] == seperator)
    {
      chocies[c] = CurrentString;
      ++c;
      CurrentString = "";
    }
    else
    {
      CurrentString += parseMe[p];
    }

    if(parseMe[p + 1] == '\0')
    {
      chocies[c] = CurrentString;
      ++c;
      CurrentString = "";
    }

    ++p;
  }
  

  // for(int s = 0; s < c; ++s) //We're going to offer you the choice of each thing in choices[]. 
  // {
  int s = 0;
  while(true)
  {
    DisplayText(">");
    for(int w = 0; w < c; ++w) //We're going to write each option in choices.
    {
      if(s == w)
      {
            DisplayText(" |");
            DisplayText(chocies[w]);
            DisplayText("| /");
      }
      else
      {
        DisplayText(chocies[w]);
        DisplayText("/");
      }
      // Keyboard.write(options[g][L]);
    }
    DisplayText("<");

    doWhatNow = AwaitInput(CycleSpeed);

    if(doWhatNow == Select1)
    {
      GridSelection =  chocies[s];

      CurrentMenu = KeyboardMenu;
      s = c;
    }
    else if(doWhatNow == Back)
    {
       GridSelection = "";
       s = c;
    }

    ClearText("> || <");
    for(int clearWord = 0; clearWord < c; ++clearWord) //We're going to clear
    {
      ClearText(chocies[clearWord]);
      // if(clearWord != s)
      // {
        ClearText("/");
      // }
    }

    ++s;
    if(s == c)
      s = 0;

    if(doWhatNow == Select1 || doWhatNow == Back)
    {
      return GridSelection;
    }

    // for(int d = 0; d < 11 ; ++d)
    // {
    //   Keyboard.write(KEY_BACKSPACE);
    // }


  }
}

//Create a list (5 long) of possible words from Autocomplete arrays, present them and await input.
//If user confirms they see the word they're trying to type, cycle those (up to) 5 options, if the user confirms the current word, type it.
//Otherwise, go back to previous behavior.
String PopulateAutoCompleteDicOptions()
{
  Serial.print("Current Typed Word: "); Serial.println(CurrentTypedWord);
  Serial.println("Looking for autocomplete options.");
  AutoCompleteOptions = "";
  if(CurrentTypedWord.length() > 0)
  {
    bool thisAutoSuggestDicExists = false; // Does an array starting with this letter exist?
    String WordsStartingWith = ""; // The current AutoSuggestDic of words starting with the same letter as our Current typed word.
    for(int a = 0; a < AutoSuggestDic_SIZE; ++a)
    {
      WordsStartingWith = AutoSuggestDic[a];

      if(WordsStartingWith[0] == CurrentTypedWord[0])
      {
        a = AutoSuggestDic_SIZE;
        thisAutoSuggestDicExists = true;
      }
    }
    int MaxAutocompleteLength = 5;
    int AutoCompleteLength = 0;
    int W = 0;
    String currentWord = "";
    int currentWordPlace = 0;

    //Serial.println("--Compiling auto complete--");
    if(thisAutoSuggestDicExists == true)
    {
      bool rejected = false;
      for (int i = 0; i < 300; ++i)
      {
        //delay(100);
        
        char newLetter = WordsStartingWith[i];
        if ('/' == newLetter || newLetter == 0) //End of array or any /
        {
          //Serial.print("/");
          if(currentWord.length() > 0 && rejected != true)
          {
            //DisplayText(currentWord);
            AutoCompleteOptions += currentWord;
            AutoCompleteOptions += '/';
            ++AutoCompleteLength;

            //Serial.print("AC_DIC : "); Serial.println(AutoCompleteOptions);

            if(AutoCompleteLength >= MaxAutocompleteLength || newLetter == 0) //We have 5 options end of array.
            {
              i = 300;
            }
          }
          rejected = false;
          currentWord = "";
          currentWordPlace = 0;
        }
        else if(newLetter == CurrentTypedWord[currentWordPlace] || currentWordPlace >= CurrentTypedWord.length())
        {
          //Serial.print("["); Serial.print(newLetter); Serial.print("]");
          currentWord += newLetter;
          ++currentWordPlace;
          // DisplayText(currentWord);
          // DisplayText(" - ");
        }
        else
        {
          //Serial.print("Rejecting: "); Serial.println(currentWord);
          rejected = true;
          //Serial.print("X");

        }
      }
    }
    if(AutoCompleteOptions.length() > 0)
    {
      AutoCompleteOptions.remove(AutoCompleteOptions.length() - 1, 1);
      DisplayText(" --- Auto Complete --- ");
      DisplayText(AutoCompleteOptions);
      Commands doWhatNow = AwaitInput(CycleSpeed + CycleSpeed/2);
      ClearText(AutoCompleteOptions);
      ClearText(" --- Auto Complete --- ");

      if(doWhatNow == Select1)
      {
        String typeWhat = ParseStringAndPresentOptions(AutoCompleteOptions, '/');

        if(typeWhat == "")
        {
          CurrentMenu = KeyboardMenu;
        }
        else
        {
          ClearText(CurrentTypedWord);
          DisplayText(typeWhat);
          DisplayText(" ");
          AutoCompleteOptions = "";
          CurrentTypedWord = "";
        }
      }

    }

    //ParseStringAndPresentOptions(AutoCompleteOptions, '/');
    //DispalyGridOptionsAndType(autoCompleteOptions, 1, '/', "KeyboardMenu");
  }

  return "";
}


/////////////////////////////ONBOARD OPTIONS/SETTINGS FUNCTIONS//////////////////////////////////////

void ChangeSelectionSettingsViaButton()
{
  //if(CircuitPlayground.slideSwitch())
  {
    if(CircuitPlayground.leftButton())
    {
      CircuitPlayground.playTone(50, 100, false);
      //Serial.println("L + L");
      int heldFor = 0;
      while(CircuitPlayground.leftButton() && heldFor < 3000 || heldFor < 200)
      {
        delay(1);
        ++heldFor;
      }
      CircuitPlayground.playTone(150, 100, false);

      CycleSpeed = heldFor;

      delay(0.5);
      CircuitPlayground.playTone(50, 100, false);
      DisplayText(" -New Cycle Speed: " + String(CycleSpeed));
      delay(CycleSpeed);
      ClearText(" -New Cycle Speed: " + String(CycleSpeed));
      CircuitPlayground.playTone(150, 100, false);

    }
    else if (CircuitPlayground.rightButton())
    {
      CircuitPlayground.playTone(50, 100, false);
      //Serial.println("L + L");
      int heldFor = 0;
      while(CircuitPlayground.rightButton() && heldFor < 3000 || heldFor < 400)
      {
        delay(1);
        ++heldFor;
      }
      CircuitPlayground.playTone(50, 100, false);

      GoBackHoldTime = heldFor;

      delay(0.5);
      CircuitPlayground.playTone(50, 100, false);
      DisplayText(" -New Hold to Go Back Time: " + String(GoBackHoldTime));
      delay(GoBackHoldTime);
      ClearText(" -New Hold to Go Back Time: " + String(GoBackHoldTime));
      CircuitPlayground.playTone(50, 100, false);
    }
  }
  // else
  // {
  //   if(CircuitPlayground.leftButton())
  //   {
  //     Serial.println("R + L");
  //   }
  //   else if (CircuitPlayground.rightButton())
  //   {
  //     Serial.println("R + R");
      
  //   }
  // }
}

void ChangeMouseSettingsViaButton()
{
  if(CircuitPlayground.slideSwitch())
  {
    if(CircuitPlayground.leftButton())
    {
      CircuitPlayground.playTone(50, 100, false);
      //Serial.println("L + L");
      int heldFor = 0;
      while(CircuitPlayground.leftButton() && heldFor < 3000 || heldFor < 1)
      {
        delay(1);
        ++heldFor;
      }
      CircuitPlayground.playTone(150, 100, false);

      CircleSpeed = constrain(heldFor/10, 0, 200);

      delay(0.5);
      CircuitPlayground.playTone(50, 100, false);
      DisplayText(" -New Mouse Circle Speed: " + String(CircleSpeed));
      delay(CycleSpeed);
      ClearText(" -New Mouse Circle Speed: " + String(CircleSpeed));
      CircuitPlayground.playTone(150, 100, false);

    }
    else if (CircuitPlayground.rightButton())
    {
      CircuitPlayground.playTone(50, 100, false);
      //Serial.println("L + L");
      int heldFor = 0;
      while(CircuitPlayground.rightButton() && heldFor < 3000 || heldFor < 200)
      {
        delay(1);
        ++heldFor;
      }
      CircuitPlayground.playTone(50, 100, false);

      MouseLinearSpeed = constrain(heldFor/10, 20, 500);

      delay(0.5);
      CircuitPlayground.playTone(50, 100, false);
      DisplayText(" -New Mouse Linear Speed: " + String(MouseLinearSpeed));
      delay(CycleSpeed);
      ClearText(" -New Mouse Linear Speed: " + String(MouseLinearSpeed));
      CircuitPlayground.playTone(50, 100, false);
    }
  }
  else
  {
    if(CircuitPlayground.leftButton())
    {
      CircuitPlayground.playTone(50, 100, false);
      //Serial.println("L + L");
      int heldFor = 0;
      while(CircuitPlayground.leftButton() && heldFor < 20000 || heldFor < 750)
      {
        delay(1);
        ++heldFor;
      }
      CircuitPlayground.playTone(150, 100, false);

      CircleSize = constrain(heldFor/1000.0, 0.75, 20.0);

      delay(0.5);
      CircuitPlayground.playTone(50, 100, false);
      DisplayText(" -New Mouse Circle Radius: " + String(CircleSize));
      delay(CycleSpeed);
      ClearText(" -New Mouse Circle Radius: " + String(CircleSize));
      CircuitPlayground.playTone(150, 100, false);

    }
    // else if (CircuitPlayground.rightButton())
    // {
    //   Serial.println("R + R");
      
    // }
  }
}

void ShortenDelay()
{
  GoBackHoldTime -= 250;
  String message = String("  --  Hold Time for Back: " + String(GoBackHoldTime));
  DisplayText(message);
  delay(2000);
  ClearText(message);
  //Serial.println(CycleSpeed);
}

void IncreaseDelay()
{
  GoBackHoldTime += 500;
  String message = String("  --  Hold Time For Back: " + String(GoBackHoldTime));
  DisplayText(message);
  delay(2000);
  ClearText(message);
  //Serial.println(CycleSpeed);
}

void IncreaseCycleSpeed()
{
  CycleSpeed *= 0.9;
  String message = String("  --  CycleSpeed: " + String(CycleSpeed));
  DisplayText(message);
  delay(2000);
  ClearText(message);
}

void DecreaseCycleSpeed()
{
  CycleSpeed *= 1.2;
  String message = String("  --  CycleSpeed: " + String(CycleSpeed));
  DisplayText(message);
  delay(2000);
  ClearText(message);
}


/////////////////////////////LED FUNCTIONS//////////////////////////////////////

void colorWipe(uint32_t color, int wait) {
   for(int i=0; i< 10; i++) { // For each pixel in strip...
     CircuitPlayground.setPixelColor(i, color);
     delay(wait);                           //  Pause for a moment
   }
}

void theaterChaseRainbow(int spinSpeed, int duration){
  
  int counter = 0;

  while(counter < duration)
  {
    for(int i=0; i< 10; i++) { // For each pixel in strip...
      uint8_t colorIndex = i * 255/10 + counter;
      CircuitPlayground.setPixelColor(i, CircuitPlayground.colorWheel(colorIndex));
    }
    delay(spinSpeed);
    counter += spinSpeed;
  }
}

void fillOverTime(uint32_t color, int currentTime, int maxTime)
{
  int numToFill =  ((float)currentTime/(float)maxTime * (float)10.5);
  //Serial.println(10 * (float)currentTime/(float)maxTime);
  //for(int i=0; i< numToFill; i++) { // For each pixel in strip...
    CircuitPlayground.setPixelColor(numToFill, color);
  //}
}