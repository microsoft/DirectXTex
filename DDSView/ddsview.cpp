//--------------------------------------------------------------------------------------
// File: DDSView.cpp
//
// DirectX 11 DDS File Viewer sample for DirectXTex
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//--------------------------------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cwchar>
#include <iterator>
#include <tuple>

#include <dxgiformat.h>
#include <d3d11_1.h>

#include <DirectXMath.h>

#pragma warning(disable : 4619 4616 26812)

#include "DirectXTex.h"

#include <shellapi.h>

using namespace DirectX;

//--------------------------------------------------------------------------------------
#define IDI_MAIN_ICON 100

//--------------------------------------------------------------------------------------
#pragma pack(push,1)
struct SimpleVertex
{
    XMFLOAT4 Pos;
    XMFLOAT4 Tex;
};

struct CBArrayControl
{
    float Index;
    float pad[3];
};
#pragma pack(pop)

//--------------------------------------------------------------------------------------

namespace
{
#include "ddsview_vs.inc"
#include "ddsview_ps1D.inc"
#include "ddsview_ps1Darray.inc"
#include "ddsview_ps2D.inc"
#include "ddsview_ps2Darray.inc"
#include "ddsview_ps3D.inc"
#include "ddsview_psCube.inc"
}

//--------------------------------------------------------------------------------------
namespace
{
    HINSTANCE                   g_hInst = nullptr;
    HWND                        g_hWnd = nullptr;
    D3D_DRIVER_TYPE             g_driverType = D3D_DRIVER_TYPE_NULL;
    D3D_FEATURE_LEVEL           g_featureLevel = D3D_FEATURE_LEVEL_11_0;
    ID3D11Device*               g_pd3dDevice = nullptr;
    ID3D11DeviceContext*        g_pImmediateContext = nullptr;
    IDXGISwapChain*             g_pSwapChain = nullptr;
    ID3D11RenderTargetView*     g_pRenderTargetView = nullptr;
    ID3D11Texture2D*            g_pDepthStencil = nullptr;
    ID3D11DepthStencilView*     g_pDepthStencilView = nullptr;
    ID3D11VertexShader*         g_pVertexShader = nullptr;
    ID3D11PixelShader*          g_pPixelShader = nullptr;
    ID3D11InputLayout*          g_pVertexLayout = nullptr;
    ID3D11Buffer*               g_pVertexBuffer = nullptr;
    ID3D11Buffer*               g_pIndexBuffer = nullptr;
    ID3D11Buffer*               g_pCBArrayControl = nullptr;
    ID3D11ShaderResourceView*   g_pSRV = nullptr;
    ID3D11BlendState*           g_AlphaBlendState = nullptr;
    ID3D11SamplerState*         g_pSamplerLinear = nullptr;

    UINT                        g_iCurrentIndex = 0;
    UINT                        g_iMaxIndex = 1;

    UINT                        g_iIndices = 0;

    LPCWSTR                     g_szAppName = L"DDSView";
}

//--------------------------------------------------------------------------------------
HRESULT InitWindow(HINSTANCE hInstance, int nCmdShow, const TexMetadata& mdata);
HRESULT InitDevice(const TexMetadata& mdata);
void CleanupDevice();
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void Render();

//--------------------------------------------------------------------------------------
#pragma warning( suppress : 6262 )
int WINAPI wWinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE /*hPrevInstance*/,
    _In_ LPWSTR lpCmdLine,
    _In_ int nCmdShow)
{
    static const wchar_t* c_usage = L"Usage: ddsview [-forcesrgb] <filename>";

    if (!*lpCmdLine)
    {
        MessageBoxW(nullptr, c_usage, g_szAppName, MB_OK | MB_ICONEXCLAMATION);
        return 0;
    }

    int argc = 0;
    auto argList = CommandLineToArgvW(lpCmdLine, &argc);

    const wchar_t* filename = nullptr;
    bool forceSRGB = false;
    for(int i = 0; i < argc; ++i)
    {
        if (*argList[i] == '-' || *argList[i] == '/')
        {
            if (_wcsicmp(argList[i]+1, L"forcesrgb") == 0)
            {
                forceSRGB = true;
            }
        }
        else if (!filename)
        {
            filename = argList[i];
        }
    }

    if (!filename)
    {
        MessageBoxW(nullptr, c_usage, g_szAppName, MB_OK | MB_ICONEXCLAMATION);
        return 1;
    }

    TexMetadata mdata;
    HRESULT hr = GetMetadataFromDDSFile(filename, DDS_FLAGS_NONE, mdata);
    if (FAILED(hr))
    {
        wchar_t buff[2048] = {};
        swprintf_s(buff, L"Failed to open texture file\n\nFilename = %ls\nHRESULT %08X", filename, static_cast<unsigned int>(hr));
        MessageBoxW(nullptr, buff, g_szAppName, MB_OK | MB_ICONEXCLAMATION);
        return 1;
    }

    if (FAILED(InitWindow(hInstance, nCmdShow, mdata)))
        return 1;

    SetWindowTextW(g_hWnd, filename);

    if (FAILED(InitDevice(mdata)))
    {
        CleanupDevice();
        return 1;
    }

    if (mdata.dimension == TEX_DIMENSION_TEXTURE3D)
    {
        if (mdata.arraySize > 1)
        {
            wchar_t buff[2048] = {};
            swprintf_s(buff, L"Arrays of volume textures are not supported\n\nFilename = %ls\nArray size %zu", filename, mdata.arraySize);
            MessageBoxW(nullptr, buff, g_szAppName, MB_OK | MB_ICONEXCLAMATION);
            return 1;
        }

        g_iMaxIndex = static_cast<UINT>(mdata.depth);
    }
    else if (mdata.arraySize > 1)
    {
        if (g_featureLevel < D3D_FEATURE_LEVEL_10_0)
        {
            wchar_t buff[2048] = {};
            swprintf_s(buff, L"Texture arrays require DirectX 10 hardware or later\n\nFilename = %ls\nArray size %zu", filename, mdata.arraySize);
            MessageBoxW(nullptr, buff, g_szAppName, MB_OK | MB_ICONEXCLAMATION);
            return 1;
        }

        g_iMaxIndex = static_cast<UINT>(mdata.arraySize);
    }

    switch (mdata.format)
    {
    case DXGI_FORMAT_BC4_TYPELESS:
    case DXGI_FORMAT_BC4_SNORM:
    case DXGI_FORMAT_BC4_UNORM:
    case DXGI_FORMAT_BC5_TYPELESS:
    case DXGI_FORMAT_BC5_SNORM:
    case DXGI_FORMAT_BC5_UNORM:
        if (g_featureLevel < D3D_FEATURE_LEVEL_10_0)
        {
            wchar_t buff[2048] = {};
            swprintf_s(buff, L"BC4/BC5 requires DirectX 10 hardware or later\n\nFilename = %ls\nDXGI Format %d\nFeature Level %d", filename, mdata.format, g_featureLevel);
            MessageBoxW(nullptr, buff, g_szAppName, MB_OK | MB_ICONEXCLAMATION);
            return 1;
        }
        break;

    case DXGI_FORMAT_BC6H_TYPELESS:
    case DXGI_FORMAT_BC6H_UF16:
    case DXGI_FORMAT_BC6H_SF16:
    case DXGI_FORMAT_BC7_TYPELESS:
    case DXGI_FORMAT_BC7_UNORM:
    case DXGI_FORMAT_BC7_UNORM_SRGB:
        if (g_featureLevel < D3D_FEATURE_LEVEL_11_0)
        {
            wchar_t buff[2048] = {};
            swprintf_s(buff, L"BC6H/BC7 requires DirectX 11 hardware or later\n\nFilename = %ls\nDXGI Format %d\nFeature Level %d", filename, mdata.format, g_featureLevel);
            MessageBoxW(nullptr, buff, g_szAppName, MB_OK | MB_ICONEXCLAMATION);
            return 1;
        }
        break;

    default:
        {
            UINT flags = 0;
            hr = g_pd3dDevice->CheckFormatSupport(mdata.format, &flags);
            constexpr UINT required = D3D11_FORMAT_SUPPORT_TEXTURE1D | D3D11_FORMAT_SUPPORT_TEXTURE2D | D3D11_FORMAT_SUPPORT_TEXTURE3D;
            if (FAILED(hr) || !(flags & required))
            {
                wchar_t buff[2048] = {};
                swprintf_s(buff, L"Format not supported by this DirectX hardware\n\nFilename = %ls\nDXGI Format %d\nFeature Level %d\nHRESULT = %08X", filename, mdata.format, g_featureLevel, static_cast<unsigned int>(hr));
                MessageBoxW(nullptr, buff, g_szAppName, MB_OK | MB_ICONEXCLAMATION);
                return 1;
            }
        }
        break;
    }

    ScratchImage image;
    hr = LoadFromDDSFile(filename, DDS_FLAGS_NONE, &mdata, image);
    if (FAILED(hr))
    {
        wchar_t buff[2048] = {};
        swprintf_s(buff, L"Failed to load texture file\n\nFilename = %ls\nHRESULT %08X", filename, static_cast<unsigned int>(hr));
        MessageBoxW(nullptr, buff, g_szAppName, MB_OK | MB_ICONEXCLAMATION);
        return 1;
    }

    // Special case to make sure Texture cubes remain arrays
    mdata.miscFlags &= ~TEX_MISC_TEXTURECUBE;

    if (forceSRGB)
    {
        mdata.format = MakeSRGB(mdata.format);
        image.OverrideFormat(mdata.format);
    }

    hr = CreateShaderResourceView(g_pd3dDevice, image.GetImages(), image.GetImageCount(), mdata, &g_pSRV);
    if (FAILED(hr))
    {
        wchar_t buff[2048] = {};
        swprintf_s(buff, L"Failed creating texture from file\n\nFilename = %ls\nHRESULT = %08X", filename, static_cast<unsigned int>(hr));
        MessageBoxW(nullptr, buff, g_szAppName, MB_OK | MB_ICONEXCLAMATION);
        return 1;
    }

    // Main message loop
    MSG msg = {};
    while (WM_QUIT != msg.message)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            Render();
        }
    }

    CleanupDevice();

    return static_cast<int>(msg.wParam);
}

//--------------------------------------------------------------------------------------
HRESULT InitWindow(
    HINSTANCE hInstance,
    int nCmdShow,
    const TexMetadata& mdata)
{
    // Register class
    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIconW(hInstance, reinterpret_cast<LPCWSTR>(IDI_MAIN_ICON));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wcex.lpszClassName = L"DDSViewWindowClass";
    wcex.hIconSm = LoadIconW(wcex.hInstance, reinterpret_cast<LPCWSTR>(IDI_MAIN_ICON));
    if (!RegisterClassExW(&wcex))
        return E_FAIL;

    // Create window
    g_hInst = hInstance;
    RECT rc = { 0, 0, 640, 480 };

    int cxborder = GetSystemMetrics(SM_CXBORDER);
    int cxedge = GetSystemMetrics(SM_CXEDGE);
    int screenX = GetSystemMetrics(SM_CXSCREEN) - std::max(cxborder, cxedge);
    if (rc.right < static_cast<LONG>(mdata.width))
        rc.right = static_cast<LONG>(mdata.width);
    if (rc.right > screenX)
        rc.right = screenX;

    int cyborder = GetSystemMetrics(SM_CYBORDER);
    int cyedge = GetSystemMetrics(SM_CYEDGE);
    int screenY = GetSystemMetrics(SM_CYSCREEN) - std::max(cyborder, cyedge);
    if (rc.bottom < static_cast<LONG>(mdata.height))
        rc.bottom = static_cast<LONG>(mdata.height);
    if (rc.bottom > screenY)
        rc.bottom = screenY;

    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    g_hWnd = CreateWindowW(L"DDSViewWindowClass", g_szAppName, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance,
        nullptr);
    if (!g_hWnd)
        return E_FAIL;

    ShowWindow(g_hWnd, nCmdShow);

    return S_OK;
}


//--------------------------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            std::ignore = BeginPaint(hWnd, &ps);
            EndPaint(hWnd, &ps);
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    case WM_KEYDOWN:
        if (wParam == VK_RIGHT)
        {
            if (g_iCurrentIndex < g_iMaxIndex - 1)
                ++g_iCurrentIndex;
        }
        else if (wParam == VK_LEFT)
        {
            if (g_iCurrentIndex > 0)
            {
                --g_iCurrentIndex;
            }
        }
        else if (wParam >= '0' && wParam <= '9')
        {
            UINT index = (wParam == '0') ? 10u : static_cast<UINT>(wParam - '1');
            if (index < g_iMaxIndex)
                g_iCurrentIndex = index;
        }
        InvalidateRect(hWnd, nullptr, FALSE);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}


//--------------------------------------------------------------------------------------
HRESULT InitDevice(const TexMetadata& mdata)
{
    HRESULT hr = S_OK;

    RECT rc;
    GetClientRect(g_hWnd, &rc);
    auto width = static_cast<UINT>(rc.right - rc.left);
    auto height = static_cast<UINT>(rc.bottom - rc.top);

    UINT createDeviceFlags = 0;
#if defined( DEBUG ) || defined( _DEBUG )
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_DRIVER_TYPE driverTypes[] =
    {
        D3D_DRIVER_TYPE_HARDWARE,
        D3D_DRIVER_TYPE_WARP,
        D3D_DRIVER_TYPE_REFERENCE,
    };
    constexpr UINT numDriverTypes = static_cast<UINT>(std::size(driverTypes));

    D3D_FEATURE_LEVEL featureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    constexpr UINT numFeatureLevels = static_cast<UINT>(std::size(featureLevels));

    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = width;
    sd.BufferDesc.Height = height;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = g_hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;

    for (UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++)
    {
        g_driverType = driverTypes[driverTypeIndex];

        // See https://walbourn.github.io/anatomy-of-direct3d-11-create-device/
        hr = D3D11CreateDeviceAndSwapChain(nullptr, g_driverType, nullptr,
            createDeviceFlags, featureLevels, numFeatureLevels,
            D3D11_SDK_VERSION, &sd,
            &g_pSwapChain, &g_pd3dDevice, &g_featureLevel, &g_pImmediateContext);
        if (SUCCEEDED(hr))
            break;
    }
    if (FAILED(hr))
        return hr;

    // Create a render target view
    ID3D11Texture2D* pBackBuffer = nullptr;
    hr = g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (FAILED(hr))
        return hr;

    D3D11_RENDER_TARGET_VIEW_DESC vd = {};
    vd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    vd.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    hr = g_pd3dDevice->CreateRenderTargetView(pBackBuffer, &vd, &g_pRenderTargetView);
    pBackBuffer->Release();
    if (FAILED(hr))
        return hr;

    // Create depth stencil texture
    D3D11_TEXTURE2D_DESC descDepth = {};
    descDepth.Width = width;
    descDepth.Height = height;
    descDepth.MipLevels = 1;
    descDepth.ArraySize = 1;
    descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    descDepth.SampleDesc.Count = 1;
    descDepth.Usage = D3D11_USAGE_DEFAULT;
    descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    hr = g_pd3dDevice->CreateTexture2D(&descDepth, nullptr, &g_pDepthStencil);
    if (FAILED(hr))
        return hr;

    // Create the depth stencil view
    D3D11_DEPTH_STENCIL_VIEW_DESC descDSV = {};
    descDSV.Format = descDepth.Format;
    descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    descDSV.Texture2D.MipSlice = 0;
    hr = g_pd3dDevice->CreateDepthStencilView(g_pDepthStencil, &descDSV, &g_pDepthStencilView);
    if (FAILED(hr))
        return hr;

    g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, g_pDepthStencilView);

    // Setup the viewport
    const D3D11_VIEWPORT vp = { 0.f, 0.f, static_cast<float>(width), static_cast<float>(height), 0.f, 1.f };
    g_pImmediateContext->RSSetViewports(1, &vp);

    // Create the vertex shader
    hr = g_pd3dDevice->CreateVertexShader(g_VS, sizeof(g_VS), nullptr, &g_pVertexShader);
    if (FAILED(hr))
        return hr;

    // Define the input layout
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, sizeof(XMFLOAT4), D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    constexpr UINT numElements = static_cast<UINT>(std::size(layout));

    // Create the input layout
    hr = g_pd3dDevice->CreateInputLayout(layout, numElements, g_VS, sizeof(g_VS), &g_pVertexLayout);
    if (FAILED(hr))
        return hr;

    // Set the input layout
    g_pImmediateContext->IASetInputLayout(g_pVertexLayout);

    // Select the pixel shader
    bool isCubeMap = false;
    bool is1D = false;
    const BYTE* pshader = nullptr;
    size_t pshader_size = 0;

    switch (mdata.dimension)
    {
    case TEX_DIMENSION_TEXTURE1D:
        if (mdata.arraySize > 1)
        {
            pshader = g_PS_1DArray;
            pshader_size = sizeof(g_PS_1DArray);
        }
        else
        {
            pshader = g_PS_1D;
            pshader_size = sizeof(g_PS_1D);
        }
        is1D = true;
        break;

    case TEX_DIMENSION_TEXTURE2D:
        if (mdata.miscFlags & TEX_MISC_TEXTURECUBE)
        {
            pshader = g_PS_Cube;
            pshader_size = sizeof(g_PS_Cube);
            isCubeMap = true;
        }
        else if (mdata.arraySize > 1)
        {
            pshader = g_PS_2DArray;
            pshader_size = sizeof(g_PS_2DArray);
        }
        else
        {
            pshader = g_PS_2D;
            pshader_size = sizeof(g_PS_2D);
        }
        break;

    case TEX_DIMENSION_TEXTURE3D:
        pshader = g_PS_3D;
        pshader_size = sizeof(g_PS_3D);
        break;

    default:
        return E_FAIL;
    }

    assert(pshader && pshader_size > 0);

    // Create the pixel shader
    hr = g_pd3dDevice->CreatePixelShader(pshader, pshader_size, nullptr, &g_pPixelShader);
    if (FAILED(hr))
        return hr;

    // Create vertex buffer
    UINT nverts;
    D3D11_SUBRESOURCE_DATA InitData = {};

    static const SimpleVertex verticesCube[] =
    {
        // Render cubemaps as horizontal cross

        // XPOS
        { XMFLOAT4(.5f, .25f, 0.f, 1.f), XMFLOAT4(0.f, 0.f, 0.f, 0.f) },
        { XMFLOAT4(1.f, .25f, 0.f, 1.f), XMFLOAT4(1.f, 0.f, 0.f, 0.f) },
        { XMFLOAT4(.5f, -.25f, 0.f, 1.f), XMFLOAT4(0.f, 1.f, 0.f, 0.f) },
        { XMFLOAT4(1.f, -.25f, 0.f, 1.f), XMFLOAT4(1.f, 1.f, 0.f, 0.f) },

        // XNEG
        { XMFLOAT4(-.5f, .25f, 0.f, 1.f), XMFLOAT4(0.f, 0.f, 1.f, 0.f) },
        { XMFLOAT4(0.f, .25f, 0.f, 1.f), XMFLOAT4(1.f, 0.f, 1.f, 0.f) },
        { XMFLOAT4(-.5f, -.25f, 0.f, 1.f), XMFLOAT4(0.f, 1.f, 1.f, 0.f) },
        { XMFLOAT4(0.f, -.25f, 0.f, 1.f), XMFLOAT4(1.f, 1.f, 1.f, 0.f) },

        // YPOS
        { XMFLOAT4(-.5f, .75f, 0.f, 1.f), XMFLOAT4(0.f, 0.f, 2.f, 0.f) },
        { XMFLOAT4(0.f, .75f, 0.f, 1.f), XMFLOAT4(1.f, 0.f, 2.f, 0.f) },
        { XMFLOAT4(-.5f, .25f, 0.f, 1.f), XMFLOAT4(0.f, 1.f, 2.f, 0.f) },
        { XMFLOAT4(0.f, .25f, 0.f, 1.f), XMFLOAT4(1.f, 1.f, 2.f, 0.f) },

        // YNEG
        { XMFLOAT4(-.5f, -.25f, 0.f, 1.f), XMFLOAT4(0.f, 0.f, 3.f, 0.f) },
        { XMFLOAT4(0.f, -.25f, 0.f, 1.f), XMFLOAT4(1.f, 0.f, 3.f, 0.f) },
        { XMFLOAT4(-.5f, -.75f, 0.f, 1.f), XMFLOAT4(0.f, 1.f, 3.f, 0.f) },
        { XMFLOAT4(0.f, -.75f, 0.f, 1.f), XMFLOAT4(1.f, 1.f, 3.f, 0.f) },

        // ZPOS
        { XMFLOAT4(0.f, .25f, 0.f, 1.f), XMFLOAT4(0.f, 0.f, 4.f, 0.f) },
        { XMFLOAT4(.5f, .25f, 0.f, 1.f), XMFLOAT4(1.f, 0.f, 4.f, 0.f) },
        { XMFLOAT4(0.f, -.25f, 0.f, 1.f), XMFLOAT4(0.f, 1.f, 4.f, 0.f) },
        { XMFLOAT4(.5f, -.25f, 0.f, 1.f), XMFLOAT4(1.f, 1.f, 4.f, 0.f) },

        // ZNEG
        { XMFLOAT4(-1.f, .25f, 0.f, 1.f), XMFLOAT4(0.f, 0.f, 5.f, 0.f) },
        { XMFLOAT4(-.5f, .25f, 0.f, 1.f), XMFLOAT4(1.f, 0.f, 5.f, 0.f) },
        { XMFLOAT4(-1.f, -.25f, 0.f, 1.f), XMFLOAT4(0.f, 1.f, 5.f, 0.f) },
        { XMFLOAT4(-.5f, -.25f, 0.f, 1.f), XMFLOAT4(1.f, 1.f, 5.f, 0.f) },
    };

    static const SimpleVertex vertices[] =
    {
        { XMFLOAT4(-1.f, 1.f, 0.f, 1.f), XMFLOAT4(0.f, 0.f, 0.f, 0.f) },
        { XMFLOAT4(1.f, 1.f, 0.f, 1.f), XMFLOAT4(1.f, 0.f, 0.f, 0.f) },
        { XMFLOAT4(-1.f, -1.f, 0.f, 1.f), XMFLOAT4(0.f, 1.f, 0.f, 0.f) },
        { XMFLOAT4(1.f, -1.f, 0.f, 1.f), XMFLOAT4(1.f, 1.f, 0.f, 0.f) },
    };

    static const SimpleVertex vertices1D[] =
    {
        { XMFLOAT4(-1.f, .05f, 0.f, 1.f), XMFLOAT4(0.f, 0.f, 0.f, 0.f) },
        { XMFLOAT4(1.f, .05f, 0.f, 1.f), XMFLOAT4(1.f, 0.f, 0.f, 0.f) },
        { XMFLOAT4(-1.f, -.05f, 0.f, 1.f), XMFLOAT4(0.f, 0.f, 0.f, 0.f) },
        { XMFLOAT4(1.f, -.05f, 0.f, 1.f), XMFLOAT4(1.f, 0.f, 0.f, 0.f) },
    };

    if (isCubeMap)
    {
        nverts = static_cast<UINT>(std::size(verticesCube));
        InitData.pSysMem = verticesCube;
    }
    else if (is1D)
    {
        nverts = static_cast<UINT>(std::size(vertices1D));
        InitData.pSysMem = vertices1D;
    }
    else
    {
        nverts = static_cast<UINT>(std::size(vertices));
        InitData.pSysMem = vertices;
    }

    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(SimpleVertex) * nverts;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;
    hr = g_pd3dDevice->CreateBuffer(&bd, &InitData, &g_pVertexBuffer);
    if (FAILED(hr))
        return hr;

    // Set vertex buffer
    UINT stride = sizeof(SimpleVertex);
    UINT offset = 0;
    g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

    // Create index buffer
    static const WORD indicesCube[] =
    {
            0, 1, 2,
            2, 1, 3,
            4, 5, 6,
            6, 5, 7,
            8, 9, 10,
            10, 9, 11,
            12, 13, 14,
            14, 13, 15,
            16, 17, 18,
            18, 17, 19,
            20, 21, 22,
            22, 21, 23
    };

    static const WORD indices[] =
    {
            0, 1, 2,
            2, 1, 3
    };

    if (isCubeMap)
    {
        g_iIndices = static_cast<UINT>(std::size(indicesCube));
        InitData.pSysMem = indicesCube;
    }
    else
    {
        g_iIndices = static_cast<UINT>(std::size(indices));
        InitData.pSysMem = indices;
    }

    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = g_iIndices * sizeof(WORD);
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    bd.CPUAccessFlags = 0;
    hr = g_pd3dDevice->CreateBuffer(&bd, &InitData, &g_pIndexBuffer);
    if (FAILED(hr))
        return hr;

    // Set index buffer
    g_pImmediateContext->IASetIndexBuffer(g_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

    // Set primitive topology
    g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Create the constant buffers
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(CBArrayControl);
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = 0;
    hr = g_pd3dDevice->CreateBuffer(&bd, nullptr, &g_pCBArrayControl);
    if (FAILED(hr))
        return hr;

    // Create the state objects
    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    hr = g_pd3dDevice->CreateSamplerState(&sampDesc, &g_pSamplerLinear);
    if (FAILED(hr))
        return hr;

    D3D11_BLEND_DESC dsc =
    {
        false,
        false,
        {
            {
            true,
            D3D11_BLEND_SRC_ALPHA,
            D3D11_BLEND_INV_SRC_ALPHA,
            D3D11_BLEND_OP_ADD,
            D3D11_BLEND_ZERO,
            D3D11_BLEND_ZERO,
            D3D11_BLEND_OP_ADD,
            D3D11_COLOR_WRITE_ENABLE_ALL
            },
        // ...
    }
    };
    hr = g_pd3dDevice->CreateBlendState(&dsc, &g_AlphaBlendState);
    if (FAILED(hr))
        return hr;

    return S_OK;
}


//--------------------------------------------------------------------------------------
void Render()
{
    float ClearColor[4] = { 0.f, 1.f, 1.f, 1.0f }; //red,green,blue,alpha
    g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, ClearColor);
    g_pImmediateContext->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

    float bf[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    g_pImmediateContext->OMSetBlendState(g_AlphaBlendState, bf, 0xffffffff);

    CBArrayControl cb = {};
    cb.Index = static_cast<float>(g_iCurrentIndex);
    g_pImmediateContext->UpdateSubresource(g_pCBArrayControl, 0, nullptr, &cb, 0, 0);

    g_pImmediateContext->VSSetShader(g_pVertexShader, nullptr, 0);
    g_pImmediateContext->PSSetShader(g_pPixelShader, nullptr, 0);
    g_pImmediateContext->PSSetConstantBuffers(0, 1, &g_pCBArrayControl);
    g_pImmediateContext->PSSetShaderResources(0, 1, &g_pSRV);
    g_pImmediateContext->PSSetSamplers(0, 1, &g_pSamplerLinear);
    g_pImmediateContext->DrawIndexed(g_iIndices, 0, 0);

    g_pSwapChain->Present(0, 0);
}


//--------------------------------------------------------------------------------------
void CleanupDevice()
{
    if (g_pImmediateContext) g_pImmediateContext->ClearState();

    if (g_pSamplerLinear) g_pSamplerLinear->Release();
    if (g_AlphaBlendState) g_AlphaBlendState->Release();
    if (g_pSRV) g_pSRV->Release();
    if (g_pVertexBuffer) g_pVertexBuffer->Release();
    if (g_pIndexBuffer) g_pIndexBuffer->Release();
    if (g_pCBArrayControl) g_pCBArrayControl->Release();
    if (g_pVertexLayout) g_pVertexLayout->Release();
    if (g_pVertexShader) g_pVertexShader->Release();
    if (g_pPixelShader) g_pPixelShader->Release();
    if (g_pDepthStencil) g_pDepthStencil->Release();
    if (g_pDepthStencilView) g_pDepthStencilView->Release();
    if (g_pRenderTargetView) g_pRenderTargetView->Release();
    if (g_pSwapChain) g_pSwapChain->Release();
    if (g_pImmediateContext) g_pImmediateContext->Release();
    if (g_pd3dDevice) g_pd3dDevice->Release();
}
