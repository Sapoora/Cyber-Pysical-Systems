#ifndef MUX_RANGE_SENSOR_H
#define MUX_RANGE_SENSOR_H

#include <CPS4042/Hardwares/Sensors/VL530X.h>
#include <CPS4042/Sketchs/AbstractSketch.h>
#include <CPS4042/Units/Byte.h>

#include <cstdint>
#include <iostream>

template <std::uint16_t MinValue, std::uint16_t MaxValue>
class MuxRangeSensor : public AbstractSketch<Sensors::Vl530x>
{
public:
    static_assert(MinValue <= MaxValue,
                  "MuxRangeSensor MinValue must be less than or equal to MaxValue");

    explicit MuxRangeSensor(Sensors::Vl530x* node) :
        AbstractSketch<Sensors::Vl530x> {node}
    {}

protected:
    std::int32_t
    setup(Sensors::Vl530x::Gpio& gpio) override
    {
        (void)gpio;

        std::cout << "VL530X range sensor setup completed. range=["
                  << MinValue << ", " << MaxValue << "]" << std::endl;

        return 0;
    }

    std::int32_t
    loop(Sensors::Vl530x::Gpio& gpio) override
    {
        (void)gpio;

        /*
         * The sensor must not publish data before it is addressed by the C2I
         * master. In the MUX architecture, the master of each C2I channel is
         * the MUX itself.
         */
        if(!node()->i2c.isAddressed()) return 0;

        const std::uint16_t value = nextValue();

        const UByte high = static_cast<UByte>(getByte<1>(value));
        const UByte low  = static_cast<UByte>(getByte<0>(value));

        const UByte checksum = high > low ? high - low : low - high;

        /*
         * The project statement uses byte-based protocol APIs.
         * We send exactly three bytes:
         *
         * byte 0: high byte of the 16-bit value
         * byte 1: low byte of the 16-bit value
         * byte 2: checksum = absolute difference between high and low
         */
        node()->i2c.write(static_cast<Byte>(high));
        node()->i2c.write(static_cast<Byte>(low));
        node()->i2c.write(static_cast<Byte>(checksum));

        return 0;
    }

private:
    std::uint16_t
    nextValue()
    {
        const std::uint16_t value = m_currentValue;

        if(m_currentValue >= MaxValue)
        {
            m_currentValue = MinValue;
        }
        else
        {
            ++m_currentValue;
        }

        return value;
    }

private:
    std::uint16_t m_currentValue {MinValue};
};

using SensorA = MuxRangeSensor<0, 20>;
using SensorB = MuxRangeSensor<50, 100>;

#endif    // MUX_RANGE_SENSOR_H
