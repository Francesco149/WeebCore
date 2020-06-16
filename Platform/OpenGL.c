/* NOTE: this intentionally uses OpenGL 1.x for maximum compatibility. do not try to use modern
 * opengl features in here. make a new platform layer for optimization with modern APIs instead. */

#include <GL/gl.h>

typedef struct _VertData {
  float x, y;
  int color;
  float u, v;
} VertData;

struct _Mesh {
  int color;
  VertData* verts;
  int* indices;
  int istart;
};

struct _Tex { GLuint handle; int width, height; };

typedef struct _MatOp {
  int type;
  union {
    float deg;
    struct { float x, y; } xy;
  } u;
} MatOp;

enum {
  SCALE,
  TRANSLATE,
  ROTATE,
  LAST_OPERATION
};

struct _Mat {
  GLfloat mat[16];
  MatOp* operations;
};

/* convert a 0xAARRGGBB integer to OpenGL's 0xAABBGGRR.
 * also flip alpha because we use 0 = opaque */

#define COL(argb) \
  ((((argb) & 0x00FF0000) >> 16) | \
    ((argb) & 0x0000FF00) | \
   (((argb) & 0x000000FF) << 16) | \
   (0xFF000000 - ((argb) & 0xFF000000)))

static void GrInit() {
  glEnable(GL_TEXTURE_2D);
  glEnable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnableClientState(GL_VERTEX_ARRAY);
  glEnableClientState(GL_COLOR_ARRAY);
  glEnableClientState(GL_TEXTURE_COORD_ARRAY);
}

/* ---------------------------------------------------------------------------------------------- */

Mesh MkMesh() {
  Mesh mesh = Alloc(sizeof(struct _Mesh));
  Col(mesh, 0xffffff);
  return mesh;
}

void RmMesh(Mesh mesh) {
  if (mesh) {
    RmArr(mesh->verts);
    RmArr(mesh->indices);
  }
  Free(mesh);
}

void Col(Mesh mesh, int color) {
  mesh->color = COL(color);
}

void Begin(Mesh mesh) {
  if (mesh->istart == -1) {
    mesh->istart = ArrLen(mesh->verts);
  }
}

void End(Mesh mesh) {
  mesh->istart = -1;
}

void Vert(Mesh mesh, float x, float y) {
  VertData* vertex = ArrAlloc(&mesh->verts, 1);
  vertex->x = x;
  vertex->y = y;
  vertex->color = mesh->color;
}

void TexCoord(Mesh mesh, float u, float v) {
  int len = ArrLen(mesh->verts);
  if (len) {
    VertData* vertex = &mesh->verts[len - 1];
    vertex->u = u;
    vertex->v = v;
  }
}

void Face(Mesh mesh, int i1, int i2, int i3) {
  int* indices = ArrAlloc(&mesh->indices, 3);
  indices[0] = mesh->istart + i1;
  indices[1] = mesh->istart + i2;
  indices[2] = mesh->istart + i3;
}

/* ---------------------------------------------------------------------------------------------- */

Mat MkMat() {
  Mat mat = Alloc(sizeof(struct _Mat));
  SetIdentity(mat);
  return mat;
}

void RmMat(Mat mat) {
  if (mat) {
    RmArr(mat->operations);
  }
  Free(mat);
}

/* using OpenGL legacy mat operations as my mat library.
 * hey, it's free real estate */

static void BeginMat(Mat mat) {
  glPushAttrib(GL_TRANSFORM_BIT);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  if (mat) {
    glLoadMatrixf(mat->mat);
  } else {
    glLoadIdentity();
  }
}

static Mat EndMat(Mat mat) {
  if (mat) {
    glGetFloatv(GL_MODELVIEW_MATRIX, mat->mat);
  }
  glPopMatrix();
  glPopAttrib();
  return mat;
}

/* i batch ops to avoid copying around the mat data for every call */
static void CalcMat(Mat mat) {
  int i;
  BeginMat(mat);
  if (mat) {
    for (i = 0; i < ArrLen(mat->operations); ++i) {
      MatOp* op = &mat->operations[i];
      switch (op->type) {
        case SCALE:     glScalef(op->u.xy.x, op->u.xy.y, 1);     break;
        case TRANSLATE: glTranslatef(op->u.xy.x, op->u.xy.y, 0); break;
        case ROTATE:    glRotatef(op->u.deg, 0, 0, 1);           break;
      }
    }
    SetArrLen(mat->operations, 0);
  }
}

Mat DupMat(Mat source) {
  Mat mat = MkMat();
  CalcMat(source);
  EndMat(source);
  return SetMat(mat, source->mat);
}

Mat SetIdentity(Mat mat) {
  SetArrLen(mat->operations, 0);
  MemSet(mat->mat, 0, sizeof(mat->mat));
  mat->mat[0] = mat->mat[5] = mat->mat[10] = mat->mat[15] = 1;
  return EndMat(mat);
}

Mat SetMat(Mat mat, float* matIn) {
  MemCpy(mat->mat, matIn, 16 * sizeof(GLfloat));
  return mat;
}

Mat GetMat(Mat mat, float* matOut) {
  CalcMat(mat);
  EndMat(mat);
  MemCpy(matOut, mat->mat, 16 * sizeof(GLfloat));
  return mat;
}

static MatOp* Op(Mat mat, int t) {
  MatOp* op = ArrAlloc(&mat->operations, 1);
  op->type = t;
  return op;
}

static Mat XYOp(Mat mat, int t, float x, float y) {
  MatOp* op = Op(mat, t);
  op->u.xy.x = x;
  op->u.xy.y = y;
  return mat;
}

Mat Scale(Mat mat, float x, float y) {
  return XYOp(mat, SCALE, x, y);
}

Mat Scale1(Mat mat, float scale) {
  return Scale(mat, scale, scale);
}

Mat Move(Mat mat, float x, float y) {
  return XYOp(mat, TRANSLATE, x, y);
}

Mat Rot(Mat mat, float deg) {
  MatOp* op = Op(mat, ROTATE);
  op->u.deg = deg;
  return mat;
}

Mat MulMatFlt(Mat mat, float* matIn) {
  CalcMat(mat);
  glMultMatrixf(matIn);
  return EndMat(mat);
}

Mat MulMat(Mat mat, Mat other) {
  return MulMatFlt(mat, other->mat);
}

void PutMesh(Mesh mesh, Mat mat, Tex tex) {
  int stride = sizeof(VertData);
  GLuint gltex, curTex;

  if (!ArrLen(mesh->indices)) {
    return;
  }

  gltex = tex ? tex->handle : 0;
  glVertexPointer(2, GL_FLOAT, stride, &mesh->verts[0].x);
  glColorPointer(4, GL_UNSIGNED_BYTE, stride, &mesh->verts[0].color);
  if (gltex) {
    glTexCoordPointer(2, GL_FLOAT, stride, &mesh->verts[0].u);
    /* scale tex coords so we can use pixs instead of texels */
    glPushAttrib(GL_TRANSFORM_BIT);
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glScalef(1.0f / tex->width, 1.0f / tex->height, 1.0f);
    glPopAttrib();
  }

  glGetIntegerv(GL_TEXTURE_BINDING_2D, (void*)&curTex);
  if (gltex != curTex) {
    glBindTexture(GL_TEXTURE_2D, gltex);
  }

  CalcMat(mat);
  glDrawElements(GL_TRIANGLES, ArrLen(mesh->indices), GL_UNSIGNED_INT, mesh->indices);
  EndMat(mat);
}

/* ---------------------------------------------------------------------------------------------- */

Tex MkTex() {
  Tex tex = Alloc(sizeof(struct _Tex));
  glGenTextures(1, &tex->handle);
  SetTexWrapU(tex, REPEAT);
  SetTexWrapV(tex, REPEAT);
  SetTexMinFilter(tex, NEAREST);
  SetTexMagFilter(tex, NEAREST);
  return tex;
}

void RmTex(Tex tex) {
  if (tex) {
    glDeleteTextures(1, &tex->handle);
  }
  Free(tex);
}

static int ConvTexWrap(int mode) {
  switch (mode) {
    case CLAMP_TO_EDGE: return GL_CLAMP_TO_EDGE;
    case MIRRORED_REPEAT: return GL_MIRRORED_REPEAT;
    case REPEAT: return GL_REPEAT;
  }
  return REPEAT;
}

static int ConvTexFilter(int filter) {
  switch (filter) {
    case NEAREST: return GL_NEAREST;
    case LINEAR: return GL_LINEAR;
  }
  return NEAREST;
}

static void SetTexParam(Tex tex, int param, int value) {
  glBindTexture(GL_TEXTURE_2D, tex->handle);
  glTexParameteri(GL_TEXTURE_2D, param, value);
}

void SetTexWrapU(Tex tex, int mode) {
  SetTexParam(tex, GL_TEXTURE_WRAP_S, ConvTexWrap(mode));
}

void SetTexWrapV(Tex tex, int mode) {
  SetTexParam(tex, GL_TEXTURE_WRAP_T, ConvTexWrap(mode));
}

void SetTexMinFilter(Tex tex, int filter) {
  SetTexParam(tex, GL_TEXTURE_MIN_FILTER, ConvTexFilter(filter));
}

void SetTexMagFilter(Tex tex, int filter) {
  SetTexParam(tex, GL_TEXTURE_MAG_FILTER, ConvTexFilter(filter));
}

void Pixs(Tex tex, int width, int height, int* data) {
  PixsEx(tex, width, height, data, width);
}

void PixsEx(Tex tex, int width, int height, int* data, int stride) {
  int* rgba = 0;
  int x, y;
  for (y = 0; y < height; ++y) {
    for (x = 0; x < width; ++x) {
      ArrCat(&rgba, COL(data[y * stride + x]));
    }
  }
  glBindTexture(GL_TEXTURE_2D, tex->handle);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
  RmArr(rgba);
  tex->width = width;
  tex->height = height;
}

/* ---------------------------------------------------------------------------------------------- */

void Viewport(Wnd window, int x, int y, int width, int height) {
  /* OpenGL uses a Y axis that starts from the bottom of the window.
   * I flip it because I find it most intuitive that way */
  glViewport(x, WndHeight(window) - y - height, width, height);
  glPushAttrib(GL_TRANSFORM_BIT);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, width, height, 0, 0, 1);
  glPopAttrib();
}

void ClsCol(int color) {
  float rgba[4];
  ColToFlts(color, rgba);
  glClearColor(rgba[0], rgba[1], rgba[2], rgba[3]);
}

void SwpBufs(Wnd window) {
  OsSwpBufs(window);
  /* this is to avoid having to have an extra function to call at the beginning of the frame */
  glClear(GL_COLOR_BUFFER_BIT);
}
