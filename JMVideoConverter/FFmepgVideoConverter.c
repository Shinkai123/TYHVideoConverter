//
//  FFmepgVideoConverter.c
//  JMVideoConverter
//
//  Created by shinkai on 2019/10/17.
//  Copyright © 2019年 concox. All rights reserved.
//

#include "FFmepgVideoConverter.h"
#include "GlobalData.h"
#include "avformat.h"
#include "avfilter.h"
#include "opt.h"
#include "mathematics.h"
#include "buffersrc.h"
#include "buffersink.h"
#include "avcodec.h"
#include "swresample.h"
#import<AudioToolbox/AudioToolbox.h>
#import<VideoToolbox/VideoToolbox.h>

typedef struct {
    AVFormatContext *formtCtx;//描述一个媒体文件或媒体流的构成和基本信息
    AVStream *videoStream; //视频流
    AVStream *audioStream; //音频流
    AVCodecContext *videoCodecCtx; //视频上下文
    AVCodecContext *audioCodecCtx; //音频上下文
} VideoFileContent;

typedef struct {
    AVFilterContext *bufferSinCtx;
    AVFilterContext *bufferSrcCtx;
    AVFilterGraph *filterGraph;
} VideoFilteringContext;

VideoFileContent *inFileCon = NULL;
VideoFileContent *outFileCon = NULL;
VideoFilteringContext *filterCtx = NULL;
SwrContext *swrCtx_Converter = NULL;
AVBSFContext *h264bsf_Converter = NULL;
AVBSFContext *aacbsf_Converter = NULL;
int video_st_index_converter = -1;
int audio_st_index_converter = -1;
int64_t audio_pre_pts_converter = -1024;
int64_t video_pre_pts_converter = 0;
double video_first_timestamp_converter = -0.01,audio_first_timestamp_converter = -0.01;
int isWaitKey_converter = 1;
int isUseAACBsf_converter = 1;
int isForce_converter = 0;

void FFmpeg_VideoConverterRelease(void);

int FFmpeg_VideoConverterOpenInputFile(const char *filename) {
    int ret;
    
    ret = avformat_open_input(&inFileCon->formtCtx, filename, NULL, NULL);
    if (ret < 0) {
        printf("Cannot openinput file:%s->>ret:%d\n", filename,ret);
        return ret;
    }
    
    ret = avformat_find_stream_info(inFileCon->formtCtx, NULL);
    if (ret < 0) {
        printf("Cannot findstream information->>ret:%d\n",ret);
        return ret;
    }
    
    for (int i = 0; i<inFileCon->formtCtx->nb_streams; i++) {
        AVStream *stream = inFileCon->formtCtx->streams[i];
        /* Reencode video & audio and remux subtitles etc. */
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            inFileCon->videoStream = stream;
            video_st_index_converter = i;
            printf("video_st_index_converter:%u->>ret:%d\n", i,ret);
            
            AVCodec *pAVCodec = avcodec_find_decoder(stream->codecpar->codec_id);
            //申请AVCodecContext空间。需要传递一个编码器，也可以不传，但不会包含编码器(配置上下文)
            AVCodecContext *pAVCodecCtx = avcodec_alloc_context3(pAVCodec);
            //将视频流信息拷贝到新的AVCodecContext结构体中
            avcodec_parameters_to_context(pAVCodecCtx, stream->codecpar);
            inFileCon->videoCodecCtx = pAVCodecCtx;
            
            ret = avcodec_open2(pAVCodecCtx, pAVCodec, NULL);
            if (ret < 0) {
                printf("Failed to open decoder for stream #%u->>ret:%d\n", i,ret);
                return ret;
            }
        } else if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            inFileCon->audioStream = stream;
            audio_st_index_converter = i;
            printf("audio_st_index_converter:%u->>ret:%d\n", i,ret);
            
            AVCodec *pAVcodec = avcodec_find_decoder(stream->codecpar->codec_id);
            AVCodecContext *pAVCodeCtx = avcodec_alloc_context3(pAVcodec);
            avcodec_parameters_to_context(pAVCodeCtx, stream->codecpar);
            inFileCon->audioCodecCtx = pAVCodeCtx;
            
            ret = avcodec_open2(pAVCodeCtx, pAVcodec, NULL);
            if (ret < 0) {
                printf("Failed to open decoder for stream #%u->>ret:%d\n", i,ret);
                return ret;
            }
        }
    }
    av_dump_format(inFileCon->formtCtx, 0, filename, 0);
    
    return 0;
}

AVStream *FFmpeg_VideoConverterAddVideoStream() {
    if (video_st_index_converter < 0) {
        return NULL;
    }
    AVStream *in_stream = inFileCon->formtCtx->streams[video_st_index_converter];
    AVCodec *codec = avcodec_find_encoder(outFileCon->formtCtx->oformat->video_codec);
    if (!codec) {
        return NULL;
    }
    AVStream *out_stream = avformat_new_stream(outFileCon->formtCtx, codec);
    if (!out_stream) {
        printf("Faile dallocating output video stream\n");
        return NULL;
    }
    
    AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
    codec_ctx->codec_tag = 0;
    codec_ctx->time_base.num = in_stream->time_base.num;
    codec_ctx->time_base.den = in_stream->time_base.den;
    codec_ctx->codec_id = AV_CODEC_ID_H264;
    codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
    codec_ctx->height = in_stream->codecpar->height;
    codec_ctx->width = in_stream->codecpar->width;
    codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    codec_ctx->color_range = AVCOL_RANGE_MPEG;
    codec_ctx->bit_rate = in_stream->codecpar->bit_rate;
    codec_ctx->qmin = 3;
    codec_ctx->qmax = 51;
    codec_ctx->qcompress = 0.5;
    
    if (outFileCon->formtCtx->oformat->flags & AVFMT_GLOBALHEADER)
        codec_ctx->flags |= CODEC_FLAG_GLOBAL_HEADER;
    
    out_stream->index = 0;
    out_stream->time_base.num = in_stream->time_base.num;
    out_stream->time_base.den = in_stream->time_base.den;
    av_stream_set_r_frame_rate(out_stream, codec_ctx->time_base);
    avcodec_parameters_from_context(out_stream->codecpar, codec_ctx);
    outFileCon->videoCodecCtx = codec_ctx;
    
    int ret = avcodec_open2(codec_ctx, codec, NULL);
    if (ret < 0) {
        printf("Failed to open encoder for video stream, ret:%d\n", ret);
        return NULL;
    }
    if (in_stream->codecpar->codec_id == out_stream->codecpar->codec_id) {
        h264bsf_Converter = FFmpeg_GetBSFContext(in_stream, "h264_mp4toannexb");
        if (h264bsf_Converter == NULL) {
            printf("Init input video filter failed, ret:%d\n", ret);
            return NULL;
        }
    } else {
        h264bsf_Converter = FFmpeg_GetBSFContext(out_stream, "h264_mp4toannexb");
        if (h264bsf_Converter == NULL) {
            printf("Init input video filter failed, ret:%d\n", ret);
            return NULL;
        }
    }
    ret = FFmpeg_AddExtradata(out_stream->codecpar, codec_ctx);
    if (ret < 0) {
        printf("Failed to get audio extradata");
        return NULL;
    }
    return out_stream;
}

int FFmpeg_VideoConverterInitAudioFilters(VideoFilteringContext *fctx, AVCodecContext *codecCtx, const char *filter_spec)
{
    char args[512];
    int ret = 0;
    AVFilter *buffersrc = NULL;
    AVFilter *buffersink = NULL;
    AVFilterContext *buffersrc_ctx = NULL;
    AVFilterContext *buffersink_ctx = NULL;
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs = avfilter_inout_alloc();
    AVFilterGraph *filter_graph = avfilter_graph_alloc();
    if (!outputs || !inputs || !filter_graph) {
        ret = AVERROR(ENOMEM);
        goto end;
    }
    
    if (codecCtx->codec_type == AVMEDIA_TYPE_AUDIO) {
        buffersrc = avfilter_get_by_name("abuffer");
        buffersink = avfilter_get_by_name("abuffersink");
        if (!buffersrc || !buffersink) {
            printf("filtering source or sink element not found\n");
            ret = -1;
            goto end;
        }
        
        if (!codecCtx->channel_layout) {
            codecCtx->channel_layout = av_get_default_channel_layout(codecCtx->channels);
        }
        
        snprintf(args, sizeof(args),
                 "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=%llu",
                 codecCtx->time_base.num, codecCtx->time_base.den, codecCtx->sample_rate,
                 av_get_sample_fmt_name(codecCtx->sample_fmt), codecCtx->channel_layout);
        
        ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in", args, NULL, filter_graph);
        if (ret < 0) {
            printf("Cannot create audio buffer source\n");
            goto end;
        }
        
        ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out", NULL, NULL, filter_graph);
        if (ret < 0) {
            printf("Cannot create audio buffer sink\n");
            goto end;
        }
        
        ret = av_opt_set_bin(buffersink_ctx, "sample_fmts", (unsigned char *) &codecCtx->sample_fmt, sizeof(codecCtx->sample_fmt), AV_OPT_SEARCH_CHILDREN);
        if (ret < 0) {
            printf("Cannot setoutput sample format\n");
            goto end;
        }
        
        ret = av_opt_set_bin(buffersink_ctx, "channel_layouts", (unsigned char *) &codecCtx->channel_layout, sizeof(codecCtx->channel_layout), AV_OPT_SEARCH_CHILDREN);
        if (ret < 0) {
            printf("Cannot setoutput channel layout\n");
            goto end;
        }
        
        ret = av_opt_set_bin(buffersink_ctx, "sample_rates", (unsigned char *) &codecCtx->sample_rate, sizeof(codecCtx->sample_rate),  AV_OPT_SEARCH_CHILDREN);
        if (ret < 0) {
            printf("Cannot setoutput sample rate\n");
            goto end;
        }
    } else {
        ret = -1;
        goto end;
    }
    
    /* Endpoints for the filter graph. */
    outputs->name = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx = 0;
    outputs->next = NULL;
    inputs->name = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx = 0;
    inputs->next = NULL;
    if (!outputs->name || !inputs->name) {
        ret = AVERROR(ENOMEM);
        goto end;
    }
    if ((ret = avfilter_graph_parse_ptr(filter_graph, filter_spec, &inputs, &outputs, NULL)) < 0)
        goto end;
    
    if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0)
        goto end;
    
    av_buffersink_set_frame_size(buffersink_ctx, codecCtx->frame_size);
    
    /* Fill FilteringContext */
    fctx->bufferSrcCtx = buffersrc_ctx;
    fctx->bufferSinCtx= buffersink_ctx;
    fctx->filterGraph = filter_graph;
    
end:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
    return ret;
}


int FFmpeg_VideoConverterAddAudioFilters()
{
    filterCtx = (VideoFilteringContext *)av_malloc_array(1, sizeof(VideoFilteringContext));
    if (!filterCtx)
        return AVERROR(ENOMEM);
    
    filterCtx->bufferSrcCtx = NULL;
    filterCtx->bufferSinCtx = NULL;
    filterCtx->filterGraph = NULL;
    
    const char*filter_spec = "anull"; /* passthrough (dummy) filter for audio */
    int ret = FFmpeg_VideoConverterInitAudioFilters(filterCtx, outFileCon->audioCodecCtx, filter_spec);
    if (ret) {
        return ret;
    }
    
    return 0;
}

AVStream *FFmpeg_VideoConverterAddAudioStream()
{
    if (audio_st_index_converter < 0) {
        return NULL;
    }
    int isUselibfdk_aac = 0;
    AVStream *in_stream = inFileCon->formtCtx->streams[audio_st_index_converter];
    
    AVCodec *codec = NULL;
//    if (in_stream->codecpar->format == AV_SAMPLE_FMT_S16) {
//        codec = avcodec_find_encoder_by_name("libfdk_aac");       //声音有问题
    //        isUselibfdk_aac = 1;
//    }
    if (codec == NULL) {
        codec = avcodec_find_encoder(outFileCon->formtCtx->oformat->audio_codec);
        isUselibfdk_aac = 0;
        if (!codec) {
            printf("Faile find output audio codec\n");
            return NULL;
        }
        
        if (in_stream->codecpar->format != AV_SAMPLE_FMT_FLTP) {     //需要重采样
            if (in_stream->codecpar->channel_layout == 0) {
                in_stream->codecpar->channel_layout = av_get_default_channel_layout(in_stream->codecpar->channels);
            }
            swrCtx_Converter = swr_alloc_set_opts(swrCtx_Converter,
                                                  in_stream->codecpar->channel_layout, AV_SAMPLE_FMT_FLTP, in_stream->codecpar->sample_rate,
                                                  in_stream->codecpar->channel_layout, in_stream->codecpar->format, in_stream->codecpar->sample_rate,
                                                  0, NULL);
            int ret = swr_init(swrCtx_Converter);
            if (ret < 0) {
                printf("Could not init swr context, ret:%d\n", ret);
                return NULL;
            }
        }
    }
    
    AVStream *out_stream = avformat_new_stream(outFileCon->formtCtx, codec);
    if (!out_stream) {
        printf("Faile dallocating output audio stream\n");
        return NULL;
    }
    
    AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
    codec_ctx->codec_tag = 0;
    codec_ctx->time_base.num = in_stream->time_base.num;
    codec_ctx->time_base.den = in_stream->time_base.den;
    if (isUselibfdk_aac) {
        codec_ctx->sample_fmt = AV_SAMPLE_FMT_S16;
    } else {
        codec_ctx->sample_fmt = AV_SAMPLE_FMT_FLTP;
    }
    codec_ctx->codec_type = AVMEDIA_TYPE_AUDIO;
    codec_ctx->sample_rate = in_stream->codecpar->sample_rate;
    codec_ctx->channels = in_stream->codecpar->channels;
    codec_ctx->channel_layout = av_get_default_channel_layout(in_stream->codecpar->channels);
    codec_ctx->bit_rate = in_stream->codecpar->bit_rate;
    codec_ctx->frame_size = 1024;
    codec_ctx->profile = in_stream->codecpar->profile;
    
    if (outFileCon->formtCtx->oformat->flags & AVFMT_GLOBALHEADER)
        codec_ctx->flags |= CODEC_FLAG_GLOBAL_HEADER;
    
    out_stream->index = 1;
    out_stream->time_base.num = in_stream->time_base.num;
    out_stream->time_base.den = in_stream->time_base.den;
    av_stream_set_r_frame_rate(out_stream, codec_ctx->time_base);
    avcodec_parameters_from_context(out_stream->codecpar, codec_ctx);
    outFileCon->audioCodecCtx = codec_ctx;
    
    if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
        printf("Failed to open encoder!!!\n");
        return NULL;
    }
    
    aacbsf_Converter = FFmpeg_GetBSFContext(out_stream, "aac_adtstoasc");
    if (aacbsf_Converter == NULL) {
        printf("Init aac bsf context fail!\n");
        return NULL;
    }
    
    if (FFmpeg_AddExtradata(out_stream->codecpar, codec_ctx) < 0) {
        printf("Failed to get audio extradata");
        return NULL;
    }
    
    if (!isUselibfdk_aac && in_stream->codecpar->format != AV_SAMPLE_FMT_FLTP && FFmpeg_VideoConverterAddAudioFilters() < 0) {
        return NULL;
    }
    
    return out_stream;
}

int FFmpeg_VideoConverterOpenOutputFile(const char *filename) {
    int ret = 0;
    //输出文件格式
    AVOutputFormat *outputFmt = av_guess_format(NULL, filename, NULL);
    if (outputFmt == NULL) {
        printf("Could not identify the format of this file\n");
        return -1;
    }
    outputFmt->video_codec = AV_CODEC_ID_H264;
    outputFmt->audio_codec = AV_CODEC_ID_AAC;
    
    ret = avformat_alloc_output_context2(&outFileCon->formtCtx, NULL, NULL, filename);
    if (ret < 0 || !outFileCon->formtCtx) {
        printf("Could not create output context\n");
        return -2;
    }
    outFileCon->formtCtx->oformat = outputFmt;
    strcpy(outFileCon->formtCtx->filename, filename);
    
    outFileCon->videoStream = FFmpeg_VideoConverterAddVideoStream();
    if (!outFileCon->videoStream) {
        return -2;
    }
    
    if (audio_st_index_converter >= 0) {
        outFileCon->audioStream = FFmpeg_VideoConverterAddAudioStream();
        if (!outFileCon->audioStream) {
            return -3;
        }
    }
    
    av_dump_format(outFileCon->formtCtx, 0, filename, 1);
    
    ret = avio_open(&outFileCon->formtCtx->pb, filename, AVIO_FLAG_READ_WRITE);
    if (ret < 0) {
        printf("Could not open output file '%s'", filename);
        return ret;
    }
    
    /* init muxer, write output file header */
    ret = avformat_write_header(outFileCon->formtCtx, NULL);
    if (ret < 0) {
        printf("Error occurred when opening output file\n");
        return ret;
    }
    
    return 0;
}

int FFmpeg_VideoConverterWriteAudioFrame(AVFrame *frame, AVPacket *pPkt) {
    AVPacket *pkt = NULL;
    if (pPkt) {
        pkt = av_packet_clone(pPkt);
    } else {
        pkt = av_packet_alloc();
    }
    
    int ret = -1;
    if (pPkt || (ret = avcodec_send_frame(outFileCon->audioCodecCtx, frame)) == 0) {
        if (pPkt || (ret = avcodec_receive_packet(outFileCon->audioCodecCtx, pkt)) == 0) {
            if (!isUseAACBsf_converter || (ret = av_bsf_send_packet(aacbsf_Converter, pkt)) == 0) {
                if (!isUseAACBsf_converter || (ret = av_bsf_receive_packet(aacbsf_Converter, pkt)) == 0) {
                    pkt->stream_index = outFileCon->audioStream->index;
                    pkt->pos = -1;
                    pkt->flags |= AV_PKT_FLAG_KEY;
                    pkt->pts = audio_pre_pts_converter + 1024;
                    pkt->dts = pkt->pts;
                    pkt->duration = pkt->pts - audio_pre_pts_converter;
                    audio_pre_pts_converter = pkt->pts;
                    
//                    cv_printf("Auido pts:%lld dts:%lld duration:%lld\n", pkt->pts, pkt->dts, pkt->duration);
                    
                    ret = av_write_frame(outFileCon->formtCtx, pkt);
                    if (ret < 0) {
                        printf("Write audio frame failed--->%s\n", av_err2str(ret));
                    }
                } else if (ret != AVERROR(EAGAIN)) {
                    isUseAACBsf_converter = 0;      //关闭AAC的整流器
                }
            }
        } else {
            printf("Receive audio pkt failed: %d\n", ret);
        }
    } else {
            printf("Send audio frame failed: %d\n", ret);
    }
    av_packet_free(&pkt);
    
    return ret;
}

int FFmpeg_VideoConverterFilterFullFrame(AVFrame *frame)
{
    /* push the decoded frame into the filtergraph */
    int ret = av_buffersrc_add_frame_flags(filterCtx->bufferSrcCtx, frame, 0);
    if (ret < 0) {
        printf("Error occured while feeding the filtergraph\n");
        return ret;
    }
    
    /* pull filtered frames from the filtergraph */
    while ( ret >= 0 ) {
        AVFrame *filt_frame = av_frame_alloc();

        ret = av_buffersink_get_samples(filterCtx->bufferSinCtx, filt_frame, outFileCon->audioStream->codecpar->frame_size);
        if (ret < 0) {
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                ret = 0;
            av_frame_free(&filt_frame);
            break;
        }
        filt_frame->pict_type = AV_PICTURE_TYPE_NONE;
        
        ret = FFmpeg_VideoConverterWriteAudioFrame(filt_frame, NULL);
        av_frame_free(&filt_frame);
    }
    
    return ret;
}

int FFmpeg_VideoConverterWriteVideoFrame(AVFrame *frame, AVPacket *pPkt) {
    AVPacket *pkt = NULL;
    if (pPkt) {
        pkt = av_packet_clone(pPkt);
    } else {
        pkt = av_packet_alloc();
    }
    
    int ret = -1;
    ret = avcodec_send_frame(outFileCon->videoCodecCtx, frame);
    if (pPkt || ret == 0) {
        ret = avcodec_receive_packet(outFileCon->videoCodecCtx, pkt);
        if (pPkt || ret == 0) {
            ret = av_bsf_send_packet(h264bsf_Converter, pkt);
            if (ret >= 0) {
                ret = av_bsf_receive_packet(h264bsf_Converter, pkt);
                if (ret == 0) {
                    VideoFrameType frameType = VideoFrameType_UNKNOWN;
                    frameType = FFmpeg_GetVideoFrameType(pkt->data, pkt->size);
                    if (frameType == VideoFrameType_I) {
                        isWaitKey_converter = 0;
                    }
                    if (!isWaitKey_converter) {
                        pkt->stream_index = outFileCon->videoStream->index;
                        pkt->pos = -1;
                        double video_current_timestamp = pkt->pts * av_q2d(inFileCon->videoStream->time_base);//当前 pkt->pts在视频中的时间位置
                        if (video_first_timestamp_converter == -0.01 && video_current_timestamp >= 0) {
                            video_first_timestamp_converter = video_current_timestamp;
                        }
                        pkt->pts = (video_current_timestamp - video_first_timestamp_converter)/av_q2d(outFileCon->videoStream->time_base);//转换成输出文件的baseTime 的时间
                        pkt->dts = pkt->pts;
                        pkt->duration = pkt->pts - video_pre_pts_converter;
                        video_pre_pts_converter = pkt->pts;
                        
                        ret = av_write_frame(outFileCon->formtCtx, pkt);
                        if (ret != 0) {
                            printf("Write video frame failed: %d\n", ret);
                        }
                    }
                }
            }
        } else {
            printf("Receive video pkt failed: %d\n", ret);
        }
    } else {
        printf("Send video frame failed: %d\n", ret);
    }
    av_packet_free(&pkt);
    return ret;
}

void FFmpeg_VideoConverterFlushEncoder()
{
    int ret;
    while (audio_st_index_converter >= 0 && (ret = FFmpeg_VideoConverterWriteAudioFrame(NULL, NULL)) == 0) {
        printf("Flush audio frame data\n");
    }
    
    while (video_st_index_converter >= 0 && (ret = FFmpeg_VideoConverterWriteVideoFrame(NULL, NULL)) == 0) {
        printf("Flush video frame data\n");
    }
}

int FFmpeg_VideoConverterStart() {
    int ret = -1;
    if (!inFileCon || !inFileCon->formtCtx || !outFileCon || !outFileCon->formtCtx) {
        return ret;
    }
//保存了解复用（demuxer)之后，解码（decode）之前的数据（仍然是压缩后的数据）和关于这些数据的一些附加的信息，如显示时间戳（pts），解码时间戳（dts）,数据时长（duration），所在流媒体的索引（stream_index）等等
    AVPacket pkt;
    av_init_packet(&pkt);
    AVFrame *frame = av_frame_alloc();
    AVFrame *swr_frame = av_frame_alloc();
    int64_t video_auto_pts = 0;
    
    while (1) {
        av_packet_unref(&pkt);
        av_frame_unref(frame);
        av_frame_unref(swr_frame);
        
        ret = av_read_frame(inFileCon->formtCtx, &pkt);
        if (ret < 0) {
            printf("Reading video frame failed, ret:%d\n", ret);
            break;
        }
        if (pkt.stream_index == video_st_index_converter) {
            if (pkt.pts < 0) {
                pkt.pts = video_auto_pts++;
            }
            if (!isForce_converter && outFileCon->videoStream->codecpar->codec_id == inFileCon->videoStream->codecpar->codec_id) {
                FFmpeg_VideoConverterWriteVideoFrame(frame, &pkt);
            } else {
                ret = avcodec_send_packet(inFileCon->videoCodecCtx, &pkt);
                if (ret == 0) {
                    ret = avcodec_receive_frame(inFileCon->videoCodecCtx, frame);
                    if (ret == 0) {
                        FFmpeg_VideoConverterWriteVideoFrame(frame, NULL);
                    } else {
                        printf("Receive video frame failed: %d\n", ret);
                    }
                } else {
                    printf("Send video packet failed: %d\n", ret);
                }
            }
        } else if (pkt.stream_index == audio_st_index_converter) {
            ret = avcodec_send_packet(inFileCon->audioCodecCtx, &pkt);
            if (!isForce_converter && outFileCon->audioStream->codecpar->codec_id == inFileCon->audioStream->codecpar->codec_id) {
                FFmpeg_VideoConverterWriteAudioFrame(frame, &pkt);
            } else if (ret == 0) {
                if ((ret = avcodec_receive_frame(inFileCon->audioCodecCtx, frame)) == 0) {
                    if (swrCtx_Converter) {
                        frame->channel_layout = inFileCon->audioStream->codecpar->channel_layout;
                        
                        swr_frame->format = outFileCon->audioStream->codecpar->format;
                        swr_frame->pts = av_frame_get_best_effort_timestamp(frame);
                        swr_frame->pkt_dts = frame->pkt_dts;
                        swr_frame->pkt_duration = frame->pkt_duration;
                        swr_frame->sample_rate = outFileCon->audioStream->codecpar->sample_rate;
                        swr_frame->channels = outFileCon->audioStream->codecpar->channels;
                        swr_frame->channel_layout = outFileCon->audioStream->codecpar->channel_layout;
                        
                        if ((ret = swr_convert_frame(swrCtx_Converter, swr_frame, frame)) < 0) {
                            printf("Audio swr converter fail\n");
                            continue;
                        }
                        
                        if (FFmpeg_VideoConverterFilterFullFrame(swr_frame) < 0) {
                            printf("Filter and encode audio frame failed!\n");
                        }
                    } else {
                        FFmpeg_VideoConverterWriteAudioFrame(frame, NULL);
                    }
                }else {
                    printf("Rreceive audio frame failed: %d\n", ret);
                }
            }else {
                printf("Send audio pkt failed: %d\n", ret);
            }
        }else {
            printf("Not recognize frame:%d\n", pkt.stream_index);
        }
    }
    FFmpeg_VideoConverterFlushEncoder();
    av_write_trailer(outFileCon->formtCtx);
    
    av_packet_unref(&pkt);
    av_frame_free(&frame);
    av_frame_free(&swr_frame);
    
    printf("Video Converter success!\n");
    
    return 0;
}

int FFmpeg_VideoConverterToMP4(const char *inFilePath, const char *outFilePath) {
    inFileCon = malloc(sizeof(VideoFileContent));
    memset(inFileCon, 0, sizeof(VideoFileContent));
    outFileCon = malloc(sizeof(VideoFileContent));
    memset(outFileCon, 0, sizeof(VideoFileContent));
    video_pre_pts_converter = 0;
    audio_pre_pts_converter = - 1024;
    video_st_index_converter = -1;
    audio_st_index_converter = -1;
    isWaitKey_converter = 1;
    isUseAACBsf_converter = 1;
    video_first_timestamp_converter = -0.01;
    audio_first_timestamp_converter = -0.01;
    
    av_register_all();
    avfilter_register_all();//滤波器注册
    
    if (FFmpeg_VideoConverterOpenInputFile(inFilePath) < 0) {
        FFmpeg_VideoConverterRelease();
        return -1;
    }
    
    if (FFmpeg_VideoConverterOpenOutputFile(outFilePath) < 0) {
        FFmpeg_VideoConverterRelease();
        return -2;
    }
    
    int ret = FFmpeg_VideoConverterStart();
    FFmpeg_VideoConverterRelease();
    
    
    return ret;
}

void FFmpeg_VideoConverterRelease() {
    if (inFileCon) {
        if (inFileCon->formtCtx) {
            avformat_close_input(&inFileCon->formtCtx);
            avformat_free_context(inFileCon->formtCtx);
        }
        
        free(inFileCon);
        inFileCon = NULL;
    }
    
    if (outFileCon) {
        if (outFileCon->videoStream) {
            FFmpeg_FreeExtradata(outFileCon->videoStream->codecpar);
        }
        if (outFileCon->audioStream) {
            FFmpeg_FreeExtradata(outFileCon->audioStream->codecpar);
        }
        if (outFileCon->videoCodecCtx) {
            avcodec_free_context(&outFileCon->videoCodecCtx);
        }
        if (outFileCon->audioCodecCtx) {
            avcodec_free_context(&outFileCon->audioCodecCtx);
        }
        
        if (outFileCon->formtCtx) {
            if (outFileCon->formtCtx && !(outFileCon->formtCtx->flags && AVFMT_NOFILE)) {
                avio_close(outFileCon->formtCtx->pb);
            }
            avformat_free_context(outFileCon->formtCtx);
        }
        free(outFileCon);
        outFileCon = NULL;
    }
    
    if (h264bsf_Converter) {
        FFmpeg_FreeBSFContext(&h264bsf_Converter);
        h264bsf_Converter = NULL;
    }
    if (swrCtx_Converter) {
        swr_free(&swrCtx_Converter);
        swrCtx_Converter = NULL;
    }
    
    if (filterCtx) {
        if (filterCtx->filterGraph) {
            avfilter_graph_free(&filterCtx->filterGraph);
        }
        av_free(filterCtx);
        filterCtx = NULL;
    }
    isForce_converter = 0;
    
}

int FFmpeg_VideoForceConverterToMP4(const char *inFilePath, const char *outFilePath)
{
    isForce_converter = 1;
    return FFmpeg_VideoConverterToMP4(inFilePath, outFilePath);
}

#pragma mark -

int read_file_codec(const char *filename, int *formatList, int *codecCount)
{
    if (formatList == NULL || codecCount == NULL) return -1;
    av_register_all();
    
    AVFormatContext *formtCtx = NULL;
    if (avformat_open_input(&formtCtx, filename, NULL, NULL) < 0) {
        printf("Cannot openinput file:%s\n", filename);
        return -2;
    }
    
    if (formtCtx && avformat_find_stream_info(formtCtx, NULL) < 0) {
        printf("Cannot findstream information\n");
        avformat_close_input(&formtCtx);
        avformat_free_context(formtCtx);
        
        return -3;
    }
    av_dump_format(formtCtx, 0, filename, 0);
    
    int index = 0;
    for (int i = 0; i < formtCtx->nb_streams; i++) {
        AVStream *stream = formtCtx->streams[i];
        
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            if (stream->codecpar->codec_id == AV_CODEC_ID_H264) {
                formatList[index ++] = 1;       //MEDIA_FILE_CODEC_H264
            } else {
                formatList[index ++] = 0;
            }
        } else if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            if (stream->codecpar->codec_id == AV_CODEC_ID_AMR_NB) {
                formatList[index ++] = 3;       //MEDIA_FILE_CODEC_AMR_NB
            }
            else if (stream->codecpar->codec_id == AV_CODEC_ID_AAC) {
                formatList[index ++] = 4;       //MEDIA_FILE_CODEC_AAC
            } else {
                formatList[index ++] = 0;
            }
        } else {
            formatList[index ++] = 0;
        }
    }
    
    if (formtCtx) {
        avformat_close_input(&formtCtx);
        avformat_free_context(formtCtx);
        formtCtx = NULL;
    }
    *codecCount = index;

    return 0;
}
