//
//  GlobalData.h
//  IOTCamera
//
//  Created by lizhijian on 16/6/13.
//
//

#ifndef GlobalData_h
#define GlobalData_h
#include <stdio.h>
#import "avcodec.h"
#import "avformat.h"

#ifdef DEBUG
#define cv_printf(fmt...) printf(fmt)
#define cv_fprintf(fmt...) fprintf(fmt)
#else
#define cv_printf(fmt...)
#define cv_fprintf(fmt...)
#endif

typedef enum {
    VideoFrameType_UNKNOWN = 0,
    VideoFrameType_I,
    VideoFrameType_P,
    VideoFrameType_B,
    VideoFrameType_SEI,
}VideoFrameType;


/**
 打印数据

 @param data 数据
 @param len 查询长度
 */
extern void printfVideoFrameData(const uint8_t *data, int len);

/**
 获取视频帧类型

 @param p 视频数据
 @param len 视频长度
 @return VideoFrameType
 */
extern VideoFrameType FFmpeg_GetVideoFrameType(const void *p, int len);    //获取视频帧属于哪一类型

/**
 获取整流器BSF

 @param streams 媒体流AVStream
 @param name 整流器名称
 @return 整流器BSF，使用完之后需要调用av_bsf_free释放
 */
extern AVBSFContext *FFmpeg_GetBSFContext(AVStream *streams, const char *name);

/**
 释放整流器BSF

 @param bsfc 整流器地址
 */
extern void FFmpeg_FreeBSFContext(AVBSFContext **bsfc);

/**
 导入extradata数据及大小
 #不需要使用时需要调用FFmpeg_freeExtradata(AVCodecParameters *)
 
 @param codecpar AVStream->codecpar
 @param codecCtx AVCodecContext，如其中的extradata不存在数据可能avcodec_open2
 @return 0：success； <0：fail
 */
extern int FFmpeg_AddExtradata(AVCodecParameters *codecpar, const AVCodecContext *codecCtx);

extern int FFmpeg_AddExtradata2(AVCodecParameters *codecpar, const uint8_t *buf, int length);

extern int FFmpeg_AddAACExtradata(AVCodecParameters *codecpar, int profileType, int sampleRate, int channels);
/**
 释放AVCodecParameters中extradata数据

 @param codecpar AVCodecParameters
 */
extern void FFmpeg_FreeExtradata(AVCodecParameters *codecpar);

/**
 AAC添加ADTS头信息

 @param packet 数据包地址
 @param packetLen 数据长度(原始数据长度)
 @param profileType 音频profile类型（暂时只支持AAC Main:1,LC:2,SSR:3,LTP:4）
 @param sampleRate 采样率
 @param channel 通道数
 */
extern void FFmpeg_AddADTS(char *packet, int packetLen, int profileType, int sampleRate, int channel);

#endif /* GlobalData_h */
