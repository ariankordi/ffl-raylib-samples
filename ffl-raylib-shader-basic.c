#if defined(_WIN32)
    #define NOGDI             // All GDI defines and routines
    #define NOUSER            // All USER defines and routines
#endif

#include "raylib.h"

#if defined(PLATFORM_DESKTOP) || defined(PLATFORM_DESKTOP_SDL)
    #if defined(GRAPHICS_API_OPENGL_ES2)
        #include "glad/gles2.h"       // Required for: OpenGL functionality
        #define VAO_NOT_SUPPORTED
        #define GLSL_VERSION            100
    #else
        #if defined(__APPLE__)
            // NOTE: ignoring macOS opengl 2.1
            #define GL_SILENCE_DEPRECATION // Silence Opengl API deprecation warnings
            #include <OpenGL/gl3.h>     // OpenGL 3 library for OSX
            #include <OpenGL/gl3ext.h>  // OpenGL 3 extensions library for OSX
            #define GLSL_VERSION            330
        #else
            #if defined(GRAPHICS_API_OPENGL_21)
                #include "glad/gl2.h" // NOTE: DOES NOT EXIST RN
                #define VAO_NOT_SUPPORTED
                #define GLSL_VERSION            120
            #else
                #include "glad/gl.h"       // Required for: OpenGL functionality
                #define GLSL_VERSION            330
            #endif
        #endif
    #endif
#else   // PLATFORM_ANDROID, PLATFORM_WEB

    // HACK: ONLY FOR CLANGD
    #include "glad/gles2.h"

    #define VAO_NOT_SUPPORTED
    #define GLSL_VERSION            100
#endif

/*
#if defined(_WIN32)           // raylib uses these names as function parameters
    #undef near
    #undef far
#endif
*/

#include "rlgl.h"           // Required for: rlDrawRenderBatchActive(), rlGetMatrixModelview(), rlGetMatrixProjection()
#include "raymath.h"        // Required for: MatrixMultiply(), MatrixToFloat()

#include <stdio.h>
#include <stdlib.h>

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#define GLSL_VERT(src) "#version " STR(GLSL_VERSION) "\n" \
    "#ifndef GL_ES\n" \
        "#define attribute in\n" \
        "#define varying out\n" \
    "#endif\n" \
#src
#define GLSL_FRAG(src) "#version " STR(GLSL_VERSION) "\n" \
    "#ifndef GL_ES\n" \
        "#define varying in\n" \
        "#define gl_FragColor FragColor\n" \
        "#define texture2D texture\n" \
        "out vec4 FragColor;\n" \
    "#endif\n" \
#src

const char* vertexShaderCodeFFL = GLSL_VERT(
    precision highp float;

    attribute vec4 a_position;
    attribute vec2 a_texCoord;
    /*
    attribute vec3 a_normal;
    attribute vec4 a_color;
    attribute vec3 a_tangent;
    */

    /*
    varying   vec4 v_color;
    */
    varying   vec4 v_position;
    /*
    varying   vec3 v_normal;
    varying   vec3 v_tangent;
    */
    varying   vec2 v_texCoord;

    uniform   mat4 u_mv;
    uniform   mat4 u_proj;
    //uniform   mat3 u_it;

    void main()
    {
        gl_Position = u_proj * u_mv * a_position;
        v_position = u_mv * a_position;

        //v_normal = normalize(u_it * a_normal);
        v_texCoord = a_texCoord;
        //v_tangent = normalize(u_it * a_tangent);
        //v_color = a_color;
    }
);

const char* fragmentShaderCodeFFL = GLSL_FRAG(
    precision mediump float;

    uniform int u_mode;
    uniform vec3 u_const1;
    uniform vec3 u_const2;
    uniform vec3 u_const3;
    uniform sampler2D s_texture;

    varying vec2 v_texCoord;

    void main(void)
    {
        if (u_mode == 0)
            gl_FragColor = vec4(u_const1, 1.0);

        else if (u_mode == 1)
            gl_FragColor = texture2D(s_texture, v_texCoord);

        else if (u_mode == 2)
        {
            vec4 textureColor = texture2D(s_texture, v_texCoord);
            gl_FragColor = vec4(
                u_const1 * textureColor.r +
                u_const2 * textureColor.g +
                u_const3 * textureColor.b,
                textureColor.a
            );
        }
        else if (u_mode == 3)
        {
            vec4 textureColor = texture2D(s_texture, v_texCoord);
            gl_FragColor = vec4(
                u_const1 * textureColor.r,
                textureColor.r
            );
        }
        else if (u_mode == 4)
        {
            vec4 textureColor = texture2D(s_texture, v_texCoord);
            gl_FragColor = vec4(
                u_const1 * textureColor.g,
                textureColor.r
            );
        }
        else if (u_mode == 5)
        {
            vec4 textureColor = texture2D(s_texture, v_texCoord);
            gl_FragColor = vec4(
                u_const1 * textureColor.r,
                1.0
            );
        }
    }
);

// Shader uniform enums for shader for FFL
enum ShaderFFLVertexUniform
{
    SH_FFL_VERTEX_UNIFORM_MV = 0,
    SH_FFL_VERTEX_UNIFORM_PROJ,
    SH_FFL_VERTEX_UNIFORM_IT,
    SH_FFL_VERTEX_UNIFORM_MAX
};
enum ShaderFFLPixelUniform
{
    SH_FFL_PIXEL_UNIFORM_CONST1 = 0,
    SH_FFL_PIXEL_UNIFORM_CONST2,
    SH_FFL_PIXEL_UNIFORM_CONST3,
    SH_FFL_PIXEL_UNIFORM_MODE,
    SH_FFL_PIXEL_UNIFORM_MAX
};

#include <nn/ffl.h>

// Shader for FFL
typedef struct {
    Shader shader; // Raylib Shader
    int vertexUniformLocation[SH_FFL_VERTEX_UNIFORM_MAX];
    int pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_MAX];
    int samplerLocation;
    int attributeLocation[FFL_ATTRIBUTE_BUFFER_TYPE_MAX];
    GLuint vboHandle[FFL_ATTRIBUTE_BUFFER_TYPE_MAX];
    GLuint vaoHandle;
    FFLShaderCallback callback;
    void* samplerTexture;
} ShaderForFFL;

// define global instance of the shader
ShaderForFFL gShaderForFFL;

// Callback forward declarations
void ShaderForFFL_ApplyAlphaTestCallback(void* pObj, bool enable, unsigned int func, float ref);
void ShaderForFFL_DrawCallback(void* pObj, const FFLDrawParam* drawParam);
void ShaderForFFL_SetMatrixCallback(void* pObj, const FFLRIOBaseMtx44f* pBaseMtx44f);

// Define RIO Texture2D for interop
typedef struct Texture2DRIO
{

    // Offset before mHandle
    char padding[128]; // NOTE: rio::Texture2D does NOT have pointers
    // We only need mHandle to bind the texture
    GLuint mHandle; // OpenGL texture handle
    // Other fields can be ignored for our purposes
} Texture2DRIO;

// Initialize the Shader
void ShaderForFFL_Initialize(ShaderForFFL* self)
{
    TraceLog(LOG_DEBUG, "In ShaderForFFL_Initialize");

    // Load the shader
    self->shader = LoadShaderFromMemory(vertexShaderCodeFFL, fragmentShaderCodeFFL);
    TraceLog(LOG_DEBUG, "Shader loaded");

    // Get uniform locations
    self->vertexUniformLocation[SH_FFL_VERTEX_UNIFORM_MV] = GetShaderLocation(self->shader, "u_mv");
    TraceLog(LOG_TRACE, "Vertex uniform 'u_mvp' location: %d", self->vertexUniformLocation[SH_FFL_VERTEX_UNIFORM_MV]);
    self->vertexUniformLocation[SH_FFL_VERTEX_UNIFORM_PROJ] = GetShaderLocation(self->shader, "u_proj");
    TraceLog(LOG_TRACE, "Vertex uniform 'u_proj' location: %d", self->vertexUniformLocation[SH_FFL_VERTEX_UNIFORM_PROJ]);

    self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_CONST1] = GetShaderLocation(self->shader, "u_const1");
    self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_CONST2] = GetShaderLocation(self->shader, "u_const2");
    self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_CONST3] = GetShaderLocation(self->shader, "u_const3");
    self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_MODE] = GetShaderLocation(self->shader, "u_mode");
    TraceLog(LOG_TRACE, "Pixel uniform 'u_const1' location: %d", self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_CONST1]);
    TraceLog(LOG_TRACE, "Pixel uniform 'u_const2' location: %d", self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_CONST2]);
    TraceLog(LOG_TRACE, "Pixel uniform 'u_const3' location: %d", self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_CONST3]);
    TraceLog(LOG_TRACE, "Pixel uniform 'u_mode' location: %d", self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_MODE]);

    self->samplerLocation = GetShaderLocation(self->shader, "s_texture");
    TraceLog(LOG_TRACE, "Sampler uniform 's_texture' location: %d", self->samplerLocation);

    // Get attribute locations
    self->attributeLocation[FFL_ATTRIBUTE_BUFFER_TYPE_COLOR] = GetShaderLocationAttrib(self->shader, "a_color");
    self->attributeLocation[FFL_ATTRIBUTE_BUFFER_TYPE_NORMAL] = GetShaderLocationAttrib(self->shader, "a_normal");
    self->attributeLocation[FFL_ATTRIBUTE_BUFFER_TYPE_POSITION] = GetShaderLocationAttrib(self->shader, "a_position");
    self->attributeLocation[FFL_ATTRIBUTE_BUFFER_TYPE_TANGENT] = GetShaderLocationAttrib(self->shader, "a_tangent");
    self->attributeLocation[FFL_ATTRIBUTE_BUFFER_TYPE_TEXCOORD] = GetShaderLocationAttrib(self->shader, "a_texCoord");
    TraceLog(LOG_TRACE, "Attribute 'a_color' location: %d", self->attributeLocation[FFL_ATTRIBUTE_BUFFER_TYPE_COLOR]);
    TraceLog(LOG_TRACE, "Attribute 'a_normal' location: %d", self->attributeLocation[FFL_ATTRIBUTE_BUFFER_TYPE_NORMAL]);
    TraceLog(LOG_TRACE, "Attribute 'a_position' location: %d", self->attributeLocation[FFL_ATTRIBUTE_BUFFER_TYPE_POSITION]);
    TraceLog(LOG_TRACE, "Attribute 'a_tangent' location: %d", self->attributeLocation[FFL_ATTRIBUTE_BUFFER_TYPE_TANGENT]);
    TraceLog(LOG_TRACE, "Attribute 'a_texCoord' location: %d", self->attributeLocation[FFL_ATTRIBUTE_BUFFER_TYPE_TEXCOORD]);

    // Create VBOs and VAO if supported
    #ifndef VAO_NOT_SUPPORTED
    TraceLog(LOG_TRACE, "Creating VAO...");
    glGenVertexArrays(1, &self->vaoHandle);
    glBindVertexArray(self->vaoHandle);
    TraceLog(LOG_TRACE, "VAO created and bound");
    #endif

    glGenBuffers(FFL_ATTRIBUTE_BUFFER_TYPE_MAX, self->vboHandle);
    TraceLog(LOG_TRACE, "VBOs created");

    #ifndef VAO_NOT_SUPPORTED
    glBindVertexArray(0);
    TraceLog(LOG_TRACE, "VAO unbound");
    #endif

    // Initialize the FFLShaderCallback
    self->callback.pObj = (void*)self;
    self->callback.pApplyAlphaTestFunc = ShaderForFFL_ApplyAlphaTestCallback;
    self->callback.pDrawFunc = ShaderForFFL_DrawCallback;
    self->callback.pSetMatrixFunc = ShaderForFFL_SetMatrixCallback;

    TraceLog(LOG_DEBUG, "FFLSetShaderCallback(%p)", &self->callback);
    FFLSetShaderCallback(&self->callback);
}

// Bind the Shader
void ShaderForFFL_Bind(ShaderForFFL* self)
{
    TraceLog(LOG_TRACE, "In ShaderForFFL_Bind, calling BeginShaderMode");

    BeginShaderMode(self->shader);

    #ifndef VAO_NOT_SUPPORTED
    glBindVertexArray(self->vaoHandle);
    TraceLog(LOG_TRACE, "VAO bound");
    #endif
/*
    for (int i = 0; i < FFL_ATTRIBUTE_BUFFER_TYPE_MAX; i++)
    {
        if (self->attributeLocation[i] != -1)
            glDisableVertexAttribArray(self->attributeLocation[i]);
            TraceLog(LOG_TRACE, "Disabled vertex attrib array at location: %d", self->attributeLocation[i]);
    }
*/
}

// Set View Uniform
void ShaderForFFL_SetViewUniform(ShaderForFFL* self, const Matrix* model_mtx, const Matrix* view_mtx, const Matrix* proj_mtx)
{
    TraceLog(LOG_TRACE, "Setting view uniform");
    Matrix mv;
    // use proj_mtx as mvp if view mtx is null
    if (view_mtx != NULL)
    {
        // use model_mtx as mv mtx if it is null
        if (model_mtx == NULL)
            mv = *view_mtx;
        else
            mv = MatrixMultiply(*model_mtx, *view_mtx);
    }
    if (view_mtx != NULL)
        SetShaderValueMatrix(self->shader, self->vertexUniformLocation[SH_FFL_VERTEX_UNIFORM_MV], mv);
    else
        // proj will be mvp in this case
        SetShaderValueMatrix(self->shader, self->vertexUniformLocation[SH_FFL_VERTEX_UNIFORM_MV], MatrixIdentity());
    SetShaderValueMatrix(self->shader, self->vertexUniformLocation[SH_FFL_VERTEX_UNIFORM_PROJ], *proj_mtx);
}

// Apply Alpha Test (no-op for now)
void ShaderForFFL_ApplyAlphaTest(bool enable, unsigned int func, float ref)
{
    // Alpha testing is not directly supported in OpenGL ES 2.0
    // This functionality can be emulated in the shader if needed
}

// Callback: Apply Alpha Test
void ShaderForFFL_ApplyAlphaTestCallback(void* pObj, bool enable, unsigned int func, float ref)
{
    ShaderForFFL_ApplyAlphaTest(enable, func, ref);
}

// Set Culling Mode
void ShaderForFFL_SetCulling(FFLCullMode mode)
{
    TraceLog(LOG_TRACE, "Setting FFLCullMode: %d", mode);

    switch (mode)
    {
    case FFL_CULL_MODE_NONE:
        glDisable(GL_CULL_FACE);
        TraceLog(LOG_TRACE, "Culling disabled (FFL_CULL_MODE_NONE)");
        break;
    case FFL_CULL_MODE_BACK:
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        TraceLog(LOG_TRACE, "Culling mode set to FFL_CULL_MODE_BACK");
        break;
    case FFL_CULL_MODE_FRONT:
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
        TraceLog(LOG_TRACE, "Culling mode set to FFL_CULL_MODE_FRONT");
        break;
    default:
        break;
    }
}

// Callback: Set Matrix
void ShaderForFFL_SetMatrixCallback(void* pObj, const FFLRIOBaseMtx44f* pBaseMtx44f)
{
    ShaderForFFL* self = (ShaderForFFL*)pObj;

    TraceLog(LOG_TRACE, "In ShaderForFFL_SetMatrixCallback");
    Matrix matrix;
    // Raylib matrix is in column-major order
    matrix.m0 = pBaseMtx44f->m[0][0];  matrix.m4 = pBaseMtx44f->m[0][1];  matrix.m8 = pBaseMtx44f->m[0][2];  matrix.m12 = pBaseMtx44f->m[0][3];
    matrix.m1 = pBaseMtx44f->m[1][0];  matrix.m5 = pBaseMtx44f->m[1][1];  matrix.m9 = pBaseMtx44f->m[1][2];  matrix.m13 = pBaseMtx44f->m[1][3];
    matrix.m2 = pBaseMtx44f->m[2][0];  matrix.m6 = pBaseMtx44f->m[2][1];  matrix.m10 = pBaseMtx44f->m[2][2]; matrix.m14 = pBaseMtx44f->m[2][3];
    matrix.m3 = pBaseMtx44f->m[3][0];  matrix.m7 = pBaseMtx44f->m[3][1];  matrix.m11 = pBaseMtx44f->m[3][2]; matrix.m15 = pBaseMtx44f->m[3][3];

    SetShaderValueMatrix(self->shader, self->vertexUniformLocation[SH_FFL_VERTEX_UNIFORM_MV], MatrixIdentity());
    SetShaderValueMatrix(self->shader, self->vertexUniformLocation[SH_FFL_VERTEX_UNIFORM_PROJ], matrix);
}

// Callback: Draw
void ShaderForFFL_DrawCallback(void* pObj, const FFLDrawParam* pDrawParam)
{
    ShaderForFFL* self = (ShaderForFFL*)pObj;

    TraceLog(LOG_TRACE, "Draw callback called, preparing to draw");

    ShaderForFFL_SetCulling(pDrawParam->cullMode);

    BeginShaderMode(self->shader);

    // Set u_mode uniform
    SetShaderValue(self->shader, self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_MODE], &pDrawParam->modulateParam.mode, SHADER_UNIFORM_INT);

    // Set uniforms based on mode
    switch (pDrawParam->modulateParam.mode)
    {
    case FFL_MODULATE_MODE_CONSTANT:
    case FFL_MODULATE_MODE_ALPHA:
    case FFL_MODULATE_MODE_LUMINANCE_ALPHA:
    case FFL_MODULATE_MODE_ALPHA_OPA:
        SetShaderValue(self->shader, self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_CONST1], &pDrawParam->modulateParam.pColorR->r, SHADER_UNIFORM_VEC3);
        break;
    case FFL_MODULATE_MODE_RGB_LAYERED:
        SetShaderValue(self->shader, self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_CONST1], &pDrawParam->modulateParam.pColorR->r, SHADER_UNIFORM_VEC3);
        SetShaderValue(self->shader, self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_CONST2], &pDrawParam->modulateParam.pColorG->r, SHADER_UNIFORM_VEC3);
        SetShaderValue(self->shader, self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_CONST3], &pDrawParam->modulateParam.pColorB->r, SHADER_UNIFORM_VEC3);
        break;
    default:
        break;
    }

    // Bind the texture if available
    if (pDrawParam->modulateParam.pTexture2D != NULL)
    {
        // Get the texture handle from Texture2D
        GLuint textureHandle = pDrawParam->modulateParam.pTexture2D->mHandle;
        TraceLog(LOG_TRACE, "Binding texture: %d", textureHandle);

        // Bind the texture to texture unit 0
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureHandle);

        // Set texture wrap to repeat
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);

        // Set the sampler uniform to use texture unit 0
        glUniform1i(self->samplerLocation, 0);
    }


    if (pDrawParam->primitiveParam.pIndexBuffer != NULL)
    {
        TraceLog(LOG_TRACE, "Binding index buffer: %p", pDrawParam->primitiveParam.pIndexBuffer);
        // Bind and set up vertex attributes
        #ifndef VAO_NOT_SUPPORTED
        glBindVertexArray(self->vaoHandle);
        #endif

        for (int type = 0; type < FFL_ATTRIBUTE_BUFFER_TYPE_MAX; ++type)
        {
            const FFLAttributeBuffer* buffer = &pDrawParam->attributeBufferParam.attributeBuffers[type];
            int location = self->attributeLocation[type];
            void* ptr = buffer->ptr;

            if (ptr != NULL && location != -1 && buffer->stride > 0)
            {
                unsigned int stride = buffer->stride;
                unsigned int vbo_handle = self->vboHandle[type];
                unsigned int size = buffer->size;

                glBindBuffer(GL_ARRAY_BUFFER, vbo_handle);
                glBufferData(GL_ARRAY_BUFFER, size, ptr, GL_STATIC_DRAW);
                glEnableVertexAttribArray(location);

                // Set attribute pointer based on type
                switch (type)
                {
                case FFL_ATTRIBUTE_BUFFER_TYPE_POSITION:
                    glVertexAttribPointer(location, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
                    break;
                case FFL_ATTRIBUTE_BUFFER_TYPE_NORMAL:
#ifndef VAO_NOT_SUPPORTED
                    glVertexAttribPointer(location, 4, GL_INT_2_10_10_10_REV, GL_TRUE, stride, (void*)0);
#else
                    glVertexAttribPointer(location, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
#endif
                    break;
                case FFL_ATTRIBUTE_BUFFER_TYPE_TANGENT:
                    glVertexAttribPointer(location, 4, GL_BYTE, GL_TRUE, stride, (void*)0);
                    break;
                case FFL_ATTRIBUTE_BUFFER_TYPE_TEXCOORD:
                    glVertexAttribPointer(location, 2, GL_FLOAT, GL_FALSE, stride, (void*)0);
                    break;
                case FFL_ATTRIBUTE_BUFFER_TYPE_COLOR:
                    glVertexAttribPointer(location, 4, GL_FLOAT, GL_FALSE, stride, (void*)0);
                    break;
                default:
                    break;
                }
            }
            else if (location != -1)
                glDisableVertexAttribArray(location);
        }

        // Create and bind index buffer
        GLuint indexBufferHandle;
        glGenBuffers(1, &indexBufferHandle);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferHandle);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, pDrawParam->primitiveParam.indexCount * sizeof(unsigned short), pDrawParam->primitiveParam.pIndexBuffer, GL_STATIC_DRAW);

        glDepthMask(GL_TRUE); // enable depth writing

        // Draw elements
        // primitiveType maps directly to OpenGL primitives
        TraceLog(LOG_TRACE, "glDrawElements(%d)", pDrawParam->primitiveParam.indexCount);
        glDrawElements(pDrawParam->primitiveParam.primitiveType, pDrawParam->primitiveParam.indexCount, GL_UNSIGNED_SHORT, 0);

        // Cleanup
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glDeleteBuffers(1, &indexBufferHandle);

        #ifndef VAO_NOT_SUPPORTED
        glBindVertexArray(0);
        #endif
    }

    // Unbind the texture
    glBindTexture(GL_TEXTURE_2D, 0);

    // Disable shader
    EndShaderMode();
}


FFLResourceDesc gResourceDesc; // Global so data can be freed

//const char* cFFLResourceHighFilename = "/home/arian/Downloads/ffl/tools/AFLResHigh_2_3_LE.dat";
const char* cFFLResourceHighFilename = "./FFLResHigh.dat";

// Calls FFLInitResEx and returns the result of FFLIsAvailable.
FFLResult InitializeFFL()
{
    TraceLog(LOG_DEBUG, "Before FFL initialization");
    FFLInitDesc init_desc = {
        .fontRegion = FFL_FONT_REGION_JP_US_EU,
        ._c = false,
        ._10 = true
    };

    // Reset high size to 0 indicating it is not allocated
    gResourceDesc.size[FFL_RESOURCE_TYPE_HIGH] = 0;
    gResourceDesc.size[FFL_RESOURCE_TYPE_MIDDLE] = 0; // Skip middle
    // Open the file in binary mode
    FILE* file = fopen(cFFLResourceHighFilename, "rb");
    if (!file) {
        TraceLog(LOG_ERROR, "Error: Cannot open file %s", cFFLResourceHighFilename);
        return FFL_RESULT_FS_ERROR;
    }
    TraceLog(LOG_DEBUG, "Opened %s", cFFLResourceHighFilename);
    // Seek to the end to determine file size
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);  // Go back to the start of the file
    if (fileSize <= 0) {
        TraceLog(LOG_ERROR, "Invalid file size for %s", cFFLResourceHighFilename);
        fclose(file);
        return FFL_RESULT_FS_ERROR;
    }
    /*
    const size_t alignment = 0x2000;
    // Allocate aligned memory
    void* fileData = NULL;
    if (posix_memalign(&fileData, alignment, (size_t)fileSize)) {
        TraceLog(LOG_ERROR, "Cannot allocate aligned memory");
        fclose(file);
        return FFL_RESULT_ERROR;
    }
    */
    void* fileData = malloc((size_t)fileSize);
    // Read the file data into fileData
    size_t bytesRead = fread(fileData, 1, (size_t)fileSize, file);
    if (bytesRead == fileSize) {
        // Store the data and size in the appropriate resource type slot
        gResourceDesc.pData[FFL_RESOURCE_TYPE_HIGH] = fileData;
        gResourceDesc.size[FFL_RESOURCE_TYPE_HIGH] = (size_t)fileSize;
    } else {
        TraceLog(LOG_ERROR, "Cannot read file %s", cFFLResourceHighFilename);
        free(fileData);
        fclose(file);

        return FFL_RESULT_FS_ERROR;
    }
    // Close the file because we are done reading it
    fclose(file);

    FFLResult result;
    TraceLog(LOG_DEBUG, "Calling FFLInitResEx");
    result = FFLInitResEx(&init_desc, &gResourceDesc);
    //FFLResult result = FFLInitResEx(&init_desc, NULL); // lets ffl find resources itself

    if (result != FFL_RESULT_OK)
    {
        TraceLog(LOG_ERROR, "FFLInitResEx() failed with result: %d", (s32)result);
        assert(false);
        return result;
    }

    assert(FFLIsAvailable());

    FFLInitResGPUStep(); // no-op on win

    TraceLog(LOG_DEBUG, "Exiting InitializeFFL()");

    return result;
}

const unsigned char cJasmineStoreData[96] = {
    0x03, 0x00, 0x00, 0x40, 0xA0, 0x41, 0x38, 0xC4, 0xA0, 0x84, 0x00, 0x00, 0xDB, 0xB8, 0x87, 0x31, 0xBE, 0x60, 0x2B, 0x2A, 0x2A, 0x42, 0x00, 0x00, 0x59, 0x2D, 0x4A, 0x00, 0x61, 0x00, 0x73, 0x00, 0x6D, 0x00, 0x69, 0x00, 0x6E, 0x00, 0x65, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1C, 0x37, 0x12, 0x10, 0x7B, 0x01, 0x21, 0x6E, 0x43, 0x1C, 0x0D, 0x64, 0xC7, 0x18, 0x00, 0x08, 0x1E, 0x82, 0x0D, 0x00, 0x30, 0x41, 0xB3, 0x5B, 0x82, 0x6D, 0x00, 0x00, 0x6F, 0x00, 0x73, 0x00, 0x69, 0x00, 0x67, 0x00, 0x6F, 0x00, 0x6E, 0x00, 0x61, 0x00, 0x6C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x90, 0x3A
};

// forward decls
void ExitFFL();

FFLResult CreateCharModelFromStoreData(FFLCharModel* pCharModel, const void* pStoreDataBuffer)
{
    FFLCharModelSource modelSource = {
        .index = 0,
        .dataSource = FFL_DATA_SOURCE_STORE_DATA,
        .pBuffer = pStoreDataBuffer
    };
    const u32 expressionFlag =
                (1 << FFL_EXPRESSION_NORMAL
               | 1 << FFL_EXPRESSION_BLINK);

    FFLCharModelDesc modelDesc = {
        .resolution = (FFLResolution)256,
        .expressionFlag = expressionFlag,
        .modelFlag = FFL_MODEL_FLAG_NORMAL,
        .resourceType = FFL_RESOURCE_TYPE_HIGH
    };

    FFLResult result;

    TraceLog(LOG_DEBUG, "Calling FFLInitCharModelCPUStep");
    result = FFLInitCharModelCPUStep(pCharModel, &modelSource, &modelDesc);

    if (result != FFL_RESULT_OK)
    {
        TraceLog(LOG_ERROR, "FFLInitCharModelCPUStep failed with result: %d", result);
        return result;
    }

    TraceLog(LOG_DEBUG, "Calling ShaderForFFL_Bind");
    ShaderForFFL_Bind(&gShaderForFFL);

    // Arrays to store the viewport and scissor box dimensions
    GLint viewport[4];
    GLint scissorBox[4];
    // Save the current viewport and scissor box
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetIntegerv(GL_SCISSOR_BOX, scissorBox);

    TraceLog(LOG_DEBUG, "Calling FFLInitCharModelGPUStep");
    FFLInitCharModelGPUStep(pCharModel);

    //glActiveTexture(GL_TEXTURE0);

    // Set the stored viewport
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    // Set the stored scissor box
    glScissor(scissorBox[0], scissorBox[1], scissorBox[2], scissorBox[3]);

    // FFLInitCharModelGPUStep changes the current FBO
    // ... so we need to change it back to the main window
    glBindFramebuffer(GL_FRAMEBUFFER, 0);


    return FFL_RESULT_OK;
}

void UpdateCharModelBlink(bool* isBlinking, double* lastBlinkTime, FFLCharModel* pCharModel);

int main(void)
{
    SetTraceLogLevel(LOG_DEBUG);
    // Initialize FFL

    bool isFFLAvailable = InitializeFFL() == FFL_RESULT_OK;
    if (!isFFLAvailable)
        TraceLog(LOG_ERROR, "FFL is not available :(");
    else
    TraceLog(LOG_DEBUG, "FFL initialized");

    // Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 800;
    const int screenHeight = 600;

    InitWindow(screenWidth, screenHeight, "raylib [models] example - draw cube texture");

    FFLCharModel charModel;
    bool isFFLModelCreated;
    if (isFFLAvailable)
    {
        TraceLog(LOG_DEBUG, "Calling ShaderForFFL_Initialize(%p)", &gShaderForFFL);
        ShaderForFFL_Initialize(&gShaderForFFL);

        TraceLog(LOG_DEBUG, "Creating FFLCharModel at %p", &charModel);
        isFFLModelCreated = CreateCharModelFromStoreData(&charModel, (const void*)(&cJasmineStoreData)) == FFL_RESULT_OK;
    }

    // Set up the camera for the 3D cube
    Camera camera = { 0 };
    camera.position = (Vector3){ 2.0f, 4.0f, 12.0f };
    camera.target = (Vector3){ 0.0f, 2.0f, 0.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    // Load default shader for the cube (optional)
    Shader cubeShader = LoadShader(0, 0); // Use default shader

    SetTargetFPS(60);
    //--------------------------------------------------------------------------------------

    // blinking logic
    bool isBlinking = false;
    double lastBlinkTime = GetTime();

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        // Calculate rotation angle
        float rotationAngle;

        // Actually, if FFL is not available, it should be constant
        if (!isFFLAvailable)
            rotationAngle = 1.0f;
        else
            rotationAngle = GetTime() * 45.0f; // 45 degrees per second
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();
            ClearBackground(SKYBLUE);

            BeginMode3D(camera);
                // Apply rotation to the cube

                rlRotatef(rotationAngle, 0.0f, 1.0f, 0.0f); // Rotate around Y axis
                DrawCubeV((Vector3){ 0.0f, 0.0f, 0.0f }, (Vector3){ 2.0f, 2.0f, 2.0f }, RED);
                DrawCubeWiresV((Vector3){ 0.0f, 0.0f, 0.0f }, (Vector3){ 2.0f, 2.0f, 2.0f }, MAROON);

                // Draw custom OpenGL object after Raylib's 3D drawing
                rlDrawRenderBatchActive();      // Flush Raylib's internal buffers

                if (isFFLModelCreated) // draw only if it is valid
                {
                    // FFL model scale is 10.0
                    Matrix matModel = MatrixScale(0.1, 0.1, 0.1);
                    rlPushMatrix();
                    Matrix matView = rlGetMatrixModelview();
                    Matrix matProjection = rlGetMatrixProjection();
                    rlPopMatrix();  // Restore the previous matrix

                    UpdateCharModelBlink(&isBlinking, &lastBlinkTime, &charModel);

                    ShaderForFFL_Bind(&gShaderForFFL);
                    ShaderForFFL_SetViewUniform(&gShaderForFFL,
                                                &matModel, &matView, &matProjection);
                    FFLDrawOpa(&charModel);
                    FFLDrawXlu(&charModel);
                }

            EndMode3D();

            // Display FPS100ms
            DrawFPS(10, 10);
        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------

    UnloadShader(cubeShader);   // Unload default shader
    UnloadShader(gShaderForFFL.shader); // Unload shader for FFL

    TraceLog(LOG_DEBUG, "FFLDeleteCharModel(%p)", &charModel);
    FFLDeleteCharModel(&charModel);
    // FFLCharModel destruction must happen before FFLExit, and before GL context is closed

    CloseWindow();              // Close window and OpenGL context
    //--------------------------------------------------------------------------------------
    ExitFFL();

    return 0;
}


// update model's blink expression

const float cBlinkInterval = 8.0; // 8 secs
const float cBlinkDuration = 0.08; // 80ms

void UpdateCharModelBlink(bool* isBlinking, double* lastBlinkTime, FFLCharModel* pCharModel)
{
    double now = GetTime();
    double timeSinceLastBlink = now - *lastBlinkTime;

    // Check if it's time to blink (every 3 seconds = 3000 ms)
    if (!*isBlinking && timeSinceLastBlink >= cBlinkInterval) {
        FFLSetExpression(pCharModel, FFL_EXPRESSION_BLINK);
        *isBlinking = true;
        TraceLog(LOG_TRACE, "expression: FFL_EXPRESSION_BLINK");
        *lastBlinkTime = now;  // Reset the blink time
    }

    // Check if the blink should stop after 100ms
    if (*isBlinking && (GetTime() - *lastBlinkTime) >= cBlinkDuration) {
        FFLSetExpression(pCharModel, FFL_EXPRESSION_NORMAL);
        TraceLog(LOG_TRACE, "expression: FFL_EXPRESSION_NORMAL");
        *isBlinking = false;
    }
}

void ExitFFL()
{
    TraceLog(LOG_DEBUG, "Calling FFLExit");
    FFLExit();

    if (gResourceDesc.size[FFL_RESOURCE_TYPE_HIGH] > 0)
        free(gResourceDesc.pData[FFL_RESOURCE_TYPE_HIGH]);
    if (gResourceDesc.size[FFL_RESOURCE_TYPE_MIDDLE] > 0)
        free(gResourceDesc.pData[FFL_RESOURCE_TYPE_MIDDLE]);
}