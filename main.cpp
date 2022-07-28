#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <cassert>
#include <vector>
#include <string>
#include <DirectXMath.h>
#include <DirectXTex.h>
#include <stdlib.h>
#include <time.h>

#define DIRECTINPUT_VERSION 0x0800 // DirectInputのバージョン指定
#include <dinput.h>
using namespace DirectX;
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#include <d3dcompiler.h>
#pragma comment(lib,"d3dcompiler.lib")

struct ConstBufferDataTransform {
	XMMATRIX mat;//3D変換行列
};

//3Dオブジェクト型
struct Object3d {
	//定数バッファ（行列用）
	ID3D12Resource* constBuffTransform;
	//定数バッファマップ（行列用）
	ConstBufferDataTransform* constMapTransform;
	//アフィン変換情報
	XMFLOAT3 scale = { 1,1,1 };
	XMFLOAT3 rotation = { 0,0,0 };
	XMFLOAT3 position = { 0,0,0 };
	//ワールド変換行列
	XMMATRIX matWorld;
	//親オブジェクトへのポインタ
	Object3d* parent = nullptr;
};

//3Dオブジェクト初期化
void InitializeObject3d(Object3d* object, ID3D12Device* device)
{
	HRESULT result;
	//定数バッファのヒープ設定
	D3D12_HEAP_PROPERTIES heapProp{};
	heapProp.Type = D3D12_HEAP_TYPE_UPLOAD;
	//定数バッファのリソース設定
	D3D12_RESOURCE_DESC resdesc{};
	resdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resdesc.Width = (sizeof(ConstBufferDataTransform) + 0xff) & ~0xff;
	resdesc.Height = 1;
	resdesc.DepthOrArraySize = 1;
	resdesc.MipLevels = 1;
	resdesc.SampleDesc.Count = 1;
	resdesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	//つづく
	//定数バッファの生成
	result = device->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &resdesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&object->constBuffTransform));
	assert(SUCCEEDED(result));
	//つづく
	//定数バッファのマッピング
	result = object->constBuffTransform->Map(0, nullptr, (void**)&object->constMapTransform);
	assert(SUCCEEDED(result));
}

void UpdateObject3d(Object3d* object, XMMATRIX& matView, XMMATRIX& matProjection) {
	XMMATRIX matScale, matRot, matTrans;
	//スケール、回転、平行移動行列の計算
	matScale = XMMatrixScaling(object->scale.x, object->scale.y, object->scale.z);
	matRot = XMMatrixIdentity(); matRot *= XMMatrixRotationZ(object->rotation.z);
	matRot *= XMMatrixRotationX(object->rotation.x);
	matRot *= XMMatrixRotationY(object->rotation.y);
	matTrans = XMMatrixTranslation(object->position.x, object->position.y, object->position.z);
	//つづく
	//ワールド行列の合成
	object->matWorld = XMMatrixIdentity();
	//変形をリセット
	object->matWorld *= matScale;
	//ワールド行列にスケーリングを反映
	object->matWorld *= matRot;
	//ワールド行列に回転を反映
	object->matWorld *= matTrans;
	//ワールド行列に平行移動を反映//親オブジェクトがあれば
	if (object->parent != nullptr)
	{
		//親オブジェクトのワールド行列を掛ける
		object->matWorld *= object->parent->matWorld;
	}
	//つづく
	//定数バッファへデータ転送
	object->constMapTransform->mat = object->matWorld * matView * matProjection;
}

void DrawObject3d(Object3d* object, ID3D12GraphicsCommandList* commandlist, D3D12_VERTEX_BUFFER_VIEW& vbView, D3D12_INDEX_BUFFER_VIEW& ibView, UINT numIndices)
{
	//頂点バッファの設定
	commandlist->IASetVertexBuffers(0, 1, &vbView);
	//インデックスバッファの設定
	commandlist->IASetIndexBuffer(&ibView);
	//定数バッファビュー(CBV)の設定コマンド
	commandlist->SetGraphicsRootConstantBufferView(2, object->constBuffTransform->GetGPUVirtualAddress());
	//描画コマンド
	commandlist->DrawIndexedInstanced(numIndices, 1, 0, 0, 0);
}

LRESULT WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	switch (msg) {
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {

#pragma region WINAPI
	OutputDebugStringA("Hello,DirectX!!\n");

	const int window_width = 1280;
	const int window_height = 720;

	WNDCLASSEX w{};
	w.cbSize = sizeof(WNDCLASSEX);
	w.lpfnWndProc = (WNDPROC)WindowProc;
	w.lpszClassName = L"DirectXGame";
	w.hInstance = GetModuleHandle(nullptr);
	w.hCursor = LoadCursor(NULL, IDC_ARROW);

	RegisterClassEx(&w);

	RECT wrc = { 0,0,window_width, window_height };

	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);


	HWND hwnd = CreateWindow(w.lpszClassName, //クラス名
		L"DirectXGame",       //タイトルバーの文字
		WS_OVERLAPPEDWINDOW,  //標準的なウィンドウスタイル
		CW_USEDEFAULT,        //表示X座標（OSに任せる）
		CW_USEDEFAULT,        //表示Y座標（OSに任せる）
		wrc.right - wrc.left, //ウィンドウ横幅
		wrc.bottom - wrc.top, //ウィンドウ縦幅
		nullptr,              //親ウィンドウハンドル
		nullptr,              //メニューハンドル
		w.hInstance,          //呼び出しアプリケーションハンドル
		nullptr);             //オプション

	//ウィンドウを表示状態にする
	ShowWindow(hwnd, SW_SHOW);

	MSG msg{};//メッセージ
#pragma endregion

#ifdef _DEBUG
	//デバックレイヤーをオンに
	ID3D12Debug* debugController;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
		debugController->EnableDebugLayer();
	}
#endif // DEBUG

#pragma region DirectX初期化
	//DirectX初期化処理　ここから
	HRESULT result;
	ID3D12Device* device = nullptr;
	IDXGIFactory7* dxgiFactory = nullptr;
	IDXGISwapChain4* swapChain = nullptr;
	ID3D12CommandAllocator* cmdAllocator = nullptr;
	ID3D12GraphicsCommandList* commandlist = nullptr;
	ID3D12CommandQueue* commandQueue = nullptr;
	ID3D12DescriptorHeap* rtvHeap = nullptr;
	// DirectInputの初期化
	IDirectInput8* directInput = nullptr;
	result = DirectInput8Create(
		w.hInstance, DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&directInput, nullptr);
	assert(SUCCEEDED(result));

	// キーボードデバイスの生成
	IDirectInputDevice8* keyboard = nullptr;
	result = directInput->CreateDevice(GUID_SysKeyboard, &keyboard, NULL);
	assert(SUCCEEDED(result));

	// 入力データ形式のセット
	result = keyboard->SetDataFormat(&c_dfDIKeyboard); // 標準形式
	assert(SUCCEEDED(result));

	// 排他制御レベルのセット
	result = keyboard->SetCooperativeLevel(
		hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
	assert(SUCCEEDED(result));

	//DirectX初期化処理　ここまで

	//DXGIファクトリーの生成
	result = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory));
	assert(SUCCEEDED(result));

	//アダプターの列挙用
	std::vector < IDXGIAdapter4*>adapters;
	//ここに特定の名前を持つアダプターオブジェクトが入る
	IDXGIAdapter4* tmpAdapter = nullptr;

	//パフォーマンスが高いものから順に、すべてのアダプターを列挙する
	for (UINT i = 0;
		dxgiFactory->EnumAdapterByGpuPreference(i,
			DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
			IID_PPV_ARGS(&tmpAdapter)) != DXGI_ERROR_NOT_FOUND;
		i++) {
		//動的配列に追加する
		adapters.push_back(tmpAdapter);
	}

	//妥当なアダプタを選別する
	for (size_t i = 0; i < adapters.size(); i++) {
		DXGI_ADAPTER_DESC3 adapterDesc;
		//アダプターの情報を収録する
		adapters[i]->GetDesc3(&adapterDesc);

		//ソフトウェアデバイスを回避
		if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
			//デバイスを採用してループを抜ける
			tmpAdapter = adapters[i];
			break;
		}
	}

	//対応レベルの配列
	D3D_FEATURE_LEVEL levels[] = {
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};

	D3D_FEATURE_LEVEL featureLevel;

	for (size_t i = 0; i < _countof(levels); i++) {
		result = D3D12CreateDevice(tmpAdapter, levels[i],
			IID_PPV_ARGS(&device));
		if (result == S_OK) {
			//デバイスを生成できた時点でループを抜ける
			featureLevel = levels[i];
			break;
		}
	}

	//コマンドアロケータの生成
	result = device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(&cmdAllocator));
	assert(SUCCEEDED(result));

	//コマンドリストを生成
	result = device->CreateCommandList(0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		cmdAllocator, nullptr,
		IID_PPV_ARGS(&commandlist));
	assert(SUCCEEDED(result));

	//コマンドキューの設定
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc{}; //構造体の中身を0でクリアしている
	//コマンドキューを生成
	result = device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&commandQueue));
	assert(SUCCEEDED(result));

	//スワップチェーンの設定
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
	swapChainDesc.Width = 1200;
	swapChainDesc.Height = 720;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
	swapChainDesc.BufferCount = 2;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	//スワップチェーンの生成
	result = dxgiFactory->CreateSwapChainForHwnd(
		commandQueue, hwnd, &swapChainDesc, nullptr, nullptr,
		(IDXGISwapChain1**)&swapChain);
	assert(SUCCEEDED(result));

	//デスクリプタヒープの設定
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; //レンダーターゲットビュー
	rtvHeapDesc.NumDescriptors = swapChainDesc.BufferCount;

	//デスクリプタヒープの生成
	device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap));

	//バックバッファ
	std::vector<ID3D12Resource*> backBuffers;
	backBuffers.resize(swapChainDesc.BufferCount);

	//スワップチェーンの全てのバッファについて処理する
	for (size_t i = 0; i < backBuffers.size(); i++) {
		//スワップチェーンからバッファを取得
		swapChain->GetBuffer((UINT)i, IID_PPV_ARGS(&backBuffers[i]));
		//デスクリプタヒ－プのハンドルを取得
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
		//裏か表かでアドレスがずれる
		rtvHandle.ptr += i * device->GetDescriptorHandleIncrementSize(rtvHeapDesc.Type);
		//レンダーターゲットビューの設定
		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
		//シェーダーの計算結果をSRGBに変換して書き込む
		rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		//レンダーターゲットビューの生成
		device->CreateRenderTargetView(backBuffers[i], &rtvDesc, rtvHandle);
	}


	// リソース設定
	D3D12_RESOURCE_DESC depthResourceDesc{};
	depthResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthResourceDesc.Width = window_width;  //レンダーターゲットに合わせる
	depthResourceDesc.Height = window_height;//レンダーターゲットに合わせる
	depthResourceDesc.DepthOrArraySize = 1;
	depthResourceDesc.Format = DXGI_FORMAT_D32_FLOAT;  //深度値フォーマット
	depthResourceDesc.SampleDesc.Count = 1;
	depthResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;  //デブスステンシル

	//深度値用ヒーププロパティ
	D3D12_HEAP_PROPERTIES depthHeapProp{};
	depthHeapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
	//深度値のクリア設定
	D3D12_CLEAR_VALUE depthClearValue{};
	depthClearValue.DepthStencil.Depth = 1.0f; //深度値1.0f(最大値)でクリア
	depthClearValue.Format = DXGI_FORMAT_D32_FLOAT; //深度値フォーマット

	//リソース生成
	ID3D12Resource* depthBuff = nullptr;
	result = device->CreateCommittedResource(
		&depthHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&depthResourceDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,  //深度値書き込みに使用
		&depthClearValue,
		IID_PPV_ARGS(&depthBuff)
	);
	//深度ビュー用デスクリプタヒープ作成
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = 1; //深度ビューは1つ
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;//デブスステンシルビュー
	ID3D12DescriptorHeap* dsvHeap = nullptr;
	result = device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap));
	//深度ビュー作成
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;  //深度値フォーマット
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	device->CreateDepthStencilView(
		depthBuff,
		&dsvDesc,
		dsvHeap->GetCPUDescriptorHandleForHeapStart()
	);



	//フェンスの生成
	ID3D12Fence* fence = nullptr;
	UINT64 fenceVal = 0;

	result = device->CreateFence(fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));

#pragma endregion

#pragma region 描画初期化処理
	// 頂点データ構造体
	struct Vertex
	{
		XMFLOAT3 pos; // xyz座標
		XMFLOAT3 normal; // 法線ベクトル
		XMFLOAT2 uv;  // uv座標
	};
	//頂点データ
	//Vertex vertices[] =
	//{
	//	//  x	 y	    z	   u	v
	//	{{0.0f,100.0f,0.0f},{0.0f,1.0f}},	//左下
	//	{{0.0f,0.0f,0.0f},{0.0f,0.0f}},		//左上
	//	{{100.0f,100.0f,0.0f},{1.0f,1.0f}},	//右下
	//	{{100.0f,0.0f,0.0f},{1.0f,0.0f}},	//右上
	//};


	Vertex vertices[] = {
		//   x	   y    z			u    v
		//前
		{{-5.0f,-5.0f,-5.0f},{}, {0.0f,1.0f}},	//左下
		{{-5.0f,5.0f,-5.0f},{}, {0.0f,0.0f}},	//左上
		{{5.0f,-5.0f,-5.0f}, {},{1.0f,1.0f}},	//右下
		{{5.0f,5.0f,-5.0f}, {}, {1.0f,0.0f}},	//右上
		//後
		{{-5.0f,-5.0f,5.0f},{}, {0.0f,1.0f}},	//左下
		{{-5.0f,5.0f,5.0f}, {}, {0.0f,0.0f}},	//左上
		{{5.0f,-5.0f,5.0f}, {}, {1.0f,1.0f}},	//右下
		{{5.0f,5.0f,5.0f},  {}, {1.0f,0.0f}},	//右上
		//左
		{{5.0f,-5.0f,-5.0f},{},{0.0f,1.0f}},	//左下
		{{5.0f,5.0f,-5.0f}, {}, {0.0f,0.0f}},	//左上
		{{5.0f,-5.0f,5.0f}, {},{1.0f,1.0f}},	//右下
		{{5.0f,5.0f,5.0f},  {},{1.0f,0.0f}},	//右上
		//右
		{{-5.0f,-5.0f,5.0f}, {}, {0.0f,1.0f}},	//左下
		{{-5.0f,5.0f,5.0f},  {}, {0.0f,0.0f}},	//左上
		{{-5.0f,-5.0f,-5.0f},{}, {1.0f,1.0f}},	//右下
		{{-5.0f,5.0f,-5.0f}, {}, {1.0f,0.0f}},	//右上
		//下
		{{-5.0f,5.0f,5.0f}, {}, {0.0f,1.0f}},	//左下
		{{5.0f,5.0f,5.0f},  {}, {0.0f,0.0f}},	//左上
		{{-5.0f,5.0f,-5.0f},{}, {1.0f,1.0f}},//右下
		{{5.0f,5.0f,-5.0f}, {}, {1.0f,0.0f}},	//右上
		//上
		{{-5.0f,-5.0f,5.0f}, {}, {0.0f,1.0f}},	//左下
		{{5.0f,-5.0f,5.0f},  {}, {0.0f,0.0f}},	//左上
		{{-5.0f,-5.0f,-5.0f},{}, {1.0f,1.0f}},	//右下
		{{5.0f,-5.0f,-5.0f}, {}, {1.0f,0.0f}},	//右上
	};

	//// 頂点データ
	//XMFLOAT3 vertices[] = {

	//	{-0.5f,  0.5f, 1.0f}, // 左上
	//	{ 0.5f,  0.5f, 0.0f}, // 右上
	//	{ 0.5f, -0.5f, 0.0f}, // 右下

	//	{ 0.5f, -0.5f, 0.0f}, // 右下
	//	{-0.5f, -0.5f, 1.0f}, // 左下
	//	{-0.5f,  0.5f, 1.0f}, // 左上
	//};
	//// 頂点データ全体のサイズ = 頂点データ一つ分のサイズ * 頂点データの要素数
	//UINT sizeVB = static_cast<UINT>(sizeof(XMFLOAT3) * _countof(vertices));
	// 頂点データ全体のサイズ = 頂点データ一つ分のサイズ * 頂点データの要素数
	UINT sizeVB = static_cast<UINT>(sizeof(vertices[0]) * _countof(vertices));

	TexMetadata metadata{};
	ScratchImage scratchImg{};
	// WICテクスチャのロード
	result = LoadFromWICFile(
		L"Resources/kirby.jpg",   //「Resources」フォルダの「texture.png」
		WIC_FLAGS_NONE,
		&metadata, scratchImg);


	ScratchImage mipChain{};
	// ミップマップ生成
	result = GenerateMipMaps(
		scratchImg.GetImages(), scratchImg.GetImageCount(), scratchImg.GetMetadata(),
		TEX_FILTER_DEFAULT, 0, mipChain);
	if (SUCCEEDED(result)) {
		scratchImg = std::move(mipChain);
		metadata = scratchImg.GetMetadata();
	}

	// 読み込んだディフューズテクスチャをSRGBとして扱う
	metadata.format = MakeSRGB(metadata.format);

	TexMetadata metadata2{};
	ScratchImage scratchImg2{};
	// WICテクスチャのロード
	result = LoadFromWICFile(
		L"Resources/reimu.png",
		WIC_FLAGS_NONE,
		&metadata2, scratchImg2);

	ScratchImage mipChain2{};
	//ミニマップ生成
	result = GenerateMipMaps(
		scratchImg2.GetImages(), scratchImg2.GetImageCount(), scratchImg2.GetMetadata(),
		TEX_FILTER_DEFAULT, 0, mipChain2);
	if (SUCCEEDED(result))
	{
		scratchImg2 = std::move(mipChain2);
		metadata2 = scratchImg2.GetMetadata();
	}

	//読み込んだディフューズテクスチャをSRGBとして扱う
	metadata2.format = MakeSRGB(metadata2.format);


	// 定数バッファ用データ構造体（マテリアル）
	struct ConstBufferDataMaterial {
		XMFLOAT4 color; // 色 (RGBA)
	};

	//定数バッファ用データ構造体（３Ｄ変換行列）
	struct ConstBufferDataTransform {
		XMMATRIX mat;//3D変換行列
	};
	ID3D12Resource* constBuffTransform0 = nullptr;
	ConstBufferDataTransform* constMapTransform0 = nullptr;

	ID3D12Resource* constBuffTransform1 = nullptr;
	ConstBufferDataTransform* constMapTransform1 = nullptr;

	// インデックスデータ
	unsigned short indices[] = {
		//前
		0, 1, 2, // 三角形1つ目
		2, 1, 3, // 三角形2つ目
		//後
		4, 5, 6, // 三角形3つ目
		6, 5, 7, // 三角形4つ目
		//左
		8, 9, 10, // 三角形5つ目
		10, 9, 11, // 三角形6つ目
		//右
		12, 13, 14, // 三角形7つ目
		14, 13, 15, // 三角形8つ目
		//下
		16, 17, 18, // 三角形9つ目
		18, 17, 19, // 三角形10つ目
		//上
		20, 21, 22, // 三角形11つ目
		22, 21, 23, // 三角形12つ目
	};
	//// インデックスデータ
	//uint16_t indices[] =
	//{
	//	0, 1, 2, // 三角形1つ目
	//	2, 3, 3, // 三角形2つ目
	//};

	// 頂点バッファの設定
	D3D12_HEAP_PROPERTIES heapProp{}; // ヒープ設定
	heapProp.Type = D3D12_HEAP_TYPE_UPLOAD; // GPUへの転送用
	// リソース設定
	D3D12_RESOURCE_DESC resDesc{};
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resDesc.Width = sizeVB; // 頂点データ全体のサイズ
	resDesc.Height = 1;
	resDesc.DepthOrArraySize = 1;
	resDesc.MipLevels = 1;
	resDesc.SampleDesc.Count = 1;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	// ヒープ設定
	D3D12_HEAP_PROPERTIES cbHeapProp{};
	cbHeapProp.Type = D3D12_HEAP_TYPE_UPLOAD;                   // GPUへの転送用
	// リソース設定
	D3D12_RESOURCE_DESC cbResourceDesc{};
	cbResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	cbResourceDesc.Width = (sizeof(ConstBufferDataMaterial) + 0xff) & ~0xff;   // 256バイトアラインメント
	cbResourceDesc.Height = 1;
	cbResourceDesc.DepthOrArraySize = 1;
	cbResourceDesc.MipLevels = 1;
	cbResourceDesc.SampleDesc.Count = 1;
	cbResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	// インデックスデータ全体のサイズ
	UINT sizeIB = static_cast<UINT>(sizeof(uint16_t) * _countof(indices));

	// リソース設定
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resDesc.Width = sizeIB; // インデックス情報が入る分のサイズ
	resDesc.Height = 1;
	resDesc.DepthOrArraySize = 1;
	resDesc.MipLevels = 1;
	resDesc.SampleDesc.Count = 1;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;




	// 定数バッファ(行列)
	{
		//ヒープ設定
		D3D12_HEAP_PROPERTIES cbHeapProp{};
		cbHeapProp.Type = D3D12_HEAP_TYPE_UPLOAD;//GPUへの転送用
		//リソース設定
		D3D12_RESOURCE_DESC cbResourceDesc{};
		cbResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		cbResourceDesc.Width = (sizeof(ConstBufferDataTransform) + 0xff) & ~0xff;//256バイトアラインメント
		cbResourceDesc.Height = 1;
		cbResourceDesc.DepthOrArraySize = 1;
		cbResourceDesc.MipLevels = 1;
		cbResourceDesc.SampleDesc.Count = 1;
		cbResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;//続く



		result = device->CreateCommittedResource(
			&cbHeapProp,//ヒープ設定
			D3D12_HEAP_FLAG_NONE, &cbResourceDesc,
			//リソース設定
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
			IID_PPV_ARGS(&constBuffTransform1));
		//定数バッファのマッピング
		result = constBuffTransform1->Map(0, nullptr, (void**)&constMapTransform1);
		//マッピング
		assert(SUCCEEDED(result));

		//単位行列を代入
		//constMapTransform->mat = XMMatrixIdentity();
		/*constMapTransform->mat.r[0].m128_f32[0] = 2.0f / 1280;
		constMapTransform->mat.r[1].m128_f32[1] = -2.0f / 720;
		constMapTransform->mat.r[3].m128_f32[0] = -1.0f;
		constMapTransform->mat.r[3].m128_f32[1] = 1.0f;*/

		//並行投影行列の計算
	//	constMapTransform0->mat = XMMatrixOrthographicOffCenterLH(
	//		1.0f,	//ViewLeft
	//		0.0f,	//ViewRight
	//		0.1f,	//ViewBottom
	//		1.0f,	//ViewTop
	//		0.1f,	//NearZ
	//		1.0f);	//Farz
	}

	//射影変換行列(透視投影)
	XMMATRIX matProjection = XMMatrixPerspectiveFovLH(
		XMConvertToRadians(45.0f),
		(float)window_width / window_height,
		0.1f, 1000.0f);

	//ビュー変換行列
	XMMATRIX matView;
	XMFLOAT3 eye(0, 0, -100);//視点座標
	XMFLOAT3 target(0, 0, 0);//注視点座標
	XMFLOAT3 up(0, 1, 0);//上方向ベクトル
	matView = XMMatrixLookAtLH(XMLoadFloat3(&eye), XMLoadFloat3(&target), XMLoadFloat3(&up));

	float angle = 0.0f;//カメラの回転角

	////ワールド変換行列
	//XMMATRIX matWorld;
	////matWorldに単位行列を代入
	//matWorld = XMMatrixIdentity();

	////ワールド行列に単位行列を代入する処理
	//XMMATRIX matScale;//スケーリング行列
	//matScale = XMMatrixScaling(1.0f, 0.5f, 1.0f);
	//matWorld *= matScale;//ワールド行列にスケーリングを反映

	//XMMATRIX matRot;//回転行列
	//matRot = XMMatrixIdentity();
	//matRot *= XMMatrixRotationZ(XMConvertToRadians(0.0f));//Z軸まわりに0度回転してから
	//matRot *= XMMatrixRotationX(XMConvertToRadians(15.0f));//X軸まわりに15度回転してから
	//matRot *= XMMatrixRotationY(XMConvertToRadians(30.0f));//Y軸まわりに30度回転
	//matWorld *= matRot;//ワールド行列に回転を反映

	//XMMATRIX matTrans;
	//matTrans = XMMatrixTranslation(-50.0f, 0, 0);
	//matWorld *= matTrans;

	//座標
	XMFLOAT3 position = { 0.0f,0.0f,0.0f };

	////定数バッファに転送
	//constMapTransform0->mat = matWorld * matView * matProjection;

	////ワールド変換行列
	//XMMATRIX matWorld1;
	//matWorld1 = XMMatrixIdentity();
	////各種変形行列を計算
	//XMMATRIX matScale1 = XMMatrixScaling(1.0f, 1.0f, 1.0f);
	//XMMATRIX matRot1 = XMMatrixRotationY(XM_PI / 4.0f);
	//XMMATRIX matTrans1 = XMMatrixTranslation(-20.0f, 0, 0);
	////ワールド行列を合成
	//matWorld1 = matScale1 * matRot1 * matTrans1;
	////ワールド、ビュー、射影行列を合成してシェーダーに転送
	//constMapTransform1->mat = matWorld1 * matView * matProjection;

	////透視投影行列の計算
	//constMapTransform->mat = XMMatrixPerspectiveFovLH(
	//	XMConvertToRadians(45.0f),//上下画角45度
	//	(float)window_width /window_height,//アスペクト比（画面横幅/画面縦幅）
	//	0.1f,1000.0f//前端、奥端
	//);
	//constMapTransform->mat = matProjection;

	//3Dオブジェクトの数
	const size_t KObjectCount = 50;
	//3Dオブジェクトの配列
	Object3d object3ds[KObjectCount];

	srand(time(NULL));
	float dice1 = rand() % 10 + 1.0f;
	//配列内の全オブジェクトに対して
	for (int i = 0; i < _countof(object3ds); i++)
	{
		//初期化
		InitializeObject3d(&object3ds[i], device);
		//ここから↓は親子構造のサンプル
		//先頭以外なら
		if (i > 0)
		{
			srand(time(NULL));
			float dice1 = rand() % 6 + 1.0f;
			//一つ前のオブジェクトを親オブジェクトとする
			object3ds[i].parent = &object3ds[i - 1];
			//親オブジェクトの9割の大きさ
			object3ds[i].scale = { 0.9f,0.9f,0.9f };
			//親オブジェクトに対してZ軸まわりに30度回転
			object3ds[i].rotation = { 0.0f,0.0f,XMConvertToRadians(30.0f) };
			//親オブジェクトに対してZ方向-8.0ずらす
			object3ds[i].position = { dice1,dice1,dice1 };
		}
	}

	for (size_t i = 0; i < _countof(object3ds); i++)
	{
		UpdateObject3d(&object3ds[i], matView, matProjection);
	}



	// インデックスバッファの生成
	ID3D12Resource* indexBuff = nullptr;
	result = device->CreateCommittedResource(
		&heapProp, // ヒープ設定
		D3D12_HEAP_FLAG_NONE,
		&resDesc, // リソース設定
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&indexBuff));

	// インデックスバッファをマッピング
	uint16_t* indexMap = nullptr;
	result = indexBuff->Map(0, nullptr, (void**)&indexMap);
	// 全インデックスに対して
	for (int i = 0; i < _countof(indices); i++)
	{
		indexMap[i] = indices[i];   // インデックスをコピー
	}
	// マッピング解除
	indexBuff->Unmap(0, nullptr);

	// インデックスバッファビューの作成
	D3D12_INDEX_BUFFER_VIEW ibView{};
	ibView.BufferLocation = indexBuff->GetGPUVirtualAddress();
	ibView.Format = DXGI_FORMAT_R16_UINT;
	ibView.SizeInBytes = sizeIB;

	// 頂点バッファの生成
	ID3D12Resource* vertBuff = nullptr;
	result = device->CreateCommittedResource(
		&heapProp, // ヒープ設定
		D3D12_HEAP_FLAG_NONE,
		&resDesc, // リソース設定
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&vertBuff));
	assert(SUCCEEDED(result));

	ID3D12Resource* constBuffMaterial = nullptr;
	// 定数バッファの生成
	result = device->CreateCommittedResource(
		&cbHeapProp, // ヒープ設定
		D3D12_HEAP_FLAG_NONE,
		&cbResourceDesc, // リソース設定
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&constBuffMaterial));
	assert(SUCCEEDED(result));

	// 定数バッファのマッピング
	ConstBufferDataMaterial* constMapMaterial = nullptr;
	result = constBuffMaterial->Map(0, nullptr, (void**)&constMapMaterial); // マッピング
	assert(SUCCEEDED(result));

	// 横方向ピクセル数
	const size_t textureWidth = 256;
	// 縦方向ピクセル数
	const size_t textureHeight = 256;
	// 配列の要素数
	const size_t imageDataCount = textureWidth * textureHeight;
	// 画像イメージデータ配列
	XMFLOAT4* imageData = new XMFLOAT4[imageDataCount]; // ※必ず後で解放する

	// 全ピクセルの色を初期化
	for (size_t i = 0; i < imageDataCount; i++) {
		imageData[i].x = 1.0f;    // R
		imageData[i].y = 0.0f;    // G
		imageData[i].z = 0.0f;    // B
		imageData[i].w = 1.0f;    // A
	}

	// ヒープ設定
	D3D12_HEAP_PROPERTIES textureHeapProp{};
	textureHeapProp.Type = D3D12_HEAP_TYPE_CUSTOM;
	textureHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
	textureHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
	// リソース設定
	//D3D12_RESOURCE_DESC textureResourceDesc{};
	//textureResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; textureResourceDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT; textureResourceDesc.Width = textureWidth;  // 幅
	//textureResourceDesc.Height = textureWidth; // 高さ
	//textureResourceDesc.DepthOrArraySize = 1;
	//textureResourceDesc.MipLevels = 1;
	//textureResourceDesc.SampleDesc.Count = 1;

	D3D12_RESOURCE_DESC textureResourceDesc{};
	textureResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	textureResourceDesc.Format = metadata.format;
	textureResourceDesc.Width = metadata.width;
	textureResourceDesc.Height = (UINT)metadata.height;
	textureResourceDesc.DepthOrArraySize = (UINT16)metadata.arraySize;
	textureResourceDesc.MipLevels = (UINT16)metadata.mipLevels;
	textureResourceDesc.SampleDesc.Count = 1;

	D3D12_RESOURCE_DESC textureResourceDesc2{};
	textureResourceDesc2.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	textureResourceDesc2.Format = metadata.format;
	textureResourceDesc2.Width = metadata.width;
	textureResourceDesc2.Height = (UINT)metadata.height;
	textureResourceDesc2.DepthOrArraySize = (UINT16)metadata.arraySize;
	textureResourceDesc2.MipLevels = (UINT16)metadata.mipLevels;
	textureResourceDesc2.SampleDesc.Count = 1;

	// テクスチャバッファの生成
	ID3D12Resource* texBuff = nullptr;
	result = device->CreateCommittedResource(
		&textureHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&textureResourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&texBuff));

	//テクスチャバッファの生成
	ID3D12Resource* texBuff2 = nullptr;
	result = device->CreateCommittedResource(
		&textureHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&textureResourceDesc2,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&texBuff2));

	// テクスチャバッファにデータ転送
	//result = texBuff->WriteToSubresource(
	//	0,
	//	nullptr, // 全領域へコピー
	//	imageData,    // 元データアドレス
	//	sizeof(XMFLOAT4) * textureWidth, // 1ラインサイズ
	//	sizeof(XMFLOAT4) * imageDataCount // 全サイズ
	//);
	// 全ミップマップについて
	for (size_t i = 0; i < metadata.mipLevels; i++) {
		// ミップマップレベルを指定してイメージを取得
		const Image* img = scratchImg.GetImage(i, 0, 0);
		// テクスチャバッファにデータ転送
		result = texBuff->WriteToSubresource(
			(UINT)i,
			nullptr,              // 全領域へコピー
			img->pixels,          // 元データアドレス
			(UINT)img->rowPitch,  // 1ラインサイズ
			(UINT)img->slicePitch // 1枚サイズ
		);
		assert(SUCCEEDED(result));
	}

	//二枚目の全ミニマップについて
	for (size_t i = 0; i < metadata2.mipLevels; i++)
	{
		//ミニマップレベルを指定してイメージを取得
		const Image* img2 = scratchImg2.GetImage(i, 0, 0);
		//テクスチャバッファにデータ転送
		result = texBuff2->WriteToSubresource(
			(UINT)i,
			nullptr,
			img2->pixels,
			(UINT)img2->rowPitch,
			(UINT)img2->slicePitch
		);
		assert(SUCCEEDED(result));
	}

	// 元データ解放
	delete[] imageData;

	// SRVの最大個数
	const size_t kMaxSRVCount = 2056;

	// デスクリプタヒープの設定
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;//シェーダから見えるように
	srvHeapDesc.NumDescriptors = kMaxSRVCount;

	// 設定を元にSRV用デスクリプタヒープを生成
	ID3D12DescriptorHeap* srvHeap = nullptr;
	result = device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&srvHeap));
	assert(SUCCEEDED(result));

	//SRVヒープの先頭ハンドルを取得
	D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = srvHeap->GetCPUDescriptorHandleForHeapStart();
	//デスクリプタのサイズを取得
	UINT incrementSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	//取得したサイズを使用してハンドルを進める
	srvHandle.ptr += incrementSize;

	// シェーダリソースビュー設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = resDesc.Format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = resDesc.MipLevels;

	// ハンドルの指す位置にシェーダーリソースビュー作成
	device->CreateShaderResourceView(texBuff, &srvDesc, srvHandle);


	//シェーダーリソースビューの設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc2{}; //設定構造体
	srvDesc2.Format = textureResourceDesc2.Format;//RGBA float
	srvDesc2.Shader4ComponentMapping =
		D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc2.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;//2Dテクスチャ
	srvDesc2.Texture2D.MipLevels = textureResourceDesc2.MipLevels;

	//ハンドルの指す位置にシェーダーリソースビュー作成
	device->CreateShaderResourceView(texBuff2, &srvDesc2, srvHandle);


	// CBV,SRV,UAVの1個分のサイズを取得
	UINT descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// ハンドルを1つ進める（SRVの位置）
	srvHandle.ptr += descriptorSize * 1;

	// CBV(コンスタントバッファビュー)の設定
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
	//cbvDescの値設定（省略）
	// 定数バッファビュー生成
	device->CreateConstantBufferView(&cbvDesc, srvHandle);

	// デスクリプタレンジの設定
	D3D12_DESCRIPTOR_RANGE descriptorRange{};
	descriptorRange.NumDescriptors = 1;         //一度の描画に使うテクスチャが1枚なので1
	descriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange.BaseShaderRegister = 0;     //テクスチャレジスタ0番
	descriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// ルートパラメータの設定
	//D3D12_ROOT_PARAMETER rootParam = {};
	//rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;    // 定数バッファビュー
	//rootParam.Descriptor.ShaderRegister = 0;                    // 定数バッファ番号
	//rootParam.Descriptor.RegisterSpace = 0;                     // デフォルト値
	//rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;   //全てのシェーダから見える
	D3D12_ROOT_PARAMETER rootParams[3] = {};
	// 定数バッファ0番
	rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;   // 種類
	rootParams[0].Descriptor.ShaderRegister = 0;                   // 定数バッファ番号
	rootParams[0].Descriptor.RegisterSpace = 0;                    // デフォルト値
	rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;  // 全てのシェーダから見える
	// テクスチャレジスタ0番
	rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;   //種類
	rootParams[1].DescriptorTable.pDescriptorRanges = &descriptorRange;		  //デスクリプタレンジ
	rootParams[1].DescriptorTable.NumDescriptorRanges = 1;              		  //デスクリプタレンジ数
	rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;               //全てのシェーダから見える
	//定数バッファ1番
	rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;//種類
	rootParams[2].Descriptor.ShaderRegister = 1;//定数バッファ番号
	rootParams[2].Descriptor.RegisterSpace = 0;//デフォルト値
	rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;//全てのシェーダから見える


	for (int i = 0; i < _countof(indices) / 3; i++) { // 三角形を一つごとに計算していく
		// 三角形のインデックスを取り出して、一時的な変数を入れる
		unsigned short index0 = indices[i * 3 + 0];
		unsigned short index1 = indices[i * 3 + 1];
		unsigned short index2 = indices[i * 3 + 2];
		// 三角形を構成する頂点座標をベクトルに代入
		XMVECTOR p0 = XMLoadFloat3(&vertices[index0].pos);
		XMVECTOR p1 = XMLoadFloat3(&vertices[index1].pos);
		XMVECTOR p2 = XMLoadFloat3(&vertices[index2].pos);
		// p0→p1ベクトル、p0→p2ベクトルを計算（ベクトルの減算）
		XMVECTOR v1 = XMVectorSubtract(p1, p0);
		XMVECTOR v2 = XMVectorSubtract(p2, p0);
		// 外積は両方から垂直なベクトル
		XMVECTOR normal = XMVector3Cross(v1, v2);
		// 正規化（長さを1にする）
		normal = XMVector3Normalize(normal);
		// 求めた法線を頂点データに代入
		XMStoreFloat3(&vertices[index0].normal, normal);
		XMStoreFloat3(&vertices[index1].normal, normal);
		XMStoreFloat3(&vertices[index2].normal, normal);
	}

	// GPU上のバッファに対応した仮想メモリ(メインメモリ上)を取得
	/*XMFLOAT3* vertMap = nullptr;
	result = vertBuff->Map(0, nullptr, (void**)&vertMap);*/
	Vertex* vertMap = nullptr;
	result = vertBuff->Map(0, nullptr, (void**)&vertMap);
	assert(SUCCEEDED(result));
	// 全頂点に対して
	for (int i = 0; i < _countof(vertices); i++) {
		vertMap[i] = vertices[i]; // 座標をコピー
	}
	// 繋がりを解除
	vertBuff->Unmap(0, nullptr);

	// 頂点バッファビューの作成
	D3D12_VERTEX_BUFFER_VIEW vbView{};
	// GPU仮想アドレス
	vbView.BufferLocation = vertBuff->GetGPUVirtualAddress();
	// 頂点バッファのサイズ
	vbView.SizeInBytes = sizeVB;

	// 頂点1つ分のデータサイズ
	//vbView.StrideInBytes = sizeof(XMFLOAT3);
	// 頂点データ1つ分のサイズ
	vbView.StrideInBytes = sizeof(vertices[0]);


	ID3DBlob* vsBlob = nullptr; // 頂点シェーダオブジェクト
	ID3DBlob* psBlob = nullptr; // ピクセルシェーダオブジェクト
	ID3DBlob* errorBlob = nullptr; // エラーオブジェクト
	// 頂点シェーダの読み込みとコンパイル
	result = D3DCompileFromFile(
		L"BasicVS.hlsl", // シェーダファイル名
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE, // インクルード可能にする
		"main", "vs_5_0", // エントリーポイント名、シェーダーモデル指定
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, // デバッグ用設定
		0,
		&vsBlob, &errorBlob);

	// エラーなら
	if (FAILED(result)) {
		// errorBlobからエラー内容をstring型にコピー
		std::string error;
		error.resize(errorBlob->GetBufferSize());
		std::copy_n((char*)errorBlob->GetBufferPointer(),
			errorBlob->GetBufferSize(),
			error.begin());
		error += "\n";
		// エラー内容を出力ウィンドウに表示
		OutputDebugStringA(error.c_str());
		assert(0);
	}


	// ピクセルシェーダの読み込みとコンパイル
	result = D3DCompileFromFile(
		L"BasicPS.hlsl", // シェーダファイル名
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE, // インクルード可能にする
		"main", "ps_5_0", // エントリーポイント名、シェーダーモデル指定
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, // デバッグ用設定
		0,
		&psBlob, &errorBlob);

	// エラーなら
	if (FAILED(result)) {
		// errorBlobからエラー内容をstring型にコピー
		std::string error;
		error.resize(errorBlob->GetBufferSize());
		std::copy_n((char*)errorBlob->GetBufferPointer(),
			errorBlob->GetBufferSize(),
			error.begin());
		error += "\n";
		// エラー内容を出力ウィンドウに表示
		OutputDebugStringA(error.c_str());
		assert(0);
	};

	// 頂点レイアウト
	D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
	{
		"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
	}, // (1行で書いたほうが見やすい)

	{//法線ベクトル(1行で書いたほうが見やすい)
		"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
	},

	{
		// uv座標(1行で書いたほうが見やすい)
		"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
	},
	};

	// グラフィックスパイプライン設定
	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc{};
	//デブスステンシルステートの設定
	pipelineDesc.DepthStencilState.DepthEnable = true;  //深度テストを行う
	pipelineDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;  //書き込み許可
	pipelineDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;  //小さければ合格
	pipelineDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT; //深度値フォーマット

	// シェーダーの設定
	pipelineDesc.VS.pShaderBytecode = vsBlob->GetBufferPointer();
	pipelineDesc.VS.BytecodeLength = vsBlob->GetBufferSize();
	pipelineDesc.PS.pShaderBytecode = psBlob->GetBufferPointer();
	pipelineDesc.PS.BytecodeLength = psBlob->GetBufferSize();

	// サンプルマスクの設定
	pipelineDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK; // 標準設定

	// ラスタライザの設定
	pipelineDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;//背面をカリング
	pipelineDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID; // ポリゴン内塗りつぶし
	pipelineDesc.RasterizerState.DepthClipEnable = true; // 深度クリッピングを有効に

	// ブレンドステート
	D3D12_RENDER_TARGET_BLEND_DESC& blenddesc = pipelineDesc.BlendState.RenderTarget[0];
	blenddesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL; // RBGA全てのチャンネルを描画

	blenddesc.BlendEnable = true;                   // ブレンドを有効にする
	blenddesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;    // 加算f
	blenddesc.SrcBlendAlpha = D3D12_BLEND_ONE;      // ソースの値を100% 使う
	blenddesc.DestBlendAlpha = D3D12_BLEND_ZERO;    // デストの値を  0% 使う


	// 半透明合成
	blenddesc.BlendOp = D3D12_BLEND_OP_ADD;             // 加算
	blenddesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;         // ソースのアルファ値
	blenddesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;    // 1.0f-ソースのアルファ値

	// 頂点レイアウトの設定
	pipelineDesc.InputLayout.pInputElementDescs = inputLayout;
	pipelineDesc.InputLayout.NumElements = _countof(inputLayout);

	// 図形の形状設定
	pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	// その他の設定
	pipelineDesc.NumRenderTargets = 1; // 描画対象は1つ
	pipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // 0~255指定のRGBA
	pipelineDesc.SampleDesc.Count = 1; // 1ピクセルにつき1回サンプリング

	// ルートシグネチャ
	ID3D12RootSignature* rootSignature;
	// ルートシグネチャの設定
	//D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	//rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	//rootSignatureDesc.pParameters = &rootParam; //ルートパラメータの先頭アドレス
	//rootSignatureDesc.NumParameters = 1;        //ルートパラメータ数

	// テクスチャサンプラーの設定
	D3D12_STATIC_SAMPLER_DESC samplerDesc{};
	samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;                 //横繰り返し（タイリング）
	samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;                 //縦繰り返し（タイリング）
	samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;                 //奥行繰り返し（タイリング）
	samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;  //ボーダーの時は黒
	samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;                   //全てリニア補間
	samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;                                 //ミップマップ最大値
	samplerDesc.MinLOD = 0.0f;                                              //ミップマップ最小値
	samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;           //ピクセルシェーダからのみ使用可能

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	rootSignatureDesc.pParameters = rootParams; //ルートパラメータの先頭アドレス
	rootSignatureDesc.NumParameters = _countof(rootParams);        //ルートパラメータ数
	rootSignatureDesc.pStaticSamplers = &samplerDesc;
	rootSignatureDesc.NumStaticSamplers = 1;

	// ルートシグネチャのシリアライズ
	ID3DBlob* rootSigBlob = nullptr;
	result = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0,
		&rootSigBlob, &errorBlob);
	assert(SUCCEEDED(result));
	result = device->CreateRootSignature(0, rootSigBlob->GetBufferPointer(), rootSigBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootSignature));
	assert(SUCCEEDED(result));
	rootSigBlob->Release();
	// パイプラインにルートシグネチャをセット
	pipelineDesc.pRootSignature = rootSignature;

	// パイプランステートの生成
	ID3D12PipelineState* pipelineState = nullptr;
	result = device->CreateGraphicsPipelineState(&pipelineDesc, IID_PPV_ARGS(&pipelineState));
	assert(SUCCEEDED(result));


#pragma endregion

#pragma region ゲームループ
	//ゲームループ
	while (true) {
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg); //キー入力メッセージの処理
			DispatchMessage(&msg);  //プロシージャにメッセージを送る
		}

		//Xボタンで終了メッセージが来たらゲームループを抜ける
		if (msg.message == WM_QUIT) {
			break;
		}

		//DirectX毎フレーム処理　ここから
				// キーボード情報の取得開始
		keyboard->Acquire();

		// 全キーの入力状態を取得する
		BYTE key[256] = {};
		keyboard->GetDeviceState(sizeof(key), key);

		// 数字の0キーが押されていたら
		if (key[DIK_0])
		{
			OutputDebugStringA("Hit 0\n");  // 出力ウィンドウに「Hit 0」と表示
		}
		// バックバッファの番号を取得(2つなので0番か1番)
		UINT bbIndex = swapChain->GetCurrentBackBufferIndex();
		// 1.リソースバリアで書き込み可能に変更
		D3D12_RESOURCE_BARRIER barrierDesc{};
		barrierDesc.Transition.pResource = backBuffers[bbIndex]; // バックバッファを指定
		barrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT; // 表示状態から
		barrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET; // 描画状態へ
		commandlist->ResourceBarrier(1, &barrierDesc);

		// 2.描画先の変更
	// レンダーターゲットビューのハンドルを取得
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
		rtvHandle.ptr += bbIndex * device->GetDescriptorHandleIncrementSize(rtvHeapDesc.Type);
		//commandlist->OMSetRenderTargets(1, &rtvHandle, false, nullptr);
		//深度ステンシルビュー用デスクリプタヒープのハンドルを取得
		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvHeap->GetCPUDescriptorHandleForHeapStart();
		commandlist->OMSetRenderTargets(1, &rtvHandle, false, &dsvHandle);

		//座標操作
		if (key[DIK_UP] || key[DIK_DOWN] || key[DIK_RIGHT] || key[DIK_LEFT])
		{
			if (key[DIK_UP]) { object3ds[0].position.y += 1.0f; }

			else if (key[DIK_DOWN]) { object3ds[0].position.y -= 1.0f; }

			if (key[DIK_RIGHT]) { object3ds[0].position.x += 1.0f; }

			else if (key[DIK_LEFT]) { object3ds[0].position.x -= 1.0f; }
		}


		for (size_t i = 0; i < _countof(object3ds); i++)
		{
			UpdateObject3d(&object3ds[i], matView, matProjection);
		}
		//if (key[DIK_D] || key[DIK_A])
		//{
		//	if (key[DIK_D]) { angle += XMConvertToRadians(1.0f); }
		//	else if (key[DIK_A]) { angle -= XMConvertToRadians(1.0f); }

		//	//angleラジアンだけY軸まわりに回転。半径は-100
		//	eye.x = -100 * sinf(angle); eye.z = -100 * cosf(angle);
		//	//ビュー変換行列を作り直す
		//	matView = XMMatrixLookAtLH(XMLoadFloat3(&eye), XMLoadFloat3(&target), XMLoadFloat3(&up));
		//}

		//いずれかのキーを押していたら
		//if (key[DIK_UP] || key[DIK_DOWN] || key[DIK_RIGHT] || key[DIK_LEFT]) {//座標を移動する処理(Z座標)
		//	if (key[DIK_UP]) { position.z += 1.0f; }
		//	else if (key[DIK_DOWN]) { position.z -= 1.0f; }
		//	if (key[DIK_RIGHT]) { position.x += 1.0f; }
		//	else if (key[DIK_LEFT]) { position.x -= 1.0f; }

		//}
		//if (key[DIK_D] || key[DIK_A])
		//{
		//	if (key[DIK_D]) { angle += XMConvertToRadians(1.0f); }
		//	else if (key[DIK_A]) { angle -= XMConvertToRadians(1.0f); }

		//	//angleラジアンだけY軸まわりに回転。半径は-100
		//	eye.x = -100 * sinf(angle); eye.z = -100 * cosf(angle);
		//	//ビュー変換行列を作り直す
		//	matView = XMMatrixLookAtLH(XMLoadFloat3(&eye), XMLoadFloat3(&target), XMLoadFloat3(&up));
		//}
		////ワールド変換行列
		//XMMATRIX matWorld;
		////matWorldに単位行列を代入
		//matWorld = XMMatrixIdentity();

		////ワールド行列に単位行列を代入する処理
		//XMMATRIX matScale;//スケーリング行列
		//matScale = XMMatrixScaling(1.0f, 1.0f, 1.0f);
		//matWorld *= matScale;//ワールド行列にスケーリングを反映

		//XMMATRIX matRot;//回転行列
		//matRot = XMMatrixIdentity();
		//matRot *= XMMatrixRotationZ(XMConvertToRadians(0.0f));//Z軸まわりに0度回転してから
		//matRot *= XMMatrixRotationX(XMConvertToRadians(15.0f));//X軸まわりに15度回転してから
		//matRot *= XMMatrixRotationY(XMConvertToRadians(0.0f));//Y軸まわりに30度回転
		//matWorld *= matRot;//ワールド行列に回転を反映

		//XMMATRIX matTrans;//平行移動行列
		//matTrans = XMMatrixTranslation(position.x, position.y, position.z);
		//matWorld *= matTrans;
		////定数バッファに転送
		//constMapTransform0->mat = matWorld * matView * matProjection;

		//-------------------------------------------------------------------------------

		//ワールド変換行列
		XMMATRIX matWorld1;
		matWorld1 = XMMatrixIdentity();
		//各種変形行列を計算
		XMMATRIX matScale1 = XMMatrixScaling(1.0f, 1.0f, 1.0f);
		XMMATRIX matRot1 = XMMatrixRotationY(XM_PI / 4.0f);
		XMMATRIX matTrans1 = XMMatrixTranslation(-20.0f, 0, 0);
		//ワールド行列を合成
		matWorld1 = matScale1 * matRot1 * matTrans1;
		//ワールド、ビュー、射影行列を合成してシェーダーに転送
		constMapTransform1->mat = matWorld1 * matView * matProjection;

		//--------------------------------------------------------------------------------
		// 3.画面クリア R G B A

	//全オブジェクトについて処理

		FLOAT clearColor[] = { 0.1f,0.25f, 0.5f,0.0f }; // 青っぽい色
		if (key[DIK_SPACE])     // スペースキーが押されていたら
		{
			clearColor[0] = { 0.0f };
			clearColor[1] = { 0.5f };
			clearColor[2] = { 0.5f };
			clearColor[3] = { 0.0f };/*画面クリアカラーの数値を書き換える*/
		}
		constMapMaterial->color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

		commandlist->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
		commandlist->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		// 4.描画コマンドここから
		// 
		// ビューポート設定コマンド
		D3D12_VIEWPORT viewport{};
		viewport.Width = window_width;
		viewport.Height = window_height;
		viewport.TopLeftX = 0;
		viewport.TopLeftY = 0;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;
		// ビューポート設定コマンドを、コマンドリストに積む
		commandlist->RSSetViewports(1, &viewport);

		// シザー矩形
		D3D12_RECT scissorRect{};
		scissorRect.left = 0; // 切り抜き座標左
		scissorRect.right = scissorRect.left + window_width; // 切り抜き座標右
		scissorRect.top = 0; // 切り抜き座標上
		scissorRect.bottom = scissorRect.top + window_height; // 切り抜き座標下
		// シザー矩形設定コマンドを、コマンドリストに積む
		commandlist->RSSetScissorRects(1, &scissorRect);

		// パイプラインステートとルートシグネチャの設定コマンド
		commandlist->SetPipelineState(pipelineState);
		commandlist->SetGraphicsRootSignature(rootSignature);

		// プリミティブ形状の設定コマンド
		commandlist->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // 三角形リスト

		// 頂点バッファビューの設定コマンド
		commandlist->IASetVertexBuffers(0, 1, &vbView);

		// 定数バッファビュー(CBV)の設定コマンド
		//commandlist->SetGraphicsRootConstantBufferView(0, constBuffMaterial->GetGPUVirtualAddress());
		// 定数バッファビュー(CBV)の設定コマンド
		commandlist->SetGraphicsRootConstantBufferView(0, constBuffMaterial->GetGPUVirtualAddress());
		// SRVヒープの設定コマンド
		commandlist->SetDescriptorHeaps(1, &srvHeap);
		// SRVヒープの先頭ハンドルを取得（SRVを指しているはず）
		D3D12_GPU_DESCRIPTOR_HANDLE srvGpuHandle = srvHeap->GetGPUDescriptorHandleForHeapStart();
		srvGpuHandle.ptr += incrementSize;
		// SRVヒープの先頭にあるSRVをルートパラメータ1番に設定
		commandlist->SetGraphicsRootDescriptorTable(1, srvGpuHandle);
		for (int i = 0; i < _countof(object3ds); i++)
		{
			DrawObject3d(&object3ds[i], commandlist, vbView, ibView, _countof(indices));
		}


		// インデックスバッファビューの設定コマンド
		commandlist->IASetIndexBuffer(&ibView);

		// 描画コマンド
		/*commandlist->SetGraphicsRootConstantBufferView(2, constBuffTransform0->GetGPUVirtualAddress());
		commandlist->DrawIndexedInstanced(_countof(indices), 1, 0, 0, 0);
		commandlist->SetGraphicsRootConstantBufferView(2, constBuffTransform1->GetGPUVirtualAddress());
		commandlist->DrawIndexedInstanced(_countof(indices), 1, 0, 0, 0);*/
		// 4.描画コマンドここまで

		// 5.リソースバリアを戻す
		barrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET; // 描画状態から
		barrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT; // 表示状態へ
		commandlist->ResourceBarrier(1, &barrierDesc);

		// 命令のクローズ
		result = commandlist->Close();
		assert(SUCCEEDED(result));
		// コマンドリストの実行
		ID3D12CommandList* commandLists[] = { commandlist };
		commandQueue->ExecuteCommandLists(1, commandLists);
		// 画面に表示するバッファをフリップ(裏表の入替え)
		result = swapChain->Present(1, 0);
		assert(SUCCEEDED(result));

		// コマンドの実行完了を待つ
		commandQueue->Signal(fence, ++fenceVal);
		if (fence->GetCompletedValue() != fenceVal) {
			HANDLE event = CreateEvent(nullptr, false, false, nullptr);
			fence->SetEventOnCompletion(fenceVal, event);
			WaitForSingleObject(event, INFINITE);
			CloseHandle(event);
		}
		// キューをクリア
		result = cmdAllocator->Reset();
		assert(SUCCEEDED(result));
		// 再びコマンドリストを貯める準備
		result = commandlist->Reset(cmdAllocator, nullptr);
		assert(SUCCEEDED(result));

		//// キーボード情報の取得開始
		//keyboard->Acquire();
		//
		//// 全キーの入力状態を取得する
		//BYTE key[256] = {};
		//keyboard->GetDeviceState(sizeof(key), key);

		//// 数字の0キーが押されていたら
		//if (key[DIK_0])
		//{
		//	OutputDebugStringA("Hit 0\n");  // 出力ウィンドウに「Hit 0」と表示
		//}


	}

#pragma endregion
	return 0;
}