#include "Model.h"
#include "ModelImpl.h"

namespace RenderEngine
{


Model::Model()
	: m_pModelImpl(new ModelImpl)
{
}

Model::~Model()
{
	delete m_pModelImpl;
}

ID3DXEffect * Model::CreateDefaultEffect(IDirect3DDevice9 * pd3dDevice)
{
	return m_pModelImpl->CreateDefaultEffect(pd3dDevice);
}

void Model::SetWorldMatrix(const D3DXMATRIX* mat)
{
	m_pModelImpl->SetWorldMatrix(mat);
}

void Model::SetViewMatrix(const D3DXMATRIX* mat)
{
	m_pModelImpl->SetViewMatrix(mat);
}

void Model::SetViewParams(const D3DXVECTOR3 *pViewEyePos, const D3DXVECTOR3 *pViewLookAt)
{
	m_pModelImpl->SetViewParams(pViewEyePos, pViewLookAt);
}

void Model::SetProjectMatrix(const D3DXMATRIX* mat)
{
	m_pModelImpl->SetProjectMatrix(mat);
}

void Model::SetWindowRect(const RECT* pRect)
{
	m_pModelImpl->SetWindowRect(pRect);
}


bool Model::Load(const char * path)
{
	return m_pModelImpl->Load(path);
}

bool Model::PostLoad(IDirect3DDevice9* g_piDevice, ID3DXEffect* g_piDefaultEffect)
{
	return m_pModelImpl->PostLoad(g_piDevice, g_piDefaultEffect);
}

float Model::GetRadius()
{
	return m_pModelImpl->GetRadius();
}

bool Model::Unload(void)
{
	return m_pModelImpl->Unload();
}


//-------------------------------------------------------------------------------
// Render the full scene, all nodes
int Model::RenderFullScene(IDirect3DDevice9* g_piDevice, ID3DXEffect* g_piDefaultEffect)
{
	return m_pModelImpl->RenderFullScene(g_piDevice, g_piDefaultEffect);
}


}
