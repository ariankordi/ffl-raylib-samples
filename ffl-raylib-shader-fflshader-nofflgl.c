#if defined(_WIN32)
    #define NOGDI             // All GDI defines and routines
    #define NOUSER            // All USER defines and routines
#endif

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
    #define VAO_NOT_SUPPORTED
    #define GLSL_VERSION            100 // assume always gles
#endif

/*
#if defined(_WIN32)           // raylib uses these names as function parameters
    #undef near
    #undef far
#endif
*/

#include "raylib.h"

#include "raymath.h" // Required for: MatrixMultiply(), MatrixToFloat()
#include "rlgl.h" // Required for: rlDrawRenderBatchActive(), rlGetMatrixModelview(), rlGetMatrixProjection()

#include <nn/ffl.h>

#include <nn/ffl/detail/FFLiCharInfo.h> // optional, should work in C

// NOTE: you need to define this when building FFL
// if you are dynamically linking, then call FFL's gladLoadGL too
#ifdef FFL_ADD_GLAD_GL_IMPLEMENTATION
#include "external/glfw/include/GLFW/glfw3.h" // glfwGetProcAddress
int FFLGladLoadGL(GLADloadfunc load); // also gladLoadGLES2
#else
#define FFLGladLoadGL(x)
#endif // FFL_ADD_GLAD_GL_IMPLEMENTATION

#include <stdio.h>
#include <stdlib.h>

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#define GLSL_VERT(src) "#version " STR(GLSL_VERSION) "\n" \
    "#ifndef GL_ES\n" \
        "#define attribute in\n" \
        "#define varying out\n" \
    "#else\n" \
        "precision highp float;\n" \
        "// define inverse and transpose functions for gles 2.0\n" \
        "highp mat3 inverse(highp mat3 m) {\n\thighp float c01 = m[2].z * m[1].y - m[1].z * m[2].y;\n\thighp float c11 = -m[2].z * m[1].x + m[1].z * m[2].x;\n\thighp float c21 = m[2].y * m[1].x - m[1].y * m[2].x;\n\thighp float d = 1.0 / (m[0].x * c01 + m[0].y * c11 + m[0].z * c21);\n\n\treturn mat3(c01, (-m[2].z * m[0].y + m[0].z * m[2].y), (m[1].z * m[0].y - m[0].z * m[1].y),\n\t\t\t\t   c11, (m[2].z * m[0].x - m[0].z * m[2].x), (-m[1].z * m[0].x + m[0].z * m[1].x),\n\t\t\t\t   c21, (-m[2].y * m[0].x + m[0].y * m[2].x), (m[1].y * m[0].x - m[0].y * m[1].x)) *\n\t\t\td;\n}\n\nhighp mat3 transpose(highp mat3 m) {\n\treturn mat3(\n\t\t\tvec3(m[0].x, m[1].x, m[2].x),\n\t\t\tvec3(m[0].y, m[1].y, m[2].y),\n\t\t\tvec3(m[0].z, m[1].z, m[2].z));\n}\n" \
    "#endif\n" \
#src
#define GLSL_FRAG(src) "#version " STR(GLSL_VERSION) "\n" \
    "#ifndef GL_ES\n" \
        "#define varying in\n" \
        "#define gl_FragColor FragColor\n" \
        "#define texture2D texture\n" \
        "out vec4 FragColor;\n" \
    "#else\n" \
        "precision mediump float;\n" \
    "#endif\n" \
#src

const char* vertexShaderCodeFFL = GLSL_VERT(
    attribute vec4 a_position;
    attribute vec2 a_texCoord;
    attribute vec3 a_normal;
    attribute vec4 a_color;
    attribute vec3 a_tangent;

    attribute vec4 vertexBoneIds;
    attribute vec4 vertexBoneWeights;

    varying   vec4 v_color;
    varying   vec4 v_position;
    varying   vec3 v_normal;
    varying   vec3 v_tangent;
    varying   vec2 v_texCoord;

    uniform   mat4 u_model; //u_mv;
    uniform   mat4 u_view;
    uniform   mat4 u_proj;
    //uniform   mat4 u_it;

    uniform mat4 boneMatrices[80];
    uniform int skinningEnabled;
    /*
        void main()
        {
            vec4 skinnedPosition;
            vec3 skinnedNormal;
            vec3 skinnedTangent;
            if (skinningEnabled == 1)
            {
                skinnedPosition = vec4(0.0);
                skinnedNormal = vec3(0.0);
                skinnedTangent = vec3(0.0);
                // Skinning: Apply bone transformations to position, normal, and tangent
                for (int i = 0; i < 4; i++)
                {
                    int boneId = int(vertexBoneIds[i]);
                    float weight = vertexBoneWeights[i];

                    mat4 boneMatrix = boneMatrices[boneId];
                    mat3 boneMatrix3 = mat3(boneMatrix); // Extract rotation part

                    skinnedPosition += weight * boneMatrix * a_position;
                    skinnedNormal += weight * boneMatrix3 * a_normal;
                    skinnedTangent += weight * boneMatrix3 * a_tangent;
                }
            }
            else
            {
                skinnedPosition = a_position;
                skinnedNormal.xyz = a_normal;
                skinnedTangent.xyz = a_tangent;
            }

            mat4 mv = u_view * u_model;
            gl_Position = u_proj * mv * skinnedPosition;
            v_position = mv * skinnedPosition;

            //mat3 normalMatrix = mat3(u_it);
            mat3 normalMatrix = transpose(inverse(mat3(mv)));

            //v_normal = vec3(0.0, 1.0, 0.0); // for test
            v_normal = normalize(normalMatrix * skinnedNormal.xyz);
            v_texCoord = a_texCoord;
            v_tangent = normalize(normalMatrix * skinnedTangent.xyz);
            v_color = a_color;
        }
    */
    void main()
    {
        vec4 position;
        vec3 normal;
        vec3 tangent;

        if (skinningEnabled == 1)
        {
            // Transform position
            position = vec4(0.0);
            position += vertexBoneWeights[0] * boneMatrices[int(vertexBoneIds[0])] * a_position;
            position += vertexBoneWeights[1] * boneMatrices[int(vertexBoneIds[1])] * a_position;
            position += vertexBoneWeights[2] * boneMatrices[int(vertexBoneIds[2])] * a_position;
            position += vertexBoneWeights[3] * boneMatrices[int(vertexBoneIds[3])] * a_position;

            // Transform normal

            vec3 skinnedNormal = vec3(0.0);
            mat3 normalMatrix0 = transpose(inverse(mat3(boneMatrices[int(vertexBoneIds[0])])));
            mat3 normalMatrix1 = transpose(inverse(mat3(boneMatrices[int(vertexBoneIds[1])])));
            mat3 normalMatrix2 = transpose(inverse(mat3(boneMatrices[int(vertexBoneIds[2])])));
            mat3 normalMatrix3 = transpose(inverse(mat3(boneMatrices[int(vertexBoneIds[3])])));

            skinnedNormal += vertexBoneWeights[0] * (normalMatrix0 * a_normal);
            skinnedNormal += vertexBoneWeights[1] * (normalMatrix1 * a_normal);
            skinnedNormal += vertexBoneWeights[2] * (normalMatrix2 * a_normal);
            skinnedNormal += vertexBoneWeights[3] * (normalMatrix3 * a_normal);
            normal = normalize(skinnedNormal);
            /*
            // Transform tangent (if using normal mapping)
            vec3 skinnedTangent = vec3(0.0);
            skinnedTangent += vertexBoneWeights[0] * (normalMatrix0 * a_tangent);
            skinnedTangent += vertexBoneWeights[1] * (normalMatrix1 * a_tangent);
            skinnedTangent += vertexBoneWeights[2] * (normalMatrix2 * a_tangent);
            skinnedTangent += vertexBoneWeights[3] * (normalMatrix3 * a_tangent);
            tangent = normalize(skinnedTangent);
            */
            tangent = a_tangent;

            //normal = a_normal;
        }
        else
        {
            position = a_position;
            normal = a_normal;
            tangent = a_tangent;
        }

        // Apply model-view and projection transformations
        mat4 mv = u_view * u_model;
        v_position = mv * position;
        gl_Position = u_proj * v_position;

        // Compute normal matrix for non-skinned vertices
        //if (skinningEnabled == 0)
        //{
            mat3 normalMatrix = transpose(inverse(mat3(mv)));
            normal = normalize(normalMatrix * normal);
            tangent = normalize(normalMatrix * tangent);
        //}

        v_normal = normal;
        v_tangent = tangent;
        v_texCoord = a_texCoord;
        v_color = a_color;
    }
);

const char* fragmentShaderCodeFFL = GLSL_FRAG(
    const int MODULATE_MODE_CONSTANT        = 0;
    const int MODULATE_MODE_TEXTURE_DIRECT  = 1;
    const int MODULATE_MODE_RGB_LAYERED     = 2;
    const int MODULATE_MODE_ALPHA           = 3;
    const int MODULATE_MODE_LUMINANCE_ALPHA = 4;
    const int MODULATE_MODE_ALPHA_OPA       = 5;

    mediump float calculateAnisotropicSpecular(mediump vec3 light, mediump vec3 tangent, mediump vec3 eye, mediump float power)
    {
        mediump float dotLT = dot(light, tangent);
        mediump float dotVT = dot(eye, tangent);
        mediump float dotLN = sqrt(1.0 - dotLT * dotLT);
        mediump float dotVR = dotLN * sqrt(1.0 - dotVT * dotVT) - dotLT * dotVT;
        return pow(max(0.0, dotVR), power);
    }

    mediump float calculateBlinnSpecular(mediump vec3 light, mediump vec3 normal, mediump vec3 eye, mediump float power)
    {
        return pow(max(dot(reflect(-light, normal), eye), 0.0), power);
    }

    mediump float calculateSpecularBlend(mediump float blend, mediump float blinn, mediump float aniso)
    {
        return mix(aniso, blinn, blend);
    }

    mediump vec3 calculateAmbientColor(mediump vec3 light, mediump vec3 material)
    {
        return light * material;
    }

    mediump vec3 calculateDiffuseColor(mediump vec3 light, mediump vec3 material, mediump float ln)
    {
        return light * material * ln;
    }

    mediump vec3 calculateSpecularColor(mediump vec3 light, mediump vec3 material, mediump float reflection, mediump float strength)
    {
        return light * material * reflection * strength;
    }

    mediump vec3 calculateRimColor(mediump vec3 color, mediump float normalZ, mediump float width, mediump float power)
    {
        return color * pow(width * (1.0 - abs(normalZ)), power);
    }

    mediump float calculateDot(mediump vec3 light, mediump vec3 normal)
    {
        return max(dot(light, normal), 0.1);
    }

    varying mediump vec4 v_color;
    varying mediump vec4 v_position;
    varying mediump vec3 v_normal;
    varying mediump vec3 v_tangent;
    varying mediump vec2 v_texCoord;

    uniform mediump vec3  u_const1;
    uniform mediump vec3  u_const2;
    uniform mediump vec3  u_const3;

    uniform mediump vec3 u_light_ambient;
    uniform mediump vec3 u_light_diffuse;
    uniform mediump vec3 u_light_dir;
    uniform bool u_light_enable;
    uniform mediump vec3 u_light_specular;

    uniform mediump vec3 u_material_ambient;
    uniform mediump vec3 u_material_diffuse;
    uniform mediump vec3 u_material_specular;
    uniform int u_material_specular_mode;
    uniform mediump float u_material_specular_power;

    uniform int u_mode;

    uniform mediump vec3  u_rim_color;
    uniform mediump float u_rim_power;

    uniform sampler2D s_texture;

    void main()
    {
        mediump vec4 color;
        mediump float specularPower = u_material_specular_power;
        mediump float rimWidth = v_color.a;

        if(u_mode == MODULATE_MODE_CONSTANT)
        {
            color = vec4(u_const1, 1.0);
        }
        else if(u_mode == MODULATE_MODE_TEXTURE_DIRECT)
        {
            color = texture2D(s_texture, v_texCoord);
        }
        else if(u_mode == MODULATE_MODE_RGB_LAYERED)
        {
            color = texture2D(s_texture, v_texCoord);
            color = vec4(color.r * u_const1.rgb + color.g * u_const2.rgb + color.b * u_const3.rgb, color.a);
        }
        else if(u_mode == MODULATE_MODE_ALPHA)
        {
            color = texture2D(s_texture, v_texCoord);
            color = vec4(u_const1.rgb, color.r);
        }
        else if(u_mode == MODULATE_MODE_LUMINANCE_ALPHA)
        {
            color = texture2D(s_texture, v_texCoord);
            color = vec4(color.g * u_const1.rgb, color.r);
        }
        else if(u_mode == MODULATE_MODE_ALPHA_OPA)
        {
            color = texture2D(s_texture, v_texCoord);
            color = vec4(color.r * u_const1.rgb, 1.0);
        }

        if(u_mode != MODULATE_MODE_CONSTANT && color.a == 0.0)
        {
            discard;
        }

        if(u_light_enable)
        {
            mediump vec3 ambient = calculateAmbientColor(u_light_ambient.xyz, u_material_ambient.xyz);
            mediump vec3 norm = normalize(v_normal);
            mediump vec3 eye = normalize(-v_position.xyz);
            mediump float fDot = calculateDot(u_light_dir, norm);
            mediump vec3 diffuse = calculateDiffuseColor(u_light_diffuse.xyz, u_material_diffuse.xyz, fDot);
            mediump float specularBlinn = calculateBlinnSpecular(u_light_dir, norm, eye, u_material_specular_power);

            mediump float reflection;
            mediump float strength = v_color.g;
            if(u_material_specular_mode == 0) // blinn
            {
                strength = 1.0;
                reflection = specularBlinn;
            }
            else
            {
                mediump float specularAniso = calculateAnisotropicSpecular(u_light_dir, v_tangent, eye, u_material_specular_power);
                reflection = calculateSpecularBlend(v_color.r, specularBlinn, specularAniso);
            }
            mediump vec3 specular = calculateSpecularColor(u_light_specular.xyz, u_material_specular.xyz, reflection, strength);
            mediump vec3 rimColor = calculateRimColor(u_rim_color.rgb, norm.z, rimWidth, u_rim_power);
            color.rgb = (ambient + diffuse) * color.rgb + specular + rimColor;
        }

        gl_FragColor = color;
    }
);


// Material tables for FFL shader
typedef struct FFLiDefaultShaderMaterial
{
    Vector3 ambient;
    Vector3 diffuse;
    Vector3 specular;
    float specularPower;
    int specularMode;
} FFLiDefaultShaderMaterial;

// NOTE: some bug makes lighting at a certain angle
// on textures (mask, glass, noseline) completely black
// for certain WebGL implementations? but using blinn fixes it
#if GLSL_VERSION == 330
    #define SPECULAR_MODE_TEXTURE_MASKS 1
#else
    #define SPECULAR_MODE_TEXTURE_MASKS 0
#endif

#define MATERIAL_PARAM_SIZE FFL_MODULATE_TYPE_SHAPE_MAX + 2

#define MATERIAL_PARAM_BODY FFL_MODULATE_TYPE_SHAPE_MAX
#define MATERIAL_PARAM_PANTS FFL_MODULATE_TYPE_SHAPE_MAX + 1
FFLiDefaultShaderMaterial cMaterialParam[MATERIAL_PARAM_SIZE] = {
    { // ShapeFaceline
        { 0.85f, 0.75f, 0.75f }, // ambient
        { 0.75f, 0.75f, 0.75f }, // diffuse
        { 0.30f, 0.30f, 0.30f }, // specular
        1.2f, // specularPower
        0 // specularMode
    },
    { // ShapeBeard
        { 1.0f, 1.0f, 1.0f }, // ambient
        { 0.7f, 0.7f, 0.7f }, // diffuse
        { 0.0f, 0.0f, 0.0f }, // specular
        40.0f, // specularPower
        1 // specularMode
    },
    { // ShapeNose
        { 0.90f, 0.85f, 0.85f }, // ambient
        { 0.75f, 0.75f, 0.75f }, // diffuse
        { 0.22f, 0.22f, 0.22f }, // specular
        1.5f, // specularPower
        0 // specularMode
    },
    { // ShapeForehead
        { 0.85f, 0.75f, 0.75f }, // ambient
        { 0.75f, 0.75f, 0.75f }, // diffuse
        { 0.30f, 0.30f, 0.30f }, // specular
        1.2f, // specularPower
        0 // specularMode
    },
    { // ShapeHair
        { 1.00f, 1.00f, 1.00f }, // ambient
        { 0.70f, 0.70f, 0.70f }, // diffuse
        { 0.35f, 0.35f, 0.35f }, // specular
        10.0f, // specularPower
        1 // specularMode
    },
    { // ShapeCap
        { 0.75f, 0.75f, 0.75f }, // ambient
        { 0.72f, 0.72f, 0.72f }, // diffuse
        { 0.30f, 0.30f, 0.30f }, // specular
        1.5f, // specularPower
        0 // specularMode
    },
    { // ShapeMask
        { 1.0f, 1.0f, 1.0f }, // ambient
        { 0.7f, 0.7f, 0.7f }, // diffuse
        { 0.0f, 0.0f, 0.0f }, // specular
        40.0f, // specularPower
        SPECULAR_MODE_TEXTURE_MASKS // specularMode
    },
    { // ShapeNoseline
        { 1.0f, 1.0f, 1.0f }, // ambient
        { 0.7f, 0.7f, 0.7f }, // diffuse
        { 0.0f, 0.0f, 0.0f }, // specular
        40.0f, // specularPower
        SPECULAR_MODE_TEXTURE_MASKS // specularMode
    },
    { // ShapeGlass
        { 1.0f, 1.0f, 1.0f }, // ambient
        { 0.7f, 0.7f, 0.7f }, // diffuse
        { 0.0f, 0.0f, 0.0f }, // specular
        40.0f, // specularPower
        SPECULAR_MODE_TEXTURE_MASKS // specularMode
    },
    // HACK: THESE COLLIDE!!!
    // but it's with textures which have no lighting
    { // body
        { 0.95622f, 0.95622f, 0.95622f }, // 0.69804
        { 0.496733f, 0.496733f, 0.496733f }, // 0.29804
        { 0.2409f, 0.2409f, 0.2409f }, // 0.16863
        3.0f, // specularPower
        0 // specularMode
    },
    { // pants
        { 0.95622f, 0.95622f, 0.95622f }, // 0.69804
        { 1.084967f, 1.084967f, 1.084967f }, // 0.65098
        { 0.2409f, 0.2409f, 0.2409f }, // 0.16863
        3.0f, // specularPower
        0 // specularMode
    }
};

const Vector3 cLightAmbient  = { 0.73f, 0.73f, 0.73f };
const Vector3 cLightDiffuse  = { 0.60f, 0.60f, 0.60f };
const Vector3 cLightSpecular = { 0.70f, 0.70f, 0.70f };

const Vector3 cLightDir = { -0.4531539381f, 0.4226179123f, 0.7848858833f };

const Vector3 cRimColor = { 0.3f, 0.3f, 0.3f };
const Vector3 cRimColorBody = { 0.4f, 0.4f, 0.4f };

const float cRimPower = 2.0f;

#ifndef SHADER_FFL_SPECULAR_MODE
    #define SHADER_FFL_SPECULAR_MODE 1 // aniso
#endif

// Shader uniform enums for shader for FFL
enum ShaderFFLVertexUniform
{
    SH_FFL_VERTEX_UNIFORM_MV = 0,
    SH_FFL_VERTEX_UNIFORM_PROJ,
    //SH_FFL_VERTEX_UNIFORM_IT,
    SH_FFL_VERTEX_UNIFORM_MAX
};
enum ShaderFFLPixelUniform
{
    SH_FFL_PIXEL_UNIFORM_CONST1 = 0,
    SH_FFL_PIXEL_UNIFORM_CONST2,
    SH_FFL_PIXEL_UNIFORM_CONST3,
    SH_FFL_PIXEL_UNIFORM_LIGHT_AMBIENT,
    SH_FFL_PIXEL_UNIFORM_LIGHT_DIFFUSE,
    SH_FFL_PIXEL_UNIFORM_LIGHT_DIR,
    SH_FFL_PIXEL_UNIFORM_LIGHT_ENABLE,
    SH_FFL_PIXEL_UNIFORM_LIGHT_SPECULAR,
    SH_FFL_PIXEL_UNIFORM_MATERIAL_AMBIENT,
    SH_FFL_PIXEL_UNIFORM_MATERIAL_DIFFUSE,
    SH_FFL_PIXEL_UNIFORM_MATERIAL_SPECULAR,
    SH_FFL_PIXEL_UNIFORM_MATERIAL_SPECULAR_MODE,
    SH_FFL_PIXEL_UNIFORM_MATERIAL_SPECULAR_POWER,
    SH_FFL_PIXEL_UNIFORM_MODE,
    SH_FFL_PIXEL_UNIFORM_RIM_COLOR,
    SH_FFL_PIXEL_UNIFORM_RIM_POWER,
    SH_FFL_PIXEL_UNIFORM_MAX
};


// Shader for FFL
typedef struct {
    Shader shader; // Raylib Shader
    //int vertexUniformLocation[SH_FFL_VERTEX_UNIFORM_MAX];
    int pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_MAX];
    //int samplerLocation;
    int attributeLocation[FFL_ATTRIBUTE_BUFFER_TYPE_MAX];
    GLuint vboHandle[FFL_ATTRIBUTE_BUFFER_TYPE_MAX];
    GLuint vaoHandle;
    FFLShaderCallback callback;
    void* samplerTexture;
} ShaderForFFL;

// define global instance of the shader
ShaderForFFL gShaderForFFL;

// Callback forward declarations
void ShaderForFFL_ApplyAlphaTestCallback(void* pObj, bool enable, FFLRIOCompareFunc func, float ref);
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

int gLocationOfShaderForFFLSkinningEnable;

// Initialize the Shader
void ShaderForFFL_Initialize(ShaderForFFL* self)
{
    TraceLog(LOG_DEBUG, "In ShaderForFFL_Initialize");

    // Load the shader
    self->shader = LoadShaderFromMemory(vertexShaderCodeFFL, fragmentShaderCodeFFL);
    assert(self->shader.locs != NULL); // Shader did not load correctly.
    TraceLog(LOG_DEBUG, "Shader loaded");

    // Get uniform locations
    self->shader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(self->shader, "u_model");//"u_mv");
    TraceLog(LOG_TRACE, "Vertex uniform 'u_model' location: %d", self->shader.locs[SHADER_LOC_MATRIX_MODEL]);
    self->shader.locs[SHADER_LOC_MATRIX_VIEW] = GetShaderLocation(self->shader, "u_view");
    TraceLog(LOG_TRACE, "Vertex uniform 'u_view' location: %d", self->shader.locs[SHADER_LOC_MATRIX_VIEW]);
    self->shader.locs[SHADER_LOC_MATRIX_PROJECTION] = GetShaderLocation(self->shader, "u_proj");
    TraceLog(LOG_TRACE, "Vertex uniform 'u_proj' location: %d", self->shader.locs[SHADER_LOC_MATRIX_PROJECTION]);
    //self->vertexUniformLocation[SH_FFL_VERTEX_UNIFORM_IT] = GetShaderLocation(self->shader, "u_it");
    //TraceLog(LOG_TRACE, "Vertex uniform 'u_it' location: %d", self->vertexUniformLocation[SH_FFL_VERTEX_UNIFORM_IT]);

    gLocationOfShaderForFFLSkinningEnable = GetShaderLocation(self->shader, "skinningEnabled");

    self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_CONST1] = GetShaderLocation(self->shader, "u_const1");
    self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_CONST2] = GetShaderLocation(self->shader, "u_const2");
    self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_CONST3] = GetShaderLocation(self->shader, "u_const3");
    self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_MODE] = GetShaderLocation(self->shader, "u_mode");
    TraceLog(LOG_TRACE, "Pixel uniform 'u_const1' location: %d", self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_CONST1]);
    TraceLog(LOG_TRACE, "Pixel uniform 'u_const2' location: %d", self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_CONST2]);
    TraceLog(LOG_TRACE, "Pixel uniform 'u_const3' location: %d", self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_CONST3]);
    TraceLog(LOG_TRACE, "Pixel uniform 'u_mode' location: %d", self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_MODE]);

    self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_LIGHT_AMBIENT] = GetShaderLocation(self->shader, "u_light_ambient");
    self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_LIGHT_DIFFUSE] = GetShaderLocation(self->shader, "u_light_diffuse");
    self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_LIGHT_DIR] = GetShaderLocation(self->shader, "u_light_dir");
    self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_LIGHT_ENABLE] = GetShaderLocation(self->shader, "u_light_enable");
    self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_LIGHT_SPECULAR] = GetShaderLocation(self->shader, "u_light_specular");
    TraceLog(LOG_TRACE, "Pixel uniform 'u_light_ambient' location: %d", self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_LIGHT_AMBIENT]);
    TraceLog(LOG_TRACE, "Pixel uniform 'u_light_diffuse' location: %d", self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_LIGHT_DIFFUSE]);
    TraceLog(LOG_TRACE, "Pixel uniform 'u_light_dir' location: %d", self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_LIGHT_DIR]);
    TraceLog(LOG_TRACE, "Pixel uniform 'u_light_enable' location: %d", self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_LIGHT_ENABLE]);
    TraceLog(LOG_TRACE, "Pixel uniform 'u_light_specular' location: %d", self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_LIGHT_SPECULAR]);

    self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_MATERIAL_AMBIENT] = GetShaderLocation(self->shader, "u_material_ambient");
    self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_MATERIAL_DIFFUSE] = GetShaderLocation(self->shader, "u_material_diffuse");
    self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_MATERIAL_SPECULAR] = GetShaderLocation(self->shader, "u_material_specular");
    self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_MATERIAL_SPECULAR_MODE] = GetShaderLocation(self->shader, "u_material_specular_mode");
    self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_MATERIAL_SPECULAR_POWER] = GetShaderLocation(self->shader, "u_material_specular_power");
    TraceLog(LOG_TRACE, "Pixel uniform 'u_material_ambient' location: %d", self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_MATERIAL_AMBIENT]);
    TraceLog(LOG_TRACE, "Pixel uniform 'u_material_diffuse' location: %d", self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_MATERIAL_DIFFUSE]);
    TraceLog(LOG_TRACE, "Pixel uniform 'u_material_specular' location: %d", self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_MATERIAL_SPECULAR]);
    TraceLog(LOG_TRACE, "Pixel uniform 'u_material_specular_mode' location: %d", self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_MATERIAL_SPECULAR_MODE]);
    TraceLog(LOG_TRACE, "Pixel uniform 'u_material_specular_power' location: %d", self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_MATERIAL_SPECULAR_POWER]);

    self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_MODE] = GetShaderLocation(self->shader, "u_mode");
    TraceLog(LOG_TRACE, "Pixel uniform 'u_mode' location: %d", self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_MODE]);

    self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_RIM_COLOR] = GetShaderLocation(self->shader, "u_rim_color");
    self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_RIM_POWER] = GetShaderLocation(self->shader, "u_rim_power");
    TraceLog(LOG_TRACE, "Pixel uniform 'u_rim_color' location: %d", self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_RIM_COLOR]);
    TraceLog(LOG_TRACE, "Pixel uniform 'u_rim_power' location: %d", self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_RIM_POWER]);


    //self->samplerLocation = GetShaderLocation(self->shader, "s_texture");
    self->shader.locs[SHADER_LOC_MAP_ALBEDO] = GetShaderLocation(self->shader, "s_texture");
    TraceLog(LOG_TRACE, "Sampler uniform 's_texture' location: %d", self->shader.locs[SHADER_LOC_MAP_ALBEDO]);

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
void ShaderForFFL_Bind(ShaderForFFL* self, bool forInitTextures)
{
    TraceLog(LOG_TRACE, "In ShaderForFFL_Bind, calling BeginShaderMode, light enable: %i", forInitTextures);

    BeginShaderMode(self->shader);

#ifndef VAO_NOT_SUPPORTED
    glBindVertexArray(self->vaoHandle);
    TraceLog(LOG_TRACE, "VAO bound");
#endif

    for (int i = 0; i < FFL_ATTRIBUTE_BUFFER_TYPE_MAX; i++)
    {
        if (self->attributeLocation[i] != -1)
        {
            glDisableVertexAttribArray(self->attributeLocation[i]);
            TraceLog(LOG_TRACE, "Disabled vertex attrib array at location: %d", self->attributeLocation[i]);
        }
    }
    const int lightEnable = (int)!forInitTextures;

    SetShaderValue(self->shader, self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_LIGHT_ENABLE], &lightEnable, SHADER_UNIFORM_INT);
    if (lightEnable == 1) // usually disabled for init textures
    {
        // Bind shader uniforms that will never change
        SetShaderValue(self->shader, self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_LIGHT_DIR], &cLightDir, SHADER_UNIFORM_VEC3);
        SetShaderValue(self->shader, self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_LIGHT_AMBIENT], &cLightAmbient, SHADER_UNIFORM_VEC3);
        SetShaderValue(self->shader, self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_LIGHT_DIFFUSE], &cLightDiffuse, SHADER_UNIFORM_VEC3);
        SetShaderValue(self->shader, self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_LIGHT_SPECULAR], &cLightSpecular, SHADER_UNIFORM_VEC3);

        SetShaderValue(self->shader, self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_RIM_COLOR], &cRimColor, SHADER_UNIFORM_VEC3);
        SetShaderValue(self->shader, self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_RIM_POWER], &cRimPower, SHADER_UNIFORM_FLOAT);
    }

    const int zero = 0;
    SetShaderValue(self->shader, gLocationOfShaderForFFLSkinningEnable, &zero, SHADER_UNIFORM_INT);
}

// Set View Uniform
void ShaderForFFL_SetViewUniform(ShaderForFFL* self, const Matrix* model_mtx, const Matrix* view_mtx, const Matrix* proj_mtx)
{
    TraceLog(LOG_TRACE, "Setting view uniform");
    Matrix model, view, proj;
    if (model_mtx != NULL)
        model = *model_mtx;
    else
        model = MatrixIdentity();
    if (view_mtx != NULL)
        view = *view_mtx;
    else
        view = MatrixIdentity();
    if (proj_mtx != NULL)
        proj = *proj_mtx;
    else
        proj = MatrixIdentity();


    SetShaderValueMatrix(self->shader, self->shader.locs[SHADER_LOC_MATRIX_MODEL], model);
    SetShaderValueMatrix(self->shader, self->shader.locs[SHADER_LOC_MATRIX_VIEW], view);
    SetShaderValueMatrix(self->shader, self->shader.locs[SHADER_LOC_MATRIX_PROJECTION], proj);

    // Calculate the inverse transpose of the MV matrix
    //Matrix normalMatrix = MatrixTranspose(MatrixInvert(mv));
    //SetShaderValueMatrix(self->shader, self->vertexUniformLocation[SH_FFL_VERTEX_UNIFORM_IT], normalMatrix);
}

// Apply Alpha Test (no-op for now)
/*
void ShaderForFFL_ApplyAlphaTest(bool enable, FFLRIOCompareFunc func, float ref)
{
    // Alpha testing is not directly supported in OpenGL ES 2.0
    // This functionality can be emulated in the shader if needed
}
*/

// Callback: Apply Alpha Test
void ShaderForFFL_ApplyAlphaTestCallback(void* pObj, bool enable, FFLRIOCompareFunc func, float ref)
{
    TraceLog(LOG_TRACE, "ApplyAlphaTestCallback(%p, %b, %d, %f)", pObj, enable, func, ref);
    //ShaderForFFL_ApplyAlphaTest(enable, func, ref);
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

// Set material uniforms from FFLiDefaultShaderMaterial
void ShaderForFFL_SetMaterial(ShaderForFFL* self, const FFLiDefaultShaderMaterial* pMaterial)
{
    SetShaderValue(self->shader, self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_MATERIAL_AMBIENT], &pMaterial->ambient.x, SHADER_UNIFORM_VEC3);
    SetShaderValue(self->shader, self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_MATERIAL_DIFFUSE], &pMaterial->diffuse.x, SHADER_UNIFORM_VEC3);
    SetShaderValue(self->shader, self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_MATERIAL_SPECULAR], &pMaterial->specular.x, SHADER_UNIFORM_VEC3);

    int specularMode = 0; // blinn as default
    if (SHADER_FFL_SPECULAR_MODE != 0) // if the default is not blinn,
        specularMode = pMaterial->specularMode; // set it
    SetShaderValue(self->shader, self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_MATERIAL_SPECULAR_MODE], &specularMode, SHADER_UNIFORM_INT);
    SetShaderValue(self->shader, self->pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_MATERIAL_SPECULAR_POWER], &pMaterial->specularPower, SHADER_UNIFORM_FLOAT);

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

    SetShaderValueMatrix(self->shader, self->shader.locs[SHADER_LOC_MATRIX_MODEL], MatrixIdentity());
    SetShaderValueMatrix(self->shader, self->shader.locs[SHADER_LOC_MATRIX_VIEW], MatrixIdentity());
    SetShaderValueMatrix(self->shader, self->shader.locs[SHADER_LOC_MATRIX_PROJECTION], matrix);
}

RenderTexture gFacelineRenderTexture;
RenderTexture gMaskRenderTextures[FFL_EXPRESSION_LIMIT]; // a render texture for each mask
RenderTexture* gMaskRenderTextureCurrent = &gMaskRenderTextures[FFL_EXPRESSION_NORMAL];
// NOTE: default expression is not always normal

void SetCharModelExpression(FFLCharModel* pCharModel, FFLExpression expression)
{
    assert(expression < FFL_EXPRESSION_LIMIT);
    // assert(&gMaskRenderTextures[expression] != NULL); // but they are not initialized to null so nvm

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
        || pDrawParam->modulateParam.type == FFL_MODULATE_TYPE_SHAPE_MASK)
    {
        GLuint textureHandle;

        // For faceline and mask (FFL should always bind a texture2D to this...)
        // we will instead use the textures we made ourself
        if (pDrawParam->modulateParam.type == FFL_MODULATE_TYPE_SHAPE_FACELINE) {
            textureHandle = gFacelineRenderTexture.texture.id;
        } else if (pDrawParam->modulateParam.type == FFL_MODULATE_TYPE_SHAPE_MASK) {
            textureHandle = gMaskRenderTextureCurrent->texture.id;
        } else {
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
        glUniform1i(self->shader.locs[SHADER_LOC_MAP_ALBEDO], 0);
    } else {
        // If there is no texture, bind nothing
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    // bind material uniforms
    if (pDrawParam->modulateParam.type < FFL_MODULATE_TYPE_SHAPE_MAX
        && pDrawParam->modulateParam.type >= 0)
    {
        const FFLiDefaultShaderMaterial* param = &cMaterialParam[pDrawParam->modulateParam.type];
        ShaderForFFL_SetMaterial(self, param);
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
#ifdef GL_INT_2_10_10_10_REV
                    glVertexAttribPointer(location, 4, GL_INT_2_10_10_10_REV, GL_TRUE, stride, (void*)0);
#else
                    // NOTE: assuming FFL converted to FFLiSnorm8_8_8_8
                    glVertexAttribPointer(location, 4, GL_BYTE, GL_TRUE, stride, (void*)0);
#endif
                    break;
                case FFL_ATTRIBUTE_BUFFER_TYPE_TANGENT:
                    glVertexAttribPointer(location, 4, GL_BYTE, GL_TRUE, stride, (void*)0);
                    break;
                case FFL_ATTRIBUTE_BUFFER_TYPE_TEXCOORD:
                    glVertexAttribPointer(location, 2, GL_FLOAT, GL_FALSE, stride, (void*)0);
                    break;
                case FFL_ATTRIBUTE_BUFFER_TYPE_COLOR:
                    glVertexAttribPointer(location, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, (void*)0);
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

//const char* cFFLResourceHighFilename = "./FFLResHigh.dat";
const char* cFFLResourceHighFilename = "C:\\Users\\arko7939\\source\\repos\\FFL-Testing\\FFLResHigh.dat";
//const char* cFFLResourceHighFilename = "/home/arian/Downloads/ffl/tools/AFLResHigh_2_3_LE.dat";

// Calls FFLInitResEx and returns the result of FFLIsAvailable.
FFLResult InitializeFFL()
{
    TraceLog(LOG_DEBUG, "Before FFL initialization");
    /*
    const FFLInitDesc initDesc = {
        .fontRegion = FFL_FONT_REGION_JP_US_EU
    };
    */

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
    unsigned long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET); // Go back to the start of the file
    if (fileSize <= 0)
    {
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
    if (fileData == NULL)
    {
        TraceLog(LOG_ERROR, "Cannot allocate memory for resource buffer");
        fclose(file);
        return FFL_RESULT_ERROR;
    }
    // Read the file data into fileData
    size_t bytesRead = fread(fileData, 1, (size_t)fileSize, file);
    if (bytesRead == fileSize)
    {
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
    result = FFLInitRes(FFL_FONT_REGION_JP_US_EU, &gResourceDesc);//Ex(&initDesc, &gResourceDesc);
    //FFLResult result = FFLInitResEx(&init_desc, NULL); // lets ffl find resources itself

    if (result != FFL_RESULT_OK)
    {
        TraceLog(LOG_ERROR, "FFLInitResEx() failed with result: %d", (int)result);
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

    // const FFLExpressionFlag expressionFlag = 1 << FFL_EXPRESSION_PUZZLED;

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
    ShaderForFFL_Bind(&gShaderForFFL, true); // for init textures

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
    // texturesMatrix.m5 *= -1.f; // NOTE: ASSUMING DEFAULT OPENGL CLIP CONTROL
    ShaderForFFL_SetViewUniform(&gShaderForFFL, NULL, NULL, &texturesMatrix);

    FFLiCharModel* piCharModel = (FFLiCharModel*)pCharModel;

    FFLShaderCallback* pCallback = &gShaderForFFL.callback;
    FFLShaderCallback** ppCallback = &pCallback;

    FFLResolution textureResolution = piCharModel->charModelDesc.resolution & FFL_RESOLUTION_MASK;
    TraceLog(LOG_DEBUG, "Faceline/mask texture resolution: %d", textureResolution);
    // apply linear filtering to mask and faceline textures
    const int renderTextureFilter = TEXTURE_FILTER_BILINEAR;

    void** ppFacelineTexture2D = (void*)&piCharModel->facelineRenderTexture; // HACK: FFLiRenderTexture = FFLTexture**
    if (*ppFacelineTexture2D != NULL) // should we draw the faceline texture?
    {
        // assuming there is only one faceline texture ever, which there is
        gFacelineRenderTexture = LoadRenderTexture(textureResolution / 2, textureResolution);
        TraceLog(LOG_DEBUG, "Created render texture for faceline: %p, texture ID %d",
            &gFacelineRenderTexture, gFacelineRenderTexture.texture.id);
        ShaderForFFL_SetCulling(FFL_CULL_MODE_NONE);
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

        // glClearColor(facelineColor.r, facelineColor.g, facelineColor.b, facelineColor.a);

        // faceline blending
        // glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);
        rlSetBlendFactorsSeparate(RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA, RL_ONE, RL_ONE, RL_MIN, RL_MIN);
        rlSetBlendMode(RL_BLEND_CUSTOM_SEPARATE);
        glBlendEquation(GL_FUNC_ADD);

        FFLiDrawFacelineTexture(&piCharModel->pTextureTempObject->facelineTexture, ppCallback);
        // FFLiShaderCallback = **FFLShaderCallback
    } else {
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
            // pMaskTextures->pRenderTextures[i] = NULL;
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
        // glBlendFuncSeparate(GL_ONE_MINUS_DST_ALPHA, GL_DST_ALPHA, GL_SRC_ALPHA, GL_DST_ALPHA);
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

void GetHeightAndBuildFromFFLCharModel(FFLCharModel* pCharModel, int* height, int* build)
{
#ifdef FFLI_CHARINFO_H_ // easier to work with but private
    // NOTE: charInfo is at offset 0 in FFLiCharModel
    FFLiCharInfo* pCharInfo = (FFLiCharInfo*)pCharModel;
    TraceLog(LOG_DEBUG, "Casting FFLCharModel(%p) to FFLiCharInfo...", pCharModel);
    *height = pCharInfo->height;
    *build = pCharInfo->build;
#else // use FFLAditionalInfo
    FFLAdditionalInfo additionalInfo;
    const void* pBuffer = (const void*)pCharModel;
    // also working off of the assumption that
    // charInfo is at offset 0, as the below
    // function copies charmodel to charinfo (intentional?)
    [[maybe_unused]] FFLResult result = FFLGetAdditionalInfo(&additionalInfo, FFL_DATA_SOURCE_BUFFER, pBuffer, 0, false);
    TraceLog(LOG_DEBUG, "FFLGetAdditionalInfo(%p) result: %d", pBuffer, result);
    // this should only be called when model is already available so should not fail
    assert(result == FFL_RESULT_OK);
    *height = additionalInfo.height;
    *build = additionalInfo.build;
#endif
    TraceLog(LOG_DEBUG, "FFL model body height: %i, build: %i", *height, *build);
}

// referenced in anonymous function in nn::mii::detail::VariableIconBodyImpl::CalculateWorldMatrix
// also in ffl_app.rpx: FUN_020ec380 (FFLUtility), FUN_020737b8 (mii maker US)
void CalculateVariableIconBodyScaleFactors(Vector3* scale, float build, float height)

{
    // ScaleApply?
    // 0.47 / 128.0 = 0.003671875
    scale->x = (build * (height * 0.003671875f + 0.4f)) / 128.0f +
                // 0.23 / 128.0 = 0.001796875
                height * 0.001796875f + 0.4f;
                // 0.77 / 128.0 = 0.006015625
    scale->y = (height * 0.006015625f) + 0.5f;

    scale->z = scale->y;
}

typedef enum VriableIconBodyBoneKind {
    all_root = 0,
    body = 1,
    skl_root = 2,
    chest = 3,
    arm_l1 = 4,
    arm_l2 = 5,
    wrist_l = 6,
    elbow_l = 7,
    shoulder_l = 8,
    arm_r1 = 9,
    arm_r2 = 10,
    wrist_r = 11,
    elbow_r = 12,
    shoulder_r = 13,
    head = 14,
    chest_2 = 15,
    hip = 16,
    foot_l1 = 17,
    foot_l2 = 18,
    ankle_l = 19,
    knee_l = 20,
    foot_r1 = 21,
    foot_r2 = 22,
    ankle_r = 23,
    knee_r = 24,
    BoneKind_End = 25
} VriableIconBodyBoneKind;

void UpdateScaleForFFLBodyModel(Vector3* scale, VriableIconBodyBoneKind boneIndex,
    Vector3 scaleFactors)

{
    switch (boneIndex)
    {
    case all_root:
    case body:
    case skl_root:
        /*
        // uuusuallly they are not done for theeseee
        assert(false && "skip these bones, do body and onwards");
        scale->x = 1.0f;
        scale->y = 1.0f;
        scale->z = 1.0f;
        // above lines are added by MEEEEE
        break;
        */
    case chest:
    case chest_2:
    case hip:
    case foot_l1:
    case foot_l2:
    case foot_r1:
    case foot_r2:
        scale->x = scaleFactors.x;
        scale->y = scaleFactors.y;
        scale->z = scaleFactors.z;
        break;
    case arm_l1:
    case arm_l2:
    case elbow_l:
    case arm_r1:
    case arm_r2:
    case elbow_r:
        scale->x = scaleFactors.y;
        scale->y = scaleFactors.x;
        scale->z = scaleFactors.z;
        break;
    case wrist_l:
    case shoulder_l:
    case wrist_r:
    case shoulder_r:
    case ankle_l:
    case knee_l:
    case ankle_r:
    case knee_r:
        scale->x = scaleFactors.x;
        scale->y = scaleFactors.x;
        scale->z = scaleFactors.x;
        break;
    case head: {
        scale->x = scaleFactors.x;
        float y = fminf(scaleFactors.y, 1.0f);
        scale->y = y;
        scale->z = scaleFactors.z;
        break;
    }
    default:
        assert(false && "not sure which bone this is");
    }
    return;
}

int parentBoneTable[] = {
    0xFFFF, // all_root
    0, // body
    0, // skl_root
    2, // chest
    3, // arm_l1
    4, // arm_l2
    5, // wrist_l
    4, // elbow_l
    4, // shoulder_l
    3, // arm_r1
    9, // arm_r2
    10, // wrist_r
    9, // elbow_r
    9, // shoulder_r
    3, // head
    2, // chest_2
    2, // hip
    16, // foot_l1
    17, // foot_l2
    18, // ankle_l
    17, // knee_l
    16, // foot_r1
    21, // foot_r2
    22, // ankle_r
    21 // knee_r
};

void MyUpdateModelAnimationBoneMatrices(Model model, ModelAnimation anim, int frame, Vector3 scaleFactors)
{
    if ((anim.frameCount > 0) && (anim.bones != NULL) && (anim.framePoses != NULL))
    {
        if (frame >= anim.frameCount)
            frame = frame % anim.frameCount;

        for (int i = 0; i < model.meshCount; i++)
        {
            if (model.meshes[i].boneMatrices)
            {
                assert(model.meshes[i].boneCount == anim.boneCount);

                for (int boneId = 0; boneId < model.meshes[i].boneCount; boneId++)
                {
                    bool scaleEnable = boneId > 2; // chest
                    Vector3 scaleForBone;
                    if (scaleEnable)
                        UpdateScaleForFFLBodyModel(&scaleForBone,
                            (VriableIconBodyBoneKind)boneId,
                            scaleFactors);

                    Vector3 inTranslation = model.bindPose[boneId].translation;
                    Quaternion inRotation = model.bindPose[boneId].rotation;
                    Vector3 inScale /*Pre*/ = model.bindPose[boneId].scale;
                    /*
                                        Vector3 inScale;
                                        if (scaleEnable)
                                            inScalePre = Vector3Multiply(inScale, scaleForBone);
                                        else
                                            inScale = inScalePre;
                    */
                    Vector3 outTranslation = anim.framePoses[frame][boneId].translation;
                    Quaternion outRotation = anim.framePoses[frame][boneId].rotation;
                    Vector3 outScale /*Pre*/ = anim.framePoses[frame][boneId].scale;
                    /*
                                        Vector3 outScale;
                                        if (scaleEnable)
                                            outScale = Vector3Multiply(outScalePre, scaleForBone);
                                        else
                                            outScale = outScalePre;
                    */
                    Vector3 invTranslation = Vector3RotateByQuaternion(Vector3Negate(inTranslation), QuaternionInvert(inRotation));
                    Quaternion invRotation = QuaternionInvert(inRotation);
                    Vector3 invScale /*Pre*/ = Vector3Divide((Vector3) { 1.0f, 1.0f, 1.0f }, inScale);
                    /*
                                        Vector3 invScale;
                                        if (scaleEnable)
                                            invScalePre = Vector3Multiply(invScale, scaleForBone);
                                        else
                                            invScale = invScalePre;
                    */
                    Vector3 boneTranslation = Vector3Add(
                        Vector3RotateByQuaternion(Vector3Multiply(outScale, invTranslation),
                            outRotation),
                        outTranslation);
                    Quaternion boneRotation = QuaternionMultiply(outRotation, invRotation);
                    Vector3 boneScalePre = Vector3Multiply(outScale, invScale);

                    Vector3 boneScale;
                    if (scaleEnable)
                        boneScale = Vector3Multiply(boneScalePre, scaleForBone);
                    else
                        boneScale = boneScalePre;

                    Matrix boneMatrix = MatrixMultiply(MatrixMultiply(
                                                           QuaternionToMatrix(boneRotation),
                                                           MatrixTranslate(boneTranslation.x, boneTranslation.y, boneTranslation.z)),
                        MatrixScale(boneScale.x, boneScale.y, boneScale.z));
                    /*
                    int parentBoneId = parentBoneTable[boneId];
                    if (parentBoneId == 0xFFFF)
                        // This is a root bone
                        model.meshes[i].boneMatrices[boneId] = boneMatrix;
                    else
                        model.meshes[i].boneMatrices[boneId] = MatrixMultiply(model.meshes[i].boneMatrices[parentBoneId], boneMatrix);
                    */

                    model.meshes[i].boneMatrices[boneId] = boneMatrix;
                }
            }
        }
    }
}

void UpdateCharModelBlink(bool* isBlinking, double* lastBlinkTime, FFLCharModel* pCharModel, FFLExpression initialExpression, double now);

void TextureCallback_Create(void* v, const FFLTextureInfo* pTextureInfo, FFLTexture* pTexture)
{
    /*
        if (!pTextureInfo || !pTexture)
            return; // Invalid input
    */

    // Log the FFLTextureInfo details
    TraceLog(LOG_DEBUG, "CreateTexture: FFLTextureInfo { width: %d, height: %d, format: %d, imageSize: %d, mipCount: %d, imagePtr: %p, mipPtr: %p }",
        pTextureInfo->width, pTextureInfo->height, pTextureInfo->format, pTextureInfo->imageSize,
        pTextureInfo->mipCount, pTextureInfo->imagePtr, pTextureInfo->mipPtr);

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
        // free(textureHandle);
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
        pTextureInfo->imagePtr);
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
    // free(textureHandle);

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

    // all FFL initialization happens after gl context is created
    // e.g. FFLInitCharModelCPUStep is creating and uploading textures
    FFLGladLoadGL(glfwGetProcAddress);

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

#ifndef GL_INT_2_10_10_10_REV
    FFLSetNormalIsSnorm8_8_8_8(true);
#endif

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
#ifndef NO_MODELS_FOR_TEST
    camera.position = (Vector3) { 10.0f, 10.0f, 22.0f };
    camera.target = (Vector3) { 0.0f, 7.0f, 0.0f };
#else
    camera.position = (Vector3) { 2.0f, 4.0f, 12.0f };
    camera.target = (Vector3) { 0.0f, 2.5f, 0.0f };
#endif
    camera.up = (Vector3) { 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    // Load default shader for the cube (optional)
    Shader cubeShader = LoadShader(0, 0); // Use default shader

#ifndef NO_MODELS_FOR_TEST
    // Load body model
    const char* modelPath = "miibodymiddle tstste.glb";
    const float bodyScale = 0.7f;
    const Vector3 vecBodyScaleConst = (Vector3) { bodyScale, bodyScale, bodyScale };
    Matrix matBodyScale = MatrixScale(vecBodyScaleConst.x, vecBodyScaleConst.y, vecBodyScaleConst.z);
    Model model = LoadModel(modelPath);
    Model acceModel = LoadModel("cat ear.glb"); // LoadModel("/dev/shm/bear.glb");;
    if (model.meshes == NULL)
        TraceLog(LOG_DEBUG, "Body model failed to load, not going to attempt drawing it.");
    if (acceModel.meshes == NULL)
        TraceLog(LOG_DEBUG, "Accessory model also failed to load.");

    if (gShaderForFFL.shader.locs != NULL)
    {
        for (int j = 0; j < model.materialCount; j++)
        {
            model.materials[j].shader = gShaderForFFL.shader;
        }
        for (int j = 0; j < acceModel.materialCount; j++)
        {
            acceModel.materials[j].shader = gShaderForFFL.shader;
        }
    }

    // Load gltf model animations
    int animsCount = 0;
    unsigned int animIndex = 0;
    unsigned int animCurrentFrame = 0;
    ModelAnimation* modelAnimations = LoadModelAnimations(modelPath, &animsCount);
    if (modelAnimations == NULL)
        TraceLog(LOG_DEBUG, "modelAnimations == NULL, not updating animation or head matrices");

#endif

    SetTargetFPS(60);
    //--------------------------------------------------------------------------------------

    // blinking logic
    bool isBlinking = false;
    double lastBlinkTime = GetTime();
    FFLExpression initialExpression = FFLGetExpression(&charModel);

#ifndef NO_MODELS_FOR_TEST
    // height and build
    Vector3 modelFFLBodyScaleFactors;
    // for accessories
    FFLPartsTransform partsTransform;
    Matrix acceMatrix;
    Matrix acceMatrixRight;
    if (isFFLModelCreated)
    {
        FFLGetPartsTransform(&partsTransform, &charModel);
        //acceMatrix = MatrixTranslate(partsTransform.hatTranslate.x, partsTransform.hatTranslate.y, partsTransform.hatTranslate.z);
        Matrix acceSideTranslate = MatrixTranslate(partsTransform.headSideTranslate.x, partsTransform.headSideTranslate.y, partsTransform.headSideTranslate.z);
        Matrix acceSideTranslateRight = MatrixTranslate(-partsTransform.headSideTranslate.x, partsTransform.headSideTranslate.y, partsTransform.headSideTranslate.z);
        Matrix acceSideRotate = MatrixRotateXYZ((Vector3) { partsTransform.headSideRotate.x, partsTransform.headSideRotate.y, partsTransform.headSideRotate.z });
        Matrix acceSideRotateRight = MatrixRotateXYZ((Vector3) { -partsTransform.headSideRotate.x, -partsTransform.headSideRotate.y, -partsTransform.headSideRotate.z });

        acceMatrix = MatrixMultiply(acceSideRotate, acceSideTranslate);
        acceMatrixRight = MatrixMultiply(acceSideRotateRight, acceSideTranslateRight);

        int iHeight, iBuild;
        iHeight = 1; iBuild = 1; // NOTE: FOR DEBUG
        //GetHeightAndBuildFromFFLCharModel(&charModel, &iHeight, &iBuild);

        CalculateVariableIconBodyScaleFactors(&modelFFLBodyScaleFactors, (float)iBuild, (float)iHeight);
        TraceLog(LOG_DEBUG, "Body scale factors: X %f, Y %f", modelFFLBodyScaleFactors.x, modelFFLBodyScaleFactors.y, modelFFLBodyScaleFactors.z);
    } else
        modelFFLBodyScaleFactors = (Vector3) { 1.0f, 1.0f, 1.0f };

    /*
    Vector3 vecBodyScaleFinal = Vector3Multiply(vecBodyScaleConst, modelFFLBodyScaleFactors);
    TraceLog(LOG_DEBUG, "Final body scale: X %f, Y %f, Z %f", vecBodyScaleFinal.x, vecBodyScaleFinal.y, vecBodyScaleFinal.z);
    Matrix matBodyScale = MatrixScale(vecBodyScaleFinal.x, vecBodyScaleFinal.y, vecBodyScaleFinal.z);

    Vector3 vecHeadScale = Vector3Multiply(vecBodyScaleConst, (Vector3){ modelFFLBodyScaleFactors.x, fmin(modelFFLBodyScaleFactors.y, 1.0f), modelFFLBodyScaleFactors.z });
    Matrix matHeadScale = MatrixScale(vecHeadScale.x, vecHeadScale.y, vecHeadScale.z);
    */
#endif

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
#ifndef NO_MODELS_FOR_TEST
        UpdateCamera(&camera, CAMERA_FIRST_PERSON);
        // Update
        //----------------------------------------------------------------------------------
        // Calculate rotation angle
#else
        float rotationAngle;
#endif
        double now;

#ifdef NO_MODELS_FOR_TEST
        // Actually, if FFL is not available, it should be constant
        if (!isFFLAvailable)
            rotationAngle = 1.0f;
        else
#endif
        {
            now = GetTime();
#ifdef NO_MODELS_FOR_TEST
            rotationAngle = (float)now * 45.0f; // 45 degrees per second
#endif
        }

#ifndef NO_MODELS_FOR_TEST
        // Update model animation
        ModelAnimation anim;
        Matrix headBoneMatrix;
        Matrix headModelMatrix;
        if (modelAnimations != NULL)
        {
            anim = modelAnimations[animIndex];
            animCurrentFrame = (animCurrentFrame + 1) % anim.frameCount;
            MyUpdateModelAnimationBoneMatrices(model, anim, animCurrentFrame, modelFFLBodyScaleFactors);

            headBoneMatrix = model.meshes[0].boneMatrices[14];

            headModelMatrix = MatrixMultiply(headBoneMatrix, matBodyScale);
        }
        else
        {
            headBoneMatrix = MatrixIdentity();
            headModelMatrix = MatrixIdentity();
        }


        //int boneId = ((int)rotationAngle / 20 ) % model.boneCount; // head??
        /*
        const int boneId = 14; // think this is head

        Vector3 inTranslation = model.bindPose[boneId].translation;
        Quaternion inRotation = model.bindPose[boneId].rotation;
        Vector3 inScale = model.bindPose[boneId].scale;

        Vector3 outTranslation = anim.framePoses[animCurrentFrame][boneId].translation;
        Quaternion outRotation = anim.framePoses[animCurrentFrame][boneId].rotation;
        Vector3 outScale = anim.framePoses[animCurrentFrame][boneId].scale;

        Vector3 invTranslation = Vector3RotateByQuaternion(Vector3Negate(inTranslation), QuaternionInvert(inRotation));
        Quaternion invRotation = QuaternionInvert(inRotation);
        Vector3 invScale = Vector3Divide((Vector3){ 1.0f, 1.0f, 1.0f }, inScale);

        Vector3 boneTranslation = Vector3Add(
            Vector3RotateByQuaternion(Vector3Multiply(outScale, invTranslation),
            outRotation), outTranslation);
        Quaternion boneRotation = QuaternionMultiply(outRotation, invRotation);
        Vector3 bodyBoneScale = Vector3Multiply(outScale, invScale);
        Vector3 boneScale = Vector3Scale(bodyBoneScale, bodyScale);

        Matrix headModelMatrix = MatrixMultiply(MatrixMultiply(
            QuaternionToMatrix(boneRotation),
            MatrixTranslate(boneTranslation.x, boneTranslation.y, boneTranslation.z)),
            MatrixScale(boneScale.x, boneScale.y, boneScale.z));
        */

        //----------------------------------------------------------------------------------

        const Vector3 bodyColor = { 0.094f, 0.094f, 0.078f };
        const Vector3 pantsColor = { 0.439f, 0.125f, 0.063f };
#endif
        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();
        ClearBackground(SKYBLUE);

        BeginMode3D(camera);
        DrawGrid(10, 1.0f);

        // Apply rotation to the cube
#ifdef NO_MODELS_FOR_TEST
        rlRotatef(rotationAngle, 0.0f, 1.0f, 0.0f); // Rotate around Y axis
#else
        /*
        DrawCubeV((Vector3){ 0.0f, 0.0f, 0.0f }, (Vector3){ 2.0f, 2.0f, 2.0f }, RED);
        DrawCubeWiresV((Vector3){ 0.0f, 0.0f, 0.0f }, (Vector3){ 2.0f, 2.0f, 2.0f }, MAROON);
        */

        if (model.meshes != NULL)
        {

            ShaderForFFL_Bind(&gShaderForFFL, false);
            const int one = 1;
            SetShaderValue(gShaderForFFL.shader, gLocationOfShaderForFFLSkinningEnable, &one, SHADER_UNIFORM_INT);

            for (int i = 0; i < model.meshCount; i++) // will be 0 if failed to load
            {
                const int zero = 0;
                SetShaderValue(gShaderForFFL.shader, gShaderForFFL.pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_MODE], &zero, SHADER_UNIFORM_INT);
                if ((i % 2) == 1) // pants
                {
                    ShaderForFFL_SetMaterial(&gShaderForFFL, &cMaterialParam[MATERIAL_PARAM_PANTS]);
                    SetShaderValue(gShaderForFFL.shader, gShaderForFFL.pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_CONST1], &pantsColor, SHADER_UNIFORM_VEC3);
                }
                else
                {
                    ShaderForFFL_SetMaterial(&gShaderForFFL, &cMaterialParam[MATERIAL_PARAM_BODY]);
                    SetShaderValue(gShaderForFFL.shader, gShaderForFFL.pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_CONST1], &bodyColor, SHADER_UNIFORM_VEC3);
                }
                DrawMesh(model.meshes[i], model.materials[model.meshMaterial[i]], matBodyScale);
            }

            EndShaderMode(); // unbind the shader if not drawing ffl model
            // Draw custom OpenGL object after Raylib's 3D drawing
            rlDrawRenderBatchActive(); // Flush Raylib's internal buffers
        }
#endif

        if (isFFLModelCreated) // draw only if it is valid
        {
            // FFL model scale is 10.0
#ifndef NO_MODELS_FOR_TEST
            Matrix matModel = MatrixMultiply(MatrixScale(0.1f, 0.1f, 0.1f), headModelMatrix);
#else
            Matrix matModel = MatrixScale(0.1f, 0.1f, 0.1f);
#endif
            rlPushMatrix();
            Matrix matView = rlGetMatrixModelview();
            Matrix matProjection = rlGetMatrixProjection();
            rlPopMatrix(); // Restore the previous matrix

            UpdateCharModelBlink(&isBlinking, &lastBlinkTime, &charModel, initialExpression, now);

            ShaderForFFL_Bind(&gShaderForFFL, false);
            ShaderForFFL_SetViewUniform(&gShaderForFFL,
                &matModel,
                &matView, &matProjection);
            FFLDrawOpa(&charModel);
            FFLDrawXlu(&charModel);
#ifndef NO_MODELS_FOR_TEST
            Matrix matAcceModel = MatrixMultiply(acceMatrix, matModel);
            Matrix matAcceModelRight = MatrixMultiply(acceMatrixRight, matModel);

            for (int i = 0; i < acceModel.meshCount; i++) // will be 0 if failed to load
            {
                const int zero = 0;
                const Vector3 hatColor = { 0.50, 0.0, 0.50 }; //{ 0.58, 0.29, 0.0 };
                SetShaderValue(gShaderForFFL.shader, gShaderForFFL.pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_MODE], &zero, SHADER_UNIFORM_INT);
                ShaderForFFL_SetMaterial(&gShaderForFFL, &cMaterialParam[MATERIAL_PARAM_BODY]);
                SetShaderValue(gShaderForFFL.shader, gShaderForFFL.pixelUniformLocation[SH_FFL_PIXEL_UNIFORM_CONST1], &hatColor, SHADER_UNIFORM_VEC3);

                DrawMesh(acceModel.meshes[0], acceModel.materials[0], matAcceModel);
                DrawMesh(acceModel.meshes[0], acceModel.materials[0], matAcceModelRight);
            }
#endif
        }

        EndMode3D();

        // Display FPS100ms
        DrawFPS(10, 10);
        /*
        char whichBone[64];
        sprintf((char*)&whichBone, "bone id %i\n", boneId);
        DrawText(whichBone, 50, 50, 60, PURPLE);
        */
        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------

    UnloadShader(cubeShader); // Unload default shader
    UnloadShader(gShaderForFFL.shader); // Unload shader for FFL
#ifndef NO_MODELS_FOR_TEST
    if (model.meshes != NULL)
        UnloadModel(model);
    if (modelAnimations != NULL)
        UnloadModelAnimations(modelAnimations, animsCount);
    if (acceModel.meshes != NULL)
        UnloadModel(acceModel);
#endif

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

    CloseWindow(); // Close window and OpenGL context
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
        // FFLSetExpression(pCharModel, FFL_EXPRESSION_BLINK);
        SetCharModelExpression(pCharModel, FFL_EXPRESSION_BLINK);
        *isBlinking = true;
        TraceLog(LOG_TRACE, "expression: %d", FFL_EXPRESSION_BLINK);
        *lastBlinkTime = now; // Reset the blink time
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
