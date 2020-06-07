/* NOTE: this intentionally uses OpenGL 1.x for maximum compatibility. do not try to use modern
 * opengl features in here. make a new platform layer for optimization with modern APIs instead. */

#include <GL/gl.h>

typedef struct _GVertex {
  float x, y;
  int color;
  float u, v;
} GVertex;

struct _GMesh {
  int color;
  GVertex* vertices;
  int* indices;
  int istart;
};

struct _GTexture {
  GLuint handle;
  int width, height;
};

typedef struct _GTransformOperation {
  int type;
  union {
    float degrees;
    struct { float x, y; } xy;
  } u;
} GTransformOperation;

enum {
  G_SCALE,
  G_TRANSLATE,
  G_ROTATE,
  G_LAST_OPERATION
};

struct _GTransform {
  GLfloat matrix[16];
  GTransformOperation* operations;
};

/* convert a 0xAARRGGBB integer to OpenGL's 0xAABBGGRR.
 * also flip alpha because we use 0 = opaque */

#define G_COLOR(argb) \
  ((((argb) & 0x00FF0000) >> 16) | \
    ((argb) & 0x0000FF00) | \
   (((argb) & 0x000000FF) << 16) | \
   (0xFF000000 - ((argb) & 0xFF000000)))

static void gInit() {
  glEnable(GL_TEXTURE_2D);
  glEnable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnableClientState(GL_VERTEX_ARRAY);
  glEnableClientState(GL_COLOR_ARRAY);
  glEnableClientState(GL_TEXTURE_COORD_ARRAY);
}

/* ---------------------------------------------------------------------------------------------- */

GMesh gCreateMesh() {
  GMesh mesh = osAlloc(sizeof(struct _GMesh));
  gColor(mesh, 0xffffff);
  return mesh;
}

void gDestroyMesh(GMesh mesh) {
  if (mesh) {
    wbDestroyArray(mesh->vertices);
    wbDestroyArray(mesh->indices);
  }
  osFree(mesh);
}

void gColor(GMesh mesh, int color) {
  mesh->color = G_COLOR(color);
}

void gBegin(GMesh mesh) {
  if (mesh->istart == -1) {
    mesh->istart = wbArrayLen(mesh->vertices);
  }
}

void gEnd(GMesh mesh) {
  mesh->istart = -1;
}

void gVertex(GMesh mesh, float x, float y) {
  GVertex* vertex = wbArrayAlloc(&mesh->vertices, 1);
  vertex->x = x;
  vertex->y = y;
  vertex->color = mesh->color;
}

void gTexCoord(GMesh mesh, float u, float v) {
  int len = wbArrayLen(mesh->vertices);
  if (len) {
    GVertex* vertex = &mesh->vertices[len - 1];
    vertex->u = u;
    vertex->v = v;
  }
}

void gFace(GMesh mesh, int i1, int i2, int i3) {
  int* indices = wbArrayAlloc(&mesh->indices, 3);
  indices[0] = mesh->istart + i1;
  indices[1] = mesh->istart + i2;
  indices[2] = mesh->istart + i3;
}

/* ---------------------------------------------------------------------------------------------- */

GTransform gCreateTransform() {
  GTransform transform = osAlloc(sizeof(struct _GTransform));
  gLoadIdentity(transform);
  return transform;
}

void gDestroyTransform(GTransform transform) {
  if (transform) {
    wbDestroyArray(transform->operations);
  }
  osFree(transform);
}

/* using OpenGL legacy matrix operations as my matrix library.
 * hey, it's free real estate */

static void gBeginTransform(GTransform transform) {
  glPushAttrib(GL_TRANSFORM_BIT);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  if (transform) {
    glLoadMatrixf(transform->matrix);
  } else {
    glLoadIdentity();
  }
}

static void gEndTransform(GTransform transform) {
  if (transform) {
    glGetFloatv(GL_MODELVIEW_MATRIX, transform->matrix);
  }
  glPopMatrix();
  glPopAttrib();
}

/* i batch ops to avoid copying around the matrix data for every call */
static void gComputeTransform(GTransform transform) {
  int i;
  gBeginTransform(transform);
  if (transform) {
    for (i = 0; i < wbArrayLen(transform->operations); ++i) {
      GTransformOperation* op = &transform->operations[i];
      switch (op->type) {
        case G_SCALE:     glScalef(op->u.xy.x, op->u.xy.y, 1);     break;
        case G_TRANSLATE: glTranslatef(op->u.xy.x, op->u.xy.y, 0); break;
        case G_ROTATE:    glRotatef(op->u.degrees, 0, 0, 1);       break;
      }
    }
    wbSetArrayLen(transform->operations, 0);
  }
}

GTransform gCloneTransform(GTransform source) {
  GTransform transform = gCreateTransform();
  gComputeTransform(source);
  gEndTransform(source);
  gLoadMatrix(transform, source->matrix);
  return transform;
}

void gLoadIdentity(GTransform transform) {
  wbSetArrayLen(transform->operations, 0);
  osMemSet(transform->matrix, 0, sizeof(transform->matrix));
  transform->matrix[0] = transform->matrix[5] = transform->matrix[10] = transform->matrix[15] = 1;
  gEndTransform(transform);
}

void gLoadMatrix(GTransform transform, float* matrixIn) {
  osMemCpy(transform->matrix, matrixIn, 16 * sizeof(GLfloat));
}

void gGetMatrix(GTransform transform, float* matrixOut) {
  gComputeTransform(transform);
  gEndTransform(transform);
  osMemCpy(matrixOut, transform->matrix, 16 * sizeof(GLfloat));
}

static GTransformOperation* gOperation(GTransform transform, int t) {
  GTransformOperation* op = wbArrayAlloc(&transform->operations, 1);
  op->type = t;
  return op;
}

static GTransformOperation* gXYOperation(GTransform transform,
  int t, float x, float y
) {
  GTransformOperation* op = gOperation(transform, t);
  op->u.xy.x = x;
  op->u.xy.y = y;
  return op;
}

void gScale(GTransform transform, float x, float y) {
  gXYOperation(transform, G_SCALE, x, y);
}

void gScale1(GTransform transform, float scale) {
  gScale(transform, scale, scale);
}

void gTranslate(GTransform transform, float x, float y) {
  gXYOperation(transform, G_TRANSLATE, x, y);
}

void gRotate(GTransform transform, float degrees) {
  GTransformOperation* op = gOperation(transform, G_ROTATE);
  op->u.degrees = degrees;
}

void gMultiplyMatrix(GTransform transform, float* matrixIn) {
  gComputeTransform(transform);
  glMultMatrixf(matrixIn);
  gEndTransform(transform);
}

void gMultiplyTransform(GTransform transform, GTransform other) {
  gMultiplyMatrix(transform, other->matrix);
}

void gDrawMesh(GMesh mesh, GTransform transform, GTexture texture) {
  int stride = sizeof(GVertex);
  GLuint tex, curTex;

  if (!wbArrayLen(mesh->indices)) {
    return;
  }

  tex = texture ? texture->handle : 0;
  glVertexPointer(2, GL_FLOAT, stride, &mesh->vertices[0].x);
  glColorPointer(4, GL_UNSIGNED_BYTE, stride, &mesh->vertices[0].color);
  if (texture) {
    glTexCoordPointer(2, GL_FLOAT, stride, &mesh->vertices[0].u);
    /* scale texture coords so we can use pixels instead of texels */
    glPushAttrib(GL_TRANSFORM_BIT);
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glScalef(1.0f / texture->width, 1.0f / texture->height, 1.0f);
    glPopAttrib();
  }

  glGetIntegerv(GL_TEXTURE_BINDING_2D, (void*)&curTex);
  if (tex != curTex) {
    glBindTexture(GL_TEXTURE_2D, tex);
  }

  gComputeTransform(transform);
  glDrawElements(GL_TRIANGLES, wbArrayLen(mesh->indices),
    GL_UNSIGNED_INT, mesh->indices);
  gEndTransform(transform);
}

/* ---------------------------------------------------------------------------------------------- */

GTexture gCreateTexture() {
  GTexture texture = osAlloc(sizeof(struct _GTexture));
  glGenTextures(1, &texture->handle);
  gSetTextureWrapU(texture, G_REPEAT);
  gSetTextureWrapV(texture, G_REPEAT);
  gSetTextureMinFilter(texture, G_NEAREST);
  gSetTextureMagFilter(texture, G_NEAREST);
  return texture;
}

void gDestroyTexture(GTexture texture) {
  if (texture) {
    glDeleteTextures(1, &texture->handle);
  }
  osFree(texture);
}

static int gTranslateTextureWrap(int mode) {
  switch (mode) {
    case G_CLAMP_TO_EDGE: return GL_CLAMP_TO_EDGE;
    case G_MIRRORED_REPEAT: return GL_MIRRORED_REPEAT;
    case G_REPEAT: return GL_REPEAT;
  }
  return G_REPEAT;
}

static int gTranslateTextureFilter(int filter) {
  switch (filter) {
    case G_NEAREST: return GL_NEAREST;
    case G_LINEAR: return GL_LINEAR;
  }
  return G_NEAREST;
}

static void gSetTextureParam(GTexture texture, int param, int value) {
  glBindTexture(GL_TEXTURE_2D, texture->handle);
  glTexParameteri(GL_TEXTURE_2D, param, value);
}

void gSetTextureWrapU(GTexture texture, int mode) {
  gSetTextureParam(texture, GL_TEXTURE_WRAP_S, gTranslateTextureWrap(mode));
}

void gSetTextureWrapV(GTexture texture, int mode) {
  gSetTextureParam(texture, GL_TEXTURE_WRAP_T, gTranslateTextureWrap(mode));
}

void gSetTextureMinFilter(GTexture texture, int filter) {
  gSetTextureParam(texture, GL_TEXTURE_MIN_FILTER, gTranslateTextureFilter(filter));
}

void gSetTextureMagFilter(GTexture texture, int filter) {
  gSetTextureParam(texture, GL_TEXTURE_MAG_FILTER, gTranslateTextureFilter(filter));
}

void gPixels(GTexture texture, int width, int height, int* data) {
  gPixelsEx(texture, width, height, data, width);
}

void gPixelsEx(GTexture texture, int width, int height, int* data, int stride) {
  int* rgba = 0;
  int x, y;
  for (y = 0; y < height; ++y) {
    for (x = 0; x < width; ++x) {
      wbArrayAppend(&rgba, G_COLOR(data[y * stride + x]));
    }
  }
  glBindTexture(GL_TEXTURE_2D, texture->handle);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
    GL_RGBA, GL_UNSIGNED_BYTE, rgba);
  wbDestroyArray(rgba);
  texture->width = width;
  texture->height = height;
}

/* ---------------------------------------------------------------------------------------------- */

void gViewport(OSWindow window, int x, int y, int width, int height) {
  /* OpenGL uses a Y axis that starts from the bottom of the window.
   * I flip it because I find it most intuitive that way */
  glViewport(x, osWindowHeight(window) - y - height, width, height);
  glPushAttrib(GL_TRANSFORM_BIT);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, width, height, 0, 0, 1);
  glPopAttrib();
}

void gClearColor(int color) {
  float rgba[4];
  gColorToFloats(color, rgba);
  glClearColor(rgba[0], rgba[1], rgba[2], rgba[3]);
}

void gSwapBuffers(OSWindow window) {
  osSwapBuffers(window);

  /* this is to avoid having to have an extra function to call at the
   * beginning of the frame */
  glClear(GL_COLOR_BUFFER_BIT);
}
