/**
 * Sample code to mux multiple audio files together into one.
 * This is assuming, all of them have the same channel configuration.
 * Bonus:
 *  - Thumbnail
 *  - Chapters
 *  - Metadata
 *  - without any proper error handling
 *  - without handling incompatible thumbnail codecs
 *
 * Compile with
 * $ gcc -lm -lssl -lcrypto ff_static.c -lavformat -lavcodec -lavutil -o ff_dynamic
 *
 * Minimum viable static configuration
 * $ ./configure --enable-debug --disable-shared\
 *  --disable-autodetect --disable-iconv\
 *  --disable-doc --disable-htmlpages --disable-manpages --disable-podpages --disable-txtpages\
 *  --disable-everything\
 *  --disable-avdevice --disable-swresample --disable-swscale --disable-postproc --disable-avfilter --disable-dwt --disable-error-resilience --disable-lsp --disable-faan --disable-pixelutils\
 *  --enable-openssl\
 *  --enable-muxer=mov,ipod\
 *  --enable-demuxer=aac,mjpeg,hls,jpeg_pipe\
 *  --enable-parser=aac,mjpeg\
 *  --enable-decoder=aac,mjpeg\
 *  --enable-bsf=aac_adtstoasc\
 *  --enable-protocol=http,tcp,file,crypto,https,httpproxy
 * $ make
 * $ gcc -lm -lssl -lcrypto sample.c libavformat/libavformat.a libavcodec/libavcodec.a libavutil/libavutil.a -o ff_static
 */
#include <stdio.h>
#include <libavformat/avformat.h>
#include "audio.h"

int download(const struct metadata *meta, const char *output_filename)
{
	AVFormatContext *output_format_ctx = NULL, *input_format_ctx = NULL;
	AVStream *input_stream, *output_stream;
	int64_t last_pts = 0;

	av_log_set_level(AV_LOG_ERROR);

	AVChapter **chapters = av_malloc_array(meta->nb_chapters, sizeof(AVChapter *));

	for (unsigned i = 0; i < meta->nb_chapters; ++i)
	{
		AVChapter *chapter = chapters[i] = av_mallocz(sizeof(AVChapter));
		chapter->id = i;
		chapter->start = last_pts;

		// Sample file path. HTTP in this case
		const char *input_audio_filename = meta->chapters[i].url;
		printf("opening %s\n", input_audio_filename);
		int ret = 0;

		AVDictionary *dictionary = NULL;
		// NOTE: keep_alive with encryption toally broken on HLS
		// av_dict_set(&dictionary, "http_persistent", "false", 0);
		av_dict_set(&dictionary, "segment_format_options", "avoid_negative_ts=make_zero", 0);

		// Open input audio file
		if ((ret = avformat_open_input(&input_format_ctx, input_audio_filename, NULL, &dictionary)) != 0)
		{
			fprintf(stderr, "Failed to open input audio file: %s\n", av_err2str(ret));
			return -1;
		}

		// Read input file header
		if ((ret = avformat_find_stream_info(input_format_ctx, NULL)) != 0)
		{
			fprintf(stderr, "Error reading input file info: %s\n", av_err2str(ret));
			return -1;
		}

		// Iterate through input audio streams and add them to the output context
		for (unsigned int i = 0; i < input_format_ctx->nb_streams; i++)
		{
			// Ignore non-audio streams
			if (input_format_ctx->streams[i]->codecpar->codec_type != AVMEDIA_TYPE_AUDIO)
				continue;

			input_stream = input_format_ctx->streams[i];

			break;
		}

		if (input_stream == NULL)
		{
			fprintf(stderr, "Failed to find audio stream in input\n");
			return -1;
		}

		if (!output_format_ctx)
		{
			AVFormatContext *thumbnail_format_ctx = NULL;
			AVStream *thumbnail_in_stream = NULL, *thumbnail_out_stream = NULL;

			// Create output format context
			output_format_ctx = avformat_alloc_context();
			if (!output_format_ctx)
			{
				fprintf(stderr, "Failed to allocate output format context\n");
				return 1;
			}

			// Set the output format to the desired format (e.g., mp4)
			avformat_alloc_output_context2(&output_format_ctx, NULL, NULL, output_filename);

			output_stream = avformat_new_stream(output_format_ctx, NULL);
			avcodec_parameters_copy(output_stream->codecpar, input_stream->codecpar);

			// Open the output file
			if (avio_open(&output_format_ctx->pb, output_filename, AVIO_FLAG_WRITE) < 0)
			{
				fprintf(stderr, "Failed to open output file\n");
				return 1;
			}

			AVDictionary *metadata = NULL;
			av_dict_set(&metadata, "title", meta->title, 0);

			output_format_ctx->metadata = metadata;

            AVDictionary *thumb_settings = NULL;
            av_dict_set(&thumb_settings, "seekable", "false", 0);

			// Add thumbnail
			if ((ret = avformat_open_input(&thumbnail_format_ctx, meta->thumbnail, NULL, &thumb_settings) != 0))
			{
				fprintf(stderr, "Opening thumbnail failed: %s\n", av_err2str(ret));
				return 1;
			}

			// Read input file header
			if ((ret = avformat_find_stream_info(thumbnail_format_ctx, NULL)) != 0)
			{
				fprintf(stderr, "Error reading input file info: %s\n", av_err2str(ret));
				return 1;
			}

			// Iterate through input audio streams and add them to the output context
			for (unsigned int i = 0; i < thumbnail_format_ctx->nb_streams; i++)
			{
				// Ignore non-video streams
				if (thumbnail_format_ctx->streams[i]->codecpar->codec_type != AVMEDIA_TYPE_VIDEO)
					continue;

				thumbnail_in_stream = thumbnail_format_ctx->streams[i];
				break;
			}

			if (thumbnail_in_stream == NULL)
			{
				fprintf(stderr, "No image stream found in thumbnail\n");
				return 1;
			}

			thumbnail_out_stream = avformat_new_stream(output_format_ctx, NULL);
			avcodec_parameters_copy(thumbnail_out_stream->codecpar, thumbnail_in_stream->codecpar);
			thumbnail_out_stream->disposition = AV_DISPOSITION_ATTACHED_PIC;

			// Write the output header
			if ((ret = avformat_write_header(output_format_ctx, NULL)) != 0)
			{
				fprintf(stderr, "Failed to write output header: %s\n", av_err2str(ret));
				return 1;
			}

			// Initialize packet for reading from the input streams
			AVPacket packet = {0};

			if ((ret = av_read_frame(thumbnail_format_ctx, &packet)) != 0)
			{
				fprintf(stderr, "failed to read thumbnail frame: %s\n", av_err2str(ret));
				return 1;
			}
			packet.stream_index = thumbnail_out_stream->index;
			if ((ret = av_write_frame(output_format_ctx, &packet)) != 0)
			{
				fprintf(stderr, "failed to read thumbnail frame: %s\n", av_err2str(ret));
				return 1;
			}
			av_packet_unref(&packet);

			avformat_close_input(&thumbnail_format_ctx);
		}

		if (output_stream == NULL)
		{
			fprintf(stderr, "output stream somehow NULL\n");
			return -1;
		}

		// Initialize packet for reading from the input streams
		AVPacket packet = {0};

		AVRational output_time_base = output_stream->time_base; // Output timebase
		int64_t offset_pts = last_pts;

		// Read and write frames from input to output
		while (1)
		{
			// Read a packet from the input audio stream
			if (av_read_frame(input_format_ctx, &packet) < 0)
			{
				break; // End of audio stream
			}

			if (packet.stream_index != input_stream->index)
			{
				fprintf(stderr, "Skipping unwanted stream\n");
				continue;
			}

			// Regenerate PTS for the audio packet
			av_packet_rescale_ts(&packet, input_stream->time_base, output_time_base);

			// Add negative dts onto the current progress and pray it not different
			//  from pts.
			if (packet.dts < 0) {
				offset_pts += -packet.dts;
			}
			packet.dts += offset_pts;
			packet.pts += offset_pts;
			last_pts = packet.pts + packet.duration;

			// Store the modified audio frame (if needed) and write to output
			if ((ret = av_write_frame(output_format_ctx, &packet)) < 0)
			{
				printf("av_write_frame: %s\n", av_err2str(ret));
				return -1;
			}

			av_packet_unref(&packet);
		}

		avformat_close_input(&input_format_ctx);

		chapter->time_base = output_time_base;
		chapter->end = last_pts;

		av_dict_set(&chapter->metadata, "title", meta->chapters[i].title, 0);
	}

	output_format_ctx->nb_chapters = meta->nb_chapters;
	output_format_ctx->chapters = chapters;

	// Write the output trailer
	av_write_trailer(output_format_ctx);

	// Clean up resources
	avio_close(output_format_ctx->pb);
	avformat_free_context(output_format_ctx);

	return 0;
}