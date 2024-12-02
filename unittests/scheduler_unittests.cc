
#include "base/worker/isolate_scheduler.h"
#include "base/worker/single_worker.h"

#include "SDL3/SDL_main.h"

int SDL_main(int argc, char* argv[]) {
  std::unique_ptr<base::WorkerScheduler> scheduler =
      base::WorkerScheduler::Create();

  std::unique_ptr<base::SingleWorker> fiber_worker =
      base::SingleWorker::CreateWorker(base::WorkerScheduleMode::kCoroutine);
  std::unique_ptr<base::SingleWorker> fiber_worker2 =
      base::SingleWorker::CreateWorker(base::WorkerScheduleMode::kCoroutine);

  std::unique_ptr<base::SingleWorker> thread_worker =
      base::SingleWorker::CreateWorker(base::WorkerScheduleMode::kAsync);

  fiber_worker->SendTask(base::BindOnce([](base::SingleWorker* worker) {
    LOG(INFO) << "Fiber worker task 1";
    while (true) {
      LOG(INFO) << "Fiber loop task - node 1";
      worker->YieldFiber();
    }
  }));

  fiber_worker2->SendTask(base::BindOnce(
      [](base::SingleWorker* thread_worker, base::SingleWorker* worker) {
        LOG(INFO) << "Fiber worker task 2";
        while (true) {
          thread_worker->SendTask(
              base::BindOnce([](base::SingleWorker* worker) {
                LOG(INFO) << "Thread worker task 1";
                std::this_thread::sleep_for(std::chrono::seconds(3));
              }));

          worker->YieldFiber();
        }
      },
      thread_worker.get()));

  scheduler->AddChildWorker(std::move(fiber_worker));
  scheduler->AddChildWorker(std::move(fiber_worker2));
  scheduler->AddChildWorker(std::move(thread_worker));

  while (true) {
    scheduler->Flush();
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  return 0;
}
