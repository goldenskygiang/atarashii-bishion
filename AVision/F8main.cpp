#ifndef UNICODE
#define UNICODE
#endif
#if defined(__GNUG__) || defined(__MINGW64__)
#include <sys/stat.h>
#endif
#include "F8API.h"
#include "windows.h"
#include "webcam-head-tracker.hpp"

#include <fstream>
#include <string>
#include <chrono>
#include <iomanip>
#include <format>
#include <thread>
#include <random>
#include <cstdio>
#include <cmath>

#include <opencv2/highgui/highgui_c.h>
#include <opencv2/highgui/highgui.hpp>

using namespace std;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class AVisionHeadTrackingPlugin
{
private:
    F8MainRibbonTabProxy ribbonTab;
    F8MainRibbonGroupProxy ribbonGroup;
    F8MainRibbonButtonProxy ribbonButton, stopBtn;
    void* p_cbHandle;
    void* p_stopHandle;

    void MoveMouse(int dx, int dy)
    {
        const int K_FACTOR = -4;

        POINT currentPosition;
        GetCursorPos(&currentPosition);
        currentPosition.x += dx * K_FACTOR;
        currentPosition.y += dy * K_FACTOR;
        SetCursorPos(currentPosition.x, currentPosition.y);
    }

    void UnfocusWindowAndSetTransparency()
    {
        HWND hwnd = (HWND)cvGetWindowHandle(WebcamHeadTracker::WindowName.c_str());

        const int transparency = 50;
        SetWindowLong(hwnd, GWL_EXSTYLE, GetWindowLong(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED);
        SetLayeredWindowAttributes(hwnd, 0, transparency, LWA_ALPHA);

        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

        // Remove focus from the window
        ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    }

public:
    AVisionHeadTrackingPlugin()
    {
        // constructor
    }
    ~AVisionHeadTrackingPlugin()
    {
        // destructor
    }

    std::atomic<bool> isCapturing;

    std::thread thd;

    void OnStartBtnClick()
    {
        isCapturing.store(true);
        thd = std::thread(&AVisionHeadTrackingPlugin::TrackHead, this);
        thd.detach();
    }

    void OnStopBtnClick()
    {
        isCapturing.store(false);
    }

    void TrackHead()
    {
        WebcamHeadTracker tracker(WebcamHeadTracker::Debug_Timing | WebcamHeadTracker::Debug_Window);
        if (!tracker.initWebcam()) {
            fprintf(stderr, "No usable webcam found\n");
            return;
        }
        if (!tracker.initPoseEstimator()) {
            fprintf(stderr, "Cannot initialize pose esimator:\n"
                "haarcascade_frontalface_alt.xml and shape_predictor_68_face_landmarks.dat\n"
                "are not where they were when libwebcamheadtracker was built\n");
            return;
        }
        float lastPos[3] = { 0.0f, 0.0f, -1.0f };

        INPUT input = { 0 };
        input.type = INPUT_MOUSE;
        SetCursorPos(960, 540);

        bool initial = true;

        cv::namedWindow(WebcamHeadTracker::WindowName, cv::WINDOW_KEEPRATIO);
        UnfocusWindowAndSetTransparency();

        while (isCapturing.load() && tracker.isReady())
        {
            if (input.mi.dwFlags != MOUSEEVENTF_LEFTDOWN)
            {
                input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
                SendInput(1, &input, sizeof(INPUT));
            }

            tracker.getNewFrame();
            bool gotPose = tracker.computeHeadPose();
            if (gotPose)
            {
                float pos[3];
                tracker.getHeadPosition(pos);
                pos[0] *= 1000.0f;
                pos[1] *= 1000.0f;
                pos[2] *= 1000.0f;
                fprintf(stderr, "position in mm: %+6.1f %+6.1f %+6.1f\n", pos[0], pos[1], pos[2]);
                if (lastPos[2] >= 0.0f)
                {
                    float diff[3] = { pos[0] - lastPos[0], pos[1] - lastPos[1], pos[2] - lastPos[2] };
                    float dist = std::sqrt(diff[0] * diff[0] + diff[1] * diff[1] + diff[2] * diff[2]);
                    fprintf(stderr, "distance to last position in mm: %+6.1f\n", dist);
                }
                float quaternion[4];
                tracker.getHeadOrientation(quaternion);
                float halfAngle = std::acos(quaternion[3]);
                float axis[3];
                axis[0] = quaternion[0] / std::sin(halfAngle);
                axis[1] = quaternion[1] / std::sin(halfAngle);
                axis[2] = quaternion[2] / std::sin(halfAngle);
                fprintf(stderr, "orientation: rotated %+4.1f degrees around axis (%+4.2f %+4.2f %+4.2f)\n",
                    2.0f * halfAngle / (float)M_PI * 180.0f, axis[0], axis[1], axis[2]);

                if (!initial)
                {
                    int dx = pos[0] - lastPos[0];
                    int dy = pos[1] - lastPos[1];
                    MoveMouse(dx, dy);
                }

                lastPos[0] = pos[0];
                lastPos[1] = pos[1];
                lastPos[2] = pos[2];

                initial = false;
            }
        }

        isCapturing.store(false);
        input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
        SendInput(1, &input, sizeof(INPUT));

        cv::destroyAllWindows();
    }
    void StartProgram()
    {
        F8MainFormProxy mainForm = g_applicationServices->GetMainForm();
        F8MainRibbonProxy ribbonMenu = mainForm->GetMainRibbonMenu();
        ribbonTab = ribbonMenu->GetTabByName(L"AVision");
        if (!Assigned(ribbonTab)) {
            ribbonTab = ribbonMenu->CreateTab(L"AVision", 10000);
            ribbonTab->SetCaption(L"AVision");
        }
        ribbonGroup = ribbonTab->CreateGroup(L"HeadTracking", 100);
        ribbonGroup->SetCaption(L"Head Tracking");

        ribbonButton = ribbonGroup->CreateButton(L"EnableLogger");
        ribbonButton->SetCaption(L"Start");
        ribbonButton->SetWidth(120);
        Cb_RibbonMenuItemOnClick callback = std::bind(&AVisionHeadTrackingPlugin::OnStartBtnClick, this);
        p_cbHandle = ribbonButton->SetCallbackOnClick(callback);

        stopBtn = ribbonGroup->CreateButton(L"DisableLogger");
        stopBtn->SetCaption(L"Stop");
        stopBtn->SetTop(ribbonButton->GetTop() + ribbonButton->GetHeight() + 6);
        callback = std::bind(&AVisionHeadTrackingPlugin::OnStopBtnClick, this);
        p_stopHandle = stopBtn->SetCallbackOnClick(callback);
    }
    void StopProgram()
    {
        ribbonButton->UnsetCallbackOnClick(p_cbHandle);
        ribbonGroup->DeleteControl(ribbonButton);
        ribbonTab->DeleteGroup(ribbonGroup);
        if (ribbonTab->GetRibbonGroupsCount() == 0)
            g_applicationServices->GetMainForm()->GetMainRibbonMenu()->DeleteTab(ribbonTab);
    }
};

AVisionHeadTrackingPlugin Plugin;

void StartProgram(void)
{
    //User Main Code Starts Here
    Plugin.StartProgram();
    //User Main Code Ends Here
}
void StopProgram(void)
{
    //User Main Code Starts Here
    Plugin.StopProgram();
    //User Main Code Ends Here
}
