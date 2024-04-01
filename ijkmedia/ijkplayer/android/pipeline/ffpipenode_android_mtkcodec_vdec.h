
#ifndef FFPLAY__FF_FFPIPENODE_ANDROID_MTKCODEC_VDEC_H
#define FFPLAY__FF_FFPIPENODE_ANDROID_MTKCODEC_VDEC_H

#include "../../ff_ffpipenode.h"
#include "../../ff_ffpipeline.h"
#include "ijksdl/ijksdl_vout.h"

typedef struct FFPlayer FFPlayer;

IJKFF_Pipenode *ffpipenode_create_video_decoder_from_android_mtkcodec(FFPlayer *ffp);
SDL_Aout *SDL_AoutAndroid_CreateForMtkCodec();

#endif
