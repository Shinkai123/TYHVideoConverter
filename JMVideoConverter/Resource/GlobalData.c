//
//  GlobalData.c
//  IOTCamera
//
//  Created by lizhijian on 2017/3/7.
//  Copyright © 2017年 concox. All rights reserved.
//

#include "GlobalData.h"

void printfVideoFrameData(const uint8_t *data, int len)
{
    if (!data) {
        return;
    }
    
    cv_printf("\n");
    for (int i=0; i<len; i++) {
        cv_printf("%02x ", data[i]);
    }
    cv_printf("\n");
}

VideoFrameType FFmpeg_GetVideoFrameType(const void *p, int len)
{
    if (!p || len <= 6)
        return VideoFrameType_UNKNOWN;
    
    int index = 0;
    unsigned char *b = (unsigned char*)p;
    
    if (b[0] == 0 && b[1] == 0 && b[2] == 0x01) {
        index += 3;
    } else if (b[0] == 0 && b[1] == 0 && b[2] == 0 && b[3] == 0x01) {
        index += 4;
    } else {
        return VideoFrameType_UNKNOWN;
    }
    
    switch( b[index] ) {
        case 0x6:
        {
            if (b[index+1] == 0x5) {
                return VideoFrameType_SEI;
            }
        }
            break;
        case 0x67:
        case 0x65:
        case 0x27:
            return VideoFrameType_I;
        case 0x61:
        case 0x41:
        case 0x21:
            return VideoFrameType_P;
        case 0x01:
            return VideoFrameType_B;
        default:
        {
            printf("Unknown vop type : %x\n", b[index]);
            return VideoFrameType_UNKNOWN;
        }
    }
    
    return VideoFrameType_UNKNOWN;
}

AVBSFContext *FFmpeg_GetBSFContext(AVStream *streams, const char *name)
{
    if (streams == NULL || name == NULL) return NULL;
    
    AVBSFContext *bsf = NULL;
    int ret = av_bsf_alloc(av_bsf_get_by_name(name), &bsf);
    if (ret < 0) {
        printf("Could not allac audio bsf context.\n");
        av_bsf_free(&bsf);
        return NULL;
    }
    avcodec_parameters_copy(bsf->par_in, streams->codecpar);
    if (ret < 0) {
        printf("Could not copy h264 bsf parameters:1.\n");
        av_bsf_free(&bsf);
        return NULL;
    }
    bsf->time_base_in = streams->time_base;
    
    av_bsf_init(bsf);
    if (ret < 0) {
        printf("Could not init h264 bsf.\n");
        av_bsf_free(&bsf);
        return NULL;
    }
    
    ret = avcodec_parameters_copy(streams->codecpar, bsf->par_out);
    if (ret < 0) {
        printf("Could not copy h264 bsf parameters:2.\n");
        av_bsf_free(&bsf);
        return NULL;
    }
    streams->time_base = bsf->time_base_out;
    
    return bsf;
}

void FFmpeg_FreeBSFContext(AVBSFContext **bsfc)
{
    if (bsfc && *bsfc) {
        av_bsf_free(bsfc);
        *bsfc = NULL;
    }
}

int FFmpeg_AddExtradata(AVCodecParameters *codecpar, const AVCodecContext *codecCtx)
{
    if (codecpar && codecCtx && codecCtx->extradata && codecCtx->extradata_size > 0) {
        if (codecpar->extradata && codecpar->extradata_size > 0) {
            av_free(codecpar->extradata);
        }
        
        codecpar->extradata_size = codecCtx->extradata_size;
        codecpar->extradata = av_malloc(codecCtx->extradata_size + AV_INPUT_BUFFER_PADDING_SIZE);
        memset(codecpar->extradata, 0, codecCtx->extradata_size + AV_INPUT_BUFFER_PADDING_SIZE);
        memcpy(codecpar->extradata, codecCtx->extradata, codecCtx->extradata_size);
        
        return 0;
    }
    
    return -1;
}

int FFmpeg_AddExtradata2(AVCodecParameters *codecpar, const uint8_t *buf, int length)
{
    if (!codecpar || !buf || length < AV_INPUT_BUFFER_PADDING_SIZE*2) return -1;
    
    for (int i = 0; i < AV_INPUT_BUFFER_PADDING_SIZE * 2 && i < length; i++) {
        if ((buf[i] == 0x00 && buf[i+1] == 0x00 && buf[i+2] == 0x00 && buf[i+3] == 0x01 && buf[i+4] == 0x65) ||
            (buf[i] == 0x00 && buf[i+1] == 0x00 && buf[i+2] == 0x01 && buf[i+3] == 0x6 && buf[i+4] == 0x5)) {
            if (i > 0) {
                if (codecpar->extradata && codecpar->extradata_size > 0) {
                    av_free(codecpar->extradata);
                }
                codecpar->extradata = av_malloc(AV_INPUT_BUFFER_PADDING_SIZE * 2);
                
                memcpy(codecpar->extradata, (const void *)buf, i+1);
                codecpar->extradata_size = i + 1;
                printf("Wtite sps and pps to extradata success!\n");
            } else {
                printf("Wtite sps and pps to extradata failed!\n");
            }
            return 0;
        }
    }
    
    return -1;
}

int FFmpeg_AddAACExtradata(AVCodecParameters *codecpar, int profileType, int sampleRate, int channels)
{
    if (!codecpar) return -1;
    
    if (!codecpar->extradata) {
        codecpar->extradata = av_malloc(AV_INPUT_BUFFER_PADDING_SIZE * 2);
        codecpar->extradata_size = 2;
    }
    
    int profile = 1; // AAC Main:1,LC:2,SSR:3,LTP:4,SBR:5
    if (profileType == FF_PROFILE_AAC_MAIN) profile = 1;
    else if (profileType == FF_PROFILE_AAC_LOW) profile = 2;
    else if (profileType == FF_PROFILE_AAC_SSR) profile = 3;
    else if (profileType == FF_PROFILE_AAC_LTP) profile = 4;
    
    int freqIdx = 0x0;
    if (sampleRate == 7350) freqIdx = 0xc;
    else if (sampleRate == 8000) freqIdx = 0xb;
    else if (sampleRate == 11025) freqIdx = 0xa;
    else if (sampleRate == 12000) freqIdx = 0x9;
    else if (sampleRate == 16000) freqIdx = 0x8;
    else if (sampleRate == 22050) freqIdx = 0x7;
    else if (sampleRate == 24000) freqIdx = 0x6;
    else if (sampleRate == 32000) freqIdx = 0x5;
    else if (sampleRate == 44100) freqIdx = 0x4;
    else if (sampleRate == 48000) freqIdx = 0x3;
    else if (sampleRate == 64000) freqIdx = 0x2;
    else if (sampleRate == 88200) freqIdx = 0x1;
    else if (sampleRate == 96000) freqIdx = 0x0;
    
    codecpar->extradata[0] = (profile << 3) | (freqIdx >> 1);
    codecpar->extradata[1] = ((freqIdx & 1) << 7) | (channels << 3);
    
    return -1;
}

void FFmpeg_FreeExtradata(AVCodecParameters *codecpar)
{
    if (codecpar && codecpar->extradata && codecpar->extradata_size > 0) {
        av_free(codecpar->extradata);
        codecpar->extradata = NULL;
        codecpar->extradata_size = 0;
    }
}

void FFmpeg_AddADTS(char *packet, int packetLen, int profileType, int sampleRate, int channel)
{
    int profile = 1; // AAC Main:1,LC:2,SSR:3,LTP:4,SBR:5
    if (profileType == FF_PROFILE_AAC_MAIN) profile = 1;
    else if (profileType == FF_PROFILE_AAC_LOW) profile = 1;
    else if (profileType == FF_PROFILE_AAC_SSR) profile = 3;
    else if (profileType == FF_PROFILE_AAC_LTP) profile = 4;
    
    int freqIdx = 0x0;
    if (sampleRate == 7350) freqIdx = 0xc;
    else if (sampleRate == 8000) freqIdx = 0xb;
    else if (sampleRate == 11025) freqIdx = 0xa;
    else if (sampleRate == 12000) freqIdx = 0x9;
    else if (sampleRate == 16000) freqIdx = 0x8;
    else if (sampleRate == 22050) freqIdx = 0x7;
    else if (sampleRate == 24000) freqIdx = 0x6;
    else if (sampleRate == 32000) freqIdx = 0x5;
    else if (sampleRate == 44100) freqIdx = 0x4;
    else if (sampleRate == 48000) freqIdx = 0x3;
    else if (sampleRate == 64000) freqIdx = 0x2;
    else if (sampleRate == 88200) freqIdx = 0x1;
    else if (sampleRate == 96000) freqIdx = 0x0;
    
    int chanCfg = channel;
    packetLen += 7;
    
    packet[0] = 0xFF;
    packet[1] = 0xF9;
    packet[2] = (((profile - 1) << 6) + (freqIdx << 2) + (chanCfg >> 2));
    packet[3] = (((chanCfg & 3) << 6) + (packetLen >> 11));
    packet[4] = ((packetLen & 0x7FF) >> 3);
    packet[5] = (((packetLen & 7) << 5) + 0x1F);
    packet[6] = 0xFC;
}
