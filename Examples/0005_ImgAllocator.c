/* draft of the img alloc, which automatically packs together img's into one bigger img so the gpu
 * doesn't have to swap img all the time.
 * it uses a fixed page size and when it runs out of space it makes a new page */

#include "WeebCore.c"

/* this demo just creates a bunch of random bouncing objects to fill up the atlas */

typedef struct {
  Trans trans;
  float r[4];
  float vx, vy;
  ImgPtr img;
  Mesh mesh;
} Ent;

Wnd wnd;
Ent* ents;
unsigned elapsed;
int frames, fps;
float fpsTimer;
Ft ft;
Mesh text;
int diagAllocator;

void MkEnt() {
  Ent* ent = ArrAlloc(&ents, 1);
  int* pixs = 0;
  int i;
  /* pseudorandom */
  int width = 20 + (elapsed & 31);
  int height = 20 + ((elapsed >> 5) & 31);
  int col = 0x404040 + (elapsed & 0x5f5f5f);
  float vx = (0xfff + (elapsed & 0xffff)) / (float)(0xffff) * 400;
  float vy = (0xfff + ((elapsed >> 16) & 0xffff)) / (float)(0xffff) * 400;
  for (i = 0; i < width * height; ++i) {
    float roff = (i % width) / (float)width;
    float goff = (i / width) / (float)height;
    ArrCat(&pixs, col + ((int)(roff * 0x3f) << 16) + ((int)(goff * 0x3f) << 8));
  }
  ent->img = ImgAlloc(width, height);
  ImgCpy(ent->img, pixs, width, height);
  FlushImgs();
  ent->mesh = MkMesh();
  Quad(ent->mesh, 0, 0, width, height);
  RmArr(pixs);
  ent->vx = vx;
  ent->vy = vy;
  SetRect(ent->r, 0, width, 0, height);
  ent->trans = MkTrans();
}

void UpdEnts() {
  int i;
  float delta = Delta(wnd);
  for (i = 0; i < ArrLen(ents); ++i) {
    Ent* ent = &ents[i];
    SetRectPos(ent->r,
      RectX(ent->r) + ent->vx * delta,
      RectY(ent->r) + ent->vy * delta);
    if (RectRight(ent->r) >= WndWidth(wnd)) {
      ent->vx *= -1;
      SetRectPos(ent->r, WndWidth(wnd) - RectWidth(ent->r), RectY(ent->r));
    } else if (RectLeft(ent->r) <= 0) {
      ent->vx *= -1;
      SetRectPos(ent->r, 0, RectY(ent->r));
    } else if (RectBot(ent->r) >= WndHeight(wnd)) {
      ent->vy *= -1;
      SetRectPos(ent->r, RectX(ent->r), WndHeight(wnd) - RectHeight(ent->r));
    } else if (RectTop(ent->r) <= 0) {
      ent->vy *= -1;
      SetRectPos(ent->r, RectX(ent->r), 0);
    }
    SetPos(ent->trans, (int)RectX(ent->r), (int)RectY(ent->r));
  }
}

void PutEnts() {
  int i;
  for (i = 0; i < ArrLen(ents); ++i) {
    PutMesh(ents[i].mesh, ToTmpMat(ents[i].trans), ents[i].img);
  }
}

void RmEnt(Ent* ent) {
  RmTrans(ent->trans);
  RmMesh(ent->mesh);
}

void RmEnts() {
  int i;
  for (i = 0; i < ArrLen(ents); ++i) {
    RmEnt(&ents[i]);
  }
  RmArr(ents);
  ents = 0;
}

void RmEntAt(int i) {
  if (i >= 0 && i < ArrLen(ents)) {
    Ent* e = &ents[i];
    /* for better visualization, blank out the freed img */
    int* black = Alloc(RectWidth(e->r) * RectHeight(e->r) * sizeof(int));
    ImgCpy(e->img, black, RectWidth(e->r) + 0.5f, RectHeight(e->r) + 0.5f);
    FlushImgs();
    Free(black);
    ImgFree(e->img);
    RmEnt(e);
    MemMv(e, e + 1, sizeof(Ent) * (ArrLen(ents) - i - 1));
    SetArrLen(ents, ArrLen(ents) - 1);
  }
}

void PutStatsText() {
  char* pagestr = 0;
  ArrStrCat(&pagestr, "fps: ");
  ArrStrCatI32(&pagestr, fps, 10);
  ArrStrCat(&pagestr, " quads: ");
  ArrStrCatI32(&pagestr, ArrLen(ents), 10);
  ArrCat(&pagestr, 0);
  PutFt(ft, 0xbebebe, 10, WndHeight(wnd) - 21, pagestr);
  RmArr(pagestr);
}

void Init() {
  wnd = AppWnd();
  ft = DefFt();
  text = MkMesh();
  Col(text, 0xbebebe);
  FtMesh(text, ft, 10, 10, "space to spawn random quads\nbackspace to free the oldest quad\n"
    "F2 to reset the allocator\nF1 to see the texture atlas\nmouse wheel to switch pages");
}

void Quit() {
  RmEnts();
  RmMesh(text);
}

void KeyDown() {
  switch (Key(wnd)) {
    case SPACE: {
      MkEnt();
      break;
    }
    case BACKSPACE: {
      RmEntAt(0);
      break;
    }
    case F1: {
      diagAllocator ^= 1;
      DiagImgAlloc(diagAllocator);
      break;
    }
    case F2: {
      RmEnts();
      ClrImgs();
      FlushImgs();
      break;
    }
  }
}

void Frame() {
  UpdEnts();
  PutEnts();
  PutMesh(text, 0, FtImg(ft));
  PutStatsText();

  elapsed += (int)(Delta() * 1000000000);

  ++frames;
  fpsTimer += Delta();
  while (fpsTimer >= 1) {
    fpsTimer -= 1;
    fps = frames;
    frames = 0;
  }
}

void AppInit() {
  SetAppName("WeebCore - Img Allocator Demo");
  SetAppClass("WeebCoreImgAllocator");
  On(INIT, Init);
  On(QUIT, Quit);
  On(KEYDOWN, KeyDown);
  On(FRAME, Frame);
}

#define WEEBCORE_IMPLEMENTATION
#include "WeebCore.c"
#include "Platform/Platform.h"
