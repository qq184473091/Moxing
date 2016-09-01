#pragma once

/* assimp include files. These three are usually needed. */
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "assimp_view\AssetHelper.h"
#include "assimp_view\Camera.h"
#include "assimp_view\RenderOptions.h"
#include "assimp_view\MaterialManager.h"
#include "assimp_view\MeshRenderer.h"

namespace RenderEngine
{

class ModelImpl
{
	AssimpView::AssetHelper* g_pcAsset;
	Camera g_sCamera;
	RenderOptions g_sOptions;

	AssimpView::CMaterialManager* m_pMaterialMgr;
	AssimpView::CMeshRenderer* m_pMeshRender;

	aiVector3D m_aabb[2];
	float m_radius;

	aiMatrix4x4 g_mWorld;
	//solution 1
	aiMatrix4x4 g_mWorldRotate;
	aiMatrix4x4 mView;
	aiMatrix4x4 mProjection;
	//TODO: can be removed
	RECT m_WindowRect;

	//solution 2
	// View projection matrix
	aiMatrix4x4 mViewProjection;
	aiVector3D vPos;

public:
	ModelImpl();
	~ModelImpl();

	bool Load(const char* path);
	bool PostLoad(IDirect3DDevice9* g_piDevice, ID3DXEffect* g_piDefaultEffect);
	//void GetAABB();
	float GetModelWorldRadius() { return m_radius; }
	bool GetModelWorldMatrix(D3DXMATRIX* pOut);
	bool Unload(void);

	ID3DXEffect* CreateDefaultEffect(IDirect3DDevice9* pd3dDevice);

	int RenderFullScene(IDirect3DDevice9* pd3dDevice, ID3DXEffect* g_piDefaultEffect);

	void ResetView();
	void SetWorldMatrix(const D3DXMATRIX* mat);
	void SetViewMatrix(const D3DXMATRIX* mat);
	void SetViewParams(const D3DXVECTOR3 *pViewEyePos, const D3DXVECTOR3 *pViewLookAt);
	void SetProjectMatrix(const D3DXMATRIX* mat);
	void SetWindowRect(const RECT* pRect);

private:
	int CreateAssetData(IDirect3DDevice9* g_piDevice, ID3DXEffect* g_piDefaultEffect);
	int DeleteAssetData(bool bNoMaterials = true);
	int GenerateNormalsAsLineList(IDirect3DDevice9* g_piDevice, AssimpView::AssetHelper::MeshHelper* pcMesh, const aiMesh* pcSource);

	//------------------------------------------------------------------
	// Render a given node in the scenegraph
	// piNode Node to be rendered
	// piMatrix Current transformation matrix
	// bAlpha Render alpha or opaque objects only?
	int RenderNode(IDirect3DDevice9* piDevice, ID3DXEffect* g_piDefaultEffect,
		aiNode* piNode, const aiMatrix4x4& piMatrix, bool bAlpha = false);

	int GetProjectionMatrix(aiMatrix4x4& p_mOut);
	aiVector3D GetCameraMatrix(aiMatrix4x4& p_mOut);
};

}
