/*
* SteamVR-VoidScene is licensed under the MIT License
* See LICENSE.txt for more information
*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <cstdio>
#include <string>
#include <sstream>
#include <thread>
#include <atomic>
#include <chrono>

#include "openvr.h"

//Globals
static std::atomic<bool> g_DoQuit(false);

//Constants
static const char* k_AppTitle = "SteamVR Void Scene";
static const char* k_AppKey = "elvissteinjr.void_scene";
static const char* k_SettingsKeyBackgroundColor = "BackgroundColor";
static const char* k_SettingsKeyUseDebugCommand = "UseDebugCommand";
static const char* k_LatencyTestingTogglePath = "vrmonitor://debugcommands/latency_testing_toggle";

//Forward function declarations
vr::EVRInitError InitOpenVR();
void ParseCommandLine(LPSTR cmdline);
void LoadSettings(uint32_t& background_color, bool& use_debug_command);
bool CreateDeviceD3D(ID3D11Device** d3d_device, ID3D11DeviceContext** d3d_device_context);
bool CreateTexture(ID3D11Device* d3d_device, ID3D11Texture2D** d3d_texture, uint32_t color);
void VRLoopNormal(vr::Texture_t vr_tex);
void VRLoopSplit(vr::Texture_t vr_tex);

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ INT nCmdShow)
{
    Microsoft::WRL::ComPtr<ID3D11Device>        d3d_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d_device_context;
    Microsoft::WRL::ComPtr<ID3D11Texture2D>     d3d_texture;
    uint32_t background_color = 0;
    bool use_debug_command = false;

    //Init OpenVR
    vr::EVRInitError init_error = InitOpenVR();

    if (init_error != vr::VRInitError_None)
    {
        if (init_error == vr::VRInitError_Init_InitCanceledByUser)  //Just do a clean silent exit if canceled by user
            return 0;

        std::string error_str = "Failed to initialize OpenVR: ";
        error_str.append(vr::VR_GetVRInitErrorAsEnglishDescription(init_error));

        ::MessageBox(nullptr, error_str.c_str(), k_AppTitle, MB_ICONERROR);
        return -1;
    }

    //Parse command line for options to allow changing settings while SteamVR is running
    ParseCommandLine(lpCmdLine);

    //Load the settings
    LoadSettings(background_color, use_debug_command);

    //Init Direct3D
    if (!CreateDeviceD3D(d3d_device.GetAddressOf(), d3d_device_context.GetAddressOf()))
    {
        ::MessageBox(nullptr, "Failed to initialize Direct3D.", k_AppTitle, MB_ICONERROR);
        return -2;
    }

    //Create static eye texture
    if (!CreateTexture(d3d_device.Get(), d3d_texture.GetAddressOf(), background_color))
    {
        ::MessageBox(nullptr, "Failed to create eye texture.", k_AppTitle, MB_ICONERROR);
        return -3;
    }

    vr::Texture_t vr_tex = {0};
    vr_tex.eColorSpace = vr::ColorSpace_Auto;
    vr_tex.eType       = vr::TextureType_DirectX;
    vr_tex.handle      = d3d_texture.Get();

    //Loop until it's time to exit
    if (use_debug_command)
    {
        VRLoopSplit(vr_tex);
    }
    else
    {
        VRLoopNormal(vr_tex);
    }
    
    //Exit after either loop returned
    vr::VR_Shutdown();

    return 0;
}

std::string StringConvertFromUTF16(LPCWSTR str)
{
    std::string stdstr;
    int length_utf8 = ::WideCharToMultiByte(CP_UTF8, 0, str, -1, nullptr, 0, nullptr, nullptr);

    if (length_utf8 != 0)
    {
        auto str_utf8 = std::unique_ptr<char[]>{new char[length_utf8]};

        if (::WideCharToMultiByte(CP_UTF8, 0, str, -1, str_utf8.get(), length_utf8, nullptr, nullptr) != 0)
        {
            stdstr = str_utf8.get();
        }
    }

    return stdstr;
}

vr::EVRInitError InitOpenVR()
{
    vr::EVRInitError init_error = vr::VRInitError_None;
    vr::IVRSystem* vr_system = vr::VR_Init(&init_error, vr::VRApplication_Scene);

    if (init_error != vr::VRInitError_None)
        return init_error;

    //Get absolute application path
    bool do_expand_buffer = true;
    int buffer_size = 1024;
    DWORD read_length;
    std::unique_ptr<WCHAR[]> buffer;

    do
    {
        buffer = std::unique_ptr<WCHAR[]>{new WCHAR[buffer_size]};
        read_length = ::GetModuleFileNameW(nullptr, buffer.get(), buffer_size);

        do_expand_buffer = ( (read_length == buffer_size) && (::GetLastError() == ERROR_INSUFFICIENT_BUFFER) );
        buffer_size += 1024;
    }
    while (do_expand_buffer);

    //Build manifest path and add the manifest
    if (::GetLastError() == ERROR_SUCCESS)
    {
        //SteamVR wants the path in UTF-8
        std::string path_str = StringConvertFromUTF16(buffer.get());

        //We got the full executable path, so let's get the folder part
        std::size_t pos = path_str.rfind("\\");
        path_str = path_str.substr(0, pos + 1);	//Includes trailing backslash

        path_str.append("manifest.vrmanifest");

        //Add manifest
        vr::VRApplications()->AddApplicationManifest(path_str.c_str(), true);
        vr::VRApplications()->IdentifyApplication(::GetCurrentProcessId(), k_AppKey);
    }

    return vr::VRInitError_None;
}

void ParseCommandLine(LPSTR cmdline)
{
    //We use SteamVR's settings backend because we're lazy, but it doesn't allow hand-editing the settings file while SteamVR is running
    //This is why there are some command-line switches to do this instead as settings GUI would be overkill

    std::stringstream ss(cmdline);
    std::string arg;

    while (std::getline(ss, arg, ' ')) 
    {
        if (arg == "--set-background-color")
        {
            std::getline(ss, arg, ' ');

            if (arg.empty())
                break;

            vr::VRSettings()->SetString(k_AppKey, k_SettingsKeyBackgroundColor, arg.c_str());
        }
        else if (arg == "--set-use-debug-command")
        {
            std::getline(ss, arg, ' ');

            if (arg.empty())
                break;

            vr::VRSettings()->SetBool(k_AppKey, k_SettingsKeyUseDebugCommand, (arg == "true"));     //Only "true" is true, better type it correctly
        }
    }
}

void LoadSettings(uint32_t& background_color, bool& use_debug_command)
{
    vr::EVRSettingsError error = vr::VRSettingsError_None;

    //Background Color
    unsigned long color_bgra = 0xFF000000;
    char buffer[16] = "";
    vr::VRSettings()->GetString(k_AppKey, k_SettingsKeyBackgroundColor, buffer, sizeof(buffer), &error);

    if (error == vr::VRSettingsError_None)
    {
        //This is run on little-endian so it's read straight as BGRA which matches our texture format so that's fine
        color_bgra |= std::strtoul( (buffer[0] == '#') ? buffer + 1 : buffer, nullptr, 16);
    }
    else if (error == vr::VRSettingsError_UnsetSettingHasNoDefault) //Set default value in file so it's easier to edit by hand
    {
        vr::VRSettings()->SetString(k_AppKey, k_SettingsKeyBackgroundColor, "#000000");
    }

    background_color = color_bgra;

    //Use Debug Command
    use_debug_command = vr::VRSettings()->GetBool(k_AppKey, k_SettingsKeyUseDebugCommand, &error);

    if (error == vr::VRSettingsError_UnsetSettingHasNoDefault)
    {
        vr::VRSettings()->SetBool(k_AppKey, k_SettingsKeyUseDebugCommand, false);
    }
}

bool CreateDeviceD3D(ID3D11Device** d3d_device, ID3D11DeviceContext** d3d_device_context)
{   
    //Get the adapter recommended by OpenVR
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter_ptr_vr;

    {
        Microsoft::WRL::ComPtr<IDXGIFactory1> factory_ptr;
        int32_t vr_gpu_id;
        vr::VRSystem()->GetDXGIOutputInfo(&vr_gpu_id);

        HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)factory_ptr.GetAddressOf());
        if (!FAILED(hr))
        {
            Microsoft::WRL::ComPtr<IDXGIAdapter> adapter_ptr;
            UINT i = 0;

            while (factory_ptr->EnumAdapters(i, adapter_ptr.GetAddressOf()) != DXGI_ERROR_NOT_FOUND)
            {
                if (i == vr_gpu_id)
                {
                    adapter_ptr_vr = adapter_ptr;
                    break;
                }

                ++i;
            }
        }
    }

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };

    if (adapter_ptr_vr != nullptr)
    {
        if (D3D11CreateDevice(adapter_ptr_vr.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0, featureLevelArray, 2, D3D11_SDK_VERSION, d3d_device, &featureLevel, d3d_device_context) != S_OK)
        {
            return false;
        }
    }
    else //Still try /something/, but it probably won't work
    {
        if (D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, featureLevelArray, 2, D3D11_SDK_VERSION, d3d_device, &featureLevel, d3d_device_context) != S_OK)
        {
            return false;
        }
    }

    return true;
}

bool CreateTexture(ID3D11Device* d3d_device, ID3D11Texture2D** d3d_texture, uint32_t color)
{
    D3D11_SUBRESOURCE_DATA init_data = { &color, sizeof(uint32_t), 0 };
    
    D3D11_TEXTURE2D_DESC tex_desc = {0};
    tex_desc.Width            = 1;
    tex_desc.Height           = 1;
    tex_desc.MipLevels        = 1;
    tex_desc.ArraySize        = 1;
    tex_desc.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
    tex_desc.SampleDesc.Count = 1;
    tex_desc.Usage            = D3D11_USAGE_IMMUTABLE;
    tex_desc.BindFlags        = D3D11_BIND_SHADER_RESOURCE;
    tex_desc.CPUAccessFlags   = 0;
    tex_desc.MiscFlags        = D3D11_RESOURCE_MISC_SHARED;

    HRESULT hr = d3d_device->CreateTexture2D(&tex_desc, &init_data, d3d_texture);

    return SUCCEEDED(hr);
}

void VRLoopNormal(vr::Texture_t vr_tex)
{
    bool do_quit = false;

    while (!do_quit)
    {
        vr::VREvent_t vr_event = {0};
        while (vr::VRSystem()->PollNextEvent(&vr_event, sizeof(vr::VREvent_t)))
        {
            switch (vr_event.eventType)
            {
                case vr::VREvent_Quit:
                {
                    vr::VRSystem()->AcknowledgeQuit_Exiting();
                    do_quit = true;
                    break;
                }
            }
        }

        vr::VRCompositor()->Submit(vr::Eye_Left,  &vr_tex);
        vr::VRCompositor()->Submit(vr::Eye_Right, &vr_tex);

        vr::VRCompositor()->WaitGetPoses(nullptr, 0, nullptr, 0);
    }
}

//Is it even safe to do this in a separate thread? It does work though.
void EntryThreadEventLoop()
{
    //The trick here is that the latency toggle debug command makes VRCompositor::WaitGetPoses() suspend the thread until it's toggled off again 
    //while not making SteamVR act like the application is no longer delivering eye textures. Overlays still work as usual too.
    //This means we'll need a separate thread to know when to quit and undo this, though.
    //Killing this process is also a bad idea since the command will still be active for any subsequent VR scene app.
    ::ShellExecuteA(nullptr, nullptr, k_LatencyTestingTogglePath, nullptr, nullptr, SW_SHOWNORMAL);

    while (!g_DoQuit)
    {
        vr::VREvent_t vr_event = {0};
        while (vr::VRSystem()->PollNextEvent(&vr_event, sizeof(vr::VREvent_t)))
        {
            switch (vr_event.eventType)
            {
                case vr::VREvent_Quit:
                {
                    vr::VRSystem()->AcknowledgeQuit_Exiting();
                    g_DoQuit = true;
                    break;
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    ::ShellExecuteA(nullptr, nullptr, k_LatencyTestingTogglePath, nullptr, nullptr, SW_SHOWNORMAL);
}

void VRLoopSplit(vr::Texture_t vr_tex)
{
    std::thread loop_thread(EntryThreadEventLoop);

    while (!g_DoQuit)
    {
        vr::VRCompositor()->Submit(vr::Eye_Left,  &vr_tex);
        vr::VRCompositor()->Submit(vr::Eye_Right, &vr_tex);

        vr::VRCompositor()->WaitGetPoses(nullptr, 0, nullptr, 0);
    }

    loop_thread.join();
}
