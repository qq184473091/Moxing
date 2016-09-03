#pragma once
#include "d3dx9.h"
#include "define.h"

namespace RenderEngine
{

class ModelImpl;

class ASSIMP_VIEW_API Model
{

public:
	Model();
	~Model();

	ID3DXEffect* CreateDefaultEffect(IDirect3DDevice9* pd3dDevice);

	void SetWorldMatrix(const D3DXMATRIX* mat);
	void SetViewMatrix(const D3DXMATRIX* mat);
	void SetProjectMatrix(const D3DXMATRIX* mat);
	void SetViewParams(const D3DXVECTOR3 *pViewEyePos, const D3DXVECTOR3 *pViewLookAt);
	void SetWindowRect(const RECT* pRect);

	bool Load(const char* path);
	bool PostLoad(IDirect3DDevice9* g_piDevice, ID3DXEffect* g_piDefaultEffect);
	float GetModelWorldRadius();
	bool GetModelWorldMatrix(D3DXMATRIX* pOut);
	void OnResetDevice(IDirect3DDevice9* g_piDevice, ID3DXEffect* g_piDefaultEffect);
	void OnLostDevice();
	bool Unload(void);

	int RenderFullScene(IDirect3DDevice9* pd3dDevice, ID3DXEffect* g_piDefaultEffect);

private:
	ModelImpl* m_pModelImpl;
};

}
