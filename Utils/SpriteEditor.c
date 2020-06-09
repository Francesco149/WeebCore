/* very rough sprite editor, will improve/rewrite as I move features into the engine. this is mainly
 * to bootstrap making fonts and simple sprites so I can work on text rendering and ui entirely
 * off my own tools */

#include "WeebCore.c"

char* edHelpString() {
  return (
    "Controls:\n"
    "\n"
    "F1           toggle this help text\n"
    "Ctrl+S       save\n"
    "Ctrl+1       encode as 1bpp and save as .txt file containing a base64 string of the data.\n"
    "             if a selection is active, the output is cropped to the selection\n"
    "Middle Drag  pan around\n"
    "Wheel Up     zoom in\n"
    "Wheel Down   zoom out\n"
    "E            switch to pencil\n"
    "Left Click   (pencil) draw with color 1\n"
    "Right Click  (pencil) draw with color 2\n"
    "C            switch to color picker. press again to expand/collapse. expanded mode lets\n"
    "             you adjust colors with the ui, non-expanded picks from the pixels you click\n"
    "Left Click   (color picker) set color 1\n"
    "Right Click  (color picker) set color 2\n"
    "S            switch to selection mode\n"
    "Left Drag    (selection) select rectangle\n"
    "R            crop current selection to closest power-of-two size\n"
    "Shift+S      remove selection\n"
    "1            fill contents of selection with color 1\n"
    "2            fill contents of selection with color 2\n"
    "D            cut the contents of the selection. this will attach them to the selection\n"
    "Shift+D      undo cut. puts the contents of the cut back where they were\n"
    "T            (after cutting) paste the contents of the selection at the current location\n"
  );
}

#define ED_FLUSH_TIME 1.0f
#define ED_FLUSH_THRESHOLD 4096
#define ED_TEXSIZE 1024
#define ED_CHECKER_SIZE 8

#define ED_FDRAGGING (1<<1)
#define ED_FDRAWING (1<<2)
#define ED_FDIRTY (1<<3)
#define ED_FEXPAND_COLOR_PICKER (1<<4)
#define ED_FSELECT (1<<5)
#define ED_FALPHA_BLEND (1<<6)
#define ED_FIGNORE_SELECTION (1<<7)
#define ED_FHELP (1<<8)

enum {
  ED_PENCIL,
  ED_COLOR_PICKER,
  ED_SELECT,
  ED_LAST_TOOL
};

enum {
  ED_SELECT_NONE,
  ED_SELECT_MID,
  ED_SELECT_LEFT, /* TODO: resize selections */
  ED_SELECT_RIGHT,
  ED_SELECT_BOTTOM,
  ED_SELECT_TOP
};

typedef struct _EDUpdate {
  int color;
  int x, y;
  int flags;
} EDUpdate;

typedef struct _Editor {
  OSWindow window;
  int flags;
  float oX, oY;
  float flushTimer; /* TODO: built in timer events */
  int scale;
  int colors[2];
  int colorIndex;
  GTexture checkerTexture;
  GMesh mesh;
  GTexture texture;
  int* textureData;
  EDUpdate* updates;
  GTransformBuilder transformBuilder;
  GTransform transform, transformOrtho;
  int tool;
  int width, height;
  int lastDrawX, lastDrawY;

  GFont font;
  GMesh helpTextMesh, helpBackgroundMesh;

  float selBlinkTimer;
  GTexture selBorderTexture;
  float selRect[4];          /* raw rectangle between the cursor and the initial drag position */
  float effectiveSelRect[4]; /* normalized, snapped, etc selection rect */
  int selAnchor;

  GTexture cutTexture;
  int* cutData;
  float cutPotRect[4]; /* power of two size of the cut texture */
  GMesh cutMesh;
  GTransform cutTransform;
  GTransformBuilder cutTransformBuilder;
  float cutSourceRect[4]; /* the location from where the cut data was taken */

  /* TODO: pull out color picker as a reusable widget */
  GMesh colorPickerMesh;
  GTransformBuilder colorPickerTransformBuilder;
  GTransform colorPickerTransform;
  int colorPickerSize;
  int grey;
  float colorX, colorY, greyY, alphaY;
  char* filePath;
}* Editor;

void edMapToTexture(Editor editor, float* point);
void edFlushUpdates(Editor editor);
void edResetSelection(Editor editor);
void edUpdateMesh(Editor editor);
void edResetPanning(Editor editor);
void edSelectedRect(Editor editor, float* rect);
void edResetDrawRateLimiter(Editor editor);
int edUpdateDrawRateLimiter(Editor editor);
int edSampleCheckerboard(float x, float y);
void edDrawPixel(Editor editor, int x, int y, int color, int flags);

/* ---------------------------------------------------------------------------------------------- */

/* TODO: do not hardcode color picker coordinates this is so ugly */

#define CPICK_WIDTH 1.0f
#define CPICK_HEIGHT 1.0f
#define CPICK_MARGIN (1/32.0f)
#define CPICK_BAR_WIDTH (1/16.0f)
#define CPICK_BBAR_LEFT (CPICK_WIDTH + CPICK_MARGIN)
#define CPICK_BBAR_RIGHT (CPICK_BBAR_LEFT + CPICK_BAR_WIDTH)
#define CPICK_ABAR_LEFT (CPICK_BBAR_LEFT + CPICK_BAR_WIDTH + CPICK_MARGIN)
#define CPICK_ABAR_RIGHT (CPICK_ABAR_LEFT + CPICK_BAR_WIDTH)

int edColorPickerSize(Editor editor) {
  OSWindow window = editor->window;
  int winSize = wbMin(osWindowWidth(window), osWindowHeight(window));
  int size = wbRoundUpToPowerOfTwo(winSize / 4);
  size = wbMax(size, 256);
  size = wbMin(size, winSize * 0.7);
  return size;
}

GMesh edCreateColorPickerMesh(int grey) {
  GMesh mesh = gCreateMesh();
  int colors[7] = { 0xff0000, 0xffff00, 0x00ff00, 0x00ffff, 0x0000ff, 0xff00ff, 0xff0000 };
  int brightness[3] = { 0xffffff, 0xff000000, 0x000000 };
  int brightnessSelect[2] = { 0xffffff, 0x000000 };
  int alphaSelect[2] = { 0x00ffffff, 0xff000000 };
  brightness[0] = grey;
  gQuadGradientH(mesh, 0, 0, CPICK_WIDTH, CPICK_HEIGHT, 7, colors);
  gQuadGradientV(mesh, 0, 0, CPICK_WIDTH, CPICK_HEIGHT, 3, brightness);
  gQuadGradientV(mesh, CPICK_BBAR_LEFT, 0, CPICK_BAR_WIDTH, CPICK_HEIGHT, 2, brightnessSelect);
  gQuadGradientV(mesh, CPICK_ABAR_LEFT, 0, CPICK_BAR_WIDTH, CPICK_HEIGHT, 2, alphaSelect);
  return mesh;
}

void edDrawColorPicker(Editor editor) {
  float colorsY = 0;
  GMesh mesh = gCreateMesh();
  if (editor->flags & ED_FEXPAND_COLOR_PICKER) {
    colorsY = CPICK_HEIGHT + CPICK_MARGIN;
    gColor(mesh, gMix(editor->grey, 0xff0000, 0.5));
    gQuad(mesh, CPICK_BBAR_LEFT, editor->greyY - 1/128.0f, 1/16.0f, 1/64.0f);
    gColor(mesh, 0x000000);
    gQuad(mesh, CPICK_ABAR_LEFT, editor->alphaY - 1/128.0f, 1/16.0f, 1/64.0f);
    gColor(mesh, ~editor->colors[editor->colorIndex] & 0xffffff);
    gQuad(mesh, editor->colorX - 1/128.0f, editor->colorY - 1/128.0f, 1/64.0f, 1/64.0f);
    gColor(mesh, 0x000000);
    gQuad(mesh, editor->colorX - 1/256.0f, editor->colorY - 1/256.0f, 1/128.0f, 1/128.0f);
    gDrawMesh(editor->colorPickerMesh, editor->colorPickerTransform, 0);
  }
  gColor(mesh, editor->colors[0]);
  gQuad(mesh, 0, colorsY         , 1/8.0f, 1/8.0f);
  gColor(mesh, editor->colors[1]);
  gQuad(mesh, 0, colorsY + 1/8.0f, 1/8.0f, 1/8.0f);
  gDrawMesh(mesh, editor->colorPickerTransform, 0);
  gDestroyMesh(mesh);
}

void edSizeColorPicker(Editor editor) {
  GTransformBuilder tbuilder = editor->colorPickerTransformBuilder;
  editor->colorPickerSize = edColorPickerSize(editor);
  gResetTransform(tbuilder);
  gSetPosition(tbuilder, 10, 10);
  gSetScale1(tbuilder, editor->colorPickerSize);
  editor->colorPickerTransform = gBuildTempTransform(tbuilder);
}

void edUpdateColorPickerMesh(Editor editor) {
  gDestroyMesh(editor->colorPickerMesh);
  editor->colorPickerMesh = edCreateColorPickerMesh(editor->grey);
}

void edMixPickedColor(Editor editor) {
  float x = editor->colorX;
  float y = editor->colorY;
  int colors[7] = { 0xff0000, 0xffff00, 0x00ff00,
    0x00ffff, 0x0000ff, 0xff00ff, 0xff0000 };
  int i = (int)(x * 6);
  int baseColor = gMix(colors[i], colors[i + 1], (x * 6) - i);
  editor->grey = 0x010101 * (int)((1 - editor->greyY) * 0xff);
  if (y <= 0.5f) {
    editor->colors[editor->colorIndex] = gMix(editor->grey, baseColor, y * 2);
  } else {
    editor->colors[editor->colorIndex] = gMix(baseColor, 0x000000, (y - 0.5f) * 2);
  }
  editor->colors[editor->colorIndex] |= (int)(editor->alphaY * 0xff) << 24;
  edUpdateColorPickerMesh(editor);
}

void edPickColor(Editor editor) {
  OSWindow window = editor->window;
  /* TODO: proper inverse transform so window can be moved around n shit */
  /* TODO: use generic rect intersect funcs */
  float x = (osMouseX(window) - 10) / (float)editor->colorPickerSize;
  float y = (osMouseY(window) - 10) / (float)editor->colorPickerSize;
  if (editor->flags & ED_FEXPAND_COLOR_PICKER) {
    /* clicked on the grey adjust bar */
    if (y >= 0 && y <= CPICK_HEIGHT && x >= CPICK_BBAR_LEFT && x < CPICK_BBAR_RIGHT) {
      editor->greyY = y;
    }
    /* clicked on the alpha adjust bar */
    else if (y >= 0 && y <= CPICK_HEIGHT && x >= CPICK_ABAR_LEFT && x < CPICK_ABAR_RIGHT) {
      editor->alphaY = y;
    }
    /* clicked on the color picker */
    else if (y >= 0 && y <= CPICK_HEIGHT && x >= 0 && x < CPICK_WIDTH) {
      editor->colorX = x;
      editor->colorY = y;
    }
    edMixPickedColor(editor);
  } else {
    float p[2];
    p[0] = osMouseX(window);
    p[1] = osMouseY(window);
    edMapToTexture(editor, p);
    if (x >= 0 && x < editor->width && y >= 0 && y < editor->height) {
      editor->colors[editor->colorIndex] =
        editor->textureData[(int)p[1] * editor->width + (int)p[0]];
    }
  }
}

/* ---------------------------------------------------------------------------------------------- */

/* NOTE: this updates rect in place to the adjusted power-of-two size */
int* edCreatePixelsFromRegion(Editor editor, float* rect) {
  float extents[4];
  int* newData = 0;
  int x, y;

  /* commit any pending changes to the current image */
  if (editor->flags & ED_FDIRTY) {
    edFlushUpdates(editor);
  }

  /* snap crop rect to the closest power of two sizes */
  maSetRectSize(rect, wbRoundUpToPowerOfTwo(maRectWidth(rect)),
    wbRoundUpToPowerOfTwo(maRectHeight(rect)));

  /* create cropped pixel data, fill any padding that goes out of the original image with transp */
  maSetRect(extents, 0, editor->width, 0, editor->height);
  for (y = maRectTop(rect); y < maRectBottom(rect); ++y) {
    for (x = maRectLeft(rect); x < maRectRight(rect); ++x) {
      if (maPtInRect(extents, x, y)) {
        wbArrayAppend(&newData, editor->textureData[y * editor->width + x]);
      } else {
        wbArrayAppend(&newData, 0xff000000);
      }
    }
  }

  return newData;
}

void edFillRect(Editor editor, float* rect, int color) {
  int x, y;
  if (edUpdateDrawRateLimiter(editor)) {
    for (y = maRectTop(rect); y < maRectBottom(rect); ++y) {
      for (x = maRectLeft(rect); x < maRectRight(rect); ++x) {
        edDrawPixel(editor, x, y, color, 0);
      }
    }
  }
}

void edCrop(Editor editor, float* rect) {
  float potRect[4];
  int* newData;

  osMemCpy(potRect, rect, sizeof(potRect));
  newData = edCreatePixelsFromRegion(editor, potRect);

  /* refresh everything */
  wbDestroyArray(editor->textureData);
  editor->textureData = newData;
  editor->width = maRectWidth(potRect);
  editor->height = maRectHeight(potRect);
  edResetSelection(editor);
  edUpdateMesh(editor);
  edFlushUpdates(editor);
  edResetPanning(editor);
}

/* TODO: clean up this mess, maybe use a generic ui function to draw rectangles with borders */

/* the way this works is i have a 4x4 texture with a black square in the middle and a transparent
 * 1px border. by using texture coord offsets and messing with the repeat mode, I can reuse the
 * texture to draw all 4 sides of the dashed selection border */

void edDashBorderH(Editor editor, GMesh mesh, float* rect, float off, int color) {
  float uSize = maRectWidth(rect) * editor->scale;
  float vSize = maRectHeight(rect) * editor->scale;
  gColor(mesh, color);
  gTexturedQuad(
    mesh,
    maRectX(rect), maRectY(rect),
    1 + off, 1,
    maRectWidth(rect), maRectHeight(rect),
    uSize, vSize * 2
  );
  gTexturedQuad(
    mesh,
    maRectX(rect), maRectY(rect),
    1 - off, -vSize * 2 + 2,
    maRectWidth(rect), maRectHeight(rect),
    uSize, vSize * 2
  );
}

void edDashBorderV(Editor editor, GMesh mesh, float* rect, float off, int color) {
  float uSize = maRectWidth(rect) * editor->scale;
  float vSize = maRectHeight(rect) * editor->scale;
  gColor(mesh, color);
  gTexturedQuad(
    mesh,
    maRectX(rect), maRectY(rect),
    1, 1 - off,
    maRectWidth(rect), maRectHeight(rect),
    uSize * 2, vSize
  );
  gTexturedQuad(
    mesh,
    maRectX(rect), maRectY(rect),
    -uSize * 2 + 2, 1 + off,
    maRectWidth(rect), maRectHeight(rect),
    uSize * 2, vSize
  );
}

void edUpdateYank(Editor editor) {
  if (editor->cutMesh) {
    GTransformBuilder tbuilder = editor->cutTransformBuilder;
    float* rect = editor->effectiveSelRect;
    edSelectedRect(editor, rect);
    gResetTransform(tbuilder);
    /* TODO: instead of multiplying by scale everywhere, keep a separate transform for scale? */
    gSetPosition(tbuilder, maRectX(rect) * editor->scale, maRectY(rect) * editor->scale);
    editor->cutTransform = gBuildTempTransform(tbuilder);
  }
}

void edDiscardYank(Editor editor) {
  gDestroyMesh(editor->cutMesh);
  editor->cutMesh = 0;
  wbDestroyArray(editor->cutData);
  editor->cutData = 0;
}

void edResetSelection(Editor editor) {
  osMemSet(editor->selRect, 0, sizeof(editor->selRect));
  editor->flags &= ~ED_FSELECT;
  edDiscardYank(editor);
}

void edYank(Editor editor) {
  float* potRect = editor->cutPotRect;
  float* rect = editor->effectiveSelRect;
  if (editor->cutMesh) {
    edDiscardYank(editor);
  }
  osMemCpy(editor->cutSourceRect, rect, sizeof(float) * 4);
  osMemCpy(potRect, rect, sizeof(float) * 4);
  editor->cutData = edCreatePixelsFromRegion(editor, potRect);
  gPixels(editor->cutTexture, maRectWidth(potRect), maRectHeight(potRect), editor->cutData);
  editor->cutMesh = gCreateMesh();
  gQuad(editor->cutMesh, 0, 0, maRectWidth(rect), maRectHeight(rect));
  edFillRect(editor, rect, 0xff000000);
  edUpdateYank(editor);
}

void edPaste(Editor editor, float* rect, int flags) {
  if (editor->cutMesh) {
    int x, y;
    float* potRect = editor->cutPotRect;
    int stride = maRectWidth(potRect);
    for (y = 0; y < wbMin(maRectHeight(potRect), maRectHeight(rect)); ++y) {
      for (x = 0; x < wbMin(maRectWidth(potRect), maRectWidth(rect)); ++x) {
        int dx = maRectLeft(rect) + x;
        int dy = maRectTop(rect) + y;
        int color = editor->cutData[y * stride + x];
        edDrawPixel(editor, dx, dy, color, flags);
      }
    }
  }
}

void edUnyank(Editor editor) {
  /* paste at start location. no alpha blending to ensure we restore that rect to exactly the state
   * it had when we yanked */
  edPaste(editor, editor->cutSourceRect, ED_FIGNORE_SELECTION);
  edDiscardYank(editor);
}

/* ---------------------------------------------------------------------------------------------- */

void edBeginSelect(Editor editor) {
  switch (editor->selAnchor) {
    case ED_SELECT_NONE: {
      float p[2];
      edResetSelection(editor);
      p[0] = osMouseX(editor->window);
      p[1] = osMouseY(editor->window);
      edMapToTexture(editor, p);
      maSetRectLeft(editor->selRect, p[0]);
      maSetRectTop(editor->selRect, p[1]);
      maSetRectSize(editor->selRect, 0, 0);
      editor->flags |= ED_FSELECT;
      break;
    }
  }
}

void edSelectedRect(Editor editor, float* rect) {
  float textureRect[4];
  float xoff = 1.5f, yoff = 1.5f;
  osMemCpy(rect, editor->selRect, sizeof(float) * 4);
  if (maRectWidth(rect) < 0) xoff *= -1;
  if (maRectHeight(rect) < 0) yoff *= -1;
  maSetRectSize(rect, (int)(maRectWidth(rect) + xoff), (int)(maRectHeight(rect) + yoff));
  maNormalizeRect(rect);
  maSetRect(textureRect, 0, editor->width, 0, editor->height);
  maClampRect(rect, textureRect);
  maFloorFloats(4, rect, rect);
}

void edDrawSelect(Editor editor) {
  GTexture texture = editor->selBorderTexture;
  float* rect = editor->effectiveSelRect;
  GMesh meshH = gCreateMesh();
  GMesh meshV = gCreateMesh();
  float off = editor->selBlinkTimer * 2;

  /* draw dashed border that alternates between white and black */
  edDashBorderH(editor, meshH, rect, off + 0, 0xffffff);
  edDashBorderH(editor, meshH, rect, off + 2, 0x000000);
  edDashBorderV(editor, meshV, rect, off + 0, 0xffffff);
  edDashBorderV(editor, meshV, rect, off + 2, 0x000000);

  gSetTextureWrapU(texture, G_REPEAT);
  gSetTextureWrapV(texture, G_CLAMP_TO_EDGE);
  gDrawMesh(meshH, editor->transform, texture);

  gSetTextureWrapU(texture, G_CLAMP_TO_EDGE);
  gSetTextureWrapV(texture, G_REPEAT);
  gDrawMesh(meshV, editor->transform, texture);

  gDestroyMesh(meshH);
  gDestroyMesh(meshV);
}

/* ---------------------------------------------------------------------------------------------- */

void edBeginDrawing(Editor editor) {
  switch (editor->tool) {
    case ED_SELECT: { edBeginSelect(editor); break; }
  }
}

void edDrawing(Editor editor) {
  switch (editor->tool) {
    case ED_PENCIL: {
      int color = editor->colors[editor->colorIndex];
      float point[2];
      point[0] = osMouseX(editor->window);
      point[1] = osMouseY(editor->window);
      edMapToTexture(editor, point);
      if (edUpdateDrawRateLimiter(editor)) {
        /* alpha blend unless we're deleting the pixel with complete transparency */
        edDrawPixel(editor, (int)point[0], (int)point[1], color,
          ((color & 0xff000000) == 0xff000000) ? 0 : ED_FALPHA_BLEND);
      }
      break;
    }
    case ED_COLOR_PICKER: {
      edPickColor(editor);
      break;
    }
    /* TODO: not sure if select dragging belongs here */
    case ED_SELECT: {
      switch (editor->selAnchor) {
        case ED_SELECT_NONE: {
          float p[2];
          p[0] = osMouseX(editor->window);
          p[1] = osMouseY(editor->window);
          edMapToTexture(editor, p);
          maSetRectRight(editor->selRect, p[0]);
          maSetRectBottom(editor->selRect, p[1]);
          break;
        }
        case ED_SELECT_MID: {
          float* rect = editor->selRect;
          maSetRectPos(rect,
            maRectX(rect) + osMouseDX(editor->window) / (float)editor->scale,
            maRectY(rect) + osMouseDY(editor->window) / (float)editor->scale);
          if (editor->cutMesh) {
            edUpdateYank(editor);
          } else {
            edSelectedRect(editor, editor->effectiveSelRect);
          }
          break;
        }
      }
    }
  }
}

void edUpdateTransform(Editor editor) {
  GTransformBuilder tbuilder = editor->transformBuilder;
  gResetTransform(tbuilder);
  gSetPosition(tbuilder, editor->oX, editor->oY);
  gSetScale1(tbuilder, editor->scale);
  editor->transform = gBuildTempTransform(tbuilder);
  editor->transformOrtho = gBuildTempTransformOrtho(tbuilder);
  edUpdateYank(editor);
}

void edResetPanning(Editor editor) {
  editor->oX = 100;
  editor->oY = 100;
  edUpdateTransform(editor);
}

void edUpdateMesh(Editor editor) {
  gDestroyMesh(editor->mesh);
  editor->mesh = gCreateMesh();
  gQuad(editor->mesh, 0, 0, editor->width, editor->height);
}

void edMapToTexture(Editor editor, float* point) {
  /* un-transform mouse coordinates so they are relative to the texture */
  gInverseTransformPoint(editor->transformOrtho, point);
  point[0] /= editor->scale;
  point[1] /= editor->scale;
}

void edChangeScale(Editor editor, int direction) {
  float p[2];
  float newScale = editor->scale + direction * (editor->scale / 8 + 1);
  newScale = wbMin(32, newScale);
  newScale = wbMax(1, newScale);
  p[0] = osMouseX(editor->window);
  p[1] = osMouseY(editor->window);
  edMapToTexture(editor, p);
  /* adjust panning so the pixel we're pointing stays under the cursor */
  editor->oX -= (int)((p[0] * newScale) - (p[0] * editor->scale));
  editor->oY -= (int)((p[1] * newScale) - (p[1] * editor->scale));
  editor->scale = newScale;
  edUpdateTransform(editor);
}

void edSave(Editor editor) {
  char* spr;
  edFlushUpdates(editor);
  spr = wbARGBToSprArray(editor->textureData, editor->width, editor->height);
  osWriteEntireFile(editor->filePath, spr, wbArrayLen(spr));
  wbDestroyArray(spr);
}

void edSave1BPP(Editor editor) {
  char* data;
  char* base64Data;
  char* path = 0;
  if (editor->flags & ED_FSELECT) {
    int x, y;
    float* rect = editor->effectiveSelRect;
    int* crop = 0;
    for (y = maRectTop(rect); y < maRectBottom(rect); ++y) {
      for (x = maRectLeft(rect); x < maRectRight(rect); ++x) {
        wbArrayAppend(&crop, editor->textureData[y * editor->width + x]);
      }
    }
    data = gARGBArrayTo1BPP(crop);
    wbDestroyArray(crop);
  } else {
    data = gARGBArrayTo1BPP(editor->textureData);
  }
  base64Data = wbArrayToBase64(data);
  wbDestroyArray(data);
  wbArrayStrCat(&path, editor->filePath);
  wbArrayStrCat(&path, ".txt");
  osWriteEntireFile(path, base64Data, wbStrLen(base64Data));
  wbDestroyArray(path);
  osFree(base64Data);
}

void edLoad(Editor editor) {
  WBSpr spr = wbCreateSprFromFile(editor->filePath);
  if (spr) {
    edFlushUpdates(editor);
    wbDestroyArray(editor->textureData);
    editor->textureData = wbSprToARGBArray(spr);
    editor->width = wbSprWidth(spr);
    editor->height = wbSprHeight(spr);
    edUpdateMesh(editor);
    wbDestroySpr(spr);
    edFlushUpdates(editor);
  }
}

void edHandleKeyDown(Editor editor, int key, int state) {
  switch (key) {
    case OS_MMID: { editor->flags |= ED_FDRAGGING; break; }
    case OS_C: {
      if (editor->tool == ED_COLOR_PICKER) {
        editor->flags ^= ED_FEXPAND_COLOR_PICKER;
      } else {
        editor->tool = ED_COLOR_PICKER;
      }
      break;
    }
    case OS_E: { editor->tool = ED_PENCIL; break; }
    case OS_S: {
      if (state & OS_FCTRL) {
        edSave(editor);
      } else if (state & OS_FSHIFT) {
        edResetSelection(editor);
      } else {
        editor->tool = ED_SELECT;
      }
      break;
    }
    case OS_R: {
      if (editor->flags & ED_FSELECT) {
        edCrop(editor, editor->effectiveSelRect);
      }
      break;
    }
    case OS_1: {
      if (state & OS_FCTRL) {
        edSave1BPP(editor);
      } else {
        edFillRect(editor, editor->effectiveSelRect, editor->colors[0]);
      }
      break;
    }
    case OS_2: { edFillRect(editor, editor->effectiveSelRect, editor->colors[1]); break; }
    case OS_D: {
      if (state & OS_FSHIFT) {
        edUnyank(editor);
      } else {
        edYank(editor);
      }
      break;
    }
    case OS_T: {
      if (edUpdateDrawRateLimiter(editor)) {
        edPaste(editor, editor->effectiveSelRect, ED_FALPHA_BLEND);
      }
      break;
    }
    case OS_MWHEELUP: { edChangeScale(editor, 1); break; }
    case OS_MWHEELDOWN: { edChangeScale(editor, -1); break; }
    case OS_MLEFT:
    case OS_MRIGHT: {
      edResetDrawRateLimiter(editor);
      editor->flags |= ED_FDRAWING;
      editor->colorIndex = key == OS_MRIGHT ? 1 : 0;
      editor->selAnchor = key == OS_MRIGHT ? ED_SELECT_MID : ED_SELECT_NONE;
      edBeginDrawing(editor);
      edDrawing(editor);
      edFlushUpdates(editor);
      break;
    }
    case OS_F1: {
      editor->flags ^= ED_FHELP;
      break;
    }
  }
}

void edHandleKeyUp(Editor editor, int key) {
  switch (key) {
    case OS_MMID: { editor->flags &= ~ED_FDRAGGING; break; }
    case OS_MLEFT:
    case OS_MRIGHT: { editor->flags &= ~ED_FDRAWING; break; }
  }
}

int edHandleMessage(Editor editor) {
  OSWindow window = editor->window;
  switch (osMessageType(window)) {
    case OS_SIZE: {
      edSizeColorPicker(editor);
      break;
    }
    case OS_KEYDOWN: {
      edHandleKeyDown(editor, osKey(window), osKeyState(editor->window));
      break;
    }
    case OS_KEYUP: { edHandleKeyUp(editor, osKey(window)); break; }
    case OS_MOTION: {
      int flags = editor->flags;
      if (flags & ED_FDRAGGING) {
        editor->oX += osMouseDX(window);
        editor->oY += osMouseDY(window);
        edUpdateTransform(editor);
      }
      if (flags & ED_FDRAWING) { edDrawing(editor); }
      break;
    }
    case OS_QUIT: { return 0; }
  }
  return 1;
}

void edUpdate(Editor editor) {
  OSWindow window = editor->window;
  if (editor->flags & ED_FDIRTY) {
    editor->flushTimer += osDeltaTime(window);
    /* flush updates periodically or if there's too many queued */
    if (editor->flushTimer > ED_FLUSH_TIME || wbArrayLen(editor->updates) > ED_FLUSH_THRESHOLD) {
      edFlushUpdates(editor);
      editor->flushTimer = wbMax(0, editor->flushTimer - ED_FLUSH_TIME);
    }
  } else {
    editor->flushTimer = 0;
  }

  editor->selBlinkTimer += osDeltaTime(editor->window);
  editor->selBlinkTimer = maFloatMod(editor->selBlinkTimer, 2);

  edSelectedRect(editor, editor->effectiveSelRect);
}

void edDrawPixel(Editor editor, int x, int y, int color, int flags) {
  float rect[4];
  int fcolor = color;
  maSetRect(rect, 0, editor->width, 0, editor->height);
  if ((editor->flags & ED_FSELECT) && !(flags & ED_FIGNORE_SELECTION)) {
    maClampRect(rect, editor->effectiveSelRect);
  }
  if (maPtInRect(rect, x, y)) {
    int* px = &editor->textureData[y * editor->width + x];
    if (flags & ED_FALPHA_BLEND) {
      /* normal alpha blending with whatever is currently here */
      fcolor = gAlphaBlend(color, *px);
    }
    if (*px != fcolor) {
      EDUpdate* u;
      *px = fcolor;
      u = wbArrayAlloc(&editor->updates, 1);
      u->x = x;
      u->y = y;
      u->color = color;
      u->flags = flags;
      editor->flags |= ED_FDIRTY;
    }
  }
}

void edFlushUpdates(Editor editor) {
  gPixels(editor->texture, editor->width, editor->height,
    editor->textureData);
  wbSetArrayLen(editor->updates, 0);
  editor->flags &= ~ED_FDIRTY;
}

void edResetDrawRateLimiter(Editor editor) {
  editor->lastDrawX = -1;
}

int edUpdateDrawRateLimiter(Editor editor) {
  float p[2];
  int x, y;
  p[0] = osMouseX(editor->window);
  p[1] = osMouseY(editor->window);
  edMapToTexture(editor, p);
  x = (int)p[0];
  y = (int)p[1];
  if (x != editor->lastDrawX || y != editor->lastDrawY) {
    editor->lastDrawX = x;
    editor->lastDrawY = y;
    return 1;
  }
  return 0;
}

void edDrawUpdates(Editor editor) {
  int i;
  GMesh mesh = gCreateMesh();
  EDUpdate* updates = editor->updates;
  for (i = 0; i < wbArrayLen(updates); ++i) {
    EDUpdate* u = &updates[i];
    if (u->flags & ED_FALPHA_BLEND) {
      /* normal alpha blending with whatever is currently here */
      gColor(mesh, u->color);
    } else {
      /* no alpha blending, just overwrite the color.
       * because we can have pending transparent pixels we need to do some big brain custom alpha
       * blending with the checkerboard color at this location */
      int baseColor = edSampleCheckerboard(u->x, u->y);
      float rgba[4];
      gColorToFloats(u->color, rgba);
      gColor(mesh, gMix(u->color & 0xffffff, baseColor, rgba[3]));
    }
    gQuad(mesh, u->x, u->y, 1, 1);
  }
  gDrawMesh(mesh, editor->transform, 0);
  gDestroyMesh(mesh);
}

/* cover rest of the area in grey to distinguish it from a transparent image */
void edDrawBorders(Editor editor) {
  /* TODO: only regenerate mesh when the transform changes */
  int winWidth = osWindowWidth(editor->window);
  int winHeight = osWindowHeight(editor->window);
  int right, bottom;
  int oX = (int)editor->oX, oY = (int)editor->oY;
  int scaledWidth = editor->width * editor->scale;
  int scaledHeight = editor->height * editor->scale;
  GMesh mesh = gCreateMesh();
  gColor(mesh, 0x111111);
  if (oX > 0) {
    gQuad(mesh, 0, 0, oX, winHeight);
  }
  right = oX + scaledWidth;
  if (right < winWidth) {
    gQuad(mesh, right, 0, winWidth - right, winHeight);
  }
  bottom = oY + scaledHeight;
  if (oY > 0) {
    gQuad(mesh, 0, 0, winWidth, oY);
  }
  if (bottom < winHeight) {
    gQuad(mesh, oX, bottom, scaledWidth, winHeight - bottom);
  }
  gDrawMesh(mesh, 0, 0);
  gDestroyMesh(mesh);
}

void edDraw(Editor editor) {
  gDrawMesh(editor->mesh, editor->transform, editor->checkerTexture);
  gDrawMesh(editor->mesh, editor->transform, editor->texture);
  edDrawBorders(editor);
  edDrawUpdates(editor);
  if (editor->cutMesh) {
    GTransform copy = gCloneTransform(editor->cutTransform);
    gMultiplyTransform(copy, editor->transform);
    gDrawMesh(editor->cutMesh, copy, editor->cutTexture);
    gDestroyTransform(copy);
  }
  if (editor->flags & ED_FSELECT) {
    edDrawSelect(editor);
  }
  if (editor->tool == ED_COLOR_PICKER) {
    edDrawColorPicker(editor);
  }
  if (editor->flags & ED_FHELP) {
    gDrawMesh(editor->helpBackgroundMesh, 0, 0);
    gDrawMesh(editor->helpTextMesh, 0, gFontTexture(editor->font));
  }
  gSwapBuffers(editor->window);
}

OSWindow edCreateWindow() {
  OSWindow window = osCreateWindow();
  osSetWindowClass(window, "WeebCoreSpriteEditor");
  osSetWindowName(window, "WeebCore Sprite Editor");
  return window;
}

int edSampleCheckerboard(float x, float y) {
  int data[4] = { 0xaaaaaa, 0x666666, 0x666666, 0xaaaaaa };
  x /= ED_CHECKER_SIZE;
  y /= ED_CHECKER_SIZE;
  return data[((int)y % 2) * 2 + ((int)x % 2)];
}

GTexture edCreateCheckerTexture() {
  int x, y;
  int* data = 0;
  GTexture texture = gCreateTexture();
  for (y = 0; y < ED_CHECKER_SIZE * 2; ++y) {
    for (x = 0; x < ED_CHECKER_SIZE * 2; ++x) {
      wbArrayAppend(&data, edSampleCheckerboard(x, y));
    }
  }
  gPixels(texture, ED_CHECKER_SIZE * 2, ED_CHECKER_SIZE * 2, data);
  wbDestroyArray(data);
  return texture;
}

GTexture edCreateSelBorderTexture() {
  GTexture texture = gCreateTexture();
  int data[4*4] = {
    0xff000000, 0xff000000, 0xff000000, 0xff000000,
    0xff000000, 0x00ffffff, 0x00ffffff, 0xff000000,
    0xff000000, 0x00ffffff, 0x00ffffff, 0xff000000,
    0xff000000, 0xff000000, 0xff000000, 0xff000000
  };
  gPixels(texture, 4, 4, data);
  return texture;
}

int* edCreatePixels(int fillColor, int width, int height) {
  int i;
  int* data = 0;
  for (i = 0; i < width * height; ++i) {
    wbArrayAppend(&data, fillColor);
  }
  return data;
}

GTransform edCreateBackgroundTransform() {
  GTransform transform = gCreateTransform();
  gScale1(transform, ED_CHECKER_SIZE);
  return transform;
}

Editor edCreate(char* filePath) {
  Editor editor = osAlloc(sizeof(struct _Editor));
  editor->scale = 4;
  editor->window = edCreateWindow();
  editor->checkerTexture = edCreateCheckerTexture();
  editor->texture = gCreateTexture();
  editor->cutTexture = gCreateTexture();
  editor->width = ED_TEXSIZE;
  editor->height = ED_TEXSIZE;
  editor->textureData = edCreatePixels(0xff000000, editor->width, editor->height);
  editor->transformBuilder = gCreateTransformBuilder();
  editor->colorPickerTransformBuilder = gCreateTransformBuilder();
  editor->cutTransformBuilder = gCreateTransformBuilder();
  editor->tool = ED_PENCIL;
  editor->grey = 0xffffff;
  editor->colors[1] = 0xff000000;
  editor->filePath = filePath ? filePath : "out.wbspr";
  editor->selBorderTexture = edCreateSelBorderTexture();

  editor->flags |= ED_FHELP;
  editor->font = gDefaultFont();
  /* TODO: scale with screen size */
  /* TODO: actual proper automatic ui layoutting */
  editor->helpTextMesh = gCreateMesh();
  gColor(editor->helpTextMesh, 0xbebebe);
  gFont(editor->helpTextMesh, editor->font, 15, 15, edHelpString());
  editor->helpBackgroundMesh = gCreateMesh();
  gColor(editor->helpBackgroundMesh, 0x33000000);
  gQuad(editor->helpBackgroundMesh, 5, 5, 610, 290);

  edResetSelection(editor);
  edUpdateColorPickerMesh(editor);
  edUpdateMesh(editor);
  edFlushUpdates(editor);
  edSizeColorPicker(editor);
  edResetPanning(editor);
  edLoad(editor);

  return editor;
}

void edDestroy(Editor editor) {
  if (editor) {
    edDiscardYank(editor);
    edResetSelection(editor);
    osDestroyWindow(editor->window);
    gDestroyMesh(editor->cutMesh);
    gDestroyMesh(editor->mesh);
    gDestroyMesh(editor->colorPickerMesh);
    gDestroyMesh(editor->helpTextMesh);
    wbDestroyArray(editor->textureData);
    wbDestroyArray(editor->updates);
    gDestroyTexture(editor->texture);
    gDestroyTexture(editor->cutTexture);
    gDestroyTexture(editor->checkerTexture);
    gDestroyTexture(editor->selBorderTexture);
    gDestroyTransformBuilder(editor->transformBuilder);
    gDestroyTransformBuilder(editor->colorPickerTransformBuilder);
    gDestroyTransformBuilder(editor->cutTransformBuilder);
    gDestroyFont(editor->font);
  }
  osFree(editor);
}

/* ---------------------------------------------------------------------------------------------- */



int main(int argc, char* argv[]) {
  Editor editor = edCreate(argc > 1 ? argv[1] : 0);
  while (1) {
    while (osNextMessage(editor->window)) {
      if (!edHandleMessage(editor)) {
        edDestroy(editor);
        return 0;
      }
    }
    edUpdate(editor);
    edDraw(editor);
  }
  return 0;
}

#define WEEBCORE_IMPLEMENTATION
#include "WeebCore.c"
#include "Platform/Platform.h"
