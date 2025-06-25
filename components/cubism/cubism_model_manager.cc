/**
 * Copyright(c) Live2D Inc. All rights reserved.
 * Portions copyright(c) Admenri Adev.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at
 * https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#include "components/cubism/cubism_model_manager.h"

#include "CubismDefaultParameterId.hpp"
#include "CubismModelSettingJson.hpp"
#include "Id/CubismIdManager.hpp"
#include "Motion/CubismMotion.hpp"
#include "Type/CubismBasicType.hpp"
#include "Utils/CubismString.hpp"

#include "SDL3/SDL_timer.h"
#include "SDL3_image/SDL_image.h"

#include "renderer/utils/texture_utils.h"

namespace cubism_render {

using namespace Live2D::Cubism::Framework;
using namespace Live2D::Cubism::Framework::DefaultParameterId;

const csmInt32 PriorityNone = 0;
const csmInt32 PriorityIdle = 1;
const csmInt32 PriorityNormal = 2;
const csmInt32 PriorityForce = 3;

const csmChar* MotionGroupIdle = "Idle";
const csmChar* MotionGroupTapBody = "TapBody";

const csmChar* HitAreaNameHead = "Head";
const csmChar* HitAreaNameBody = "Body";

CubismModel::CubismModel(filesystem::IOService* ioService,
                         renderer::RenderDevice* device)
    : Csm::CubismUserModel(),
      _ioService(ioService),
      _device(device),
      _lastTick(SDL_GetPerformanceCounter()),
      _modelSetting(nullptr),
      _userTimeSeconds(0.0f),
      _deleteModel(false) {
  _mocConsistency = true;
  _debugMode = true;

  _idParamAngleX = CubismFramework::GetIdManager()->GetId(ParamAngleX);
  _idParamAngleY = CubismFramework::GetIdManager()->GetId(ParamAngleY);
  _idParamAngleZ = CubismFramework::GetIdManager()->GetId(ParamAngleZ);
  _idParamBodyAngleX = CubismFramework::GetIdManager()->GetId(ParamBodyAngleX);
  _idParamEyeBallX = CubismFramework::GetIdManager()->GetId(ParamEyeBallX);
  _idParamEyeBallY = CubismFramework::GetIdManager()->GetId(ParamEyeBallY);
}

CubismModel::~CubismModel() {
  _renderBuffer.DestroyOffscreenSurface();

  ReleaseMotions();
  ReleaseExpressions();

  for (csmInt32 i = 0; i < _modelSetting->GetMotionGroupCount(); i++) {
    const csmChar* group = _modelSetting->GetMotionGroupName(i);
    ReleaseMotionGroup(group);
  }

  // テクスチャの開放
  _bindTextures.clear();

  delete _modelSetting;
}

bool CubismModel::LoadAssets(const csmChar* dir, const csmChar* fileName) {
  _modelHomeDir = dir;

  if (_debugMode)
    LOG(INFO) << "[Cubism] load model setting: " << fileName;

  csmSizeInt size;
  const csmString path = csmString(dir) + fileName;

  csmByte* buffer = CreateBuffer(path.GetRawString(), &size);
  ICubismModelSetting* setting = new CubismModelSettingJson(buffer, size);
  DeleteBuffer(buffer, path.GetRawString());

  SetupModel(setting);

  if (_model == nullptr) {
    LOG(ERROR) << "[Cubism] Failed to LoadAssets().";
    return false;
  }

  CreateRenderer();

  SetupTextures();

  return true;
}

void CubismModel::SetupModel(ICubismModelSetting* setting) {
  _updating = true;
  _initialized = false;

  _modelSetting = setting;

  csmByte* buffer;
  csmSizeInt size;

  // Cubism Model
  if (strcmp(_modelSetting->GetModelFileName(), "") != 0) {
    csmString path = _modelSetting->GetModelFileName();
    path = _modelHomeDir + path;

    if (_debugMode)
      LOG(INFO) << "[Cubism] create model: " << setting->GetModelFileName();

    buffer = CreateBuffer(path.GetRawString(), &size);
    LoadModel(buffer, size, _mocConsistency);
    DeleteBuffer(buffer, path.GetRawString());
  }

  // Expression
  if (_modelSetting->GetExpressionCount() > 0) {
    const csmInt32 count = _modelSetting->GetExpressionCount();
    for (csmInt32 i = 0; i < count; i++) {
      csmString name = _modelSetting->GetExpressionName(i);
      csmString path = _modelSetting->GetExpressionFileName(i);
      path = _modelHomeDir + path;

      buffer = CreateBuffer(path.GetRawString(), &size);
      ACubismMotion* motion = LoadExpression(buffer, size, name.GetRawString());

      if (motion) {
        if (_expressions[name] != nullptr) {
          ACubismMotion::Delete(_expressions[name]);
          _expressions[name] = nullptr;
        }
        _expressions[name] = motion;
      }

      DeleteBuffer(buffer, path.GetRawString());
    }
  }

  // Physics
  if (strcmp(_modelSetting->GetPhysicsFileName(), "") != 0) {
    csmString path = _modelSetting->GetPhysicsFileName();
    path = _modelHomeDir + path;

    buffer = CreateBuffer(path.GetRawString(), &size);
    LoadPhysics(buffer, size);
    DeleteBuffer(buffer, path.GetRawString());
  }

  // Pose
  if (strcmp(_modelSetting->GetPoseFileName(), "") != 0) {
    csmString path = _modelSetting->GetPoseFileName();
    path = _modelHomeDir + path;

    buffer = CreateBuffer(path.GetRawString(), &size);
    LoadPose(buffer, size);
    DeleteBuffer(buffer, path.GetRawString());
  }

  // EyeBlink
  if (_modelSetting->GetEyeBlinkParameterCount() > 0) {
    _eyeBlink = CubismEyeBlink::Create(_modelSetting);
  }

  // Breath
  {
    _breath = CubismBreath::Create();

    csmVector<CubismBreath::BreathParameterData> breathParameters;

    breathParameters.PushBack(CubismBreath::BreathParameterData(
        _idParamAngleX, 0.0f, 15.0f, 6.5345f, 0.5f));
    breathParameters.PushBack(CubismBreath::BreathParameterData(
        _idParamAngleY, 0.0f, 8.0f, 3.5345f, 0.5f));
    breathParameters.PushBack(CubismBreath::BreathParameterData(
        _idParamAngleZ, 0.0f, 10.0f, 5.5345f, 0.5f));
    breathParameters.PushBack(CubismBreath::BreathParameterData(
        _idParamBodyAngleX, 0.0f, 4.0f, 15.5345f, 0.5f));
    breathParameters.PushBack(CubismBreath::BreathParameterData(
        CubismFramework::GetIdManager()->GetId(ParamBreath), 0.5f, 0.5f,
        3.2345f, 0.5f));

    _breath->SetParameters(breathParameters);
  }

  // UserData
  if (strcmp(_modelSetting->GetUserDataFile(), "") != 0) {
    csmString path = _modelSetting->GetUserDataFile();
    path = _modelHomeDir + path;
    buffer = CreateBuffer(path.GetRawString(), &size);
    LoadUserData(buffer, size);
    DeleteBuffer(buffer, path.GetRawString());
  }

  // EyeBlinkIds
  {
    csmInt32 eyeBlinkIdCount = _modelSetting->GetEyeBlinkParameterCount();
    for (csmInt32 i = 0; i < eyeBlinkIdCount; ++i) {
      _eyeBlinkIds.PushBack(_modelSetting->GetEyeBlinkParameterId(i));
    }
  }

  // LipSyncIds
  {
    csmInt32 lipSyncIdCount = _modelSetting->GetLipSyncParameterCount();
    for (csmInt32 i = 0; i < lipSyncIdCount; ++i) {
      _lipSyncIds.PushBack(_modelSetting->GetLipSyncParameterId(i));
    }
  }

  if (_modelSetting == nullptr || _modelMatrix == nullptr) {
    LOG(ERROR) << "[Cubism] Failed to SetupModel().";
    return;
  }

  // Layout
  csmMap<csmString, csmFloat32> layout;
  _modelSetting->GetLayoutMap(layout);
  _modelMatrix->SetupFromLayout(layout);

  _model->SaveParameters();

  for (csmInt32 i = 0; i < _modelSetting->GetMotionGroupCount(); i++) {
    const csmChar* group = _modelSetting->GetMotionGroupName(i);
    PreloadMotionGroup(group);
  }

  _motionManager->StopAllMotions();

  _updating = false;
  _initialized = true;
}

void CubismModel::PreloadMotionGroup(const csmChar* group) {
  const csmInt32 count = _modelSetting->GetMotionCount(group);

  for (csmInt32 i = 0; i < count; i++) {
    // ex) idle_0
    csmString name = Utils::CubismString::GetFormatedString("%s_%d", group, i);
    csmString path = _modelSetting->GetMotionFileName(group, i);
    path = _modelHomeDir + path;

    if (_debugMode)
      LOG(INFO) << "[Cubism] load motion: " << path.GetRawString() << " => ["
                << group << "_" << i << "]";

    csmByte* buffer;
    csmSizeInt size;
    buffer = CreateBuffer(path.GetRawString(), &size);
    CubismMotion* tmpMotion = static_cast<CubismMotion*>(
        LoadMotion(buffer, size, name.GetRawString(), nullptr, nullptr,
                   _modelSetting, group, i));

    if (tmpMotion) {
      tmpMotion->SetEffectIds(_eyeBlinkIds, _lipSyncIds);

      if (_motions[name] != nullptr) {
        ACubismMotion::Delete(_motions[name]);
      }
      _motions[name] = tmpMotion;
    }

    DeleteBuffer(buffer, path.GetRawString());
  }
}

void CubismModel::ReleaseMotionGroup(const csmChar* group) const {
  const csmInt32 count = _modelSetting->GetMotionCount(group);
  for (csmInt32 i = 0; i < count; i++) {
    csmString voice = _modelSetting->GetMotionSoundFileName(group, i);
    if (strcmp(voice.GetRawString(), "") != 0) {
      csmString path = voice;
      path = _modelHomeDir + path;
    }
  }
}

void CubismModel::ReleaseMotions() {
  for (csmMap<csmString, ACubismMotion*>::const_iterator iter =
           _motions.Begin();
       iter != _motions.End(); ++iter) {
    ACubismMotion::Delete(iter->Second);
  }

  _motions.Clear();
}

void CubismModel::ReleaseExpressions() {
  for (csmMap<csmString, ACubismMotion*>::const_iterator iter =
           _expressions.Begin();
       iter != _expressions.End(); ++iter) {
    ACubismMotion::Delete(iter->Second);
  }

  _expressions.Clear();
}

void CubismModel::Update() {
  const csmFloat32 deltaTimeSeconds = GetDeltaTime();
  _userTimeSeconds += deltaTimeSeconds;

  _dragManager->Update(deltaTimeSeconds);
  _dragX = _dragManager->GetX();
  _dragY = _dragManager->GetY();

  // モーションによるパラメータ更新の有無
  csmBool motionUpdated = false;

  //-----------------------------------------------------------------
  _model->LoadParameters();  // 前回セーブされた状態をロード
  if (_motionManager->IsFinished()) {
    // モーションの再生がない場合、待機モーションの中からランダムで再生する
    StartRandomMotion(MotionGroupIdle, PriorityIdle);
  } else {
    motionUpdated = _motionManager->UpdateMotion(
        _model, deltaTimeSeconds);  // モーションを更新
  }
  _model->SaveParameters();  // 状態を保存
  //-----------------------------------------------------------------

  // 不透明度
  _opacity = _model->GetModelOpacity();

  // まばたき
  if (!motionUpdated) {
    if (_eyeBlink != nullptr) {
      // メインモーションの更新がないとき
      _eyeBlink->UpdateParameters(_model, deltaTimeSeconds);  // 目パチ
    }
  }

  if (_expressionManager != nullptr) {
    _expressionManager->UpdateMotion(
        _model, deltaTimeSeconds);  // 表情でパラメータ更新（相対変化）
  }

  // ドラッグによる変化
  // ドラッグによる顔の向きの調整
  _model->AddParameterValue(_idParamAngleX,
                            _dragX * 30);  // -30から30の値を加える
  _model->AddParameterValue(_idParamAngleY, _dragY * 30);
  _model->AddParameterValue(_idParamAngleZ, _dragX * _dragY * -30);

  // ドラッグによる体の向きの調整
  _model->AddParameterValue(_idParamBodyAngleX,
                            _dragX * 10);  // -10から10の値を加える

  // ドラッグによる目の向きの調整
  _model->AddParameterValue(_idParamEyeBallX, _dragX);  // -1から1の値を加える
  _model->AddParameterValue(_idParamEyeBallY, _dragY);

  // 呼吸など
  if (_breath != nullptr) {
    _breath->UpdateParameters(_model, deltaTimeSeconds);
  }

  // 物理演算の設定
  if (_physics != nullptr) {
    _physics->Evaluate(_model, deltaTimeSeconds);
  }

  //// リップシンクの設定
  // if (_lipSync) {
  //   //
  //   リアルタイムでリップシンクを行う場合、システムから音量を取得して0〜1の範囲で値を入力します。
  //   csmFloat32 value = 0.0f;

  //  // 状態更新/RMS値取得
  //  _wavFileHandler.Update(deltaTimeSeconds);
  //  value = _wavFileHandler.GetRms();

  //  for (csmUint32 i = 0; i < _lipSyncIds.GetSize(); ++i) {
  //    _model->AddParameterValue(_lipSyncIds[i], value, 0.8f);
  //  }
  //}

  // ポーズの設定
  if (_pose != nullptr) {
    _pose->UpdateParameters(_model, deltaTimeSeconds);
  }

  _model->Update();
}

CubismMotionQueueEntryHandle CubismModel::StartMotion(
    const csmChar* group,
    csmInt32 no,
    csmInt32 priority,
    ACubismMotion::FinishedMotionCallback onFinishedMotionHandler,
    ACubismMotion::BeganMotionCallback onBeganMotionHandler) {
  if (priority == PriorityForce) {
    _motionManager->SetReservePriority(priority);
  } else if (!_motionManager->ReserveMotion(priority)) {
    if (_debugMode)
      LOG(INFO) << "[Cubism] can't start motion.";

    return InvalidMotionQueueEntryHandleValue;
  }

  const csmString fileName = _modelSetting->GetMotionFileName(group, no);

  // ex) idle_0
  csmString name = Utils::CubismString::GetFormatedString("%s_%d", group, no);
  CubismMotion* motion =
      static_cast<CubismMotion*>(_motions[name.GetRawString()]);
  csmBool autoDelete = false;

  if (motion == nullptr) {
    csmString path = fileName;
    path = _modelHomeDir + path;

    csmByte* buffer;
    csmSizeInt size;
    buffer = CreateBuffer(path.GetRawString(), &size);
    motion = static_cast<CubismMotion*>(
        LoadMotion(buffer, size, nullptr, onFinishedMotionHandler,
                   onBeganMotionHandler, _modelSetting, group, no));

    if (motion) {
      motion->SetEffectIds(_eyeBlinkIds, _lipSyncIds);
      autoDelete = true;  // 終了時にメモリから削除
    }

    DeleteBuffer(buffer, path.GetRawString());
  } else {
    motion->SetBeganMotionHandler(onBeganMotionHandler);
    motion->SetFinishedMotionHandler(onFinishedMotionHandler);
  }

  //// voice
  // csmString voice = _modelSetting->GetMotionSoundFileName(group, no);
  // if (strcmp(voice.GetRawString(), "") != 0) {
  //   csmString path = voice;
  //   path = _modelHomeDir + path;
  //   _wavFileHandler.Start(path);
  // }

  if (_debugMode) {
    LOG(INFO) << "[Cubism] start motion: [" << group << "_" << no << "]";
  }
  return _motionManager->StartMotionPriority(motion, autoDelete, priority);
}

CubismMotionQueueEntryHandle CubismModel::StartRandomMotion(
    const csmChar* group,
    csmInt32 priority,
    ACubismMotion::FinishedMotionCallback onFinishedMotionHandler,
    ACubismMotion::BeganMotionCallback onBeganMotionHandler) {
  if (_modelSetting->GetMotionCount(group) == 0) {
    return InvalidMotionQueueEntryHandleValue;
  }

  csmInt32 no = rand() % _modelSetting->GetMotionCount(group);

  return StartMotion(group, no, priority, onFinishedMotionHandler,
                     onBeganMotionHandler);
}

void CubismModel::DoDraw() {
  GetRenderer<Rendering::CubismRenderer_Diligent>()->DrawModel();
}

Csm::csmByte* CubismModel::CreateBuffer(const Csm::csmChar* path,
                                        Csm::csmSizeInt* size) {
  filesystem::IOState state;
  SDL_IOStream* file_stream = _ioService->OpenReadRaw(path, &state);
  if (state.error_count) {
    LOG(ERROR) << "[Cubism] " << state.error_message;
    return nullptr;
  }

  int64_t file_size = file_stream ? SDL_GetIOSize(file_stream) : 0;
  Csm::csmByte* buffer = static_cast<Csm::csmByte*>(SDL_malloc(file_size));
  if (buffer)
    SDL_ReadIO(file_stream, buffer, file_size);
  *size = static_cast<uint32_t>(file_size);

  if (file_stream)
    SDL_CloseIO(file_stream);

  return buffer;
}

void CubismModel::DeleteBuffer(Csm::csmByte* buffer, const Csm::csmChar* path) {
  if (buffer)
    SDL_free(buffer);
}

Csm::csmFloat32 CubismModel::GetDeltaTime() {
  const uint64_t nowTick = SDL_GetPerformanceCounter();
  const uint64_t deltaTime = nowTick - _lastTick;
  _lastTick = nowTick;

  const double deltaSecond = static_cast<double>(deltaTime) /
                             static_cast<double>(SDL_GetPerformanceFrequency());

  return static_cast<float>(deltaSecond);
}

void CubismModel::Draw(Csm::CubismMatrix44& matrix) {
  Rendering::CubismRenderer_Diligent* renderer =
      GetRenderer<Rendering::CubismRenderer_Diligent>();

  if (_model == nullptr || _deleteModel || renderer == nullptr) {
    return;
  }

  // 投影行列と乗算
  matrix.MultiplyByMatrix(_modelMatrix);

  renderer->SetMvpMatrix(&matrix);

  DoDraw();
}

csmBool CubismModel::HitTest(const csmChar* hitAreaName,
                             csmFloat32 x,
                             csmFloat32 y) {
  // 透明時は当たり判定なし。
  if (_opacity < 1.0f) {
    return false;
  }
  const csmInt32 count = _modelSetting->GetHitAreasCount();
  for (csmInt32 i = 0; i < count; i++) {
    if (strcmp(_modelSetting->GetHitAreaName(i), hitAreaName) == 0) {
      const CubismIdHandle drawID = _modelSetting->GetHitAreaId(i);
      return IsHit(drawID, x, y);
    }
  }
  return false;  // 存在しない場合はfalse
}

void CubismModel::SetExpression(const csmChar* expressionID) {
  ACubismMotion* motion = _expressions[expressionID];
  if (_debugMode) {
    LOG(INFO) << "[Cubism] expression: [" << expressionID << "]";
  }

  if (motion != nullptr) {
    _expressionManager->StartMotion(motion, false);
  } else {
    if (_debugMode)
      LOG(INFO) << "[Cubism] expression[" << expressionID << "] is null ";
  }
}

void CubismModel::SetRandomExpression() {
  if (_expressions.GetSize() == 0) {
    return;
  }

  csmInt32 no = rand() % _expressions.GetSize();
  csmMap<csmString, ACubismMotion*>::const_iterator map_ite;
  csmInt32 i = 0;
  for (map_ite = _expressions.Begin(); map_ite != _expressions.End();
       map_ite++) {
    if (i == no) {
      csmString name = (*map_ite).First;
      SetExpression(name.GetRawString());
      return;
    }
    i++;
  }
}

void CubismModel::ReloadRenderer() {
  DeleteRenderer();

  CreateRenderer();

  SetupTextures();
}

void CubismModel::SetupTextures() {
  constexpr bool isPreMult = false;

  _bindTextures.clear();

  for (csmInt32 modelTextureNumber = 0;
       modelTextureNumber < _modelSetting->GetTextureCount();
       modelTextureNumber++) {
    // テクスチャ名が空文字だった場合はロード・バインド処理をスキップ
    if (strcmp(_modelSetting->GetTextureFileName(modelTextureNumber), "") ==
        0) {
      continue;
    }

    // テクスチャをロードする
    csmString texturePath =
        _modelSetting->GetTextureFileName(modelTextureNumber);
    texturePath = _modelHomeDir + texturePath;

    // Load texture from filesystem
    filesystem::IOState state;
    SDL_Surface* surf;

    _ioService->OpenRead(
        texturePath.GetRawString(),
        base::BindRepeating(
            [](SDL_Surface** surf, SDL_IOStream* io, const base::String& ext) {
              *surf = IMG_LoadTyped_IO(io, true, ext.c_str());
              return !!*surf;
            },
            &surf),
        &state);

    if (state.error_count) {
      LOG(ERROR) << "[Cubism] " << state.error_message;
      continue;
    }

    RRefPtr<Diligent::ITexture> renderTexture;
    renderer::CreateTexture2D(**_device, &renderTexture, "live2d.cubism", surf);

    if (renderTexture) {
      auto* textureView =
          renderTexture->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
      GetRenderer<Rendering::CubismRenderer_Diligent>()->BindTexture(
          modelTextureNumber, textureView);
      _bindTextures.push_back(std::move(renderTexture));
    }
  }

  // premultであるなら設定
  GetRenderer<Rendering::CubismRenderer_Diligent>()->IsPremultipliedAlpha(
      isPreMult);
}

void CubismModel::MotionEventFired(const csmString& eventValue) {
  CubismLogInfo("%s is fired on CubismModel!!", eventValue.GetRawString());
}

Csm::Rendering::CubismOffscreenSurface_Diligent&
CubismModel::GetRenderBuffer() {
  return _renderBuffer;
}

csmBool CubismModel::HasMocConsistencyFromFile(const csmChar* mocFileName) {
  CSM_ASSERT(strcmp(mocFileName, ""));

  csmByte* buffer;
  csmSizeInt size;

  csmString path = mocFileName;
  path = _modelHomeDir + path;

  buffer = CreateBuffer(path.GetRawString(), &size);

  csmBool consistency =
      CubismMoc::HasMocConsistencyFromUnrevivedMoc(buffer, size);
  if (!consistency) {
    CubismLogInfo("Inconsistent MOC3.");
  } else {
    CubismLogInfo("Consistent MOC3.");
  }

  DeleteBuffer(buffer);

  return consistency;
}

}  // namespace cubism_render
