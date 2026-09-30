/*
 * GLAD - OpenGL Loader (Generated for OpenGL 4.3 Core Profile)
 * 
 * This is a minimal glad loader. For production, generate a full version from:
 *   https://glad.dav1d.de/
 *   - Language: C/C++
 *   - Specification: OpenGL
 *   - Profile: Core
 *   - API gl: Version 4.3
 *   - Generate a loader: checked
 *
 * For now, we use a lightweight implementation that loads essential GL functions.
 */

#include <glad/glad.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
static HMODULE libGL = NULL;
#else
#include <dlfcn.h>
static void* libGL = NULL;
#endif

static int glad_gl_version = 0;

typedef void* (*GLADloadfunc)(const char* name);
static GLADloadfunc glad_loader = NULL;

static void* glad_get_proc(const char* name) {
    void* result = NULL;
    if (glad_loader) {
        result = glad_loader(name);
    }
#ifdef _WIN32
    if (!result && libGL) {
        result = (void*)GetProcAddress(libGL, name);
    }
#else
    if (!result && libGL) {
        result = dlsym(libGL, name);
    }
#endif
    return result;
}

static int glad_open_gl(void) {
#ifdef _WIN32
    libGL = LoadLibraryW(L"opengl32.dll");
    if (!libGL) return 0;
#elif defined(__APPLE__)
    static const char* NAMES[] = {
        "../Frameworks/OpenGL.framework/OpenGL",
        "/Library/Frameworks/OpenGL.framework/OpenGL",
        "/System/Library/Frameworks/OpenGL.framework/OpenGL",
        "/System/Library/Frameworks/OpenGL.framework/Versions/Current/OpenGL"
    };
    for (int i = 0; i < 4; i++) {
        libGL = dlopen(NAMES[i], RTLD_NOW | RTLD_GLOBAL);
        if (libGL) break;
    }
    if (!libGL) return 0;
#else
    libGL = dlopen("libGL.so.1", RTLD_NOW | RTLD_GLOBAL);
    if (!libGL) libGL = dlopen("libGL.so", RTLD_NOW | RTLD_GLOBAL);
    if (!libGL) return 0;
#endif
    return 1;
}

static void glad_close_gl(void) {
#ifdef _WIN32
    if (libGL) { FreeLibrary(libGL); libGL = NULL; }
#else
    if (libGL) { dlclose(libGL); libGL = NULL; }
#endif
}

/* ── GL function pointers ── */

/* Core 1.0-1.1 */
PFNGLCLEARPROC glad_glClear = NULL;
PFNGLCLEARCOLORPROC glad_glClearColor = NULL;
PFNGLENABLEPROC glad_glEnable = NULL;
PFNGLDISABLEPROC glad_glDisable = NULL;
PFNGLVIEWPORTPROC glad_glViewport = NULL;
PFNGLDRAWELEMENTSPROC glad_glDrawElements = NULL;
PFNGLDRAWARRAYSPROC glad_glDrawArrays = NULL;
PFNGLPOLYGONMODEPROC glad_glPolygonMode = NULL;
PFNGLGETINTEGERVPROC glad_glGetIntegerv = NULL;
PFNGLGETSTRINGPROC glad_glGetString = NULL;
PFNGLBLENDFUNCPROC glad_glBlendFunc = NULL;
PFNGLDEPTHFUNCPROC glad_glDepthFunc = NULL;
PFNGLCULLFACEPROC glad_glCullFace = NULL;
PFNGLFRONTFACEPROC glad_glFrontFace = NULL;
PFNGLLINEWIDTHPROC glad_glLineWidth = NULL;
PFNGLPOINTSIZEPROC glad_glPointSize = NULL;
PFNGLSCISSORPROC glad_glScissor = NULL;
PFNGLTEXIMAGE2DPROC glad_glTexImage2D = NULL;
PFNGLTEXPARAMETERIPROC glad_glTexParameteri = NULL;
PFNGLGENTEXTURESPROC glad_glGenTextures = NULL;
PFNGLBINDTEXTUREPROC glad_glBindTexture = NULL;
PFNGLDELETETEXTURESPROC glad_glDeleteTextures = NULL;
PFNGLREADBUFFERPROC glad_glReadBuffer = NULL;
PFNGLREADPIXELSPROC glad_glReadPixels = NULL;
PFNGLGETFLOATVPROC glad_glGetFloatv = NULL;
PFNGLGETERRORPROC glad_glGetError = NULL;
PFNGLDEPTHMASKPROC glad_glDepthMask = NULL;
PFNGLFINISHPROC glad_glFinish = NULL;
PFNGLFLUSHPROC glad_glFlush = NULL;

/* Core 1.5 - Buffers */
PFNGLGENBUFFERSPROC glad_glGenBuffers = NULL;
PFNGLBINDBUFFERPROC glad_glBindBuffer = NULL;
PFNGLBUFFERDATAPROC glad_glBufferData = NULL;
PFNGLBUFFERSUBDATAPROC glad_glBufferSubData = NULL;
PFNGLDELETEBUFFERSPROC glad_glDeleteBuffers = NULL;

/* Core 2.0 - Shaders */
PFNGLCREATESHADERPROC glad_glCreateShader = NULL;
PFNGLSHADERSOURCEPROC glad_glShaderSource = NULL;
PFNGLCOMPILESHADERPROC glad_glCompileShader = NULL;
PFNGLGETSHADERIVPROC glad_glGetShaderiv = NULL;
PFNGLGETSHADERINFOLOGPROC glad_glGetShaderInfoLog = NULL;
PFNGLDELETESHADERPROC glad_glDeleteShader = NULL;
PFNGLCREATEPROGRAMPROC glad_glCreateProgram = NULL;
PFNGLATTACHSHADERPROC glad_glAttachShader = NULL;
PFNGLLINKPROGRAMPROC glad_glLinkProgram = NULL;
PFNGLGETPROGRAMIVPROC glad_glGetProgramiv = NULL;
PFNGLGETPROGRAMINFOLOGPROC glad_glGetProgramInfoLog = NULL;
PFNGLDELETEPROGRAMPROC glad_glDeleteProgram = NULL;
PFNGLUSEPROGRAMPROC glad_glUseProgram = NULL;
PFNGLGETUNIFORMLOCATIONPROC glad_glGetUniformLocation = NULL;
PFNGLUNIFORM1IPROC glad_glUniform1i = NULL;
PFNGLUNIFORM1FPROC glad_glUniform1f = NULL;
PFNGLUNIFORM2FPROC glad_glUniform2f = NULL;
PFNGLUNIFORM3FPROC glad_glUniform3f = NULL;
PFNGLUNIFORM4FPROC glad_glUniform4f = NULL;
PFNGLUNIFORMMATRIX4FVPROC glad_glUniformMatrix4fv = NULL;
PFNGLUNIFORM3FVPROC glad_glUniform3fv = NULL;
PFNGLUNIFORM1FVPROC glad_glUniform1fv = NULL;

/* Core 2.0 - Vertex Attribs */
PFNGLVERTEXATTRIBPOINTERPROC glad_glVertexAttribPointer = NULL;
PFNGLENABLEVERTEXATTRIBARRAYPROC glad_glEnableVertexAttribArray = NULL;
PFNGLDISABLEVERTEXATTRIBARRAYPROC glad_glDisableVertexAttribArray = NULL;

/* Core 3.0 - VAOs and FBOs */
PFNGLGENVERTEXARRAYSPROC glad_glGenVertexArrays = NULL;
PFNGLBINDVERTEXARRAYPROC glad_glBindVertexArray = NULL;
PFNGLDELETEVERTEXARRAYSPROC glad_glDeleteVertexArrays = NULL;
PFNGLGENFRAMEBUFFERSPROC glad_glGenFramebuffers = NULL;
PFNGLBINDFRAMEBUFFERPROC glad_glBindFramebuffer = NULL;
PFNGLDELETEFRAMEBUFFERSPROC glad_glDeleteFramebuffers = NULL;
PFNGLFRAMEBUFFERTEXTURE2DPROC glad_glFramebufferTexture2D = NULL;
PFNGLCHECKFRAMEBUFFERSTATUSPROC glad_glCheckFramebufferStatus = NULL;
PFNGLGENERATEMIPMAPPROC glad_glGenerateMipmap = NULL;
PFNGLGETSTRINGIPROC glad_glGetStringi = NULL;
PFNGLGENRENDERBUFFERSPROC glad_glGenRenderbuffers = NULL;
PFNGLBINDRENDERBUFFERPROC glad_glBindRenderbuffer = NULL;
PFNGLRENDERBUFFERSTORAGEPROC glad_glRenderbufferStorage = NULL;
PFNGLFRAMEBUFFERRENDERBUFFERPROC glad_glFramebufferRenderbuffer = NULL;
PFNGLDELETERENDERBUFFERSPROC glad_glDeleteRenderbuffers = NULL;
PFNGLTEXIMAGE2DMULTISAMPLEPROC glad_glTexImage2DMultisample = NULL;
PFNGLBLITFRAMEBUFFERPROC glad_glBlitFramebuffer = NULL;
PFNGLDRAWBUFFERSPROC glad_glDrawBuffers = NULL;

/* Core 3.1 - Uniform Buffers */
PFNGLGETUNIFORMBLOCKINDEXPROC glad_glGetUniformBlockIndex = NULL;
PFNGLUNIFORMBLOCKBINDINGPROC glad_glUniformBlockBinding = NULL;
PFNGLBINDBUFFERBASEPROC glad_glBindBufferBase = NULL;
PFNGLBINDBUFFERRANGEPROC glad_glBindBufferRange = NULL;

/* Core 3.3 - Samplers */
PFNGLGENSAMPLERSPROC glad_glGenSamplers = NULL;
PFNGLBINDSAMPLERPROC glad_glBindSampler = NULL;
PFNGLSAMPLERPARAMETERIPROC glad_glSamplerParameteri = NULL;
PFNGLDELETESAMPLERSPROC glad_glDeleteSamplers = NULL;

/* Core 4.3 - Debug */
PFNGLDEBUGMESSAGECALLBACKPROC glad_glDebugMessageCallback = NULL;
PFNGLDEBUGMESSAGECONTROLPROC glad_glDebugMessageControl = NULL;

/* Texture operations */
PFNGLACTIVETEXTUREPROC glad_glActiveTexture = NULL;
PFNGLTEXSUBIMAGE2DPROC glad_glTexSubImage2D = NULL;
PFNGLTEXIMAGE3DPROC glad_glTexImage3D = NULL;
PFNGLTEXSUBIMAGE3DPROC glad_glTexSubImage3D = NULL;

/* Instancing */
PFNGLDRAWARRAYSINSTANCEDPROC glad_glDrawArraysInstanced = NULL;
PFNGLDRAWELEMENTSINSTANCEDPROC glad_glDrawElementsInstanced = NULL;
PFNGLVERTEXATTRIBDIVISORPROC glad_glVertexAttribDivisor = NULL;

/* Map buffer */
PFNGLMAPBUFFERPROC glad_glMapBuffer = NULL;
PFNGLUNMAPBUFFERPROC glad_glUnmapBuffer = NULL;

/* OpenGL 4.3+ Compute & Texture Storage */
PFNGLVERTEXATTRIBIPOINTERPROC glad_glVertexAttribIPointer = NULL;
PFNGLDISPATCHCOMPUTEPROC glad_glDispatchCompute = NULL;
PFNGLMEMORYBARRIERPROC glad_glMemoryBarrier = NULL;
PFNGLBINDIMAGETEXTUREPROC glad_glBindImageTexture = NULL;
PFNGLTEXSTORAGE2DPROC glad_glTexStorage2D = NULL;
PFNGLTEXSTORAGE3DPROC glad_glTexStorage3D = NULL;

static void glad_load_gl_functions(void) {
    /* Core 1.0-1.1 */
    glad_glClear = (PFNGLCLEARPROC)glad_get_proc("glClear");
    glad_glClearColor = (PFNGLCLEARCOLORPROC)glad_get_proc("glClearColor");
    glad_glEnable = (PFNGLENABLEPROC)glad_get_proc("glEnable");
    glad_glDisable = (PFNGLDISABLEPROC)glad_get_proc("glDisable");
    glad_glViewport = (PFNGLVIEWPORTPROC)glad_get_proc("glViewport");
    glad_glDrawElements = (PFNGLDRAWELEMENTSPROC)glad_get_proc("glDrawElements");
    glad_glDrawArrays = (PFNGLDRAWARRAYSPROC)glad_get_proc("glDrawArrays");
    glad_glPolygonMode = (PFNGLPOLYGONMODEPROC)glad_get_proc("glPolygonMode");
    glad_glGetIntegerv = (PFNGLGETINTEGERVPROC)glad_get_proc("glGetIntegerv");
    glad_glGetString = (PFNGLGETSTRINGPROC)glad_get_proc("glGetString");
    glad_glBlendFunc = (PFNGLBLENDFUNCPROC)glad_get_proc("glBlendFunc");
    glad_glDepthFunc = (PFNGLDEPTHFUNCPROC)glad_get_proc("glDepthFunc");
    glad_glCullFace = (PFNGLCULLFACEPROC)glad_get_proc("glCullFace");
    glad_glFrontFace = (PFNGLFRONTFACEPROC)glad_get_proc("glFrontFace");
    glad_glLineWidth = (PFNGLLINEWIDTHPROC)glad_get_proc("glLineWidth");
    glad_glPointSize = (PFNGLPOINTSIZEPROC)glad_get_proc("glPointSize");
    glad_glScissor = (PFNGLSCISSORPROC)glad_get_proc("glScissor");
    glad_glTexImage2D = (PFNGLTEXIMAGE2DPROC)glad_get_proc("glTexImage2D");
    glad_glTexParameteri = (PFNGLTEXPARAMETERIPROC)glad_get_proc("glTexParameteri");
    glad_glGenTextures = (PFNGLGENTEXTURESPROC)glad_get_proc("glGenTextures");
    glad_glBindTexture = (PFNGLBINDTEXTUREPROC)glad_get_proc("glBindTexture");
    glad_glDeleteTextures = (PFNGLDELETETEXTURESPROC)glad_get_proc("glDeleteTextures");
    glad_glReadBuffer = (PFNGLREADBUFFERPROC)glad_get_proc("glReadBuffer");
    glad_glReadPixels = (PFNGLREADPIXELSPROC)glad_get_proc("glReadPixels");
    glad_glGetFloatv = (PFNGLGETFLOATVPROC)glad_get_proc("glGetFloatv");
    glad_glGetError = (PFNGLGETERRORPROC)glad_get_proc("glGetError");
    glad_glDepthMask = (PFNGLDEPTHMASKPROC)glad_get_proc("glDepthMask");
    glad_glFinish = (PFNGLFINISHPROC)glad_get_proc("glFinish");
    glad_glFlush = (PFNGLFLUSHPROC)glad_get_proc("glFlush");

    /* Core 1.3+ */
    glad_glActiveTexture = (PFNGLACTIVETEXTUREPROC)glad_get_proc("glActiveTexture");
    glad_glTexSubImage2D = (PFNGLTEXSUBIMAGE2DPROC)glad_get_proc("glTexSubImage2D");
    glad_glTexImage3D = (PFNGLTEXIMAGE3DPROC)glad_get_proc("glTexImage3D");
    glad_glTexSubImage3D = (PFNGLTEXSUBIMAGE3DPROC)glad_get_proc("glTexSubImage3D");

    /* Core 1.5 - Buffers */
    glad_glGenBuffers = (PFNGLGENBUFFERSPROC)glad_get_proc("glGenBuffers");
    glad_glBindBuffer = (PFNGLBINDBUFFERPROC)glad_get_proc("glBindBuffer");
    glad_glBufferData = (PFNGLBUFFERDATAPROC)glad_get_proc("glBufferData");
    glad_glBufferSubData = (PFNGLBUFFERSUBDATAPROC)glad_get_proc("glBufferSubData");
    glad_glDeleteBuffers = (PFNGLDELETEBUFFERSPROC)glad_get_proc("glDeleteBuffers");
    glad_glMapBuffer = (PFNGLMAPBUFFERPROC)glad_get_proc("glMapBuffer");
    glad_glUnmapBuffer = (PFNGLUNMAPBUFFERPROC)glad_get_proc("glUnmapBuffer");

    /* Core 2.0 - Shaders */
    glad_glCreateShader = (PFNGLCREATESHADERPROC)glad_get_proc("glCreateShader");
    glad_glShaderSource = (PFNGLSHADERSOURCEPROC)glad_get_proc("glShaderSource");
    glad_glCompileShader = (PFNGLCOMPILESHADERPROC)glad_get_proc("glCompileShader");
    glad_glGetShaderiv = (PFNGLGETSHADERIVPROC)glad_get_proc("glGetShaderiv");
    glad_glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)glad_get_proc("glGetShaderInfoLog");
    glad_glDeleteShader = (PFNGLDELETESHADERPROC)glad_get_proc("glDeleteShader");
    glad_glCreateProgram = (PFNGLCREATEPROGRAMPROC)glad_get_proc("glCreateProgram");
    glad_glAttachShader = (PFNGLATTACHSHADERPROC)glad_get_proc("glAttachShader");
    glad_glLinkProgram = (PFNGLLINKPROGRAMPROC)glad_get_proc("glLinkProgram");
    glad_glGetProgramiv = (PFNGLGETPROGRAMIVPROC)glad_get_proc("glGetProgramiv");
    glad_glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)glad_get_proc("glGetProgramInfoLog");
    glad_glDeleteProgram = (PFNGLDELETEPROGRAMPROC)glad_get_proc("glDeleteProgram");
    glad_glUseProgram = (PFNGLUSEPROGRAMPROC)glad_get_proc("glUseProgram");
    glad_glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)glad_get_proc("glGetUniformLocation");
    glad_glUniform1i = (PFNGLUNIFORM1IPROC)glad_get_proc("glUniform1i");
    glad_glUniform1f = (PFNGLUNIFORM1FPROC)glad_get_proc("glUniform1f");
    glad_glUniform2f = (PFNGLUNIFORM2FPROC)glad_get_proc("glUniform2f");
    glad_glUniform3f = (PFNGLUNIFORM3FPROC)glad_get_proc("glUniform3f");
    glad_glUniform4f = (PFNGLUNIFORM4FPROC)glad_get_proc("glUniform4f");
    glad_glUniformMatrix4fv = (PFNGLUNIFORMMATRIX4FVPROC)glad_get_proc("glUniformMatrix4fv");
    glad_glUniform3fv = (PFNGLUNIFORM3FVPROC)glad_get_proc("glUniform3fv");
    glad_glUniform1fv = (PFNGLUNIFORM1FVPROC)glad_get_proc("glUniform1fv");

    /* Core 2.0 - Vertex Attribs */
    glad_glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)glad_get_proc("glVertexAttribPointer");
    glad_glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)glad_get_proc("glEnableVertexAttribArray");
    glad_glDisableVertexAttribArray = (PFNGLDISABLEVERTEXATTRIBARRAYPROC)glad_get_proc("glDisableVertexAttribArray");

    /* Core 3.0 - VAOs and FBOs */
    glad_glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)glad_get_proc("glGenVertexArrays");
    glad_glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)glad_get_proc("glBindVertexArray");
    glad_glDeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSPROC)glad_get_proc("glDeleteVertexArrays");
    glad_glGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC)glad_get_proc("glGenFramebuffers");
    glad_glBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC)glad_get_proc("glBindFramebuffer");
    glad_glDeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSPROC)glad_get_proc("glDeleteFramebuffers");
    glad_glFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC)glad_get_proc("glFramebufferTexture2D");
    glad_glCheckFramebufferStatus = (PFNGLCHECKFRAMEBUFFERSTATUSPROC)glad_get_proc("glCheckFramebufferStatus");
    glad_glGenerateMipmap = (PFNGLGENERATEMIPMAPPROC)glad_get_proc("glGenerateMipmap");
    glad_glGetStringi = (PFNGLGETSTRINGIPROC)glad_get_proc("glGetStringi");
    glad_glGenRenderbuffers = (PFNGLGENRENDERBUFFERSPROC)glad_get_proc("glGenRenderbuffers");
    glad_glBindRenderbuffer = (PFNGLBINDRENDERBUFFERPROC)glad_get_proc("glBindRenderbuffer");
    glad_glRenderbufferStorage = (PFNGLRENDERBUFFERSTORAGEPROC)glad_get_proc("glRenderbufferStorage");
    glad_glFramebufferRenderbuffer = (PFNGLFRAMEBUFFERRENDERBUFFERPROC)glad_get_proc("glFramebufferRenderbuffer");
    glad_glDeleteRenderbuffers = (PFNGLDELETERENDERBUFFERSPROC)glad_get_proc("glDeleteRenderbuffers");
    glad_glTexImage2DMultisample = (PFNGLTEXIMAGE2DMULTISAMPLEPROC)glad_get_proc("glTexImage2DMultisample");
    glad_glBlitFramebuffer = (PFNGLBLITFRAMEBUFFERPROC)glad_get_proc("glBlitFramebuffer");
    glad_glDrawBuffers = (PFNGLDRAWBUFFERSPROC)glad_get_proc("glDrawBuffers");

    /* Core 3.1 - Uniform Buffers */
    glad_glGetUniformBlockIndex = (PFNGLGETUNIFORMBLOCKINDEXPROC)glad_get_proc("glGetUniformBlockIndex");
    glad_glUniformBlockBinding = (PFNGLUNIFORMBLOCKBINDINGPROC)glad_get_proc("glUniformBlockBinding");
    glad_glBindBufferBase = (PFNGLBINDBUFFERBASEPROC)glad_get_proc("glBindBufferBase");
    glad_glBindBufferRange = (PFNGLBINDBUFFERRANGEPROC)glad_get_proc("glBindBufferRange");

    /* Core 3.3 - Samplers */
    glad_glGenSamplers = (PFNGLGENSAMPLERSPROC)glad_get_proc("glGenSamplers");
    glad_glBindSampler = (PFNGLBINDSAMPLERPROC)glad_get_proc("glBindSampler");
    glad_glSamplerParameteri = (PFNGLSAMPLERPARAMETERIPROC)glad_get_proc("glSamplerParameteri");
    glad_glDeleteSamplers = (PFNGLDELETESAMPLERSPROC)glad_get_proc("glDeleteSamplers");

    /* Core 4.3 - Debug */
    glad_glDebugMessageCallback = (PFNGLDEBUGMESSAGECALLBACKPROC)glad_get_proc("glDebugMessageCallback");
    glad_glDebugMessageControl = (PFNGLDEBUGMESSAGECONTROLPROC)glad_get_proc("glDebugMessageControl");

    /* Instancing */
    glad_glDrawArraysInstanced = (PFNGLDRAWARRAYSINSTANCEDPROC)glad_get_proc("glDrawArraysInstanced");
    glad_glDrawElementsInstanced = (PFNGLDRAWELEMENTSINSTANCEDPROC)glad_get_proc("glDrawElementsInstanced");
    glad_glVertexAttribDivisor = (PFNGLVERTEXATTRIBDIVISORPROC)glad_get_proc("glVertexAttribDivisor");

    /* OpenGL 4.3+ Compute & Texture Storage */
    glad_glVertexAttribIPointer = (PFNGLVERTEXATTRIBIPOINTERPROC)glad_get_proc("glVertexAttribIPointer");
    glad_glDispatchCompute = (PFNGLDISPATCHCOMPUTEPROC)glad_get_proc("glDispatchCompute");
    glad_glMemoryBarrier = (PFNGLMEMORYBARRIERPROC)glad_get_proc("glMemoryBarrier");
    glad_glBindImageTexture = (PFNGLBINDIMAGETEXTUREPROC)glad_get_proc("glBindImageTexture");
    glad_glTexStorage2D = (PFNGLTEXSTORAGE2DPROC)glad_get_proc("glTexStorage2D");
    glad_glTexStorage3D = (PFNGLTEXSTORAGE3DPROC)glad_get_proc("glTexStorage3D");
}

int gladLoadGLLoader(GLADloadproc load) {
    glad_loader = load;

    if (!glad_open_gl()) return 0;

    glad_load_gl_functions();

    glad_close_gl();

    /* Determine GL version */
    if (glad_glGetIntegerv) {
        int major = 0, minor = 0;
        glad_glGetIntegerv(GL_MAJOR_VERSION, &major);
        glad_glGetIntegerv(GL_MINOR_VERSION, &minor);
        glad_gl_version = major * 10 + minor;
    }

    return glad_gl_version;
}
