#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <VapourSynth.h>
#include <VSHelper.h>


#define MAX_DEPTH 25
#define MAX_OPT 9

#define PROP_FRAME "Median_frame"
#define PROP_CLIPS "Median_clips"
#define PROP_SYNC_RADIUS "Median_sync_radius"
#define PROP_SYNC_METRICS "Median_sync_metrics"


enum MedianFilterTypes {
    Median,
    TemporalMedian,
    MedianBlend
};


static const char *filter_names[3] = {
    "Median",
    "TemporalMedian",
    "MedianBlend"
};


struct MedianData;


static inline void sortPixels(uint8_t &a, uint8_t &b) {
    if (a > b)
        std::swap(a, b);
}


template <int depth>
static void processPlaneFast(const uint8_t *srcp[MAX_DEPTH], uint8_t *dstp, int width, int height, int stride, const MedianData *) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint8_t v0 = srcp[0][x];
            uint8_t v1 = srcp[1][x];
            uint8_t v2 = srcp[2][x];
            uint8_t v3 = srcp[3][x];
            uint8_t v4 = srcp[4][x];
            uint8_t v5 = srcp[5][x];
            uint8_t v6 = srcp[6][x];
            uint8_t v7 = srcp[7][x];
            uint8_t v8 = srcp[8][x];

            if (depth == 3) {
                dstp[x] = std::max(std::min(v0, v1),
                                   std::min(std::max(v0, v1),
                                            v2));
            } else if (depth == 5) {
                sortPixels(v0, v1); sortPixels(v3, v4); sortPixels(v0, v3);
                sortPixels(v1, v4); sortPixels(v1, v2); sortPixels(v2, v3);
                sortPixels(v1, v2);

                dstp[x] = v2;
            } else if (depth == 7) {
                sortPixels(v0, v5); sortPixels(v0, v3); sortPixels(v1, v6);
                sortPixels(v2, v4); sortPixels(v0, v1); sortPixels(v3, v5);
                sortPixels(v2, v6); sortPixels(v2, v3); sortPixels(v3, v6);
                sortPixels(v4, v5); sortPixels(v1, v4); sortPixels(v1, v3);
                sortPixels(v3, v4);

                dstp[x] = v3;
            } else if (depth == 9) {
                sortPixels(v1, v2); sortPixels(v4, v5); sortPixels(v7, v8);
                sortPixels(v0, v1); sortPixels(v3, v4); sortPixels(v6, v7);
                sortPixels(v1, v2); sortPixels(v4, v5); sortPixels(v7, v8);
                sortPixels(v0, v3); sortPixels(v5, v8); sortPixels(v4, v7);
                sortPixels(v3, v6); sortPixels(v1, v4); sortPixels(v2, v5);
                sortPixels(v4, v7); sortPixels(v4, v2); sortPixels(v6, v4);
                sortPixels(v4, v2);

                dstp[x] = v4;
            }
        }

        for (int i = 0; i < depth; i++)
            srcp[i] += stride;
        dstp += stride;
    }
}


struct MedianData {
    VSNodeRef *clips[MAX_DEPTH];
    const VSVideoInfo *vi;

    int process[3];

    int radius;
    int low;
    int high;
    int sync;
    int samples;
    bool debug;

    MedianFilterTypes filter_type;

    int depth;
    int blend;

    decltype(processPlaneFast<3>) *process_plane;
};


static void processPlaneSlow(const uint8_t *srcp[MAX_DEPTH], uint8_t *dstp, int width, int height, int stride, const MedianData *d) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint8_t values[MAX_DEPTH];

            for (int i = 0; i < d->depth; i++)
                values[i] = srcp[i][x];

            int sum = 0;

            if (d->blend != d->depth)
                std::sort(values, values + d->depth);

            for (int i = d->low; i < d->low + d->blend; i++)
                sum += values[i];

            dstp[x] = sum / d->blend;
        }

        for (int i = 0; i < d->depth; i++)
            srcp[i] += stride;
        dstp += stride;
    }
}


static double compareFrames(const VSFrameRef *src1, const VSFrameRef *src2, int points, const VSAPI *vsapi) {
    const uint8_t *src1p = vsapi->getReadPtr(src1, 0);
    const uint8_t *src2p = vsapi->getReadPtr(src2, 0);

    int width = vsapi->getFrameWidth(src1, 0);
    int height = vsapi->getFrameHeight(src1, 0);
    int stride = vsapi->getStride(src1, 0);

    int length = width * height;

    if (points < 1 || points > length)
        points = length;

    int step = length / points;

    int64_t sum = 0;
    int effective_points = 0;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += step) {
            sum += std::abs(src1p[x] - src2p[x]);
            effective_points++;
        }

        src1p += stride;
        src2p += stride;
    }

    double difference = (100.0 * sum) / (255.0 * effective_points);

    return 100.0 - difference;
}


static void VS_CC MedianInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
    (void)in;
    (void)out;
    (void)core;

    MedianData *d = (MedianData *) *instanceData;

    vsapi->setVideoInfo(d->vi, 1, node);
}


static const VSFrameRef *VS_CC MedianGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    (void)frameData;

    const MedianData *d = (const MedianData *) *instanceData;

    if (activationReason == arInitial) {
        if (d->filter_type == TemporalMedian) {
            for (int i = 0; i < d->depth; i++)
                vsapi->requestFrameFilter(std::max(0, n - d->radius + i), d->clips[0], frameCtx);
        } else if (d->sync > 0) {
            vsapi->requestFrameFilter(n, d->clips[0], frameCtx);

            for (int i = 1; i < d->depth; i++) {
                int radius = d->sync;

                for (int j = -radius; j <= radius; j++)
                    vsapi->requestFrameFilter(std::max(0, n + j), d->clips[i], frameCtx);
            }
        } else {
            for (int i = 0; i < d->depth; i++)
                vsapi->requestFrameFilter(n, d->clips[i], frameCtx);
        }
    } else if (activationReason == arAllFramesReady) {
        const VSFrameRef *src[MAX_DEPTH] = { nullptr };

        double best[MAX_DEPTH] = { 0 };
        int match[MAX_DEPTH] = { 0 };

        if (d->filter_type == TemporalMedian) {
            for (int i = 0; i < d->depth; i++)
                src[i] = vsapi->getFrameFilter(std::max(0, n - d->radius + i), d->clips[0], frameCtx);
        } else if (d->sync > 0) {
            src[0] = vsapi->getFrameFilter(n, d->clips[0], frameCtx);

            for (int i = 1; i < d->depth; i++) {
                int radius = d->sync;

                for (int j = -radius; j <= radius; j++) {
                    const VSFrameRef *temp = vsapi->getFrameFilter(std::max(0, n + j), d->clips[i], frameCtx);

                    double similarity = compareFrames(src[0], temp, d->samples, vsapi);

                    if (similarity > best[i]) {
                        best[i] = similarity;
                        match[i] = j;
                    }

                    vsapi->freeFrame(temp);
                }

                src[i] = vsapi->getFrameFilter(std::max(0, n + match[i]), d->clips[i], frameCtx);
            }
        } else {
            for (int i = 0; i < d->depth; i++)
                src[i] = vsapi->getFrameFilter(n, d->clips[i], frameCtx);
        }


        const VSFrameRef *source_frame = src[0];
        if (d->filter_type == TemporalMedian)
            source_frame = src[d->low];

        const VSFrameRef *plane_src[3] = {
            d->process[0] ? nullptr : source_frame,
            d->process[1] ? nullptr : source_frame,
            d->process[2] ? nullptr : source_frame
        };

        int planes[3] = { 0, 1, 2 };

        VSFrameRef *dst = vsapi->newVideoFrame2(d->vi->format, d->vi->width, d->vi->height, plane_src, planes, source_frame, core);

        for (int plane = 0; plane < d->vi->format->numPlanes; plane++) {
            if (!d->process[plane])
                continue;

            const uint8_t *srcp[MAX_DEPTH];

            for (int i = 0; i < d->depth; i++)
                srcp[i] = vsapi->getReadPtr(src[i], plane);

            uint8_t *dstp = vsapi->getWritePtr(dst, plane);

            int width = vsapi->getFrameWidth(dst, plane);
            int height = vsapi->getFrameHeight(dst, plane);
            int stride = vsapi->getStride(dst, plane);

            d->process_plane(srcp, dstp, width, height, stride, d);
        }


        if (d->debug) {
            VSMap *props = vsapi->getFramePropsRW(dst);

            vsapi->propSetInt(props, PROP_FRAME, n, paReplace);
            vsapi->propSetInt(props, PROP_CLIPS, d->depth, paReplace);

            if (d->sync > 0) {
                vsapi->propSetInt(props, PROP_SYNC_RADIUS, d->sync, paReplace);

                char metrics[27 * (MAX_DEPTH - 1) + 1] = { 0 };
                int total_printed = 0;

                for (int i = 1; i < d->depth; i++) {
                    int printed = snprintf(metrics + total_printed, 27 + 1, "%2d %+3d %f", i + 1, match[i], best[i]);

                    total_printed += std::min(printed, 27);
                }

                vsapi->propSetData(props, PROP_SYNC_METRICS, metrics, -1, paReplace);
            }
        }


        for (int i = 0; i < d->depth; i++)
            vsapi->freeFrame(src[i]);

        return dst;
    }

    return nullptr;
}


static void VS_CC MedianFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    (void)core;

    MedianData *d = (MedianData *)instanceData;

    for (int i = 0; i < MAX_DEPTH; i++)
        vsapi->freeNode(d->clips[i]);

    free(d);
}


static void VS_CC MedianCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
    MedianData d;
    memset(&d, 0, sizeof(d));

    d.filter_type = (MedianFilterTypes)(intptr_t)userData;

#define MAX_ERROR 100
    char error[MAX_ERROR + 1] = { 0 };

    int err;

    d.radius = int64ToIntS(vsapi->propGetInt(in, "radius", 0, &err));
    if (err)
        d.radius = 1;

    d.low = int64ToIntS(vsapi->propGetInt(in, "low", 0, &err));
    if (err)
        d.low = 1;

    d.high = int64ToIntS(vsapi->propGetInt(in, "high", 0, &err));
    if (err)
        d.high = 1;

    d.sync = int64ToIntS(vsapi->propGetInt(in, "sync", 0, &err));
    if (err)
        d.sync = 0;

    d.samples = int64ToIntS(vsapi->propGetInt(in, "samples", 0, &err));
    if (err)
        d.samples = 4096;

    d.debug = !!vsapi->propGetInt(in, "debug", 0, &err);
    if (err)
        d.debug = false;


    if (d.radius < 1 || d.radius > 12) {
        snprintf(error, MAX_ERROR, "%s: %s", filter_names[d.filter_type], "radius must be between 1 and 12.");
        vsapi->setError(out, error);
        return;
    }

    if (d.sync < 0) {
        snprintf(error, MAX_ERROR, "%s: %s", filter_names[d.filter_type], "sync must not be negative.");
        vsapi->setError(out, error);
        return;
    }

    if (d.samples < 0) {
        snprintf(error, MAX_ERROR, "%s: %s", filter_names[d.filter_type], "samples must not be negative.");
        vsapi->setError(out, error);
        return;
    }

    int num_clips = vsapi->propNumElements(in, d.filter_type == TemporalMedian ? "clip" : "clips");

    if (d.filter_type == TemporalMedian) {
        d.clips[0] = vsapi->propGetNode(in, "clip", 0, nullptr);
    } else {
        if (d.low < 0 || d.low >= num_clips || d.high < 0 || d.high >= num_clips) {
            snprintf(error, MAX_ERROR, "%s: %s", filter_names[d.filter_type], "low and high must be at least 0 and less than the number of clips.");
            vsapi->setError(out, error);
            return;
        }

        if (d.low + d.high >= num_clips) {
            snprintf(error, MAX_ERROR, "%s: %s", filter_names[d.filter_type], "low + high must be less than the number of clips.");
            vsapi->setError(out, error);
            return;
        }

        if (num_clips < 3 || num_clips > 25) {
            snprintf(error, MAX_ERROR, "%s: %s", filter_names[d.filter_type], "The number of clips must be between 3 and 25.");
            vsapi->setError(out, error);
            return;
        }

        if (d.filter_type == Median && num_clips % 2 == 0) {
            snprintf(error, MAX_ERROR, "%s: %s", filter_names[d.filter_type], "Need an odd number of clips.");
            vsapi->setError(out, error);
            return;
        }

        for (int i = 0; i < num_clips; i++)
            d.clips[i] = vsapi->propGetNode(in, "clips", i, nullptr);
    }

    d.vi = vsapi->getVideoInfo(d.clips[0]);


    if (d.vi->width == 0 || d.vi->height == 0 || !d.vi->format || d.vi->format->bitsPerSample > 8) {
        for (int j = 0; j < num_clips; j++)
            vsapi->freeNode(d.clips[j]);
        snprintf(error, MAX_ERROR, "%s: %s", filter_names[d.filter_type], "clips must be 8 bit with constant format and dimensions.");
        vsapi->setError(out, error);
        return;
    }

    for (int i = 1; i < num_clips; i++) {
        const VSVideoInfo *other = vsapi->getVideoInfo(d.clips[i]);

        if (d.vi->width != other->width || d.vi->height != other->height || d.vi->format != other->format) {
            for (int j = 0; j < num_clips; j++)
                vsapi->freeNode(d.clips[j]);
            snprintf(error, MAX_ERROR, "%s: %s", filter_names[d.filter_type], "clips must all have the same format and dimensions.");
            vsapi->setError(out, error);
            return;
        }
    }


    int num_planes = d.vi->format->numPlanes;
    int num_elements = vsapi->propNumElements(in, "planes");

    for (int i = 0; i < 3; i++)
        d.process[i] = (num_elements <= 0);

    for (int i = 0; i < num_elements; i++) {
        int plane = int64ToIntS(vsapi->propGetInt(in, "planes", i, nullptr));

        if (plane < 0 || plane >= num_planes) {
            for (int j = 0; j < num_clips; j++)
                vsapi->freeNode(d.clips[j]);
            snprintf(error, MAX_ERROR, "%s: %s", filter_names[d.filter_type], "plane index out of range.");
            vsapi->setError(out, error);
            return;
        }

        if (d.process[plane]) {
            for (int j = 0; j < num_clips; j++)
                vsapi->freeNode(d.clips[j]);
            snprintf(error, MAX_ERROR, "%s: %s", filter_names[d.filter_type], "plane specified twice.");
            vsapi->setError(out, error);
            return;
        }

        d.process[plane] = 1;
    }


    if (d.filter_type == TemporalMedian) {
        d.low = d.high = d.radius;
        d.depth = d.radius * 2 + 1;
    } else {
        if (d.filter_type == Median)
            d.low = d.high = (num_clips - 1) / 2;
        d.depth = num_clips;
    }

    d.blend = d.depth - d.low - d.high;


    bool fast_processing = d.blend == 1 && d.low == d.high && d.depth <= MAX_OPT;

    if (fast_processing) {
        if (d.depth == 3)
            d.process_plane = processPlaneFast<3>;
        else if (d.depth == 5)
            d.process_plane = processPlaneFast<5>;
        else if (d.depth == 7)
            d.process_plane = processPlaneFast<7>;
        else if (d.depth == 9)
            d.process_plane = processPlaneFast<9>;
    } else {
        d.process_plane = processPlaneSlow;
    }


    MedianData *data = (MedianData *)malloc(sizeof(d));
    *data = d;

    vsapi->createFilter(in, out, "Median", MedianInit, MedianGetFrame, MedianFree, fmParallel, 0, data, core);

    if (d.debug) {
        VSPlugin *text_plugin = vsapi->getPluginById("com.vapoursynth.text", core);

        VSMap *args = vsapi->createMap();

        VSNodeRef *clip = vsapi->propGetNode(out, "clip", 0, nullptr);
        vsapi->propSetNode(args, "clip", clip, paReplace);
        vsapi->freeNode(clip);

        vsapi->propSetData(args, "props", PROP_FRAME, -1, paAppend);
        vsapi->propSetData(args, "props", PROP_CLIPS, -1, paAppend);
        if (d.sync > 0) {
            vsapi->propSetData(args, "props", PROP_SYNC_RADIUS, -1, paAppend);
            vsapi->propSetData(args, "props", PROP_SYNC_METRICS, -1, paAppend);
        }

        VSMap *vsret = vsapi->invoke(text_plugin, "FrameProps", args);
        vsapi->freeMap(args);
        if (vsapi->getError(vsret)) {
            vsapi->setError(out, vsapi->getError(vsret));
            vsapi->freeMap(vsret);
            return;
        }
        clip = vsapi->propGetNode(vsret, "clip", 0, nullptr);
        vsapi->freeMap(vsret);
        vsapi->propSetNode(out, "clip", clip, paReplace);
        vsapi->freeNode(clip);
    }
}
#undef MAX_ERROR


VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
    configFunc("com.nodame.median", "median", "Median of clips", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc(filter_names[Median],
                 "clips:clip[];"
                 "sync:int:opt;"
                 "samples:int:opt;"
                 "debug:int:opt;"
                 "planes:int[]:opt;"
                 , MedianCreate, (void *)Median, plugin);

    registerFunc(filter_names[TemporalMedian],
                 "clip:clip;"
                 "radius:int:opt;"
                 "debug:int:opt;"
                 "planes:int[]:opt;"
                 , MedianCreate, (void *)TemporalMedian, plugin);

    registerFunc(filter_names[MedianBlend],
                 "clips:clip[];"
                 "low:int:opt;"
                 "high:int:opt;"
                 "sync:int:opt;"
                 "samples:int:opt;"
                 "debug:int:opt;"
                 "planes:int[]:opt;"
                 , MedianCreate, (void *)MedianBlend, plugin);
}
