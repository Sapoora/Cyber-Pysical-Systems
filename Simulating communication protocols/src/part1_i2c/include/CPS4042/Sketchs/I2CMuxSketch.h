#ifndef I2C_MUX_SKETCH_H
#define I2C_MUX_SKETCH_H

#include <CPS4042/Hardwares/Comm/I2CMultiplexer.h>
#include <CPS4042/Sketchs/AbstractSketch.h>

#include <bit>
#include <cstdint>
#include <iostream>

class I2CMuxSketch : public AbstractSketch<Sensors::I2CMultiplexer>
{
public:
    explicit I2CMuxSketch(Sensors::I2CMultiplexer* node) :
        AbstractSketch<Sensors::I2CMultiplexer> {node}
    {}

    std::int32_t
    setup(Sensors::I2CMultiplexer::Gpio& gpio) override
    {
        std::cout << "I2C MUX setup completed." << std::endl;
        return 0;
    }

    std::int32_t
    loop(Sensors::I2CMultiplexer::Gpio& gpio) override
    {
        if(node()->hasResponseToForward())
        {
            node()->forwardQueuedResponse();
            return 0;
        }

        if(!node()->usart.isDataAvailable()) return 0;

        auto command = static_cast<UByte>(node()->usart.read());
        if(command == 0) return 0;

        // One-hot channel code: 0x01 -> channel 0, 0x02 -> channel 1.
        auto channel = static_cast<std::uint8_t>(
          std::countr_zero(command) % Sensors::I2CMultiplexer::channelCount);

        node()->requestChannel(channel);
        return 0;
    }
};

#endif    // I2C_MUX_SKETCH_H
