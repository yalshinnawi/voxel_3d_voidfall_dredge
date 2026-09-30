/*
 * GLAD - OpenGL Loader Header (OpenGL 4.3 Core Profile)
 * Minimal header for the VoxelEngine project.
 */
#ifndef GLAD_H
#define GLAD_H

#ifdef __cplusplus
extern "C" {
#endif

/* ── Platform includes ── */
#ifdef _WIN32
    #ifndef APIENTRY
        #define APIENTRY __stdcall
    #endif
    #ifndef WINGDIAPI
        #define WINGDIAPI __declspec(dllimport)
    #endif
#else
    #define APIENTRY
    #define WINGDIAPI
#endif

#include <stddef.h>

/* ── GL types ── */
typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef signed char GLbyte;
typedef short GLshort;
typedef int GLint;
typedef int GLsizei;
typedef unsigned char GLubyte;
typedef unsigned short GLushort;
typedef unsigned int GLuint;
typedef float GLfloat;
typedef float GLclampf;
typedef double GLdouble;
typedef double GLclampd;
typedef void GLvoid;
typedef char GLchar;
typedef ptrdiff_t GLsizeiptr;
typedef ptrdiff_t GLintptr;
typedef unsigned short GLhalf;

/* ── GL constants ── */
#define GL_FALSE                  0
#define GL_TRUE                   1
#define GL_NONE                   0
#define GL_NO_ERROR               0

/* Data types */
#define GL_BYTE                   0x1400
#define GL_UNSIGNED_BYTE          0x1401
#define GL_SHORT                  0x1402
#define GL_UNSIGNED_SHORT         0x1403
#define GL_INT                    0x1404
#define GL_UNSIGNED_INT           0x1405
#define GL_FLOAT                  0x1406

/* Primitives */
#define GL_POINTS                 0x0000
#define GL_LINES                  0x0001
#define GL_LINE_LOOP              0x0002
#define GL_LINE_STRIP             0x0003
#define GL_TRIANGLES              0x0004
#define GL_TRIANGLE_STRIP         0x0005
#define GL_TRIANGLE_FAN           0x0006

/* Buffers */
#define GL_ARRAY_BUFFER           0x8892
#define GL_ELEMENT_ARRAY_BUFFER   0x8893
#define GL_UNIFORM_BUFFER         0x8A11
#define GL_STATIC_DRAW            0x88E4
#define GL_DYNAMIC_DRAW           0x88E8
#define GL_STREAM_DRAW            0x88E0

/* Textures */
#define GL_TEXTURE_2D             0x0DE1
#define GL_TEXTURE_3D             0x806F
#define GL_TEXTURE0               0x84C0
#define GL_TEXTURE1               0x84C1
#define GL_TEXTURE2               0x84C2
#define GL_TEXTURE3               0x84C3
#define GL_TEXTURE_WRAP_S         0x2802
#define GL_TEXTURE_WRAP_T         0x2803
#define GL_TEXTURE_MIN_FILTER     0x2801
#define GL_TEXTURE_MAG_FILTER     0x2800
#define GL_NEAREST                0x2600
#define GL_LINEAR                 0x2601
#define GL_NEAREST_MIPMAP_NEAREST 0x2700
#define GL_LINEAR_MIPMAP_LINEAR   0x2703
#define GL_NEAREST_MIPMAP_LINEAR  0x2702
#define GL_LINEAR_MIPMAP_NEAREST  0x2701
#define GL_CLAMP_TO_EDGE          0x812F
#define GL_REPEAT                 0x2901
#define GL_TEXTURE_2D_MULTISAMPLE 0x9100
#define GL_TEXTURE_WRAP_R         0x8072
#define GL_TEXTURE_MAX_LEVEL      0x813D

/* Pixel formats */
#define GL_RED                    0x1903
#define GL_RGB                    0x1907
#define GL_RGBA                   0x1908
#define GL_RGBA8                  0x8058
#define GL_R8                     0x8229
#define GL_SRGB8_ALPHA8           0x8C43

/* Depth / Stencil */
#define GL_DEPTH_TEST             0x0B71
#define GL_DEPTH_BUFFER_BIT       0x00000100
#define GL_DEPTH_COMPONENT        0x1902
#define GL_DEPTH_COMPONENT16      0x81A5
#define GL_DEPTH_COMPONENT24      0x81A6
#define GL_DEPTH_COMPONENT32F     0x8CAC
#define GL_DEPTH_STENCIL          0x84F9
#define GL_DEPTH24_STENCIL8       0x88F0
#define GL_STENCIL_BUFFER_BIT     0x00000400

/* Clear / Draw */
#define GL_COLOR_BUFFER_BIT       0x00004000
#define GL_LEQUAL                 0x0203
#define GL_LESS                   0x0201
#define GL_GREATER                0x0204
#define GL_GEQUAL                 0x0206
#define GL_EQUAL                  0x0202
#define GL_NOTEQUAL               0x0205
#define GL_ALWAYS                 0x0207
#define GL_NEVER                  0x0200

/* Face Culling */
#define GL_CULL_FACE              0x0B44
#define GL_BACK                   0x0405
#define GL_FRONT                  0x0404
#define GL_FRONT_AND_BACK         0x0408
#define GL_CW                     0x0900
#define GL_CCW                    0x0901

/* Blending */
#define GL_BLEND                  0x0BE2
#define GL_SRC_ALPHA              0x0302
#define GL_ONE_MINUS_SRC_ALPHA    0x0303
#define GL_ONE                    1
#define GL_ZERO                   0

/* Shaders */
#define GL_FRAGMENT_SHADER        0x8B30
#define GL_VERTEX_SHADER          0x8B31
#define GL_GEOMETRY_SHADER        0x8DD9
#define GL_COMPILE_STATUS         0x8B81
#define GL_LINK_STATUS            0x8B82
#define GL_INFO_LOG_LENGTH        0x8B84
#define GL_ACTIVE_UNIFORMS        0x8B86

/* Framebuffers */
#define GL_FRAMEBUFFER            0x8D40
#define GL_READ_FRAMEBUFFER       0x8CA8
#define GL_DRAW_FRAMEBUFFER       0x8CA9
#define GL_RENDERBUFFER           0x8D41
#define GL_COLOR_ATTACHMENT0      0x8CE0
#define GL_DEPTH_ATTACHMENT       0x8D00
#define GL_STENCIL_ATTACHMENT     0x8D20
#define GL_DEPTH_STENCIL_ATTACHMENT 0x821A
#define GL_FRAMEBUFFER_COMPLETE   0x8CD5

/* Polygon mode */
#define GL_FILL                   0x1B02
#define GL_LINE                   0x1B01
#define GL_POINT                  0x1B00

/* Version query */
#define GL_MAJOR_VERSION          0x821B
#define GL_MINOR_VERSION          0x821C
#define GL_VERSION                0x1F02
#define GL_RENDERER               0x1F01
#define GL_VENDOR                 0x1F00
#define GL_EXTENSIONS             0x1F03
#define GL_NUM_EXTENSIONS         0x821D

/* Misc */
#define GL_MAX_TEXTURE_SIZE       0x0D33
#define GL_SCISSOR_TEST           0x0C11
#define GL_MULTISAMPLE            0x809D
#define GL_READ_ONLY              0x88B8
#define GL_WRITE_ONLY             0x88B9
#define GL_READ_WRITE             0x88BA
#define GL_INVALID_INDEX          0xFFFFFFFFu

/* OpenGL 4.3+ Compute & Texture Array Constants */
#define GL_COMPUTE_SHADER         0x91B9
#define GL_TEXTURE_2D_ARRAY       0x8C1A
#define GL_TEXTURE_3D             0x806F
#define GL_SHADER_IMAGE_ACCESS_BARRIER_BIT 0x00000020
#define GL_ALL_BARRIER_BITS       0xFFFFFFFF
#define GL_RGBA16F                0x881A
#define GL_RGBA32F                0x8814
#define GL_RGB16F                 0x881B
#define GL_R16F                   0x822D
#define GL_R32F                   0x822E
#define GL_RED                    0x1903
#define GL_DEPTH_COMPONENT24      0x81A6
#define GL_COLOR_ATTACHMENT1      0x8CE1
#define GL_COLOR_ATTACHMENT2      0x8CE2
#define GL_COLOR_ATTACHMENT3      0x8CE3

/* Debug */
#define GL_DEBUG_OUTPUT           0x92E0
#define GL_DEBUG_OUTPUT_SYNCHRONOUS 0x8242
#define GL_DONT_CARE              0x1100
#define GL_DEBUG_SEVERITY_HIGH    0x9146
#define GL_DEBUG_SEVERITY_MEDIUM  0x9148
#define GL_DEBUG_SEVERITY_LOW     0x9149
#define GL_DEBUG_SEVERITY_NOTIFICATION 0x826B

/* ── Function pointer typedefs ── */
typedef void  (APIENTRY *GLDEBUGPROC)(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam);

typedef void  (APIENTRY *PFNGLCLEARPROC)(GLbitfield mask);
typedef void  (APIENTRY *PFNGLCLEARCOLORPROC)(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
typedef void  (APIENTRY *PFNGLENABLEPROC)(GLenum cap);
typedef void  (APIENTRY *PFNGLDISABLEPROC)(GLenum cap);
typedef void  (APIENTRY *PFNGLVIEWPORTPROC)(GLint x, GLint y, GLsizei w, GLsizei h);
typedef void  (APIENTRY *PFNGLDRAWELEMENTSPROC)(GLenum mode, GLsizei count, GLenum type, const void* indices);
typedef void  (APIENTRY *PFNGLDRAWARRAYSPROC)(GLenum mode, GLint first, GLsizei count);
typedef void  (APIENTRY *PFNGLPOLYGONMODEPROC)(GLenum face, GLenum mode);
typedef void  (APIENTRY *PFNGLGETINTEGERVPROC)(GLenum pname, GLint* params);
typedef const GLubyte* (APIENTRY *PFNGLGETSTRINGPROC)(GLenum name);
typedef void  (APIENTRY *PFNGLBLENDFUNCPROC)(GLenum sfactor, GLenum dfactor);
typedef void  (APIENTRY *PFNGLDEPTHFUNCPROC)(GLenum func);
typedef void  (APIENTRY *PFNGLCULLFACEPROC)(GLenum mode);
typedef void  (APIENTRY *PFNGLFRONTFACEPROC)(GLenum mode);
typedef void  (APIENTRY *PFNGLLINEWIDTHPROC)(GLfloat width);
typedef void  (APIENTRY *PFNGLPOINTSIZEPROC)(GLfloat size);
typedef void  (APIENTRY *PFNGLSCISSORPROC)(GLint x, GLint y, GLsizei w, GLsizei h);
typedef void  (APIENTRY *PFNGLTEXIMAGE2DPROC)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels);
typedef void  (APIENTRY *PFNGLTEXPARAMETERIPROC)(GLenum target, GLenum pname, GLint param);
typedef void  (APIENTRY *PFNGLGENTEXTURESPROC)(GLsizei n, GLuint* textures);
typedef void  (APIENTRY *PFNGLBINDTEXTUREPROC)(GLenum target, GLuint texture);
typedef void  (APIENTRY *PFNGLDELETETEXTURESPROC)(GLsizei n, const GLuint* textures);
typedef void  (APIENTRY *PFNGLREADBUFFERPROC)(GLenum mode);
typedef void  (APIENTRY *PFNGLREADPIXELSPROC)(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void* pixels);
typedef void  (APIENTRY *PFNGLGETFLOATVPROC)(GLenum pname, GLfloat* params);
typedef GLenum(APIENTRY *PFNGLGETERRORPROC)(void);
typedef void  (APIENTRY *PFNGLDEPTHMASKPROC)(GLboolean flag);
typedef void  (APIENTRY *PFNGLFINISHPROC)(void);
typedef void  (APIENTRY *PFNGLFLUSHPROC)(void);

typedef void  (APIENTRY *PFNGLGENBUFFERSPROC)(GLsizei n, GLuint* buffers);
typedef void  (APIENTRY *PFNGLBINDBUFFERPROC)(GLenum target, GLuint buffer);
typedef void  (APIENTRY *PFNGLBUFFERDATAPROC)(GLenum target, GLsizeiptr size, const void* data, GLenum usage);
typedef void  (APIENTRY *PFNGLBUFFERSUBDATAPROC)(GLenum target, GLintptr offset, GLsizeiptr size, const void* data);
typedef void  (APIENTRY *PFNGLDELETEBUFFERSPROC)(GLsizei n, const GLuint* buffers);

typedef GLuint(APIENTRY *PFNGLCREATESHADERPROC)(GLenum type);
typedef void  (APIENTRY *PFNGLSHADERSOURCEPROC)(GLuint shader, GLsizei count, const GLchar** string, const GLint* length);
typedef void  (APIENTRY *PFNGLCOMPILESHADERPROC)(GLuint shader);
typedef void  (APIENTRY *PFNGLGETSHADERIVPROC)(GLuint shader, GLenum pname, GLint* params);
typedef void  (APIENTRY *PFNGLGETSHADERINFOLOGPROC)(GLuint shader, GLsizei maxLength, GLsizei* length, GLchar* infoLog);
typedef void  (APIENTRY *PFNGLDELETESHADERPROC)(GLuint shader);
typedef GLuint(APIENTRY *PFNGLCREATEPROGRAMPROC)(void);
typedef void  (APIENTRY *PFNGLATTACHSHADERPROC)(GLuint program, GLuint shader);
typedef void  (APIENTRY *PFNGLLINKPROGRAMPROC)(GLuint program);
typedef void  (APIENTRY *PFNGLGETPROGRAMIVPROC)(GLuint program, GLenum pname, GLint* params);
typedef void  (APIENTRY *PFNGLGETPROGRAMINFOLOGPROC)(GLuint program, GLsizei maxLength, GLsizei* length, GLchar* infoLog);
typedef void  (APIENTRY *PFNGLDELETEPROGRAMPROC)(GLuint program);
typedef void  (APIENTRY *PFNGLUSEPROGRAMPROC)(GLuint program);
typedef GLint (APIENTRY *PFNGLGETUNIFORMLOCATIONPROC)(GLuint program, const GLchar* name);
typedef void  (APIENTRY *PFNGLUNIFORM1IPROC)(GLint location, GLint v0);
typedef void  (APIENTRY *PFNGLUNIFORM1FPROC)(GLint location, GLfloat v0);
typedef void  (APIENTRY *PFNGLUNIFORM2FPROC)(GLint location, GLfloat v0, GLfloat v1);
typedef void  (APIENTRY *PFNGLUNIFORM3FPROC)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
typedef void  (APIENTRY *PFNGLUNIFORM4FPROC)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
typedef void  (APIENTRY *PFNGLUNIFORMMATRIX4FVPROC)(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
typedef void  (APIENTRY *PFNGLUNIFORM3FVPROC)(GLint location, GLsizei count, const GLfloat* value);
typedef void  (APIENTRY *PFNGLUNIFORM1FVPROC)(GLint location, GLsizei count, const GLfloat* value);

typedef void  (APIENTRY *PFNGLVERTEXATTRIBPOINTERPROC)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer);
typedef void  (APIENTRY *PFNGLENABLEVERTEXATTRIBARRAYPROC)(GLuint index);
typedef void  (APIENTRY *PFNGLDISABLEVERTEXATTRIBARRAYPROC)(GLuint index);

typedef void  (APIENTRY *PFNGLGENVERTEXARRAYSPROC)(GLsizei n, GLuint* arrays);
typedef void  (APIENTRY *PFNGLBINDVERTEXARRAYPROC)(GLuint array);
typedef void  (APIENTRY *PFNGLDELETEVERTEXARRAYSPROC)(GLsizei n, const GLuint* arrays);
typedef void  (APIENTRY *PFNGLGENFRAMEBUFFERSPROC)(GLsizei n, GLuint* framebuffers);
typedef void  (APIENTRY *PFNGLBINDFRAMEBUFFERPROC)(GLenum target, GLuint framebuffer);
typedef void  (APIENTRY *PFNGLDELETEFRAMEBUFFERSPROC)(GLsizei n, const GLuint* framebuffers);
typedef void  (APIENTRY *PFNGLFRAMEBUFFERTEXTURE2DPROC)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef GLenum(APIENTRY *PFNGLCHECKFRAMEBUFFERSTATUSPROC)(GLenum target);
typedef void  (APIENTRY *PFNGLGENERATEMIPMAPPROC)(GLenum target);
typedef const GLubyte* (APIENTRY *PFNGLGETSTRINGIPROC)(GLenum name, GLuint index);
typedef void  (APIENTRY *PFNGLGENRENDERBUFFERSPROC)(GLsizei n, GLuint* renderbuffers);
typedef void  (APIENTRY *PFNGLBINDRENDERBUFFERPROC)(GLenum target, GLuint renderbuffer);
typedef void  (APIENTRY *PFNGLRENDERBUFFERSTORAGEPROC)(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
typedef void  (APIENTRY *PFNGLFRAMEBUFFERRENDERBUFFERPROC)(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
typedef void  (APIENTRY *PFNGLDELETERENDERBUFFERSPROC)(GLsizei n, const GLuint* renderbuffers);
typedef void  (APIENTRY *PFNGLTEXIMAGE2DMULTISAMPLEPROC)(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations);
typedef void  (APIENTRY *PFNGLBLITFRAMEBUFFERPROC)(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
typedef void  (APIENTRY *PFNGLDRAWBUFFERSPROC)(GLsizei n, const GLenum* bufs);

typedef GLuint(APIENTRY *PFNGLGETUNIFORMBLOCKINDEXPROC)(GLuint program, const GLchar* uniformBlockName);
typedef void  (APIENTRY *PFNGLUNIFORMBLOCKBINDINGPROC)(GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding);
typedef void  (APIENTRY *PFNGLBINDBUFFERBASEPROC)(GLenum target, GLuint index, GLuint buffer);
typedef void  (APIENTRY *PFNGLBINDBUFFERRANGEPROC)(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size);

typedef void  (APIENTRY *PFNGLGENSAMPLERSPROC)(GLsizei count, GLuint* samplers);
typedef void  (APIENTRY *PFNGLBINDSAMPLERPROC)(GLuint unit, GLuint sampler);
typedef void  (APIENTRY *PFNGLSAMPLERPARAMETERIPROC)(GLuint sampler, GLenum pname, GLint param);
typedef void  (APIENTRY *PFNGLDELETESAMPLERSPROC)(GLsizei count, const GLuint* samplers);

typedef void  (APIENTRY *PFNGLDEBUGMESSAGECALLBACKPROC)(GLDEBUGPROC callback, const void* userParam);
typedef void  (APIENTRY *PFNGLDEBUGMESSAGECONTROLPROC)(GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint* ids, GLboolean enabled);

typedef void  (APIENTRY *PFNGLACTIVETEXTUREPROC)(GLenum texture);
typedef void  (APIENTRY *PFNGLTEXSUBIMAGE2DPROC)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels);
typedef void  (APIENTRY *PFNGLTEXIMAGE3DPROC)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void* pixels);
typedef void  (APIENTRY *PFNGLTEXSUBIMAGE3DPROC)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void* pixels);

typedef void  (APIENTRY *PFNGLDRAWARRAYSINSTANCEDPROC)(GLenum mode, GLint first, GLsizei count, GLsizei instancecount);
typedef void  (APIENTRY *PFNGLDRAWELEMENTSINSTANCEDPROC)(GLenum mode, GLsizei count, GLenum type, const void* indices, GLsizei instancecount);
typedef void  (APIENTRY *PFNGLVERTEXATTRIBDIVISORPROC)(GLuint index, GLuint divisor);

typedef void* (APIENTRY *PFNGLMAPBUFFERPROC)(GLenum target, GLenum access);
typedef GLboolean (APIENTRY *PFNGLUNMAPBUFFERPROC)(GLenum target);

typedef void  (APIENTRY *PFNGLVERTEXATTRIBIPOINTERPROC)(GLuint index, GLint size, GLenum type, GLsizei stride, const void* pointer);
typedef void  (APIENTRY *PFNGLDISPATCHCOMPUTEPROC)(GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z);
typedef void  (APIENTRY *PFNGLMEMORYBARRIERPROC)(GLbitfield barriers);
typedef void  (APIENTRY *PFNGLBINDIMAGETEXTUREPROC)(GLuint unit, GLuint texture, GLint level, GLboolean layered, GLint layer, GLenum access, GLenum format);
typedef void  (APIENTRY *PFNGLTEXSTORAGE2DPROC)(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height);
typedef void  (APIENTRY *PFNGLTEXSTORAGE3DPROC)(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth);

/* ── Extern function pointers ── */
#define GLAD_GL_FUNC(ret, name) extern ret name

extern PFNGLCLEARPROC glad_glClear;
#define glClear glad_glClear
extern PFNGLCLEARCOLORPROC glad_glClearColor;
#define glClearColor glad_glClearColor
extern PFNGLENABLEPROC glad_glEnable;
#define glEnable glad_glEnable
extern PFNGLDISABLEPROC glad_glDisable;
#define glDisable glad_glDisable
extern PFNGLVIEWPORTPROC glad_glViewport;
#define glViewport glad_glViewport
extern PFNGLDRAWELEMENTSPROC glad_glDrawElements;
#define glDrawElements glad_glDrawElements
extern PFNGLDRAWARRAYSPROC glad_glDrawArrays;
#define glDrawArrays glad_glDrawArrays
extern PFNGLPOLYGONMODEPROC glad_glPolygonMode;
#define glPolygonMode glad_glPolygonMode
extern PFNGLGETINTEGERVPROC glad_glGetIntegerv;
#define glGetIntegerv glad_glGetIntegerv
extern PFNGLGETSTRINGPROC glad_glGetString;
#define glGetString glad_glGetString
extern PFNGLBLENDFUNCPROC glad_glBlendFunc;
#define glBlendFunc glad_glBlendFunc
extern PFNGLDEPTHFUNCPROC glad_glDepthFunc;
#define glDepthFunc glad_glDepthFunc
extern PFNGLCULLFACEPROC glad_glCullFace;
#define glCullFace glad_glCullFace
extern PFNGLFRONTFACEPROC glad_glFrontFace;
#define glFrontFace glad_glFrontFace
extern PFNGLLINEWIDTHPROC glad_glLineWidth;
#define glLineWidth glad_glLineWidth
extern PFNGLPOINTSIZEPROC glad_glPointSize;
#define glPointSize glad_glPointSize
extern PFNGLSCISSORPROC glad_glScissor;
#define glScissor glad_glScissor
extern PFNGLTEXIMAGE2DPROC glad_glTexImage2D;
#define glTexImage2D glad_glTexImage2D
extern PFNGLTEXPARAMETERIPROC glad_glTexParameteri;
#define glTexParameteri glad_glTexParameteri
extern PFNGLGENTEXTURESPROC glad_glGenTextures;
#define glGenTextures glad_glGenTextures
extern PFNGLBINDTEXTUREPROC glad_glBindTexture;
#define glBindTexture glad_glBindTexture
extern PFNGLDELETETEXTURESPROC glad_glDeleteTextures;
#define glDeleteTextures glad_glDeleteTextures
extern PFNGLREADBUFFERPROC glad_glReadBuffer;
#define glReadBuffer glad_glReadBuffer
extern PFNGLREADPIXELSPROC glad_glReadPixels;
#define glReadPixels glad_glReadPixels
extern PFNGLGETFLOATVPROC glad_glGetFloatv;
#define glGetFloatv glad_glGetFloatv
extern PFNGLGETERRORPROC glad_glGetError;
#define glGetError glad_glGetError
extern PFNGLDEPTHMASKPROC glad_glDepthMask;
#define glDepthMask glad_glDepthMask
extern PFNGLFINISHPROC glad_glFinish;
#define glFinish glad_glFinish
extern PFNGLFLUSHPROC glad_glFlush;
#define glFlush glad_glFlush

extern PFNGLGENBUFFERSPROC glad_glGenBuffers;
#define glGenBuffers glad_glGenBuffers
extern PFNGLBINDBUFFERPROC glad_glBindBuffer;
#define glBindBuffer glad_glBindBuffer
extern PFNGLBUFFERDATAPROC glad_glBufferData;
#define glBufferData glad_glBufferData
extern PFNGLBUFFERSUBDATAPROC glad_glBufferSubData;
#define glBufferSubData glad_glBufferSubData
extern PFNGLDELETEBUFFERSPROC glad_glDeleteBuffers;
#define glDeleteBuffers glad_glDeleteBuffers

extern PFNGLCREATESHADERPROC glad_glCreateShader;
#define glCreateShader glad_glCreateShader
extern PFNGLSHADERSOURCEPROC glad_glShaderSource;
#define glShaderSource glad_glShaderSource
extern PFNGLCOMPILESHADERPROC glad_glCompileShader;
#define glCompileShader glad_glCompileShader
extern PFNGLGETSHADERIVPROC glad_glGetShaderiv;
#define glGetShaderiv glad_glGetShaderiv
extern PFNGLGETSHADERINFOLOGPROC glad_glGetShaderInfoLog;
#define glGetShaderInfoLog glad_glGetShaderInfoLog
extern PFNGLDELETESHADERPROC glad_glDeleteShader;
#define glDeleteShader glad_glDeleteShader
extern PFNGLCREATEPROGRAMPROC glad_glCreateProgram;
#define glCreateProgram glad_glCreateProgram
extern PFNGLATTACHSHADERPROC glad_glAttachShader;
#define glAttachShader glad_glAttachShader
extern PFNGLLINKPROGRAMPROC glad_glLinkProgram;
#define glLinkProgram glad_glLinkProgram
extern PFNGLGETPROGRAMIVPROC glad_glGetProgramiv;
#define glGetProgramiv glad_glGetProgramiv
extern PFNGLGETPROGRAMINFOLOGPROC glad_glGetProgramInfoLog;
#define glGetProgramInfoLog glad_glGetProgramInfoLog
extern PFNGLDELETEPROGRAMPROC glad_glDeleteProgram;
#define glDeleteProgram glad_glDeleteProgram
extern PFNGLUSEPROGRAMPROC glad_glUseProgram;
#define glUseProgram glad_glUseProgram
extern PFNGLGETUNIFORMLOCATIONPROC glad_glGetUniformLocation;
#define glGetUniformLocation glad_glGetUniformLocation
extern PFNGLUNIFORM1IPROC glad_glUniform1i;
#define glUniform1i glad_glUniform1i
extern PFNGLUNIFORM1FPROC glad_glUniform1f;
#define glUniform1f glad_glUniform1f
extern PFNGLUNIFORM2FPROC glad_glUniform2f;
#define glUniform2f glad_glUniform2f
extern PFNGLUNIFORM3FPROC glad_glUniform3f;
#define glUniform3f glad_glUniform3f
extern PFNGLUNIFORM4FPROC glad_glUniform4f;
#define glUniform4f glad_glUniform4f
extern PFNGLUNIFORMMATRIX4FVPROC glad_glUniformMatrix4fv;
#define glUniformMatrix4fv glad_glUniformMatrix4fv
extern PFNGLUNIFORM3FVPROC glad_glUniform3fv;
#define glUniform3fv glad_glUniform3fv
extern PFNGLUNIFORM1FVPROC glad_glUniform1fv;
#define glUniform1fv glad_glUniform1fv

extern PFNGLVERTEXATTRIBPOINTERPROC glad_glVertexAttribPointer;
#define glVertexAttribPointer glad_glVertexAttribPointer
extern PFNGLENABLEVERTEXATTRIBARRAYPROC glad_glEnableVertexAttribArray;
#define glEnableVertexAttribArray glad_glEnableVertexAttribArray
extern PFNGLDISABLEVERTEXATTRIBARRAYPROC glad_glDisableVertexAttribArray;
#define glDisableVertexAttribArray glad_glDisableVertexAttribArray

extern PFNGLGENVERTEXARRAYSPROC glad_glGenVertexArrays;
#define glGenVertexArrays glad_glGenVertexArrays
extern PFNGLBINDVERTEXARRAYPROC glad_glBindVertexArray;
#define glBindVertexArray glad_glBindVertexArray
extern PFNGLDELETEVERTEXARRAYSPROC glad_glDeleteVertexArrays;
#define glDeleteVertexArrays glad_glDeleteVertexArrays
extern PFNGLGENFRAMEBUFFERSPROC glad_glGenFramebuffers;
#define glGenFramebuffers glad_glGenFramebuffers
extern PFNGLBINDFRAMEBUFFERPROC glad_glBindFramebuffer;
#define glBindFramebuffer glad_glBindFramebuffer
extern PFNGLDELETEFRAMEBUFFERSPROC glad_glDeleteFramebuffers;
#define glDeleteFramebuffers glad_glDeleteFramebuffers
extern PFNGLFRAMEBUFFERTEXTURE2DPROC glad_glFramebufferTexture2D;
#define glFramebufferTexture2D glad_glFramebufferTexture2D
extern PFNGLCHECKFRAMEBUFFERSTATUSPROC glad_glCheckFramebufferStatus;
#define glCheckFramebufferStatus glad_glCheckFramebufferStatus
extern PFNGLGENERATEMIPMAPPROC glad_glGenerateMipmap;
#define glGenerateMipmap glad_glGenerateMipmap
extern PFNGLGETSTRINGIPROC glad_glGetStringi;
#define glGetStringi glad_glGetStringi
extern PFNGLGENRENDERBUFFERSPROC glad_glGenRenderbuffers;
#define glGenRenderbuffers glad_glGenRenderbuffers
extern PFNGLBINDRENDERBUFFERPROC glad_glBindRenderbuffer;
#define glBindRenderbuffer glad_glBindRenderbuffer
extern PFNGLRENDERBUFFERSTORAGEPROC glad_glRenderbufferStorage;
#define glRenderbufferStorage glad_glRenderbufferStorage
extern PFNGLFRAMEBUFFERRENDERBUFFERPROC glad_glFramebufferRenderbuffer;
#define glFramebufferRenderbuffer glad_glFramebufferRenderbuffer
extern PFNGLDELETERENDERBUFFERSPROC glad_glDeleteRenderbuffers;
#define glDeleteRenderbuffers glad_glDeleteRenderbuffers
extern PFNGLTEXIMAGE2DMULTISAMPLEPROC glad_glTexImage2DMultisample;
#define glTexImage2DMultisample glad_glTexImage2DMultisample
extern PFNGLBLITFRAMEBUFFERPROC glad_glBlitFramebuffer;
#define glBlitFramebuffer glad_glBlitFramebuffer
extern PFNGLDRAWBUFFERSPROC glad_glDrawBuffers;
#define glDrawBuffers glad_glDrawBuffers

extern PFNGLGETUNIFORMBLOCKINDEXPROC glad_glGetUniformBlockIndex;
#define glGetUniformBlockIndex glad_glGetUniformBlockIndex
extern PFNGLUNIFORMBLOCKBINDINGPROC glad_glUniformBlockBinding;
#define glUniformBlockBinding glad_glUniformBlockBinding
extern PFNGLBINDBUFFERBASEPROC glad_glBindBufferBase;
#define glBindBufferBase glad_glBindBufferBase
extern PFNGLBINDBUFFERRANGEPROC glad_glBindBufferRange;
#define glBindBufferRange glad_glBindBufferRange

extern PFNGLGENSAMPLERSPROC glad_glGenSamplers;
#define glGenSamplers glad_glGenSamplers
extern PFNGLBINDSAMPLERPROC glad_glBindSampler;
#define glBindSampler glad_glBindSampler
extern PFNGLSAMPLERPARAMETERIPROC glad_glSamplerParameteri;
#define glSamplerParameteri glad_glSamplerParameteri
extern PFNGLDELETESAMPLERSPROC glad_glDeleteSamplers;
#define glDeleteSamplers glad_glDeleteSamplers

extern PFNGLDEBUGMESSAGECALLBACKPROC glad_glDebugMessageCallback;
#define glDebugMessageCallback glad_glDebugMessageCallback
extern PFNGLDEBUGMESSAGECONTROLPROC glad_glDebugMessageControl;
#define glDebugMessageControl glad_glDebugMessageControl

extern PFNGLACTIVETEXTUREPROC glad_glActiveTexture;
#define glActiveTexture glad_glActiveTexture
extern PFNGLTEXSUBIMAGE2DPROC glad_glTexSubImage2D;
#define glTexSubImage2D glad_glTexSubImage2D
extern PFNGLTEXIMAGE3DPROC glad_glTexImage3D;
#define glTexImage3D glad_glTexImage3D
extern PFNGLTEXSUBIMAGE3DPROC glad_glTexSubImage3D;
#define glTexSubImage3D glad_glTexSubImage3D

extern PFNGLDRAWARRAYSINSTANCEDPROC glad_glDrawArraysInstanced;
#define glDrawArraysInstanced glad_glDrawArraysInstanced
extern PFNGLDRAWELEMENTSINSTANCEDPROC glad_glDrawElementsInstanced;
#define glDrawElementsInstanced glad_glDrawElementsInstanced
extern PFNGLVERTEXATTRIBDIVISORPROC glad_glVertexAttribDivisor;
#define glVertexAttribDivisor glad_glVertexAttribDivisor

extern PFNGLMAPBUFFERPROC glad_glMapBuffer;
#define glMapBuffer glad_glMapBuffer
extern PFNGLUNMAPBUFFERPROC glad_glUnmapBuffer;
#define glUnmapBuffer glad_glUnmapBuffer

extern PFNGLBUFFERSUBDATAPROC glad_glBufferSubData;
#define glBufferSubData glad_glBufferSubData
extern PFNGLVERTEXATTRIBIPOINTERPROC glad_glVertexAttribIPointer;
#define glVertexAttribIPointer glad_glVertexAttribIPointer
extern PFNGLDISPATCHCOMPUTEPROC glad_glDispatchCompute;
#define glDispatchCompute glad_glDispatchCompute
extern PFNGLMEMORYBARRIERPROC glad_glMemoryBarrier;
#define glMemoryBarrier glad_glMemoryBarrier
extern PFNGLBINDIMAGETEXTUREPROC glad_glBindImageTexture;
#define glBindImageTexture glad_glBindImageTexture
extern PFNGLTEXSTORAGE2DPROC glad_glTexStorage2D;
#define glTexStorage2D glad_glTexStorage2D
extern PFNGLTEXSTORAGE3DPROC glad_glTexStorage3D;
#define glTexStorage3D glad_glTexStorage3D

/* ── Loader ── */
typedef void* (*GLADloadproc)(const char* name);
int gladLoadGLLoader(GLADloadproc load);

#ifdef __cplusplus
}
#endif

#endif /* GLAD_H */
