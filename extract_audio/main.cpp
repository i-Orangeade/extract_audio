#define __STDC_CONSTANT_MACROS
extern "C" {
#include <libavutil/avutil.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avutil.lib")



int main(int argc, char** argv) {
    int ret = 0;
    AVFormatContext* pFmtCtx = NULL;
    AVFormatContext* oFmtCtx = NULL;
    AVStream* instream = NULL;
    AVStream* outstream = NULL;
    AVPacket pkt;
    int idx = -1;
    char error_buf[AV_ERROR_MAX_STRING_SIZE] = { 0 }; 

    av_log_set_level(AV_LOG_DEBUG);

    if (argc < 3) {
        av_log(NULL, AV_LOG_ERROR, "Usage: %s input.mp4 output.aac\n", argv[0]);
        return -1;
    }

    // initial variables
    AVOutputFormat* outFmt = NULL; 
    oFmtCtx = NULL;

    //open the multimedia file
    if ((ret = avformat_open_input(&pFmtCtx, argv[1], NULL, NULL)) != 0) {
        av_strerror(ret, error_buf, sizeof(error_buf));
        av_log(NULL, AV_LOG_ERROR, "open the multimedia file failed!\n", error_buf);
        goto end;
    }

    // obtain flow information
    if ((ret = avformat_find_stream_info(pFmtCtx, NULL)) < 0) {
        av_strerror(ret, error_buf, sizeof(error_buf));
        av_log(pFmtCtx, AV_LOG_ERROR, "obtain flow information failed!\n", error_buf);
        goto end;
    }

    //find the audio stream from container
    idx = av_find_best_stream(pFmtCtx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (idx < 0) {
        av_log(pFmtCtx, AV_LOG_ERROR, "find the audio stream failed!\n");
        ret = -1;
        goto end;
    }

    outFmt = av_guess_format("adts", NULL, NULL);
    if (!outFmt) {
        av_log(NULL, AV_LOG_ERROR, "CREAT ADTS container failed!\n");
        ret = -1;
        goto end;
    }

    oFmtCtx = avformat_alloc_context();
    oFmtCtx->oformat = outFmt;

	// create output stream
    if (!(outstream = avformat_new_stream(oFmtCtx, NULL))) {
        av_log(oFmtCtx, AV_LOG_ERROR, "create output stream failed!\n");
        ret = -1;
        goto end;
    }

    // copy encoding parameters
    instream = pFmtCtx->streams[idx];
    if ((ret = avcodec_parameters_copy(outstream->codecpar, instream->codecpar)) < 0) {
        av_strerror(ret, error_buf, sizeof(error_buf));
        av_log(oFmtCtx, AV_LOG_ERROR, "copy encoding parameters failed!\n", error_buf);
        goto end;
    }
    outstream->time_base = instream->time_base;

    // open output file
    if (!(outFmt->flags & AVFMT_NOFILE)) {
        if ((ret = avio_open(&oFmtCtx->pb, argv[2], AVIO_FLAG_WRITE)) < 0) {
            av_strerror(ret, error_buf, sizeof(error_buf));
            av_log(oFmtCtx, AV_LOG_ERROR, "open output file failed!\n", error_buf);
            goto end;
        }
    }

    // write the head file
    if ((ret = avformat_write_header(oFmtCtx, NULL)) < 0) {
        av_strerror(ret, error_buf, sizeof(error_buf));
        av_log(oFmtCtx, AV_LOG_ERROR, "write the head file failed!\n", error_buf);
        goto end;
    }

    while (av_read_frame(pFmtCtx, &pkt) >= 0) {
        if (pkt.stream_index == idx) {
            pkt.pts = av_rescale_q(pkt.pts,
                instream->time_base,
                outstream->time_base);
            pkt.dts = pkt.pts;
            pkt.duration = av_rescale_q(pkt.duration,
                instream->time_base,
                outstream->time_base);
            pkt.pos = -1;
            pkt.stream_index = outstream->index;

            if ((ret = av_interleaved_write_frame(oFmtCtx, &pkt)) < 0) {
                av_strerror(ret, error_buf, sizeof(error_buf));
                av_log(oFmtCtx, AV_LOG_ERROR, "data packet write failed!\n", error_buf);
                av_packet_unref(&pkt);
                break;
            }
        }
        av_packet_unref(&pkt);
    }

    //write end of file
    av_write_trailer(oFmtCtx);

end:
    if (pFmtCtx) {
        avformat_close_input(&pFmtCtx);
        pFmtCtx = NULL; 
    }

    if (oFmtCtx) {
        if (oFmtCtx->pb) {
            avio_close(oFmtCtx->pb);
            oFmtCtx->pb = NULL; 
        }
        avformat_free_context(oFmtCtx);
        oFmtCtx = NULL;
    }
    return 0;
}