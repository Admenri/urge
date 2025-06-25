/**
 * Copyright(c) Live2D Inc. All rights reserved.
 * Portions copyright(c) Admenri Adev.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at
 * https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#ifndef COMPONENTS_CUBISM_CUBISM_MODEL_MANAGER_H_
#define COMPONENTS_CUBISM_CUBISM_MODEL_MANAGER_H_

#include "CubismFramework.hpp"
#include "Model/CubismUserModel.hpp"
#include "Rendering/Diligent/CubismOffscreenSurface_Diligent.h"
#include "Rendering/Diligent/CubismRenderer_Diligent.h"
#include "Type/CubismBasicType.hpp"

#include "components/filesystem/io_service.h"
#include "renderer/device/render_device.h"

namespace cubism_render {

class CubismModel : public Csm::CubismUserModel {
 public:
  CubismModel(filesystem::IOService* ioService, renderer::RenderDevice* device);
  ~CubismModel() override;

  CubismModel(const CubismModel&) = delete;
  CubismModel& operator=(const CubismModel&) = delete;

  bool LoadAssets(const Csm::csmChar* dir, const Csm::csmChar* fileName);
  void ReloadRenderer();
  void Update();
  void Draw(Csm::CubismMatrix44& matrix);
  Csm::CubismMotionQueueEntryHandle StartMotion(
      const Csm::csmChar* group,
      Csm::csmInt32 no,
      Csm::csmInt32 priority,
      Csm::ACubismMotion::FinishedMotionCallback onFinishedMotionHandler = NULL,
      Csm::ACubismMotion::BeganMotionCallback onBeganMotionHandler = NULL);
  Csm::CubismMotionQueueEntryHandle StartRandomMotion(
      const Csm::csmChar* group,
      Csm::csmInt32 priority,
      Csm::ACubismMotion::FinishedMotionCallback onFinishedMotionHandler = NULL,
      Csm::ACubismMotion::BeganMotionCallback onBeganMotionHandler = NULL);
  void SetExpression(const Csm::csmChar* expressionID);
  void SetRandomExpression();
  virtual void MotionEventFired(
      const Live2D::Cubism::Framework::csmString& eventValue);
  virtual Csm::csmBool HitTest(const Csm::csmChar* hitAreaName,
                               Csm::csmFloat32 x,
                               Csm::csmFloat32 y);
  void DeleteMark() { _deleteModel = true; }
  Csm::Rendering::CubismOffscreenSurface_Diligent& GetRenderBuffer();
  Csm::csmBool HasMocConsistencyFromFile(const Csm::csmChar* mocFileName);

 protected:
  void DoDraw();

 private:
  Csm::csmByte* CreateBuffer(const Csm::csmChar* path, Csm::csmSizeInt* size);
  void DeleteBuffer(Csm::csmByte* buffer, const Csm::csmChar* path = "");
  Csm::csmFloat32 GetDeltaTime();

  void SetupModel(Csm::ICubismModelSetting* setting);
  void SetupTextures();
  void PreloadMotionGroup(const Csm::csmChar* group);
  void ReleaseMotionGroup(const Csm::csmChar* group) const;
  void ReleaseMotions();
  void ReleaseExpressions();

  filesystem::IOService* _ioService;
  renderer::RenderDevice* _device;
  uint64_t _lastTick;

  Csm::ICubismModelSetting* _modelSetting;
  Csm::csmString _modelHomeDir;
  Csm::csmFloat32 _userTimeSeconds;
  Csm::csmVector<Csm::CubismIdHandle> _eyeBlinkIds;
  Csm::csmVector<Csm::CubismIdHandle> _lipSyncIds;
  Csm::csmMap<Csm::csmString, Csm::ACubismMotion*> _motions;
  Csm::csmMap<Csm::csmString, Csm::ACubismMotion*> _expressions;
  Csm::csmVector<Csm::csmRectF> _hitArea;
  Csm::csmVector<Csm::csmRectF> _userArea;
  const Csm::CubismId* _idParamAngleX;
  const Csm::CubismId* _idParamAngleY;
  const Csm::CubismId* _idParamAngleZ;
  const Csm::CubismId* _idParamBodyAngleX;
  const Csm::CubismId* _idParamEyeBallX;
  const Csm::CubismId* _idParamEyeBallY;

  base::Vector<RRefPtr<Diligent::ITexture>> _bindTextures;

  bool _deleteModel;

  Csm::Rendering::CubismOffscreenSurface_Diligent _renderBuffer;
};

}  // namespace cubism_render

#endif  //! COMPONENTS_CUBISM_CUBISM_MODEL_MANAGER_H_
