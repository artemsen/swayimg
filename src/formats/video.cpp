// SPDX-License-Identifier: MIT
// Video as storyboard image format.
// Copyright (C) 2026 Artem Senichev <artemsen@gmail.com>

#include "../imageformat.hpp"

#include <chrono>
#include <cmath>
#include <format>
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/display.h>
#include <libavutil/error.h>
#include <libavutil/log.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}

namespace {

class ImageFormatVideo : public ImageFormat {
public:
    ImageFormatVideo() noexcept
        : ImageFormat(Priority::Lowest, "video")
    {
        av_log_set_level(AV_LOG_QUIET); // keep quiet
    }

    [[nodiscard]] ImagePtr decode(const Data& data) const override
    {
        if (!is_video(data)) {
            return nullptr;
        }

        AvDecoder decoder(data);
        if (!decoder.open()) {
            return nullptr;
        }

        // storyboard grid
        const size_t grid_cols = columns;
        const size_t grid_rows = rows;

        // tile size derived from video aspect ratio
        const double aspect = decoder.aspect();
        const size_t tile_width = size;
        const size_t tile_height = std::clamp(
            static_cast<size_t>(static_cast<double>(tile_width) / aspect),
            static_cast<size_t>(1), tile_width * 2);

        // allocate storyboard canvas
        ImagePtr image = std::make_shared<Image>();
        image->frames.resize(1);
        Pixmap& canvas = image->frames[0].pm;
        canvas.create(Pixmap::ARGB,
                      grid_cols * tile_width + (grid_cols - 1) * padding,
                      grid_rows * tile_height + (grid_rows - 1) * padding);
        canvas.fill({ 0, 0, canvas.width(), canvas.height() },
                    { argb_t::max, argb_t::min, argb_t::min, argb_t::min });

        // grab frames evenly spaced over the video duration
        const int64_t total = decoder.duration();
        const size_t count = grid_cols * grid_rows;
        for (size_t i = 0; i < count; ++i) {
            const int64_t target = total * static_cast<int64_t>(i * 2 + 1) /
                static_cast<int64_t>(count * 2);

            Pixmap tile;
            tile.create(Pixmap::ARGB, tile_width, tile_height);
            if (decoder.grab_frame(target, tile)) {
                const size_t col = i % grid_cols;
                const size_t row = i / grid_cols;
                const Point pos {
                    .x = static_cast<ssize_t>(col * (tile_width + padding)),
                    .y = static_cast<ssize_t>(row * (tile_height + padding)),
                };
                canvas.copy(tile, pos);
            }
        }

        image->format = std::format("Video ({}) {} {}x{}", decoder.codec_name(),
                                    decoder.duration_hr(), decoder.width(),
                                    decoder.height());

        return image;
    }

    [[nodiscard]] Pixmap preview(const Data& data, const size_t sz,
                                 const bool fill) const override
    {
        if (!is_video(data)) {
            return {};
        }

        AvDecoder decoder(data);
        if (!decoder.open()) {
            return {};
        }

        // decode a single frame scaled to the thumbnail resolution
        const double aspect = decoder.aspect();
        size_t tw = aspect >= 1.0 ? sz : static_cast<size_t>(sz * aspect);
        size_t th = aspect >= 1.0 ? static_cast<size_t>(sz / aspect) : sz;
        tw = std::max(tw, static_cast<size_t>(1));
        th = std::max(th, static_cast<size_t>(1));

        Pixmap pm;
        pm.create(Pixmap::ARGB, tw, th);
        if (!decoder.grab_frame(decoder.duration() / 3, pm)) {
            return {};
        }
        return make_thumb(pm, sz, fill);
    }

    void set_config(Config& params) override
    {
        ImageFormat::set_config(params);
        params.get("size", size, 10, 10000);
        params.get("columns", columns, 1, 100);
        params.get("rows", rows, 1, 100);
        params.get("padding", padding, 0, 1000);
    }

private:
    /**
     * Probe the first bytes of the buffer for a known media container.
     * @param data media stream data
     * @return true if media is valid
     */
    static bool is_video(const Data& data)
    {
        constexpr size_t probe_size = 4096;

        const size_t size = std::min(data.size, probe_size);
        std::vector<uint8_t> probe(size + AVPROBE_PADDING_SIZE, 0);
        std::memcpy(probe.data(), data.data, size);

        AVProbeData pd {};
        pd.filename = "";
        pd.buf = probe.data();
        pd.buf_size = static_cast<int>(size);
        pd.mime_type = nullptr;

        return !!av_probe_input_format(&pd, 1);
    }

private:
    size_t size = 300;  ///< Size (width) of a single tile (frame)
    size_t columns = 3; ///< Number of columns in storyboard
    size_t rows = 3;    ///< Number of rows in storyboard
    size_t padding = 5; ///< Gap between tiles in pixels

private:
    /** Memory buffer reader. */
    struct MemBuffer {
        MemBuffer(const Data& raw_data)
            : data(raw_data)
        {
        }

        /** Buffer reader: see avio_alloc_context for details. */
        static int read(void* opaque, uint8_t* buf, int buf_size)
        {
            MemBuffer* mb = static_cast<MemBuffer*>(opaque);

            const size_t remaining = mb->data.size - mb->position;
            if (remaining == 0) {
                return AVERROR_EOF;
            }

            const size_t sz =
                std::min(static_cast<size_t>(buf_size), remaining);
            std::memcpy(buf, mb->data.data + mb->position, sz);
            mb->position += sz;

            return static_cast<int>(sz);
        }

        /** Buffer position change: see avio_alloc_context for details. */
        static int64_t seek(void* opaque, int64_t offset, int whence)
        {
            MemBuffer* mb = static_cast<MemBuffer*>(opaque);

            if (whence & AVSEEK_SIZE) {
                return static_cast<int64_t>(mb->data.size);
            }

            size_t pos;
            switch (whence) {
                case SEEK_SET:
                    if (offset < 0) {
                        return AVERROR(EINVAL);
                    }
                    pos = offset;
                    break;
                case SEEK_CUR:
                    pos = static_cast<int64_t>(mb->position) + offset;
                    break;
                case SEEK_END:
                    pos = static_cast<int64_t>(mb->data.size) + offset;
                    break;
                default:
                    return AVERROR(EINVAL);
            }
            if (pos > mb->data.size) {
                return AVERROR(EINVAL);
            }

            mb->position = pos;

            return mb->position;
        }

        const Data& data;
        size_t position = 0;
    };

    /** AV decoder. */
    class AvDecoder {
    public:
        /**
         * Constructor.
         * @param data media stream data
         */
        AvDecoder(const Data& data)
            : mb(data)
        {
        }

        /** Destructor. */
        ~AvDecoder()
        {
            if (fmt) {
                avformat_close_input(&fmt);
            }
            if (io) {
                av_freep(static_cast<void*>(&io->buffer));
                avio_context_free(&io);
            }
            if (dec) {
                avcodec_free_context(&dec);
            }
            if (frame) {
                av_frame_free(&frame);
            }
        }

        /**
         * Open data stream.
         * @return true if stream opened
         */
        bool open()
        {
            constexpr size_t io_buffer_size = 4096;
            unsigned char* io_buffer =
                static_cast<unsigned char*>(av_malloc(io_buffer_size));
            if (!io_buffer) {
                return false;
            }

            io =
                avio_alloc_context(io_buffer, io_buffer_size, 0, &mb,
                                   &MemBuffer::read, nullptr, &MemBuffer::seek);
            if (!io) {
                av_free(io_buffer);
                return false;
            }

            fmt = avformat_alloc_context();
            if (!fmt) {
                return false;
            }
            fmt->pb = io;

            const int rc = avformat_open_input(&fmt, nullptr, nullptr, nullptr);
            if (rc < 0) {
                return false;
            }

            // fast check: bail out before decoding when the streams are
            // already known and none of them is a video stream. Some
            // demuxers (e.g. MPEG-PS) discover streams lazily and report
            // none here; those are handled after avformat_find_stream_info.
            if (fmt->nb_streams > 0) {
                bool has_video = false;
                for (unsigned i = 0; i < fmt->nb_streams && !has_video; ++i) {
                    has_video = fmt->streams[i]->codecpar->codec_type ==
                        AVMEDIA_TYPE_VIDEO;
                }
                if (!has_video) {
                    return false;
                }
            }

            if (avformat_find_stream_info(fmt, nullptr) < 0) {
                return false;
            }

            stream_id = av_find_best_stream(fmt, AVMEDIA_TYPE_VIDEO, -1, -1,
                                            nullptr, 0);
            if (stream_id < 0) {
                return false;
            }
            st = fmt->streams[stream_id];
            tb = st->time_base;

            const AVCodec* codec = avcodec_find_decoder(st->codecpar->codec_id);
            if (!codec) {
                return false;
            }
            cname = codec->name;

            dec = avcodec_alloc_context3(codec);
            if (!dec) {
                return false;
            }
            if (avcodec_parameters_to_context(dec, st->codecpar) < 0) {
                return false;
            }
            dec->thread_count = 3;
            if (avcodec_open2(dec, codec, nullptr) < 0) {
                return false;
            }

            frame = av_frame_alloc();
            if (!frame) {
                return false;
            }

            rotation = read_rotation();

            return st->duration != AV_NOPTS_VALUE ||
                fmt->duration != AV_NOPTS_VALUE;
        }

        /**
         * Get frame width.
         * @return frame width in pixels
         */
        [[nodiscard]] size_t width() const
        {
            return st ? st->codecpar->width : 0;
        }

        /**
         * Get frame height.
         * @return frame height in pixels
         */
        [[nodiscard]] size_t height() const
        {
            return st ? st->codecpar->height : 0;
        }

        /**
         * Get frame aspect ratio.
         * @return aspect ratio (width / height)
         */
        [[nodiscard]] double aspect() const
        {
            if (!st || st->codecpar->width <= 0 || st->codecpar->height <= 0) {
                return 16.0 / 9.0;
            }
            AVRational sar = st->sample_aspect_ratio;
            if (sar.num <= 0 || sar.den <= 0) {
                sar.num = 1;
                sar.den = 1;
            }
            double ratio = static_cast<double>(st->codecpar->width) * sar.num /
                (static_cast<double>(st->codecpar->height) * sar.den);
            if (rotation == 90 || rotation == 270) {
                ratio = 1.0 / ratio;
            }
            return ratio;
        }

        /**
         * Get total length expressed in stream time base.
         * @return total length
         */
        [[nodiscard]] int64_t duration() const
        {
            if (st->duration != AV_NOPTS_VALUE) {
                return st->duration;
            }
            return av_rescale_q(fmt->duration,
                                AVRational { .num = 1, .den = AV_TIME_BASE },
                                tb);
        }

        /**
         * Get total length in human readable format.
         * @return total length in format HH:MM:SS
         */
        [[nodiscard]] std::string duration_hr() const
        {
            const size_t seconds =
                static_cast<double>(duration()) * tb.num / tb.den;
            const std::chrono::seconds cd(seconds);
            const std::chrono::hh_mm_ss hms { cd };
            return std::format("{:%T}", hms);
        }

        /**
         * Get short codec name.
         * @return codec name
         */
        [[nodiscard]] const std::string& codec_name() const { return cname; }

        /**
         * Grab frame.
         * @param time stream time base units
         * @param pm target pixmap
         * @return true if frame grabbed
         */
        bool grab_frame(const int64_t time, Pixmap& pm)
        {
            avcodec_flush_buffers(dec);
            if (av_seek_frame(fmt, stream_id, time, AVSEEK_FLAG_BACKWARD) < 0) {
                return false;
            }

            AVPacket* pkt = av_packet_alloc();
            if (!pkt) {
                return false;
            }

            bool found = false;
            while (!found && av_read_frame(fmt, pkt) >= 0) {
                if (pkt->stream_index != stream_id) {
                    av_packet_unref(pkt);
                    continue;
                }
                const int rc = avcodec_send_packet(dec, pkt);
                av_packet_unref(pkt);
                if (rc < 0) {
                    break;
                }

                while (true) {
                    const int rc = avcodec_receive_frame(dec, frame);
                    if (rc == AVERROR(EAGAIN) || rc == AVERROR_EOF) {
                        break;
                    }
                    if (rc < 0) {
                        break;
                    }

                    int64_t pts = frame->best_effort_timestamp;
                    if (pts == AV_NOPTS_VALUE) {
                        pts = frame->pts;
                    }
                    if (pts == AV_NOPTS_VALUE || pts >= time) {
                        found = true;
                        break;
                    }
                }
            }
            av_packet_free(&pkt);
            if (!found) {
                return false;
            }

            return scale(pm);
        }

    private:
        /**
         * Scale current frame.
         * @param pm target pixmap
         * @return true if frame was written to pixmap
         */
        bool scale(Pixmap& pm) const
        {
            const int rot = display_rotation();
            const bool swap = rot == 90 || rot == 270;

            // For 90/270 rotation the frame has to be scaled in its encoded
            // orientation first and then rotated into the display-oriented
            // target pixmap.
            Pixmap tmp;
            Pixmap* dst = &pm;
            if (swap) {
                tmp.create(Pixmap::ARGB, pm.height(), pm.width());
                dst = &tmp;
            }

            SwsContext* sws = sws_getContext(
                frame->width, frame->height,
                static_cast<AVPixelFormat>(frame->format),
                static_cast<int>(dst->width()), static_cast<int>(dst->height()),
                AV_PIX_FMT_BGRA, SWS_BILINEAR, nullptr, nullptr, nullptr);
            if (!sws) {
                return false;
            }

            uint8_t* const src[4] = { static_cast<uint8_t*>(dst->ptr(0, 0)),
                                      nullptr, nullptr, nullptr };
            const int src_stride[4] = { static_cast<int>(dst->stride()), 0, 0,
                                        0 };
            sws_scale(sws, frame->data, frame->linesize, 0, frame->height, src,
                      src_stride);
            sws_freeContext(sws);

            if (rot != 0) {
                // Pixmap::rotate uses clockwise angles while the display
                // matrix rotation is counterclockwise.
                const size_t angle = (360 - rot) % 360;
                dst->rotate(angle);
                if (swap) {
                    pm.copy(tmp, { .x = 0, .y = 0 });
                }
            }
            return true;
        }

        /**
         * Read rotation metadata from the stream's side data.
         * @return counterclockwise rotation in degrees (multiple of 90)
         */
        [[nodiscard]] int read_rotation() const
        {
            if (st && st->codecpar) {
                const AVPacketSideData* sd =
                    av_packet_side_data_get(st->codecpar->coded_side_data,
                                            st->codecpar->nb_coded_side_data,
                                            AV_PKT_DATA_DISPLAYMATRIX);
                if (sd) {
                    return normalize_rotation(sd->data, sd->size);
                }
            }
            return 0;
        }

        /**
         * Get current frame rotation (frame metadata overrides stream one).
         * @return counterclockwise rotation in degrees (multiple of 90)
         */
        [[nodiscard]] int display_rotation() const
        {
            if (rotation != 0) {
                return rotation;
            }
            const AVFrameSideData* sd =
                av_frame_get_side_data(frame, AV_FRAME_DATA_DISPLAYMATRIX);
            if (sd) {
                return normalize_rotation(sd->data, sd->size);
            }
            return 0;
        }

        /**
         * Normalize rotation angle from a display matrix.
         * @param data matrix data
         * @param size matrix size in bytes
         * @return counterclockwise rotation in degrees (multiple of 90)
         */
        static int normalize_rotation(const uint8_t* data, const size_t size)
        {
            if (!data || size < 9 * sizeof(int32_t)) {
                return 0;
            }
            const double angle =
                av_display_rotation_get(reinterpret_cast<const int32_t*>(data));
            if (std::isnan(angle)) {
                return 0;
            }
            int deg = static_cast<int>(std::lround(angle)) % 360;
            if (deg < 0) {
                deg += 360;
            }
            return deg / 90 * 90;
        }

    private:
        MemBuffer mb;                   ///< Memory buffer with stream data
        AVFormatContext* fmt = nullptr; ///< AV format context
        AVIOContext* io = nullptr;      ///< AV context
        AVCodecContext* dec = nullptr;  ///< AV codec context
        AVFrame* frame = nullptr;       ///< Current frame
        AVStream* st = nullptr;         ///< AV stream context
        AVRational tb {};               ///< Duration
        int stream_id = -1;             ///< AV stream id
        int rotation = 0;               ///< Display rotation (counterclockwise)
        std::string cname;              ///< Short codec name
    };
};

// register format in factory
ImageFormatVideo format_video;

} // anonymous namespace
