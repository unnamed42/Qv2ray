#pragma once
#include "LatencyTest.hpp"

#include <QThread>
#include <atomic>
#include <mutex>
#include <vector>

class QObject;

namespace Qv2ray::components::latency
{
    class LatencyTestThread : public QThread
    {
        Q_OBJECT
      public:
        explicit LatencyTestThread(QObject *parent = nullptr);
        void stopLatencyTest()
        {
            isStop.store(true);
        }
        void pushRequest(const QList<ConnectionId> &ids, int totalTestCount, Qv2rayLatencyTestingMethod method);
        void pushRequest(const ConnectionId &id, int totalTestCount, Qv2rayLatencyTestingMethod method);

      protected:
        void run() override;

      private:
        void processPending();
        void onWorkerFinished(QObject *worker);

        std::atomic<bool> isStop{ false };
        std::vector<LatencyTestRequest> requests;
        std::vector<QObject *> workers;
        std::mutex m;
    };

} // namespace Qv2ray::components::latency
