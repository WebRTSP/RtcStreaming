#include "GstCameraStreamer.h"

#include <cassert>

#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>
#include <gst/webrtc/rtptransceiver.h>

#include <CxxPtr/GlibPtr.h>
#include <CxxPtr/GstPtr.h>

#include "LibGst.h"
#include "Helpers.h"
#include "Log.h"


static const auto Log = GstRtStreamingLog;

GstCameraStreamer::GstCameraStreamer(
    std::optional<VideoResolution> resolution,
    std::optional<std::string> h264Level,
    bool useHwEncoder) :
    _resolution(resolution),
    _h264Level(h264Level),
    _useHwEncoder(useHwEncoder)
{
}

bool GstCameraStreamer::prepare() noexcept
{
    const char* pipelineDesc;
    if(_useHwEncoder) {
        pipelineDesc =
            "libcamerasrc ! "
            "capsfilter name=cameraFilter ! "
            "v4l2h264enc ! "
            "capsfilter name=encoderFilter ! "
            "rtph264pay pt=99 config-interval=-1 ! tee name = tee";
    } else {
        pipelineDesc =
            "libcamerasrc ! "
            "capsfilter name=cameraFilter ! "
            "x264enc ! "
            "capsfilter name=encoderFilter ! "
            "rtph264pay pt=99 config-interval=-1 ! tee name = tee";
    }

    GError* parseError = nullptr;
    GstElementPtr pipelinePtr(gst_parse_launch(pipelineDesc, &parseError));
    GErrorPtr parseErrorPtr(parseError);
    if(parseError) {
        Log()->error("Failed to parse pipeline: {}", parseError->message);
        return false;
    }

    GstElement* pipeline = pipelinePtr.get();
    GstElementPtr teePtr(gst_bin_get_by_name(GST_BIN(pipeline), "tee"));

    GstElementPtr cameraFilterPtr(gst_bin_get_by_name(GST_BIN(pipeline), "cameraFilter"));
    if(_resolution) {
        const std::string caps =
            "video/x-raw,"
            "interlace-mode=(string)progressive,"
            "width=" + std::to_string(_resolution->width) + ","
            "height=" + std::to_string(_resolution->height);
        gst_util_set_object_arg(G_OBJECT(cameraFilterPtr.get()), "caps", caps.c_str());
    } else {
        gst_util_set_object_arg(
            G_OBJECT(cameraFilterPtr.get()),
            "caps",
            "video/x-raw, interlace-mode=(string)progressive");
    }

    GstElementPtr encoderFilterPtr(gst_bin_get_by_name(GST_BIN(pipeline), "encoderFilter"));
    if(_h264Level) {
        const std::string caps =
            "video/x-h264,"
            "profile=(string)constrained-baseline,"
            "level=(string)" + _h264Level.value();
        gst_util_set_object_arg(G_OBJECT(encoderFilterPtr.get()), "caps", caps.c_str());
    } else {
        gst_util_set_object_arg(
            G_OBJECT(encoderFilterPtr.get()),
            "caps",
            "video/x-h264, profile=(string)constrained-baseline,level=(string)4");
    }

    setPipeline(std::move(pipelinePtr));
    setTee(teePtr.get());

    return true;
}
