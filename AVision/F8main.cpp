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

using namespace std;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

std::atomic<bool> FLAG_STOP, FLAG_RESET_VIEW;

LRESULT CALLBACK KeyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0)
    {
        if (wParam == WM_KEYDOWN)
        {
            KBDLLHOOKSTRUCT* kbStruct = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
            if (kbStruct->vkCode == VK_ESCAPE)
            {
                // User pressed Esc, so exit the application
                FLAG_STOP.store(true);
                PostQuitMessage(0);
            }
            else if (kbStruct->vkCode == VK_SPACE)
            {
                int screenWidth = GetSystemMetrics(SM_CXSCREEN);
                int screenHeight = GetSystemMetrics(SM_CYSCREEN);
                SetCursorPos(screenWidth / 2, screenHeight / 2);
                FLAG_RESET_VIEW.store(true);
            }
        }
    }

    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

class AVisionHeadTrackingPlugin
{
private:
    F8MainRibbonTabProxy ribbonTab;
    F8MainRibbonGroupProxy ribbonGroup;
    F8MainRibbonButtonProxy trackBtn, stopBtn;
    F8MainRibbonCheckBoxProxy enableFeedChk;
    void* p_startHandle;
    void* p_stopHandle;

    int screenWidth, screenHeight;

    void MoveMouse(int dx, int dy)
    {
        const int K_FACTOR = 5;

        POINT currentPosition;
        GetCursorPos(&currentPosition);
        currentPosition.x += dx * K_FACTOR;
        currentPosition.y += dy * -K_FACTOR;
        SetCursorPos(currentPosition.x, currentPosition.y);
    }

    void RunKeyboardHook()
    {
        HHOOK hKeyboardHook = SetWindowsHookEx(
            WH_KEYBOARD_LL,
            KeyboardHookProc,
            GetModuleHandle(NULL),
            0
        );

        // Message loop
        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        UnhookWindowsHookEx(hKeyboardHook);
    }

public:
    std::atomic<bool> isCapturing;
    std::thread thdTrackHead, thdKeyboardHook;

    void OnStartBtnClick()
    {
        FLAG_STOP.store(false);
        isCapturing.store(true);
        trackBtn->SetEnabled(!isCapturing.load());
        enableFeedChk->SetEnabled(!isCapturing.load());

        thdTrackHead = std::thread(&AVisionHeadTrackingPlugin::TrackHead, this);
        thdTrackHead.detach();

        thdKeyboardHook = std::thread(&AVisionHeadTrackingPlugin::RunKeyboardHook, this);
        thdKeyboardHook.detach();
    }

    void OnStopBtnClick()
    {
        isCapturing.store(false);
    }

    void TrackHead()
    {
        int previewWindow = enableFeedChk->GetChecked();
        WebcamHeadTracker tracker(WebcamHeadTracker::Debug_Window & previewWindow);

        if (!tracker.initWebcam())
        {
            MessageBox(NULL, L"No usable webcam found",
                std::wstring(WebcamHeadTracker::WindowName.begin(), WebcamHeadTracker::WindowName.end()).c_str(),
                MB_OK | MB_ICONERROR);
            return;
        }
        if (!tracker.initPoseEstimator())
        {
            MessageBox(NULL, L"Cannot initialize pose esimator:\n"
                "haarcascade_frontalface_alt.xml and shape_predictor_68_face_landmarks.dat\n"
                "are not where they were when libwebcamheadtracker was built\n",
                std::wstring(WebcamHeadTracker::WindowName.begin(), WebcamHeadTracker::WindowName.end()).c_str(),
                MB_OK | MB_ICONERROR);
            return;
        }
        float lastPos[3] = { 0.0f, 0.0f, -1.0f };

        screenWidth = GetSystemMetrics(SM_CXSCREEN);
        screenHeight = GetSystemMetrics(SM_CYSCREEN);

        INPUT input = { 0 };
        input.type = INPUT_MOUSE;
        SetCursorPos(screenWidth / 2, screenHeight / 2);

        FLAG_RESET_VIEW.store(true);

        while (isCapturing.load() && tracker.isReady() && !FLAG_STOP.load())
        {
            if ((GetKeyState(VK_LBUTTON) & 0x8000) == 0)
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

                if (!FLAG_RESET_VIEW.load())
                {
                    int dx = pos[0] - lastPos[0];
                    int dy = pos[1] - lastPos[1];
                    MoveMouse(dx, dy);
                }

                lastPos[0] = pos[0];
                lastPos[1] = pos[1];
                lastPos[2] = pos[2];

                FLAG_RESET_VIEW.store(false);
            }
        }

        FLAG_STOP.store(false);
        isCapturing.store(false);
        trackBtn->SetEnabled(!isCapturing.load());
        enableFeedChk->SetEnabled(!isCapturing.load());

        input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
        SendInput(1, &input, sizeof(INPUT));
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
        F8MainRibbonPanelProxy btnPanel = ribbonGroup->CreatePanel(L"BtnPanel");
        F8MainRibbonPanelProxy chkPanel = ribbonGroup->CreatePanel(L"ChkPanel");

        trackBtn = btnPanel->CreateButton(L"BtnStart");
        trackBtn->SetCaption(L"Start");
        Cb_RibbonMenuItemOnClick callback = std::bind(&AVisionHeadTrackingPlugin::OnStartBtnClick, this);
        p_startHandle = trackBtn->SetCallbackOnClick(callback);

        stopBtn = btnPanel->CreateButton(L"BtnStop");
        stopBtn->SetCaption(L"Stop");
        stopBtn->SetLeft(trackBtn->GetLeft());
        stopBtn->SetTop(trackBtn->GetTop() + trackBtn->GetHeight() + 6);
        callback = std::bind(&AVisionHeadTrackingPlugin::OnStopBtnClick, this);
        p_stopHandle = stopBtn->SetCallbackOnClick(callback);

        enableFeedChk = chkPanel->CreateCheckBox(L"ChkEnableFeed");
        enableFeedChk->SetCaption(L"Webcam preview");
        enableFeedChk->SetTop(trackBtn->GetTop() + 3);

        btnPanel->SetWidth(trackBtn->GetWidth() + 6);
        btnPanel->SetHeight(2 * trackBtn->GetHeight() + 6);
        chkPanel->SetWidth(enableFeedChk->GetWidth());
        chkPanel->SetHeight(enableFeedChk->GetHeight() + 3);

        FLAG_STOP.store(false);
    }

    void StopProgram()
    {
        trackBtn->UnsetCallbackOnClick(p_startHandle);
        stopBtn->UnsetCallbackOnClick(p_stopHandle);

        ribbonGroup->DeleteControl(trackBtn);
        ribbonGroup->DeleteControl(stopBtn);
        ribbonGroup->DeleteControl(enableFeedChk);

        ribbonTab->DeleteGroup(ribbonGroup);
        if (ribbonTab->GetRibbonGroupsCount() == 0)
            g_applicationServices->GetMainForm()->GetMainRibbonMenu()->DeleteTab(ribbonTab);
    }
};

AVisionHeadTrackingPlugin Plugin;

void StartProgram(void)
{
    Plugin.StartProgram();
}
void StopProgram(void)
{
    Plugin.StopProgram();
}
