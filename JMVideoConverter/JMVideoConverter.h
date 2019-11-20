//
//  JMVideoConverter.h
//  JMVideoConverter
//
//  Created by YaoHua Tan on 2019/10/17.
//  Copyright © 2019 YaoHua Tan. All rights reserved.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface JMVideoConverter : NSObject

/**
 视频转码成H264+AAC数据流文件

 @param inFilePath 视频输入路径
 @param outFilePath 视频输出路径
 @return YES：开始转码
 */
- (BOOL)startVideoConverter:(NSString *)inFilePath outFilePath:(NSString *)outFilePath;
+ (BOOL)startVideoConverter:(NSString *)inFilePath outFilePath:(NSString *)outFilePath;
+ (BOOL)startVideoForceConverter:(NSString *)inFilePath outFilePath:(NSString *)outFilePath;    //强制转码

+ (void)stopVideoConverter;

@end

NS_ASSUME_NONNULL_END
