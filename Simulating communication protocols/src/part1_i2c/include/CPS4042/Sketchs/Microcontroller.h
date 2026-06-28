#ifndef MICROCONTROLLER_H
#define MICROCONTROLLER_H

#include <CPS4042/Hardwares/Boards/Esp8266.h>
#include <CPS4042/Hardwares/Sensors/VL530X.h>
#include <CPS4042/Sketchs/AbstractSketch.h>
#include <CPS4042/Utils/ByteStream.h>
#include <CPS4042/Utils/Wave.h>
#include <bitset>
class MicroController : public AbstractSketch<Boards::Esp8266>
{
public:
    explicit MicroController(Boards::Esp8266* node) :
        AbstractSketch<Boards::Esp8266> {node}
    {}

    std::int32_t
    setup(Boards::Esp8266::Gpio& gpio) override
    {
        std::cout << "esp8266 setup completed." << std::endl;
        node()->i2c.init(Sensors::Vl530x::address);
        return 0;
    }

    std::int32_t
    loop(Boards::Esp8266::Gpio& gpio) override
    {
        if(!node()->i2c.isDataAvailable()) return 0;

        Byte incoming = node()->i2c.read();

        if(!m_awaitingChecksum)
        {
            m_distanceStream << incoming;

            if(m_distanceStream.isReady())
            {
                m_distance         = m_distanceStream.take();
                m_awaitingChecksum = true;
            }

            return 0;
        }

        UByte high     = static_cast<UByte>(getByte<1>(m_distance));
        UByte low      = static_cast<UByte>(getByte<0>(m_distance));
        UByte expected = high > low ? high - low : low - high;

        if(static_cast<UByte>(incoming) == expected)
            std::cout << "new distance: " << m_distance << std::endl;
        else
            std::cerr << "checksum mismatch, discarding reading" << std::endl;

        m_awaitingChecksum = false;

        return 0;
    }

private:
    ByteStream<std::uint16_t> m_distanceStream;
    std::uint16_t             m_distance {0};
    bool                      m_awaitingChecksum {false};
};

#endif    // MICROCONTROLLER_H
