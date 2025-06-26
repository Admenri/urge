/**
 * Copyright(c) Live2D Inc. All rights reserved.
 * Portions (c) Admenri Adev.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at
 * https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#pragma once

#include "CubismFramework.hpp"
#include "CubismOffscreenSurface_Diligent.h"
#include "CubismPipeline_Diligent.h"
#include "Math/CubismVector2.hpp"
#include "Rendering/CubismClippingManager.hpp"
#include "Rendering/CubismRenderer.hpp"
#include "Type/csmMap.hpp"
#include "Type/csmRectF.hpp"
#include "Type/csmVector.hpp"

#include "renderer/device/render_device.h"

//------------ LIVE2D NAMESPACE ------------
namespace Live2D {
namespace Cubism {
namespace Framework {
namespace Rendering {

//  前方宣言
class CubismRenderer_Diligent;
class CubismClippingContext_Diligent;

struct CubismVertexDiligent {
  float x, y;  // Position
  float u, v;  // UVs
};

struct CubismConstantBufferDiligent {
  float projectMatrix[16];
  float clipMatrix[16];
  float baseColor[4];
  float multiplyColor[4];
  float screenColor[4];
  float channelFlag[4];
};

/**
 * @brief  クリッピングマスクの処理を実行するクラス
 *
 */
class CubismClippingManager_Diligent
    : public CubismClippingManager<CubismClippingContext_Diligent,
                                   CubismOffscreenSurface_Diligent> {
 public:
  /**
   * @brief   クリッピングコンテキストを作成する。モデル描画時に実行する。
   *
   * @param[in]   model       ->  モデルのインスタンス
   * @param[in]   renderer    ->  レンダラのインスタンス
   */
  void SetupClippingContext(renderer::RenderDevice* device,
                            Diligent::IDeviceContext* renderContext,
                            CubismModel& model,
                            CubismRenderer_Diligent* renderer,
                            csmInt32 offscreenCurrent);
};

/**
 * @brief   クリッピングマスクのコンテキスト
 */
class CubismClippingContext_Diligent : public CubismClippingContext {
  friend class CubismClippingManager_Diligent;
  friend class CubismRenderer_Diligent;

 public:
  /**
   * @brief   引数付きコンストラクタ
   *
   */
  CubismClippingContext_Diligent(
      CubismClippingManager<CubismClippingContext_Diligent,
                            CubismOffscreenSurface_Diligent>* manager,
      CubismModel& model,
      const csmInt32* clippingDrawableIndices,
      csmInt32 clipCount);

  /**
   * @brief   デストラクタ
   */
  virtual ~CubismClippingContext_Diligent();

  /**
   * @brief   このマスクを管理するマネージャのインスタンスを取得する。
   *
   * @return  クリッピングマネージャのインスタンス
   */
  CubismClippingManager<CubismClippingContext_Diligent,
                        CubismOffscreenSurface_Diligent>*
  GetClippingManager();

  CubismClippingManager<CubismClippingContext_Diligent,
                        CubismOffscreenSurface_Diligent>*
      _owner;  ///< このマスクを管理しているマネージャのインスタンス
};

/**
 * @brief   DirectX11用の描画命令を実装したクラス
 *
 */
class CubismRenderer_Diligent : public CubismRenderer {
  friend class CubismRenderer;
  friend class CubismClippingManager_Diligent;

 public:
  /**
   * @brief    レンダラを作成するための各種設定
   *           モデルロードの前に一度だけ呼び出す
   *
   * @param[in]   bufferSetNum -> 描画コマンドバッファ数
   * @param[in]   device       -> 使用デバイス
   */
  static void InitializeConstantSettings(csmUint32 bufferSetNum,
                                         renderer::RenderDevice* device);

  /**
   * @brief  Cubism描画関連の先頭で行う処理。
   *          各フレームでのCubism処理前にこれを呼んでもらう
   *
   * @param[in]   device         -> 使用デバイス
   */
  static void StartFrame(Diligent::IDeviceContext* renderContext,
                         CubismOffscreenSurface_Diligent* renderTarget);

  /**
   * @brief  Cubism描画関連の終了時行う処理。
   *          各フレームでのCubism処理前にこれを呼んでもらう
   */
  static void EndFrame();

  /**
   * @brief   シェーダ管理機構の取得
   */
  static CubismPipeline_Diligent* GetPipelineManager();

  /**
   * @brief   シェーダ管理機構の削除
   */
  static void DeletePipelineManager();

  /**
   * @brief   レンダラの初期化処理を実行する<br>
   *           引数に渡したモデルからレンダラの初期化処理に必要な情報を取り出すことができる
   *
   * @param[in]  model -> モデルのインスタンス
   */
  virtual void Initialize(Framework::CubismModel* model);

  /**
   * @brief   レンダラの初期化処理を実行する<br>
   *           引数に渡したモデルからレンダラの初期化処理に必要な情報を取り出すことができる
   *
   * @param[in]  model -> モデルのインスタンス
   */
  virtual void Initialize(Framework::CubismModel* model,
                          csmInt32 maskBufferCount);

  /**
   * @brief   OpenGLテクスチャのバインド処理<br>
   *           CubismRendererにテクスチャを設定し、CubismRenderer中でその画像を参照するためのIndex値を戻り値とする
   *
   * @param[in]   modelTextureAssign  ->
   * セットするモデルテクスチャのアサイン番号
   * @param[in]   textureView            ->  描画に使用するテクスチャ
   *
   */
  void BindTexture(csmUint32 modelTextureAssign,
                   Diligent::ITextureView* textureView);

  /**
   * @brief   OpenGLにバインドされたテクスチャのリストを取得する
   *
   * @return  テクスチャのアドレスのリスト
   */
  const csmMap<csmInt32, RRefPtr<Diligent::ITextureView>>& GetBindedTextures()
      const;

  /**
   * @brief  クリッピングマスクバッファのサイズを設定する<br>
   *         マスク用のOffscreenSurfaceを破棄・再作成するため処理コストは高い。
   *
   * @param[in]  size -> クリッピングマスクバッファのサイズ
   *
   */
  void SetClippingMaskBufferSize(csmFloat32 width, csmFloat32 height);

  /**
   * @brief  レンダーテクスチャの枚数を取得する。
   *
   * @return  レンダーテクスチャの枚数
   *
   */
  csmInt32 GetRenderTextureCount() const;

  /**
   * @brief  クリッピングマスクバッファのサイズを取得する
   *
   * @return クリッピングマスクバッファのサイズ
   *
   */
  CubismVector2 GetClippingMaskBufferSize() const;

  /**
   * @brief  クリッピングマスクのバッファを取得する
   *
   * @return クリッピングマスクのバッファへの参照
   *
   */
  CubismOffscreenSurface_Diligent* GetMaskBuffer(csmUint32 backbufferNum,
                                                 csmInt32 offscreenIndex);

 protected:
  /**
   * @brief   コンストラクタ
   */
  CubismRenderer_Diligent();

  /**
   * @brief   デストラクタ
   */
  virtual ~CubismRenderer_Diligent();

  /**
   * @brief   モデルを描画する実際の処理
   *
   */
  virtual void DoDrawModel() override;

  /**
   * @brief   描画オブジェクト（アートメッシュ）を描画する。
   *
   * @param[in]   model       ->  描画対象のモデル
   * @param[in]   index       ->  描画対象のインデックス
   */
  void DrawMeshDiligent(const CubismModel& model, const csmInt32 index);

 private:
  /**
   * @brief
   * 描画用に使用するシェーダの設定・コンスタントバッファの設定などを行い、マスク描画を実行
   *
   * @param[in]   model       ->  描画対象のモデル
   * @param[in]   index       ->  描画対象のインデックス
   */
  void ExecuteDrawForMask(const CubismModel& model,
                          const csmInt32 index,
                          CubismConstantBufferDiligent& constantsData);

  /**
   * @brief
   * マスク作成用に使用するシェーダの設定・コンスタントバッファの設定などを行い、描画を実行
   *
   * @param[in]   model       ->  描画対象のモデル
   * @param[in]   index       ->  描画対象のインデックス
   */
  void ExecuteDrawForDraw(const CubismModel& model,
                          const csmInt32 index,
                          CubismConstantBufferDiligent& constantsData);

  /**
   * @brief  指定されたメッシュインデックスに対して描画命令を実行する
   *
   * @param[in]   model       ->  描画対象のモデル
   * @param[in]   index       ->  描画対象のインデックス
   */
  void DrawDrawableIndexed(const CubismModel& model, const csmInt32 index);

  /**
   * @brief   レンダラが保持する静的なリソースを解放する
   */
  static void DoStaticRelease();

  // Prevention of copy Constructor
  CubismRenderer_Diligent(const CubismRenderer_Diligent&) = delete;
  CubismRenderer_Diligent& operator=(const CubismRenderer_Diligent&) = delete;

  /**
   * @brief   描画完了後の追加処理。<br>
   *           ダブル・トリプルバッファリングに必要な処理を実装している。
   */
  void PostDraw();

  /**
   * @brief   モデル描画直前のステートを保持する
   */
  virtual void SaveProfile() override;

  /**
   * @brief   モデル描画直前のステートを保持する
   */
  virtual void RestoreProfile() override;

  /**
   * @brief   マスクテクスチャに描画するクリッピングコンテキストをセットする。
   */
  void SetClippingContextBufferForMask(CubismClippingContext_Diligent* clip);

  /**
   * @brief   マスクテクスチャに描画するクリッピングコンテキストを取得する。
   *
   * @return  マスクテクスチャに描画するクリッピングコンテキスト
   */
  CubismClippingContext_Diligent* GetClippingContextBufferForMask() const;

  /**
   * @brief   画面上に描画するクリッピングコンテキストをセットする。
   */
  void SetClippingContextBufferForDraw(CubismClippingContext_Diligent* clip);

  /**
   * @brief   画面上に描画するクリッピングコンテキストを取得する。
   *
   * @return  画面上に描画するクリッピングコンテキスト
   */
  CubismClippingContext_Diligent* GetClippingContextBufferForDraw() const;

  /**
   * @brief   GetDrawableVertices,GetDrawableVertexUvsの内容をバッファへコピー
   */
  void CopyToBuffer(Diligent::IDeviceContext* renderContext,
                    csmInt32 drawAssign,
                    const csmInt32 vcount,
                    const csmFloat32* varray,
                    const csmFloat32* uvarray);

  /**
   * @brief   modelのindexにバインドされているテクスチャを返す。<br>
   *          もしバインドしていないindexを指定した場合NULLが返る。
   * @param[in]   model       ->  描画対象のモデル
   * @param[in]   index       ->  描画対象のインデックス
   */
  Diligent::ITextureView* GetTextureViewWithIndex(const CubismModel& model,
                                                  const csmInt32 index);

  /**
   * @brief   シェーダをコンテキストにセットする<br>
   * @param[in]   model       ->  描画対象のモデル
   * @param[in]   index       ->  描画対象のインデックス
   */
  Diligent::IPipelineState* DerivePipeline(const CubismBlendMode blendMode,
                                           const CubismModel& model,
                                           const csmInt32 index);

  /**
   * @brief  描画に使用するテクスチャを設定する。
   *
   * @param[in]   model       ->  描画対象のモデル
   * @param[in]   index       ->  描画対象のインデックス
   */
  void SetTextureView(const CubismModel& model,
                      const csmInt32 index,
                      CubismShaderBinding_Diligent* binding);

  /**
   * @brief  色関連の定数バッファを設定する
   *
   * @param[in]   cb             ->  設定する定数バッファ
   * @param[in]   model          ->  描画対象のモデル
   * @param[in]   index          ->  描画対象のインデックス
   * @param[in]   baseColor      ->  ベースカラー
   * @param[in]   multiplyColor  ->  乗算カラー
   * @param[in]   screenColor    ->  スクリーンカラー
   */
  void SetColorConstantBuffer(CubismConstantBufferDiligent& cb,
                              const CubismModel& model,
                              const csmInt32 index,
                              CubismTextureColor& baseColor,
                              CubismTextureColor& multiplyColor,
                              CubismTextureColor& screenColor);

  /**
   * @brief  描画に使用するカラーチャンネルを設定
   *
   * @param[in]   cb            ->  設定する定数バッファ
   * @param[in]   contextBuffer ->  描画コンテキスト
   */
  void SetColorChannel(CubismConstantBufferDiligent& cb,
                       CubismClippingContext_Diligent* contextBuffer);

  /**
   * @brief  定数バッファを更新する
   *
   * @param[in]   cb          ->  書き込む定数バッファ情報
   * @param[in]   index       ->  描画対象のインデックス
   */
  void UpdateConstantBuffer(CubismConstantBufferDiligent& cb,
                            csmInt32 index,
                            CubismShaderBinding_Diligent* binding);

  /**
   * @brief   マスク生成時かを判定する
   *
   * @return  判定値
   */
  const csmBool inline IsGeneratingMask() const;

  Diligent::IBuffer*** _vertexBuffers;    ///< 描画バッファ カラー無し、位置+UV
  Diligent::IBuffer*** _indexBuffers;     ///< インデックスのバッファ
  Diligent::IBuffer*** _constantBuffers;  ///< 定数のバッファ
  csmUint32 _drawableNum;  ///< _vertexBuffers, _indexBuffersの確保数

  csmVector<csmVector<CubismShaderBinding_Diligent>> _shaderBindings;

  csmInt32 _commandBufferNum;
  csmInt32 _commandBufferCurrent;

  csmVector<csmInt32>
      _sortedDrawableIndexList;  ///< 描画オブジェクトのインデックスを描画順に並べたリスト

  csmMap<csmInt32, RRefPtr<Diligent::ITextureView>>
      _textures;  ///< モデルが参照するテクスチャとレンダラでバインドしているテクスチャとのマップ

  csmVector<csmVector<CubismOffscreenSurface_Diligent>>
      _offscreenSurfaces;  ///< マスク描画用のフレームバッファ

  CubismClippingManager_Diligent*
      _clippingManager;  ///< クリッピングマスク管理オブジェクト
  CubismClippingContext_Diligent*
      _clippingContextBufferForMask;  ///< マスクテクスチャに描画するためのクリッピングコンテキスト
  CubismClippingContext_Diligent*
      _clippingContextBufferForDraw;  ///< 画面上描画するためのクリッピングコンテキスト
};

}  // namespace Rendering
}  // namespace Framework
}  // namespace Cubism
}  // namespace Live2D
//------------ LIVE2D NAMESPACE ------------
