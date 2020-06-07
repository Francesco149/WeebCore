#include <GL/glx.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <immintrin.h>
#include <errno.h>

static void gInit();

struct _OSWindow {
  Display* dpy;
  Window ptr;
  Colormap colorMap;
  GLXContext gl;
  int width, height;
  int mouseX, mouseY;
  float minFrameTime;
  struct timespec lastTime, now;
  float deltaTime;
  int messageType;
  union _OSMessageData {
    struct _OSKeyMessageData {
      int code;
      int state;
    } key;
    struct _OSMouseMessageData {
      int dx, dy;
    } mouse;
  } u;
};

static void osDie(char* fmt, ...) {
  va_list va;
  va_start(va, fmt);
  vprintf(fmt, va);
  va_end(va);
  putchar('\n');
  exit(1);
}

void* osAlloc(int n) {
  void* p = malloc(n);
  osMemSet(p, 0, n);
  return p;
}

void* osRealloc(void* p, int n) {
  return realloc(p, n);
}

void osFree(void* p) {
  free(p);
}

void osMemSet(void* p, unsigned char val, int n) {
  memset(p, val, n);
}

void osMemCpy(void* dst, void* src, int n) {
  memcpy(dst, src, n);
}

int osWriteEntireFile(char* path, void* data, int dataLen) {
  FILE* f = fopen(path, "wb");
  int res;
  if (!f) {
    return -1;
  }
  res = (int)fwrite(data, 1, dataLen, f);
  fclose(f);
  return res;
}

int osReadEntireFile(char* path, void* data, int maxSize) {
  FILE* f = fopen(path, "rb");
  int res;
  if (!f) {
    return -1;
  }
  res = (int)fread(data, 1, maxSize, f);
  fclose(f);
  return res;
}

/* pick fb config that has the the highest samples visual */
static GLXFBConfig osChooseFBConfig(Display* dpy) {
  static int attrs[] = {
    GLX_X_RENDERABLE, True,
    GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
    GLX_RENDER_TYPE, GLX_RGBA_BIT,
    GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
    GLX_RED_SIZE, 8,
    GLX_GREEN_SIZE, 8,
    GLX_BLUE_SIZE, 8,
    GLX_ALPHA_SIZE, 8,
    GLX_DEPTH_SIZE, 24,
    GLX_STENCIL_SIZE, 8,
    GLX_DOUBLEBUFFER, True,
    None
  };
  int n, i, bestSamples = -1, sampleBufs, samples;
  GLXFBConfig res = 0, *configs;

  configs = glXChooseFBConfig(dpy, DefaultScreen(dpy), attrs, &n);
  if (!n || !configs) { return 0; }
  for (i = 0; i < n; ++i) {
    glXGetFBConfigAttrib(dpy, configs[i], GLX_SAMPLE_BUFFERS, &sampleBufs);
    glXGetFBConfigAttrib(dpy, configs[i], GLX_SAMPLES, &samples);
    if (bestSamples < 0 || (sampleBufs && samples > bestSamples)) {
      bestSamples = samples;
      res = configs[i];
    }
  }
  XFree(configs);
  return res;
}

static void osSetAtom(OSWindow window, char* name, char* valueName) {
  Atom atom = XInternAtom(window->dpy, name, True);
  Atom value = XInternAtom(window->dpy, valueName, True);
  if (atom != None && value != None) {
    XChangeProperty(window->dpy, window->ptr, atom, XA_ATOM, 32,
      PropModeReplace, (unsigned char*)&value, 1);
  }
}

static void osSetWMProtocol(OSWindow window, char* name) {
  Atom wmProtocol = XInternAtom(window->dpy, name, False);
  XSetWMProtocols(window->dpy, window->ptr, &wmProtocol, 1);
}

static void osTimeDelta(struct timespec* delta, struct timespec* a, struct timespec* b) {
  if (a->tv_nsec > b->tv_nsec) {
    delta->tv_sec = b->tv_sec - a->tv_sec - 1;
    delta->tv_nsec = 1000000000 + b->tv_nsec - a->tv_nsec;
  } else {
    delta->tv_sec = b->tv_sec - a->tv_sec;
    delta->tv_nsec = b->tv_nsec - a->tv_nsec;
  }
}

static void osUpdateTime(OSWindow window) {
  struct timespec delta;
  if (clock_gettime(CLOCK_MONOTONIC, &window->now) == -1) {
    osDie("clock_gettime failed: %s\n", strerror(errno));
  }
  osTimeDelta(&delta, &window->lastTime, &window->now);
  window->deltaTime = delta.tv_sec + delta.tv_nsec * 1e-9f;
}

OSWindow osCreateWindow() {
  OSWindow window = osAlloc(sizeof(struct _OSWindow));
  Window rootwin;
  XSetWindowAttributes swa;
  GLXFBConfig fb;
  XVisualInfo* vi;

  window->dpy = XOpenDisplay(0);
  if (!window->dpy) {
    osDie("no display");
  }

  fb = osChooseFBConfig(window->dpy);
  if (!fb) {
    osDie("couldn't find appropriate fb config");
  }

  rootwin = DefaultRootWindow(window->dpy);
  vi = glXGetVisualFromFBConfig(window->dpy, fb);

  swa.event_mask = ExposureMask | StructureNotifyMask | KeyPressMask | ButtonPressMask |
    KeyReleaseMask | ButtonReleaseMask | PointerMotionMask;

  swa.colormap = window->colorMap = XCreateColormap(window->dpy, rootwin, vi->visual, AllocNone);

  window->width = 640;
  window->height = 480;
  window->ptr = XCreateWindow(window->dpy, rootwin, 0, 0, window->width, window->height, 0,
    vi->depth, InputOutput, vi->visual, CWColormap | CWEventMask, &swa);

  /* tell window managers we'd like this window to float if possible */
  osSetAtom(window, "_NET_WM_WINDOW_TYPE", "_NET_WM_WINDOW_TYPE_UTILITY");

  /* capture delete event so closing the window actually stops msgloop */
  osSetWMProtocol(window, "WM_DELETE_WINDOW");

  osSetWindowName(window, "WeebCoreX11");
  osSetWindowClass(window, "WeebCore");

  window->gl = glXCreateNewContext(window->dpy, fb, GLX_RGBA_TYPE, 0, True);
  glXMakeCurrent(window->dpy, window->ptr, window->gl);

  XMapWindow(window->dpy, window->ptr);
  XFree(vi);

  osSetWindowFPS(window, 10000);
  osUpdateTime(window);
  osMemCpy(&window->lastTime, &window->now, sizeof(window->lastTime));
  window->deltaTime = window->minFrameTime;

  gInit();

  return window;
}

void osDestroyWindow(OSWindow window) {
  glXMakeCurrent(window->dpy, 0, 0);
  glXDestroyContext(window->dpy, window->gl);
  XDestroyWindow(window->dpy, window->ptr);
  XFreeColormap(window->dpy, window->colorMap);
  XCloseDisplay(window->dpy);
  osFree(window);
}

void osSetWindowSize(OSWindow window, int width, int height) {
  XResizeWindow(window->dpy, window->ptr, width, height);
}

void osSetWindowName(OSWindow window, char* windowName) {
  XStoreName(window->dpy, window->ptr, windowName);
}

void osSetWindowClass(OSWindow window, char* className) {
  XClassHint* classHints = XAllocClassHint();
  classHints->res_name = className;
  classHints->res_class = "WeebCoreX11";
  XSetClassHint(window->dpy, window->ptr, classHints);
  XFree(classHints);
}

void osSetWindowFPS(OSWindow window, int fps) {
  window->minFrameTime = 1.0f / (fps ? fps : 10000);
}

float osDeltaTime(OSWindow window) {
  return window->deltaTime;
}

int osWindowWidth(OSWindow window) {
  return window->width;
}

int osWindowHeight(OSWindow window) {
  return window->height;
}

/* keycode list adapted from glfw */
/* TODO: use Xkb to handle keyboard layouts? */
static int osTranslateKey(Display* dpy, int xkeycode) {
  int sym, symsPerKey;
  KeySym* syms = XGetKeyboardMapping(dpy, xkeycode, 1, &symsPerKey);
  sym = syms[0];
  XFree(syms);
  switch (sym) {
    case XK_Escape:           return OS_ESCAPE;
    case XK_Tab:              return OS_TAB;
    case XK_Shift_L:          return OS_LEFT_SHIFT;
    case XK_Shift_R:          return OS_RIGHT_SHIFT;
    case XK_Control_L:        return OS_LEFT_CONTROL;
    case XK_Control_R:        return OS_RIGHT_CONTROL;
    case XK_Meta_L:
    case XK_Alt_L:            return OS_LEFT_ALT;
    case XK_Mode_switch:      /* mapped to Alt_R on many keyboards */
    case XK_ISO_Level3_Shift: /* AltGr on at least some machines */
    case XK_Meta_R:
    case XK_Alt_R:            return OS_RIGHT_ALT;
    case XK_Super_L:          return OS_LEFT_SUPER;
    case XK_Super_R:          return OS_RIGHT_SUPER;
    case XK_Menu:             return OS_MENU;
    case XK_Num_Lock:         return OS_NUM_LOCK;
    case XK_Caps_Lock:        return OS_CAPS_LOCK;
    case XK_Print:            return OS_PRINT_SCREEN;
    case XK_Scroll_Lock:      return OS_SCROLL_LOCK;
    case XK_Pause:            return OS_PAUSE;
    case XK_Delete:           return OS_DELETE;
    case XK_BackSpace:        return OS_BACKSPACE;
    case XK_Return:           return OS_ENTER;
    case XK_Home:             return OS_HOME;
    case XK_End:              return OS_END;
    case XK_Page_Up:          return OS_PAGE_UP;
    case XK_Page_Down:        return OS_PAGE_DOWN;
    case XK_Insert:           return OS_INSERT;
    case XK_Left:             return OS_LEFT;
    case XK_Right:            return OS_RIGHT;
    case XK_Down:             return OS_DOWN;
    case XK_Up:               return OS_UP;
    case XK_F1:               return OS_F1;
    case XK_F2:               return OS_F2;
    case XK_F3:               return OS_F3;
    case XK_F4:               return OS_F4;
    case XK_F5:               return OS_F5;
    case XK_F6:               return OS_F6;
    case XK_F7:               return OS_F7;
    case XK_F8:               return OS_F8;
    case XK_F9:               return OS_F9;
    case XK_F10:              return OS_F10;
    case XK_F11:              return OS_F11;
    case XK_F12:              return OS_F12;
    case XK_F13:              return OS_F13;
    case XK_F14:              return OS_F14;
    case XK_F15:              return OS_F15;
    case XK_F16:              return OS_F16;
    case XK_F17:              return OS_F17;
    case XK_F18:              return OS_F18;
    case XK_F19:              return OS_F19;
    case XK_F20:              return OS_F20;
    case XK_F21:              return OS_F21;
    case XK_F22:              return OS_F22;
    case XK_F23:              return OS_F23;
    case XK_F24:              return OS_F24;
    case XK_F25:              return OS_F25;

    /* numeric keypad */
    case XK_KP_Divide:        return OS_KP_DIVIDE;
    case XK_KP_Multiply:      return OS_KP_MULTIPLY;
    case XK_KP_Subtract:      return OS_KP_SUBTRACT;
    case XK_KP_Add:           return OS_KP_ADD;

    /* if I add xkb support, this will be a fallback to a fixed layout */
    case XK_KP_Insert:        return OS_KP_0;
    case XK_KP_End:           return OS_KP_1;
    case XK_KP_Down:          return OS_KP_2;
    case XK_KP_Page_Down:     return OS_KP_3;
    case XK_KP_Left:          return OS_KP_4;
    case XK_KP_Right:         return OS_KP_6;
    case XK_KP_Home:          return OS_KP_7;
    case XK_KP_Up:            return OS_KP_8;
    case XK_KP_Page_Up:       return OS_KP_9;
    case XK_KP_Delete:        return OS_KP_DECIMAL;
    case XK_KP_Equal:         return OS_KP_EQUAL;
    case XK_KP_Enter:         return OS_KP_ENTER;

    /* printable keys */
    case XK_a:                return OS_A;
    case XK_b:                return OS_B;
    case XK_c:                return OS_C;
    case XK_d:                return OS_D;
    case XK_e:                return OS_E;
    case XK_f:                return OS_F;
    case XK_g:                return OS_G;
    case XK_h:                return OS_H;
    case XK_i:                return OS_I;
    case XK_j:                return OS_J;
    case XK_k:                return OS_K;
    case XK_l:                return OS_L;
    case XK_m:                return OS_M;
    case XK_n:                return OS_N;
    case XK_o:                return OS_O;
    case XK_p:                return OS_P;
    case XK_q:                return OS_Q;
    case XK_r:                return OS_R;
    case XK_s:                return OS_S;
    case XK_t:                return OS_T;
    case XK_u:                return OS_U;
    case XK_v:                return OS_V;
    case XK_w:                return OS_W;
    case XK_x:                return OS_X;
    case XK_y:                return OS_Y;
    case XK_z:                return OS_Z;
    case XK_1:                return OS_1;
    case XK_2:                return OS_2;
    case XK_3:                return OS_3;
    case XK_4:                return OS_4;
    case XK_5:                return OS_5;
    case XK_6:                return OS_6;
    case XK_7:                return OS_7;
    case XK_8:                return OS_8;
    case XK_9:                return OS_9;
    case XK_0:                return OS_0;
    case XK_space:            return OS_SPACE;
    case XK_minus:            return OS_MINUS;
    case XK_equal:            return OS_EQUAL;
    case XK_bracketleft:      return OS_LEFT_BRACKET;
    case XK_bracketright:     return OS_RIGHT_BRACKET;
    case XK_backslash:        return OS_BACKSLASH;
    case XK_semicolon:        return OS_SEMICOLON;
    case XK_apostrophe:       return OS_APOSTROPHE;
    case XK_grave:            return OS_GRAVE_ACCENT;
    case XK_comma:            return OS_COMMA;
    case XK_period:           return OS_PERIOD;
    case XK_slash:            return OS_SLASH;
    case XK_less:             return OS_WORLD_1;
  }
  return -1;
}

static int osTranslateKeyState(int state) {
  int res = 0;
  if (state & ShiftMask) res |= OS_FSHIFT;
  if (state & ControlMask) res |= OS_FCONTROL;
  if (state & Mod1Mask) res |= OS_FALT;
  if (state & Mod4Mask) res |= OS_FSUPER;
  if (state & LockMask) res |= OS_FCAPS_LOCK;
  if (state & Mod2Mask) res |= OS_FNUM_LOCK;
  return res;
}

int osMessageType(OSWindow window) {
  return window->messageType;
}

int osKey(OSWindow window) {
  return window->u.key.code;
}

int osKeyState(OSWindow window) {
  return window->u.key.state;
}

int osMouseX(OSWindow window) {
  return window->mouseX;
}

int osMouseY(OSWindow window) {
  return window->mouseY;
}

int osMouseDX(OSWindow window) {
  return window->u.mouse.dx;
}

int osMouseDY(OSWindow window) {
  return window->u.mouse.dy;
}

int osNextMessage(OSWindow window) {
  XEvent xev;
  Atom deleteMessage = XInternAtom(window->dpy, "WM_DELETE_WINDOW", False);
  while (window->messageType != OS_QUIT && XPending(window->dpy)) {
    XNextEvent(window->dpy, &xev);
    osMemSet(&window->u, 0, sizeof(window->u));
    switch (xev.type) {
      case ClientMessage: {
        if (xev.xclient.data.l[0] == deleteMessage) {
          osPostQuitMessage(window);
        }
        return 1;
      }
      case ConfigureNotify: {
        window->messageType = OS_SIZE;
        window->width = xev.xconfigure.width;
        window->height = xev.xconfigure.height;
        gViewport(window, 0, 0,
          xev.xconfigure.width, xev.xconfigure.height);
        return 1;
      }
      case KeyPress: {
        window->messageType = OS_KEYDOWN;
        window->u.key.code = osTranslateKey(window->dpy, xev.xkey.keycode);
        window->u.key.state |= osTranslateKeyState(xev.xkey.state);
        return 1;
      }
      case KeyRelease: {
        XEvent nextXev;
        window->u.key.code = osTranslateKey(window->dpy, xev.xkey.keycode);
        window->u.key.state |= osTranslateKeyState(xev.xkey.state);
        /* anything released for < 20ms is a repeat */
        /* TODO: Xkb can detect this without guessing */
        if (XEventsQueued(window->dpy, QueuedAfterReading)) {
          int is_repeat;
          XPeekEvent(window->dpy, &nextXev);
          is_repeat = (
            nextXev.type == KeyPress &&
            nextXev.xkey.window == xev.xkey.window &&
            nextXev.xkey.keycode == xev.xkey.keycode &&
            nextXev.xkey.time - xev.xkey.time < 20
          );
          if (is_repeat) {
            window->u.key.state |= OS_FREPEAT;
            window->u.key.state |= osTranslateKeyState(nextXev.xkey.state);
          }
        }
        if (window->u.key.state & OS_FREPEAT) {
          XNextEvent(window->dpy, &nextXev); /* swallow the repeat press */
          window->messageType = OS_KEYDOWN;
        } else {
          window->messageType = OS_KEYUP;
        }
        return 1;
      }
      case ButtonPress: {
        window->messageType = OS_KEYDOWN;
        window->u.key.code = xev.xbutton.button;
        window->u.key.state |= osTranslateKeyState(xev.xkey.state);
        return 1;
      }
      case ButtonRelease: {
        window->messageType = OS_KEYUP;
        window->u.key.code = xev.xbutton.button;
        window->u.key.state |= osTranslateKeyState(xev.xkey.state);
        return 1;
      }
      case MotionNotify: {
        window->messageType = OS_MOTION;
        window->u.mouse.dx = xev.xmotion.x - window->mouseX;
        window->u.mouse.dy = xev.xmotion.y - window->mouseY;
        window->mouseX = xev.xmotion.x;
        window->mouseY = xev.xmotion.y;
        return 1;
      }
    }
  }
  return window->messageType == OS_QUIT;
}

void osPostQuitMessage(OSWindow window) {
  window->messageType = OS_QUIT;
}

static void osSwapBuffers(OSWindow window) {
  glXSwapBuffers(window->dpy, window->ptr);

  osUpdateTime(window);
  while (osDeltaTime(window) < window->minFrameTime) {
    osUpdateTime(window);
    _mm_pause();
  }

  if (window->deltaTime == 0) {
    osDie("deltaTime was zero, something is very wrong with the timer.");
  }

  osMemCpy(&window->lastTime, &window->now, sizeof(window->lastTime));
}

#include "Platform/OpenGL.c"
