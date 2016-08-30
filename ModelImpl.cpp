#define AI_SHADER_COMPILE_FLAGS D3DXSHADER_USE_LEGACY_D3DX9_31_DLL
/* assimp include files. These three are usually needed. */
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
// Include ASSIMP headers (XXX: do we really need all of them?)
#include <assimp/cimport.h>
#include <assimp/Importer.hpp>
#include <assimp/ai_assert.h>
#include <assimp/cfileio.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/IOSystem.hpp>
#include <assimp/IOStream.hpp>
#include <assimp/LogStream.hpp>
#include <assimp/DefaultLogger.hpp>

#include "ModelImpl.h"

#include "assimp_view\assimp_view.h"
#include "assimp_view\SceneAnimator.h"
#include "assimp_view\MaterialManager.h"
#include "assimp_view\MeshRenderer.h"
#include "assimp_view\Shaders.h"

#undef min
#undef max
#include <algorithm>


extern D3DCAPS9 g_sCaps;
IDirect3DVertexDeclaration9* gDefaultVertexDecl = NULL;


namespace AssimpView
{

// NOTE: The second light direction is now computed from the first
aiVector3D g_avLightDirs[1] = 
{	aiVector3D(-0.5f,0.6f,0.2f)  };

D3DCOLOR g_avLightColors[3] = 
{
    D3DCOLOR_ARGB(0xFF,0xFF,0xFF,0xFF),
    D3DCOLOR_ARGB(0xFF,0xFF,0x00,0x00),
    D3DCOLOR_ARGB(0xFF,0x05,0x05,0x05),
};

float g_fLightIntensity = 1.0f;
float g_fLightColor = 1.0f;










//-------------------------------------------------------------------------------
// Calculate the boundaries of a given node and all of its children
// The boundaries are in Worldspace (AABB)
// piNode Input node
// p_avOut Receives the min/max boundaries. Must point to 2 vec3s
// piMatrix Transformation matrix of the graph at this position
//-------------------------------------------------------------------------------
int CalculateBounds(AssetHelper* g_pcAsset, aiNode* piNode, aiVector3D* p_avOut,
	const aiMatrix4x4& piMatrix)
{
	ai_assert(NULL != piNode);
	ai_assert(NULL != p_avOut);

	aiMatrix4x4 mTemp = piNode->mTransformation;
	mTemp.Transpose();
	aiMatrix4x4 aiMe = mTemp * piMatrix;

	for (unsigned int i = 0; i < piNode->mNumMeshes; ++i)
	{
		for (unsigned int a = 0; a < g_pcAsset->pcScene->mMeshes[
			piNode->mMeshes[i]]->mNumVertices; ++a)
		{
			aiVector3D pc = g_pcAsset->pcScene->mMeshes[piNode->mMeshes[i]]->mVertices[a];

			aiVector3D pc1;
			D3DXVec3TransformCoord((D3DXVECTOR3*)&pc1, (D3DXVECTOR3*)&pc,
				(D3DXMATRIX*)&aiMe);

			p_avOut[0].x = std::min(p_avOut[0].x, pc1.x);
			p_avOut[0].y = std::min(p_avOut[0].y, pc1.y);
			p_avOut[0].z = std::min(p_avOut[0].z, pc1.z);
			p_avOut[1].x = std::max(p_avOut[1].x, pc1.x);
			p_avOut[1].y = std::max(p_avOut[1].y, pc1.y);
			p_avOut[1].z = std::max(p_avOut[1].z, pc1.z);
		}
	}
	for (unsigned int i = 0; i < piNode->mNumChildren; ++i)
	{
		CalculateBounds(g_pcAsset, piNode->mChildren[i], p_avOut, aiMe);
	}
	return 1;
}
//-------------------------------------------------------------------------------
// Scale the asset that it fits perfectly into the viewer window
// The function calculates the boundaries of the mesh and modifies the
// global world transformation matrix according to the aset AABB
//-------------------------------------------------------------------------------
int ScaleAsset(AssetHelper* g_pcAsset, aiMatrix4x4* pOut)
{
	aiVector3D aiVecs[2] = { aiVector3D(1e10f, 1e10f, 1e10f),
		aiVector3D(-1e10f, -1e10f, -1e10f) };

	if (g_pcAsset->pcScene->mRootNode)
	{
		aiMatrix4x4 m;
		CalculateBounds(g_pcAsset, g_pcAsset->pcScene->mRootNode, aiVecs, m);
	}

	aiVector3D vDelta = aiVecs[1] - aiVecs[0];
	aiVector3D vHalf = aiVecs[0] + (vDelta / 2.0f);
	float fScale = 10.0f / vDelta.Length();

	*pOut = aiMatrix4x4(
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		-vHalf.x, -vHalf.y, -vHalf.z, 1.0f) *
		aiMatrix4x4(
			fScale, 0.0f, 0.0f, 0.0f,
			0.0f, fScale, 0.0f, 0.0f,
			0.0f, 0.0f, fScale, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f);
	return 1;
}


}


namespace RenderEngine
{


ModelImpl::ModelImpl()
	: g_pcAsset(NULL)
	, m_pMaterialMgr(NULL)
	, m_pMeshRender(NULL)
{
	struct aiLogStream stream;
	/* get a handle to the predefined STDOUT log stream and attach
	it to the logging system. It remains active for all further
	calls to aiImportFile(Ex) and aiApplyPostProcessing. */
	stream = aiGetPredefinedLogStream(aiDefaultLogStream_STDOUT, NULL);
	aiAttachLogStream(&stream);

	/* ... same procedure, but this stream now writes the
	log messages to assimp_log.txt */
	stream = aiGetPredefinedLogStream(aiDefaultLogStream_FILE, "assimp_log.txt");
	aiAttachLogStream(&stream);
}

ModelImpl::~ModelImpl()
{
	aiDetachAllLogStreams();
	delete g_pcAsset;
	delete m_pMaterialMgr;
	delete m_pMeshRender;
}

bool ModelImpl::Load(const char * path)
{
	ResetView();

	// default pp steps
	unsigned int ppsteps = aiProcess_CalcTangentSpace | // calculate tangents and bitangents if possible
		aiProcess_JoinIdenticalVertices | // join identical vertices/ optimize indexing
		aiProcess_ValidateDataStructure | // perform a full validation of the loader's output
		aiProcess_ImproveCacheLocality | // improve the cache locality of the output vertices
		aiProcess_RemoveRedundantMaterials | // remove redundant materials
		aiProcess_FindDegenerates | // remove degenerated polygons from the import
		aiProcess_FindInvalidData | // detect invalid model data, such as invalid normal vectors
		aiProcess_GenUVCoords | // convert spherical, cylindrical, box and planar mapping to proper UVs
		aiProcess_TransformUVCoords | // preprocess UV transformations (scaling, translation ...)
		aiProcess_FindInstances | // search for instanced meshes and remove them by references to one master
		aiProcess_LimitBoneWeights | // limit bone weights to 4 per vertex
		aiProcess_OptimizeMeshes | // join small meshes, if possible;
		aiProcess_SplitByBoneCount | // split meshes with too many bones. Necessary for our (limited) hardware skinning shader
		0;

	unsigned int ppstepsdefault = ppsteps;
	float g_smoothAngle = 80.f;
	bool nopointslines = false;

	aiPropertyStore* props = aiCreatePropertyStore();
	aiSetImportPropertyInteger(props, AI_CONFIG_IMPORT_TER_MAKE_UVS, 1);
	aiSetImportPropertyFloat(props, AI_CONFIG_PP_GSN_MAX_SMOOTHING_ANGLE, g_smoothAngle);
	aiSetImportPropertyInteger(props, AI_CONFIG_PP_SBP_REMOVE, nopointslines ? aiPrimitiveType_LINE | aiPrimitiveType_POINT : 0);

	aiSetImportPropertyInteger(props, AI_CONFIG_GLOB_MEASURE_TIME, 1);
	//aiSetImportPropertyInteger(props,AI_CONFIG_PP_PTV_KEEP_HIERARCHY,1);

	g_pcAsset = new AssimpView::AssetHelper();
	// Call ASSIMPs C-API to load the file
	g_pcAsset->pcScene = (aiScene*)aiImportFileExWithProperties(path,
		ppsteps | /* configurable pp steps */
		aiProcess_GenSmoothNormals | // generate smooth normal vectors if not existing
		aiProcess_SplitLargeMeshes | // split large, unrenderable meshes into submeshes
		aiProcess_Triangulate | // triangulate polygons with more than 3 edges
		aiProcess_ConvertToLeftHanded | // convert everything to D3D left handed space
		aiProcess_SortByPType | // make 'clean' meshes which consist of a single typ of primitives
		0,
		NULL,
		props);

	aiReleasePropertyStore(props);

	return (NULL != g_pcAsset->pcScene);
}

bool ModelImpl::PostLoad(IDirect3DDevice9* g_piDevice, ID3DXEffect* g_piDefaultEffect)
{
	// allocate a new MeshHelper array and build a new instance
	// for each mesh in the original asset
	g_pcAsset->apcMeshes = new AssimpView::AssetHelper::MeshHelper*[g_pcAsset->pcScene->mNumMeshes]();
	for (unsigned int i = 0; i < g_pcAsset->pcScene->mNumMeshes; ++i)
		g_pcAsset->apcMeshes[i] = new AssimpView::AssetHelper::MeshHelper();

	// create animator
	g_pcAsset->mAnimator = new AssimpView::SceneAnimator(g_pcAsset->pcScene);

	g_pcAsset->iNormalSet = AssimpView::AssetHelper::ORIGINAL;

	// scale the asset vertices to fit into the viewer window
	AssimpView::ScaleAsset(g_pcAsset, &g_mWorld);

	// reset the camera view to the default position
	g_sCamera.vPos = aiVector3D(0.0f, 0.0f, -10.0f);
	g_sCamera.vLookAt = aiVector3D(0.0f, 0.0f, 1.0f);
	g_sCamera.vUp = aiVector3D(0.0f, 1.0f, 0.0f);
	g_sCamera.vRight = aiVector3D(0.0f, 1.0f, 0.0f);

	if (!m_pMaterialMgr)
		m_pMaterialMgr = new AssimpView::CMaterialManager(g_sOptions, g_sCamera, g_pcAsset);
	else
		m_pMaterialMgr->Reset();
	if(!m_pMeshRender)
		m_pMeshRender = new AssimpView::CMeshRenderer(g_sOptions, g_sCamera, g_pcAsset);

	// build native D3D vertex/index buffers, textures, materials
	if (1 != CreateAssetData(g_piDevice, g_piDefaultEffect))
		return false;

	//other environment settings
	D3DVERTEXELEMENT9* vdecl = AssimpView::AssetHelper::Vertex::GetDeclarationElements();
	if (FAILED(g_piDevice->CreateVertexDeclaration(vdecl, &gDefaultVertexDecl)))
	{
		return false;
	}

	return true;
}

//-------------------------------------------------------------------------------
// Generate a vertex buffer which holds the normals of the asset as
// a list of unconnected lines
// pcMesh Input mesh
// pcSource Source mesh from ASSIMP
//-------------------------------------------------------------------------------
int ModelImpl::GenerateNormalsAsLineList(IDirect3DDevice9* g_piDevice, AssimpView::AssetHelper::MeshHelper* pcMesh, const aiMesh* pcSource)
{
	ai_assert(NULL != pcMesh);
	ai_assert(NULL != pcSource);

	if (!pcSource->mNormals)return 0;

	// create vertex buffer
	if (FAILED(g_piDevice->CreateVertexBuffer(sizeof(AssimpView::AssetHelper::LineVertex) *
		pcSource->mNumVertices * 2,
		D3DUSAGE_WRITEONLY,
		AssimpView::AssetHelper::LineVertex::GetFVF(),
		D3DPOOL_DEFAULT, &pcMesh->piVBNormals, NULL)))
	{
		assert(false);
		return 2;
	}

	// now fill the vertex buffer with data
	AssimpView::AssetHelper::LineVertex* pbData2;
	pcMesh->piVBNormals->Lock(0, 0, (void**)&pbData2, 0);
	for (unsigned int x = 0; x < pcSource->mNumVertices; ++x)
	{
		pbData2->vPosition = pcSource->mVertices[x];

		++pbData2;

		aiVector3D vNormal = pcSource->mNormals[x];
		vNormal.Normalize();

		// scalo with the inverse of the world scaling to make sure
		// the normals have equal length in each case
		// TODO: Check whether this works in every case, I don't think so
		vNormal.x /= g_mWorld.a1 * 4;
		vNormal.y /= g_mWorld.b2 * 4;
		vNormal.z /= g_mWorld.c3 * 4;

		pbData2->vPosition = pcSource->mVertices[x] + vNormal;

		++pbData2;
	}
	pcMesh->piVBNormals->Unlock();
	return 1;
}

//-------------------------------------------------------------------------------
// Create the native D3D representation of the asset: vertex buffers,
// index buffers, materials ...
//-------------------------------------------------------------------------------
int ModelImpl::CreateAssetData(IDirect3DDevice9* g_piDevice, ID3DXEffect* g_piDefaultEffect)
{
	if (!g_pcAsset)return 0;

	// reset all subsystems
	//m_pMaterialMgr->Reset();
	//CDisplay::Instance().Reset();

	for (unsigned int i = 0; i < g_pcAsset->pcScene->mNumMeshes; ++i)
	{
		const aiMesh* mesh = g_pcAsset->pcScene->mMeshes[i];

		// create the material for the mesh
		if (!g_pcAsset->apcMeshes[i]->piEffect) {
			m_pMaterialMgr->CreateMaterial(g_piDevice, g_piDefaultEffect,
				g_pcAsset->apcMeshes[i], mesh);
		}

		// create vertex buffer
		if (FAILED(g_piDevice->CreateVertexBuffer(sizeof(AssimpView::AssetHelper::Vertex) *
			mesh->mNumVertices,
			D3DUSAGE_WRITEONLY,
			0,
			D3DPOOL_DEFAULT, &g_pcAsset->apcMeshes[i]->piVB, NULL)))
		{
			assert(false);
			return 2;
		}

		DWORD dwUsage = 0;
		if (g_pcAsset->apcMeshes[i]->piOpacityTexture || 1.0f != g_pcAsset->apcMeshes[i]->fOpacity)
			dwUsage |= D3DUSAGE_DYNAMIC;

		unsigned int nidx;
		switch (mesh->mPrimitiveTypes) {
		case aiPrimitiveType_POINT:
			nidx = 1; break;
		case aiPrimitiveType_LINE:
			nidx = 2; break;
		case aiPrimitiveType_TRIANGLE:
			nidx = 3; break;
		default: ai_assert(false);
		};

		// check whether we can use 16 bit indices
		if (mesh->mNumFaces * 3 >= 65536) {
			// create 32 bit index buffer
			if (FAILED(g_piDevice->CreateIndexBuffer(4 *
				mesh->mNumFaces * nidx,
				D3DUSAGE_WRITEONLY | dwUsage,
				D3DFMT_INDEX32,
				D3DPOOL_DEFAULT,
				&g_pcAsset->apcMeshes[i]->piIB,
				NULL)))
			{
				assert(false);
				return 2;
			}

			// now fill the index buffer
			unsigned int* pbData;
			g_pcAsset->apcMeshes[i]->piIB->Lock(0, 0, (void**)&pbData, 0);
			for (unsigned int x = 0; x < mesh->mNumFaces; ++x)
			{
				for (unsigned int a = 0; a < nidx; ++a)
				{
					*pbData++ = mesh->mFaces[x].mIndices[a];
				}
			}
		}
		else {
			// create 16 bit index buffer
			if (FAILED(g_piDevice->CreateIndexBuffer(2 *
				mesh->mNumFaces * nidx,
				D3DUSAGE_WRITEONLY | dwUsage,
				D3DFMT_INDEX16,
				D3DPOOL_DEFAULT,
				&g_pcAsset->apcMeshes[i]->piIB,
				NULL)))
			{
				assert(false);
				return 2;
			}

			// now fill the index buffer
			uint16_t* pbData;
			g_pcAsset->apcMeshes[i]->piIB->Lock(0, 0, (void**)&pbData, 0);
			for (unsigned int x = 0; x < mesh->mNumFaces; ++x)
			{
				for (unsigned int a = 0; a < nidx; ++a)
				{
					*pbData++ = (uint16_t)mesh->mFaces[x].mIndices[a];
				}
			}
		}
		g_pcAsset->apcMeshes[i]->piIB->Unlock();

		// collect weights on all vertices. Quick and careless
		std::vector<std::vector<aiVertexWeight> > weightsPerVertex(mesh->mNumVertices);
		for (unsigned int a = 0; a < mesh->mNumBones; a++) {
			const aiBone* bone = mesh->mBones[a];
			for (unsigned int b = 0; b < bone->mNumWeights; b++)
				weightsPerVertex[bone->mWeights[b].mVertexId].push_back(aiVertexWeight(a, bone->mWeights[b].mWeight));
		}

		// now fill the vertex buffer
		AssimpView::AssetHelper::Vertex* pbData2;
		g_pcAsset->apcMeshes[i]->piVB->Lock(0, 0, (void**)&pbData2, 0);
		for (unsigned int x = 0; x < mesh->mNumVertices; ++x)
		{
			pbData2->vPosition = mesh->mVertices[x];

			if (NULL == mesh->mNormals)
				pbData2->vNormal = aiVector3D(0.0f, 0.0f, 0.0f);
			else pbData2->vNormal = mesh->mNormals[x];

			if (NULL == mesh->mTangents) {
				pbData2->vTangent = aiVector3D(0.0f, 0.0f, 0.0f);
				pbData2->vBitangent = aiVector3D(0.0f, 0.0f, 0.0f);
			}
			else {
				pbData2->vTangent = mesh->mTangents[x];
				pbData2->vBitangent = mesh->mBitangents[x];
			}

			if (mesh->HasVertexColors(0)) {
				pbData2->dColorDiffuse = D3DCOLOR_ARGB(
					((unsigned char)std::max(std::min(mesh->mColors[0][x].a * 255.0f, 255.0f), 0.0f)),
					((unsigned char)std::max(std::min(mesh->mColors[0][x].r * 255.0f, 255.0f), 0.0f)),
					((unsigned char)std::max(std::min(mesh->mColors[0][x].g * 255.0f, 255.0f), 0.0f)),
					((unsigned char)std::max(std::min(mesh->mColors[0][x].b * 255.0f, 255.0f), 0.0f)));
			}
			else pbData2->dColorDiffuse = D3DCOLOR_ARGB(0xFF, 0xff, 0xff, 0xff);

			// ignore a third texture coordinate component
			if (mesh->HasTextureCoords(0)) {
				pbData2->vTextureUV = aiVector2D(
					mesh->mTextureCoords[0][x].x,
					mesh->mTextureCoords[0][x].y);
			}
			else pbData2->vTextureUV = aiVector2D(0.5f, 0.5f);

			if (mesh->HasTextureCoords(1)) {
				pbData2->vTextureUV2 = aiVector2D(
					mesh->mTextureCoords[1][x].x,
					mesh->mTextureCoords[1][x].y);
			}
			else pbData2->vTextureUV2 = aiVector2D(0.5f, 0.5f);

			// Bone indices and weights
			if (mesh->HasBones()) {
				unsigned char boneIndices[4] = { 0, 0, 0, 0 };
				unsigned char boneWeights[4] = { 0, 0, 0, 0 };
				ai_assert(weightsPerVertex[x].size() <= 4);
				for (unsigned int a = 0; a < weightsPerVertex[x].size(); a++)
				{
					boneIndices[a] = weightsPerVertex[x][a].mVertexId;
					boneWeights[a] = (unsigned char)(weightsPerVertex[x][a].mWeight * 255.0f);
				}

				memcpy(pbData2->mBoneIndices, boneIndices, sizeof(boneIndices));
				memcpy(pbData2->mBoneWeights, boneWeights, sizeof(boneWeights));
			}
			else
			{
				memset(pbData2->mBoneIndices, 0, sizeof(pbData2->mBoneIndices));
				memset(pbData2->mBoneWeights, 0, sizeof(pbData2->mBoneWeights));
			}

			++pbData2;
		}
		g_pcAsset->apcMeshes[i]->piVB->Unlock();

		// now generate the second vertex buffer, holding all normals
		if (!g_pcAsset->apcMeshes[i]->piVBNormals) {
			GenerateNormalsAsLineList(g_piDevice, g_pcAsset->apcMeshes[i], mesh);
		}
	}
	return 1;
}

//-------------------------------------------------------------------------------
// Delete all effects, textures, vertex buffers ... associated with
// an asset
//-------------------------------------------------------------------------------
int ModelImpl::DeleteAssetData(bool bNoMaterials)
{
	if (!g_pcAsset)return 0;

	// TODO: Move this to a proper destructor
	for (unsigned int i = 0; i < g_pcAsset->pcScene->mNumMeshes; ++i)
	{
		if (g_pcAsset->apcMeshes[i]->piVB)
		{
			g_pcAsset->apcMeshes[i]->piVB->Release();
			g_pcAsset->apcMeshes[i]->piVB = NULL;
		}
		if (g_pcAsset->apcMeshes[i]->piVBNormals)
		{
			g_pcAsset->apcMeshes[i]->piVBNormals->Release();
			g_pcAsset->apcMeshes[i]->piVBNormals = NULL;
		}
		if (g_pcAsset->apcMeshes[i]->piIB)
		{
			g_pcAsset->apcMeshes[i]->piIB->Release();
			g_pcAsset->apcMeshes[i]->piIB = NULL;
		}

		// TODO ... unfixed memory leak
		// delete storage eventually allocated to hold a copy
		// of the original vertex normals
		//if (g_pcAsset->apcMeshes[i]->pvOriginalNormals)
		//{
		//	delete[] g_pcAsset->apcMeshes[i]->pvOriginalNormals;
		//}

		if (!bNoMaterials)
		{
			if (g_pcAsset->apcMeshes[i]->piEffect)
			{
				g_pcAsset->apcMeshes[i]->piEffect->Release();
				g_pcAsset->apcMeshes[i]->piEffect = NULL;
			}
			if (g_pcAsset->apcMeshes[i]->piDiffuseTexture)
			{
				g_pcAsset->apcMeshes[i]->piDiffuseTexture->Release();
				g_pcAsset->apcMeshes[i]->piDiffuseTexture = NULL;
			}
			if (g_pcAsset->apcMeshes[i]->piNormalTexture)
			{
				g_pcAsset->apcMeshes[i]->piNormalTexture->Release();
				g_pcAsset->apcMeshes[i]->piNormalTexture = NULL;
			}
			if (g_pcAsset->apcMeshes[i]->piSpecularTexture)
			{
				g_pcAsset->apcMeshes[i]->piSpecularTexture->Release();
				g_pcAsset->apcMeshes[i]->piSpecularTexture = NULL;
			}
			if (g_pcAsset->apcMeshes[i]->piAmbientTexture)
			{
				g_pcAsset->apcMeshes[i]->piAmbientTexture->Release();
				g_pcAsset->apcMeshes[i]->piAmbientTexture = NULL;
			}
			if (g_pcAsset->apcMeshes[i]->piEmissiveTexture)
			{
				g_pcAsset->apcMeshes[i]->piEmissiveTexture->Release();
				g_pcAsset->apcMeshes[i]->piEmissiveTexture = NULL;
			}
			if (g_pcAsset->apcMeshes[i]->piOpacityTexture)
			{
				g_pcAsset->apcMeshes[i]->piOpacityTexture->Release();
				g_pcAsset->apcMeshes[i]->piOpacityTexture = NULL;
			}
			if (g_pcAsset->apcMeshes[i]->piShininessTexture)
			{
				g_pcAsset->apcMeshes[i]->piShininessTexture->Release();
				g_pcAsset->apcMeshes[i]->piShininessTexture = NULL;
			}
		}
	}
	return 1;
}

//-------------------------------------------------------------------------------
// Delete the loaded asset
// The function does nothing is no asset is loaded
//-------------------------------------------------------------------------------
bool ModelImpl::Unload(void)
{
	if (!g_pcAsset)return false;

	// delete everything
	DeleteAssetData();
	for (unsigned int i = 0; i < g_pcAsset->pcScene->mNumMeshes; ++i)
	{
		delete g_pcAsset->apcMeshes[i];
	}
	aiReleaseImport(g_pcAsset->pcScene);
	delete[] g_pcAsset->apcMeshes;
	delete g_pcAsset->mAnimator;
	delete g_pcAsset;
	g_pcAsset = NULL;

	delete m_pMaterialMgr;
	m_pMaterialMgr = NULL;
	return 1;
}


//-------------------------------------------------------------------------------
// Render the full scene, all nodes
int ModelImpl::RenderFullScene(IDirect3DDevice9* g_piDevice, ID3DXEffect* g_piDefaultEffect)
{
	aiMatrix4x4 pcProj;
	GetProjectionMatrix(pcProj);
	//pcProj = mProjection;

	vPos = GetCameraMatrix(mViewProjection);
	mViewProjection = mViewProjection * pcProj;

	// setup wireframe/solid rendering mode
	if (g_sOptions.eDrawMode == RenderOptions::WIREFRAME)
		g_piDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
	else g_piDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);

	if (g_sOptions.bCulling)
		g_piDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
	else g_piDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);

	// for high-quality mode, enable anisotropic texture filtering
	if (g_sOptions.bLowQuality) {
		for (DWORD d = 0; d < 8; ++d) {
			g_piDevice->SetSamplerState(d, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
			g_piDevice->SetSamplerState(d, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
			g_piDevice->SetSamplerState(d, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
		}
	}
	else {
		for (DWORD d = 0; d < 8; ++d) {
			g_piDevice->SetSamplerState(d, D3DSAMP_MAGFILTER, D3DTEXF_ANISOTROPIC);
			g_piDevice->SetSamplerState(d, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC);
			g_piDevice->SetSamplerState(d, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);

			g_piDevice->SetSamplerState(d, D3DSAMP_MAXANISOTROPY, g_sCaps.MaxAnisotropy);
		}
	}

	// draw the scene background (clear and texture 2d)
	//CBackgroundPainter::Instance().OnPreRender();
	//g_piDevice->Clear(0, NULL, D3DCLEAR_ZBUFFER | D3DCLEAR_TARGET,
	//	D3DCOLOR_ARGB(0xff, 100, 100, 100), 1.0f, 0);
	g_piDevice->Clear(0, NULL, D3DCLEAR_ZBUFFER, 0, 1.0f, 0);

	aiMatrix4x4 m = g_mWorld;
	// draw all opaque objects in the scene
	if (NULL != g_pcAsset && NULL != g_pcAsset->pcScene->mRootNode)
	{
		RenderNode(g_piDevice, g_piDefaultEffect, g_pcAsset->pcScene->mRootNode, m, false);
	}

	// draw all non-opaque objects in the scene
	if (NULL != g_pcAsset && NULL != g_pcAsset->pcScene->mRootNode)
	{
		// disable the z-buffer
		if (!g_sOptions.bNoAlphaBlending) {
			g_piDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
		}
		RenderNode(g_piDevice, g_piDefaultEffect, g_pcAsset->pcScene->mRootNode, m, true);

		if (!g_sOptions.bNoAlphaBlending) {
			g_piDevice->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
		}
	}

	// draw the HUD texture on top of the rendered scene using
	// pre-projected vertices
	//if (!g_bFPSView && g_pcAsset && g_pcTexture)
	//	DrawHUD();

	return 1;
}

void ModelImpl::ResetView()
{
	vPos = g_sCamera.vPos = aiVector3D(0.0f, 0.0f, -10.0f);
	g_sCamera.vLookAt = aiVector3D(0.0f, 0.0f, 1.0f);
	g_sCamera.vUp = aiVector3D(0.0f, 1.0f, 0.0f);
	g_sCamera.vRight = aiVector3D(0.0f, 1.0f, 0.0f);
	//g_mWorldRotate = aiMatrix4x4();
	g_mWorld = aiMatrix4x4();
	mView = aiMatrix4x4();
	mProjection = aiMatrix4x4();
	
	m_WindowRect.left = m_WindowRect.top = 0;
	m_WindowRect.right = 1920; m_WindowRect.bottom = 1080;
}

void ModelImpl::SetWorldMatrix(const D3DXMATRIX* mat)
{
	return;		//TODO: get world matrix from DXUT

	g_mWorld.a1 = mat->_11;
	g_mWorld.a2 = mat->_12;
	g_mWorld.a3 = mat->_13;
	g_mWorld.a4 = mat->_14;

	g_mWorld.b1 = mat->_21;
	g_mWorld.b2 = mat->_22;
	g_mWorld.b3 = mat->_23;
	g_mWorld.b4 = mat->_24;

	g_mWorld.c1 = mat->_31;
	g_mWorld.c2 = mat->_32;
	g_mWorld.c3 = mat->_33;
	g_mWorld.c4 = mat->_34;

	g_mWorld.d1 = mat->_41;
	g_mWorld.d2 = mat->_42;
	g_mWorld.d3 = mat->_43;
	g_mWorld.d4 = mat->_44;
}

void ModelImpl::SetViewMatrix(const D3DXMATRIX* mat)
{
	mView.a1 = mat->_11;
	mView.a2 = mat->_12;
	mView.a3 = mat->_13;
	mView.a4 = mat->_14;

	mView.b1 = mat->_21;
	mView.b2 = mat->_22;
	mView.b3 = mat->_23;
	mView.b4 = mat->_24;

	mView.c1 = mat->_31;
	mView.c2 = mat->_32;
	mView.c3 = mat->_33;
	mView.c4 = mat->_34;

	mView.d1 = mat->_41;
	mView.d2 = mat->_42;
	mView.d3 = mat->_43;
	mView.d4 = mat->_44;
}

void ModelImpl::SetProjectMatrix(const D3DXMATRIX* mat)
{
	mProjection.a1 = mat->_11;
	mProjection.a2 = mat->_12;
	mProjection.a3 = mat->_13;
	mProjection.a4 = mat->_14;

	mProjection.b1 = mat->_21;
	mProjection.b2 = mat->_22;
	mProjection.b3 = mat->_23;
	mProjection.b4 = mat->_24;

	mProjection.c1 = mat->_31;
	mProjection.c2 = mat->_32;
	mProjection.c3 = mat->_33;
	mProjection.c4 = mat->_34;

	mProjection.d1 = mat->_41;
	mProjection.d2 = mat->_42;
	mProjection.d3 = mat->_43;
	mProjection.d4 = mat->_44;
}

void ModelImpl::SetWindowRect(const RECT* pRect)
{
	m_WindowRect = *pRect;
}


ID3DXEffect * ModelImpl::CreateDefaultEffect(IDirect3DDevice9 * pd3dDevice)
{
	ID3DXEffect* g_piDefaultEffect = NULL;
	// compile the default material shader (gray gouraud/phong)
	ID3DXBuffer* piBuffer = NULL;
	if (FAILED(D3DXCreateEffect(pd3dDevice,
		AssimpView::g_szDefaultShader.c_str(),
		(UINT)AssimpView::g_szDefaultShader.length(),
		NULL,
		NULL,
		AI_SHADER_COMPILE_FLAGS,
		NULL,
		&g_piDefaultEffect, &piBuffer)))
	{
		if (piBuffer)
		{
			//MessageBox(g_hDlg, (LPCSTR)piBuffer->GetBufferPointer(), "HLSL", MB_OK);
			assert(false);
			piBuffer->Release();
		}
		return 0;
	}
	if (piBuffer)
	{
		piBuffer->Release();
		piBuffer = NULL;
	}

	// use Fixed Function effect when working with shaderless cards
	if (g_sCaps.PixelShaderVersion < D3DPS_VERSION(2, 0))
		g_piDefaultEffect->SetTechnique("DefaultFXSpecular_FF");

	return g_piDefaultEffect;
}

int ModelImpl::GetProjectionMatrix(aiMatrix4x4& p_mOut)
{
	const float fFarPlane = 100.0f;
	const float fNearPlane = 0.1f;
	const float fFOV = (float)(45.0 * 0.0174532925);

	const float s = 1.0f / tanf(fFOV * 0.5f);
	const float Q = fFarPlane / (fFarPlane - fNearPlane);

	RECT sRect = m_WindowRect;
	//GetWindowRect(GetDlgItem(g_hDlg, IDC_RT), &sRect);
	sRect.right -= sRect.left;
	sRect.bottom -= sRect.top;
	const float fAspect = (float)sRect.right / (float)sRect.bottom;

	p_mOut = aiMatrix4x4(
		s / fAspect, 0.0f, 0.0f, 0.0f,
		0.0f, s, 0.0f, 0.0f,
		0.0f, 0.0f, Q, 1.0f,
		0.0f, 0.0f, -Q * fNearPlane, 0.0f);
	return 1;
}

aiVector3D ModelImpl::GetCameraMatrix(aiMatrix4x4& p_mOut)
{
	D3DXMATRIX view;
	D3DXMatrixIdentity(&view);

	D3DXVec3Normalize((D3DXVECTOR3*)&g_sCamera.vLookAt, (D3DXVECTOR3*)&g_sCamera.vLookAt);
	D3DXVec3Cross((D3DXVECTOR3*)&g_sCamera.vRight, (D3DXVECTOR3*)&g_sCamera.vUp, (D3DXVECTOR3*)&g_sCamera.vLookAt);
	D3DXVec3Normalize((D3DXVECTOR3*)&g_sCamera.vRight, (D3DXVECTOR3*)&g_sCamera.vRight);
	D3DXVec3Cross((D3DXVECTOR3*)&g_sCamera.vUp, (D3DXVECTOR3*)&g_sCamera.vLookAt, (D3DXVECTOR3*)&g_sCamera.vRight);
	D3DXVec3Normalize((D3DXVECTOR3*)&g_sCamera.vUp, (D3DXVECTOR3*)&g_sCamera.vUp);

	view._11 = g_sCamera.vRight.x;
	view._12 = g_sCamera.vUp.x;
	view._13 = g_sCamera.vLookAt.x;
	view._14 = 0.0f;

	view._21 = g_sCamera.vRight.y;
	view._22 = g_sCamera.vUp.y;
	view._23 = g_sCamera.vLookAt.y;
	view._24 = 0.0f;

	view._31 = g_sCamera.vRight.z;
	view._32 = g_sCamera.vUp.z;
	view._33 = g_sCamera.vLookAt.z;
	view._34 = 0.0f;

	view._41 = -D3DXVec3Dot((D3DXVECTOR3*)&g_sCamera.vPos, (D3DXVECTOR3*)&g_sCamera.vRight);
	view._42 = -D3DXVec3Dot((D3DXVECTOR3*)&g_sCamera.vPos, (D3DXVECTOR3*)&g_sCamera.vUp);
	view._43 = -D3DXVec3Dot((D3DXVECTOR3*)&g_sCamera.vPos, (D3DXVECTOR3*)&g_sCamera.vLookAt);
	view._44 = 1.0f;

	memcpy(&p_mOut, &view, sizeof(aiMatrix4x4));

	return g_sCamera.vPos;
}

//-------------------------------------------------------------------------------
// Render a single node
int ModelImpl::RenderNode(IDirect3DDevice9* g_piDevice, ID3DXEffect* g_piDefaultEffect,
	aiNode* piNode, const aiMatrix4x4& piMatrix, bool bAlpha /*= false*/)
{
	aiMatrix4x4 aiMe = g_pcAsset->mAnimator->GetGlobalTransform(piNode);

	aiMe.Transpose();
	aiMe *= piMatrix;

	bool bChangedVM = false;
	//if (VIEWMODE_NODE == m_iViewMode && m_pcCurrentNode)
	//{
	//	if (piNode != m_pcCurrentNode->psNode)
	//	{
	//		// directly call our children
	//		for (unsigned int i = 0; i < piNode->mNumChildren; ++i)
	//			RenderNode(piNode->mChildren[i], piMatrix, bAlpha);

	//		return 1;
	//	}
	//	m_iViewMode = VIEWMODE_FULL;
	//	bChangedVM = true;
	//}

	aiMatrix4x4 pcProj = aiMe * mViewProjection;

	aiMatrix4x4 pcCam = aiMe;
	pcCam.Inverse().Transpose();

	// VERY UNOPTIMIZED, much stuff is redundant. Who cares?
	if (!g_sOptions.bRenderMats && !bAlpha)
	{
		// this is very similar to the code in SetupMaterial()
		ID3DXEffect* piEnd = g_piDefaultEffect;

		// commit transformation matrices to the shader
		piEnd->SetMatrix("WorldViewProjection",
			(const D3DXMATRIX*)&pcProj);

		piEnd->SetMatrix("World", (const D3DXMATRIX*)&aiMe);
		piEnd->SetMatrix("WorldInverseTranspose",
			(const D3DXMATRIX*)&pcCam);

		// commit light colors and direction to the shader
		D3DXVECTOR4 apcVec[5];
		apcVec[0].x = AssimpView::g_avLightDirs[0].x;
		apcVec[0].y = AssimpView::g_avLightDirs[0].y;
		apcVec[0].z = AssimpView::g_avLightDirs[0].z;
		apcVec[0].w = 0.0f;
		apcVec[1].x = AssimpView::g_avLightDirs[0].x * -1.0f;
		apcVec[1].y = AssimpView::g_avLightDirs[0].y * -1.0f;
		apcVec[1].z = AssimpView::g_avLightDirs[0].z * -1.0f;
		apcVec[1].w = 0.0f;

		D3DXVec4Normalize(&apcVec[0], &apcVec[0]);
		D3DXVec4Normalize(&apcVec[1], &apcVec[1]);
		piEnd->SetVectorArray("afLightDir", apcVec, 5);

		apcVec[0].x = ((AssimpView::g_avLightColors[0] >> 16) & 0xFF) / 255.0f;
		apcVec[0].y = ((AssimpView::g_avLightColors[0] >> 8) & 0xFF) / 255.0f;
		apcVec[0].z = ((AssimpView::g_avLightColors[0]) & 0xFF) / 255.0f;
		apcVec[0].w = 1.0f;

		if (g_sOptions.b3Lights)
		{
			apcVec[1].x = ((AssimpView::g_avLightColors[1] >> 16) & 0xFF) / 255.0f;
			apcVec[1].y = ((AssimpView::g_avLightColors[1] >> 8) & 0xFF) / 255.0f;
			apcVec[1].z = ((AssimpView::g_avLightColors[1]) & 0xFF) / 255.0f;
			apcVec[1].w = 0.0f;
		}
		else
		{
			apcVec[1].x = 0.0f;
			apcVec[1].y = 0.0f;
			apcVec[1].z = 0.0f;
			apcVec[1].w = 0.0f;
		}

		apcVec[0] *= AssimpView::g_fLightIntensity;
		apcVec[1] *= AssimpView::g_fLightIntensity;
		piEnd->SetVectorArray("afLightColor", apcVec, 5);

		apcVec[0].x = vPos.x;
		apcVec[0].y = vPos.y;
		apcVec[0].z = vPos.z;
		piEnd->SetVector("vCameraPos", &apcVec[0]);

		// setup the best technique
		if (g_sCaps.PixelShaderVersion < D3DPS_VERSION(2, 0))
		{
			g_piDefaultEffect->SetTechnique("DefaultFXSpecular_FF");
		}
		else
			if (g_sCaps.PixelShaderVersion < D3DPS_VERSION(3, 0) || g_sOptions.bLowQuality)
			{
				if (g_sOptions.b3Lights)
					piEnd->SetTechnique("DefaultFXSpecular_PS20_D2");
				else piEnd->SetTechnique("DefaultFXSpecular_PS20_D1");
			}
			else
			{
				if (g_sOptions.b3Lights)
					piEnd->SetTechnique("DefaultFXSpecular_D2");
				else piEnd->SetTechnique("DefaultFXSpecular_D1");
			}

		// setup the default material
		UINT dwPasses = 0;
		piEnd->Begin(&dwPasses, 0);
		piEnd->BeginPass(0);
	}

	if (!(!g_sOptions.bRenderMats && bAlpha))
	{
		for (unsigned int i = 0; i < piNode->mNumMeshes; ++i)
		{
			const aiMesh* mesh = g_pcAsset->pcScene->mMeshes[piNode->mMeshes[i]];
			AssimpView::AssetHelper::MeshHelper* helper = g_pcAsset->apcMeshes[piNode->mMeshes[i]];

			// don't render the mesh if the render pass is incorrect
			if (g_sOptions.bRenderMats && (helper->piOpacityTexture || helper->fOpacity != 1.0f) && !mesh->HasBones())
			{
				if (!bAlpha)continue;
			}
			else if (bAlpha)continue;

			// Upload bone matrices. This maybe is the wrong place to do it, but for the heck of it I don't understand this code flow
			if (mesh->HasBones())
			{
				if (helper->piEffect)
				{
					static float matrices[4 * 4 * 60];
					float* tempmat = matrices;
					const std::vector<aiMatrix4x4>& boneMats = g_pcAsset->mAnimator->GetBoneMatrices(piNode, i);
					ai_assert(boneMats.size() == mesh->mNumBones);

					for (unsigned int a = 0; a < mesh->mNumBones; a++)
					{
						const aiMatrix4x4& mat = boneMats[a];
						*tempmat++ = mat.a1; *tempmat++ = mat.a2; *tempmat++ = mat.a3; *tempmat++ = mat.a4;
						*tempmat++ = mat.b1; *tempmat++ = mat.b2; *tempmat++ = mat.b3; *tempmat++ = mat.b4;
						*tempmat++ = mat.c1; *tempmat++ = mat.c2; *tempmat++ = mat.c3; *tempmat++ = mat.c4;
						*tempmat++ = mat.d1; *tempmat++ = mat.d2; *tempmat++ = mat.d3; *tempmat++ = mat.d4;
						//tempmat += 4;
					}

					if (g_sOptions.bRenderMats)
					{
						helper->piEffect->SetMatrixTransposeArray("gBoneMatrix", (D3DXMATRIX*)matrices, 60);
					}
					else
					{
						g_piDefaultEffect->SetMatrixTransposeArray("gBoneMatrix", (D3DXMATRIX*)matrices, 60);
						g_piDefaultEffect->CommitChanges();
					}
				}
			}
			else
			{
				// upload identity matrices instead. Only the first is ever going to be used in meshes without bones
				if (!g_sOptions.bRenderMats)
				{
					D3DXMATRIX identity(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
					g_piDefaultEffect->SetMatrixTransposeArray("gBoneMatrix", &identity, 1);
					g_piDefaultEffect->CommitChanges();
				}
			}

			// now setup the material
			if (g_sOptions.bRenderMats)
			{
				m_pMaterialMgr->SetupMaterial(g_piDevice, g_piDefaultEffect,
					helper, pcProj, aiMe, pcCam, vPos);
			}
			g_piDevice->SetVertexDeclaration(gDefaultVertexDecl);

			if (g_sOptions.bNoAlphaBlending) {
				// manually disable alphablending
				g_piDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
			}

			if (bAlpha)
				m_pMeshRender->DrawSorted(g_piDevice, piNode->mMeshes[i], aiMe);
			else 
				m_pMeshRender->DrawUnsorted(g_piDevice, piNode->mMeshes[i]);

			// now end the material
			if (g_sOptions.bRenderMats)
			{
				m_pMaterialMgr->EndMaterial(g_piDevice, helper);
			}
		}
		// end the default material
		if (!g_sOptions.bRenderMats)
		{
			g_piDefaultEffect->EndPass();
			g_piDefaultEffect->End();
		}
	}

	// render all child nodes
	for (unsigned int i = 0; i < piNode->mNumChildren; ++i)
		RenderNode(g_piDevice, g_piDefaultEffect, piNode->mChildren[i], piMatrix, bAlpha);

	return 1;
}

}
