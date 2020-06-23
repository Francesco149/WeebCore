/* early prototype of the software renderer that will be built into WeebCore */

#include "WeebCore.c"

/* passing pt's by value can generate a lot of inefficient copying but it makes code nice */

typedef struct _Pt { float x, y; } Pt;

Pt SetPt(float x, float y) {
  Pt pt;
  pt.x = x; pt.y = y;
  return pt;
}

Pt AddPt(Pt a, Pt b) { return SetPt(a.x + b.x, a.y + b.y); }
Pt SubPt(Pt a, Pt b) { return SetPt(a.x - b.x, a.y - b.y); }
Pt MulPt(Pt a, Pt b) { return SetPt(a.x * b.x, a.y * b.y); }
Pt MulPtFlt(Pt a, float x) { return SetPt(a.x * x, a.y * x); }
float PtLen(Pt pt) { return Sqrt(pt.x * pt.x + pt.y * pt.y); }
Pt NormPt(Pt pt) { return MulPtFlt(pt, 1 / PtLen(pt)); }

typedef struct _Surf {
  int* pixs;
  int width, height;
  int col;
}* Surf;

Surf MkSurf(int width, int height) {
  Surf surf = Alloc(sizeof(struct _Surf));
  surf->width = width;
  surf->height = height;
  surf->pixs = Alloc(width * height * sizeof(int));
  surf->col = 0xffffff;
  return surf;
}

void RmSurf(Surf surf) {
  Free(surf->pixs);
  Free(surf);
}

void SwpPt(Pt* a, Pt* b) {
  Pt tmp = *b;
  *b = *a;
  *a = tmp;
}

void SurfScan(Surf surf, int y, int leftx, int rightx) {
  int x;
  for (x = Max(leftx, 0); x <= Min(rightx, surf->width - 1); ++x) {
    AlphaBlendp(&surf->pixs[y * surf->width + x], surf->col);
  }
}

void SurfTri(Surf surf, float x1, float y1, float x2, float y2, float x3, float y3) {
  Pt a = SetPt(x1, y1), b = SetPt(x2, y2), c = SetPt(x3, y3);
  Pt slope1, slope2, slope3;
  float startx, endx, xstep1, xstep2, xstep3;
  int y;

  /* sort points abc by y */
  if (a.y > b.y) { SwpPt(&a, &b); }
  if (a.y > c.y) { SwpPt(&a, &c); }
  if (b.y > c.y) { SwpPt(&b, &c); }

  /* find slope of each edge and calculate how much x changes for yeach 1-pix step in y */
  slope1 = SubPt(b, a);
  xstep1 = slope1.x / slope1.y;
  slope2 = SubPt(c, a);
  xstep2 = slope2.x / slope2.y;
  slope3 = SubPt(c, b);
  xstep3 = slope3.x / slope3.y;

  /* ensure the triangle is on screen at all vertically */
  if (a.y >= surf->height || c.y < 0) { return; }

  /* ensure xstep1 is left and xstep2 is right */
  if (b.x > c.x) { SwpFlts(&xstep1, &xstep2); }

  if (b.y >= 0 && a.y < surf->height) {
    /* draw scanlines from point a to b */
    startx = endx = a.x;
    y = a.y;
    if (y < 0) {
      /* skip until vertically onscreen part */
      startx += xstep1 * -y;
      endx += xstep2 * -y;
      y = 0;
    }
    for (; y < Min(b.y, surf->height - 1); ++y) {
      SurfScan(surf, y, startx, endx);
      startx += xstep1;
      endx += xstep2;
    }
  }

  if (b.y < surf->height) {
    /* figure out which side needs to swap x step and draw scanlines from point b to c */
    if (b.x < c.x) {
      startx = b.x;
      endx = a.x + xstep2 * (b.y - a.y);
      xstep1 = xstep3;
    } else {
      startx = a.x + xstep1 * (b.y - a.y);
      endx = b.x;
      xstep2 = xstep3;
    }
    y = b.y;
    if (y < 0) {
      /* skip until vertically onscreen part */
      startx += xstep1 * -y;
      endx += xstep2 * -y;
      y = 0;
    }
    for (; y <= Min(c.y, surf->height - 1); ++y) {
      SurfScan(surf, y, startx, endx);
      startx += xstep1;
      endx += xstep2;
    }
  }
}

int* SurfPixs(Surf surf) { return surf->pixs; }
int SurfWidth(Surf surf) { return surf->width; }
int SurfHeight(Surf surf) { return surf->height; }
void SurfCol(Surf surf, int col) { surf->col = col; }

void SurfToImg(Surf surf, ImgPtr img) {
  ImgCpy(img, SurfPixs(surf), SurfWidth(surf), SurfHeight(surf));
}

Mesh MkFrameMesh() {
  Mesh mesh = MkMesh();
  Quad(mesh, 0, 0, 640, 480);
  return mesh;
}

ImgPtr PutFrame() {
  ImgPtr img = ImgAlloc(640, 480);
  Surf surf = MkSurf(640, 480);
  SurfTri(surf, 0, -240, 320, 240, -320, 240);
  SurfTri(surf, 640, 240, 960, 720, 320, 720);
  SurfCol(surf, 0x7fff3333);
  SurfTri(surf, 200, 20, 620, 240, 20, 460);
  SurfToImg(surf, img);
  RmSurf(surf);
  return img;
}

Wnd wnd;
Mesh mesh;
ImgPtr img;

void Init() {
  wnd = AppWnd();
  mesh = MkFrameMesh();
  img = PutFrame();
}

void Quit() {
  RmMesh(mesh);
}

void Frame() {
  PutMesh(mesh, 0, img);
}

void AppInit() {
  SetAppClass("WeebCoreSoftwareRenderer");
  SetAppName("WeebCore - Software Renderer Demo");
  On(INIT, Init);
  On(QUIT, Quit);
  On(FRAME, Frame);
}

#define WEEBCORE_IMPLEMENTATION
#include "WeebCore.c"
#include "Platform/Platform.h"
