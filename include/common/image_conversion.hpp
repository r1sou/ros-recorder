#pragma once

#include <arm_neon.h>
#include "hobot_cv/hobotcv_imgproc.h"

#include "common/common.h"

struct image_conversion
{
    static void bgr_to_nv12(cv::Mat &bgr, cv::Mat &nv12)
    {
        hobot_cv::hobotcv_color(bgr, nv12, hobot_cv::DCOLOR_BGR2YUV_NV12);
    }

    static void nv12_to_bgr(cv::Mat &nv12, cv::Mat &bgr)
    {
        hobot_cv::hobotcv_color(nv12, bgr, hobot_cv::DCOLOR_YUV2BGR_NV12);
    }

    static void rgb_to_bgr(cv::Mat &rgb, cv::Mat &bgr)
    {
        cv::cvtColor(rgb, bgr, cv::COLOR_RGB2BGR);
    }

    static void opencv_resize(cv::Mat &src, int src_height, int src_width, cv::Mat &dst, int dst_height, int dst_width)
    {
        if (src_height == dst_height && src_width == dst_width)
        {
            dst = src;
        }
        else{
            cv::resize(src, dst, cv::Size(dst_width, dst_height));
        } 
    }

    static void hobotcv_resize(cv::Mat &src, int src_height, int src_width, cv::Mat &dst, int dst_height, int dst_width)
    {
        // the src_h and src_w of src are both in bgr/rgb format
        hobot_cv::hobotcv_resize(src, src_height, src_width, dst, dst_height, dst_width);
    }
};