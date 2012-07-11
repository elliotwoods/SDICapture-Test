#define _CRT_SECURE_NO_WARNINGS 1
#include <windows.h>
#include <d3d11.h>
#include <d3dx11.h>
#include <d3dcompiler.h>
#include <xnamath.h>
#include <math.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <assert.h>
#include <signal.h>

extern "C" {
#include "../../../header/dvs_setup.h"
#include "../../../header/dvs_clib.h"
#include "../../../header/dvs_direct.h"
#include "../../../header/dvs_thread.h"
}

sv_handle * sv;
sv_direct_handle * dh_out, * dh_in;
sv_storageinfo storageinfo;

// IO Threads
const int numBuffers = 1;
// Events
dvs_cond  record_ready[numBuffers];
dvs_mutex record_ready_mutex[numBuffers];
dvs_cond  display_ready[numBuffers];
dvs_mutex display_ready_mutex[numBuffers];
dvs_cond  record_go[numBuffers];
dvs_mutex record_go_mutex[numBuffers];
dvs_cond  display_go[numBuffers];
dvs_mutex display_go_mutex[numBuffers];

const int DVPINPUT  = 0;
const int DVPOUTPUT = 1;
int WIDTH  = 1920;
int HEIGHT = 1080;

// Direct3D globals
HWND                      hWnd = NULL;
HINSTANCE                 g_hInst = NULL;
D3D_DRIVER_TYPE           g_driverType = D3D_DRIVER_TYPE_NULL;
D3D_FEATURE_LEVEL         g_featureLevel = D3D_FEATURE_LEVEL_11_0;
ID3D11Device*             g_pd3dDevice = NULL;
ID3D11DeviceContext*      g_pImmediateContext = NULL;
IDXGISwapChain*           g_pSwapChain = NULL;
ID3D11RenderTargetView*   g_pRenderTargetView = NULL;
ID3D11Texture2D*          g_pBackBuffer = NULL;
ID3D11Texture2D*          g_pDepthStencil = NULL;
ID3D11DepthStencilView*   g_pDepthStencilView = NULL;
ID3D11VertexShader*       g_pVertexShader = NULL;
ID3D11PixelShader*        g_pPixelShader = NULL;
ID3D11InputLayout*        g_pVertexLayout = NULL;
ID3D11Buffer*             g_pCubeVertexBuffer = NULL;
ID3D11Buffer*             g_pCubeIndexBuffer = NULL;
ID3D11Buffer*             g_pQuadVertexBuffer = NULL;
ID3D11Buffer*             g_pQuadIndexBuffer = NULL;
ID3D11Buffer*             g_pCBViewCube = NULL;
ID3D11Buffer*             g_pCBViewQuad = NULL;
ID3D11Buffer*             g_pCBPerspectiveProjection = NULL;
ID3D11Buffer*             g_pCBOrthographicProjection = NULL;
ID3D11Buffer*             g_pCBChangesEveryFrame = NULL;
ID3D11ShaderResourceView* g_pVideoTextureRV = NULL;
ID3D11SamplerState*       g_pSamplerLinear = NULL;
XMFLOAT4                  g_vMeshColor( 0.7f, 0.7f, 0.7f, 1.0f );

ID3D11Texture2D*          g_directRenderTarget[numBuffers][2] = {0};
ID3D11RenderTargetView*   g_directRenderTargetView[numBuffers][2] = {0};

#define SAFE_RELEASE(x) if(x){ (x)->Release(); (x) = NULL; }
struct Vertex
{
  XMFLOAT3 Pos;
  XMFLOAT2 Tex;
	XMFLOAT4 Col;
};
struct CBView
{
    XMMATRIX mView;
};
struct CBProjection
{
    XMMATRIX mProjection;
};
struct CBChangesEveryFrame
{
    XMMATRIX mWorld;
};

// Timecode buffers
sv_direct_timecode timecodes[numBuffers][2];

static int verbose = 0;
static int running = TRUE;

HRESULT init(void);
typedef struct {
  sv_direct_handle * dh_in;
  sv_direct_handle * dh_out;
} renderthread_data;
DWORD WINAPI renderThread(void * data);
DWORD WINAPI recordThread(void * vdh);
DWORD WINAPI displayThread(void * vdh);
void startThread(HANDLE * t, LPTHREAD_START_ROUTINE func, void * data);
void stopThread(HANDLE t);

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow);

void Error(const char * fmt, ...)
{
  char buffer[256];
  va_list args;
  va_start(args, fmt);
  vsprintf(buffer, fmt, args);
  MessageBox(0, buffer, 0, 0);  
  va_end(args);
}

//--------------------------------------------------------------------------------------
// Helper for compiling shaders with D3DX11
//--------------------------------------------------------------------------------------
HRESULT CompileShaderFromMemory( LPCSTR szShaderTxt, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut )
{
  HRESULT hr = S_OK;

  DWORD dwShaderFlags = D3D10_SHADER_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
  // Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
  // Setting this flag improves the shader debugging experience, but still allows 
  // the shaders to be optimized and to run exactly the way they will run in 
  // the release configuration of this program.
  dwShaderFlags |= D3D10_SHADER_DEBUG;
#endif

  hr = D3DX11CompileFromMemory(szShaderTxt, strlen(szShaderTxt), NULL, NULL, NULL, szEntryPoint, szShaderModel, dwShaderFlags, 0, NULL, ppBlobOut, NULL, NULL);
  if(FAILED(hr)) {
    return hr;
  }
  return S_OK;
}

bool initBuffers()
{
  for(unsigned int buffer = 0; buffer < numBuffers; buffer++) {
    for(int i = DVPINPUT; i <= DVPOUTPUT; i++) {
      D3D11_TEXTURE2D_DESC textureDesc;
      D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;

      // Initialize the render target texture description.
      ZeroMemory(&textureDesc, sizeof(textureDesc));

      // Setup the render target texture description.
      textureDesc.Width = WIDTH;
      textureDesc.Height = HEIGHT;
      textureDesc.MipLevels = 1;
      textureDesc.ArraySize = 1;
      textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
      textureDesc.SampleDesc.Count = 1;
      textureDesc.Usage = D3D11_USAGE_DEFAULT;
      textureDesc.BindFlags =  D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
      //D3D11_BIND_RENDER_TARGET
      textureDesc.CPUAccessFlags = 0;
      textureDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

      // Create the render target texture.            
      if(!g_directRenderTarget[buffer][i]) {
        g_pd3dDevice->CreateTexture2D(&textureDesc, NULL, &g_directRenderTarget[buffer][i]);
      }

      // Setup the description of the render target view.
      renderTargetViewDesc.Format = textureDesc.Format;
      renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
      renderTargetViewDesc.Texture2D.MipSlice = 0;

      // Create the render target view.
      if(!g_directRenderTargetView[buffer][i]) {
        g_pd3dDevice->CreateRenderTargetView(g_directRenderTarget[buffer][i], &renderTargetViewDesc, &g_directRenderTargetView[buffer][i]);
      }
    }
  }
  return true;
}

void cleanupBuffers() 
{
  for(unsigned int buffer = 0; buffer < numBuffers; buffer++) {
    for(int i = DVPINPUT; i <= DVPOUTPUT; i++) {
      if(g_directRenderTarget[buffer][i]) {
        g_directRenderTarget[buffer][i]->Release();
        g_directRenderTarget[buffer][i] = 0;
      }
    }
  }
}

HRESULT init( void )
{
  HRESULT hr = S_OK;

  // Set width and height to video width and height.
  UINT width = WIDTH;
  UINT height = HEIGHT;
  UINT createDeviceFlags = 0;

  D3D_DRIVER_TYPE driverTypes[] =
    {
      D3D_DRIVER_TYPE_HARDWARE,
      D3D_DRIVER_TYPE_WARP,
      D3D_DRIVER_TYPE_REFERENCE,
    };
  UINT numDriverTypes = ARRAYSIZE( driverTypes );

  D3D_FEATURE_LEVEL featureLevels[] =
    {
      D3D_FEATURE_LEVEL_11_0,
      //D3D_FEATURE_LEVEL_10_1,
      D3D_FEATURE_LEVEL_10_0,
    };
  UINT numFeatureLevels = ARRAYSIZE( featureLevels );

  DXGI_SWAP_CHAIN_DESC sd;
  ZeroMemory( &sd, sizeof( sd ) );
  sd.BufferCount = 1;
  sd.BufferDesc.Width = width;
  sd.BufferDesc.Height = height;
  sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  sd.BufferDesc.RefreshRate.Numerator = 60;
  sd.BufferDesc.RefreshRate.Denominator = 1;
  sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.OutputWindow = hWnd;
  sd.SampleDesc.Count = 1;
  sd.SampleDesc.Quality = 0;
  sd.Windowed = TRUE;

  for(UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++) {
    g_driverType = driverTypes[driverTypeIndex];
    hr = D3D11CreateDeviceAndSwapChain( NULL, g_driverType, NULL, createDeviceFlags, featureLevels, numFeatureLevels,
    D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &g_featureLevel, &g_pImmediateContext );
    if(SUCCEEDED(hr))
      break;
  }
  if(FAILED(hr))
    return hr;

  DXGI_OUTPUT_DESC outputDesc;
  IDXGIOutput *pDXGIOutput;
  g_pSwapChain->GetContainingOutput(&pDXGIOutput);
  pDXGIOutput->GetDesc(&outputDesc);

  // Create a render target view
  hr = g_pSwapChain->GetBuffer( 0, __uuidof( ID3D11Texture2D ), ( LPVOID* )&g_pBackBuffer );
  if(FAILED(hr))
    return hr;

  hr = g_pd3dDevice->CreateRenderTargetView( g_pBackBuffer, NULL, &g_pRenderTargetView );
  if(FAILED(hr))
    return hr;

  // Create depth stencil texture
  D3D11_TEXTURE2D_DESC descDepth;
  ZeroMemory( &descDepth, sizeof(descDepth) );
  descDepth.Width = width;
  descDepth.Height = height;
  descDepth.MipLevels = 1;
  descDepth.ArraySize = 1;
  descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
  descDepth.SampleDesc.Count = 1;
  descDepth.SampleDesc.Quality = 0;
  descDepth.Usage = D3D11_USAGE_DEFAULT;
  descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
  descDepth.CPUAccessFlags = 0;
  descDepth.MiscFlags = 0;
  hr = g_pd3dDevice->CreateTexture2D( &descDepth, NULL, &g_pDepthStencil );
  if(FAILED(hr))
    return hr;

  // Set the depth stencil view
  D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
  ZeroMemory( &descDSV, sizeof(descDSV) );
  descDSV.Format = descDepth.Format;
  descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
  descDSV.Texture2D.MipSlice = 0;
  hr = g_pd3dDevice->CreateDepthStencilView( g_pDepthStencil, &descDSV, &g_pDepthStencilView );
  if(FAILED(hr))
    return hr;

  g_pImmediateContext->OMSetRenderTargets( 1, &g_pRenderTargetView, g_pDepthStencilView );

  // Set depth stencil state
  D3D11_DEPTH_STENCIL_DESC descDSS;
  ZeroMemory( &descDSS, sizeof(descDSS) );
  descDSS.DepthEnable = FALSE;  // Turn off depth testing
  ID3D11DepthStencilState *pDepthStencilState;
  hr = g_pd3dDevice->CreateDepthStencilState( &descDSS, &pDepthStencilState );
  if(FAILED(hr)) { 
    Error("CreateDepthStencilState failed hr:%08x", hr);
    return hr;
  }
  g_pImmediateContext->OMSetDepthStencilState( pDepthStencilState, 1 );
  SAFE_RELEASE(pDepthStencilState);

  // Setup the viewport
  D3D11_VIEWPORT vp;
  vp.Width = (FLOAT)width;
  vp.Height = (FLOAT)height;
  vp.MinDepth = 0.0f;
  vp.MaxDepth = 1.0f;
  vp.TopLeftX = 0;
  vp.TopLeftY = 0;
  g_pImmediateContext->RSSetViewports( 1, &vp );

  const LPCSTR txt = 
    "Texture2D txDiffuse : register( t0 );\n"
    "SamplerState samLinear : register( s0 );\n"
    "cbuffer cbView : register( b0 ) {\n"
    "  matrix View;\n"
    "};\n"
    "cbuffer cbProjection : register( b1 ){\n"
    "  matrix Projection;"
    "};\n"
    "cbuffer cbChangesEveryFrame : register( b2 ){\n"
    "  matrix World;\n"
    "};\n"
    "struct VS_INPUT {\n"
    "  float4 Pos : POSITION;\n"
    "  float2 Tex : TEXCOORD0;\n"
    "  float4 Col : COLOR;\n"
    "};\n"
    "struct VS_OUTPUT {\n"
    "  float4 Pos : SV_POSITION;\n"
    "  float2 Tex : TEXCOORD0;\n"
    "  float4 Col : COLOR0;\n"
    "};\n"
    ""
    "VS_OUTPUT VS( VS_INPUT input ) {\n"
    "  VS_OUTPUT output = (VS_OUTPUT)0;\n"
    "  output.Pos = mul( input.Pos, World );\n"
    "  output.Pos = mul( output.Pos, View );\n"
    "  output.Pos = mul( output.Pos, Projection );\n"
    "  output.Tex = input.Tex;\n"
    "  output.Col = input.Col;\n"
    "  return output;\n"
    "}\n"
    ""
    "float4 PS( VS_OUTPUT input ) : SV_Target {\n"
    "  return txDiffuse.Sample( samLinear, input.Tex);\n"
    "}\n"
    "";

  // Compile the vertex shader
  ID3DBlob* pVSBlob = NULL;
  hr = CompileShaderFromMemory( txt, "VS", "vs_4_0", &pVSBlob );
  if(FAILED(hr)) { 
    Error("The vertex shader cannot be compiled. hr:%08x", hr);
    return hr;
  }

  // Create the vertex shader
  hr = g_pd3dDevice->CreateVertexShader( pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), NULL, &g_pVertexShader );
  if(FAILED(hr)) {    
    pVSBlob->Release();
    return hr;
  }

  // Define the input layout
  D3D11_INPUT_ELEMENT_DESC layout[] =
    {
      { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
      { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
      { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
  UINT numElements = ARRAYSIZE( layout );

  // Create the input layout
  hr = g_pd3dDevice->CreateInputLayout( layout, numElements, pVSBlob->GetBufferPointer(),
  pVSBlob->GetBufferSize(), &g_pVertexLayout );
  pVSBlob->Release();
  if(FAILED(hr))
    return hr;

  // Set the input layout
  g_pImmediateContext->IASetInputLayout( g_pVertexLayout );

  // Compile the pixel shader
  ID3DBlob* pPSBlob = NULL;
  hr = CompileShaderFromMemory( txt, "PS", "ps_4_0", &pPSBlob );
  if(FAILED(hr)) { 
    Error("The pixel shader cannot be compiled. hr:%08x", hr);
    return hr;
  }

  // Create the pixel shader
  hr = g_pd3dDevice->CreatePixelShader( pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &g_pPixelShader );
  pPSBlob->Release();
  if(FAILED(hr))
    return hr;

  // Create pixel shader sampler 
  D3D11_SAMPLER_DESC samplerDesc;
  ZeroMemory( &samplerDesc, sizeof(samplerDesc) );
  samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
  samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
  samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
  samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
  samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
  samplerDesc.MinLOD = 0;
  samplerDesc.MaxLOD = 0;
  hr = g_pd3dDevice->CreateSamplerState(&samplerDesc, &g_pSamplerLinear);
  if(FAILED(hr))
    return hr;

  // Create vertex buffer for spinning cube
  Vertex cubevertices[] =
  {
    { XMFLOAT3( -1.0f, 1.0f, -1.0f ), XMFLOAT2( 1.0f, 0.0f ), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
    { XMFLOAT3( 1.0f, 1.0f, -1.0f ), XMFLOAT2( 0.0f, 0.0f ), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
    { XMFLOAT3( 1.0f, 1.0f, 1.0f ), XMFLOAT2( 0.0f, 1.0f ), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) },
    { XMFLOAT3( -1.0f, 1.0f, 1.0f ), XMFLOAT2( 1.0f, 1.0f ), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },

    { XMFLOAT3( -1.0f, -1.0f, -1.0f ), XMFLOAT2( 0.0f, 0.0f ), XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f) },
    { XMFLOAT3( 1.0f, -1.0f, -1.0f ), XMFLOAT2( 1.0f, 0.0f ), XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) },
    { XMFLOAT3( 1.0f, -1.0f, 1.0f ), XMFLOAT2( 1.0f, 1.0f ), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) },
    { XMFLOAT3( -1.0f, -1.0f, 1.0f ), XMFLOAT2( 0.0f, 1.0f ), XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f) },

    { XMFLOAT3( -1.0f, -1.0f, 1.0f ), XMFLOAT2( 1.0f, 0.0f ), XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f) },
    { XMFLOAT3( -1.0f, -1.0f, -1.0f ), XMFLOAT2( 0.0f, 0.0f ), XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f) },
    { XMFLOAT3( -1.0f, 1.0f, -1.0f ), XMFLOAT2( 0.0f, 1.0f ), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
    { XMFLOAT3( -1.0f, 1.0f, 1.0f ), XMFLOAT2( 1.0f, 1.0f ), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },

    { XMFLOAT3( 1.0f, -1.0f, 1.0f ), XMFLOAT2( 0.0f, 0.0f ), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) },
    { XMFLOAT3( 1.0f, -1.0f, -1.0f ), XMFLOAT2( 1.0f, 0.0f ), XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) },
    { XMFLOAT3( 1.0f, 1.0f, -1.0f ), XMFLOAT2( 1.0f, 1.0f ), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
    { XMFLOAT3( 1.0f, 1.0f, 1.0f ), XMFLOAT2( 0.0f, 1.0f ), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) },

    { XMFLOAT3( -1.0f, -1.0f, -1.0f ), XMFLOAT2( 1.0f, 0.0f ), XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f) },
    { XMFLOAT3( 1.0f, -1.0f, -1.0f ), XMFLOAT2( 0.0f, 0.0f ), XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) },
    { XMFLOAT3( 1.0f, 1.0f, -1.0f ), XMFLOAT2( 0.0f, 1.0f ), XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) },
    { XMFLOAT3( -1.0f, 1.0f, -1.0f ), XMFLOAT2( 1.0f, 1.0f ), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },

    { XMFLOAT3( -1.0f, -1.0f, 1.0f ), XMFLOAT2( 0.0f, 0.0f ), XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f) },
    { XMFLOAT3( 1.0f, -1.0f, 1.0f ), XMFLOAT2( 1.0f, 0.0f ), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) },
    { XMFLOAT3( 1.0f, 1.0f, 1.0f ), XMFLOAT2( 1.0f, 1.0f ), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) },
    { XMFLOAT3( -1.0f, 1.0f, 1.0f ), XMFLOAT2( 0.0f, 1.0f ), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },
  };

  D3D11_BUFFER_DESC bd;
  ZeroMemory( &bd, sizeof(bd) );
  bd.Usage = D3D11_USAGE_DEFAULT;
  bd.ByteWidth = sizeof( Vertex ) * 24;
  bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
  bd.CPUAccessFlags = 0;
  D3D11_SUBRESOURCE_DATA InitData;
  ZeroMemory( &InitData, sizeof(InitData) );
  InitData.pSysMem = cubevertices;
  hr = g_pd3dDevice->CreateBuffer( &bd, &InitData, &g_pCubeVertexBuffer );
  if(FAILED(hr))
    return hr;

  // Create index buffer
  WORD cubeindices[] =
  {
    3,1,0,
    2,1,3,

    6,4,5,
    7,4,6,

    11,9,8,
    10,9,11,

    14,12,13,
    15,12,14,

    19,17,16,
    18,17,19,

    22,20,21,
    23,20,22
  };

  bd.Usage = D3D11_USAGE_DEFAULT;
  bd.ByteWidth = sizeof( WORD ) * 36;
  bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
  bd.CPUAccessFlags = 0;
  InitData.pSysMem = cubeindices;
  hr = g_pd3dDevice->CreateBuffer( &bd, &InitData, &g_pCubeIndexBuffer );
  if(FAILED(hr))
    return hr;

  // Create vertex buffer for video background quad
  Vertex quadvertices[] =
  {
    { XMFLOAT3( -1.0f,  1.0f, -1.0f ), XMFLOAT2( 1.0f, 0.0f ), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) },
    { XMFLOAT3(  1.0f,  1.0f, -1.0f ), XMFLOAT2( 0.0f, 0.0f ), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) },
    { XMFLOAT3( -1.0f, -1.0f, -1.0f ), XMFLOAT2( 1.0f, 1.0f ), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) },
    { XMFLOAT3(  1.0f, -1.0f, -1.0f ), XMFLOAT2( 0.0f, 1.0f ), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) },
  };

  ZeroMemory( &bd, sizeof(bd) );
  bd.Usage = D3D11_USAGE_DEFAULT;
  bd.ByteWidth = sizeof( Vertex ) * 4;
  bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
  bd.CPUAccessFlags = 0;
  ZeroMemory( &InitData, sizeof(InitData) );
  InitData.pSysMem = quadvertices;
  hr = g_pd3dDevice->CreateBuffer( &bd, &InitData, &g_pQuadVertexBuffer );
  if(FAILED(hr))
    return hr;

  // Create index buffer
  WORD quadindices[] =
  {
    2,0,1,
    2,1,3,
  };

  bd.Usage = D3D11_USAGE_DEFAULT;
  bd.ByteWidth = sizeof( WORD ) * 6;
  bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
  bd.CPUAccessFlags = 0;
  InitData.pSysMem = quadindices;
  hr = g_pd3dDevice->CreateBuffer( &bd, &InitData, &g_pQuadIndexBuffer );
  if(FAILED(hr))
    return hr;

  // Create the constant buffers
  bd.Usage = D3D11_USAGE_DEFAULT;
  bd.ByteWidth = sizeof(CBView);
  bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  bd.CPUAccessFlags = 0;
  hr = g_pd3dDevice->CreateBuffer( &bd, NULL, &g_pCBViewCube );
  if(FAILED(hr))
    return hr;

  hr = g_pd3dDevice->CreateBuffer( &bd, NULL, &g_pCBViewQuad );
  if(FAILED(hr))
    return hr;

  bd.ByteWidth = sizeof(CBProjection);
  hr = g_pd3dDevice->CreateBuffer( &bd, NULL, &g_pCBPerspectiveProjection );
  if(FAILED(hr))
    return hr;

  bd.ByteWidth = sizeof(CBProjection);
  hr = g_pd3dDevice->CreateBuffer( &bd, NULL, &g_pCBOrthographicProjection );
  if(FAILED(hr))
    return hr;

  bd.ByteWidth = sizeof(CBChangesEveryFrame);
  hr = g_pd3dDevice->CreateBuffer( &bd, NULL, &g_pCBChangesEveryFrame );
  if(FAILED(hr))
    return hr;

  // Initialize the world matrices
  XMMATRIX world;
  world = XMMatrixIdentity();

  // Initialize the view matrix for the quad
  XMMATRIX view;
  view = XMMatrixIdentity();
  CBView cbViewQuad;
  cbViewQuad.mView = XMMatrixTranspose( view );
  g_pImmediateContext->UpdateSubresource( g_pCBViewQuad, 0, NULL, &cbViewQuad, 0, 0 );

  // Initialize the view matrix for the spinning cube
  XMVECTOR Eye = XMVectorSet( 0.0f, 0.0f, 5.0f, 0.0f );
  XMVECTOR At = XMVectorSet( 0.0f, 0.0f, 0.0f, 0.0f );
  XMVECTOR Up = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
  view = XMMatrixLookAtLH( Eye, At, Up );
  CBView cbViewCube;
  cbViewCube.mView = XMMatrixTranspose( view );
  g_pImmediateContext->UpdateSubresource( g_pCBViewCube, 0, NULL, &cbViewCube, 0, 0 );

  // Initialize the perspective projection matrix used when drawing the cube
  CBProjection cbPerspectiveProjection;
  cbPerspectiveProjection.mProjection = XMMatrixTranspose( XMMatrixPerspectiveFovLH( XM_PIDIV4, width / (FLOAT)height, 0.1f, 100.0f ) );
  g_pImmediateContext->UpdateSubresource( g_pCBPerspectiveProjection, 0, NULL, &cbPerspectiveProjection, 0, 0 );

  // Initialize the orthographic projection matrix used when drawing the background video
  CBProjection cbOrthographicProjection;
  cbOrthographicProjection.mProjection = XMMatrixTranspose( XMMatrixOrthographicLH(2.0f, 2.0f, -1.0f, 1.0f) );
  g_pImmediateContext->UpdateSubresource( g_pCBOrthographicProjection, 0, NULL, &cbOrthographicProjection, 0, 0 );

   // Initialize buffers for GPU Direct for Video transfers.
  initBuffers();

  static SIZE_T  dwMin = 0, dwMax = 0;
  HANDLE hProcess = OpenProcess( PROCESS_QUERY_INFORMATION | PROCESS_SET_QUOTA, FALSE, GetCurrentProcessId());

  if(!hProcess) {
    Error("OpenProcess failed:%d", GetLastError());
  }

  // Retrieve the working set size of the process.
  if(!dwMin && !GetProcessWorkingSetSize(hProcess, &dwMin, &dwMax)) {
    Error("GetProcessWorkingSetSize failed:%d", GetLastError());
  }

  // 128MB extra
  if(!SetProcessWorkingSetSize(hProcess, (128 * 0x100000) + dwMin, (128 * 0x100000) + (dwMax - dwMin))) {
    Error("SetProcessWorkingSetSize failed:%d", GetLastError());
  }

  CloseHandle(hProcess);

  return 0;
}

void closeApp() {
  cleanupBuffers();
  if( g_pImmediateContext ) g_pImmediateContext->ClearState();
  SAFE_RELEASE(g_pSamplerLinear);
  SAFE_RELEASE(g_pVideoTextureRV);
  SAFE_RELEASE(g_pCBViewCube);
  SAFE_RELEASE(g_pCBViewQuad);
  SAFE_RELEASE(g_pCBPerspectiveProjection);
  SAFE_RELEASE(g_pCBOrthographicProjection); 
  SAFE_RELEASE(g_pCBChangesEveryFrame);
  SAFE_RELEASE(g_pCubeVertexBuffer);
  SAFE_RELEASE(g_pCubeIndexBuffer);
  SAFE_RELEASE(g_pVertexLayout); 
  SAFE_RELEASE(g_pVertexShader); 
  SAFE_RELEASE(g_pPixelShader); 
  SAFE_RELEASE(g_pDepthStencil); 
  SAFE_RELEASE(g_pDepthStencilView); 
  SAFE_RELEASE(g_pRenderTargetView); 
  SAFE_RELEASE(g_pSwapChain);
  SAFE_RELEASE(g_pImmediateContext);
  SAFE_RELEASE(g_pd3dDevice); 
}

void render(ID3D11Texture2D * dxFrameTexIn, ID3D11Texture2D * dxFrameTexOut)
{
  // Update our time
  static float t = 0.0f;
  if( g_driverType == D3D_DRIVER_TYPE_REFERENCE ) {
    t += ( float )XM_PI * 0.0125f;
  } else {
    static DWORD dwTimeStart = 0;
    DWORD dwTimeCur = GetTickCount();
    if( dwTimeStart == 0 )
    dwTimeStart = dwTimeCur;
    t = ( dwTimeCur - dwTimeStart ) / 1000.0f;
  }

  // Clear the back buffer
  float ClearColor[4] = { 0.0f, 0.125f, 0.3f, 1.0f }; // red, green, blue, alpha
  g_pImmediateContext->ClearRenderTargetView( g_pRenderTargetView, ClearColor );

  // Clear the depth buffer to 1.0 (max depth)
  g_pImmediateContext->ClearDepthStencilView( g_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0 );

  // Set the vertex and pixel shaders.
  g_pImmediateContext->VSSetShader( g_pVertexShader, NULL, 0 );
  g_pImmediateContext->VSSetConstantBuffers( 0, 1, &g_pCBViewCube );
  g_pImmediateContext->VSSetConstantBuffers( 1, 1, &g_pCBPerspectiveProjection );
  g_pImmediateContext->VSSetConstantBuffers( 2, 1, &g_pCBChangesEveryFrame );
  g_pImmediateContext->PSSetShader( g_pPixelShader, NULL, 0 );
  g_pImmediateContext->PSSetConstantBuffers( 2, 1, &g_pCBChangesEveryFrame );
  g_pImmediateContext->PSSetSamplers( 0, 1, &g_pSamplerLinear );

  UINT stride;
  UINT offset;

  XMMATRIX world;
  CBChangesEveryFrame cb;

  // Render the background video

  // Create pixel shader resource view for input texture
  D3D11_SHADER_RESOURCE_VIEW_DESC rvdesc;
  ZeroMemory( &rvdesc, sizeof(rvdesc) );
  rvdesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  rvdesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
  rvdesc.Texture2D.MipLevels = 1;
  rvdesc.Texture2D.MostDetailedMip = 0;
  HRESULT hr = g_pd3dDevice->CreateShaderResourceView( dxFrameTexIn, &rvdesc, &g_pVideoTextureRV);
  if(FAILED(hr)) {
    Error("Error create texture resource view. hr:%08x", hr);
    return;
  }

  // Update world.
  world = XMMatrixIdentity();
  cb.mWorld = XMMatrixTranspose( world );
  g_pImmediateContext->UpdateSubresource( g_pCBChangesEveryFrame, 0, NULL, &cb, 0, 0 );

  // Set pixel shader resource view
  g_pImmediateContext->PSSetShaderResources( 0, 1, &g_pVideoTextureRV );

  // Set the view transform
  g_pImmediateContext->VSSetConstantBuffers( 0, 1, &g_pCBViewQuad );

  // Set the orthographic projection
  g_pImmediateContext->VSSetConstantBuffers( 1, 1, &g_pCBOrthographicProjection );

  // Set background video quad vertex buffer
  stride = sizeof( Vertex );
  offset = 0;
  g_pImmediateContext->IASetVertexBuffers( 0, 1, &g_pQuadVertexBuffer, &stride, &offset );

  // Set background video quad index buffer
  g_pImmediateContext->IASetIndexBuffer( g_pQuadIndexBuffer, DXGI_FORMAT_R16_UINT, 0 );

  // Set primitive topology
  g_pImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

  // Draw the background video quad
  g_pImmediateContext->DrawIndexed( 6, 0, 0 );


  //  
  // Render the cube

  // Rotate cube around the origin
  XMVECTOR vec = {1.0f, -1.0f, 0.0f};
  world = XMMatrixRotationAxis(vec, t);

  // Update world.
  cb.mWorld = XMMatrixTranspose( world );
  g_pImmediateContext->UpdateSubresource( g_pCBChangesEveryFrame, 0, NULL, &cb, 0, 0 );

  // Set pixel shader resource view
  g_pImmediateContext->PSSetShaderResources( 0, 1, &g_pVideoTextureRV );

  // Set the view transform
  g_pImmediateContext->VSSetConstantBuffers( 0, 1, &g_pCBViewCube );

  // Set the perspective projection
  g_pImmediateContext->VSSetConstantBuffers( 1, 1, &g_pCBPerspectiveProjection );

  // Set cube vertex buffer
  stride = sizeof( Vertex );
  offset = 0;
  g_pImmediateContext->IASetVertexBuffers( 0, 1, &g_pCubeVertexBuffer, &stride, &offset );

  // Set cube index buffer
  g_pImmediateContext->IASetIndexBuffer( g_pCubeIndexBuffer, DXGI_FORMAT_R16_UINT, 0 );

  // Set primitive topology
  g_pImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

  // Draw the spinning cube
  g_pImmediateContext->DrawIndexed( 36, 0, 0 );

  // Release texture resource view
  SAFE_RELEASE(g_pVideoTextureRV);

  // Copy backbuffer to output texture
  g_pImmediateContext->CopyResource(dxFrameTexOut, g_pBackBuffer);

  // Present our back buffer to our front buffer
  g_pSwapChain->Present( 0, 0 );
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	WNDCLASSEX winClass = { sizeof(WNDCLASSEX), NULL, WndProc, 0L, 0L, hInstance, NULL, NULL, NULL, NULL, "DVP_WINDOW_CLASS", NULL };
  winClass.hCursor = LoadCursor(NULL, IDC_ARROW);

	// Register window class.
	if( !RegisterClassEx(&winClass) )
		return E_FAIL;

	hWnd = CreateWindow( "DVP_WINDOW_CLASS", "DirectAPI Direct3D", WS_OVERLAPPEDWINDOW, 100, 100, 640, 480, GetDesktopWindow(), NULL, GetModuleHandle(NULL), NULL );
	if(hWnd == NULL)
		return E_FAIL;

  if(FAILED(init())) {
    closeApp();
    return E_FAIL;
  }
  ShowWindow(hWnd, nCmdShow);
  UpdateWindow(hWnd);

  int flags = 0;
  int res = SV_OK;
  HANDLE renderThreadHandle = NULL, recordThreadHandle = NULL, displayThreadHandle = NULL;

  res = sv_openex(&sv, "", SV_OPENPROGRAM_DEMOPROGRAM, SV_OPENTYPE_DEFAULT, 0, 0);
  if(res != SV_OK) {
    Error("sv_openex:%d (%s)", res, sv_geterrortext(res));
    return 1;
  }

  // raster information
  res = sv_storage_status(sv, 0, NULL, &storageinfo, sizeof(sv_storageinfo), SV_STORAGEINFO_COOKIEISJACK);
  if(res != SV_OK) {
    Error("sv_storage_status():%d (%s)", res, sv_geterrortext(res));
    return 1;
  }

  res = sv_jack_option_set(sv, 1, SV_OPTION_ALPHAGAIN, 0);
  if(res != SV_OK) {
    Error("sv_storage_status():%d (%s)", res, sv_geterrortext(res));
    return 1;
  }

  res = sv_jack_option_set(sv, 1, SV_OPTION_ALPHAOFFSET, 0x10000);
  if(res != SV_OK) {
    Error("sv_storage_status():%d (%s)", res, sv_geterrortext(res));
    return 1;
  }

  // output
  res = sv_direct_initex(sv, &dh_out, "NVIDIA/DIRECTX11", 0, numBuffers, flags, g_pd3dDevice);
  if(res != SV_OK) {
    Error("sv_direct_init(out):%d (%s)", res, sv_geterrortext(res));
    return 1;
  }

  // input
  res = sv_direct_initex(sv, &dh_in, "NVIDIA/DIRECTX11", 1, numBuffers, flags, g_pd3dDevice);
  if(res != SV_OK) {
    Error("sv_direct_init(in):%d (%s)", res, sv_geterrortext(res));
    return 1;
  }
    
  for(int buffer = 0; buffer < numBuffers; buffer++) {
    for (int i = DVPINPUT; i <= DVPOUTPUT; i++) {
      sv_direct_handle * dh = NULL;
      switch(i) {
      case DVPOUTPUT:
        dh = dh_out;
        break;
      case DVPINPUT:
        dh = dh_in;
        break;
      default:;
      }
      if(dh) {
        res = sv_direct_bind_d3d11resource(sv, dh, buffer, (ID3D11Resource *) g_directRenderTarget[buffer][i]);
        if(res != SV_OK) {
          Error("sv_direct_bind_opengl:%d (%s)", res, sv_geterrortext(res));
          return 1;
        }
        res = sv_direct_bind_timecode(sv, dh, buffer, &timecodes[buffer][i]);
        if(res != SV_OK) {
          Error("sv_direct_bind_timecode:%d (%s)", res, sv_geterrortext(res));
          return 1;
        }
      }
    }
  }

  // init events
  for(int buffer = 0; buffer < numBuffers; buffer++) {
    dvs_cond_init(&record_ready[buffer]);
    dvs_mutex_init(&record_ready_mutex[buffer]);
    dvs_cond_init(&display_ready[buffer]);
    dvs_mutex_init(&display_ready_mutex[buffer]);
    dvs_cond_init(&record_go[buffer]);
    dvs_mutex_init(&record_go_mutex[buffer]);
    dvs_cond_init(&display_go[buffer]);
    dvs_mutex_init(&display_go_mutex[buffer]);
  }

  renderthread_data rtdata;
  rtdata.dh_in = dh_in;
  rtdata.dh_out = dh_out;
  startThread(&renderThreadHandle, renderThread, &rtdata);
  startThread(&recordThreadHandle, recordThread, dh_in);
  startThread(&displayThreadHandle, displayThread, dh_out);

  // first record go
  dvs_cond_broadcast(&record_go[0], &record_go_mutex[0], FALSE);
    
	while(running) {
    MSG uMsg;
    while(PeekMessage(&uMsg, NULL, 0, 0, PM_NOREMOVE) == TRUE) { 
      if(GetMessage(&uMsg, NULL, 0, 0)) { 
        TranslateMessage(&uMsg); 
        DispatchMessage(&uMsg);
      } else { 
        return TRUE; 
      } 
      Sleep(250);
    }
  }

  stopThread(renderThreadHandle);
  stopThread(recordThreadHandle);
  stopThread(displayThreadHandle);

  for(int buffer = 0; buffer < numBuffers; buffer++) {
    dvs_cond_free(&record_ready[buffer]);
    dvs_mutex_free(&record_ready_mutex[buffer]);
    dvs_cond_free(&display_ready[buffer]);
    dvs_mutex_free(&display_ready_mutex[buffer]);
    dvs_cond_free(&record_go[buffer]);
    dvs_mutex_free(&record_go_mutex[buffer]);
    dvs_cond_free(&display_go[buffer]);
    dvs_mutex_free(&display_go_mutex[buffer]);
  }

  sv_direct_free(sv, dh_in);
  sv_direct_free(sv, dh_out);
  sv_close(sv);

	closeApp();
	DestroyWindow(hWnd);
  UnregisterClass( "MY_WINDOWS_CLASS", winClass.hInstance );

	return S_OK;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch(msg) {
  case WM_KEYDOWN:
    switch(wParam) {
    case VK_ESCAPE:
      running = FALSE;
      break;
    }
    break;
  case WM_SIZE:
    InvalidateRect(hWnd, NULL, FALSE);
    break;
  default:
    return DefWindowProc( hWnd, msg, wParam, lParam );
  }
  return 0;
}

DWORD WINAPI renderThread(void * data)
{
  sv_direct_handle * dh_in = ((renderthread_data *)data)->dh_in;
  sv_direct_handle * dh_out = ((renderthread_data *)data)->dh_out;
  int recordCurrent = 0;
  int displayCurrent = 0;
  int res = SV_OK;

  while(running) {
    dvs_cond_wait(&record_ready[recordCurrent % numBuffers], &record_ready_mutex[recordCurrent % numBuffers], FALSE);
    if(displayCurrent > 0) dvs_cond_wait(&display_ready[displayCurrent % numBuffers], &display_ready_mutex[displayCurrent % numBuffers], FALSE);

    res = sv_direct_sync(sv, dh_in, recordCurrent % numBuffers, 0);
    if(res != SV_OK) {
      Error("sv_direct_sync(in):%d (%s)", res, sv_geterrortext(res));
    } else {
      render(g_directRenderTarget[recordCurrent % numBuffers][DVPINPUT], g_directRenderTarget[displayCurrent % numBuffers][DVPOUTPUT]);

      // Simply copy timecodes from input to output.
      memcpy(&timecodes[displayCurrent % numBuffers][DVPOUTPUT], &timecodes[recordCurrent % numBuffers][DVPINPUT], sizeof(sv_direct_timecode));

      // next input buffer
      recordCurrent++;
      dvs_cond_broadcast(&record_go[recordCurrent % numBuffers], &record_go_mutex[recordCurrent % numBuffers], FALSE);

      res = sv_direct_sync(sv, dh_out, displayCurrent % numBuffers, 0);
      if(res != SV_OK) {
        Error("sv_direct_sync(out):%d (%s)", res, sv_geterrortext(res));
      } else {
        dvs_cond_broadcast(&display_go[displayCurrent % numBuffers], &display_go_mutex[displayCurrent % numBuffers], FALSE);
        // next output buffer
        displayCurrent++;    
      }
    }
  }

  return 0;
}

DWORD WINAPI recordThread(void * vdh) 
{
  sv_direct_handle * dh = (sv_direct_handle *) vdh;
  sv_direct_bufferinfo bufferinfo = { sizeof(sv_direct_bufferinfo) };
  sv_direct_info info;
  int dropped = 0;
  int res = SV_OK;
  int recordCurrent = 0;

  do {
    if(res == SV_OK) {
      dvs_cond_wait(&record_go[recordCurrent % numBuffers], &record_go_mutex[recordCurrent % numBuffers], FALSE);

      res = sv_direct_record(sv, dh, recordCurrent % numBuffers, SV_DIRECT_FLAG_DISCARD, &bufferinfo);
      if(res != SV_OK) {
        switch(res) {
        case SV_ERROR_INPUT_VIDEO_NOSIGNAL:
        case SV_ERROR_INPUT_VIDEO_RASTER:
        case SV_ERROR_INPUT_VIDEO_DETECTING:
          // non-fatal errors
          res = SV_OK;
          break;
        default:
          Error("sv_direct_record:%d (%s)", res, sv_geterrortext(res));
        }
      }

      if(verbose) {
        unsigned int vsync_duration = (1000000 / storageinfo.fps / (storageinfo.videomode & SV_MODE_STORAGE_FRAME ? 1 : 2));
        unsigned __int64 record_go = ((unsigned __int64)bufferinfo.clock_high << 32) | bufferinfo.clock_low;
        unsigned __int64 dma_ready = ((unsigned __int64)bufferinfo.dma.clock_ready_high << 32) | bufferinfo.dma.clock_ready_low;
        printf("%d> record:\tvsync->dmadone: %5d dmastart->vsync:%5d\n", recordCurrent % numBuffers, (int)(dma_ready - record_go - vsync_duration), bufferinfo.dma.clock_distance);
      }
    }

    if(res == SV_OK) {
      dvs_cond_broadcast(&record_ready[recordCurrent % numBuffers], &record_ready_mutex[recordCurrent % numBuffers], FALSE);
      // next record buffer
      recordCurrent++;
    }

    if(res == SV_OK) {
      info.size = sizeof(sv_direct_info);
      res = sv_direct_status(sv, dh, &info);
      if(res != SV_OK) {
        Error("sv_direct_status:%d (%s)", res, sv_geterrortext(res));
      } else {
        if(info.dropped > dropped) {
          printf("recordThread:  dropped:%d\n", info.dropped);
          dropped = info.dropped;
        }
      }
    }
  } while(running && (res == SV_OK));

  return 0;
}

DWORD WINAPI displayThread(void * vdh) 
{
  sv_direct_handle * dh = (sv_direct_handle *) vdh;
  sv_direct_bufferinfo bufferinfo = { sizeof(sv_direct_bufferinfo) };
  sv_direct_info info;
  int dropped = 0;
  int res = SV_OK;
  int displayCurrent = 0;

  do {
    if(res == SV_OK) {
      dvs_cond_wait(&display_go[displayCurrent % numBuffers], &display_go_mutex[displayCurrent % numBuffers], FALSE);
      
      res = sv_direct_display(sv, dh, displayCurrent % numBuffers, SV_DIRECT_FLAG_DISCARD, &bufferinfo);
      if(res != SV_OK) {
        switch(res) {
        case SV_ERROR_VSYNCPASSED:
          // might happen in case of delay by GPU
          res = SV_OK;
          break;
        default:
          Error("sv_direct_display:%d (%s)\n", res, sv_geterrortext(res));
        }
      }

      if(verbose) {
        unsigned int vsync_duration = (1000000 / storageinfo.fps / (storageinfo.videomode & SV_MODE_STORAGE_FRAME ? 1 : 2));
        printf("%d> display:\tvsync->dmastart:%5d dmastart->vsync:%5d\n", displayCurrent % numBuffers, (int)(vsync_duration - bufferinfo.dma.clock_distance), bufferinfo.dma.clock_distance);
      }
    }

    if(res == SV_OK) {
      // next display buffer
      displayCurrent++;
      dvs_cond_broadcast(&display_ready[displayCurrent % numBuffers], &display_ready_mutex[displayCurrent % numBuffers], FALSE);
    }

    if(res == SV_OK) {
      info.size = sizeof(sv_direct_info);
      res = sv_direct_status(sv, dh, &info);
      if(res != SV_OK) {
        Error("sv_direct_status:%d (%s)", res, sv_geterrortext(res));
      } else {
        if(info.dropped > dropped) {
          printf("displayThread: dropped:%d\n", info.dropped);
          dropped = info.dropped;
        }
      }
    }
  } while(running && (res == SV_OK));

  return 0;
}

void startThread(HANDLE * t, LPTHREAD_START_ROUTINE func, void * data)
{
  if(t) {
    *t = CreateThread(0, 0, func, data, 0, NULL);
  }
}

void stopThread(HANDLE t)
{
  if(t) {
    running = FALSE;
    WaitForSingleObject(t, 1000);
    CloseHandle(t);
  }
}
