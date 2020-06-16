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

static void GrInit();

struct _Wnd {
  Display* dpy;
  Window ptr;
  Colormap colorMap;
  GLXContext gl;
  int width, height;
  int mouseX, mouseY;
  float minFrameTime;
  struct timespec lastTime, now;
  float delta;
  int msgType;
  union _MsgData {
    struct _KeyMsgData {
      int code;
      int state;
    } key;
    struct _MouseMsgData {
      int dx, dy;
    } mouse;
  } u;
};

static void Die(char* fmt, ...) {
  va_list va;
  va_start(va, fmt);
  vprintf(fmt, va);
  va_end(va);
  putchar('\n');
  exit(1);
}

void* Alloc(int n) {
  void* p = malloc(n);
  MemSet(p, 0, n);
  return p;
}

void* Realloc(void* p, int n) {
  return realloc(p, n);
}

void Free(void* p) {
  free(p);
}

void MemSet(void* p, unsigned char val, int n) {
  memset(p, val, n);
}

void MemCpy(void* dst, void* src, int n) {
  memcpy(dst, src, n);
}

int WriteFile(char* path, void* data, int dataLen) {
  FILE* f = fopen(path, "wb");
  int res;
  if (!f) {
    return -1;
  }
  res = (int)fwrite(data, 1, dataLen, f);
  fclose(f);
  return res;
}

int ReadFile(char* path, void* data, int maxSize) {
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
static GLXFBConfig PickBufCfg(Display* dpy) {
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

static void SetAtom(Wnd wnd, char* name, char* valueName) {
  Atom atom = XInternAtom(wnd->dpy, name, True);
  Atom value = XInternAtom(wnd->dpy, valueName, True);
  if (atom != None && value != None) {
    XChangeProperty(wnd->dpy, wnd->ptr, atom, XA_ATOM, 32,
      PropModeReplace, (unsigned char*)&value, 1);
  }
}

static void SetWMProtocol(Wnd wnd, char* name) {
  Atom wmProtocol = XInternAtom(wnd->dpy, name, False);
  XSetWMProtocols(wnd->dpy, wnd->ptr, &wmProtocol, 1);
}

static void TSpecDelta(struct timespec* delta, struct timespec* a, struct timespec* b) {
  if (a->tv_nsec > b->tv_nsec) {
    delta->tv_sec = b->tv_sec - a->tv_sec - 1;
    delta->tv_nsec = 1000000000 + b->tv_nsec - a->tv_nsec;
  } else {
    delta->tv_sec = b->tv_sec - a->tv_sec;
    delta->tv_nsec = b->tv_nsec - a->tv_nsec;
  }
}

static void UpdateTime(Wnd wnd) {
  struct timespec delta;
  if (clock_gettime(CLOCK_MONOTONIC, &wnd->now) == -1) {
    Die("clock_gettime failed: %s\n", strerror(errno));
  }
  TSpecDelta(&delta, &wnd->lastTime, &wnd->now);
  wnd->delta = delta.tv_sec + delta.tv_nsec * 1e-9f;
}

Wnd MkWnd() {
  Wnd wnd = Alloc(sizeof(struct _Wnd));
  Window rootwin;
  XSetWindowAttributes swa;
  GLXFBConfig fb;
  XVisualInfo* vi;

  wnd->dpy = XOpenDisplay(0);
  if (!wnd->dpy) {
    Die("no display");
  }

  fb = PickBufCfg(wnd->dpy);
  if (!fb) {
    Die("couldn't find appropriate fb config");
  }

  rootwin = DefaultRootWindow(wnd->dpy);
  vi = glXGetVisualFromFBConfig(wnd->dpy, fb);

  swa.event_mask = ExposureMask | StructureNotifyMask | KeyPressMask | ButtonPressMask |
    KeyReleaseMask | ButtonReleaseMask | PointerMotionMask;

  swa.colormap = wnd->colorMap = XCreateColormap(wnd->dpy, rootwin, vi->visual, AllocNone);

  wnd->width = 640;
  wnd->height = 480;
  wnd->ptr = XCreateWindow(wnd->dpy, rootwin, 0, 0, wnd->width, wnd->height, 0,
    vi->depth, InputOutput, vi->visual, CWColormap | CWEventMask, &swa);

  /* tell wnd managers we'd like this wnd to float if possible */
  SetAtom(wnd, "_NET_WM_WINDOW_TYPE", "_NET_WM_WINDOW_TYPE_UTILITY");

  /* capture delete event so closing the wnd actually stops msgloop */
  SetWMProtocol(wnd, "WM_DELETE_WINDOW");

  SetWndName(wnd, "WeebCoreX11");
  SetWndClass(wnd, "WeebCore");

  wnd->gl = glXCreateNewContext(wnd->dpy, fb, GLX_RGBA_TYPE, 0, True);
  glXMakeCurrent(wnd->dpy, wnd->ptr, wnd->gl);

  XMapWindow(wnd->dpy, wnd->ptr);
  XFree(vi);

  SetWndFPS(wnd, 10000);
  UpdateTime(wnd);
  MemCpy(&wnd->lastTime, &wnd->now, sizeof(wnd->lastTime));
  wnd->delta = wnd->minFrameTime;

  GrInit();

  return wnd;
}

void RmWnd(Wnd wnd) {
  glXMakeCurrent(wnd->dpy, 0, 0);
  glXDestroyContext(wnd->dpy, wnd->gl);
  XDestroyWindow(wnd->dpy, wnd->ptr);
  XFreeColormap(wnd->dpy, wnd->colorMap);
  XCloseDisplay(wnd->dpy);
  Free(wnd);
}

void SetWndSize(Wnd wnd, int width, int height) {
  XResizeWindow(wnd->dpy, wnd->ptr, width, height);
}

void SetWndName(Wnd wnd, char* wndName) {
  XStoreName(wnd->dpy, wnd->ptr, wndName);
}

void SetWndClass(Wnd wnd, char* className) {
  XClassHint* classHints = XAllocClassHint();
  classHints->res_name = className;
  classHints->res_class = "WeebCoreX11";
  XSetClassHint(wnd->dpy, wnd->ptr, classHints);
  XFree(classHints);
}

void SetWndFPS(Wnd wnd, int fps) {
  wnd->minFrameTime = 1.0f / (fps ? fps : 10000);
}

float Delta(Wnd wnd) {
  return wnd->delta;
}

int WndWidth(Wnd wnd) {
  return wnd->width;
}

int WndHeight(Wnd wnd) {
  return wnd->height;
}

/* keycode list adapted from glfw */
/* TODO: use Xkb to handle keyboard layouts? */
static int ConvKey(Display* dpy, int xkeycode) {
  int sym, symsPerKey;
  KeySym* syms = XGetKeyboardMapping(dpy, xkeycode, 1, &symsPerKey);
  sym = syms[0];
  XFree(syms);
  switch (sym) {
    case XK_Escape:           return ESCAPE;
    case XK_Tab:              return TAB;
    case XK_Shift_L:          return LEFT_SHIFT;
    case XK_Shift_R:          return RIGHT_SHIFT;
    case XK_Control_L:        return LEFT_CONTROL;
    case XK_Control_R:        return RIGHT_CONTROL;
    case XK_Meta_L:
    case XK_Alt_L:            return LEFT_ALT;
    case XK_Mode_switch:      /* mapped to Alt_R on many keyboards */
    case XK_ISO_Level3_Shift: /* AltGr on at least some machines */
    case XK_Meta_R:
    case XK_Alt_R:            return RIGHT_ALT;
    case XK_Super_L:          return LEFT_SUPER;
    case XK_Super_R:          return RIGHT_SUPER;
    case XK_Menu:             return MENU;
    case XK_Num_Lock:         return NUM_LOCK;
    case XK_Caps_Lock:        return CAPS_LOCK;
    case XK_Print:            return PRINT_SCREEN;
    case XK_Scroll_Lock:      return SCROLL_LOCK;
    case XK_Pause:            return PAUSE;
    case XK_Delete:           return DELETE;
    case XK_BackSpace:        return BACKSPACE;
    case XK_Return:           return ENTER;
    case XK_Home:             return HOME;
    case XK_End:              return END;
    case XK_Page_Up:          return PAGE_UP;
    case XK_Page_Down:        return PAGE_DOWN;
    case XK_Insert:           return INSERT;
    case XK_Left:             return LEFT;
    case XK_Right:            return RIGHT;
    case XK_Down:             return DOWN;
    case XK_Up:               return UP;
    case XK_F1:               return F1;
    case XK_F2:               return F2;
    case XK_F3:               return F3;
    case XK_F4:               return F4;
    case XK_F5:               return F5;
    case XK_F6:               return F6;
    case XK_F7:               return F7;
    case XK_F8:               return F8;
    case XK_F9:               return F9;
    case XK_F10:              return F10;
    case XK_F11:              return F11;
    case XK_F12:              return F12;
    case XK_F13:              return F13;
    case XK_F14:              return F14;
    case XK_F15:              return F15;
    case XK_F16:              return F16;
    case XK_F17:              return F17;
    case XK_F18:              return F18;
    case XK_F19:              return F19;
    case XK_F20:              return F20;
    case XK_F21:              return F21;
    case XK_F22:              return F22;
    case XK_F23:              return F23;
    case XK_F24:              return F24;
    case XK_F25:              return F25;

    /* numeric keypad */
    case XK_KP_Divide:        return KP_DIVIDE;
    case XK_KP_Multiply:      return KP_MULTIPLY;
    case XK_KP_Subtract:      return KP_SUBTRACT;
    case XK_KP_Add:           return KP_ADD;

    /* if I add xkb support, this will be a fallback to a fixed layout */
    case XK_KP_Insert:        return KP_0;
    case XK_KP_End:           return KP_1;
    case XK_KP_Down:          return KP_2;
    case XK_KP_Page_Down:     return KP_3;
    case XK_KP_Left:          return KP_4;
    case XK_KP_Right:         return KP_6;
    case XK_KP_Home:          return KP_7;
    case XK_KP_Up:            return KP_8;
    case XK_KP_Page_Up:       return KP_9;
    case XK_KP_Delete:        return KP_DECIMAL;
    case XK_KP_Equal:         return KP_EQUAL;
    case XK_KP_Enter:         return KP_ENTER;

    /* printable keys */
    case XK_a:                return A;
    case XK_b:                return B;
    case XK_c:                return C;
    case XK_d:                return D;
    case XK_e:                return E;
    case XK_f:                return F;
    case XK_g:                return G;
    case XK_h:                return H;
    case XK_i:                return I;
    case XK_j:                return J;
    case XK_k:                return K;
    case XK_l:                return L;
    case XK_m:                return M;
    case XK_n:                return N;
    case XK_o:                return O;
    case XK_p:                return P;
    case XK_q:                return Q;
    case XK_r:                return R;
    case XK_s:                return S;
    case XK_t:                return T;
    case XK_u:                return U;
    case XK_v:                return V;
    case XK_w:                return W;
    case XK_x:                return X;
    case XK_y:                return Y;
    case XK_z:                return Z;
    case XK_1:                return K1;
    case XK_2:                return K2;
    case XK_3:                return K3;
    case XK_4:                return K4;
    case XK_5:                return K5;
    case XK_6:                return K6;
    case XK_7:                return K7;
    case XK_8:                return K8;
    case XK_9:                return K9;
    case XK_0:                return K0;
    case XK_space:            return SPACE;
    case XK_minus:            return MINUS;
    case XK_equal:            return EQUAL;
    case XK_bracketleft:      return LEFT_BRACKET;
    case XK_bracketright:     return RIGHT_BRACKET;
    case XK_backslash:        return BACKSLASH;
    case XK_semicolon:        return SEMICOLON;
    case XK_apostrophe:       return APOSTROPHE;
    case XK_grave:            return GRAVE_ACCENT;
    case XK_comma:            return COMMA;
    case XK_period:           return PERIOD;
    case XK_slash:            return SLASH;
    case XK_less:             return WORLD_1;
  }
  return -1;
}

static int ConvKeyState(int state) {
  int res = 0;
  if (state & ShiftMask) res |= FSHIFT;
  if (state & ControlMask) res |= FCONTROL;
  if (state & Mod1Mask) res |= FALT;
  if (state & Mod4Mask) res |= FSUPER;
  if (state & LockMask) res |= FCAPS_LOCK;
  if (state & Mod2Mask) res |= FNUM_LOCK;
  return res;
}

int MsgType(Wnd wnd) {
  return wnd->msgType;
}

int Key(Wnd wnd) {
  return wnd->u.key.code;
}

int KeyState(Wnd wnd) {
  return wnd->u.key.state;
}

int MouseX(Wnd wnd) {
  return wnd->mouseX;
}

int MouseY(Wnd wnd) {
  return wnd->mouseY;
}

int MouseDX(Wnd wnd) {
  return wnd->u.mouse.dx;
}

int MouseDY(Wnd wnd) {
  return wnd->u.mouse.dy;
}

int NextMsg(Wnd wnd) {
  XEvent xev;
  Atom deleteMsg = XInternAtom(wnd->dpy, "WM_DELETE_WINDOW", False);
  while (wnd->msgType != QUIT && XPending(wnd->dpy)) {
    XNextEvent(wnd->dpy, &xev);
    MemSet(&wnd->u, 0, sizeof(wnd->u));
    switch (xev.type) {
      case ClientMessage: {
        if (xev.xclient.data.l[0] == deleteMsg) {
          PostQuitMsg(wnd);
        }
        return 1;
      }
      case ConfigureNotify: {
        wnd->msgType = SIZE;
        wnd->width = xev.xconfigure.width;
        wnd->height = xev.xconfigure.height;
        Viewport(wnd, 0, 0, xev.xconfigure.width, xev.xconfigure.height);
        return 1;
      }
      case KeyPress: {
        wnd->msgType = KEYDOWN;
        wnd->u.key.code = ConvKey(wnd->dpy, xev.xkey.keycode);
        wnd->u.key.state |= ConvKeyState(xev.xkey.state);
        return 1;
      }
      case KeyRelease: {
        XEvent nextXev;
        wnd->u.key.code = ConvKey(wnd->dpy, xev.xkey.keycode);
        wnd->u.key.state |= ConvKeyState(xev.xkey.state);
        /* anything released for < 20ms is a repeat */
        /* TODO: Xkb can detect this without guessing */
        if (XEventsQueued(wnd->dpy, QueuedAfterReading)) {
          int is_repeat;
          XPeekEvent(wnd->dpy, &nextXev);
          is_repeat = (
            nextXev.type == KeyPress &&
            nextXev.xkey.window == xev.xkey.window &&
            nextXev.xkey.keycode == xev.xkey.keycode &&
            nextXev.xkey.time - xev.xkey.time < 20
          );
          if (is_repeat) {
            wnd->u.key.state |= FREPEAT;
            wnd->u.key.state |= ConvKeyState(nextXev.xkey.state);
          }
        }
        if (wnd->u.key.state & FREPEAT) {
          XNextEvent(wnd->dpy, &nextXev); /* swallow the repeat press */
          wnd->msgType = KEYDOWN;
        } else {
          wnd->msgType = KEYUP;
        }
        return 1;
      }
      case ButtonPress: {
        wnd->msgType = KEYDOWN;
        wnd->u.key.code = xev.xbutton.button;
        wnd->u.key.state |= ConvKeyState(xev.xkey.state);
        return 1;
      }
      case ButtonRelease: {
        wnd->msgType = KEYUP;
        wnd->u.key.code = xev.xbutton.button;
        wnd->u.key.state |= ConvKeyState(xev.xkey.state);
        return 1;
      }
      case MotionNotify: {
        wnd->msgType = MOTION;
        wnd->u.mouse.dx = xev.xmotion.x - wnd->mouseX;
        wnd->u.mouse.dy = xev.xmotion.y - wnd->mouseY;
        wnd->mouseX = xev.xmotion.x;
        wnd->mouseY = xev.xmotion.y;
        return 1;
      }
    }
  }
  return wnd->msgType == QUIT;
}

void PostQuitMsg(Wnd wnd) { wnd->msgType = QUIT; }

static void OsSwpBufs(Wnd wnd) {
  glXSwapBuffers(wnd->dpy, wnd->ptr);

  UpdateTime(wnd);
  while (Delta(wnd) < wnd->minFrameTime) {
    UpdateTime(wnd);
    _mm_pause();
  }

  if (wnd->delta == 0) {
    Die("delta was zero, something is very wrong with the timer.");
  }

  MemCpy(&wnd->lastTime, &wnd->now, sizeof(wnd->lastTime));
}

#include "Platform/OpenGL.c"
