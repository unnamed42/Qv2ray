#include "LatencyTestThread.hpp"

#include "RealPing.hpp"
#include "TCPing.hpp"
#include "core/CoreUtils.hpp"

#ifdef Q_OS_UNIX
#include "unix/ICMPPing.hpp"
#else
#include "win/ICMPPing.hpp"
#endif

#include <QTimer>
#include <algorithm>

namespace Qv2ray::components::latency
{

    LatencyTestThread::LatencyTestThread(QObject *parent) : QThread(parent)
    {
    }

    void LatencyTestThread::pushRequest(const ConnectionId &id, int totalTestCount, Qv2rayLatencyTestingMethod method)
    {
        if (isStop.load())
            return;
        std::unique_lock<std::mutex> lockGuard{ m };
        const auto &[protocol, host, port] = GetConnectionInfo(id);
        requests.emplace_back(LatencyTestRequest{ id, host, port, totalTestCount, method });
    }

    void LatencyTestThread::run()
    {
        // Qt event loop for this worker thread. Latency-test workers (QObjects) are
        // created here, so they have affinity with this thread and their timers /
        // sockets run on this thread's event loop.
        QTimer pollTimer;
        pollTimer.setInterval(100);
        QObject::connect(&pollTimer, &QTimer::timeout, [&]() { processPending(); });
        pollTimer.start();
        exec();
    }

    void LatencyTestThread::processPending()
    {
        if (isStop.load())
        {
            std::unique_lock<std::mutex> lockGuard{ m };
            requests.clear();
            if (workers.empty())
            {
                lockGuard.unlock();
                quit();
            }
            return;
        }

        std::unique_lock<std::mutex> lockGuard{ m };
        if (requests.empty())
            return;
        auto batch = std::move(requests);
        lockGuard.unlock();

        auto parent = qobject_cast<LatencyTestHost *>(this->parent());
        for (auto &req : batch)
        {
            switch (req.method)
            {
                case ICMPING:
                {
                    auto worker = new icmping::ICMPPing(req, parent);
                    worker->setOnFinished([this, worker]() { onWorkerFinished(worker); });
                    {
                        std::unique_lock<std::mutex> gl{ m };
                        workers.push_back(worker);
                    }
                    worker->start();
                    break;
                }
                case TCPING:
                default:
                {
                    auto worker = new tcping::TCPing(req, parent);
                    worker->setOnFinished([this, worker]() { onWorkerFinished(worker); });
                    {
                        std::unique_lock<std::mutex> gl{ m };
                        workers.push_back(worker);
                    }
                    worker->start();
                    break;
                }
                case REALPING:
                {
                    auto worker = new realping::RealPing(req, parent);
                    worker->setOnFinished([this, worker]() { onWorkerFinished(worker); });
                    {
                        std::unique_lock<std::mutex> gl{ m };
                        workers.push_back(worker);
                    }
                    worker->start();
                    break;
                }
            }
        }
    }

    void LatencyTestThread::onWorkerFinished(QObject *worker)
    {
        bool quitNow = false;
        {
            std::unique_lock<std::mutex> lockGuard{ m };
            auto it = std::find(workers.begin(), workers.end(), worker);
            if (it != workers.end())
                workers.erase(it);
            quitNow = isStop.load() && workers.empty();
        }
        // Deferred deletion, safe even when triggered from the worker's own
        // signal/slot chain.
        worker->deleteLater();
        if (quitNow)
            quit();
    }

    void LatencyTestThread::pushRequest(const QList<ConnectionId> &ids, int totalTestCount, Qv2rayLatencyTestingMethod method)
    {
        if (isStop.load())
            return;
        std::unique_lock<std::mutex> lockGuard{ m };
        for (const auto &id : ids)
        {
            const auto &[protocol, host, port] = GetConnectionInfo(id);
            requests.emplace_back(LatencyTestRequest{ id, host, port, totalTestCount, method });
        }
    }
} // namespace Qv2ray::components::latency
