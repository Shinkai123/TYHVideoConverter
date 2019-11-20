//
//  JMVideoConverter.m
//  JMVideoConverter
//
//  Created by YaoHua Tan on 2019/10/17.
//  Copyright © 2019 YaoHua Tan. All rights reserved.
//

#import "JMVideoConverter.h"
#import "FFmepgVideoConverter.h"

@implementation JMVideoConverter

+ (BOOL)startVideoConverter:(NSString *)inFilePath outFilePath:(NSString *)outFilePath
{
    int ret = FFmpeg_VideoConverterToMP4([inFilePath UTF8String], [outFilePath UTF8String]);
    
    return ret==0?YES:NO;
}

+ (BOOL)startVideoForceConverter:(NSString *)inFilePath outFilePath:(NSString *)outFilePath
{
    int ret = FFmpeg_VideoForceConverterToMP4([inFilePath UTF8String], [outFilePath UTF8String]);
    
    return ret==0?YES:NO;
}

- (BOOL)startVideoConverter:(NSString *)inFilePath outFilePath:(NSString *)outFilePath
{
    return [JMVideoConverter startVideoConverter:inFilePath outFilePath:outFilePath];
}


+ (void)stopVideoConverter {
    FFmpeg_VideoConverterRelease();
}

@end
