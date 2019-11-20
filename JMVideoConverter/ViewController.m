//
//  ViewController.m
//  JMVideoConverter
//
//  Created by YaoHua Tan on 2019/10/17.
//  Copyright © 2019 YaoHua Tan. All rights reserved.
//

#import "ViewController.h"
#import "JMVideoConverter.h"

@interface ViewController ()

@end

@implementation ViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    // Do any additional setup after loading the view.
    self.view.backgroundColor = [UIColor whiteColor];
    
    UIButton *btn = [UIButton buttonWithType:UIButtonTypeCustom];
    btn.frame = CGRectMake(100, 100, 200, 50);
    btn.backgroundColor = [UIColor blueColor];
    btn.titleLabel.textColor = [UIColor whiteColor];
    [btn setTitle:@"starConverter" forState:UIControlStateNormal];
    [btn addTarget:self action:@selector(testConverter) forControlEvents:UIControlEventTouchUpInside];
    [self.view addSubview:btn];
    
    UIButton *stopBtn = [UIButton buttonWithType:UIButtonTypeCustom];
    stopBtn.frame = CGRectMake(100, 200, 200, 50);
    stopBtn.backgroundColor = [UIColor blueColor];
    stopBtn.titleLabel.textColor = [UIColor whiteColor];
    [stopBtn setTitle:@"stopConverter" forState:UIControlStateNormal];
    [stopBtn addTarget:self action:@selector(testStopConverter) forControlEvents:UIControlEventTouchUpInside];
    [self.view addSubview:stopBtn];
}

- (void)testConverter {
    NSString *pathStr = [[NSBundle mainBundle] pathForResource:@"2019_10_17_18_28_10" ofType:@".avi"];
    BOOL ret = 1;
    NSString *videoCodeName = videoCodeName = [NSString stringWithFormat:@"%@123.mp4", [self getPathForDocumentsResourceDic:@"/VideoDicTemp/"]];
     ret = [JMVideoConverter startVideoForceConverter:pathStr outFilePath:videoCodeName];//视频转码成H264+AAC数据流文件
    NSLog(@"reuslt->>>>>>>%d",ret);
}

- (void)testStopConverter {
    [JMVideoConverter stopVideoConverter];
}

//获取沙盒文档中文件夹相对路径，如果没有则创建
- (NSString *)getPathForDocumentsResourceDic:(NSString *)relativePathDic
{
    NSString *documentsPath = [NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) objectAtIndex:0];
    
    NSString *str = [documentsPath stringByAppendingString:relativePathDic];
    if (![[NSFileManager defaultManager] fileExistsAtPath:str]) {
        [[NSFileManager defaultManager] createDirectoryAtPath:str withIntermediateDirectories:YES attributes:nil error:nil];
    }
    
    return str;
}


@end
