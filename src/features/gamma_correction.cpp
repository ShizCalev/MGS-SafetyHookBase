#include "gamma_correction.hpp"
/*
#include "d3d11_api.hpp"
#include <d3dcompiler.h>
#include "spdlog/spdlog.h"

ID3D11PixelShader* g_GammaPixelShader = nullptr;
ID3D11Buffer* g_GammaConstantBuffer = nullptr;
float g_GammaValue = 2.2f; // Default gamma
void SetGamma(float gamma);
ID3DBlob* psBlob = nullptr;
ID3DBlob* errorBlob = nullptr;

void Init_GammaShader()
{
    HMODULE d3dcompiler = LoadLibraryA("d3dcompiler_43.dll");
    if (!d3dcompiler)
    {
        spdlog::error("CompileGeometryShader: Failed to load d3dcompiler_43.dll");
        return;
    }


    pD3DCompile D3DCompileFunc = reinterpret_cast<pD3DCompile>(GetProcAddress(d3dcompiler, "D3DCompile"));
    if (!D3DCompileFunc)
    {
        spdlog::error("CompileGeometryShader: Failed to get address for D3DCompile");
        return;
    }

    // Pixel shader source with gamma correction
    const char* psSource = R"(
        cbuffer GammaBuffer : register(b0)
        {
            float g_Gamma;
        };
        struct PS_INPUT {
            float4 Position : SV_Position;
            float4 param1 : TEXCOORD0;
            float4 param2 : TEXCOORD1;
        };
        float4 PS_Main(PS_INPUT input) : SV_Target
        {
            float4 color = float4(1,1,1,1); // Replace with your color logic
            color.rgb = pow(color.rgb, 1.0 / g_Gamma);
            return color;
        }
    )";


    HRESULT hr = D3DCompileFunc(
        psSource, strlen(psSource),
        nullptr, nullptr, nullptr,
        "PS_Main", "ps_4_0",
        0, 0, &psBlob, &errorBlob
    );
    if (FAILED(hr))
    {
        if (errorBlob)
        {
            spdlog::error("Gamma PS compile error: {}", (char*)errorBlob->GetBufferPointer());
            errorBlob->Release();
        }
        return;
    }
}

void createGammaShader()
{

    // Create pixel shader
    d3dDevice->CreatePixelShader(
        psBlob->GetBufferPointer(),
        psBlob->GetBufferSize(),
        nullptr,
        &g_GammaPixelShader
    );
    psBlob->Release();

    // Create constant buffer for gamma
    D3D11_BUFFER_DESC cbd = {};
    cbd.Usage = D3D11_USAGE_DEFAULT;
    cbd.ByteWidth = sizeof(float) * 4; // 16-byte aligned
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    d3dDevice->CreateBuffer(&cbd, nullptr, &g_GammaConstantBuffer);
}

void SetGamma(float gamma)
{
    g_GammaValue = gamma;
    float gammaData[4] = { g_GammaValue, 0, 0, 0 };
    d3dDeviceContext->UpdateSubresource(
        g_GammaConstantBuffer, 0, nullptr, gammaData, 0, 0
    );
    d3dDeviceContext->PSSetConstantBuffers(0, 1, &g_GammaConstantBuffer);
    d3dDeviceContext->PSSetShader(g_GammaPixelShader, nullptr, 0);
}
*/