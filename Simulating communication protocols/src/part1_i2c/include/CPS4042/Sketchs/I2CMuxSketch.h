#ifndef I2C_MUX_SKETCH_H
#define I2C_MUX_SKETCH_H

#include <CPS4042/Hardwares/Comm/I2CMultiplexer.h>
#include <CPS4042/Hardwares/Sensors/VL530X.h>
#include <CPS4042/Sketchs/AbstractSketch.h>

#include <array>
#include <cstdint>
#include <iostream>

class I2CMuxSketch : public AbstractSketch<Comm::I2CMultiplexer>
{
public:
    explicit I2CMuxSketch(Comm::I2CMultiplexer* node) :
        AbstractSketch<Comm::I2CMultiplexer> {node}
    {}

protected:
    std::int32_t
    setup(Comm::I2CMultiplexer::Gpio& gpio) override
    {
        (void)gpio;

        /*
         * Each C2I channel is an independent bus.
         * Both sensors can have the same address because they are placed on
         * different channels of the MUX.
         */
        node()->i2c0.init(Sensors::Vl530x::address);
        node()->i2c1.init(Sensors::Vl530x::address);

        std::cout << "I2C MUX setup completed." << std::endl;
        return 0;
    }

    std::int32_t
    loop(Comm::I2CMultiplexer::Gpio& gpio) override
    {
        (void)gpio;

        readChannelCommand();
        forwardSelectedSensorSample();

        return 0;
    }

private:
    void
    readChannelCommand()
    {
        if(!node()->usart.isDataAvailable()) return;

        Byte command = node()->usart.read();
        auto requestedChannel = static_cast<UByte>(command);

        if(requestedChannel >= Comm::I2CMultiplexer::channelCount)
        {
            std::cerr << "I2C MUX ignored invalid channel: "
                      << static_cast<int>(requestedChannel) << std::endl;
            return;
        }

        m_selectedChannel = requestedChannel;
        m_waitingForSample = true;
        m_sampleByteCount = 0;
    }

    void
    forwardSelectedSensorSample()
    {
        if(!m_waitingForSample) return;

        auto& selectedI2C = selectedChannelI2C();

        while(selectedI2C.isDataAvailable() &&
              m_sampleByteCount < m_sensorSample.size())
        {
            m_sensorSample[m_sampleByteCount] = selectedI2C.read();
            ++m_sampleByteCount;
        }

        if(m_sampleByteCount != m_sensorSample.size()) return;

        /*
         * The MUX must forward exactly what it has received from the selected
         * C2I channel:
         *
         * byte 0: high byte
         * byte 1: low byte
         * byte 2: checksum
         */
        for(Byte byte : m_sensorSample)
        {
            node()->usart.write(byte);
        }

        m_waitingForSample = false;
        m_sampleByteCount = 0;
    }

    Comm::I2CMultiplexer::C2I&
    selectedChannelI2C() const
    {
        if(m_selectedChannel == 0)
        {
            return node()->i2c0;
        }

        return node()->i2c1;
    }

private:
    static constexpr std::uint8_t sensorSampleSize = 3;

    UByte m_selectedChannel {0};
    bool  m_waitingForSample {false};

    std::array<Byte, sensorSampleSize> m_sensorSample {};
    std::uint8_t                       m_sampleByteCount {0};
};

#endif    // I2C_MUX_SKETCH_H
