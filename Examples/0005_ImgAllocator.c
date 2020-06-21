/* draft of the img man, which automatically packs together img's into one bigger img so the gpu
 * doesn't have to swap img all the time.
 * it uses a fixed page size and when it runs out of space it makes a new page */

#include "WeebCore.c"

typedef struct _ImgMan* ImgMan;
typedef int ImgHandle;

typedef struct _ImgPage {
  Img img;
  int* pixs;
  Packer pak;
} ImgPage;

typedef struct _ImgRegion {
  int page;
  float r[4];
} ImgRegion;

struct _ImgMan {
  int width, height;
  ImgPage* pages;
  ImgRegion* regions;
  Packer pak;
};

ImgMan MkImgMan(int width, int height) {
  ImgMan res = Alloc(sizeof(struct _ImgMan));
  if (res) {
    res->width = RoundUpToPowerOfTwo(width);
    res->height = RoundUpToPowerOfTwo(height);
  }
  return res;
}

void RmImgMan(ImgMan man) {
  if (man) {
    int i;
    for (i = 0; i < ArrLen(man->pages); ++i) {
      RmImg(man->pages[i].img);
      RmPacker(man->pages[i].pak);
      Free(man->pages[i].pixs);
    }
    RmArr(man->pages);
    RmArr(man->regions);
  }
  Free(man);
}

ImgHandle ManPixs(ImgMan man, int width, int height, int* pixs) {
  ImgRegion* region;
  float r[4];
  int i;
  ImgPage* page = 0;
  SetRect(r, 0, width, 0, height);
  for (i = 0; i < ArrLen(man->pages); ++i) {
    page = &man->pages[i];
    if (Pack(page->pak, r)) {
      break;
    }
  }
  if (i >= ArrLen(man->pages)) {
    /* no free pages, make a new page */
    page = ArrAlloc(&man->pages, 1);
    page->img = MkImg();
    page->pixs = Alloc(man->width * man->height * sizeof(int));
    page->pak = MkPacker(man->width, man->height);
    if (!Pack(page->pak, r)) {
      return -1;
    }
  }
  region = ArrAlloc(&man->regions, 1);
  if (region) {
    int x, y;
    for (y = 0; y < height; ++y) {
      for (x = 0; x < width; ++x) {
        int dx = RectX(r) + x;
        int dy = RectY(r) + y;
        page->pixs[dy * man->width + dx] = pixs[y * width + x];
      }
    }
    CpyRect(region->r, r);
    region->page = i;
    Pixs(page->img, man->width, man->height, page->pixs);
  }
  return ArrLen(man->regions) - 1;
}

ImgHandle ManSprFile(ImgMan man, char* path) {
  ImgHandle res = -1;
  Spr spr = MkSprFromFile(path);
  if (spr) {
    int* pixs = SprToArgbArr(spr);
    res = ManPixs(man, SprWidth(spr), SprHeight(spr), pixs);
    RmArr(pixs);
  }
  RmSpr(spr);
  return res;
}

void ManPutMesh(ImgMan man, Mesh mesh, Mat mat, ImgHandle handle) {
  ImgRegion* region = &man->regions[handle];
  PutMeshEx(mesh, mat, man->pages[region->page].img, RectX(region->r), RectY(region->r));
}

Wnd MkImgManDemoWnd() {
  Wnd wnd = MkWnd();
  SetWndName(wnd, "WeebCore - Img Man Demo");
  SetWndClass(wnd, "WeebCoreImgMan");
  return wnd;
}

/* this demo just creates a bunch of random bouncing objects to fill up the atlas */

typedef struct {
  Trans trans;
  float r[4];
  float vx, vy;
  ImgHandle img;
  Mesh mesh;
} Ent;

void MkEnt(Ent** ents, ImgMan man, unsigned elapsed) {
  Ent* ent = ArrAlloc(ents, 1);
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
  ent->img = ManPixs(man, width, height, pixs);
  ent->mesh = MkMesh();
  Quad(ent->mesh, 0, 0, width, height);
  RmArr(pixs);
  ent->vx = vx;
  ent->vy = vy;
  SetRect(ent->r, 0, width, 0, height);
  ent->trans = MkTrans();
}

void UpdEnts(Ent* ents, Wnd wnd) {
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

void PutEnts(Ent* ents, ImgMan man) {
  int i;
  for (i = 0; i < ArrLen(ents); ++i) {
    ManPutMesh(man, ents[i].mesh, ToTmpMat(ents[i].trans), ents[i].img);
  }
}

void RmEnts(Ent* ents) {
  int i;
  for (i = 0; i < ArrLen(ents); ++i) {
    RmTrans(ents[i].trans);
  }
  RmArr(ents);
}

void PutPageText(Ft ft, Wnd wnd, int page) {
  char* pagestr = 0;
  ArrStrCat(&pagestr, "page: ");
  ArrStrCatI32(&pagestr, page, 10);
  ArrCat(&pagestr, 0);
  PutFt(ft, 0xbebebe, WndWidth(wnd) - StrLen(pagestr) * 6 - 10, 10, pagestr);
  RmArr(pagestr);
}

void PutStatsText(Ft ft, Wnd wnd, int fps, int ents, int pages) {
  char* pagestr = 0;
  ArrStrCat(&pagestr, "fps: ");
  ArrStrCatI32(&pagestr, fps, 10);
  ArrStrCat(&pagestr, " quads: ");
  ArrStrCatI32(&pagestr, ents, 10);
  ArrStrCat(&pagestr, " textures: ");
  ArrStrCatI32(&pagestr, pages, 10);
  ArrCat(&pagestr, 0);
  PutFt(ft, 0xbebebe, 10, WndHeight(wnd) - 21, pagestr);
  RmArr(pagestr);
}

int main() {
  Wnd wnd = MkImgManDemoWnd();
  ImgMan man = MkImgMan(1024, 1024);
  Ent* ents = 0;

  int frames = 0;
  float fpsTimer = 0;
  int fps = 0;

  unsigned elapsed = 0;
  int showAtlas = 0;
  int page = 0;
  Mesh atlas = MkMesh();
  Ft ft = DefFt();
  Mesh text = MkMesh();

  Col(text, 0xbebebe);
  FtMesh(text, ft, 10, 10, "space to spawn random quads\n"
    "F1 to see the texture atlas\nmouse wheel to switch pages");
  Quad(atlas, 0, 0, 1024, 1024);

  while (1) {
    while (NextMsg(wnd)) {
      switch (MsgType(wnd)) {
        case QUIT: {
          RmEnts(ents);
          RmImgMan(man);
          RmWnd(wnd);
          return 0;
        }
        case KEYDOWN: {
          switch (Key(wnd)) {
            case SPACE: {
              MkEnt(&ents, man, elapsed);
              break;
            }
            case F1: {
              showAtlas ^= 1;
              break;
            }
            case MWHEELUP: {
              page = Max(0, Min(page + 1, ArrLen(man->pages) - 1));
              break;
            }
            case MWHEELDOWN: {
              page = Max(page - 1, 0);
              break;
            }
          }
          break;
        }
      }
    }

    UpdEnts(ents, wnd);

    if (showAtlas) {
      if (ArrLen(man->pages)) {
        PutMesh(atlas, 0, man->pages[page].img);
      }
      PutPageText(ft, wnd, page);
    } else {
      PutEnts(ents, man);
    }

    PutMesh(text, 0, FtImg(ft));
    PutStatsText(ft, wnd, fps, ArrLen(ents), ArrLen(man->pages));

    elapsed += (int)(Delta(wnd) * 1000000000);

    ++frames;
    fpsTimer += Delta(wnd);
    while (fpsTimer >= 1) {
      fpsTimer -= 1;
      fps = frames;
      frames = 0;
    }

    SwpBufs(wnd);
  }
  return 0;
}

#define WEEBCORE_IMPLEMENTATION
#include "WeebCore.c"
#include "Platform/Platform.h"
