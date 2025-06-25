
#include "CubismRenderer_Diligent.h"

#include <cfloat>  // FLT_MAX,MIN

#include "Math/CubismMatrix44.hpp"
#include "Model/CubismModel.hpp"
#include "Type/csmVector.hpp"

//------------ LIVE2D NAMESPACE ------------
namespace Live2D {
namespace Cubism {
namespace Framework {
namespace Rendering {

/*********************************************************************************************************************
 *                                      CubismShaderBinding_Diligent
 ********************************************************************************************************************/

CubismShaderBinding_Diligent::CubismShaderBinding_Diligent() = default;

void CubismShaderBinding_Diligent::InitializeBinding(
    Diligent::IPipelineResourceSignature* signature) {
  signature->CreateShaderResourceBinding(&_binding);

  _constantBufferVert = _binding->GetVariableByName(
      Diligent::SHADER_TYPE_VERTEX, "CubismConstants");
  _constantBufferPixel = _binding->GetVariableByName(
      Diligent::SHADER_TYPE_PIXEL, "CubismConstants");
  _mainTexture =
      _binding->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "mainTexture");
  _maskTexture =
      _binding->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "maskTexture");
}

void CubismShaderBinding_Diligent::SetConstantBuffer(
    Diligent::IBuffer* constantBuffer) {
  _constantBufferVert->Set(constantBuffer);
  _constantBufferPixel->Set(constantBuffer);
}

void CubismShaderBinding_Diligent::SetMainTexture(
    Diligent::ITextureView* shaderResourceView) {
  _mainTexture->Set(shaderResourceView);
}

void CubismShaderBinding_Diligent::SetMaskTexture(
    Diligent::ITextureView* shaderResourceView) {
  _maskTexture->Set(shaderResourceView);
}

/*********************************************************************************************************************
 *                                      CubismClippingManager_Diligent
 ********************************************************************************************************************/

void CubismClippingManager_Diligent::SetupClippingContext(
    renderer::RenderDevice* device,
    renderer::RenderContext* renderContext,
    CubismModel& model,
    CubismRenderer_Diligent* renderer,
    csmInt32 offscreenCurrent) {
  // 全てのクリッピングを用意する
  // 同じクリップ（複数の場合はまとめて１つのクリップ）を使う場合は１度だけ設定する
  csmInt32 usingClipCount = 0;
  for (csmUint32 clipIndex = 0;
       clipIndex < _clippingContextListForMask.GetSize(); clipIndex++) {
    // １つのクリッピングマスクに関して
    CubismClippingContext_Diligent* cc = _clippingContextListForMask[clipIndex];

    // このクリップを利用する描画オブジェクト群全体を囲む矩形を計算
    CalcClippedDrawTotalBounds(model, cc);

    if (cc->_isUsing) {
      usingClipCount++;  // 使用中としてカウント
    }
  }

  if (usingClipCount <= 0) {
    return;
  }

  // 後の計算のためにインデックスの最初をセット
  _currentMaskBuffer = renderer->GetMaskBuffer(offscreenCurrent, 0);

  // ----- マスク描画処理 -----
  // マスク用RenderTextureをactiveにセット
  _currentMaskBuffer->BeginDraw(renderContext);

  // マスク作成処理
  // ビューポートは退避済み
  // 生成したOffscreenSurfaceと同じサイズでビューポートを設定
  Diligent::Rect clippingScissor(0, 0, _clippingMaskBufferSize.X,
                                 _clippingMaskBufferSize.Y);
  (*renderContext)
      ->SetScissorRects(1, &clippingScissor, UINT32_MAX, UINT32_MAX);

  // 各マスクのレイアウトを決定していく
  SetupLayoutBounds(usingClipCount);

  // サイズがレンダーテクスチャの枚数と合わない場合は合わせる
  if (_clearedMaskBufferFlags.GetSize() != _renderTextureCount) {
    _clearedMaskBufferFlags.Clear();

    for (csmInt32 i = 0; i < _renderTextureCount; ++i) {
      _clearedMaskBufferFlags.PushBack(false);
    }
  } else {
    // マスクのクリアフラグを毎フレーム開始時に初期化
    for (csmInt32 i = 0; i < _renderTextureCount; ++i) {
      _clearedMaskBufferFlags[i] = false;
    }
  }

  // 実際にマスクを生成する
  // 全てのマスクをどの様にレイアウトして描くかを決定し、ClipContext ,
  // ClippedDrawContext に記憶する
  for (csmUint32 clipIndex = 0;
       clipIndex < _clippingContextListForMask.GetSize(); clipIndex++) {
    // --- 実際に１つのマスクを描く ---
    CubismClippingContext_Diligent* clipContext =
        _clippingContextListForMask[clipIndex];
    csmRectF* allClippedDrawRect =
        clipContext
            ->_allClippedDrawRect;  // このマスクを使う、全ての描画オブジェクトの論理座標上の囲み矩形
    csmRectF* layoutBoundsOnTex01 =
        clipContext->_layoutBounds;  // この中にマスクを収める
    const csmFloat32 MARGIN = 0.05f;
    const csmBool isRightHanded = true;

    // clipContextに設定したレンダーテクスチャをインデックスで取得
    CubismOffscreenSurface_Diligent* clipContextRenderTexture =
        renderer->GetMaskBuffer(offscreenCurrent, clipContext->_bufferIndex);

    // 現在のレンダーテクスチャがclipContextのものと異なる場合
    if (_currentMaskBuffer != clipContextRenderTexture) {
      _currentMaskBuffer->EndDraw(renderContext);
      _currentMaskBuffer = clipContextRenderTexture;

      // マスク用RenderTextureをactiveにセット
      _currentMaskBuffer->BeginDraw(renderContext);
    }

    // モデル座標上の矩形を、適宜マージンを付けて使う
    _tmpBoundsOnModel.SetRect(allClippedDrawRect);
    _tmpBoundsOnModel.Expand(allClippedDrawRect->Width * MARGIN,
                             allClippedDrawRect->Height * MARGIN);
    // ########## 本来は割り当てられた領域の全体を使わず必要最低限のサイズがよい
    //  シェーダ用の計算式を求める。回転を考慮しない場合は以下のとおり
    //  movePeriod' = movePeriod * scaleX + offX [[ movePeriod' = (movePeriod -
    //  tmpBoundsOnModel.movePeriod)*scale + layoutBoundsOnTex01.movePeriod ]]
    csmFloat32 scaleX = layoutBoundsOnTex01->Width / _tmpBoundsOnModel.Width;
    csmFloat32 scaleY = layoutBoundsOnTex01->Height / _tmpBoundsOnModel.Height;

    // マスク生成時に使う行列を求める
    createMatrixForMask(isRightHanded, layoutBoundsOnTex01, scaleX, scaleY);

    clipContext->_matrixForMask.SetMatrix(_tmpMatrixForMask.GetArray());
    clipContext->_matrixForDraw.SetMatrix(_tmpMatrixForDraw.GetArray());

    const csmInt32 clipDrawCount = clipContext->_clippingIdCount;
    for (csmInt32 i = 0; i < clipDrawCount; i++) {
      const csmInt32 clipDrawIndex = clipContext->_clippingIdList[i];

      // 頂点情報が更新されておらず、信頼性がない場合は描画をパスする
      if (!model.GetDrawableDynamicFlagVertexPositionsDidChange(
              clipDrawIndex)) {
        continue;
      }

      renderer->IsCulling(model.GetDrawableCulling(clipDrawIndex) != 0);

      // マスクがクリアされていないなら処理する
      if (!_clearedMaskBufferFlags[clipContext->_bufferIndex]) {
        // マスクをクリアする
        // (仮仕様)
        // 1が無効（描かれない）領域、0が有効（描かれる）領域。（シェーダーCd*Csで0に近い値をかけてマスクを作る。1をかけると何も起こらない）
        renderer->GetMaskBuffer(offscreenCurrent, clipContext->_bufferIndex)
            ->Clear(renderContext, 1.0f, 1.0f, 1.0f, 1.0f);
        _clearedMaskBufferFlags[clipContext->_bufferIndex] = true;
      }

      // 今回専用の変換を適用して描く
      // チャンネルも切り替える必要がある(A,R,G,B)
      renderer->SetClippingContextBufferForMask(clipContext);
      renderer->DrawMeshDiligent(model, clipDrawIndex);
    }
  }

  // --- 後処理 ---
  _currentMaskBuffer->EndDraw(renderContext);
  renderer->SetClippingContextBufferForMask(nullptr);
}

/*********************************************************************************************************************
 *                                      CubismClippingContext_Diligent
 ********************************************************************************************************************/
CubismClippingContext_Diligent::CubismClippingContext_Diligent(
    CubismClippingManager<CubismClippingContext_Diligent,
                          CubismOffscreenSurface_Diligent>* manager,
    CubismModel& model,
    const csmInt32* clippingDrawableIndices,
    csmInt32 clipCount)
    : CubismClippingContext(clippingDrawableIndices, clipCount) {
  _isUsing = false;

  _owner = manager;
}

CubismClippingContext_Diligent::~CubismClippingContext_Diligent() {}

CubismClippingManager<CubismClippingContext_Diligent,
                      CubismOffscreenSurface_Diligent>*
CubismClippingContext_Diligent::GetClippingManager() {
  return _owner;
}

/*********************************************************************************************************************
 *                                      CubismRenderer_Diligent
 ********************************************************************************************************************/

// 各種静的変数
namespace {

CubismPipeline_Diligent* s_pipelinesInstance = nullptr;

csmUint32 s_bufferSetNum = 0;

renderer::RenderDevice* s_device = nullptr;
renderer::RenderContext* s_context = nullptr;

csmBool s_enableDepthTest = false;
csmUint32 s_viewportWidth = 0;
csmUint32 s_viewportHeight = 0;

}  // namespace

CubismRenderer* CubismRenderer::Create() {
  return CSM_NEW CubismRenderer_Diligent();
}

void CubismRenderer::StaticRelease() {
  CubismRenderer_Diligent::DeletePipelineManager();
}

CubismPipeline_Diligent* CubismRenderer_Diligent::GetPipelineManager() {
  if (s_pipelinesInstance == nullptr) {
    s_pipelinesInstance = CSM_NEW CubismPipeline_Diligent(**s_device);
  }
  return s_pipelinesInstance;
}

void CubismRenderer_Diligent::DeletePipelineManager() {
  if (s_pipelinesInstance) {
    CSM_DELETE_SELF(CubismPipeline_Diligent, s_pipelinesInstance);
    s_pipelinesInstance = nullptr;
  }
}

renderer::RenderDevice* CubismRenderer_Diligent::GetCurrentDevice() {
  return s_device;
}

CubismRenderer_Diligent::CubismRenderer_Diligent()
    : _vertexBuffers(nullptr),
      _indexBuffers(nullptr),
      _constantBuffers(nullptr),
      _drawableNum(0),
      _clippingManager(nullptr),
      _clippingContextBufferForMask(nullptr),
      _clippingContextBufferForDraw(nullptr) {
  _commandBufferNum = 0;
  _commandBufferCurrent = 0;

  // テクスチャ対応マップの容量を確保しておく.
  _textures.PrepareCapacity(32, true);
}

CubismRenderer_Diligent::~CubismRenderer_Diligent() {
  {
    // オフスクリーンを作成していたのなら開放
    for (csmUint32 i = 0; i < _offscreenSurfaces.GetSize(); i++) {
      for (csmUint32 j = 0; j < _offscreenSurfaces[i].GetSize(); j++) {
        _offscreenSurfaces[i][j].DestroyOffscreenSurface();
      }
      _offscreenSurfaces[i].Clear();
    }
    _offscreenSurfaces.Clear();
  }

  const csmInt32 drawableCount =
      _drawableNum;  // GetModel()->GetDrawableCount();

  for (csmUint32 buffer = 0; buffer < _commandBufferNum; buffer++) {
    for (csmUint32 drawAssign = 0; drawAssign < drawableCount; drawAssign++) {
      if (_constantBuffers[buffer][drawAssign]) {
        _constantBuffers[buffer][drawAssign]->Release();
        _constantBuffers[buffer][drawAssign] = nullptr;
      }
      // インデックス
      if (_indexBuffers[buffer][drawAssign]) {
        _indexBuffers[buffer][drawAssign]->Release();
        _indexBuffers[buffer][drawAssign] = nullptr;
      }
      // 頂点
      if (_vertexBuffers[buffer][drawAssign]) {
        _vertexBuffers[buffer][drawAssign]->Release();
        _vertexBuffers[buffer][drawAssign] = nullptr;
      }
    }

    CSM_FREE(_constantBuffers[buffer]);
    CSM_FREE(_indexBuffers[buffer]);
    CSM_FREE(_vertexBuffers[buffer]);
  }

  CSM_FREE(_constantBuffers);
  CSM_FREE(_indexBuffers);
  CSM_FREE(_vertexBuffers);

  CSM_DELETE_SELF(CubismClippingManager_Diligent, _clippingManager);
}

void CubismRenderer_Diligent::Initialize(CubismModel* model) {
  Initialize(model, 1);
}

void CubismRenderer_Diligent::Initialize(CubismModel* model,
                                         csmInt32 maskBufferCount) {
  if (s_device == 0) {
    CubismLogError("Device has not been set.");
    CSM_ASSERT(0);
    return;
  }

  if (maskBufferCount < 1) {
    maskBufferCount = 1;
    CubismLogWarning(
        "The number of render textures must be an integer greater than or "
        "equal to 1. Set the number of render textures to 1.");
  }

  if (model->IsUsingMasking()) {
    _clippingManager = CSM_NEW CubismClippingManager_Diligent();
    _clippingManager->Initialize(*model, maskBufferCount);

    const csmInt32 bufferWidth =
        _clippingManager->GetClippingMaskBufferSize().X;
    const csmInt32 bufferHeight =
        _clippingManager->GetClippingMaskBufferSize().Y;

    _offscreenSurfaces.Clear();

    for (csmUint32 i = 0; i < s_bufferSetNum; i++) {
      csmVector<CubismOffscreenSurface_Diligent> vector;
      _offscreenSurfaces.PushBack(vector);
      for (csmUint32 j = 0; j < maskBufferCount; j++) {
        CubismOffscreenSurface_Diligent offscreenSurface;
        offscreenSurface.CreateOffscreenSurface(s_device, bufferWidth,
                                                bufferHeight);
        _offscreenSurfaces[i].PushBack(offscreenSurface);
      }
    }
  }

  _sortedDrawableIndexList.Resize(model->GetDrawableCount(), 0);

  CubismRenderer::Initialize(model, maskBufferCount);

  _vertexBuffers = static_cast<Diligent::IBuffer***>(
      CSM_MALLOC(sizeof(Diligent::IBuffer**) * s_bufferSetNum));
  _indexBuffers = static_cast<Diligent::IBuffer***>(
      CSM_MALLOC(sizeof(Diligent::IBuffer**) * s_bufferSetNum));
  _constantBuffers = static_cast<Diligent::IBuffer***>(
      CSM_MALLOC(sizeof(Diligent::IBuffer**) * s_bufferSetNum));

  const csmInt32 drawableCount = GetModel()->GetDrawableCount();
  _drawableNum = drawableCount;

  for (csmUint32 buffer = 0; buffer < s_bufferSetNum; buffer++) {
    _vertexBuffers[buffer] = static_cast<Diligent::IBuffer**>(
        CSM_MALLOC(sizeof(Diligent::IBuffer*) * drawableCount));
    _indexBuffers[buffer] = static_cast<Diligent::IBuffer**>(
        CSM_MALLOC(sizeof(Diligent::IBuffer*) * drawableCount));
    _constantBuffers[buffer] = static_cast<Diligent::IBuffer**>(
        CSM_MALLOC(sizeof(Diligent::IBuffer*) * drawableCount));

    memset(_vertexBuffers[buffer], 0,
           sizeof(Diligent::IBuffer*) * drawableCount);
    memset(_indexBuffers[buffer], 0,
           sizeof(Diligent::IBuffer*) * drawableCount);
    memset(_constantBuffers[buffer], 0,
           sizeof(Diligent::IBuffer*) * drawableCount);

    for (csmUint32 drawAssign = 0; drawAssign < drawableCount; drawAssign++) {
      const csmInt32 vcount = GetModel()->GetDrawableVertexCount(drawAssign);
      if (vcount != 0) {
        Diligent::BufferDesc bufferDesc;
        bufferDesc.Size = sizeof(CubismVertexDiligent) * vcount;
        bufferDesc.Usage = Diligent::USAGE_DYNAMIC;
        bufferDesc.BindFlags = Diligent::BIND_VERTEX_BUFFER;
        bufferDesc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;

        (*s_device)->CreateBuffer(bufferDesc, nullptr,
                                  &_vertexBuffers[buffer][drawAssign]);
        if (!_vertexBuffers[buffer][drawAssign])
          CubismLogError("Vertexbuffer create failed : %d", vcount);
      } else {
        _vertexBuffers[buffer][drawAssign] = nullptr;
      }

      _indexBuffers[buffer][drawAssign] = nullptr;
      const csmInt32 icount =
          GetModel()->GetDrawableVertexIndexCount(drawAssign);
      if (icount != 0) {
        Diligent::BufferDesc bufferDesc;
        bufferDesc.Size = sizeof(uint16_t) * icount;
        bufferDesc.Usage = Diligent::USAGE_DEFAULT;
        bufferDesc.BindFlags = Diligent::BIND_INDEX_BUFFER;
        bufferDesc.CPUAccessFlags = Diligent::CPU_ACCESS_NONE;

        Diligent::BufferData subResourceData;
        subResourceData.pData =
            GetModel()->GetDrawableVertexIndices(drawAssign);
        subResourceData.DataSize = bufferDesc.Size;

        (*s_device)->CreateBuffer(bufferDesc, &subResourceData,
                                  &_indexBuffers[buffer][drawAssign]);
        if (!_indexBuffers[buffer][drawAssign])
          CubismLogError("Indexbuffer create failed : %d", icount);
      }

      _constantBuffers[buffer][drawAssign] = nullptr;
      {
        Diligent::BufferDesc bufferDesc;
        bufferDesc.Size = sizeof(CubismConstantBufferDiligent);
        bufferDesc.Usage = Diligent::USAGE_DEFAULT;
        bufferDesc.BindFlags = Diligent::BIND_UNIFORM_BUFFER;
        bufferDesc.CPUAccessFlags = Diligent::CPU_ACCESS_NONE;

        (*s_device)->CreateBuffer(bufferDesc, nullptr,
                                  &_constantBuffers[buffer][drawAssign]);
        if (!_constantBuffers[buffer][drawAssign])
          CubismLogError("ConstantBuffers create failed");
      }
    }
  }

  _shaderBindings.Resize(s_bufferSetNum);
  for (csmUint32 buffer = 0; buffer < s_bufferSetNum; buffer++) {
    _shaderBindings[buffer].Resize(drawableCount);
    for (csmInt32 drawAssign = 0; drawAssign < drawableCount; drawAssign++)
      _shaderBindings[buffer][drawAssign].InitializeBinding(
          GetPipelineManager()->GetSignature());
  }

  _commandBufferNum = s_bufferSetNum;
  _commandBufferCurrent = 0;
}

void CubismRenderer_Diligent::PostDraw() {
  // ダブル・トリプルバッファを回す
  _commandBufferCurrent++;
  if (_commandBufferNum <= _commandBufferCurrent) {
    _commandBufferCurrent = 0;
  }
}

void CubismRenderer_Diligent::DoDrawModel() {
  // nullptrは許されず
  CSM_ASSERT(s_device != nullptr);
  CSM_ASSERT(s_context != nullptr);

  //------------ クリッピングマスク・バッファ前処理方式の場合 ------------
  if (_clippingManager != nullptr) {
    // サイズが違う場合はここで作成しなおし
    for (csmInt32 i = 0; i < _clippingManager->GetRenderTextureCount(); ++i) {
      if (_offscreenSurfaces[_commandBufferCurrent][i].GetBufferWidth() !=
              static_cast<csmUint32>(
                  _clippingManager->GetClippingMaskBufferSize().X) ||
          _offscreenSurfaces[_commandBufferCurrent][i].GetBufferHeight() !=
              static_cast<csmUint32>(
                  _clippingManager->GetClippingMaskBufferSize().Y)) {
        _offscreenSurfaces[_commandBufferCurrent][i].CreateOffscreenSurface(
            s_device,
            static_cast<csmUint32>(
                _clippingManager->GetClippingMaskBufferSize().X),
            static_cast<csmUint32>(
                _clippingManager->GetClippingMaskBufferSize().Y));
      }
    }

    if (IsUsingHighPrecisionMask()) {
      _clippingManager->SetupMatrixForHighPrecision(*GetModel(), true);
    } else {
      _clippingManager->SetupClippingContext(s_device, s_context, *GetModel(),
                                             this, _commandBufferCurrent);
    }
  }

  const csmInt32 drawableCount = GetModel()->GetDrawableCount();
  const csmInt32* renderOrder = GetModel()->GetDrawableRenderOrders();

  // インデックスを描画順でソート
  for (csmInt32 i = 0; i < drawableCount; ++i) {
    const csmInt32 order = renderOrder[i];
    _sortedDrawableIndexList[order] = i;
  }

  // 描画
  for (csmInt32 i = 0; i < drawableCount; ++i) {
    const csmInt32 drawableIndex = _sortedDrawableIndexList[i];

    // Drawableが表示状態でなければ処理をパスする
    if (!GetModel()->GetDrawableDynamicFlagIsVisible(drawableIndex)) {
      continue;
    }

    // クリッピングマスクをセットする
    CubismClippingContext_Diligent* clipContext =
        (_clippingManager != nullptr)
            ? (*_clippingManager
                    ->GetClippingContextListForDraw())[drawableIndex]
            : nullptr;

    if (clipContext != nullptr && IsUsingHighPrecisionMask()) {
      if (clipContext->_isUsing) {
        // 正しいレンダーターゲットを持つオフスクリーンサーフェイスバッファを呼ぶ
        CubismOffscreenSurface_Diligent* currentHighPrecisionMaskColorBuffer =
            &_offscreenSurfaces[_commandBufferCurrent]
                               [clipContext->_bufferIndex];

        currentHighPrecisionMaskColorBuffer->BeginDraw(s_context);
        currentHighPrecisionMaskColorBuffer->Clear(s_context, 1.0f, 1.0f, 1.0f,
                                                   1.0f);

        const csmInt32 clipDrawCount = clipContext->_clippingIdCount;
        for (csmInt32 ctx = 0; ctx < clipDrawCount; ctx++) {
          const csmInt32 clipDrawIndex = clipContext->_clippingIdList[ctx];

          // 頂点情報が更新されておらず、信頼性がない場合は描画をパスする
          if (!GetModel()->GetDrawableDynamicFlagVertexPositionsDidChange(
                  clipDrawIndex)) {
            continue;
          }

          IsCulling(GetModel()->GetDrawableCulling(clipDrawIndex) != 0);

          // 今回専用の変換を適用して描く
          // チャンネルも切り替える必要がある(A,R,G,B)
          SetClippingContextBufferForMask(clipContext);
          DrawMeshDiligent(*GetModel(), clipDrawIndex);
        }

        {
          // --- 後処理 ---
          currentHighPrecisionMaskColorBuffer->EndDraw(s_context);
          SetClippingContextBufferForMask(nullptr);
        }
      }
    }

    // クリッピングマスクをセットする
    SetClippingContextBufferForDraw(clipContext);

    IsCulling(GetModel()->GetDrawableCulling(drawableIndex) != 0);

    DrawMeshDiligent(*GetModel(), drawableIndex);
  }

  // ダブルバッファ・トリプルバッファを回す
  PostDraw();
}

void CubismRenderer_Diligent::ExecuteDrawForMask(const CubismModel& model,
                                                 const csmInt32 index) {
  Diligent::IBuffer* constantBuffer = nullptr;
  Diligent::ITextureView *textureView = nullptr, *maskView = nullptr;

  // 定数バッファ
  {
    CubismConstantBufferDiligent cb;
    std::memset(&cb, 0, sizeof(cb));

    // 使用するカラーチャンネルを設定
    CubismClippingContext_Diligent* contextBuffer =
        GetClippingContextBufferForMask();
    SetColorChannel(cb, contextBuffer);

    // 色
    csmRectF* rect = GetClippingContextBufferForMask()->_layoutBounds;
    CubismTextureColor baseColor = {
        rect->X * 2.0f - 1.0f, rect->Y * 2.0f - 1.0f,
        rect->GetRight() * 2.0f - 1.0f, rect->GetBottom() * 2.0f - 1.0f};
    CubismTextureColor multiplyColor = model.GetMultiplyColor(index);
    CubismTextureColor screenColor = model.GetScreenColor(index);
    SetColorConstantBuffer(cb, model, index, baseColor, multiplyColor,
                           screenColor);

    // プロジェクションMtx
    SetProjectionMatrix(cb, GetClippingContextBufferForMask()->_matrixForMask);

    // Update
    constantBuffer = UpdateConstantBuffer(cb, index);
  }

  // 使用シェーダエフェクト取得
  CubismPipeline_Diligent* shaderManager = Live2D::Cubism::Framework::
      Rendering::CubismRenderer_Diligent::GetPipelineManager();
  if (!shaderManager) {
    return;
  }

  // シェーダーセット
  Diligent::IPipelineState* derivePipelineState =
      shaderManager->GetPipeline(ShaderNames_SetupMask, ShaderNames_SetupMask,
                                 Blend_Mask, IsCulling(), s_enableDepthTest);
  (*s_context)->SetPipelineState(derivePipelineState);

  // テクスチャ+サンプラーセット
  SetTextureView(model, index, &textureView, &maskView);

  // Setup shader resources
  auto& shaderBinding = _shaderBindings[_commandBufferCurrent][index];
  shaderBinding.SetConstantBuffer(constantBuffer);
  shaderBinding.SetMainTexture(textureView);
  shaderBinding.SetMaskTexture(maskView);
  (*s_context)
      ->CommitShaderResources(
          shaderBinding.GetBinding(),
          Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // 描画
  DrawDrawableIndexed(model, index);
}

void CubismRenderer_Diligent::ExecuteDrawForDraw(const CubismModel& model,
                                                 const csmInt32 index) {
  Diligent::IBuffer* constantBuffer = nullptr;
  Diligent::ITextureView *textureView = nullptr, *maskView = nullptr;

  // 定数バッファ
  {
    CubismConstantBufferDiligent cb;
    std::memset(&cb, 0, sizeof(cb));

    const csmBool masked = GetClippingContextBufferForDraw() != nullptr;
    if (masked) {
      // View座標をClippingContextの座標に変換するための行列を設定
      ConvertToNative(GetClippingContextBufferForDraw()->_matrixForDraw,
                      cb.clipMatrix);

      // 使用するカラーチャンネルを設定
      CubismClippingContext_Diligent* contextBuffer =
          GetClippingContextBufferForDraw();
      SetColorChannel(cb, contextBuffer);
    }

    // 色
    CubismTextureColor baseColor =
        GetModelColorWithOpacity(model.GetDrawableOpacity(index));
    CubismTextureColor multiplyColor = model.GetMultiplyColor(index);
    CubismTextureColor screenColor = model.GetScreenColor(index);
    SetColorConstantBuffer(cb, model, index, baseColor, multiplyColor,
                           screenColor);

    // プロジェクションMtx
    SetProjectionMatrix(cb, GetMvpMatrix());

    // Update
    constantBuffer = UpdateConstantBuffer(cb, index);
  }

  // 使用シェーダエフェクト取得
  CubismPipeline_Diligent* shaderManager = Live2D::Cubism::Framework::
      Rendering::CubismRenderer_Diligent::GetPipelineManager();
  if (!shaderManager) {
    return;
  }

  // シェーダーセット
  Diligent::IPipelineState* pipelineState = DerivePipeline(model, index);
  (*s_context)->SetPipelineState(pipelineState);

  // テクスチャ+サンプラーセット
  SetTextureView(model, index, &textureView, &maskView);

  // Setup shader resources
  auto& shaderBinding = _shaderBindings[_commandBufferCurrent][index];
  shaderBinding.SetConstantBuffer(constantBuffer);
  shaderBinding.SetMainTexture(textureView);
  shaderBinding.SetMaskTexture(maskView);
  (*s_context)
      ->CommitShaderResources(
          shaderBinding.GetBinding(),
          Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // 描画
  DrawDrawableIndexed(model, index);
}

void CubismRenderer_Diligent::DrawDrawableIndexed(const CubismModel& model,
                                                  const csmInt32 index) {
  Diligent::IBuffer* vertexBuffer =
      _vertexBuffers[_commandBufferCurrent][index];
  Diligent::IBuffer* indexBuffer = _indexBuffers[_commandBufferCurrent][index];
  const csmInt32 indexCount = model.GetDrawableVertexIndexCount(index);

  (*s_context)
      ->SetVertexBuffers(0, 1, &vertexBuffer, nullptr,
                         Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  (*s_context)
      ->SetIndexBuffer(indexBuffer, 0,
                       Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  Diligent::DrawIndexedAttribs drawAttribs;
  drawAttribs.NumIndices = indexCount;
  drawAttribs.IndexType = Diligent::VT_UINT16;
  (*s_context)->DrawIndexed(drawAttribs);
}

void CubismRenderer_Diligent::DrawMeshDiligent(const CubismModel& model,
                                               const csmInt32 index) {
  // デバイス未設定
  if (s_device == nullptr) {
    return;
  }

  // 描画物無し
  if (model.GetDrawableVertexIndexCount(index) == 0) {
    return;
  }

  // 描画不要なら描画処理をスキップする
  if (model.GetDrawableOpacity(index) <= 0.0f && !IsGeneratingMask()) {
    return;
  }

  // モデルが参照するテクスチャがバインドされていない場合は描画をスキップする
  if (GetTextureViewWithIndex(model, index) == nullptr) {
    return;
  }

  // 頂点バッファにコピー
  {
    const csmInt32 drawableIndex = index;
    const csmInt32 vertexCount = model.GetDrawableVertexCount(index);
    const csmFloat32* vertexArray = model.GetDrawableVertices(index);
    const csmFloat32* uvArray =
        reinterpret_cast<const csmFloat32*>(model.GetDrawableVertexUvs(index));
    CopyToBuffer(s_context, drawableIndex, vertexCount, vertexArray, uvArray);
  }

  // シェーダーセット・描画
  if (IsGeneratingMask()) {
    ExecuteDrawForMask(model, index);
  } else {
    ExecuteDrawForDraw(model, index);
  }

  SetClippingContextBufferForDraw(nullptr);
  SetClippingContextBufferForMask(nullptr);
}

void CubismRenderer_Diligent::SaveProfile() {
  // nullptrは許されず
  CSM_ASSERT(s_device != nullptr);
  CSM_ASSERT(s_context != nullptr);
}

void CubismRenderer_Diligent::RestoreProfile() {
  // nullptrは許されず
  CSM_ASSERT(s_device != nullptr);
  CSM_ASSERT(s_context != nullptr);
}

void CubismRenderer_Diligent::BindTexture(csmUint32 modelTextureAssign,
                                          Diligent::ITextureView* textureView) {
  _textures[modelTextureAssign] = textureView;
}

const csmMap<csmInt32, RRefPtr<Diligent::ITextureView>>&
CubismRenderer_Diligent::GetBindedTextures() const {
  return _textures;
}

void CubismRenderer_Diligent::SetClippingMaskBufferSize(csmFloat32 width,
                                                        csmFloat32 height) {
  if (_clippingManager == nullptr) {
    return;
  }

  // インスタンス破棄前にレンダーテクスチャの数を保存
  const csmInt32 renderTextureCount = _clippingManager->GetRenderTextureCount();

  // OffscreenSurfaceのサイズを変更するためにインスタンスを破棄・再作成する
  CSM_DELETE_SELF(CubismClippingManager_Diligent, _clippingManager);

  _clippingManager = CSM_NEW CubismClippingManager_Diligent();

  _clippingManager->SetClippingMaskBufferSize(width, height);

  _clippingManager->Initialize(*GetModel(), renderTextureCount);
}

csmInt32 CubismRenderer_Diligent::GetRenderTextureCount() const {
  return _clippingManager->GetRenderTextureCount();
}

CubismVector2 CubismRenderer_Diligent::GetClippingMaskBufferSize() const {
  return _clippingManager->GetClippingMaskBufferSize();
}

CubismOffscreenSurface_Diligent* CubismRenderer_Diligent::GetMaskBuffer(
    csmUint32 backbufferNum,
    csmInt32 offscreenIndex) {
  return &_offscreenSurfaces[backbufferNum][offscreenIndex];
}

void CubismRenderer_Diligent::InitializeConstantSettings(
    csmUint32 bufferSetNum,
    renderer::RenderDevice* device) {
  s_bufferSetNum = bufferSetNum;
  s_device = device;

  GetPipelineManager();
}

void CubismRenderer_Diligent::StartFrame(renderer::RenderDevice* device,
                                         renderer::RenderContext* renderContext,
                                         csmUint32 viewportWidth,
                                         csmUint32 viewportHeight,
                                         csmBool enableDepth) {
  // フレームで使用するデバイス設定
  s_device = device;
  s_context = renderContext;
  s_viewportWidth = viewportWidth;
  s_viewportHeight = viewportHeight;
  s_enableDepthTest = enableDepth;
}

void CubismRenderer_Diligent::EndFrame(renderer::RenderDevice* device) {}

void CubismRenderer_Diligent::SetClippingContextBufferForDraw(
    CubismClippingContext_Diligent* clip) {
  _clippingContextBufferForDraw = clip;
}

CubismClippingContext_Diligent*
CubismRenderer_Diligent::GetClippingContextBufferForDraw() const {
  return _clippingContextBufferForDraw;
}

void CubismRenderer_Diligent::SetClippingContextBufferForMask(
    CubismClippingContext_Diligent* clip) {
  _clippingContextBufferForMask = clip;
}

CubismClippingContext_Diligent*
CubismRenderer_Diligent::GetClippingContextBufferForMask() const {
  return _clippingContextBufferForMask;
}

void CubismRenderer_Diligent::CopyToBuffer(
    renderer::RenderContext* renderContext,
    csmInt32 drawAssign,
    const csmInt32 vcount,
    const csmFloat32* varray,
    const csmFloat32* uvarray) {
  // CubismVertexD3D11の書き込み
  if (_vertexBuffers[_commandBufferCurrent][drawAssign]) {
    void* mappingPointer = nullptr;
    (*renderContext)
        ->MapBuffer(_vertexBuffers[_commandBufferCurrent][drawAssign],
                    Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD,
                    mappingPointer);
    if (mappingPointer) {
      CubismVertexDiligent* lockPointer =
          reinterpret_cast<CubismVertexDiligent*>(mappingPointer);
      for (csmInt32 ct = 0; ct < vcount * 2;
           ct += 2) {  // モデルデータからのコピー
        lockPointer[ct / 2].x = varray[ct + 0];
        lockPointer[ct / 2].y = varray[ct + 1];
        lockPointer[ct / 2].u = uvarray[ct + 0];
        lockPointer[ct / 2].v = uvarray[ct + 1];
      }

      (*renderContext)
          ->UnmapBuffer(_vertexBuffers[_commandBufferCurrent][drawAssign],
                        Diligent::MAP_WRITE);
    }
  }
}

Diligent::ITextureView* CubismRenderer_Diligent::GetTextureViewWithIndex(
    const CubismModel& model,
    const csmInt32 index) {
  Diligent::ITextureView* result = nullptr;
  const csmInt32 textureIndex = model.GetDrawableTextureIndex(index);
  if (textureIndex >= 0) {
    result = _textures[textureIndex];
  }
  return result;
}

Diligent::IPipelineState* CubismRenderer_Diligent::DerivePipeline(
    const CubismModel& model,
    const csmInt32 index) {
  const csmBool masked = GetClippingContextBufferForDraw() != nullptr;
  const csmBool premult = IsPremultipliedAlpha();
  const csmBool invertedMask = model.GetDrawableInvertedMask(index);

  const ShaderNames vertexShaderNames =
      (masked ? ShaderNames_NormalMasked : ShaderNames_Normal);
  ShaderNames pixelShaderNames;
  if (masked) {
    if (premult) {
      if (invertedMask) {
        pixelShaderNames = ShaderNames_NormalMaskedInvertedPremultipliedAlpha;
      } else {
        pixelShaderNames = ShaderNames_NormalMaskedPremultipliedAlpha;
      }
    } else {
      if (invertedMask) {
        pixelShaderNames = ShaderNames_NormalMaskedInverted;
      } else {
        pixelShaderNames = ShaderNames_NormalMasked;
      }
    }
  } else {
    if (premult) {
      pixelShaderNames = ShaderNames_NormalPremultipliedAlpha;
    } else {
      pixelShaderNames = ShaderNames_Normal;
    }
  }

  CubismPipeline_Diligent* shaderManager = Live2D::Cubism::Framework::
      Rendering::CubismRenderer_Diligent::GetPipelineManager();
  CubismBlendMode colorBlendMode = model.GetDrawableBlendMode(index);

  return shaderManager->GetPipeline(vertexShaderNames, pixelShaderNames,
                                    colorBlendMode, IsCulling(),
                                    s_enableDepthTest);
}

void CubismRenderer_Diligent::SetTextureView(
    const CubismModel& model,
    const csmInt32 index,
    Diligent::ITextureView** textureView,
    Diligent::ITextureView** maskView) {
  const csmBool masked = GetClippingContextBufferForDraw() != nullptr;
  const csmBool drawing = !IsGeneratingMask();

  *textureView = GetTextureViewWithIndex(model, index);
  *maskView =
      (masked && drawing
           ? _offscreenSurfaces[_commandBufferCurrent]
                               [GetClippingContextBufferForDraw()->_bufferIndex]
                                   .GetTextureView()
           : nullptr);
}

void CubismRenderer_Diligent::SetColorConstantBuffer(
    CubismConstantBufferDiligent& cb,
    const CubismModel& model,
    const csmInt32 index,
    CubismTextureColor& baseColor,
    CubismTextureColor& multiplyColor,
    CubismTextureColor& screenColor) {
  const float baseColorNormal[] = {baseColor.R, baseColor.G, baseColor.B,
                                   baseColor.A};
  const float multiplyColorNormal[] = {multiplyColor.R, multiplyColor.G,
                                       multiplyColor.B, multiplyColor.A};
  const float screenColorNormal[] = {screenColor.R, screenColor.G,
                                     screenColor.B, screenColor.A};

  std::memcpy(&cb.baseColor, baseColorNormal, sizeof(baseColorNormal));
  std::memcpy(&cb.multiplyColor, multiplyColorNormal,
              sizeof(multiplyColorNormal));
  std::memcpy(&cb.screenColor, screenColorNormal, sizeof(screenColorNormal));
}

void CubismRenderer_Diligent::SetColorChannel(
    CubismConstantBufferDiligent& cb,
    CubismClippingContext_Diligent* contextBuffer) {
  const csmInt32 channelIndex = contextBuffer->_layoutChannelIndex;
  CubismRenderer::CubismTextureColor* colorChannel =
      contextBuffer->GetClippingManager()->GetChannelFlagAsColor(channelIndex);
  const float srcColor[] = {colorChannel->R, colorChannel->G, colorChannel->B,
                            colorChannel->A};
  std::memcpy(cb.channelFlag, srcColor, sizeof(srcColor));
}

void CubismRenderer_Diligent::SetProjectionMatrix(
    CubismConstantBufferDiligent& cb,
    CubismMatrix44 matrix) {
  ConvertToNative(matrix, cb.projectMatrix);
}

Diligent::IBuffer* CubismRenderer_Diligent::UpdateConstantBuffer(
    CubismConstantBufferDiligent& cb,
    csmInt32 index) {
  Diligent::IBuffer* constantBuffer =
      _constantBuffers[_commandBufferCurrent][index];
  (*s_context)
      ->UpdateBuffer(constantBuffer, 0, sizeof(cb), &cb,
                     Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  return constantBuffer;
}

const csmBool inline CubismRenderer_Diligent::IsGeneratingMask() const {
  return (GetClippingContextBufferForMask() != nullptr);
}

}  // namespace Rendering
}  // namespace Framework
}  // namespace Cubism
}  // namespace Live2D

//------------ LIVE2D NAMESPACE ------------
