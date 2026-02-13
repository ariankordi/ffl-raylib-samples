#define BONE_NAME_LENGTH 32
// Load IQM animation data
static ModelAnimation *LoadModelAnimationsIQMParents(const char *fileName, int *animCount)
{
    #define IQM_MAGIC       "INTERQUAKEMODEL"   // IQM file magic number
    #define IQM_VERSION     2                   // only IQM version 2 supported

    int dataSize = 0;
    unsigned char *fileData = LoadFileData(fileName, &dataSize);
    unsigned char *fileDataPtr = fileData;

    typedef struct IQMHeader {
        char magic[16];
        unsigned int version;
        unsigned int dataSize;
        unsigned int flags;
        unsigned int num_text, ofs_text;
        unsigned int num_meshes, ofs_meshes;
        unsigned int num_vertexarrays, num_vertexes, ofs_vertexarrays;
        unsigned int num_triangles, ofs_triangles, ofs_adjacency;
        unsigned int num_joints, ofs_joints;
        unsigned int num_poses, ofs_poses;
        unsigned int num_anims, ofs_anims;
        unsigned int num_frames, num_framechannels, ofs_frames, ofs_bounds;
        unsigned int num_comment, ofs_comment;
        unsigned int num_extensions, ofs_extensions;
    } IQMHeader;

    typedef struct IQMJoint {
        unsigned int name;
        int parent;
        float translate[3], rotate[4], scale[3];
    } IQMJoint;

    typedef struct IQMPose {
        int parent;
        unsigned int mask;
        float channeloffset[10];
        float channelscale[10];
    } IQMPose;

    typedef struct IQMAnim {
        unsigned int name;
        unsigned int first_frame, num_frames;
        float framerate;
        unsigned int flags;
    } IQMAnim;

    // In case file can not be read, return an empty model
    if (fileDataPtr == NULL) return NULL;

    // Read IQM header
    IQMHeader *iqmHeader = (IQMHeader *)fileDataPtr;

    if (memcmp(iqmHeader->magic, IQM_MAGIC, sizeof(IQM_MAGIC)) != 0)
    {
        TRACELOG(LOG_WARNING, "MODEL: [%s] IQM file is not a valid model", fileName);
        UnloadFileData(fileData);
        return NULL;
    }

    if (iqmHeader->version != IQM_VERSION)
    {
        TRACELOG(LOG_WARNING, "MODEL: [%s] IQM file version not supported (%i)", fileName, iqmHeader->version);
        UnloadFileData(fileData);
        return NULL;
    }

    // Get bones data
    IQMPose *poses = (IQMPose *)RL_MALLOC(iqmHeader->num_poses*sizeof(IQMPose));
    //fseek(iqmFile, iqmHeader->ofs_poses, SEEK_SET);
    //fread(poses, sizeof(IQMPose), iqmHeader->num_poses, iqmFile);
    memcpy(poses, fileDataPtr + iqmHeader->ofs_poses, iqmHeader->num_poses*sizeof(IQMPose));

    // Get animations data
    *animCount = iqmHeader->num_anims;
    IQMAnim *anim = (IQMAnim *)RL_MALLOC(iqmHeader->num_anims*sizeof(IQMAnim));
    //fseek(iqmFile, iqmHeader->ofs_anims, SEEK_SET);
    //fread(anim, sizeof(IQMAnim), iqmHeader->num_anims, iqmFile);
    memcpy(anim, fileDataPtr + iqmHeader->ofs_anims, iqmHeader->num_anims*sizeof(IQMAnim));

    ModelAnimation *animations = (ModelAnimation *)RL_MALLOC(iqmHeader->num_anims*sizeof(ModelAnimation));

    // frameposes
    unsigned short *framedata = (unsigned short *)RL_MALLOC(iqmHeader->num_frames*iqmHeader->num_framechannels*sizeof(unsigned short));
    //fseek(iqmFile, iqmHeader->ofs_frames, SEEK_SET);
    //fread(framedata, sizeof(unsigned short), iqmHeader->num_frames*iqmHeader->num_framechannels, iqmFile);
    memcpy(framedata, fileDataPtr + iqmHeader->ofs_frames, iqmHeader->num_frames*iqmHeader->num_framechannels*sizeof(unsigned short));

    // joints
    IQMJoint *joints = (IQMJoint *)RL_MALLOC(iqmHeader->num_joints*sizeof(IQMJoint));
    memcpy(joints, fileDataPtr + iqmHeader->ofs_joints, iqmHeader->num_joints*sizeof(IQMJoint));

    for (unsigned int a = 0; a < iqmHeader->num_anims; a++)
    {
        animations[a].frameCount = anim[a].num_frames;
        animations[a].boneCount = iqmHeader->num_poses;
        animations[a].bones = (BoneInfo *)RL_MALLOC(iqmHeader->num_poses*sizeof(BoneInfo));
        animations[a].framePoses = (Transform **)RL_MALLOC(anim[a].num_frames*sizeof(Transform *));
        memcpy(animations[a].name, fileDataPtr + iqmHeader->ofs_text + anim[a].name, 32);
        TRACELOG(LOG_INFO, "IQM Anim %s", animations[a].name);
        //animations[a].framerate = anim.framerate; // TODO: Use animation framerate data?

        for (unsigned int j = 0; j < iqmHeader->num_poses; j++)
        {
            // If animations and skeleton are in the same file, copy bone names to anim
            if (iqmHeader->num_joints > 0) memcpy(animations[a].bones[j].name, fileDataPtr + iqmHeader->ofs_text + joints[j].name, BONE_NAME_LENGTH*sizeof(char));
            else memcpy(animations[a].bones[j].name, "ANIMJOINTNAME", 13); // Default bone name otherwise
            animations[a].bones[j].parent = poses[j].parent;
        }

        for (unsigned int j = 0; j < anim[a].num_frames; j++) animations[a].framePoses[j] = (Transform *)RL_MALLOC(iqmHeader->num_poses*sizeof(Transform));

        int dcounter = anim[a].first_frame*iqmHeader->num_framechannels;

        for (unsigned int frame = 0; frame < anim[a].num_frames; frame++)
        {
            for (unsigned int i = 0; i < iqmHeader->num_poses; i++)
            {
                animations[a].framePoses[frame][i].translation.x = poses[i].channeloffset[0];

                if (poses[i].mask & 0x01)
                {
                    animations[a].framePoses[frame][i].translation.x += framedata[dcounter]*poses[i].channelscale[0];
                    dcounter++;
                }

                animations[a].framePoses[frame][i].translation.y = poses[i].channeloffset[1];

                if (poses[i].mask & 0x02)
                {
                    animations[a].framePoses[frame][i].translation.y += framedata[dcounter]*poses[i].channelscale[1];
                    dcounter++;
                }

                animations[a].framePoses[frame][i].translation.z = poses[i].channeloffset[2];

                if (poses[i].mask & 0x04)
                {
                    animations[a].framePoses[frame][i].translation.z += framedata[dcounter]*poses[i].channelscale[2];
                    dcounter++;
                }

                animations[a].framePoses[frame][i].rotation.x = poses[i].channeloffset[3];

                if (poses[i].mask & 0x08)
                {
                    animations[a].framePoses[frame][i].rotation.x += framedata[dcounter]*poses[i].channelscale[3];
                    dcounter++;
                }

                animations[a].framePoses[frame][i].rotation.y = poses[i].channeloffset[4];

                if (poses[i].mask & 0x10)
                {
                    animations[a].framePoses[frame][i].rotation.y += framedata[dcounter]*poses[i].channelscale[4];
                    dcounter++;
                }

                animations[a].framePoses[frame][i].rotation.z = poses[i].channeloffset[5];

                if (poses[i].mask & 0x20)
                {
                    animations[a].framePoses[frame][i].rotation.z += framedata[dcounter]*poses[i].channelscale[5];
                    dcounter++;
                }

                animations[a].framePoses[frame][i].rotation.w = poses[i].channeloffset[6];

                if (poses[i].mask & 0x40)
                {
                    animations[a].framePoses[frame][i].rotation.w += framedata[dcounter]*poses[i].channelscale[6];
                    dcounter++;
                }

                animations[a].framePoses[frame][i].scale.x = poses[i].channeloffset[7];

                if (poses[i].mask & 0x80)
                {
                    animations[a].framePoses[frame][i].scale.x += framedata[dcounter]*poses[i].channelscale[7];
                    dcounter++;
                }

                animations[a].framePoses[frame][i].scale.y = poses[i].channeloffset[8];

                if (poses[i].mask & 0x100)
                {
                    animations[a].framePoses[frame][i].scale.y += framedata[dcounter]*poses[i].channelscale[8];
                    dcounter++;
                }

                animations[a].framePoses[frame][i].scale.z = poses[i].channeloffset[9];

                if (poses[i].mask & 0x200)
                {
                    animations[a].framePoses[frame][i].scale.z += framedata[dcounter]*poses[i].channelscale[9];
                    dcounter++;
                }

                animations[a].framePoses[frame][i].rotation = QuaternionNormalize(animations[a].framePoses[frame][i].rotation);
            }
        }

        // Build frameposes
        /*
        for (unsigned int frame = 0; frame < anim[a].num_frames; frame++)
        {
            for (int i = 0; i < animations[a].boneCount; i++)
            {
                if (animations[a].bones[i].parent >= 0)
                {
                    animations[a].framePoses[frame][i].rotation = QuaternionMultiply(animations[a].framePoses[frame][animations[a].bones[i].parent].rotation, animations[a].framePoses[frame][i].rotation);
                    animations[a].framePoses[frame][i].translation = Vector3RotateByQuaternion(animations[a].framePoses[frame][i].translation, animations[a].framePoses[frame][animations[a].bones[i].parent].rotation);
                    animations[a].framePoses[frame][i].translation = Vector3Add(animations[a].framePoses[frame][i].translation, animations[a].framePoses[frame][animations[a].bones[i].parent].translation);
                    animations[a].framePoses[frame][i].scale = Vector3Multiply(animations[a].framePoses[frame][i].scale, animations[a].framePoses[frame][animations[a].bones[i].parent].scale);
                }
            }
        }
        */
    }

    UnloadFileData(fileData);

    RL_FREE(joints);
    RL_FREE(framedata);
    RL_FREE(poses);
    RL_FREE(anim);

    return animations;
}
