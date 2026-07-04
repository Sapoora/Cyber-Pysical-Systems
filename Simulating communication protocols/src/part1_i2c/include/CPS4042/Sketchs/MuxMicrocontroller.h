#ifndef MUX_MICROCONTROLLER_H
#define MUX_MICROCONTROLLER_H

#include <CPS4042/Hardwares/Boards/Esp8266.h>
#include <CPS4042/Hardwares/Comm/I2CMultiplexer.h>
#include <CPS4042/Sketchs/AbstractSketch.h>
#include <CPS4042/Utils/ByteStream.h>

#include <cstdint>
#include <iostream>

class MuxMicroController : public AbstractSketch<Boards::Esp8266>
{
public:
    explicit MuxMicroController(Boards::Esp8266* node) :
        AbstractSketch<Boards::Esp8266> {node}
    {}

    std::int32_t
    setup(Boards::Esp8266::Gpio& gpio) override
    {
        std::cout << "MUX microcontroller setup completed." << std::endl;
        return 0;
    }

    std::int32_t
    loop(Boards::Esp8266::Gpio& gpio) override
    {
        readPendingUsartData();

        if(m_waitingForResponse)
        {
            if(m_responseCycles++ < m_responseTimeout) return 0;

            std::cerr << "MUX channel " << static_cast<int>(m_pendingChannel)
                      << " response timeout, retrying." << std::endl;
            sendRequest(m_pendingChannel);
            m_responseCycles = 0;
            return 0;
        }

        if(m_waitCycles++ < m_period) return 0;
        m_waitCycles = 0;

        static std::uint8_t nextChannel {0};
        sendRequest(nextChannel);
        nextChannel = static_cast<std::uint8_t>(
          (nextChannel + 1U) % Sensors::I2CMultiplexer::channelCount);

        return 0;
    }

private:
    void
    sendRequest(std::uint8_t channel)
    {
        m_pendingChannel    = channel;
        m_waitingForResponse = true;
        m_responseCycles     = 0;
        m_awaitingChecksum   = false;
        m_valueStream.clear();

        node()->usart.write(static_cast<Byte>(1U << channel));
    }

    void
    readPendingUsartData()
    {
        while(node()->usart.isDataAvailable())
        {
            Byte incoming = node()->usart.read();

            if(!m_waitingForResponse) continue;

            if(!m_awaitingChecksum)
            {
                m_valueStream << incoming;

                if(m_valueStream.isReady())
                {
                    m_value            = m_valueStream.take();
                    m_awaitingChecksum = true;
                }

                continue;
            }

            UByte high = static_cast<UByte>(getByte<1>(m_value));
            UByte low  = static_cast<UByte>(getByte<0>(m_value));
            UByte expectedChecksum = high > low ? high - low : low - high;

            if(static_cast<UByte>(incoming) == expectedChecksum)
            {
                std::cout << "MUX channel " << static_cast<int>(m_pendingChannel)
                          << " -> value " << m_value << std::endl;
            }
            else
            {
                std::cerr << "MUX channel " << static_cast<int>(m_pendingChannel)
                          << " checksum mismatch." << std::endl;
            }

            m_waitingForResponse = false;
            m_awaitingChecksum   = false;
            m_responseCycles     = 0;
        }
    }

private:
    static constexpr std::uint32_t m_period {800};
    static constexpr std::uint32_t m_responseTimeout {10000};

    ByteStream<std::uint16_t> m_valueStream;
    std::uint16_t             m_value {0};
    std::uint32_t             m_waitCycles {0};
    std::uint32_t             m_responseCycles {0};
    std::uint8_t              m_pendingChannel {0};
    bool                      m_waitingForResponse {false};
    bool                      m_awaitingChecksum {false};
};

#endif    // MUX_MICROCONTROLLER_H
