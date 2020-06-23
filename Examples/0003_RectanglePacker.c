#include "WeebCore.c"

Wnd wnd;
Ft font;
Mesh help, full;
Mesh packedRects;
Packer pak;
unsigned timeElapsed;
int col;
int isFull;
float dragRect[4];

Mesh MkHelpText(Ft font) {
  Mesh mesh = MkMesh();
  Col(mesh, 0xbebebe);
  FtMesh(mesh, font, 10, 10, "Left-click and drag to create a rectangle. Release to pack it\n"
    "F2 to reset");
  return mesh;
}

Mesh MkFullText(Ft font) {
  Mesh mesh = MkMesh();
  Col(mesh, 0x663333);
  FtMesh(mesh, font, 10, 32, "Rectangle didn't fit!");
  return mesh;
}

void Init() {
  wnd = AppWnd();
  font = DefFt();
  help = MkHelpText(font);
  full = MkFullText(font);
  packedRects = MkMesh();
  pak = MkPacker(WndWidth(wnd), WndHeight(wnd));
  dragRect[0] = dragRect[2] = -1;
}

void Quit() {
  RmPacker(pak);
  RmMesh(packedRects);
  RmMesh(help);
  RmMesh(full);
  RmFt(font);
}

void KeyDown() {
  switch (Key(wnd)) {
    case MLEFT: {
      dragRect[0] = MouseX(wnd);
      dragRect[2] = MouseY(wnd);
      dragRect[1] = dragRect[0];
      dragRect[3] = dragRect[2];
      col = 0x606060 + (timeElapsed & 0x3f3f3f); /* pseudorandom col */
      break;
    }
    case F2: {
      RmMesh(packedRects);
      packedRects = MkMesh();
      RmPacker(pak);
      pak = MkPacker(WndWidth(wnd), WndHeight(wnd));
      break;
    }
  }
}

void KeyUp() {
  switch (Key(wnd)) {
    case MLEFT: {
      /* ensure the coordinates are in the right order
       * when we drag right to left / bot to top */
      float norm[4];
      MemCpy(norm, dragRect, sizeof(norm));
      NormRect(norm);
      isFull = !Pack(pak, norm);
      if (!isFull) {
        Col(packedRects, col);
        Quad(packedRects, norm[0], norm[2], RectWidth(norm), RectHeight(norm));
      }
      dragRect[0] = dragRect[2] = -1;
    }
  }
}

void Motion() {
  if (dragRect[0] >= 0) {
    dragRect[1] += MouseDX(wnd);
    dragRect[3] += MouseDY(wnd);
  }
}

void Frame() {
  PutMesh(packedRects, 0, 0);
  timeElapsed += (int)(Delta(wnd) * 1000000);
  if (dragRect[0] >= 0) {
    Mesh mesh = MkMesh();
    float norm[4];
    MemCpy(norm, dragRect, sizeof(norm));
    NormRect(norm);
    Col(mesh, col);
    Quad(mesh, norm[0], norm[2], RectWidth(norm), RectHeight(norm));
    PutMesh(mesh, 0, 0);
    RmMesh(mesh);
  }
  PutMesh(help, 0, FtImg(font));
  if (isFull) {
    PutMesh(full, 0, FtImg(font));
  }
}

void AppInit() {
  SetAppName("WeebCore - Rectangle Packer Example");
  SetAppClass("WeebCoreRectanglePacker");
  On(INIT, Init);
  On(QUIT, Quit);
  On(KEYDOWN, KeyDown);
  On(KEYUP, KeyUp);
  On(MOTION, Motion);
  On(FRAME, Frame);
}

#define WEEBCORE_IMPLEMENTATION
#include "WeebCore.c"
#include "Platform/Platform.h"
