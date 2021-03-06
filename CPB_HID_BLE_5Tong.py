import time
import board
from adafruit_circuitplayground import cp
import touchio

#BLE STUFF
import adafruit_ble
from adafruit_ble.advertising import Advertisement
from adafruit_ble.advertising.standard import ProvideServicesAdvertisement
from adafruit_ble.services.standard.hid import HIDService
from adafruit_ble.services.standard.device_info import DeviceInfoService
from adafruit_hid.keyboard import Keyboard
from adafruit_hid.keyboard_layout_us import KeyboardLayoutUS
from adafruit_hid.keycode import Keycode
from adafruit_hid.mouse import Mouse
#END BLE STUFF

#HID BLE SETUP
hid = HIDService()

device_info = DeviceInfoService(software_revision=adafruit_ble.__version__, manufacturer="Adafruit Industries")
advertisement = ProvideServicesAdvertisement(hid)
advertisement.appearance = 961
scan_response = Advertisement()
scan_response.complete_name = "CircuitPython HID"

ble = adafruit_ble.BLERadio()
if not ble.connected:
    print("advertising")
    ble.start_advertising(advertisement, scan_response)
else:
    print("already connected")
    print(ble.connections)

k = Keyboard(hid.devices)
kl = KeyboardLayoutUS(k)
mouse = Mouse(hid.devices)
#End BLE SETUP

#COLORS
ColorToLight = 0xFFFFFF

SingleLight_Color = 0xFFFFFF
B1_Color = 0x100010
B2_Color = 0x100300
B3_Color = 0x010310
B4_Color = 0x101000
TooMany_Color = 0xFF0007

#TODO: Use these to indicate when Left or Right Click is held down.
#First Layer of Colors, overwrites baseColors
#L1Colors = [0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000]
#BaseColors = [0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000]

#END COLORS

def clamp(n, smallest, largest): return max(smallest, min(n, largest))

#TOUCH SENSITIVE "BUTTONS"
class ButtonStates:
    UP = 0
    CONTACT = 1
    JUST_PRESSED = 2 #A 1 frame state for the frame we passed the pressed threshold
    PRESSED = 3 #We'll stayin pressed until we pas the time for down.
    DOWN = 4 #We're over timeTillHeld threshold time.
    JUST_RELEASED_HOLD = 5 #We released from a long hold
    JUST_RELEASED_PRESS = 6 #We released, but weren't holding
    RELEASED = 7 #Do we need this? maybe for a cooldown?

class Button:
    #These CAPS are python "static" variable, don't overwrite it per instace, only via Button.var
    ANY_ACTIVATED = False
    CURRENT_ACTIVATED = None
    LAST_ACTIVATED = None
    LAST_CONTACTED = None
    state = ButtonStates.UP #What is this buttons state?
    order = 0 #... we may not need this, it's in init.
    
    myPixels = []
    myColor = 0xFFFFFF

    #Note, there are some time.sleeps for debouncing that add an additional 0.11s to these times.
    holdTime = 0.0 #How long has this butotn been held for?
    timeTillPressed = 0.5 #over this threshold, we're pressing the button.
    timeTillHeld = 1.0 #over this threshold we're holding this button.




    def __init__(self, name, order, pixels, color, condition):
        self.name = name
        self.order = order
        self.myPixels = pixels
        self.myColor = color
        self.touchCondition = condition

    def SetPressedAndHoldTimes(self, pressTime, holdTime):
        self.timeTillPressed = pressTime
        self.timeTillHeld = holdTime

    def Pressed(self): #returns "are we pressed"
        return self.holdTime > self.timeTillPressed and self.holdTime < self.timeTillHeld

    def Held(self): #returns "are we held"
        return self.holdTime > self.timeTillHeld

    def Update(self): #update the state of this button. Needed to incriment timer in button.
        slide = Button.CURRENT_ACTIVATED is not None and Button.LAST_ACTIVATED is self #If there was a button activated last frame that wasn't us.
        
        if self.touchCondition() and not Button.CURRENT_ACTIVATED: # this is touched and no button is the current touched button
            self.holdTime += 0.01
            Button.CURRENT_ACTIVATED = self
            if(self.state == ButtonStates.UP and not self.Pressed()): #contact made but not long enough to count as a press
                time.sleep(0.1) # Wait a frame and see if we're still being touched
                if(self.touchCondition()): #A little bit of debounce on sound (but not light) in case we get some noise 
                    self.state = ButtonStates.CONTACT
                    Button.LAST_CONTACTED = self
                    cp.stop_tone()
                    cp.start_tone(100 + (50 * (MODE * 9 +self.order)))
            elif (self.state == ButtonStates.CONTACT and self.Pressed()):
                self.state = ButtonStates.JUST_PRESSED
                cp.stop_tone()
                time.sleep(0.01)
            elif self.state == ButtonStates.JUST_PRESSED:
                self.state = ButtonStates.PRESSED
                cp.start_tone(100 + (50 * (MODE * 9 +self.order)))
            elif self.state == ButtonStates.PRESSED and self.Held():
                cp.stop_tone()
                time.sleep(0.01)
                cp.start_tone(100 + (50 * (MODE * 9 +self.order)))
                self.state = ButtonStates.DOWN
        else :
            self.holdTime = 0.0
            if self.state == ButtonStates.DOWN:
                self.state = ButtonStates.JUST_RELEASED_HOLD
                if not slide:
                    cp.stop_tone()
            elif self.state== ButtonStates.JUST_PRESSED or self.state == ButtonStates.PRESSED:
                self.state = ButtonStates.JUST_RELEASED_PRESS
                if not slide:
                    cp.stop_tone()
            elif self.state == ButtonStates.JUST_RELEASED_HOLD or self.state == ButtonStates.JUST_RELEASED_PRESS:
                self.state = ButtonStates.RELEASED
            elif self.state == ButtonStates.CONTACT:
                if not slide:
                    cp.stop_tone()
                self.state = ButtonStates.UP
            else:
                self.state = ButtonStates.UP


A1 = Button("A1", 1, [9], 0x101010,  lambda: cp.touch_A1)
B1 = Button("B1", 2, [9, 8], B1_Color,  lambda: cp.touch_A1 and cp.touch_A2) #this "button" is pressed if both pins are pressed
A2 = Button("A2", 3, [8], 0x101010,  lambda: cp.touch_A2)
B2 = Button("B2", 4, [8, 7], B2_Color,  lambda: cp.touch_A2 and cp.touch_A3)
A3 = Button("A3", 5, [7], 0x101010,  lambda: cp.touch_A3)
B3 = Button("B3", 6, [7, 6], B3_Color,  lambda: cp.touch_A3 and cp.touch_A4)
A4 = Button("A4", 7, [6], 0x101010,  lambda: cp.touch_A4)
B4 = Button("B4", 8, [6, 5], B4_Color,  lambda: cp.touch_A4 and cp.touch_A5)
A5 = Button("A5", 9, [5], 0x101010,  lambda: cp.touch_A5)

#Set custom (shorter) Hold Times for Left/Right Up/Down since they're non-destructive
#Note, there are some time.sleeps for debouncing that add an additional 0.11s to these times.
A2.SetPressedAndHoldTimes(0.05, 0.1)
B2.SetPressedAndHoldTimes(0.05, 0.1)

B3.SetPressedAndHoldTimes(0.05, 0.1)
A4.SetPressedAndHoldTimes(0.05, 0.1)


#Put buttons in a list to iterate through. Put dual condition ones first because first true blocks the rest.
Buttons = [B1, B2, B3, B4, A1, A2, A3, A4, A5] #A list we can iterate through. Must Evaluate Double buttons first as they'll block single buttons.
#END TOUCH SENSITIVE BUTTONS

MODE = 0 #What if we want to change what all the buttons do?

SHORT_MOUSE_MOVE = 10 #How many units should the mouse move on a tap.

MOUSE_ACCEL_SPEED = 0.3
MOUSE_ACCEL_TIMER = 0
MOUSE_MAX_SPEED = 5.0
LEFT_MOUSE_HELD = False
RIGHT_MOUSE_HELD = False

SCROLL_UP = False #Bool to flip up/down scrolling


#Updates which LEDs to light and color to light them.
def UpdateLightsToTouches():
    global ColorToLight
    if (B1.Held() and B2.Held()) or (B2.Held() and B3.Held()) or (B3.Held() and B4.Held()):
        ColorToLight = TooMany_Color

    PixelsToLight = [(0, 0, 0)] * 10

    if MODE == 0:
        PixelsToLight = [(5, 2, 2)] * 5 + [(0, 0, 0)] * 5
    elif MODE == 1:
        PixelsToLight = [(2, 5, 2)] * 5 + [(0, 0, 0)] * 5
    elif MODE == 2:
        PixelsToLight = [(2, 2, 5)] * 5 + [(0, 0, 0)] * 5

    if Button.CURRENT_ACTIVATED:
        for p in Button.CURRENT_ACTIVATED.myPixels:
            PixelsToLight[p] = Button.CURRENT_ACTIVATED.myColor

    #Additional lights for L/R mouse held down.
    if LEFT_MOUSE_HELD:
        PixelsToLight[9] = (30, 30, 30)

    if RIGHT_MOUSE_HELD:
        PixelsToLight[8] = (30, 30, 30)


    for i in range(10):
        if cp.pixels[i] != PixelsToLight[i]:
            cp.pixels[i] = PixelsToLight[i];

    #print(cp.pixels)


#main loop
while True:
    while not ble.connected:
        cp.pixels.fill((1, 0, 0))
        time.sleep(0.5)
        cp.pixels.fill((0, 0, 1))
        time.sleep(0.5)
        pass
    print("Start typing:")

    while ble.connected:
        cp.pixels.brightness = 0.7 #low for deving (kept blinding myself.)

        #Update all button states and hold times (single pin and double pin buttons)
        #TODO: This should be in the button class. Update Self.
        #TODO: Better Pressed and held times that flip state. Will probbably need a state between pressed and down/ released and up, so as to not do pressed and released functionality multiple times.
        Button.LAST_ACTIVATED = Button.CURRENT_ACTIVATED
        Button.CURRENT_ACTIVATED = None #Reset blocking so one can get through.

        for i in range(len(Buttons)):
            Buttons[i].Update()

            #A handy Debug
            #print(Buttons[i].name, "", Buttons[i].touchCondition(), Buttons[i].holdTime, Buttons[i].state, end="  -  ")

        #print("")
        UpdateLightsToTouches()

        if cp.switch: # Do real functionality when switch is on
            if True: #MODE == 0:
                #B1 - Cycle Open windows and on screen Keyboard
                if Buttons[0].state == ButtonStates.JUST_RELEASED_PRESS: #Cycle through open windows
                    k.send(Keycode.ESCAPE, Keycode.ALT)
                    # time.sleep(0.01)
                    # k.release(Keycode.ESCAPE, Keycode.ALT)
                elif Buttons[0].state == ButtonStates.JUST_RELEASED_HOLD:  #Open Onscreen Keyboard
                    k.send(Keycode.WINDOWS, Keycode.RIGHT_CONTROL, Keycode.O)

                #B2 - Move Mouse Up
                elif Buttons[1].state == ButtonStates.JUST_RELEASED_PRESS:
                    mouse.move(y=SHORT_MOUSE_MOVE)
                elif Buttons[1].state == ButtonStates.DOWN:
                    MOUSE_ACCEL_TIMER += MOUSE_ACCEL_SPEED
                    speed = clamp((1 + MOUSE_ACCEL_TIMER), 1, MOUSE_MAX_SPEED)
                    mouse.move(y = int(speed))
                elif Buttons[1].state == ButtonStates.JUST_RELEASED_HOLD:
                    MOUSE_ACCEL_TIMER = 0

                #B3 - Move Mouse Down
                elif Buttons[2].state == ButtonStates.JUST_RELEASED_PRESS:
                    mouse.move(y=-SHORT_MOUSE_MOVE)
                elif Buttons[2].state == ButtonStates.DOWN:
                    MOUSE_ACCEL_TIMER += MOUSE_ACCEL_SPEED
                    speed = clamp((1 + MOUSE_ACCEL_TIMER), 1, MOUSE_MAX_SPEED)
                    mouse.move(y = -int(speed))
                elif Buttons[2].state == ButtonStates.JUST_RELEASED_HOLD:
                    MOUSE_ACCEL_TIMER = 0

                #B4 - Right Click, also right click and drag.
                elif Buttons[3].state == ButtonStates.JUST_RELEASED_PRESS:
                    if RIGHT_MOUSE_HELD:
                        mouse.release(Mouse.RIGHT_BUTTON)
                        RIGHT_MOUSE_HELD = False
                    else:
                        mouse.click(Mouse.RIGHT_BUTTON)
                elif Buttons[3].state == ButtonStates.JUST_RELEASED_HOLD:
                    if not RIGHT_MOUSE_HELD:
                        RIGHT_MOUSE_HELD = True
                        mouse.press(Mouse.RIGHT_BUTTON)
                    else:
                        RIGHT_MOUSE_HELD = False
                        mouse.release(Mouse.RIGHT_BUTTON)


                #A1 - Change Mode (and also tone of sounds)
                elif Buttons[4].state == ButtonStates.JUST_RELEASED_PRESS:
                    MODE = 0
                elif Buttons[4].state == ButtonStates.JUST_RELEASED_HOLD:
                    MODE += 1
                    if MODE > 2:
                        MODE = 0


                #A2 - Go to the Right
                elif Buttons[5].state == ButtonStates.JUST_RELEASED_PRESS:
                    mouse.move(x=-SHORT_MOUSE_MOVE)
                elif Buttons[5].state == ButtonStates.DOWN:
                    MOUSE_ACCEL_TIMER += MOUSE_ACCEL_SPEED
                    speed = clamp((1 + MOUSE_ACCEL_TIMER), 1, MOUSE_MAX_SPEED)
                    mouse.move(x = -int(speed))
                elif Buttons[5].state == ButtonStates.JUST_RELEASED_HOLD:
                    MOUSE_ACCEL_TIMER = 0

                #A3 - Tap to Left Click, Hold to Hold L-Mouse
                elif Buttons[6].state == ButtonStates.JUST_RELEASED_PRESS:
                    if LEFT_MOUSE_HELD:
                        mouse.release(Mouse.LEFT_BUTTON)
                        LEFT_MOUSE_HELD = False
                    else:
                        mouse.click(Mouse.LEFT_BUTTON)
                elif Buttons[6].state == ButtonStates.JUST_RELEASED_HOLD:
                    if not LEFT_MOUSE_HELD:
                        LEFT_MOUSE_HELD = True
                        mouse.press(Mouse.LEFT_BUTTON)
                    else:
                        LEFT_MOUSE_HELD = False
                        mouse.release(Mouse.LEFT_BUTTON)

                #A4 - Goes Right
                elif Buttons[7].state == ButtonStates.JUST_RELEASED_PRESS:
                    mouse.move(x=SHORT_MOUSE_MOVE)
                elif Buttons[7].state == ButtonStates.DOWN:
                    MOUSE_ACCEL_TIMER += MOUSE_ACCEL_SPEED
                    speed = clamp((1 + MOUSE_ACCEL_TIMER), 1, MOUSE_MAX_SPEED)
                    mouse.move(x = int(speed))
                elif Buttons[7].state == ButtonStates.JUST_RELEASED_HOLD:
                    MOUSE_ACCEL_TIMER = 0

                #A5 - Scroll wheel, press to change directions (and scroll a little), hold to scroll
                elif Buttons[8].state == ButtonStates.JUST_RELEASED_PRESS:
                    SCROLL_UP = not SCROLL_UP #Toggle the direction we'll scroll and indicate it by doing it.
                    if SCROLL_UP:
                        mouse.move(wheel=3)
                    else:
                         mouse.move(wheel=-3)
                    time.sleep(0.2)
                elif Buttons[8].state == ButtonStates.DOWN:
                    if SCROLL_UP:
                        mouse.move(wheel=1)
                    else:
                        mouse.move(wheel=-1)
                    time.sleep(0.1)

            #TODO: Make a new mode and add a bunch of new functions. As well as more clear feedback we're in a new mode.
            elif False: #MODE == 2:
                True
        else: #switch is off, we're debugging
            message = ""

            for i in range(len(Buttons)):
                message += str(Buttons[i].state)
                message += "\t"

            print(message)


        time.sleep(0.001)  # debounce delay