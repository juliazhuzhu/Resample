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

AVCodecContext* open_encoder(void){
//    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    const AVCodec* codec = avcodec_find_encoder_by_name("libfdk_aac");
    AVCodecContext* codec_ctx = avcodec_alloc_context3(codec);
    codec_ctx->profile = FF_PROFILE_AAC_HE_V2;
    codec_ctx->bit_rate = 0;
    codec_ctx->sample_rate = 44100;
    codec_ctx->sample_fmt = AV_SAMPLE_FMT_S16;
    codec_ctx->channel_layout = AV_CH_LAYOUT_STEREO;
//    codec_ctx->ch_layout.nb_channels = AV_CH_LAYOUT_STEREO;
    codec_ctx->channels = 2;
    codec_ctx->ch_layout.nb_channels = 2;
    int ret = 0;
    char averr[1024]={0};
    
    codec_ctx->flags = AV_CODEC_FLAG_GLOBAL_HEADER;
    if ((ret = avcodec_open2(codec_ctx, codec, NULL)) < 0){
        av_strerror(ret, averr, sizeof(averr));
        printf("open codec failed, reason:%s! \n",averr);
        exit(-1);
        return NULL;
    }
    printf("2 frame_size:%d\n\n", codec_ctx->frame_size); //
    return codec_ctx;
}

static void get_adts_header(AVCodecContext *ctx, uint8_t *adts_header, int aac_length)
{
    uint8_t freq_idx = 0;    //0: 96000 Hz  3: 48000 Hz 4: 44100 Hz
    switch (ctx->sample_rate) {
        case 96000: freq_idx = 0; break;
        case 88200: freq_idx = 1; break;
        case 64000: freq_idx = 2; break;
        case 48000: freq_idx = 3; break;
        case 44100: freq_idx = 4; break;
        case 32000: freq_idx = 5; break;
        case 24000: freq_idx = 6; break;
        case 22050: freq_idx = 7; break;
        case 16000: freq_idx = 8; break;
        case 12000: freq_idx = 9; break;
        case 11025: freq_idx = 10; break;
        case 8000: freq_idx = 11; break;
        case 7350: freq_idx = 12; break;
        default: freq_idx = 4; break;
    }
    uint8_t chanCfg = ctx->channels;
    uint32_t frame_length = aac_length + 7;
    adts_header[0] = 0xFF;
    adts_header[1] = 0xF1;
    adts_header[2] = ((ctx->profile) << 6) + (freq_idx << 2) + (chanCfg >> 2);
    adts_header[3] = (((chanCfg & 3) << 6) + (frame_length  >> 11));
    adts_header[4] = ((frame_length & 0x7FF) >> 3);
    adts_header[5] = (((frame_length & 7) << 5) + 0x1F);
    adts_header[6] = 0xFC;
}

void encode(AVCodecContext* codec_ctx,AVFrame* frame, AVPacket* newpkt,FILE* out_file){
   
    char averr[1024]={0};
    int ret = avcodec_send_frame(codec_ctx, frame);
    if (ret < 0){
        av_strerror(ret, averr, sizeof(averr));
        printf("send frame failed %s! \n",averr);
        return;
    }
    while (ret >= 0){
        ret = avcodec_receive_packet(codec_ctx, newpkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return ;
        else if (ret < 0){
            printf("error in audio encoder! \n");
            exit(-1);
        }

        
//        printf("ctx->flags:0x%x & AV_CODEC_FLAG_GLOBAL_HEADER:0x%x, name:%s\n",codec_ctx->flags, codec_ctx->flags & AV_CODEC_FLAG_GLOBAL_HEADER, codec_ctx->codec->name);
            if((codec_ctx->flags & AV_CODEC_FLAG_GLOBAL_HEADER)) {
 
            // 需要额外的adts header写入
            uint8_t aac_header[7]={0};
            get_adts_header(codec_ctx, aac_header, newpkt->size);
            int len = fwrite(aac_header, 1, 7, out_file);
            if(len != 7) {
                printf("fwrite aac_header failed\n");
                return;
            }
        }
        
        fwrite((void*)newpkt->data, newpkt->size,1, out_file);
        fflush(out_file);

    }
    
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
    
    int inSampleRate = 44100;
    int outSampleRate = 44100;
    enum AVSampleFormat inFormat = AV_SAMPLE_FMT_FLT;
    enum AVSampleFormat outFormat = AV_SAMPLE_FMT_S16;
    int in_ch_layout = AV_CH_LAYOUT_STEREO;
    int out_ch_layout = AV_CH_LAYOUT_STEREO;
    int inChNum = 2;
    int outChNum = 2;
    //4096/4=1024,32位->4个字节
    //1024/2=512
    int inSamples = 512;
    int inBytesPerSample = 2 * av_get_bytes_per_sample(outFormat);
    int outSamples = 512;//av_rescale_rnd(outSampleRate, inSamples, inSampleRate, AV_ROUND_UP);
    int pts = 0;
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
    FILE *out_file = fopen("/Users/xiaoye/Downloads/record.aac", "wb+");
    
    AVCodecContext* codec_ctx = open_encoder();
   
    AVFrame* frame = av_frame_alloc();
    if (frame == NULL){
        printf("frame alloc failed! \n");
        return;
    }
//    frame->nb_samples = 512;//单通道一个音频帧采样数
//    frame->format = AV_SAMPLE_FMT_S16;
//    frame->channels = 2;
//    frame->ch_layout.nb_channels = 2;
//    frame->sample_rate = 44100;
    frame->nb_samples     = codec_ctx->frame_size;
    frame->format         = codec_ctx->sample_fmt;
       
    
    frame->channel_layout = AV_CH_LAYOUT_STEREO;
    av_frame_get_buffer(frame, 0);
    if (frame->buf[0] == NULL){
        printf("frame buf alloc failed! \n");
        return;
    }
    ret = av_frame_make_writable(frame);
    if (ret != 0){
        printf("frame is not wirtable!\n");
        return;
    }

    AVPacket* newpkt = av_packet_alloc();//分配编码后的数据空间
    if (!newpkt){
        printf("av packet alloc failed! \n");
        return;
    }
    
    
    
    SwrContext *swr_ctx = NULL;
    swr_ctx = swr_alloc_set_opts(NULL,
                                 out_ch_layout,
                                 outFormat,
                                 inSampleRate,
                                 in_ch_layout,
                                 inFormat,
                                 outSampleRate,
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
                                       inChNum,//通道个数
                                       inSamples,//单通道采样数
                                       inFormat,//采样格式
                                       0);
    
    
    
    //创建输出缓冲区
    uint8_t **dst_data = NULL;
    int dst_linesize = 0;
    av_samples_alloc_array_and_samples(&dst_data,//输出缓冲区地址
                                       &dst_linesize,//输出缓冲区大小
                                       outChNum,//通道个数
                                       outSamples,//单通道采样数
                                       outFormat,//采样格式
                                       0);
    
    
    av_init_packet(&packet);
//    pkt = av_packet_alloc();
    while(((ret = av_read_frame(av_format_ctx, &packet)) == 0 || ret == -35)
          && rec_status){
        if(ret == -35){
            usleep(100);
            continue;
        }

//        int inBytesPerSample = 2 * av_get_bytes_per_sample(outFormat);
//        inSamples = packet.size / inBytesPerSample;
//        outSamples = av_rescale_rnd(outSampleRate, inSamples, inSampleRate, AV_ROUND_UP);
//
        memcpy((void*)src_data[0],packet.data,src_linesize);
        
        int ret = swr_convert(swr_ctx,
                    dst_data,
                    dst_linesize,
                    (const uint8_t **)src_data,
                              inSamples);//sample in one channel
        if (ret < 0){
            printf("swr error! \n");
            return;
        }
        memcpy((void*)frame->data[0], dst_data[0], dst_linesize);
//        ret = av_samples_fill_arrays(frame->data,
//                                     frame->linesize,
//                                     dst_data[0],
//                                     frame->channels,
//                                     frame->nb_samples,
//                                     frame->format, 0);
//        if (ret < 0){
//            printf("fill frame failed! \n");
//            return;
//        }
//        ret = avcodec_send_frame(codec_ctx, frame);
//        while (ret >= 0){
//            ret = avcodec_receive_packet(codec_ctx, newpkt);
//            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
//                break;
//            else if (ret < 0){
//                printf("error in audio encoder! \n");
//                return;
//            }
//
//
//            fwrite((void*)newpkt->data, 1,newpkt->size, out_file);
//            fflush(out_file);
//
//        }
        
//        pts += frame->nb_samples;
//        frame->pts = pts;
        encode(codec_ctx,frame,newpkt, out_file);
       
//        int outBytesPerSample = outChNum * av_get_bytes_per_sample(outFormat);
//        fwrite((void*)dst_data[0], dst_linesize, 1, out_file);
//        fwrite((void*)src_data[0], src_linesize, 1, out_file);
//        fwrite(packet.data, packet.size, 1, out_file);
//        fflush(out_file);
        av_log(NULL, AV_LOG_INFO,"packet size %d %d\n", packet.size,count);
        av_packet_unref(&packet);
//        av_init_packet(&packet);
//        av_packet_unref(pkt);
//        av_init_packet(pkt);
        count++;
       
    }
    
    encode(codec_ctx,NULL,newpkt, out_file);
    
    if (src_data){
        av_freep(&src_data[0]);
        av_freep(src_data);
    }
    if (dst_data){
        av_freep(&dst_data[0]);
        av_freep(dst_data);
    }
    av_frame_free(&frame);
    av_packet_free(&newpkt);
    av_packet_unref(&packet);
//    av_packet_free(&pkt);
    avformat_close_input(&av_format_ctx);
    swr_free(&swr_ctx);
    fclose(out_file);
    return;
}
