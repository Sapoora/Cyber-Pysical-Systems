#ifndef USARTMICROCONTROLLER_H
#define USARTMICROCONTROLLER_H

#include <CPS4042/Hardwares/Boards/Esp8266.h>
#include <CPS4042/Sketchs/AbstractSketch.h>

class UsartMicroController : public AbstractSketch<Boards::Esp8266>
{
public:
    explicit UsartMicroController(Boards::Esp8266* node) :
        AbstractSketch<Boards::Esp8266> {node}
    {}

    std::int32_t
    setup(Boards::Esp8266::Gpio& gpio) override
    {
        std::cout << "USART microcontroller setup completed." << std::endl;
        return 0;
    }

    std::int32_t
    loop(Boards::Esp8266::Gpio& gpio) override
    {
        if(node()->usart.isDataAvailable())
        {
            Byte data = node()->usart.read();
            if(m_waitingForResponse && data == expectedData(m_pendingAddress))
            {
                std::cout << "USART address "
                          << static_cast<int>(m_pendingAddress) << " -> data "
                          << static_cast<int>(static_cast<UByte>(data))
                          << std::endl;
                m_waitingForResponse = false;
                m_responseCycles      = 0;
            }
        }

        if(m_waitingForResponse)
        {
            if(m_responseCycles++ < m_responseTimeout) return 0;

            m_responseCycles = 0;
            node()->usart.write(static_cast<Byte>(m_pendingAddress));
            return 0;
        }

        if(m_waitCycles++ < m_period) return 0;

        m_waitCycles        = 0;
        m_pendingAddress    = m_nextAddress++;
        m_waitingForResponse = true;
        m_responseCycles     = 0;
        node()->usart.write(static_cast<Byte>(m_pendingAddress));

        return 0;
    }

private:
    static constexpr std::uint32_t m_period {200};
    static constexpr std::uint32_t m_responseTimeout {2000};

    static constexpr Byte
    expectedData(UByte address)
    {
        return static_cast<Byte>(address * 2U);
    }

    std::uint32_t m_waitCycles {0};
    std::uint32_t m_responseCycles {0};
    UByte         m_nextAddress {0};
    UByte         m_pendingAddress {0};
    bool          m_waitingForResponse {false};
};

#endif    // USARTMICROCONTROLLER_H
