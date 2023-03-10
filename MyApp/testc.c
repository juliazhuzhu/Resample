//
//  testc.c
//  MyApp
//
//  Created by 晓叶 on 2023/3/3.
//

#include "testc.h"
#include "libavutil/avutil.h"
#include "libavformat/avformat.h"
#include "libavdevice/avdevice.h"
#include "libavcodec/avcodec.h"
#include "libswresample/swresample.h"
#include <unistd.h>
#include <string.h>

static int rec_status = 0;

void stop_record() {
    rec_status = 0;
}

void record_audio(void ) {
    printf("record_audio \n");
    int ret = 0;
    char averr[1024]={0};
    rec_status = 1;
    //ctx var
    AVFormatContext *av_format_ctx = NULL;
    char* device_name = ":0";
    AVDictionary* opt = NULL;
    AVInputFormat* iFormat = NULL;
    
    //packet var
    int count = 0;
    AVPacket packet;
//    AVPacket* pkt;
    av_log_set_level(AV_LOG_DEBUG);
    av_log(NULL, AV_LOG_DEBUG, "hello ffmpeg! \n");
    
    avdevice_register_all();
    iFormat = av_find_input_format("avfoundation");
    if ((ret = avformat_open_input( &av_format_ctx, device_name, iFormat, &opt)) < 0){
        
        av_strerror(ret, averr, sizeof(averr));
        printf("failed to open device %s\n", averr);
        return;
    }
    FILE *out_file = fopen("/Users/xiaoye/Downloads/record.pcm", "wb+");
    
    SwrContext *swr_ctx = NULL;
    swr_ctx = swr_alloc_set_opts(NULL,
                                 AV_CH_LAYOUT_STEREO,
                                 AV_SAMPLE_FMT_S16,
                                 44100,
                                 AV_CH_LAYOUT_STEREO,
                                 AV_SAMPLE_FMT_FLT,
                                 44100,
                                 0, NULL);
    
    if (swr_ctx == NULL) {
        printf("swr ctx alloc failed! \n");
        return;
    }
        
    if (swr_init(swr_ctx) < 0) {
        printf("swr ctx init failed! \n");
        return;
    }
    
    //创建输入缓冲区
    uint8_t **src_data = NULL;
    int src_linesize = 0;
    av_samples_alloc_array_and_samples(&src_data,//输出缓冲区地址
                                       &src_linesize,//输出缓冲区大小
                                       2,//通道个数
                                       512,//单通道采样数
                                       AV_SAMPLE_FMT_FLT,//采样格式
                                       0);
    
    
    //创建输出缓冲区
    uint8_t **dst_data = NULL;
    int dst_linesize = 0;
    av_samples_alloc_array_and_samples(&dst_data,//输出缓冲区地址
                                       &dst_linesize,//输出缓冲区大小
                                       2,//通道个数
                                       512,//单通道采样数
                                       AV_SAMPLE_FMT_S16,//采样格式
                                       0);
    
    
    av_init_packet(&packet);
//    pkt = av_packet_alloc();
    while(((ret = av_read_frame(av_format_ctx, &packet)) == 0 || ret == -35)
          && rec_status){
        if(ret == -35){
            usleep(100);
            continue;
        }
        memcpy((void*)src_data[0],packet.data,src_linesize);
        
        int ret = swr_convert(swr_ctx,
                    dst_data,
                    dst_linesize,
                    (const uint8_t **)src_data,
                              512);//sample in one channel
        if (ret < 0){
            printf("swr error! \n");
            return;
        }
        fwrite((void*)dst_data[0], dst_linesize, 1, out_file);
//        fwrite((void*)src_data[0], src_linesize, 1, out_file);
//        fwrite(packet.data, packet.size, 1, out_file);
        fflush(out_file);
        av_log(NULL, AV_LOG_INFO,"packet size %d %d\n", packet.size,count);
        av_packet_unref(&packet);
        av_init_packet(&packet);
//        av_packet_unref(pkt);
//        av_init_packet(pkt);
        count++;
       
    }
    if (src_data){
        av_freep(&src_data[0]);
        av_freep(src_data);
    }
    if (dst_data){
        av_freep(&dst_data[0]);
        av_freep(dst_data);
    }
    
   
    av_packet_unref(&packet);
//    av_packet_free(&pkt);
    avformat_close_input(&av_format_ctx);
    swr_free(&swr_ctx);
    fclose(out_file);
    return;
}
