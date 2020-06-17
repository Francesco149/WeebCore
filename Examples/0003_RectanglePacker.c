#include "WeebCore.c"

/* a simple best fit 2D rectangle packer. early version of what's going to be built into WeebCore */

Wnd MkPackerWnd() {
  Wnd wnd = MkWnd();
  SetWndName(wnd, "WeebCore - Rectangle Packer Example");
  SetWndClass(wnd, "WeebCoreRectanglePacker");
  return wnd;
}

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

typedef struct { float r[4]; } PackerRect; /* left, right, top bottom */

void AddRect(PackerRect** rectsArr, float left, float right, float top, float bottom) {
  PackerRect* newRect = ArrAlloc(rectsArr, 1);
  SetRect(newRect->r, left, right, top, bottom);
}

void AddRectFloats(PackerRect** rectsArr, float* r) {
  PackerRect* newRect = ArrAlloc(rectsArr, 1);
  MemCpy(newRect, r, sizeof(float) * 4);
}

/* find the best fit free rectangle (smallest area that can fit the rect).
 * initially we will have 1 big free rect that takes the entire area */
int FindFreeRect(PackerRect* rects, PackerRect* toBePackedStruct) {
  int i, bestFit = -1;
  float bestFitArea = 2000000000;
  float* toBePacked = toBePackedStruct->r;
  for (i = 0; i < ArrLen(rects); ++i) {
    float* rect = rects[i].r;
    if (RectInRectArea(toBePacked, rect)) {
      float area = RectWidth(rect) * RectHeight(rect);
      if (area < bestFitArea) {
        bestFitArea = area;
        bestFit = i;
      }
    }
  }
  return bestFit;
}

/* once we have found a location for the rectangle, we need to split any free rectangles it
 * partially intersects with. this will generate two or more smaller rects */
void SplitRects(PackerRect** rectsArr, PackerRect* toBePackedStruct) {
  PackerRect* rects = *rectsArr;
  float* toBePacked = toBePackedStruct->r;
  int i;
  PackerRect* newRects = 0;
  for (i = 0; i < ArrLen(rects); ++i) {
    float* r = rects[i].r;
    if (RectSect(toBePacked, r)) {
      if (toBePacked[0] > r[0]) { AddRect(&newRects, r[0], toBePacked[0], r[2], r[3]); /* left  */ }
      if (toBePacked[1] < r[1]) { AddRect(&newRects, toBePacked[1], r[1], r[2], r[3]); /* right */ }
      if (toBePacked[2] > r[2]) { AddRect(&newRects, r[0], r[1], r[2], toBePacked[2]); /* top   */ }
      if (toBePacked[3] < r[3]) { AddRect(&newRects, r[0], r[1], toBePacked[3], r[3]); /* bott  */ }
    } else {
      AddRectFloats(&newRects, r);
    }
  }

  RmArr(*rectsArr);
  *rectsArr = newRects;
}

/* after the split step, there will be redundant rects because we create 1 rect for each side */
void RmRedundantRects(PackerRect** rectsArr) {
  int i, j;
  PackerRect* rects = *rectsArr;
  PackerRect* newRects = 0;
  for (i = 0; i < ArrLen(rects); ++i) {
    for (j = 0; j < ArrLen(rects); ++j) {
      if (RectInRect(rects[i].r, rects[j].r)) {
        rects[i].r[0] = rects[i].r[1] = 0;
      } else if (RectInRect(rects[j].r, rects[i].r)) {
        rects[j].r[0] = rects[j].r[1] = 0;
      }
    }
    if (RectWidth(rects[i].r) > 0) {
      AddRectFloats(&newRects, rects[i].r);
    }
  }
  RmArr(*rectsArr);
  *rectsArr = newRects;
}

int PackRect(PackerRect** rectsArr, PackerRect* toBePacked) {
  PackerRect* rects = *rectsArr;

  int freeRect = FindFreeRect(rects, toBePacked);
  if (freeRect < 0) { /* full */
    return 0;
  }

  /* move rectangle to the top left of the free area */
  SetRectPos(toBePacked->r, rects[freeRect].r[0], rects[freeRect].r[2]);

  SplitRects(rectsArr, toBePacked);
  RmRedundantRects(rectsArr);

  return 1;
}

void ClrRects(PackerRect** rectsArr, Wnd wnd) {
  /* initialize area to be one big free rect */
  SetArrLen(*rectsArr, 0);
  AddRect(rectsArr, 0, WndWidth(wnd), 0, WndHeight(wnd));
}

int main() {
  Wnd wnd = MkPackerWnd();
  Ft font = DefFt();
  Mesh help = MkHelpText(font);
  Mesh full = MkFullText(font);
  Mesh packedRects = MkMesh();

  PackerRect* rects = 0;
  unsigned timeElapsed = 0;
  int col = 0;
  int isFull = 0;
  PackerRect dragRectStruct;
  float* dragRect = dragRectStruct.r;

  dragRect[0] = dragRect[2] = -1;
  ClrRects(&rects, wnd);

  while (1) {
    while (NextMsg(wnd)) {
      switch (MsgType(wnd)) {
        case QUIT: {
          RmArr(rects);
          RmMesh(packedRects);
          RmMesh(help);
          RmMesh(full);
          RmFt(font);
          RmWnd(wnd);
          return 0;
        }
        case KEYDOWN: {
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
              ClrRects(&rects, wnd);
              break;
            }
          }
          break;
        }
        case KEYUP: {
          if (Key(wnd) == MLEFT) {
            /* ensure the coordinates are in the right order
             * when we drag right to left / bot to top */
            PackerRect norm = dragRectStruct;
            NormRect(norm.r);
            isFull = !PackRect(&rects, &norm);
            if (!isFull) {
              Col(packedRects, col);
              Quad(packedRects, norm.r[0], norm.r[2], RectWidth(norm.r), RectHeight(norm.r));
            }
            dragRect[0] = dragRect[2] = -1;
          }
          break;
        }
        case MOTION: {
          if (dragRect[0] >= 0) {
            dragRect[1] += MouseDX(wnd);
            dragRect[3] += MouseDY(wnd);
          }
          break;
        }
      }
    }

    PutMesh(packedRects, 0, 0);

    timeElapsed += (int)(Delta(wnd) * 1000000);

    if (dragRect[0] >= 0) {
      Mesh mesh = MkMesh();
      PackerRect norm = dragRectStruct;
      NormRect(norm.r);
      Col(mesh, col);
      Quad(mesh, norm.r[0], norm.r[2], RectWidth(norm.r), RectHeight(norm.r));
      PutMesh(mesh, 0, 0);
      RmMesh(mesh);
    }

    PutMesh(help, 0, FtImg(font));
    if (isFull) {
      PutMesh(full, 0, FtImg(font));
    }
    SwpBufs(wnd);
  }

  return 0;
}

#define WEEBCORE_IMPLEMENTATION
#include "WeebCore.c"
#include "Platform/Platform.h"
