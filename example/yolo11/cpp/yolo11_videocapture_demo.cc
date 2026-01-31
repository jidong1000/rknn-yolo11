#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <chrono>

#include "yolo11.h"
#include "image_utils.h"
#include "file_utils.h"

#include <opencv2/opencv.hpp>

#define MODEL_INPUT_WIDTH   640
#define MODEL_INPUT_HEIGHT  640

#define DEFAULT_MODEL_PATH  "/home/orangepi/Desktop/project/yolo11/lubancat_ai_manual_code-master/example/yolo11/model/yolo11.rknn"
#define DEFAULT_CAMERA_ID   0

static const unsigned char colors[19][3] = {
    {54, 67, 244}, {99, 30, 233}, {176, 39, 156}, {183, 58, 103},
    {181, 81, 63}, {243, 150, 33}, {244, 169, 3}, {212, 188, 0},
    {136, 150, 0}, {80, 175, 76}, {74, 195, 139}, {57, 220, 205},
    {59, 235, 255}, {7, 193, 255}, {0, 152, 255}, {34, 87, 255},
    {72, 85, 121}, {158, 158, 158}, {139, 125, 96}
};

int main(int argc, char **argv)
{
    const char *model_path = DEFAULT_MODEL_PATH;
    int camera_id = DEFAULT_CAMERA_ID;

    cv::VideoCapture cap(camera_id);
    if (!cap.isOpened())
    {
        printf("无法打开摄像头\n");
        return -1;
    }

    cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);

    rknn_app_context_t rknn_app_ctx;
    memset(&rknn_app_ctx, 0, sizeof(rknn_app_context_t));

    image_buffer_t src_image;
    memset(&src_image, 0, sizeof(image_buffer_t));

    init_post_process();
    if (init_yolo11_model(model_path, &rknn_app_ctx) != 0)
    {
        printf("模型初始化失败\n");
        return -1;
    }

    cv::Mat frame, resized, rgb;
    auto last_time = std::chrono::steady_clock::now();
    float fps = 0.f;

    while (true)
    {
        if (!cap.read(frame) || frame.empty())
            continue;

        /* ========= 1. 推理前处理（640×640） ========= */
        cv::resize(frame, resized,
                   cv::Size(MODEL_INPUT_WIDTH, MODEL_INPUT_HEIGHT));
        cv::cvtColor(resized, rgb, cv::COLOR_BGR2RGB);

        src_image.width  = MODEL_INPUT_WIDTH;
        src_image.height = MODEL_INPUT_HEIGHT;
        src_image.format = IMAGE_FORMAT_RGB888;
        src_image.virt_addr = rgb.data;

        object_detect_result_list results;
        if (inference_yolo11_model(&rknn_app_ctx, &src_image, &results) != 0)
            break;

        /* ========= 2. 坐标映射参数 ========= */
        float scale_x = frame.cols / 640.0f;   // = 1.0
        float scale_y = frame.rows / 640.0f;   // = 0.75

        /* ========= 3. 在原始 640×480 frame 上画框 ========= */
        for (int i = 0; i < results.count; i++)
        {
            auto &det = results.results[i];

            int left   = det.box.left   * scale_x;
            int right  = det.box.right  * scale_x;
            int top    = det.box.top    * scale_y;
            int bottom = det.box.bottom * scale_y;

            const unsigned char *c = colors[det.cls_id % 19];
            cv::Scalar color(c[0], c[1], c[2]);

            cv::rectangle(frame,
                cv::Point(left, top),
                cv::Point(right, bottom),
                color, 2);

            char label[64];
            sprintf(label, "%s %.1f%%",
                    coco_cls_to_name(det.cls_id),
                    det.prop * 100);

            int baseLine = 0;
            auto size = cv::getTextSize(label,
                                        cv::FONT_HERSHEY_SIMPLEX,
                                        0.5, 1, &baseLine);

            int x = left;
            int y = top - size.height - baseLine;
            if (y < 0) y = 0;

            cv::rectangle(frame,
                cv::Rect(x, y, size.width, size.height + baseLine),
                color, -1);

            cv::putText(frame, label,
                cv::Point(x, y + size.height),
                cv::FONT_HERSHEY_SIMPLEX,
                0.5, cv::Scalar(255,255,255), 1);
        }

        /* ========= 4. FPS ========= */
        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float, std::milli>(now - last_time).count();
        last_time = now;
        fps = fps * 0.9f + (1000.f / dt) * 0.1f;

        char fps_text[32];
        sprintf(fps_text, "FPS: %.1f", fps);
        cv::putText(frame, fps_text,
                    cv::Point(15, 30),
                    cv::FONT_HERSHEY_SIMPLEX,
                    0.8, cv::Scalar(0,255,0), 2);

        /* ========= 5. 显示 ========= */
        cv::imshow("YOLO11 640x480", frame);
        if (cv::waitKey(1) == 27)
            break;
    }

    release_yolo11_model(&rknn_app_ctx);
    deinit_post_process();
    cap.release();
    cv::destroyAllWindows();
    return 0;
}
