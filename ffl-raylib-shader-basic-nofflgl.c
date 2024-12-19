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

#include <nn/ffl.h>

#include <nn/ffl/FFLTextureCallback.h>

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
void ShaderForFFL_DrawCallback(void* pObj, const FFLDrawParam* drawParam);

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
    self->callback.pDrawFunc = ShaderForFFL_DrawCallback;
    // NOTE: ffl will never call either of these...
    // ... now that it does not use FFLInitCharModelGPUStep
    //self->callback.pApplyAlphaTestFunc = ShaderForFFL_ApplyAlphaTestCallback;
    //self->callback.pSetMatrixFunc = ShaderForFFL_SetMatrixCallback;

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

RenderTexture gFacelineRenderTexture;
RenderTexture gMaskRenderTextures[FFL_EXPRESSION_LIMIT]; // a render texture for each mask
RenderTexture* gMaskRenderTextureCurrent = &gMaskRenderTextures[FFL_EXPRESSION_NORMAL];
                                       // NOTE: default expression is not always normal

void SetCharModelExpression(FFLCharModel* pCharModel, FFLExpression expression)
{
    assert(expression < FFL_EXPRESSION_LIMIT);
    //assert(&gMaskRenderTextures[expression] != NULL); // but they are not initialized to null so nvm

    FFLSetExpression(pCharModel, expression);
    // NOTE: assuming only one instance
    gMaskRenderTextureCurrent = &gMaskRenderTextures[expression];
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
    if (pDrawParam->modulateParam.pTexture2D != NULL
        || pDrawParam->modulateParam.type == FFL_MODULATE_TYPE_SHAPE_FACELINE
        || pDrawParam->modulateParam.type == FFL_MODULATE_TYPE_SHAPE_MASK
    )
    {
        GLuint textureHandle;

        // For faceline and mask (FFL should always bind a texture2D to this...)
        // we will instead use the textures we made ourself
        if (pDrawParam->modulateParam.type == FFL_MODULATE_TYPE_SHAPE_FACELINE)
        {
            textureHandle = gFacelineRenderTexture.texture.id;
        }
        else if (pDrawParam->modulateParam.type == FFL_MODULATE_TYPE_SHAPE_MASK)
        {
            textureHandle = gMaskRenderTextureCurrent->texture.id;
        }
        else {
            assert(pDrawParam->modulateParam.pTexture2D != NULL);
            // assuming that the pTexture2D is really not null
#ifndef FFL_USE_TEXTURE_CALLBACK
            // Get the texture handle from Texture2D
            textureHandle = FFL_GET_RIO_NATIVE_TEXTURE_HANDLE(pDrawParam->modulateParam.pTexture2D);

#else
            textureHandle = (GLuint)pDrawParam->modulateParam.pTexture2D;
#endif
            TraceLog(LOG_TRACE, "Binding texture: %d", textureHandle);
        }

        // Bind the texture to texture unit 0
        glActiveTexture(GL_TEXTURE0);

        glBindTexture(GL_TEXTURE_2D, textureHandle);

        // Set texture wrap to repeat
        if (pDrawParam->modulateParam.type < FFL_MODULATE_TYPE_SHAPE_MAX)
        {
            // Only apply texture wrap to shapes (glass, faceline)
            // since those are NPOT textures
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
        } else {
            // Otherwise do not use repeat wrap
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }

        // Do not use mipmaps
        // TODO: ADD MIPMAP SUPPORT but NOT SUPPORTED FOR ES 2.0
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

        // Set the sampler uniform to use texture unit 0
        glUniform1i(self->samplerLocation, 0);
    } else {
        // If there is no texture, bind nothing
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);
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

const char* cFFLResourceHighFilename = "./FFLResHigh.dat";
//const char* cFFLResourceHighFilename = "/home/arian/Downloads/ffl/tools/AFLResHigh_2_3_LE.dat";

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

const unsigned char cLaneStoreData[96] = {
    0x03, 0x01, 0x00, 0x30, 0x80, 0x21, 0x58, 0x64, 0x80, 0x44, 0x00, 0xa0, 0x91, 0xcd, 0xf6, 0x74, 0xe0, 0x0c, 0x7f, 0xe4, 0x7c, 0x69, 0x00, 0x00, 0x58, 0x58, 0x4c, 0x00, 0x61, 0x00, 0x6e, 0x00, 0x65, 0x00, 0x00, 0x00, 0x74, 0x00, 0x00, 0x00, 0x61, 0x00, 0x6e, 0x00, 0x65, 0x00, 0x7f, 0x2e, 0x08, 0x00, 0x33, 0x06, 0xa5, 0x28, 0x43, 0x12, 0xe1, 0x23, 0x84, 0x0e, 0x61, 0x10, 0x15, 0x86, 0x0d, 0x00, 0x20, 0x41, 0x00, 0x52, 0x10, 0x1d, 0x4c, 0x00, 0x61, 0x00, 0x6e, 0x00, 0x65, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x91, 0x78
};

// forward decls
void ExitFFL();


#include <nn/ffl/FFLiCharModel.h> // need pObject, charModelDesc,
#include <nn/ffl/FFLiFacelineTexture.h> // FFLiInvalidateTempObjectFacelineTexture, FFLiDrawFacelineTexture

#include <nn/ffl/FFLiMaskTextures.h> // FFLiDeleteTempObjectMaskTextures
// FFLiInvalidatePartsTextures
#include <nn/ffl/FFLiRawMask.h> // FFLiInvalidateRawMask, FFLiDrawRawMask

// calls FFLInitCharModelCPUStep, loading charmodel data,
// , loading shapes, loading textures, uploading textures
FFLResult CreateCharModelFromStoreData(FFLCharModel* pCharModel, const void* pStoreDataBuffer)
{
    FFLCharModelSource modelSource = {
        .dataSource = FFL_DATA_SOURCE_STORE_DATA,
        .pBuffer = pStoreDataBuffer,
        .index = 0,
    };

    const u32 expressionFlag =
            (1 << FFL_EXPRESSION_NORMAL
            | 1 << FFL_EXPRESSION_BLINK);

    //const FFLExpressionFlag expressionFlag = 1 << FFL_EXPRESSION_PUZZLED;

    FFLCharModelDesc modelDesc = {
        .resolution = (FFLResolution)512,
        .expressionFlag = expressionFlag,
        .modelFlag = FFL_MODEL_FLAG_NORMAL, //FFL_MODEL_FLAG_FACE_ONLY,
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

    // NOTE: FFLInitCharModelGPUStep was moved out of here into InitCharModelTextures

    return FFL_RESULT_OK;
}

// calls FFLInitCharModelGPUStep or our alternative, both draw faceline and masks
void InitCharModelTextures(FFLCharModel* pCharModel)
{
    TraceLog(LOG_DEBUG, "InitCharModelTextures(%p), drawing faceline and masks...", pCharModel);

    TraceLog(LOG_DEBUG, "Calling ShaderForFFL_Bind");
    ShaderForFFL_Bind(&gShaderForFFL); // for init textures

    /*
    GLint viewport[4];
    GLint scissorBox[4];
    // save viewport and scissor
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetIntegerv(GL_SCISSOR_BOX, scissorBox);

    TraceLog(LOG_DEBUG, "Calling FFLInitCharModelGPUStep");
    FFLInitCharModelGPUStep(pCharModel);

    // load viewport and scissor
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    glScissor(scissorBox[0], scissorBox[1], scissorBox[2], scissorBox[3]);

    // FFLInitCharModelGPUStep changes the current FBO
    // ... so we need to change it back to the main window
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    */

    // FFLInitCharModelGPUStep SUBSTITUTE!!

    TraceLog(LOG_DEBUG, "(Using our alternative for FFLInitCharModelGPUStep)", pCharModel);

    Matrix texturesMatrix = MatrixIdentity();
    //texturesMatrix.m5 *= -1.f; // NOTE: ASSUMING DEFAULT OPENGL CLIP CONTROL
    ShaderForFFL_SetViewUniform(&gShaderForFFL, NULL, NULL, &texturesMatrix);

    FFLiCharModel* piCharModel = (FFLiCharModel*)pCharModel;

    FFLShaderCallback* pCallback = &gShaderForFFL.callback;
    FFLShaderCallback** ppCallback = &pCallback;

    FFLResolution textureResolution = piCharModel->charModelDesc.resolution & FFL_RESOLUTION_MASK;
    TraceLog(LOG_DEBUG, "Faceline/mask texture resolution: %d", textureResolution);
    // apply linear filtering to mask and faceline textures
    const int renderTextureFilter = TEXTURE_FILTER_BILINEAR;

    ShaderForFFL_SetCulling(FFL_CULL_MODE_NONE);

    void** ppFacelineTexture2D = (void*)&piCharModel->facelineRenderTexture; // HACK: FFLiRenderTexture = FFLTexture**
    if (*ppFacelineTexture2D != NULL) // should we draw the faceline texture?
    {
        // assuming there is only one faceline texture ever, which there is
        gFacelineRenderTexture = LoadRenderTexture(textureResolution / 2, textureResolution);
        TraceLog(LOG_DEBUG, "Created render texture for faceline: %p, texture ID %d",
            &gFacelineRenderTexture, gFacelineRenderTexture.texture.id);
        // faceline texture
        BeginTextureMode(gFacelineRenderTexture);
        SetTextureFilter(gFacelineRenderTexture.texture, renderTextureFilter);
        FFLiInvalidateTempObjectFacelineTexture(&piCharModel->pTextureTempObject->facelineTexture); // before drawing...

        FFLColor facelineColor = FFLGetFacelineColor(piCharModel->charInfo.parts.facelineColor);
        TraceLog(LOG_DEBUG, "Faceline color: %f, %f, %f, %f", facelineColor.r, facelineColor.g, facelineColor.b, facelineColor.a);
        // set raylib color from FFLColor
        ClearBackground((Color) {
            (unsigned char)(facelineColor.r * 255.0f),
            (unsigned char)(facelineColor.g * 255.0f),
            (unsigned char)(facelineColor.b * 255.0f),
            (unsigned char)(facelineColor.a * 255.0f),
        });

        //glClearColor(facelineColor.r, facelineColor.g, facelineColor.b, facelineColor.a);

        // faceline blending
        //glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);
        rlSetBlendFactorsSeparate(RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA, RL_ONE, RL_ONE, RL_MIN, RL_MIN);
        rlSetBlendMode(RL_BLEND_CUSTOM_SEPARATE);
        glBlendEquation(GL_FUNC_ADD);

        FFLiDrawFacelineTexture(&piCharModel->pTextureTempObject->facelineTexture, ppCallback);
                                                    // FFLiShaderCallback = **FFLShaderCallback
    }
    else
    {
        TraceLog(LOG_DEBUG, "Skipping rendering faceline texture (*ppFacelineTexture2D == NULL)");
    }


    // begin drrawing mask
    FFLiMaskTexturesTempObject* pObject = &piCharModel->pTextureTempObject->maskTextures;
    FFLiInvalidatePartsTextures(&pObject->partsTextures); // before looping at ALL

    // NOTE: LOOP BEGINS HERE
    u32 lExpressionFlag = piCharModel->charModelDesc.expressionFlag;
    for (u32 i = 0; lExpressionFlag != 0; i++, lExpressionFlag >>= 1)
    {
        if ((lExpressionFlag & 1) == 0)
            //pMaskTextures->pRenderTextures[i] = NULL;
            continue;

        TraceLog(LOG_DEBUG, "Enabled expression: %d", i);

        // create this mask texture
        gMaskRenderTextures[i] = LoadRenderTexture(textureResolution, textureResolution);
        TraceLog(LOG_DEBUG, "Created mask texture for expression %d: %p, texture ID %d",
            i, &gMaskRenderTextures[i], gMaskRenderTextures[i].texture.id);

        SetTextureFilter(gMaskRenderTextures[i].texture, renderTextureFilter);

        // begin rendering to this mask texture
        FFLiInvalidateRawMask(pObject->pRawMaskDrawParam[i]); // after verifying thisis supposed to be drawn but before ANY drawing

        BeginTextureMode(gMaskRenderTextures[i]); // switch to this texture mode
        ClearBackground(BLANK); // rgba 0 0 0 0

        // mask blending
        //glBlendFuncSeparate(GL_ONE_MINUS_DST_ALPHA, GL_DST_ALPHA, GL_SRC_ALPHA, GL_DST_ALPHA);
        rlSetBlendFactorsSeparate(RL_ONE_MINUS_DST_ALPHA, RL_DST_ALPHA, RL_SRC_ALPHA, RL_DST_ALPHA, RL_MIN, RL_MIN);
        rlSetBlendMode(RL_BLEND_CUSTOM_SEPARATE);
        glBlendEquation(GL_FUNC_ADD);

        FFLiDrawRawMask(pObject->pRawMaskDrawParam[i], ppCallback); // submits draw calls to your callback
    }
    // set current expresssion as the mask
    gMaskRenderTextureCurrent = &gMaskRenderTextures[piCharModel->expression];



    // cleanup!!!
    if (*ppFacelineTexture2D != NULL)
        FFLiDeleteTempObjectFacelineTexture(&piCharModel->pTextureTempObject->facelineTexture, &piCharModel->charInfo, piCharModel->charModelDesc.resourceType);
    FFLiDeleteTempObjectMaskTextures(&piCharModel->pTextureTempObject->maskTextures, piCharModel->charModelDesc.allExpressionFlag, piCharModel->charModelDesc.resourceType);
    FFLiDeleteTextureTempObject(piCharModel);

    EndTextureMode();
    // Go back to normal blend mode
    rlSetBlendMode(BLEND_ALPHA);
    /*
    rlSetBlendFactorsSeparate(RL_ONE_MINUS_DST_ALPHA, RL_DST_ALPHA, RL_SRC_ALPHA, RL_DST_ALPHA, RL_MIN, RL_MIN);
    rlSetBlendMode(RL_BLEND_CUSTOM_SEPARATE);
*/
    // FFLInitCharModelGPUStep does not return anything so neither do we
    TraceLog(LOG_DEBUG, "Exiting InitCharModelTextures");
}

void UpdateCharModelBlink(bool* isBlinking, double* lastBlinkTime, FFLCharModel* pCharModel, FFLExpression initialExpression, double now);

void TextureCallback_Create(void* v, const FFLTextureInfo* pTextureInfo, FFLTexture* pTexture)
{
/*
    if (!pTextureInfo || !pTexture)
        return; // Invalid input
*/

    // Log the FFLTextureInfo details
    TraceLog(LOG_DEBUG, "CreateTexture: FFLTextureInfo { width: %d, height: %d, format: %d, size: %d, numMips: %d, imagePtr: %p, mipPtr: %p }",
             pTextureInfo->width, pTextureInfo->height, pTextureInfo->format, pTextureInfo->size,
             pTextureInfo->numMips, pTextureInfo->imagePtr, pTextureInfo->mipPtr);

    // Allocate memory for the OpenGL texture handle
    /*
    GLuint* textureHandle = (GLuint*)malloc(sizeof(GLuint));
    if (!textureHandle)
        return; // Memory allocation failed
    */
    GLuint textureHandle = 0;

    // Generate a texture
    glGenTextures(1, &textureHandle);
    glBindTexture(GL_TEXTURE_2D, textureHandle);

    // Configure texture parameters (wrap and filter modes)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Determine OpenGL format based on FFLTextureInfo format
    GLenum internalFormat, format, type;
    switch (pTextureInfo->format)
    {
        case FFL_TEXTURE_FORMAT_R8_UNORM:
#ifdef VAO_NOT_SUPPORTED // GL ES 2.0 compatible types
            internalFormat = GL_LUMINANCE;
            format = GL_LUMINANCE;
#else
            internalFormat = GL_R8;
            format = GL_RED;
#endif // VAO_NOT_SUPPORTED
            type = GL_UNSIGNED_BYTE;
            break;
        case FFL_TEXTURE_FORMAT_R8_G8_UNORM:
#ifdef VAO_NOT_SUPPORTED // GL ES 2.0 compatible types
            internalFormat = GL_LUMINANCE_ALPHA;
            format = GL_LUMINANCE_ALPHA;
#else
            internalFormat = GL_RG8;
            format = GL_RG;
#endif // VAO_NOT_SUPPORTED
            type = GL_UNSIGNED_BYTE;
            break;
        case FFL_TEXTURE_FORMAT_R8_G8_B8_A8_UNORM:
            internalFormat = GL_RGBA;
            format = GL_RGBA;
            type = GL_UNSIGNED_BYTE;
            break;
        default:
            // Unsupported format
            glBindTexture(GL_TEXTURE_2D, 0);
            glDeleteTextures(1, &textureHandle);
            //free(textureHandle);
            return;
    }

    // Upload texture data
    glTexImage2D(
        GL_TEXTURE_2D,
        0, // Level 0 (base level)
        internalFormat,
        pTextureInfo->width,
        pTextureInfo->height,
        0, // Border (must be 0)
        format,
        type,
        pTextureInfo->imagePtr
    );
/*
    // Set up mipmaps if provided
    if (pTextureInfo->numMips > 1 && pTextureInfo->mipPtr)
    {
        const unsigned char* mipData = (const unsigned char*)pTextureInfo->mipPtr;
        for (u32 mipLevel = 1; mipLevel < pTextureInfo->numMips; ++mipLevel)
        {
            u32 mipWidth = pTextureInfo->width >> mipLevel;
            u32 mipHeight = pTextureInfo->height >> mipLevel;
            if (mipWidth == 0 || mipHeight == 0)
                break;

            glTexImage2D(
                GL_TEXTURE_2D,
                mipLevel,
                internalFormat,
                mipWidth,
                mipHeight,
                0,
                format,
                type,
                mipData
            );

            // Calculate mipData offset (this assumes tightly packed mip levels)
            mipData += mipWidth * mipHeight * (type == GL_UNSIGNED_BYTE ? 1 : 4);
        }
    }
*/
    // Unbind texture and return the handle
    glBindTexture(GL_TEXTURE_2D, 0);
    *(void**)pTexture = (void*)textureHandle;

    // Log the created texture handle
    TraceLog(LOG_DEBUG, "CreateTexture: Generated texture handle: %u", textureHandle);
}

void TextureCallback_Delete(void* v, FFLTexture* pTexture)
{
/*
    if (!pTexture || !*pTexture)
        return; // Invalid input
*/
    GLuint textureHandle = (GLuint)(*(void**)pTexture);

    // Log the handle being deleted
    TraceLog(LOG_DEBUG, "DeleteTexture: Deleting texture handle: %u", textureHandle);

    // Delete the OpenGL texture
    glDeleteTextures(1, &textureHandle);

    // Free the allocated memory for the texture handle
    //free(textureHandle);

    // Set the texture handle to null
    *(void**)pTexture = (void*)NULL;
}


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


    TraceLog(LOG_DEBUG, "Calling ShaderForFFL_Initialize(%p)", &gShaderForFFL);
    ShaderForFFL_Initialize(&gShaderForFFL);

#ifdef FFL_USE_TEXTURE_CALLBACK
    FFLTextureCallback textureCallback = {
        .useOriginalTileMode = false,
        .pCreateFunc = TextureCallback_Create,
        .pDeleteFunc = TextureCallback_Delete
    };
    FFLSetTextureCallback(&textureCallback);
#endif // FFL_USE_TEXTURE_CALLBACK

    // custom FFL function that flips Y for mask/faceline (ASSUMES default gl clip control...)
    FFLSetTextureFlipY(true);

    FFLCharModel charModel;
    bool isFFLModelCreated = false;
    if (isFFLAvailable)
    {
        TraceLog(LOG_DEBUG, "Creating FFLCharModel at %p", &charModel);
        isFFLModelCreated = CreateCharModelFromStoreData(&charModel, (const void*)(&cJasmineStoreData)) == FFL_RESULT_OK;
        if (isFFLModelCreated)
            InitCharModelTextures(&charModel); // does drawing
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
    FFLExpression initialExpression = FFLGetExpression(&charModel);

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        // Calculate rotation angle
        float rotationAngle;
        double now = GetTime();

        // Actually, if FFL is not available, it should be constant
        if (!isFFLAvailable)
            rotationAngle = 1.0f;
        else
            rotationAngle = now * 45.0f; // 45 degrees per second
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

                    UpdateCharModelBlink(&isBlinking, &lastBlinkTime, &charModel, initialExpression, now);

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

    if (isFFLModelCreated)
    {
        TraceLog(LOG_DEBUG, "FFLDeleteCharModel(%p)", &charModel);
        FFLDeleteCharModel(&charModel);
        // FFLCharModel destruction must happen before FFLExit, and before GL context is closed
    }

    // assuming that raylib checks if it is loaded or not
    UnloadRenderTexture(gFacelineRenderTexture);
    for (int i = 0; i < FFL_EXPRESSION_LIMIT; i++)
        UnloadRenderTexture(gMaskRenderTextures[i]);

    CloseWindow();              // Close window and OpenGL context
    //--------------------------------------------------------------------------------------
    ExitFFL();

    return 0;
}


// update model's blink expression

const float cBlinkInterval = 8.0f; // 8 secs
const float cBlinkDuration = 0.08f; // 80ms

void UpdateCharModelBlink(bool* isBlinking, double* lastBlinkTime, FFLCharModel* pCharModel, FFLExpression initialExpression, double now)
{
    //double now = GetTime();
    double timeSinceLastBlink = now - *lastBlinkTime;

    // Check if it's time to blink (every 3 seconds = 3000 ms)
    if (!*isBlinking && timeSinceLastBlink >= cBlinkInterval) {
        //FFLSetExpression(pCharModel, FFL_EXPRESSION_BLINK);
        SetCharModelExpression(pCharModel, FFL_EXPRESSION_BLINK);
        *isBlinking = true;
        TraceLog(LOG_TRACE, "expression: %d", FFL_EXPRESSION_BLINK);
        *lastBlinkTime = now;  // Reset the blink time
    }

    // Check if the blink should stop after 100ms
    if (*isBlinking && (now - *lastBlinkTime) >= cBlinkDuration) {
        SetCharModelExpression(pCharModel, initialExpression); // back to previous
        TraceLog(LOG_TRACE, "expression: %d", initialExpression);
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
