#include "ICMPPing.hpp"

#include <QObject>

#ifdef Q_OS_UNIX
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h> //macos need that
#include <netinet/ip_icmp.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#ifdef Q_OS_MAC
#define SOL_IP 0
#endif

#define QV_MODULE_NAME "ICMPingWorker"

namespace Qv2ray::components::latency::icmping
{
    /// 1s complementary checksum
    uint16_t ping_checksum(const char *buf, size_t size)
    {
        size_t i;
        uint64_t sum = 0;

        for (i = 0; i < size; i += 2)
        {
            sum += *(uint16_t *) buf;
            buf += 2;
        }
        if (size - i > 0)
        {
            sum += *(uint8_t *) buf;
        }

        while ((sum >> 16) != 0)
        {
            sum = (sum & 0xffff) + (sum >> 16);
        }

        return (uint16_t) ~sum;
    }

    void ICMPPing::deinit()
    {
        if (socketId >= 0)
        {
            close(socketId);
            socketId = -1;
        }
        if (readNotifier)
        {
            readNotifier->setEnabled(false);
            readNotifier->deleteLater();
            readNotifier = nullptr;
        }
    }

    void ICMPPing::start(int ttl)
    {
        data.totalCount = req.totalCount;
        data.failedCount = 0;
        data.worst = 0;
        data.avg = 0;
        data.best = 0;

        if (((socketId = socket(PF_INET, SOCK_DGRAM, IPPROTO_ICMP)) < 0))
        {
            data.errorMessage = "EPING_SOCK: " + QObject::tr("Socket creation failed");
            data.avg = LATENCY_TEST_VALUE_ERROR;
            data.totalCount = req.totalCount;
            data.failedCount = req.totalCount;
            notifyCompleted();
            finish();
            return;
        }
        // set TTL
        if (setsockopt(socketId, SOL_IP, IP_TTL, &ttl, sizeof(ttl)) != 0)
        {
            data.errorMessage = "EPING_TTL: " + QObject::tr("Failed to setup TTL value");
            data.avg = LATENCY_TEST_VALUE_ERROR;
            data.totalCount = req.totalCount;
            data.failedCount = req.totalCount;
            deinit();
            notifyCompleted();
            finish();
            return;
        }
        startResolve();
    }

    void ICMPPing::onHostResolved()
    {
        // This raw-socket ping only supports IPv4.
        if (targetAddress.isNull() || targetAddress.protocol() != QAbstractSocket::IPv4Protocol)
        {
            data.errorMessage = QObject::tr("ICMP ping only supports IPv4 addresses");
            data.avg = LATENCY_TEST_VALUE_ERROR;
            data.totalCount = req.totalCount;
            data.failedCount = req.totalCount;
            deinit();
            notifyCompleted();
            finish();
            return;
        }
        ping();
    }

    void ICMPPing::ping()
    {
        memset(&storage4, 0, sizeof(storage4));
        storage4.sin_family = AF_INET;
        storage4.sin_port = 0;
        storage4.sin_addr.s_addr = htonl(targetAddress.toIPv4Address());

        connect(&timeoutTimer, &QTimer::timeout, this, &ICMPPing::onTimeout);
        timeoutTimer.setSingleShot(true);
        timeoutTimer.setInterval(10000);
        timeoutTimer.start();

        readNotifier = new QSocketNotifier(socketId, QSocketNotifier::Read, this);
        connect(readNotifier, &QSocketNotifier::activated, this, [this]() { onDataAvailable(); });

        for (int i = 0; i < req.totalCount; ++i)
        {
            // prepare echo request packet
            icmp _icmp_request;
            memset(&_icmp_request, 0, sizeof(_icmp_request));
            _icmp_request.icmp_type = ICMP_ECHO;
            _icmp_request.icmp_hun.ih_idseq.icd_id = 0; // SOCK_DGRAM & 0 => id will be set by kernel
            _icmp_request.icmp_hun.ih_idseq.icd_seq = seq++;
            _icmp_request.icmp_cksum = ping_checksum(reinterpret_cast<char *>(&_icmp_request), sizeof(_icmp_request));

            timeval start;
            gettimeofday(&start, nullptr);
            startTimevals.push_back(start);

            int n;
            do
            {
                n = ::sendto(socketId, &_icmp_request, sizeof(icmp), 0, (struct sockaddr *) &storage4, sizeof(struct sockaddr));
            } while (n < 0 && errno == EINTR);
        }
    }

    void ICMPPing::onDataAvailable()
    {
        timeval end;
        sockaddr_in addr;
        socklen_t slen = sizeof(sockaddr_in);
        int rlen = 0;
        char buf[1024];

        do
        {
            do
            {
                rlen = recvfrom(socketId, buf, 1024, 0, (struct sockaddr *) &addr, &slen);
            } while (rlen == -1 && errno == EINTR);

            // skip malformed
#ifdef Q_OS_MAC
            if (rlen < sizeof(icmp) + 20)
#else
            if (rlen < sizeof(icmp))
#endif
                continue;

#ifdef Q_OS_MAC
            auto &resp = *reinterpret_cast<icmp *>(buf + 20);
#else
            auto &resp = *reinterpret_cast<icmp *>(buf);
#endif
            // skip the ones we didn't send
            auto cur_seq = resp.icmp_hun.ih_idseq.icd_seq;
            if (cur_seq >= seq)
                continue;

            switch (resp.icmp_type)
            {
                case ICMP_ECHOREPLY:
                    gettimeofday(&end, nullptr);
                    data.avg += 1000 * (end.tv_sec - startTimevals[cur_seq - 1].tv_sec) + (end.tv_usec - startTimevals[cur_seq - 1].tv_usec) / 1000;
                    successCount++;
                    notifyTestHost();
                    continue;
                case ICMP_UNREACH:
                    data.errorMessage = "EPING_DST: " + QObject::tr("Destination unreachable");
                    data.failedCount++;
                    if (notifyTestHost())
                        return;
                    continue;
                case ICMP_TIMXCEED:
                    data.errorMessage = "EPING_TIME: " + QObject::tr("Timeout");
                    data.failedCount++;
                    if (notifyTestHost())
                        return;
                    continue;
                default:
                    data.errorMessage = "EPING_UNK: " + QObject::tr("Unknown error");
                    data.failedCount++;
                    if (notifyTestHost())
                        return;
                    continue;
            }
        } while (rlen > 0);

        // Re-arm the notifier (QSocketNotifier disables itself on activation).
        if (readNotifier)
            readNotifier->setEnabled(true);
    }

    void ICMPPing::onTimeout()
    {
        successCount = 0;
        data.failedCount = data.totalCount = req.totalCount;
        notifyTestHost();
    }

    bool ICMPPing::notifyTestHost()
    {
        if (data.failedCount + successCount == data.totalCount)
        {
            if (data.failedCount == data.totalCount)
                data.avg = LATENCY_TEST_VALUE_ERROR;
            else
                data.errorMessage.clear(), data.avg = data.avg / successCount;

            timeoutTimer.stop();
            deinit();
            notifyCompleted();
            finish();
            return true;
        }
        return false;
    }

    ICMPPing::~ICMPPing()
    {
        deinit();
    }
} // namespace Qv2ray::components::latency::icmping
#endif
