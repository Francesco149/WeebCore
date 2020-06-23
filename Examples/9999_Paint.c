/* very early draft of the sprite ed
 * middle click drag to pan
 * left click drag to draw
 * mouse wheel to zoom
 *
 * to avoid updating the img for every new pix, the pixs are batched up and temporarily
 * rendered as individual quads until they are flushed to the real img */

#include "WeebCore.c"

#define EDIMGSIZE 1024
#define EDCHECKER_SIZE 64

#define EDDRAGGING (1<<1)
#define EDPAINTING (1<<2)
#define EDDIRTY (1<<3)

typedef struct _EDUpd {
  int color;
  int x, y;
} EDUpd;

Wnd wnd;
int flags;
float oX, oY;
float flushTimer;
int scale;
int color;
Mesh bgMesh;
Img checkerImg;
Mesh mesh;
ImgPtr img;
int* imgData;
EDUpd* updates;
Trans trans;
Mat bgMat;

void EdFlushUpds() {
  ImgCpy(img, imgData, EDIMGSIZE, EDIMGSIZE);
  FlushImgs();
  SetArrLen(updates, 0);
  flags &= ~EDDIRTY;
}

void EdUpdTrans() {
  ClrTrans(trans);
  SetPos(trans, oX, oY);
  SetScale1(trans, scale);
}

/* there's no way to do texture wrapping with the img allocator so for these kind of cases we have
 * to use a raw img and call PutMeshRaw */
Img MkCheckerImg() {
  static int data[4] = { 0xaaaaaa, 0x666666, 0x666666, 0xaaaaaa };
  Img img = MkImg();
  return Pixs(img, 2, 2, data);
}

int* MkPixs(int fillCol, int width, int height) {
  int i;
  int* data = 0;
  for (i = 0; i < width * height; ++i) {
    ArrCat(&data, fillCol);
  }
  return data;
}

void EdMapToImg(float* point) {
  /* un-trans mouse coordinates so they are relative to the img */
  InvTransPt(ToTmpMatOrtho(trans), point);
  point[0] /= scale;
  point[1] /= scale;
}

void EdPaintPix() {
  int x, y;
  float point[2];
  point[0] = MouseX(wnd);
  point[1] = MouseY(wnd);
  EdMapToImg(point);
  x = (int)point[0];
  y = (int)point[1];
  if (x >= 0 && x < EDIMGSIZE && y >= 0 && y < EDIMGSIZE) {
    int* px = &imgData[y * EDIMGSIZE + x];
    if (*px != color) {
      EDUpd* u;
      *px = color;
      u = ArrAlloc(&updates, 1);
      u->x = x;
      u->y = y;
      u->color = color;
      flags |= EDDIRTY;
    }
  }
}

void EdChangeScale(int direction) {
  float p[2];
  float newScale = scale + direction * (scale / 8 + 1);
  newScale = Min(32, newScale);
  newScale = Max(1, newScale);
  p[0] = MouseX(wnd);
  p[1] = MouseY(wnd);
  EdMapToImg(p);
  /* adjust panning so the pix we're pointing stays under the cursor */
  oX -= (int)((p[0] * newScale) - (p[0] * scale) + 0.5f);
  oY -= (int)((p[1] * newScale) - (p[1] * scale) + 0.5f);
  scale = newScale;
  EdUpdTrans();
}

void EdPutUpds() {
  int i;
  Mesh mesh = MkMesh();
  for (i = 0; i < ArrLen(updates); ++i) {
    EDUpd* u = &updates[i];
    Col(mesh, u->color);
    Quad(mesh, u->x, u->y, 1, 1);
  }
  PutMesh(mesh, ToTmpMat(trans), 0);
  RmMesh(mesh);
}

void Size();

void Init() {
  wnd = AppWnd();
  scale = 4;
  oX = 100;
  oY = 100;
  checkerImg = MkCheckerImg();
  img = ImgAlloc(EDIMGSIZE, EDIMGSIZE);
  imgData = MkPixs(0xffffff, EDIMGSIZE, EDIMGSIZE);
  trans = MkTrans();
  bgMat = Scale1(MkMat(), EDCHECKER_SIZE);
  mesh = MkMesh();
  Quad(mesh, 0, 0, EDIMGSIZE, EDIMGSIZE);
  EdFlushUpds();
  Size();
  EdUpdTrans();
}

void Quit() {
  RmMesh(mesh);
  RmMesh(bgMesh);
  RmArr(imgData);
  RmArr(updates);
  RmImg(checkerImg);
  RmTrans(trans);
  RmMat(bgMat);
}

void Size() {
  RmMesh(bgMesh);
  bgMesh = MkMesh();
  Quad(bgMesh, 0, 0,
    Ceil(WndWidth(wnd)  / (float)EDCHECKER_SIZE),
    Ceil(WndHeight(wnd) / (float)EDCHECKER_SIZE)
  );
}

void KeyDown() {
  switch (Key(wnd)) {
    case MMID: { flags |= EDDRAGGING; break; }
    case MWHEELUP: { EdChangeScale(1); break; }
    case MWHEELDOWN: { EdChangeScale(-1); break; }
    case MLEFT: {
      flags |= EDPAINTING;
      EdPaintPix();
      EdFlushUpds();
      break;
    }
  }
}

void KeyUp() {
  switch (Key(wnd)) {
    case MMID: { flags &= ~EDDRAGGING; break; }
    case MLEFT: { flags &= ~EDPAINTING; break; }
  }
}

void Motion() {
  if (flags & EDDRAGGING) {
    oX += MouseDX(wnd);
    oY += MouseDY(wnd);
    EdUpdTrans();
  }
  if (flags & EDPAINTING) {
    EdPaintPix();
  }
}

void Frame() {
  flushTimer += Delta(wnd);
  if ((flags & EDDIRTY) && flushTimer > 1.0f) {
    EdFlushUpds();
    flushTimer -= 1.0f;
  }

  PutMeshRaw(bgMesh, bgMat, checkerImg);
  PutMesh(mesh, ToTmpMat(trans), img);
  EdPutUpds();
}

void AppInit() {
  SetAppClass("HelloWnd");
  SetAppName("Hello WeebCore");
  On(INIT, Init);
  On(QUIT, Quit);
  On(SIZE, Size);
  On(KEYDOWN, KeyDown);
  On(KEYUP, KeyUp);
  On(MOTION, Motion);
  On(FRAME, Frame);
}

#define WEEBCORE_IMPLEMENTATION
#include "WeebCore.c"
#include "Platform/Platform.h"
