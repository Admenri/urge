/**
 * Copyright(c) Live2D Inc. All rights reserved.
 * Portions (c) Admenri Adev.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at
 * https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#include "CubismRenderer_Diligent.h"

#include <cfloat>  // FLT_MAX,MIN

#include "CubismPipeline_Diligent.h"
#include "Math/CubismMatrix44.hpp"
#include "Model/CubismModel.hpp"
#include "Type/csmVector.hpp"

//------------ LIVE2D NAMESPACE ------------
namespace Live2D {
namespace Cubism {
namespace Framework {
namespace Rendering {

inline void ConvertToNative(CubismMatrix44& mtx, float* out) {
  memcpy(out, mtx.GetArray(), sizeof(float) * 16);
}

inline void Transpose4x4(float* matrix) {
  for (int i = 0; i < 4; ++i)
    for (int j = i + 1; j < 4; ++j)
      std::swap(matrix[i * 4 + j], matrix[j * 4 + i]);
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

  // マスク作成処理
  // ビューポートは退避済み
  // 生成したOffscreenSurfaceと同じサイズでビューポートを設定
  // CubismRenderer_Diligent::GetRenderStateManager()->SetViewport(
  //    renderContext, 0, 0, static_cast<FLOAT>(_clippingMaskBufferSize.X),
  //    static_cast<FLOAT>(_clippingMaskBufferSize.Y), 0.0f, 1.0f);

  // 後の計算のためにインデックスの最初をセット
  _currentMaskBuffer = renderer->GetMaskBuffer(offscreenCurrent, 0);

  // ----- マスク描画処理 -----
  // マスク用RenderTextureをactiveにセット
  _currentMaskBuffer->BeginDraw(renderContext);

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
      renderer->DrawMeshDX11(model, clipDrawIndex);
    }
  }

  // --- 後処理 ---
  _currentMaskBuffer->EndDraw(renderContext);
  renderer->SetClippingContextBufferForMask(NULL);
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

CubismPipeline_Diligent* s_pipelineManagerInstance = NULL;  ///< シェーダー管理

csmUint32 s_bufferSetNum =
    0;  ///< 作成コンテキストの数。モデルロード前に設定されている必要あり。
renderer::RenderDevice* s_device =
    NULL;  ///< 使用デバイス。モデルロード前に設定されている必要あり。
renderer::RenderContext* s_context = NULL;  ///< 使用描画コンテキスト
CubismOffscreenSurface_Diligent* s_renderTarget = NULL;

}  // namespace

CubismRenderer* CubismRenderer::Create() {
  return CSM_NEW CubismRenderer_Diligent();
}

void CubismRenderer::StaticRelease() {
  CubismRenderer_Diligent::DoStaticRelease();
}

CubismPipeline_Diligent* CubismRenderer_Diligent::GetPipelineManager() {
  if (s_pipelineManagerInstance == NULL) {
    s_pipelineManagerInstance = CSM_NEW CubismPipeline_Diligent(**s_device);
  }
  return s_pipelineManagerInstance;
}

void CubismRenderer_Diligent::DeletePipelineManager() {
  if (s_pipelineManagerInstance) {
    CSM_DELETE_SELF(CubismPipeline_Diligent, s_pipelineManagerInstance);
    s_pipelineManagerInstance = NULL;
  }
}

CubismRenderer_Diligent::CubismRenderer_Diligent()
    : _vertexBuffers(NULL),
      _indexBuffers(NULL),
      _constantBuffers(NULL),
      _drawableNum(0),
      _clippingManager(NULL),
      _clippingContextBufferForMask(NULL),
      _clippingContextBufferForDraw(NULL) {
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
        _constantBuffers[buffer][drawAssign] = NULL;
      }
      // インデックス
      if (_indexBuffers[buffer][drawAssign]) {
        _indexBuffers[buffer][drawAssign]->Release();
        _indexBuffers[buffer][drawAssign] = NULL;
      }
      // 頂点
      if (_vertexBuffers[buffer][drawAssign]) {
        _vertexBuffers[buffer][drawAssign]->Release();
        _vertexBuffers[buffer][drawAssign] = NULL;
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

void CubismRenderer_Diligent::DoStaticRelease() {
  // シェーダマネージャ削除
  DeletePipelineManager();
}

void CubismRenderer_Diligent::Initialize(CubismModel* model) {
  Initialize(model, 1);
}

void CubismRenderer_Diligent::Initialize(CubismModel* model,
                                         csmInt32 maskBufferCount) {
  // 0は許されず ここに来るまでに設定しなければならない
  if (s_device == 0) {
    CubismLogError("Device has not been set.");
    CSM_ASSERT(0);
    return;
  }

  // 1未満は1に補正する
  if (maskBufferCount < 1) {
    maskBufferCount = 1;
    CubismLogWarning(
        "The number of render textures must be an integer greater than or "
        "equal to 1. Set the number of render textures to 1.");
  }

  if (model->IsUsingMasking()) {
    _clippingManager = CSM_NEW
    CubismClippingManager_Diligent();  // クリッピングマスク・バッファ前処理方式を初期化
    _clippingManager->Initialize(*model, maskBufferCount);

    const csmInt32 bufferWidth =
        _clippingManager->GetClippingMaskBufferSize().X;
    const csmInt32 bufferHeight =
        _clippingManager->GetClippingMaskBufferSize().Y;

    _offscreenSurfaces.Clear();

    // バックバッファ分確保
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

  CubismRenderer::Initialize(model, maskBufferCount);  // 親クラスの処理を呼ぶ

  // コマンドバッファごとに確保
  // 頂点バッファをコンテキスト分
  _vertexBuffers = static_cast<Diligent::IBuffer***>(
      CSM_MALLOC(sizeof(Diligent::IBuffer**) * s_bufferSetNum));
  _indexBuffers = static_cast<Diligent::IBuffer***>(
      CSM_MALLOC(sizeof(Diligent::IBuffer**) * s_bufferSetNum));
  _constantBuffers = static_cast<Diligent::IBuffer***>(
      CSM_MALLOC(sizeof(Diligent::IBuffer**) * s_bufferSetNum));

  // モデルパーツごとに確保
  const csmInt32 drawableCount = GetModel()->GetDrawableCount();
  _drawableNum = drawableCount;

  for (csmUint32 buffer = 0; buffer < s_bufferSetNum; buffer++) {
    // 頂点バッファ
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
      // 頂点
      const csmInt32 vcount = GetModel()->GetDrawableVertexCount(drawAssign);
      if (vcount != 0) {
        Diligent::BufferDesc bufferDesc;
        bufferDesc.Size =
            sizeof(CubismVertexDiligent) * vcount;  // 総長 構造体サイズ*個数
        bufferDesc.Usage = Diligent::USAGE_DYNAMIC;
        bufferDesc.BindFlags = Diligent::BIND_VERTEX_BUFFER;
        bufferDesc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;

        // 後で頂点を入れるので領域だけ
        (*s_device)->CreateBuffer(bufferDesc, nullptr,
                                  &_vertexBuffers[buffer][drawAssign]);
        if (!_vertexBuffers[buffer][drawAssign]) {
          CubismLogError("Vertexbuffer create failed : %d", vcount);
        }
      } else {
        _vertexBuffers[buffer][drawAssign] = NULL;
      }

      // インデックスはここで要素コピーを済ませる
      _indexBuffers[buffer][drawAssign] = NULL;
      const csmInt32 icount =
          GetModel()->GetDrawableVertexIndexCount(drawAssign);
      if (icount != 0) {
        Diligent::BufferDesc bufferDesc;
        bufferDesc.Size = sizeof(WORD) * icount;  // 総長 構造体サイズ*個数
        bufferDesc.Usage = Diligent::USAGE_DEFAULT;
        bufferDesc.BindFlags = Diligent::BIND_INDEX_BUFFER;
        bufferDesc.CPUAccessFlags = Diligent::CPU_ACCESS_NONE;

        Diligent::BufferData subResourceData;
        subResourceData.pData =
            GetModel()->GetDrawableVertexIndices(drawAssign);
        subResourceData.DataSize = bufferDesc.Size;

        (*s_device)->CreateBuffer(bufferDesc, &subResourceData,
                                  &_indexBuffers[buffer][drawAssign]);
        if (!_indexBuffers[buffer][drawAssign]) {
          CubismLogError("Indexbuffer create failed : %d", icount);
        }
      }

      _constantBuffers[buffer][drawAssign] = NULL;
      {
        Diligent::BufferDesc bufferDesc;
        bufferDesc.Size =
            sizeof(CubismConstantBufferDiligent);  // 総長 構造体サイズ*個数
        bufferDesc.Usage = Diligent::
            USAGE_DEFAULT;  // 定数バッファに関しては「Map用にDynamic」にしなくともよい
        bufferDesc.BindFlags = Diligent::BIND_UNIFORM_BUFFER;
        bufferDesc.CPUAccessFlags = Diligent::CPU_ACCESS_NONE;

        (*s_device)->CreateBuffer(bufferDesc, NULL,
                                  &_constantBuffers[buffer][drawAssign]);
        if (!_constantBuffers[buffer][drawAssign]) {
          CubismLogError("ConstantBuffers create failed");
        }
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
  // NULLは許されず
  CSM_ASSERT(s_device != NULL);
  CSM_ASSERT(s_context != NULL);

  //------------ クリッピングマスク・バッファ前処理方式の場合 ------------
  if (_clippingManager != NULL) {
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

    // if (!IsUsingHighPrecisionMask()) {
    //   // ビューポートを元に戻す
    //   GetRenderStateManager()->SetViewport(
    //       s_context, 0.0f, 0.0f, static_cast<float>(s_viewportWidth),
    //       static_cast<float>(s_viewportHeight), 0.0f, 1.0f);
    // }
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
        (_clippingManager != NULL)
            ? (*_clippingManager
                    ->GetClippingContextListForDraw())[drawableIndex]
            : NULL;

    if (clipContext != NULL &&
        IsUsingHighPrecisionMask())  // マスクを書く必要がある
    {
      if (clipContext->_isUsing)  // 書くことになっていた
      {
        // CubismRenderer_Diligent::GetRenderStateManager()->SetViewport(
        //     s_context, 0, 0,
        //     static_cast<FLOAT>(_clippingManager->GetClippingMaskBufferSize().X),
        //     static_cast<FLOAT>(_clippingManager->GetClippingMaskBufferSize().Y),
        //     0.0f, 1.0f);

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
          DrawMeshDX11(*GetModel(), clipDrawIndex);
        }

        {
          // --- 後処理 ---
          currentHighPrecisionMaskColorBuffer->EndDraw(s_context);
          SetClippingContextBufferForMask(NULL);

          //// ビューポートを元に戻す
          // GetRenderStateManager()->SetViewport(
          //     s_context, 0.0f, 0.0f, static_cast<float>(s_viewportWidth),
          //     static_cast<float>(s_viewportHeight), 0.0f, 1.0f);
        }
      }
    }

    // クリッピングマスクをセットする
    SetClippingContextBufferForDraw(clipContext);

    IsCulling(GetModel()->GetDrawableCulling(drawableIndex) != 0);

    s_renderTarget->BeginDraw(s_context);
    DrawMeshDX11(*GetModel(), drawableIndex);
    s_renderTarget->EndDraw(s_context);
  }

  // ダブルバッファ・トリプルバッファを回す
  PostDraw();
}

void CubismRenderer_Diligent::ExecuteDrawForMask(const CubismModel& model,
                                                 const csmInt32 index) {
  // 使用シェーダエフェクト取得
  CubismPipeline_Diligent* shaderManager =
      CubismRenderer_Diligent::GetPipelineManager();
  if (!shaderManager) {
    return;
  }

  // シェーダーセット
  auto* pipeline = shaderManager->GetPipeline(
      ShaderNames_SetupMask, ShaderNames_SetupMask, Blend_Mask, IsCulling());
  auto& binding = _shaderBindings[_commandBufferCurrent][index];
  (*s_context)->SetPipelineState(pipeline);

  // テクスチャ+サンプラーセット
  SetTextureView(model, index, &binding);

  // 定数バッファ
  {
    CubismConstantBufferDiligent cb;
    memset(&cb, 0, sizeof(cb));

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
    UpdateConstantBuffer(cb, index, &binding);
  }

  // 描画
  DrawDrawableIndexed(model, index);
}

void CubismRenderer_Diligent::ExecuteDrawForDraw(const CubismModel& model,
                                                 const csmInt32 index) {
  // 使用シェーダエフェクト取得
  CubismPipeline_Diligent* shaderManager =
      CubismRenderer_Diligent::GetPipelineManager();
  if (!shaderManager) {
    return;
  }

  // ブレンドステート
  CubismBlendMode colorBlendMode = model.GetDrawableBlendMode(index);

  // シェーダーセット
  auto* pipeline = DerivePipeline(colorBlendMode, model, index);
  auto& binding = _shaderBindings[_commandBufferCurrent][index];
  (*s_context)->SetPipelineState(pipeline);

  // テクスチャ+サンプラーセット
  SetTextureView(model, index, &binding);

  // 定数バッファ
  {
    CubismConstantBufferDiligent cb;
    memset(&cb, 0, sizeof(cb));

    const csmBool masked = GetClippingContextBufferForDraw() != NULL;
    if (masked) {
      // View座標をClippingContextの座標に変換するための行列を設定
      ConvertToNative(GetClippingContextBufferForDraw()->_matrixForDraw,
                      cb.clipMatrix);
      Transpose4x4(cb.clipMatrix);

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
    UpdateConstantBuffer(cb, index, &binding);
  }

  // 描画
  DrawDrawableIndexed(model, index);
}

void CubismRenderer_Diligent::DrawDrawableIndexed(const CubismModel& model,
                                                  const csmInt32 index) {
  auto& binding = _shaderBindings[_commandBufferCurrent][index];
  (*s_context)
      ->CommitShaderResources(
          binding.GetBinding(),
          Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  UINT strides = sizeof(CubismVertexDiligent);
  UINT offsets = 0;
  Diligent::IBuffer* vertexBuffer =
      _vertexBuffers[_commandBufferCurrent][index];
  Diligent::IBuffer* indexBuffer = _indexBuffers[_commandBufferCurrent][index];
  const csmInt32 indexCount = model.GetDrawableVertexIndexCount(index);

  (*s_context)
      ->SetVertexBuffers(0, 1, &vertexBuffer, NULL,
                         Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  (*s_context)
      ->SetIndexBuffer(indexBuffer, 0,
                       Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  Diligent::DrawIndexedAttribs drawAttribs;
  drawAttribs.NumIndices = indexCount;
  drawAttribs.IndexType = Diligent::VT_UINT16;
  (*s_context)->DrawIndexed(drawAttribs);
}

void CubismRenderer_Diligent::DrawMeshDX11(const CubismModel& model,
                                           const csmInt32 index) {
  // デバイス未設定
  if (s_device == NULL) {
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
  if (GetTextureViewWithIndex(model, index) == NULL) {
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

  SetClippingContextBufferForDraw(NULL);
  SetClippingContextBufferForMask(NULL);
}

void CubismRenderer_Diligent::SaveProfile() {}

void CubismRenderer_Diligent::RestoreProfile() {}

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
  if (_clippingManager == NULL) {
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

  // 実体を作成しておく
  CubismRenderer_Diligent::GetPipelineManager();
}

void CubismRenderer_Diligent::StartFrame(
    renderer::RenderContext* renderContext,
    CubismOffscreenSurface_Diligent* renderTarget) {
  // フレームで使用するデバイス設定
  s_context = renderContext;
  s_renderTarget = renderTarget;
}

void CubismRenderer_Diligent::EndFrame() {
  s_renderTarget = NULL;
}

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
    }

    (*renderContext)
        ->UnmapBuffer(_vertexBuffers[_commandBufferCurrent][drawAssign],
                      Diligent::MAP_WRITE);
  }
}

Diligent::ITextureView* CubismRenderer_Diligent::GetTextureViewWithIndex(
    const CubismModel& model,
    const csmInt32 index) {
  Diligent::ITextureView* result = NULL;
  const csmInt32 textureIndex = model.GetDrawableTextureIndex(index);
  if (textureIndex >= 0) {
    result = _textures[textureIndex];
  }
  return result;
}

Diligent::IPipelineState* CubismRenderer_Diligent::DerivePipeline(
    const CubismBlendMode blendMode,
    const CubismModel& model,
    const csmInt32 index) {
  const csmBool masked = GetClippingContextBufferForDraw() != NULL;
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

  csmInt32 pipelineBlend;
  switch (blendMode) {
    case CubismRenderer::CubismBlendMode::CubismBlendMode_Normal:
    default:
      pipelineBlend = Blend_Normal;
      break;
    case CubismRenderer::CubismBlendMode::CubismBlendMode_Additive:
      pipelineBlend = Blend_Add;
      break;
    case CubismRenderer::CubismBlendMode::CubismBlendMode_Multiplicative:
      pipelineBlend = Blend_Mult;
      break;
  }

  CubismPipeline_Diligent* shaderManager =
      CubismRenderer_Diligent::GetPipelineManager();
  return shaderManager->GetPipeline(vertexShaderNames, pixelShaderNames,
                                    pipelineBlend, IsCulling());
}

void CubismRenderer_Diligent::SetTextureView(
    const CubismModel& model,
    const csmInt32 index,
    CubismShaderBinding_Diligent* binding) {
  const csmBool masked = GetClippingContextBufferForDraw() != NULL;
  const csmBool drawing = !IsGeneratingMask();

  Diligent::ITextureView* textureView = GetTextureViewWithIndex(model, index);
  Diligent::ITextureView* maskView =
      (masked && drawing
           ? _offscreenSurfaces[_commandBufferCurrent]
                               [GetClippingContextBufferForDraw()->_bufferIndex]
                                   .GetTextureView()
           : NULL);

  binding->SetMainTexture(textureView);
  binding->SetMaskTexture(maskView);
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

  std::memcpy(cb.baseColor, baseColorNormal, sizeof(baseColorNormal));
  std::memcpy(cb.multiplyColor, multiplyColorNormal,
              sizeof(multiplyColorNormal));
  std::memcpy(cb.screenColor, screenColorNormal, sizeof(screenColorNormal));
}

void CubismRenderer_Diligent::SetColorChannel(
    CubismConstantBufferDiligent& cb,
    CubismClippingContext_Diligent* contextBuffer) {
  const csmInt32 channelIndex = contextBuffer->_layoutChannelIndex;
  CubismRenderer::CubismTextureColor* colorChannel =
      contextBuffer->GetClippingManager()->GetChannelFlagAsColor(channelIndex);

  const float sourceColor[] = {colorChannel->R, colorChannel->G,
                               colorChannel->B, colorChannel->A};
  memcpy(cb.channelFlag, sourceColor, sizeof(sourceColor));
}

void CubismRenderer_Diligent::SetProjectionMatrix(
    CubismConstantBufferDiligent& cb,
    CubismMatrix44 matrix) {
  ConvertToNative(matrix, cb.projectMatrix);
}

void CubismRenderer_Diligent::UpdateConstantBuffer(
    CubismConstantBufferDiligent& cb,
    csmInt32 index,
    CubismShaderBinding_Diligent* binding) {
  Diligent::IBuffer* constantBuffer =
      _constantBuffers[_commandBufferCurrent][index];
  (*s_context)
      ->UpdateBuffer(constantBuffer, 0, sizeof(cb), &cb,
                     Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  binding->SetConstantBuffer(constantBuffer);
}

const csmBool inline CubismRenderer_Diligent::IsGeneratingMask() const {
  return (GetClippingContextBufferForMask() != NULL);
}

}  // namespace Rendering
}  // namespace Framework
}  // namespace Cubism
}  // namespace Live2D

//------------ LIVE2D NAMESPACE ------------
