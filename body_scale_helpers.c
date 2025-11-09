#include "cgltf.h"

//
// rmodels.c
//

#define GLTF_ANIMDELAY 17    // Animation frames delay, (~1000 ms/60 FPS = 16.666666* ms)

// Load bone info from GLTF skin data
static BoneInfo *my_LoadBoneInfoGLTF(cgltf_skin skin, int *boneCount)
{
    *boneCount = (int)skin.joints_count;
    BoneInfo *bones = (BoneInfo *)RL_CALLOC(skin.joints_count, sizeof(BoneInfo));

    for (unsigned int i = 0; i < skin.joints_count; i++)
    {
        cgltf_node node = *skin.joints[i];
        if (node.name != NULL) strncpy(bones[i].name, node.name, sizeof(bones[i].name) - 1);

        // Find parent bone index
        int parentIndex = -1;

        for (unsigned int j = 0; j < skin.joints_count; j++)
        {
            if (skin.joints[j] == node.parent)
            {
                parentIndex = (int)j;
                break;
            }
        }

        bones[i].parent = parentIndex;
    }

    return bones;
}

// Get interpolated pose for bone sampler at a specific time. Returns true on success
static bool my_GetPoseAtTimeGLTF(cgltf_interpolation_type interpolationType, cgltf_accessor *input, cgltf_accessor *output, float time, void *data)
{
    if (interpolationType >= cgltf_interpolation_type_max_enum) return false;

    // Input and output should have the same count
    float tstart = 0.0f;
    float tend = 0.0f;
    int keyframe = 0;       // Defaults to first pose

    for (int i = 0; i < (int)input->count - 1; i++)
    {
        cgltf_bool r1 = cgltf_accessor_read_float(input, i, &tstart, 1);
        if (!r1) return false;

        cgltf_bool r2 = cgltf_accessor_read_float(input, i + 1, &tend, 1);
        if (!r2) return false;

        if ((tstart <= time) && (time < tend))
        {
            keyframe = i;
            break;
        }
    }

    // Constant animation, no need to interpolate
    if (FloatEquals(tend, tstart)) return true;

    float duration = fmaxf((tend - tstart), EPSILON);
    float t = (time - tstart)/duration;
    t = (t < 0.0f)? 0.0f : t;
    t = (t > 1.0f)? 1.0f : t;

    if (output->component_type != cgltf_component_type_r_32f) return false;

    if (output->type == cgltf_type_vec3)
    {
        switch (interpolationType)
        {
            case cgltf_interpolation_type_step:
            {
                float tmp[3] = { 0.0f };
                cgltf_accessor_read_float(output, keyframe, tmp, 3);
                Vector3 v1 = {tmp[0], tmp[1], tmp[2]};
                Vector3 *r = (Vector3 *)data;

                *r = v1;
            } break;
            case cgltf_interpolation_type_linear:
            {
                float tmp[3] = { 0.0f };
                cgltf_accessor_read_float(output, keyframe, tmp, 3);
                Vector3 v1 = {tmp[0], tmp[1], tmp[2]};
                cgltf_accessor_read_float(output, keyframe+1, tmp, 3);
                Vector3 v2 = {tmp[0], tmp[1], tmp[2]};
                Vector3 *r = (Vector3 *)data;

                *r = Vector3Lerp(v1, v2, t);
            } break;
            case cgltf_interpolation_type_cubic_spline:
            {
                float tmp[3] = { 0.0f };
                cgltf_accessor_read_float(output, 3*keyframe+1, tmp, 3);
                Vector3 v1 = {tmp[0], tmp[1], tmp[2]};
                cgltf_accessor_read_float(output, 3*keyframe+2, tmp, 3);
                Vector3 tangent1 = {tmp[0], tmp[1], tmp[2]};
                cgltf_accessor_read_float(output, 3*(keyframe+1)+1, tmp, 3);
                Vector3 v2 = {tmp[0], tmp[1], tmp[2]};
                cgltf_accessor_read_float(output, 3*(keyframe+1), tmp, 3);
                Vector3 tangent2 = {tmp[0], tmp[1], tmp[2]};
                Vector3 *r = (Vector3 *)data;

                *r = Vector3CubicHermite(v1, tangent1, v2, tangent2, t);
            } break;
            default: break;
        }
    }
    else if (output->type == cgltf_type_vec4)
    {
        // Only v4 is for rotations, so we know it's a quaternion
        switch (interpolationType)
        {
            case cgltf_interpolation_type_step:
            {
                float tmp[4] = { 0.0f };
                cgltf_accessor_read_float(output, keyframe, tmp, 4);
                Vector4 v1 = {tmp[0], tmp[1], tmp[2], tmp[3]};
                Vector4 *r = (Vector4 *)data;

                *r = v1;
            } break;
            case cgltf_interpolation_type_linear:
            {
                float tmp[4] = { 0.0f };
                cgltf_accessor_read_float(output, keyframe, tmp, 4);
                Vector4 v1 = {tmp[0], tmp[1], tmp[2], tmp[3]};
                cgltf_accessor_read_float(output, keyframe+1, tmp, 4);
                Vector4 v2 = {tmp[0], tmp[1], tmp[2], tmp[3]};
                Vector4 *r = (Vector4 *)data;

                *r = QuaternionSlerp(v1, v2, t);
            } break;
            case cgltf_interpolation_type_cubic_spline:
            {
                float tmp[4] = { 0.0f };
                cgltf_accessor_read_float(output, 3*keyframe+1, tmp, 4);
                Vector4 v1 = {tmp[0], tmp[1], tmp[2], tmp[3]};
                cgltf_accessor_read_float(output, 3*keyframe+2, tmp, 4);
                Vector4 outTangent1 = {tmp[0], tmp[1], tmp[2], 0.0f};
                cgltf_accessor_read_float(output, 3*(keyframe+1)+1, tmp, 4);
                Vector4 v2 = {tmp[0], tmp[1], tmp[2], tmp[3]};
                cgltf_accessor_read_float(output, 3*(keyframe+1), tmp, 4);
                Vector4 inTangent2 = {tmp[0], tmp[1], tmp[2], 0.0f};
                Vector4 *r = (Vector4 *)data;

                v1 = QuaternionNormalize(v1);
                v2 = QuaternionNormalize(v2);

                if (Vector4DotProduct(v1, v2) < 0.0f)
                {
                    v2 = Vector4Negate(v2);
                }

                outTangent1 = Vector4Scale(outTangent1, duration);
                inTangent2 = Vector4Scale(inTangent2, duration);

                *r = QuaternionCubicHermiteSpline(v1, outTangent1, v2, inTangent2, t);
            } break;
            default: break;
        }
    }

    return true;
}

//
// Copy-pasted version of LoadModelAnimationsGLTF and its
// reliant methods from rmodels.c, but with the function
// BuildPoseFromParentJoints commented out. This leaves
// the joints unmultiplied by their parents, so we can
// do it ourselves for proper per-bone body scaling.
//

static ModelAnimation *LoadModelAnimationsGLBParents(const char *fileName, int *animCount)
{
    // glTF file loading
    int dataSize = 0;
    unsigned char *fileData = LoadFileData(fileName, &dataSize);

    ModelAnimation *animations = NULL;

    // glTF data loading
    cgltf_options options = { 0 };
    // no loading files
    //options.file.read = LoadFileGLTFCallback;
    //options.file.release = ReleaseFileGLTFCallback;
    cgltf_data *data = NULL;
    cgltf_result result = cgltf_parse(&options, fileData, dataSize, &data);

    if (result != cgltf_result_success)
    {
        TRACELOG(LOG_WARNING, "MODEL: [%s] Failed to load glTF data", fileName);
        *animCount = 0;
        return NULL;
    }

    result = cgltf_load_buffers(&options, data, fileName);
    if (result != cgltf_result_success) TRACELOG(LOG_INFO, "MODEL: [%s] Failed to load animation buffers", fileName);

    if (result == cgltf_result_success)
    {
        if (data->skins_count > 0)
        {
            cgltf_skin skin = data->skins[0];
            *animCount = (int)data->animations_count;
            animations = (ModelAnimation *)RL_CALLOC(data->animations_count, sizeof(ModelAnimation));

            Transform worldTransform = { 0 };
            cgltf_float cgltf_worldTransform[16] = { 0 };
            cgltf_node *node = skin.joints[0];
            cgltf_node_transform_world(node->parent, cgltf_worldTransform);
            Matrix worldMatrix = {
                cgltf_worldTransform[0], cgltf_worldTransform[4], cgltf_worldTransform[8], cgltf_worldTransform[12],
                cgltf_worldTransform[1], cgltf_worldTransform[5], cgltf_worldTransform[9], cgltf_worldTransform[13],
                cgltf_worldTransform[2], cgltf_worldTransform[6], cgltf_worldTransform[10], cgltf_worldTransform[14],
                cgltf_worldTransform[3], cgltf_worldTransform[7], cgltf_worldTransform[11], cgltf_worldTransform[15]
            };
            MatrixDecompose(worldMatrix, &worldTransform.translation, &worldTransform.rotation, &worldTransform.scale);

            for (unsigned int i = 0; i < data->animations_count; i++)
            {
                animations[i].bones = my_LoadBoneInfoGLTF(skin, &animations[i].boneCount);

                cgltf_animation animData = data->animations[i];

                struct Channels {
                    cgltf_animation_channel *translate;
                    cgltf_animation_channel *rotate;
                    cgltf_animation_channel *scale;
                    cgltf_interpolation_type interpolationType;
                };

                struct Channels *boneChannels = (struct Channels *)RL_CALLOC(animations[i].boneCount, sizeof(struct Channels));
                float animDuration = 0.0f;

                for (unsigned int j = 0; j < animData.channels_count; j++)
                {
                    cgltf_animation_channel channel = animData.channels[j];
                    int boneIndex = -1;

                    for (unsigned int k = 0; k < skin.joints_count; k++)
                    {
                        if (animData.channels[j].target_node == skin.joints[k])
                        {
                            boneIndex = k;
                            break;
                        }
                    }

                    if (boneIndex == -1)
                    {
                        // Animation channel for a node not in the armature
                        continue;
                    }

                    boneChannels[boneIndex].interpolationType = animData.channels[j].sampler->interpolation;

                    if (animData.channels[j].sampler->interpolation != cgltf_interpolation_type_max_enum)
                    {
                        if (channel.target_path == cgltf_animation_path_type_translation)
                        {
                            boneChannels[boneIndex].translate = &animData.channels[j];
                        }
                        else if (channel.target_path == cgltf_animation_path_type_rotation)
                        {
                            boneChannels[boneIndex].rotate = &animData.channels[j];
                        }
                        else if (channel.target_path == cgltf_animation_path_type_scale)
                        {
                            boneChannels[boneIndex].scale = &animData.channels[j];
                        }
                        else
                        {
                            TRACELOG(LOG_WARNING, "MODEL: [%s] Unsupported target_path on channel %d's sampler for animation %d. Skipping.", fileName, j, i);
                        }
                    }
                    else TRACELOG(LOG_WARNING, "MODEL: [%s] Invalid interpolation curve encountered for GLTF animation.", fileName);

                    float t = 0.0f;
                    cgltf_bool r = cgltf_accessor_read_float(channel.sampler->input, channel.sampler->input->count - 1, &t, 1);

                    if (!r)
                    {
                        TRACELOG(LOG_WARNING, "MODEL: [%s] Failed to load input time", fileName);
                        continue;
                    }

                    animDuration = (t > animDuration)? t : animDuration;
                }

                if (animData.name != NULL) strncpy(animations[i].name, animData.name, sizeof(animations[i].name) - 1);

                animations[i].frameCount = (int)(animDuration*1000.0f/GLTF_ANIMDELAY) + 1;
                animations[i].framePoses = (Transform **)RL_MALLOC(animations[i].frameCount*sizeof(Transform *));

                for (int j = 0; j < animations[i].frameCount; j++)
                {
                    animations[i].framePoses[j] = (Transform *)RL_MALLOC(animations[i].boneCount*sizeof(Transform));
                    float time = ((float) j*GLTF_ANIMDELAY)/1000.0f;

                    for (int k = 0; k < animations[i].boneCount; k++)
                    {
                        Vector3 translation = {skin.joints[k]->translation[0], skin.joints[k]->translation[1], skin.joints[k]->translation[2]};
                        Quaternion rotation = {skin.joints[k]->rotation[0], skin.joints[k]->rotation[1], skin.joints[k]->rotation[2], skin.joints[k]->rotation[3]};
                        Vector3 scale = {skin.joints[k]->scale[0], skin.joints[k]->scale[1], skin.joints[k]->scale[2]};

                        if (boneChannels[k].translate)
                        {
                            if (!my_GetPoseAtTimeGLTF(boneChannels[k].interpolationType, boneChannels[k].translate->sampler->input, boneChannels[k].translate->sampler->output, time, &translation))
                            {
                                TRACELOG(LOG_INFO, "MODEL: [%s] Failed to load translate pose data for bone %s", fileName, animations[i].bones[k].name);
                            }
                        }

                        if (boneChannels[k].rotate)
                        {
                            if (!my_GetPoseAtTimeGLTF(boneChannels[k].interpolationType, boneChannels[k].rotate->sampler->input, boneChannels[k].rotate->sampler->output, time, &rotation))
                            {
                                TRACELOG(LOG_INFO, "MODEL: [%s] Failed to load rotate pose data for bone %s", fileName, animations[i].bones[k].name);
                            }
                        }

                        if (boneChannels[k].scale)
                        {
                            if (!my_GetPoseAtTimeGLTF(boneChannels[k].interpolationType, boneChannels[k].scale->sampler->input, boneChannels[k].scale->sampler->output, time, &scale))
                            {
                                TRACELOG(LOG_INFO, "MODEL: [%s] Failed to load scale pose data for bone %s", fileName, animations[i].bones[k].name);
                            }
                        }

                        animations[i].framePoses[j][k] = (Transform){
                            .translation = translation,
                            .rotation = rotation,
                            .scale = scale
                        };
                    }

                    Transform* root = &animations[i].framePoses[j][0];
                    root->rotation = QuaternionMultiply(worldTransform.rotation, root->rotation);
                    root->scale = Vector3Multiply(root->scale, worldTransform.scale);
                    root->translation = Vector3Multiply(root->translation, worldTransform.scale);
                    root->translation = Vector3RotateByQuaternion(root->translation, worldTransform.rotation);
                    root->translation = Vector3Add(root->translation, worldTransform.translation);

                    // BuildPoseFromParentJoints(animations[i].bones, animations[i].boneCount, animations[i].framePoses[j]);
                }

                TRACELOG(LOG_INFO, "MODEL: [%s] Loaded animation: %s (%d frames, %fs)", fileName, (animData.name != NULL)? animData.name : "NULL", animations[i].frameCount, animDuration);
                RL_FREE(boneChannels);
            }
        }

        if (data->skins_count > 1)
        {
            TRACELOG(LOG_WARNING, "MODEL: [%s] expected exactly one skin to load animation data from, but found %i", fileName, data->skins_count);
        }

        cgltf_free(data);
    }
    UnloadFileData(fileData);
    return animations;
}
