/* Ripped shamelessly from: http://emg-2.blogspot.com/2008/01/xfree86xorg-keylogger.html

   compile with:
   g++ -o Space2Ctrl Space2Ctrl.cpp -W -Wall -L/usr/X11R6/lib -lX11 -lXtst

   To install libx11:
   in Ubuntu: sudo apt-get install libx11-dev

   To install libXTst:
   in Ubuntu: sudo apt-get install libxtst-dev

   Needs module XRecord installed. To install it, add line Load "record" to Section "Module" in
   /etc/X11/xorg.conf like this:

   Section "Module"
   Load  "record"
   EndSection

*/

#include <chrono>
#include <csignal>
#include <iostream>

#include <X11/Xlibint.h>
#include <X11/extensions/XTest.h>
#include <X11/extensions/record.h>
#include <X11/keysym.h>

struct CallbackClosure {
  Display* ctrlDisplay;
  Display* dataDisplay;
  int curX;
  int curY;
  void* initialObject;
};

typedef union {
  unsigned char type;
  xEvent event;
  xResourceReq req;
  xGenericReply reply;
  xError error;
  xConnSetupPrefix setup;
} XRecordDatum;

class Space2Ctrl {
  std::string m_displayName;
  CallbackClosure userData;
  std::pair<int, int> recVer;
  XRecordRange* recRange;
  XRecordClientSpec recClientSpec;
  XRecordContext recContext;

  void setupXTestExtension() {
    int ev, er, ma, mi;
    if (!XTestQueryExtension(userData.ctrlDisplay, &ev, &er, &ma, &mi)) {
      std::cout << "There is no XTest extension loaded to X server." << std::endl;
      throw std::exception();
    }
  }

  void setupRecordExtension() {
    XSynchronize(userData.ctrlDisplay, True);

    // Record extension exists?
    if (!XRecordQueryVersion(userData.ctrlDisplay, &recVer.first, &recVer.second)) {
      std::cout << R"(There is no RECORD extension loaded to X server.
You must add following line:
   Load  \"record\"
to /etc/X11/xorg.conf, in section `Module'.)"
                << std::endl;
      throw std::exception();
    }

    recRange = XRecordAllocRange();
    if (!recRange) {
      // "Could not alloc record range object!\n";
      throw std::exception();
    }
    recRange->device_events.first = KeyPress;
    recRange->device_events.last = ButtonRelease;
    recClientSpec = XRecordAllClients;

    // Get context with our configuration
    recContext = XRecordCreateContext(userData.ctrlDisplay, 0, &recClientSpec, 1, &recRange, 1);
    if (!recContext) {
      std::cout << "Could not create a record context!" << std::endl;
      throw std::exception();
    }
  }

  // Called from Xserver when new event occurs.
  static void eventCallback(XPointer priv, XRecordInterceptData* hook) {
    if (hook->category != XRecordFromServer) {
      XRecordFreeData(hook);
      return;
    }

    CallbackClosure* userData = (CallbackClosure*)priv;
    XRecordDatum* data = (XRecordDatum*)hook->data;
    static bool space_down = false;
    static bool key_combo = false;
    static bool modifier_down = false;
    static struct timeval startWait, endWait;

    unsigned char t = data->event.u.u.type;
    int c = data->event.u.u.detail;

    auto pressed_at = std::chrono::system_clock::now();
    switch (t) {
      case KeyPress: {
        if (c == 65) {
          space_down = true;
          pressed_at = std::chrono::system_clock::now();

        } else if ((c == XKeysymToKeycode(userData->ctrlDisplay, XK_Control_L)) ||
                   (c == XKeysymToKeycode(userData->ctrlDisplay, XK_Control_R))) {
          // ctrl pressed
          if (space_down) {  // space ctrl sequence
            XTestFakeKeyEvent(userData->ctrlDisplay, 255, True, CurrentTime);
            XTestFakeKeyEvent(userData->ctrlDisplay, 255, False, CurrentTime);
          }
        } else if ((c == XKeysymToKeycode(userData->ctrlDisplay, XK_Shift_L)) ||
                   (c == XKeysymToKeycode(userData->ctrlDisplay, XK_Shift_R)) || (c == 108)) {
          // cout << "    Modifier" << endl;
          // TODO: Find a better way to get those modifiers!!!
          modifier_down = true;

        } else {  // another key pressed
          if (space_down) {
            key_combo = true;
          } else {
            key_combo = false;
          }
        }

        break;
      }
      case KeyRelease: {
        if (c == 65) {
          space_down = false;  // space released

          if (!key_combo && !modifier_down) {
            const auto released_at = std::chrono::system_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(released_at - pressed_at) <
                std::chrono::milliseconds(600)) {
              // if minimum timeout elapsed since space was pressed
              XTestFakeKeyEvent(userData->ctrlDisplay, 255, True, CurrentTime);
              XTestFakeKeyEvent(userData->ctrlDisplay, 255, False, CurrentTime);
            }
          }
          key_combo = false;
          // cout << endl;
        } else if ((c == XKeysymToKeycode(userData->ctrlDisplay, XK_Control_L)) ||
                   (c == XKeysymToKeycode(userData->ctrlDisplay, XK_Control_R))) {
          // ctrl release
          if (space_down) key_combo = true;
        } else if ((c == XKeysymToKeycode(userData->ctrlDisplay, XK_Shift_L)) ||
                   (c == XKeysymToKeycode(userData->ctrlDisplay, XK_Shift_R)) || (c == 108)) {
          // TODO: Find a better way to get those modifiers!!!
          modifier_down = false;
        }

        break;
      }
      case ButtonPress: {
        if (space_down) {
          key_combo = true;
        } else {
          key_combo = false;
        }

        break;
      }
    }

    XRecordFreeData(hook);
  }

public:
  Space2Ctrl() { }
  ~Space2Ctrl() {
    stop();
  }

  bool connect(std::string displayName) {
    m_displayName = displayName;
    if (NULL == (userData.ctrlDisplay = XOpenDisplay(m_displayName.c_str()))) {
      return false;
    }
    if (NULL == (userData.dataDisplay = XOpenDisplay(m_displayName.c_str()))) {
      XCloseDisplay(userData.ctrlDisplay);
      userData.ctrlDisplay = NULL;
      return false;
    }

    // You may want to set custom X error handler here

    userData.initialObject = (void*)this;
    setupXTestExtension();
    setupRecordExtension();

    return true;
  }

  void start() {
    // // Remap keycode 255 to Keysym space:
    // KeySym spc = XK_space;
    // XChangeKeyboardMapping(userData.ctrlDisplay, 255, 1, &spc, 1);
    // XFlush(userData.ctrlDisplay);

    // TODO: document why the following event is needed
    XTestFakeKeyEvent(userData.ctrlDisplay, 255, True, CurrentTime);
    XTestFakeKeyEvent(userData.ctrlDisplay, 255, False, CurrentTime);

    if (!XRecordEnableContext(userData.dataDisplay, recContext, eventCallback,
                              (XPointer)&userData)) {
      throw std::exception();
    }
  }

  void stop() {
    if (!XRecordDisableContext(userData.ctrlDisplay, recContext)) {
      throw std::exception();
    }
  }
};

Space2Ctrl* space2ctrl;

void stop(int param) {
  delete space2ctrl;
  if (param == SIGTERM) std::cout << "-- Terminating Space2Ctrl --" << std::endl;
  exit(1);
}

int main() {
  char* var_value;
  std::string display_env = (var_value = std::getenv("DISPLAY")) ? std::string(var_value) : ":0";
  std::cout << "-- Starting Space2Ctrl using display " << display_env << " --\n";

  space2ctrl = new Space2Ctrl();

  void (*prev_fn)(int);

  prev_fn = signal(SIGTERM, stop);
  if (prev_fn == SIG_IGN) signal(SIGTERM, SIG_IGN);

  if (space2ctrl->connect(":1")) {
    space2ctrl->start();
  } else {
    std::cout << "-- Error: Could not connect to display. Not started --" << std::endl;
  }

  return 0;
}
