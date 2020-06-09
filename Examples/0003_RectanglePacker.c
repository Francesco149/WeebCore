#include "WeebCore.c"

/* a simple best fit 2D rectangle packer. early version of what's going to be built into WeebCore */

OSWindow CreateWindow() {
  OSWindow window = osCreateWindow();
  osSetWindowName(window, "WeebCore - Rectangle Packer Example");
  osSetWindowClass(window, "WeebCoreRectanglePacker");
  return window;
}

GMesh CreateHelpText(GFont font) {
  GMesh mesh = gCreateMesh();
  gColor(mesh, 0xbebebe);
  gFont(mesh, font, 10, 10,
    "Left-click and drag to create a rectangle. Release to pack it\nF2 to reset");
  return mesh;
}

GMesh CreateFullText(GFont font) {
  GMesh mesh = gCreateMesh();
  gColor(mesh, 0x663333);
  gFont(mesh, font, 10, 32, "Rectangle didn't fit!");
  return mesh;
}

typedef struct { float r[4]; } PackerRect; /* left, right, top bottom */

void AddRect(PackerRect** rectsArray, float left, float right, float top, float bottom) {
  PackerRect* newRect = wbArrayAlloc(rectsArray, 1);
  maSetRect(newRect->r, left, right, top, bottom);
}

void AddRectFloats(PackerRect** rectsArray, float* r) {
  PackerRect* newRect = wbArrayAlloc(rectsArray, 1);
  osMemCpy(newRect, r, sizeof(float) * 4);
}

/* find the best fit free rectangle (smallest area that can fit the rect).
 * initially we will have 1 big free rect that takes the entire area */
int FindFreeRect(PackerRect* rects, PackerRect* toBePackedStruct) {
  int i, bestFit = -1;
  float bestFitArea = 2000000000;
  float* toBePacked = toBePackedStruct->r;
  for (i = 0; i < wbArrayLen(rects); ++i) {
    float* rect = rects[i].r;
    if (maRectInRectArea(toBePacked, rect)) {
      float area = maRectWidth(rect) * maRectHeight(rect);
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
void SplitRects(PackerRect** rectsArray, PackerRect* toBePackedStruct) {
  PackerRect* rects = *rectsArray;
  float* toBePacked = toBePackedStruct->r;
  int i;
  PackerRect* newRects = 0;
  for (i = 0; i < wbArrayLen(rects); ++i) {
    float* r = rects[i].r;
    if (maRectIntersect(toBePacked, r)) {
      if (toBePacked[0] > r[0]) { AddRect(&newRects, r[0], toBePacked[0], r[2], r[3]); /* left  */ }
      if (toBePacked[1] < r[1]) { AddRect(&newRects, toBePacked[1], r[1], r[2], r[3]); /* right */ }
      if (toBePacked[2] > r[2]) { AddRect(&newRects, r[0], r[1], r[2], toBePacked[2]); /* top   */ }
      if (toBePacked[3] < r[3]) { AddRect(&newRects, r[0], r[1], toBePacked[3], r[3]); /* bott  */ }
    } else {
      AddRectFloats(&newRects, r);
    }
  }

  wbDestroyArray(*rectsArray);
  *rectsArray = newRects;
}

/* after the split step, there will be redundant rects because we create 1 rect for each side */
void RemoveRedundantRects(PackerRect** rectsArray) {
  int i, j;
  PackerRect* rects = *rectsArray;
  PackerRect* newRects = 0;
  for (i = 0; i < wbArrayLen(rects); ++i) {
    for (j = 0; j < wbArrayLen(rects); ++j) {
      if (maRectInRect(rects[i].r, rects[j].r)) {
        rects[i].r[0] = rects[i].r[1] = 0;
      } else if (maRectInRect(rects[j].r, rects[i].r)) {
        rects[j].r[0] = rects[j].r[1] = 0;
      }
    }
    if (maRectWidth(rects[i].r) > 0) {
      AddRectFloats(&newRects, rects[i].r);
    }
  }
  wbDestroyArray(*rectsArray);
  *rectsArray = newRects;
}

int PackRect(PackerRect** rectsArray, PackerRect* toBePacked) {
  PackerRect* rects = *rectsArray;

  int freeRect = FindFreeRect(rects, toBePacked);
  if (freeRect < 0) { /* full */
    return 0;
  }

  /* move rectangle to the top left of the free area */
  maSetRectPos(toBePacked->r, rects[freeRect].r[0], rects[freeRect].r[2]);

  SplitRects(rectsArray, toBePacked);
  RemoveRedundantRects(rectsArray);

  return 1;
}

void ResetRects(PackerRect** rectsArray, OSWindow window) {
  /* initialize area to be one big free rect */
  wbSetArrayLen(*rectsArray, 0);
  AddRect(rectsArray, 0, osWindowWidth(window), 0, osWindowHeight(window));
}

int main() {
  OSWindow window = CreateWindow();
  GFont font = gDefaultFont();
  GMesh help = CreateHelpText(font);
  GMesh full = CreateFullText(font);
  GMesh packedRects = gCreateMesh();

  PackerRect* rects = 0;
  unsigned timeElapsed = 0;
  int color = 0;
  int isFull = 0;
  PackerRect dragRectStruct;
  float* dragRect = dragRectStruct.r;

  dragRect[0] = dragRect[2] = -1;
  ResetRects(&rects, window);

  while (1) {
    while (osNextMessage(window)) {
      switch (osMessageType(window)) {
        case OS_QUIT: {
          wbDestroyArray(rects);
          gDestroyMesh(packedRects);
          gDestroyMesh(help);
          gDestroyFont(font);
          osDestroyWindow(window);
          return 0;
        }
        case OS_KEYDOWN: {
          switch (osKey(window)) {
            case OS_MLEFT: {
              dragRect[0] = osMouseX(window);
              dragRect[2] = osMouseY(window);
              dragRect[1] = dragRect[0];
              dragRect[3] = dragRect[2];
              color = timeElapsed & 0x8f8f8f; /* pseudorandom color */
              break;
            }
            case OS_F2: {
              gDestroyMesh(packedRects);
              packedRects = gCreateMesh();
              ResetRects(&rects, window);
              break;
            }
          }
          break;
        }
        case OS_KEYUP: {
          if (osKey(window) == OS_MLEFT) {
            /* ensure the coordinates are in the right order
             * when we drag right to left / bot to top */
            PackerRect norm = dragRectStruct;
            maNormalizeRect(norm.r);
            isFull = !PackRect(&rects, &norm);
            if (!isFull) {
              gColor(packedRects, color);
              gQuad(packedRects, norm.r[0], norm.r[2], maRectWidth(norm.r), maRectHeight(norm.r));
            }
            dragRect[0] = dragRect[2] = -1;
          }
          break;
        }
        case OS_MOTION: {
          if (dragRect[0] >= 0) {
            dragRect[1] += osMouseDX(window);
            dragRect[3] += osMouseDY(window);
          }
          break;
        }
      }
    }

    gDrawMesh(packedRects, 0, 0);

    timeElapsed += (int)(osDeltaTime(window) * 1000000);

    if (dragRect[0] >= 0) {
      GMesh mesh = gCreateMesh();
      PackerRect norm = dragRectStruct;
      maNormalizeRect(norm.r);
      gColor(mesh, color);
      gQuad(mesh, norm.r[0], norm.r[2], maRectWidth(norm.r), maRectHeight(norm.r));
      gDrawMesh(mesh, 0, 0);
      gDestroyMesh(mesh);
    }

    gDrawMesh(help, 0, gFontTexture(font));
    if (isFull) {
      gDrawMesh(full, 0, gFontTexture(font));
    }
    gSwapBuffers(window);
  }

  return 0;
}

#define WEEBCORE_IMPLEMENTATION
#include "WeebCore.c"
#include "Platform/Platform.h"
