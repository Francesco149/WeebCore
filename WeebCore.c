#ifndef WEEBCORE_HEADER
#define WEEBCORE_HEADER

/* ---------------------------------------------------------------------------------------------- */
/*                                        PLATFORM LAYER                                          */
/*                                                                                                */
/* you can either include Platform/Platform.h to use the built in sample platform layers or write */
/* your own by defining these structs and funcs                                                   */
/* ---------------------------------------------------------------------------------------------- */

/* opaque handles */
typedef struct _OSWindow* OSWindow;
typedef struct _OSMessage* OSMessage;
typedef struct _OSClock* OSClock;

OSWindow osCreateWindow();
void osDestroyWindow(OSWindow window);
void osSetWindowName(OSWindow window, char* windowName);
void osSetWindowClass(OSWindow window, char* className);
void osSetWindowSize(OSWindow window, int width, int height);
int osWindowWidth(OSWindow window);
int osWindowHeight(OSWindow window);

/* get time elapsed in seconds since the last osSwapBuffers. guaranteed to return non-zero even if
 * no osSwapBuffers happened */
float osDeltaTime(OSWindow window);

/* FPS limiter, 0 for unlimited. limiting happens in gSwapBuffers. note that unlimited fps still
 * waits for the minimum timer resolution for osGetTime */
void osSetWindowFPS(OSWindow window, int fps);

/* fetch one message. returns non-zero as long as there are more */
int osNextMessage(OSWindow window);

/* sends OS_QUIT message to the window */
void osPostQuitMessage(OSWindow window);

/* these funcs get data from the last message fetched by osNextMessage */
int osMessageType(OSWindow window);
int osKey(OSWindow window);
int osKeyState(OSWindow window);
int osMouseX(OSWindow window);
int osMouseY(OSWindow window);
int osMouseDX(OSWindow window);
int osMouseDY(OSWindow window);

/* allocates n bytes and initializes memory to zero */
void* osAlloc(int n);

/* reallocate p to new size n. memory that wasn't initialized is not guaranteed to be zero */
void* osRealloc(void* p, int n);

void osFree(void* p);
void osMemSet(void* p, unsigned char val, int n);
void osMemCpy(void* dst, void* src, int n);

/* write data to disk. returns number of bytes written or < 0 for errors */
int osWriteEntireFile(char* path, void* data, int dataLen);

/* read up to maxSize bytes from disk */
int osReadEntireFile(char* path, void* data, int maxSize);

/* ---------------------------------------------------------------------------------------------- */
/*                                           RENDERER                                             */
/*                                                                                                */
/* you can either include Platform/Platform.h to use the built in sample renderers or write your  */
/* own by defining these structs and funcs                                                        */
/* ---------------------------------------------------------------------------------------------- */

/* opaque handles */
typedef struct _GMesh* GMesh;
typedef struct _GTexture* GTexture;
typedef struct _GTransform* GTransform;
typedef struct _GTransformBuilder* GTransformBuilder;

/* ---------------------------------------------------------------------------------------------- */

/* GMesh is a collection of vertices that will be rendered using the same texture and transform. try
 * to batch up as much stuff into a mesh as possible for optimal performance. */

GMesh gCreateMesh();
void gDestroyMesh(GMesh mesh);

/* change current color. color is ARGB 32-bit int (0xAARRGGBB). 255 alpha = completely transparent,
 * 0 alpha = completely opaque */
void gColor(GMesh mesh, int color);

/* these are for custom meshes.
 * wrap gVertex and gFace calls between gBegin and gEnd.
 * indices start at zero and we are indexing the vertices submitted between gBegin and gEnd. see the
 * implementations of gQuad and gTriangle for examples */
void gBegin(GMesh mesh);
void gEnd(GMesh mesh);
void gVertex(GMesh mesh, float x, float y);
void gTexCoord(GMesh mesh, float u, float v);
void gFace(GMesh mesh, int i1, int i2, int i3);

/* ---------------------------------------------------------------------------------------------- */

/* OpenGL-like post-multiplied transform. matrix memory layout is row major */

GTransform gCreateTransform();
void gDestroyTransform(GTransform transform);
void gDrawMesh(GMesh mesh, GTransform transform, GTexture texture);
GTransform gCloneTransform(GTransform source);
void gLoadIdentity(GTransform transform);
void gLoadMatrix(GTransform transform, float* matrixIn);
void gGetMatrix(GTransform transform, float* matrixOut);
void gScale(GTransform transform, float x, float y);
void gScale1(GTransform transform, float scale);
void gTranslate(GTransform transform, float x, float y);
void gRotate(GTransform transform, float degrees);
void gMultiplyTransform(GTransform transform, GTransform other);
void gMultiplyMatrix(GTransform transform, float* matrixIn);

/* ---------------------------------------------------------------------------------------------- */

GTexture gCreateTexture();
void gDestroyTexture(GTexture texture);

/* set wrap mode for texture coordinates. default is G_REPEAT */
void gSetTextureWrapU(GTexture texture, int mode);
void gSetTextureWrapV(GTexture texture, int mode);

/* set min/mag filter for texture. default is G_NEAREST */
void gSetTextureMinFilter(GTexture texture, int filter);
void gSetTextureMagFilter(GTexture texture, int filter);

/* set the texture's pixel data. must be an array of 0xAARRGGBB colors as explained in gColor.
 * pixels are laid out row major - for example a 4x4 image would be:
 * { px00, px10, px01, px11 }
 * note that this is usually an expensive call. only update the texture data when it's actually
 * changing */
void gPixels(GTexture texture, int width, int height, int* data);

/* same as gPixels but you can specify stride which is how many bytes are between the beginning
 * of each row, for cases when you have extra padding or when you're submitting a sub-region of a
 * bigger image */
void gPixelsEx(GTexture texture, int width, int height, int* data, int stride);

/* ---------------------------------------------------------------------------------------------- */

/* flush all rendered geometry to the screen */
void gSwapBuffers(OSWindow window);

/* set rendering rectangle from the top left of the window, in pixels. this is automatically called
 * when the window is resized. it can also be called manually to render to a subregion of the
 * window. all coordinates passed to other rendering functions start from the top left corner of
 * this rectangle. */
void gViewport(OSWindow window, int x, int y, int width, int height);

/* the initial color of a blank frame */
void gClearColor(int color);

/* ---------------------------------------------------------------------------------------------- */
/*                                         MATH FUNCTIONS                                         */
/*                                                                                                */
/* you can either include Platform/Platform.h to use the built in sample implementations or write */
/* your own by defining these funcs                                                               */
/* ---------------------------------------------------------------------------------------------- */

float maFloatMod(float x, float y); /* returns x modulo y */
float maSin(float degrees);
float maCos(float degrees);
int maCeil(float x);
int maFloor(float x);
float maSqrt(float x);

/* ---------------------------------------------------------------------------------------------- */
/*                                    BUILT-IN MATH FUNCTIONS                                     */
/* ---------------------------------------------------------------------------------------------- */

float maLerp(float a, float b, float amount);
void maLerpFloats(int n, float* a, float* b, float* result, float amount);
void maMulFloatsScalar(int n, float* floats, float* result, float scalar);
void maFloorFloats(int n, float* floats, float* result);
void maClampFloats(int n, float* floats, float* result, float min, float max);
void maAddFloats(int n, float* a, float* b, float* result);

/* these funcs operate on a rectangle represented as an array of 4 floats (left, top, right, bot) */
void maSetRect(float* rect, float left, float right, float top, float bottom);
void maSetRectPos(float* rect, float x, float y);
void maSetRectSize(float* rect, float width, float height);
void maSetRectLeft(float* rect, float left);
void maSetRectRight(float* rect, float right);
void maSetRectTop(float* rect, float top);
void maSetRectBottom(float* rect, float bottom);
void maNormalizeRect(float* rect); /* swaps values around so width/height aren't negative */
void maClampRect(float* rect, float* other); /* clamp rect to be inside of other rect */
float maRectWidth(float* rect);
float maRectHeight(float* rect);
float maRectX(float* rect);
float maRectY(float* rect);
float maRectLeft(float* rect);
float maRectRight(float* rect);
float maRectTop(float* rect);
float maRectBottom(float* rect);
int maPtInRect(float* rect, float x, float y); /* check if xy lies in rect. must be normalized. */

/* ---------------------------------------------------------------------------------------------- */
/*                                          WBSPR FORMAT                                          */
/* this is meant as simple RLE compression for 2D spriteswith a limited color palette             */
/* ---------------------------------------------------------------------------------------------- */

typedef struct _WBSpr* WBSpr;

WBSpr wbCreateSpr(char* data, int length);
WBSpr wbCreateSprFromArray(char* data);
WBSpr wbCreateSprFromFile(char* filePath);
void wbDestroySpr(WBSpr spr);
int wbSprWidth(WBSpr spr);
int wbSprHeight(WBSpr spr);
void wbSprToARGB(WBSpr spr, int* argb);
int* wbSprToARGBArray(WBSpr spr);

/* returns a resizable array that must be freed with wbDestroyArray */
char* wbARGBToSprArray(int* argb, int width, int height);

/* Format Details:
 *
 * char[4] "WBSP"
 * varint formatVersion
 * varint width, height
 * varint paletteLength
 * int32 palette[paletteLength]
 * Blob {
 *   varint type (repeat/data)
 *   varint size
 *   varint paletteIndices[size] <- 1 varint for repeat
 * }
 * ...
 *
 * varint's are protobuf style encoded integers. see wbEncodeVarInt32 */

/* ---------------------------------------------------------------------------------------------- */
/*                                          RENDER UTILS                                          */
/* ---------------------------------------------------------------------------------------------- */

/* create a transform from scale, position, origin, rotation applied in a fixed order */

GTransformBuilder gCreateTransformBuilder();
void gDestroyTransformBuilder(GTransformBuilder builder);
void gResetTransform(GTransformBuilder builder);
void gSetScale(GTransformBuilder builder, float x, float y);
void gSetScale1(GTransformBuilder builder, float scale);
void gSetPosition(GTransformBuilder builder, float x, float y);
void gSetOrigin(GTransformBuilder builder, float x, float y);
void gSetRotation(GTransformBuilder builder, float degrees);
GTransform gBuildTransform(GTransformBuilder build);

/* the ortho version of gBuild* functions produce a matrix that is orthogonal (doesn't apply scale
 * among other things) this is useful for trivial inversion */
GTransform gBuildTransformOrtho(GTransformBuilder build);

/* the transform returned by this does not need to be destroyed. it will be automatically destroyed
 * when gDestroyTransformBuilder is called.
 *
 * note that subsequent calls to this function will invalidate the previously generated transform.
 *
 * Ortho version does not share the same transform as the non-Ortho so it doesnt invalidate it */
GTransform gBuildTempTransform(GTransformBuilder build);
GTransform gBuildTempTransformOrtho(GTransformBuilder build);

/* transform 2D point in place */
void gTransformPoint(GTransform transform, float* point);

/* transform 2D point in place by inverse of transform note that this only works if the matrix is
 * orthogonal, which means that it cannot have scale */
void gInverseTransformPoint(GTransform transform, float* point);

/* add a rectangle to mesh */
void gQuad(GMesh mesh, float x, float y, float width, float height);
void gTexturedQuad(GMesh mesh,
  float x, float y, float u, float v,
  float width, float height, float uWidth, float vHeight
);

/* add a triangle to mesh */
void gTriangle(GMesh mesh,
  float x1, float y1, float x2, float y2, float x3, float y3);
void gTexturedTriangle(GMesh mesh,
  float x1, float y1, float u1, float v1,
  float x2, float y2, float u2, float v2,
  float x3, float y3, float u3, float v3
);

/* draw gradients using vertex color interpolation */

void gQuadGradientH(GMesh mesh, float x, float y, float width, float height, int n, int* colors);
void gQuadGradientV(GMesh mesh, float x, float y, float width, float height, int n, int* colors);

/* convert ARGB color to { r, g, b, a } (0.0-1.0) */
void gColorToFloats(int color, float* floats);

/* convert { r, g, b, a } (0.0-1.0) to ARGB color */
int gFloatsToColor(float* f);

/* linearly interpolate between colors a and b */
int gMix(int a, int b, float amount);

/* multiply color's rgb values by scalar (does not touch alpha) */
int gMulScalar(int color, float scalar);

/* add colors together (clamps values to avoid overflow) */
int gAdd(int a, int b);

/* alpha blend between color src and dst. alpha is not premultiplied */
int gAlphaBlend(int src, int dst);

/* convert rgba pixels to 1bpp. all non-transparent colors become a 1 and transparency becomes 0.
 * the data is tightly packed (8 pixels per byte).
 * the returned data is an array and must be freed with wbArrayFree */
char* gARGBTo1BPP(int* pixels, int numPixels);
char* gARGBArrayTo1BPP(int* pixels);
int* g1BPPToARGB(char* data, int numBytes);
int* g1BPPArrayToARGB(char* data);

/* ---------------------------------------------------------------------------------------------- */
/*                                        BITMAP FONTS                                            */
/* ---------------------------------------------------------------------------------------------- */

typedef struct _GFont* GFont;

/* simplest form of bitmap font.
 *
 * this assumes pixels is a tightly packed grid of 32x3 characters that cover ascii 0x20-0x7f
 * (from space to the end of the ascii table).
 *
 * while this is the fastest way for the renderer to look up characters, it should only be used in
 * cases where you have a hardcoded font that's never gonna change. it was created to bootstrap
 * the built-in font */
GFont gCreateFontFromSimpleGrid(int* pixels, int width, int height, int charWidth, int charHeight);
GFont gCreateFontFromSimpleGridFile(char* filePath, int charWidth, int charHeight);

/* basic built in bitmap font */
GFont gDefaultFont();

void gDestroyFont(GFont font);
GTexture gFontTexture(GFont font);

/* generate vertices and uv's for drawing string. must be rendered with the font texture from
 * gFontTexture or a texture that has the same exact layout */
void gFont(GMesh mesh, GFont font, int x, int y, char* string);

void gDrawFont(GFont font, int x, int y, char* string);

/* ---------------------------------------------------------------------------------------------- */
/*                                       RESIZABLE ARRAYS                                         */
/*                                                                                                */
/* make sure your initial pointer is initialized to NULL which counts as an empty array.          */
/* these are special fat pointers and must be freed with wbDestroyArray                           */
/* ---------------------------------------------------------------------------------------------- */

void wbDestroyArray(void* array);
int wbArrayLen(void* array);
void wbSetArrayLen(void* array, int len);

/* shorthand macro to append a single element to the array */
#define wbArrayAppend(pArray, x) { \
  int len = wbArrayLen(*(pArray)); \
  wbArrayReserve((pArray), 1); \
  (*(pArray))[len] = (x); \
  wbSetArrayLen(*(pArray), len + 1); \
}

/* reserve memory for at least numElements extra elements.
 * returns a pointer to the end of the array */
#define wbArrayReserve(pArray, numElements) \
  wbArrayReserveEx((void**)(pArray), sizeof((*pArray)[0]), numElements)

/* reserve memory for at least numElements extra elements and set the
 * array length to current length + numElements.
 * returns a pointer to the beginning of the new elements */
#define wbArrayAlloc(pArray, numElements) \
  wbArrayAllocEx((void**)(pArray), sizeof((*pArray)[0]), numElements)

void wbArrayStrCat(char** pArray, char* str);

void* wbArrayReserveEx(void** pArray, int elementSize, int numElements);
void* wbArrayAllocEx(void** pArray, int elementSize, int numElements);

/* ---------------------------------------------------------------------------------------------- */
/*                                   MISC UTILS AND MACROS                                        */
/* ---------------------------------------------------------------------------------------------- */

#define wbMin(x, y) ((x) < (y) ? (x) : (y))
#define wbMax(x, y) ((x) > (y) ? (x) : (y))
#define wbClamp(x, min, max) wbMax(min, wbMin(max, x))

/* (a3, b3) = (a1, b1) - (a2, b2) */
#define wbSub2(a1, b1, a2, b2, a3, b3) \
  ((a3) = (a1) - (a2)), \
  ((b3) = (b1) - (b2))

/* cross product between (x1, y1) and (x2, y2) */
#define wbCross(x1, y1, x2, y2) ((x1) * (y2) - (y1) * (x2))

#define wbToRadians(degrees) ((degrees) / 360 * WB_PI * 2)

/* return the closest power of two that is higher than x */
int wbRoundUpToPowerOfTwo(int x);

/* null-terminated string utils */
int wbStrLen(char* s);
void wbStrCopy(char* dst, char* src);

/* mem utils */
int wbMemCmp(void* a, void* b, int n);

/* protobuf style varints. encoded in base 128. each byte contains 7 bits of the integer and the
 * msb is set if there's more. byte order is little endian */

int wbDecodeVarInt32(char** pData);          /* increments *pData past the varint */
int wbEncodeVarInt32(void* data, int x);     /* returns num of bytes written */
void wbAppendVarInt32(char** pArray, int x); /* append to resizable arr */

/* encode integers in little endian */
int wbEncodeInt32(void* data, int x);
int wbDecodeInt32(char** pData);
void wbAppendInt32(char** pArray, int x);

void wbSwapFloats(float* a, float* b);

/* encode data to a base64 string. returned string must be freed with osFree */
char* wbToBase64(void* data, int dataSize);
char* wbArrayToBase64(char* data);
char* wbArrayFromBase64(char* base64Data);

/* ---------------------------------------------------------------------------------------------- */
/*                                      ENUMS AND CONSTANTS                                       */
/* ---------------------------------------------------------------------------------------------- */

#define WB_PI 3.141592653589793238462643383279502884

/* message types returned by osMessageType */
enum {
  OS_QUIT = 1,
  OS_KEYDOWN,
  OS_KEYUP,
  OS_MOTION,
  OS_SIZE,
  OS_LAST_EVENT_TYPE
};

/* flags for the bitfield returned by osMessageKeyState */
enum {
  OS_FSHIFT     = 1<<1,
  OS_FCONTROL   = 1<<2,
  OS_FALT       = 1<<3,
  OS_FSUPER     = 1<<4,
  OS_FCAPS_LOCK = 1<<5,
  OS_FNUM_LOCK  = 1<<6,
  OS_FREPEAT    = 1<<7,
  OS_FLAST_STATE
};

#define OS_FCTRL OS_FCONTROL

#define OS_MLEFT OS_MOUSE1
#define OS_MMID OS_MOUSE2
#define OS_MRIGHT OS_MOUSE3
#define OS_MWHEELUP OS_MOUSE4
#define OS_MWHEELDOWN OS_MOUSE5

/* keys returned by osMessageKey */
enum {
  OS_MOUSE1          =   1,
  OS_MOUSE2          =   2,
  OS_MOUSE3          =   3,
  OS_MOUSE4          =   4,
  OS_MOUSE5          =   5,
  OS_SPACE           =  32,
  OS_APOSTROPHE      =  39,
  OS_COMMA           =  44,
  OS_MINUS           =  45,
  OS_PERIOD          =  46,
  OS_SLASH           =  47,
  OS_0               =  48,
  OS_1               =  49,
  OS_2               =  50,
  OS_3               =  51,
  OS_4               =  52,
  OS_5               =  53,
  OS_6               =  54,
  OS_7               =  55,
  OS_8               =  56,
  OS_9               =  57,
  OS_SEMICOLON       =  59,
  OS_EQUAL           =  61,
  OS_A               =  65,
  OS_B               =  66,
  OS_C               =  67,
  OS_D               =  68,
  OS_E               =  69,
  OS_F               =  70,
  OS_G               =  71,
  OS_H               =  72,
  OS_I               =  73,
  OS_J               =  74,
  OS_K               =  75,
  OS_L               =  76,
  OS_M               =  77,
  OS_N               =  78,
  OS_O               =  79,
  OS_P               =  80,
  OS_Q               =  81,
  OS_R               =  82,
  OS_S               =  83,
  OS_T               =  84,
  OS_U               =  85,
  OS_V               =  86,
  OS_W               =  87,
  OS_X               =  88,
  OS_Y               =  89,
  OS_Z               =  90,
  OS_LEFT_BRACKET    =  91,
  OS_BACKSLASH       =  92,
  OS_RIGHT_BRACKET   =  93,
  OS_GRAVE_ACCENT    =  96,
  OS_WORLD_1         = 161,
  OS_WORLD_2         = 162,
  OS_ESCAPE          = 256,
  OS_ENTER           = 257,
  OS_TAB             = 258,
  OS_BACKSPACE       = 259,
  OS_INSERT          = 260,
  OS_DELETE          = 261,
  OS_RIGHT           = 262,
  OS_LEFT            = 263,
  OS_DOWN            = 264,
  OS_UP              = 265,
  OS_PAGE_UP         = 266,
  OS_PAGE_DOWN       = 267,
  OS_HOME            = 268,
  OS_END             = 269,
  OS_CAPS_LOCK       = 280,
  OS_SCROLL_LOCK     = 281,
  OS_NUM_LOCK        = 282,
  OS_PRINT_SCREEN    = 283,
  OS_PAUSE           = 284,
  OS_F1              = 290,
  OS_F2              = 291,
  OS_F3              = 292,
  OS_F4              = 293,
  OS_F5              = 294,
  OS_F6              = 295,
  OS_F7              = 296,
  OS_F8              = 297,
  OS_F9              = 298,
  OS_F10             = 299,
  OS_F11             = 300,
  OS_F12             = 301,
  OS_F13             = 302,
  OS_F14             = 303,
  OS_F15             = 304,
  OS_F16             = 305,
  OS_F17             = 306,
  OS_F18             = 307,
  OS_F19             = 308,
  OS_F20             = 309,
  OS_F21             = 310,
  OS_F22             = 311,
  OS_F23             = 312,
  OS_F24             = 313,
  OS_F25             = 314,
  OS_KP_0            = 320,
  OS_KP_1            = 321,
  OS_KP_2            = 322,
  OS_KP_3            = 323,
  OS_KP_4            = 324,
  OS_KP_5            = 325,
  OS_KP_6            = 326,
  OS_KP_7            = 327,
  OS_KP_8            = 328,
  OS_KP_9            = 329,
  OS_KP_DECIMAL      = 330,
  OS_KP_DIVIDE       = 331,
  OS_KP_MULTIPLY     = 332,
  OS_KP_SUBTRACT     = 333,
  OS_KP_ADD          = 334,
  OS_KP_ENTER        = 335,
  OS_KP_EQUAL        = 336,
  OS_LEFT_SHIFT      = 340,
  OS_LEFT_CONTROL    = 341,
  OS_LEFT_ALT        = 342,
  OS_LEFT_SUPER      = 343,
  OS_RIGHT_SHIFT     = 344,
  OS_RIGHT_CONTROL   = 345,
  OS_RIGHT_ALT       = 346,
  OS_RIGHT_SUPER     = 347,
  OS_MENU            = 348,
  OS_LAST_KEY
};

/* texture wrap modes */
enum {
  G_CLAMP_TO_EDGE,
  G_MIRRORED_REPEAT,
  G_REPEAT,
  G_LAST_TEXTURE_WRAP
};

/* texture filters */
enum {
  G_NEAREST,
  G_LINEAR,
  G_LAST_TEXTURE_FILTER
};

#endif /* WEEBCORE_HEADER */

/* ############################################################################################## */
/* ############################################################################################## */
/* ############################################################################################## */
/*                                                                                                */
/*      Header part ends here. This is all you need to know to use it. Implementation below.      */
/*                                                                                                */
/* ############################################################################################## */
/* ############################################################################################## */
/* ############################################################################################## */

#if defined(WEEBCORE_IMPLEMENTATION) && !defined(WEEBCORE_OVERRIDE_MONOLITHIC_BUILD)

/* ---------------------------------------------------------------------------------------------- */

typedef struct _WBArrayHeader {
  int capacity;
  int length;
} WBArrayHeader;

static WBArrayHeader* wbArrayHeader(void* array) {
  if (!array) return 0;
  return (WBArrayHeader*)array - 1;
}

void wbDestroyArray(void* array) {
  osFree(wbArrayHeader(array));
}

int wbArrayLen(void* array) {
  if (!array) return 0;
  return wbArrayHeader(array)->length;
}

void wbSetArrayLen(void* array, int length) {
  if (array) {
    wbArrayHeader(array)->length = length;
  }
}

void wbArrayStrCat(char** pArray, char* str) {
  for (; *str; ++str) {
    wbArrayAppend(pArray, *str);
  }
}

void* wbArrayReserveEx(void** pArray, int elementSize, int numElements) {
  WBArrayHeader* header;
  if (!*pArray) {
    int capacity = wbRoundUpToPowerOfTwo(wbMax(numElements, 16));
    header = osAlloc(sizeof(WBArrayHeader) + elementSize * capacity);
    header->capacity = capacity;
    *pArray = header + 1;
  } else {
    int minCapacity;
    header = wbArrayHeader(*pArray);
    minCapacity = header->length + numElements;
    if (header->capacity < minCapacity) {
      int newSize;
      while (header->capacity < minCapacity) {
        header->capacity *= 2;
      }
      newSize = sizeof(WBArrayHeader) + elementSize * header->capacity;
      header = osRealloc(header, newSize);
      *pArray = header + 1;
    }
  }
  return (char*)*pArray + wbArrayLen(*pArray) * elementSize;
}

void* wbArrayAllocEx(void** pArray, int elementSize, int numElements) {
  void* res = wbArrayReserveEx(pArray, elementSize, numElements);
  wbSetArrayLen(*pArray, wbArrayLen(*pArray) + numElements);
  return res;
}

/* ---------------------------------------------------------------------------------------------- */

/* https://graphics.stanford.edu/~seander/bithacks.html */
int wbRoundUpToPowerOfTwo(int x) {
  --x;
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  x |= x >> 16;
  return ++x;
}

int wbStrLen(char* s) {
  char* p;
  for (p = s; *p; ++p);
  return p - s;
}

void wbStrCopy(char* dst, char* src) {
  while (*src) {
    *dst++ = *src++;
  }
}

int wbDecodeVarInt32(char** pData) {
  unsigned char* p;
  int res = 0, shift = 0, more = 1;
  for (p = (unsigned char*)*pData; more; ++p) {
    more = *p & 0x80;
    res |= (*p & 0x7F) << shift;
    shift += 7;
  }
  *pData = (char*)p;
  return res;
}

int wbEncodeVarInt32(void* data, int x) {
  unsigned char* p = data;
  unsigned u = (unsigned)x;
  while (1) {
    unsigned char* byte = p++;
    *byte = u & 0x7F;
    u >>= 7;
    if (u) {
      *byte |= 0x80;
    } else {
      break;
    }
  }
  return p - (unsigned char*)data;
}

void wbAppendVarInt32(char** pArray, int x) {
  char* p = wbArrayReserve(pArray, 4);
  int n = wbEncodeVarInt32(p, x);
  wbSetArrayLen(*pArray, wbArrayLen(*pArray) + n);
}

int wbEncodeInt32(void* data, int x) {
  unsigned char* p = data;
  p[0] = (x & 0x000000ff) >>  0;
  p[1] = (x & 0x0000ff00) >>  8;
  p[2] = (x & 0x00ff0000) >> 16;
  p[3] = (x & 0xff000000) >> 24;
  return 4;
}

int wbDecodeInt32(char** pData) {
  unsigned char* p = (unsigned char*)*pData;
  *pData += 4;
  return (p[0] <<  0) | (p[1] <<  8) | (p[2] << 16) | (p[3] << 24);
}

void wbAppendInt32(char** pArray, int x) {
  char* p = wbArrayAlloc(pArray, 4);
  wbEncodeInt32(p, x);
}

int wbMemCmp(void* a, void* b, int n) {
  unsigned char* p1 = a;
  unsigned char* p2 = b;
  int i;
  for (i = 0; i < n; ++i) {
    if (*p1 < *p2) {
      return -1;
    } else if (*p1 > *p2) {
      return 1;
    }
  }
  return 0;
}

void wbSwapFloats(float* a, float* b) {
  float tmp = *a;
  *a = *b;
  *b = tmp;
}

/* ---------------------------------------------------------------------------------------------- */

struct _GTransformBuilder {
  float sX, sY;
  float x, y;
  float oX, oY;
  float degrees;
  GTransform tempTransform, tempTransformOrtho;
};

GTransformBuilder gCreateTransformBuilder() {
  GTransformBuilder builder = osAlloc(sizeof(struct _GTransformBuilder));
  builder->tempTransform = gCreateTransform();
  builder->tempTransformOrtho = gCreateTransform();
  gResetTransform(builder);
  return builder;
}

void gDestroyTransformBuilder(GTransformBuilder builder) {
  gDestroyTransform(builder->tempTransform);
  gDestroyTransform(builder->tempTransformOrtho);
  osFree(builder);
}

void gResetTransform(GTransformBuilder builder) {
  builder->sX = 1;
  builder->sY = 1;
  builder->x = builder->y =
  builder->oX = builder->oY =
  builder->degrees = 0;
}

void gSetScale(GTransformBuilder builder, float x, float y) {
  builder->sX = x;
  builder->sY = y;
}

void gSetScale1(GTransformBuilder builder, float scale) {
  gSetScale(builder, scale, scale);
}

void gSetPosition(GTransformBuilder builder, float x, float y) {
  builder->x = x;
  builder->y = y;
}

void gSetOrigin(GTransformBuilder builder, float x, float y) {
  builder->oX = x;
  builder->oY = y;
}

void gSetRotation(GTransformBuilder builder, float degrees) {
  builder->degrees = degrees;
}

static void gComputeTransformBuilder(GTransformBuilder builder,
  GTransform transform, int ortho)
{
  gTranslate(transform, builder->x, builder->y);
  gRotate(transform, builder->degrees);
  if (!ortho) {
    gScale(transform, builder->sX, builder->sY);
  }
  gTranslate(transform, -builder->oX, -builder->oY);
}

GTransform gBuildTransform(GTransformBuilder builder) {
  GTransform transform = gCreateTransform();
  gComputeTransformBuilder(builder, transform, 0);
  return transform;
}

GTransform gBuildTempTransform(GTransformBuilder builder) {
  gLoadIdentity(builder->tempTransform);
  gComputeTransformBuilder(builder, builder->tempTransform, 0);
  return builder->tempTransform;
}

GTransform gBuildTransformOrtho(GTransformBuilder builder) {
  GTransform transform = gCreateTransform();
  gComputeTransformBuilder(builder, transform, 1);
  return transform;
}

GTransform gBuildTempTransformOrtho(GTransformBuilder builder) {
  gLoadIdentity(builder->tempTransformOrtho);
  gComputeTransformBuilder(builder, builder->tempTransformOrtho, 1);
  return builder->tempTransformOrtho;
}

/* ---------------------------------------------------------------------------------------------- */

void gTransformPoint(GTransform transform, float* point) {
  GTransform copy = gCloneTransform(transform);
  float mPoint[16];
  osMemSet(mPoint, 0, sizeof(mPoint));
  osMemCpy(mPoint, point, sizeof(float) * 2);
  mPoint[3] = 1;
  gMultiplyMatrix(copy, mPoint);
  gGetMatrix(copy, mPoint);
  osMemCpy(point, mPoint, sizeof(float) * 2);
  gDestroyTransform(copy);
}

void gInverseTransformPoint(GTransform transform, float* point) {
  /* https://en.wikibooks.org/wiki/GLSL_Programming/Applying_Matrix_Transformations#Transforming_Points_with_the_Inverse_Matrix */
  GTransform copy = gCreateTransform();
  float m[16];
  gGetMatrix(transform, m);
  point[0] -= m[12];
  point[1] -= m[13];
  m[12] = m[13] = m[14] = m[15] = m[3] = m[7] = m[11] = 0; /* equivalent of taking the 3x3 matrix */
  gLoadMatrix(copy, m);
  gTransformPoint(copy, point);
  gDestroyTransform(copy);
}

void gTexturedTriangle(GMesh mesh,
  float x1, float y1, float u1, float v1,
  float x2, float y2, float u2, float v2,
  float x3, float y3, float u3, float v3
) {
  /* ensure vertex order is counter-clockwise so we can cull backfaces */
  float v1x, v1y, v2x, v2y;
  float i2 = 1, i3 = 2;
  wbSub2(x2, y2, x1, y1, v1x, v1y);
  wbSub2(x3, y3, x1, y1, v2x, v2y);
  if (wbCross(v1x, v1y, v2x, v2y) > 0) {
    i2 = 2, i3 = 1;
  }
  gBegin(mesh);
  gVertex(mesh, x1, y1);
  gTexCoord(mesh, u1, v1);
  gVertex(mesh, x2, y2);
  gTexCoord(mesh, u2, v2);
  gVertex(mesh, x3, y3);
  gTexCoord(mesh, u3, v3);
  gFace(mesh, 0, i2, i3);
  gEnd(mesh);
}

void gTriangle(GMesh mesh, float x1, float y1, float x2, float y2, float x3, float y3) {
  gTexturedTriangle(mesh,
    x1, y1, 0, 0,
    x2, y2, 0, 0,
    x3, y3, 0, 0
  );
}

void gTexturedQuad(GMesh mesh, float x, float y, float u, float v,
  float width, float height, float uWidth, float vHeight
) {
  gBegin(mesh);
  gVertex(mesh,   x,          y          );
  gTexCoord(mesh, u,          v          );
  gVertex(mesh,   x,          y + height );
  gTexCoord(mesh, u,          v + vHeight);
  gVertex(mesh,   x + width,  y + height );
  gTexCoord(mesh, u + uWidth, v + vHeight);
  gVertex(mesh,   x + width,  y          );
  gTexCoord(mesh, u + uWidth, v          );
  gFace(mesh, 0, 1, 2);
  gFace(mesh, 0, 2, 3);
  gEnd(mesh);
}


void gQuad(GMesh mesh, float x, float y, float width, float height) {
  gTexturedQuad(mesh, x, y, 0, 0, width, height, width, height);
}


void gQuadGradientH(GMesh mesh,
  float x, float y, float width, float height, int n, int* colors)
{
  int i;
  float off;
  gBegin(mesh);
  for (i = 0; i < n; ++i) {
    int s = i * 2;
    off = (float)i / (n - 1) * width;
    gColor(mesh, colors[i]);
    gVertex(mesh, x + off, y);
    gVertex(mesh, x + off, y + height);
    if (i < n - 1) {
      gFace(mesh, s + 0, s + 1, s + 2);
      gFace(mesh, s + 1, s + 3, s + 2);
    }
  }
  gEnd(mesh);
}

void gQuadGradientV(GMesh mesh,
  float x, float y, float width, float height, int n, int* colors)
{
  int i;
  float off;
  gBegin(mesh);
  for (i = 0; i < n; ++i) {
    int s = i * 2;
    off = (float)i / (n - 1) * height;
    gColor(mesh, colors[i]);
    gVertex(mesh, x + width, y + off);
    gVertex(mesh,         x, y + off);
    if (i < n - 1) {
      gFace(mesh, s + 0, s + 1, s + 2);
      gFace(mesh, s + 1, s + 3, s + 2);
    }
  }
  gEnd(mesh);
}

void gColorToFloats(int c, float* floats) {
  floats[0] = ((c & 0x00ff0000) >> 16) / 255.0f;
  floats[1] = ((c & 0x0000ff00) >>  8) / 255.0f;
  floats[2] = ((c & 0x000000ff) >>  0) / 255.0f;
  floats[3] = ((c & 0xff000000) >> 24) / 255.0f;
}

int gFloatsToColor(float* f) {
  int color = 0;
  color |= (int)(f[0] * 255.0f) << 16;
  color |= (int)(f[1] * 255.0f) <<  8;
  color |= (int)(f[2] * 255.0f) <<  0;
  color |= (int)(f[3] * 255.0f) << 24;
  return color;
}

char* wbToBase64(void* vdata, int dataSize) {
  static char* charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  int i, len = (dataSize / 3 + 1) * 4 + 1;
  char* result = osAlloc(len);
  char* p = result;
  unsigned char* data = vdata;
  for (i = 0; i < dataSize; i += 3) {
    p = &result[i / 3 * 4];
    switch (dataSize - i) {
      default: /* abusing fallthrough because it's fun */
      case 3:
        p[3] = charset[data[i + 2] & 0x3f];
        p[2] = ((data[i + 1] & 0x0f) << 2) | ((data[i + 2] & 0xc0) >> 6);
      case 2:
        p[2] = charset[p[2] | (data[i + 1] & 0x0f) << 2];
        p[1] = ((data[i] & 0x03) << 4) | ((data[i + 1] & 0xf0) >> 4);
      case 1:
        p[1] = charset[p[1] | (data[i] & 0x03) << 4];
        p[0] = charset[(data[i] & 0xfc) >> 2];
    }
  }
  if (!p[2]) p[2] = '=';
  if (!p[3]) p[3] = '=';
  return result;
}

#define NLETTERS ('Z' - 'A' + 1)

static char wbDecodeBase64(char c) {
  if (c >= 'A' && c <= 'Z') {
    c -= 'A';
  } else if (c >= 'a' && c <= 'z') {
    c = c - 'a' + NLETTERS;
  } else if (c >= '0' && c <= '9') {
    c = c - '0' + NLETTERS * 2;
  } else if (c == '+') {
    c = NLETTERS * 2 + 10;
  } else if (c == '/') {
    c = NLETTERS * 2 + 10 + 1;
  } else {
    c = 0;
  }
  return c;
}

char* wbArrayFromBase64(char* data) {
  char* result = 0;
  int len = wbStrLen(data), i;
  for (i = 0; i < len; i += 4) {
    int chunkLen = wbMin(3, len - i);
    char* p = wbArrayAlloc(&result, chunkLen);
    osMemSet(p, 0, chunkLen);
    switch (len - i) {
      default:
      case 4:
        p[2] |= wbDecodeBase64(data[i + 3]);        /* 6 bits */
      case 3:
        p[2] |= (wbDecodeBase64(data[i + 2]) << 6); /* 2 bits */
        p[1] |= wbDecodeBase64(data[i + 2]) >> 2;   /* 4 bits */
      case 2:
        p[1] |= (wbDecodeBase64(data[i + 1]) << 4); /* 4 bits */
        p[0] |= wbDecodeBase64(data[i + 1]) >> 4;   /* 2 bits */
      case 1:
        p[0] |= wbDecodeBase64(data[i]) << 2;       /* 6 bits */
    }
  }
  return result;
}

char* wbArrayToBase64(char* data) {
  return wbToBase64(data, wbArrayLen(data));
}

char* gARGBTo1BPP(int* pixels, int numPixels) {
  char* data = 0;
  int i, j;
  for (i = 0; i < numPixels;) {
    char b = 0;
    for (j = 0; j < 8 && i < numPixels; ++j, ++i) {
      if ((pixels[i] & 0xff000000) != 0xff000000) {
        b |= 1 << (7 - j);
      }
    }
    wbArrayAppend(&data, b);
  }
  return data;
}

char* gARGBArrayTo1BPP(int* pixels) {
  return gARGBTo1BPP(pixels, wbArrayLen(pixels));
}

int* g1BPPToARGB(char* data, int numBytes) {
  int* result = 0;
  int i, j;
  for (i = 0; i < numBytes; ++i) {
    for (j = 0; j < 8; ++j) {
      if (data[i] & (1 << (7 - j))) {
        wbArrayAppend(&result, 0x00000000);
      } else {
        wbArrayAppend(&result, 0xff000000);
      }
    }
  }
  return result;
}

int* g1BPPArrayToARGB(char* data) {
  return g1BPPToARGB(data, wbArrayLen(data));
}

int gMix(int a, int b, float amount) {
  float fa[4], fb[4];
  gColorToFloats(a, fa);
  gColorToFloats(b, fb);
  maLerpFloats(4, fa, fb, fa, amount);
  maClampFloats(4, fa, fa, 0, 1);
  return gFloatsToColor(fa);
}

static int gMulScalarUnchecked(int color, float scalar) {
  float rgba[4];
  gColorToFloats(color, rgba);
  maMulFloatsScalar(3, rgba, rgba, scalar);
  return gFloatsToColor(rgba);
}

int gMulScalar(int color, float scalar) {
  float rgba[4];
  gColorToFloats(color, rgba);
  maMulFloatsScalar(3, rgba, rgba, scalar);
  maClampFloats(3, rgba, rgba, 0, 1);
  return gFloatsToColor(rgba);
}

int gAdd(int a, int b) {
  float fa[4], fb[4];
  gColorToFloats(a, fa);
  gColorToFloats(b, fb);
  maAddFloats(4, fa, fb, fa);
  maClampFloats(4, fa, fa, 0, 1);
  return gFloatsToColor(fa);
}

int gAlphaBlend(int src, int dst) {
  int result;
  float d[4], s[4], da, sa;
  float outAlpha;
  gColorToFloats(dst, d);
  gColorToFloats(src, s);
  sa = s[3] = 1 - s[3]; /* engine uses inverted alpha, but here we need the real alpha */
  da = d[3] = 1 - d[3];
  /*
   * https://en.wikipedia.org/wiki/Alpha_compositing
   *
   * outAlpha = srcAlpha + dstAlpha * (1 - srcAlpha)
   * finalRGB = (srcRGB * srcAlpha + dstRGB * dstAlpha * (1 - srcAlpha)) / outAlpha
   */
  outAlpha = sa + da * (1 - sa);
  src = gMulScalarUnchecked(src & 0xffffff, sa);
  dst = gMulScalarUnchecked(dst & 0xffffff, da);
  result = src + gMulScalarUnchecked(dst, 1 - sa);
  result = gMulScalarUnchecked(result, 1.0f / outAlpha);
  result |= (int)((1 - outAlpha) * 255) << 24; /* invert alpha back */
  return result;
}

/* ---------------------------------------------------------------------------------------------- */

struct _GFont {
  GTexture texture;
  int charWidth, charHeight;
};

GFont gCreateFontFromSimpleGrid(int* pixels, int width, int height, int charWidth, int charHeight) {
  GFont font = osAlloc(sizeof(struct _GFont));
  if (font) {
    int* decolored = 0;
    int i;
    font->texture = gCreateTexture();
    for (i = 0; i < width * height; ++i) {
      /* we want the base pixels to be white so it can be colored */
      wbArrayAppend(&decolored, pixels[i] | 0xffffff);
    }
    gPixels(font->texture, width, height, decolored);
    wbDestroyArray(decolored);
    font->charWidth = charWidth;
    font->charHeight = charHeight;
  }
  return font;
}

GFont gCreateFontFromSimpleGridFile(char* filePath, int charWidth, int charHeight) {
  WBSpr spr = wbCreateSprFromFile(filePath);
  if (spr) {
    GFont font;
    int* textureData = wbSprToARGBArray(spr);
    int width = wbSprWidth(spr), height = wbSprHeight(spr);
    font = gCreateFontFromSimpleGrid(textureData, width, height, charWidth, charHeight);
    wbDestroyArray(textureData);
    wbDestroySpr(spr);
    return font;
  }
  return 0;
}

void gDestroyFont(GFont font) {
  if (font) {
    gDestroyTexture(font->texture);
  }
  osFree(font);
}

GTexture gFontTexture(GFont font) { return font->texture; }

static int* wbPadPixels(int* pixels, int width, int height, int newWidth, int newHeight) {
  int* newPixels = 0;
  int x, y;
  for (y = 0; y < newHeight; ++y) {
    if (y < height) {
      for (x = 0; x < newWidth; ++x) {
        if (x < width) {
          wbArrayAppend(&newPixels, pixels[y * width + x]);
        } else {
          wbArrayAppend(&newPixels, 0xff000000);
        }
      }
    } else {
      wbArrayAppend(&newPixels, 0xff000000);
    }
  }
  return newPixels;
}

GFont gDefaultFont() {
  /* generated from font.wbspr using the SpriteEditor */
  static char* base64Data =
    "AIUQIQYIIIAAAAAEcIccE+c+ccAAAAAcAIUUcokIQEIIAAAEiYiiMgiCiiIIAAAiAIU0oSoAQEqIAAAImICCUggEiiAAEA"
    "QCAIAecMQAQEc+AcAIqIEMk88EceAAIcIEAIA0KQqAQEqIAAAQyIICkCiIiCAAQAEIAIAe8kkAQEIIAAAQiIQC+CiIiCAA"
    "IcIIAAAUIKkAQEAAEAIgiIgiECiQiiIIEAQAAIAEIEaAIIAAIAAgcc+cE8cQccAQAAAIAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAcc8e8++cicOigiic8c8e+iii"
    "ii+MQYIAmiigiggiiIEig2yiiiigIiiiiiCIQIUAqiigigggiIEkgqqiiiigIiiiUUEIIIiAq+8gi88u+IE4gimi8i8cIi"
    "iqIIIIIIAAsiigiggiiIEkgiiigqiCIiiqUIQIIIAAiiigiggiiIEigiiigmiCIii2iIgIEIAAiiigiggiiIkigiiigiiC"
    "IiUiiIgIEIAAci8e8+gcicYi+iicgci8IcIiiI+MEYA+AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAgAgACAMAgIIgYAAAAAAAQAAAAAAEIIAAQAgACAQAgAAgIAAA"
    "AAAAQAAAAAAIIEAAIc8eecQe8YYkI08c8e8e8iiiii+IIEAAACigii8iiIIkIqiiiiigQiiiUiEIIEYAAeigi+QiiII4Iq"
    "iiiigcQiiiIiIQICqIAiigigQiiIIkIqiiiigCQiiqUiQIIEMAAiigigQiiIIiIiiiiigCQiUqiigIIEAAAe8eeeQeicIi"
    "ciic8eg8MeIUie+IIEAAAAAAAAACAAIAAAAAgCAAAAAAACAEIIAAAAAAAAAcAAQAAAAAgCAAAAAAAcAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAA";
  char* data = wbArrayFromBase64(base64Data);
  int* pixels = g1BPPArrayToARGB(data);
  int charWidth = 6;
  int charHeight = 11;
  int width = charWidth * 0x20;
  int height = charHeight * 3;
  /* temporarily pad it to power-of-two size until I have an actual rectangle packer */
  int potWidth = wbRoundUpToPowerOfTwo(width);
  int potHeight = wbRoundUpToPowerOfTwo(height);
  int* potPixels = wbPadPixels(pixels, width, height, potWidth, potHeight);
  GFont font = gCreateFontFromSimpleGrid(potPixels, potWidth, potHeight, charWidth, charHeight);
  wbDestroyArray(data);
  wbDestroyArray(pixels);
  wbDestroyArray(potPixels);
  return font;
}

void gFont(GMesh mesh, GFont font, int x, int y, char* string) {
  char c;
  int left = x;
  int width = font->charWidth;
  int height = font->charHeight;
  for (; (c = *string); ++string) {
    if (c >= 0x20) {
      int u = ((c - 0x20) % 0x20) * width;
      int v = ((c - 0x20) / 0x20) * height;
      gTexturedQuad(mesh, x, y, u, v, width, height, width, height);
      x += width;
    } else if (c == '\n') {
      y += height;
      x = left;
    }
  }
}

void gDrawFont(GFont font, int x, int y, char* string) {
  GTransformBuilder tbuilder = gCreateTransformBuilder();
  GMesh mesh = gCreateMesh();
  gFont(mesh, font, x, y, string);
  gSetPosition(tbuilder, x, y);
  gDrawMesh(mesh, gBuildTempTransform(tbuilder), gFontTexture(font));
  gDestroyMesh(mesh);
}

/* ---------------------------------------------------------------------------------------------- */

float maLerp(float a, float b, float amount) {
  return a * (1 - amount) + b * amount;
}

void maLerpFloats(int n, float* a, float* b, float* c, float amount) {
  int i;
  for (i = 0; i < n; ++i) {
    c[i] = maLerp(a[i], b[i], amount);
  }
}

void maMulFloatsScalar(int n, float* floats, float* result, float scalar) {
  int i;
  for (i = 0; i < n; ++i) {
    result[i] = floats[i] * scalar;
  }
}

void maFloorFloats(int n, float* floats, float* result) {
  int i;
  for (i = 0; i < n; ++i) {
    result[i] = maFloor(floats[i]);
  }
}

void maClampFloats(int n, float* floats, float* result, float min, float max) {
  int i;
  for (i = 0; i < n; ++i) {
    result[i] = wbClamp(floats[i], min, max);
  }
}

void maAddFloats(int n, float* a, float* b, float* result) {
  int i;
  for (i = 0; i < n; ++i) {
    result[i] = a[i] + b[i];
  }
}

void maSetRect(float* rect, float left, float right, float top, float bottom) {
  rect[0] = left;
  rect[1] = right;
  rect[2] = top;
  rect[3] = bottom;
}

void maSetRectPos(float* rect, float x, float y) {
  rect[1] = x + maRectWidth(rect);
  rect[3] = y + maRectHeight(rect);
  rect[0] = x;
  rect[2] = y;
}

void maSetRectSize(float* rect, float width, float height) {
  rect[1] = maRectX(rect) + width;
  rect[3] = maRectY(rect) + height;
}

void maNormalizeRect(float* rect) {
  if (rect[0] > rect[1]) {
    wbSwapFloats(&rect[0], &rect[1]);
  }
  if (rect[2] > rect[3]) {
    wbSwapFloats(&rect[2], &rect[3]);
  }
}

void maClampRect(float* rect, float* other) {
  rect[0] = wbMin(wbMax(rect[0], other[0]), other[1]);
  rect[1] = wbMin(wbMax(rect[1], other[0]), other[1]);
  rect[2] = wbMin(wbMax(rect[2], other[2]), other[3]);
  rect[3] = wbMin(wbMax(rect[3], other[2]), other[3]);
}

int maPtInRect(float* rect, float x, float y) {
  return x >= rect[0] && x < rect[1] && y >= rect[2] && y < rect[3];
}

void maSetRectLeft(float* rect, float left)     { rect[0] = left; }
void maSetRectRight(float* rect, float right)   { rect[1] = right; }
void maSetRectTop(float* rect, float top)       { rect[2] = top; }
void maSetRectBottom(float* rect, float bottom) { rect[3] = bottom; }
float maRectWidth(float* rect)  { return rect[1] - rect[0]; }
float maRectHeight(float* rect) { return rect[3] - rect[2]; }
float maRectX(float* rect) { return rect[0]; }
float maRectY(float* rect) { return rect[2]; }
float maRectLeft(float* rect)   { return rect[0]; }
float maRectRight(float* rect)  { return rect[1]; }
float maRectTop(float* rect)    { return rect[2]; }
float maRectBottom(float* rect) { return rect[3]; }

/* ---------------------------------------------------------------------------------------------- */

struct _WBSpr {
  int width, height;
  int* palette;
  char* data;
};

#define WBSPR_DATA 0xd
#define WBSPR_REPEAT 0xe

/* TODO: maybe use hashmap to make palette lookup faster */
static int PaletteIndex(int** palette, int color) {
  int i;
  for (i = 0; i < wbArrayLen(*palette); ++i) {
    if ((*palette)[i] == color) {
      return i;
    }
  }
  wbArrayAppend(palette, color);
  return wbArrayLen(*palette) - 1;
}

void wbAppendSprHeader(char** pArray, int formatVersion, int width,
  int height, int* palette)
{
  int i;
  char* p = wbArrayAlloc(pArray, 4);
  wbStrCopy(p, "WBSP");
  wbAppendVarInt32(pArray, formatVersion);
  wbAppendVarInt32(pArray, width);
  wbAppendVarInt32(pArray, height);
  wbAppendVarInt32(pArray, wbArrayLen(palette));
  for (i = 0; i < wbArrayLen(palette); ++i) {
    wbAppendInt32(pArray, palette[i]);
  }
}

char* wbARGBToSprArray(int* argb, int width, int height) {
  char* spr = 0;
  int* palette = 0;
  int i;
  for (i = 0; i < width * height; ++i) {
    PaletteIndex(&palette, argb[i]);
  }
  wbAppendSprHeader(&spr, 1, width, height, palette);
  for (i = 0; i < width * height;) {
    int start;
    for (start = i; i < width * height && argb[i] == argb[start]; ++i);
    if (i - start > 1) {
      wbAppendVarInt32(&spr, WBSPR_REPEAT);
      wbAppendVarInt32(&spr, i - start);
      wbAppendVarInt32(&spr, PaletteIndex(&palette, argb[start]));
    } else {
      i = start + 1;
      for (; i < width * height && argb[i] != argb[i - 1]; ++i);
      wbAppendVarInt32(&spr, WBSPR_DATA);
      wbAppendVarInt32(&spr, i - start);
      for (; start < i; ++start) {
        wbAppendVarInt32(&spr, PaletteIndex(&palette, argb[start]));
      }
    }
  }
  wbDestroyArray(palette);
  return spr;
}

int wbSprWidth(WBSpr spr) {
  return spr->width;
}

int wbSprHeight(WBSpr spr) {
  return spr->height;
}

WBSpr wbCreateSpr(char* data, int length) {
  char* p = data;
  int paletteSize, i, dataLen;
  WBSpr spr = osAlloc(sizeof(struct _WBSpr));
  if (wbMemCmp(p, "WBSP", 4)) {
    /* TODO: error codes or something */
    return 0;
  }
  p += 4;
  wbDecodeVarInt32(&p); /* format version */
  spr->width = wbDecodeVarInt32(&p);
  spr->height = wbDecodeVarInt32(&p);
  paletteSize = wbDecodeVarInt32(&p);
  for (i = 0; i < paletteSize; ++i) {
    wbArrayAppend(&spr->palette, wbDecodeInt32(&p));
  }
  dataLen = length - (p - data);
  wbArrayAlloc(&spr->data, dataLen);
  osMemCpy(spr->data, p, dataLen);
  return spr;
}

WBSpr wbCreateSprFromArray(char* data) {
  return wbCreateSpr(data, wbArrayLen(data));
}

WBSpr wbCreateSprFromFile(char* filePath) {
  int len = 1024000; /* TODO: handle bigger files */
  char* data = osAlloc(len);
  WBSpr res;
  len = osReadEntireFile(filePath, data, len);
  res = len >= 0 ? wbCreateSpr(data, len) : 0;
  osFree(data);
  return res;
}

void wbSprToARGB(WBSpr spr, int* argb) {
  char* p = spr->data;
  int* pixel = argb;
  while (p - spr->data < wbArrayLen(spr->data)) {
    int i;
    int type = wbDecodeVarInt32(&p);
    int length = wbDecodeVarInt32(&p);
    switch (type) {
      case WBSPR_DATA: {
        for (i = 0; i < length; ++i) {
          *pixel++ = spr->palette[wbDecodeVarInt32(&p)];
        }
        break;
      }
      case WBSPR_REPEAT: {
        int color = spr->palette[wbDecodeVarInt32(&p)];
        for (i = 0; i < length; ++i) {
          *pixel++ = color;
        }
      }
    }
  }
}

int* wbSprToARGBArray(WBSpr spr) {
  int* res = 0;
  wbArrayAlloc(&res, spr->width * spr->height);
  wbSprToARGB(spr, res);
  return res;
}

void wbDestroySpr(WBSpr spr) {
  if (spr) {
    wbDestroyArray(spr->palette);
    wbDestroyArray(spr->data);
  }
  osFree(spr);
}

#endif /* WEEBCORE_IMPLEMENTATION */
