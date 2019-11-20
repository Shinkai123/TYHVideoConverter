//
//  FFmepgVideoConverter.h
//  JMVideoConverter
//
//  Created by shinkai on 2019/10/17.
//  Copyright © 2019年 concox. All rights reserved.
//

#ifndef FFmepgVideoConverter_h
#define FFmepgVideoConverter_h

#include <stdio.h>

/**
 阻塞试转码为MP4文件（H264+AAC）

 @param inFilePath 转码前视频文件路径
 @param outFilePath 转码后视频保存路径，文件后缀名必须是mp4
 @return 0:成功，否则转码失败
 */
int FFmpeg_VideoConverterToMP4(const char *inFilePath, const char *outFilePath);


/**
 阻塞试强制转码为MP4文件（H264+AAC）
 
 @param inFilePath 转码前视频文件路径
 @param outFilePath 转码后视频保存路径，文件后缀名必须是mp4
 @return 0:成功，否则转码失败
 */
int FFmpeg_VideoForceConverterToMP4(const char *inFilePath, const char *outFilePath);

/**
 读取音视频文件格式

 @param filename 文件路FFmpeg_VideoConverterToMP4的数组（数组大小最好大于10）
 @param codecCount 接收格式的数组个数
 @return 0：成功，<0：读取失败
 */
int read_file_codec(const char *filename, int *formatList, int *codecCount);

void FFmpeg_VideoConverterRelease();

#endif /* FFmepgVideoConverter_h */
