#ifndef SENSOR_H
#define SENSOR_H

#include <CPS4042/Hardwares/Sensors/VL530X.h>
#include <CPS4042/Sketchs/AbstractSketch.h>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>

class Sensor : public AbstractSketch<Sensors::Vl530x>
{
public:
    explicit Sensor(Sensors::Vl530x* node) :
        AbstractSketch<Sensors::Vl530x> {node}
    {}

    std::int32_t
    setup(Sensors::Vl530x::Gpio& gpio) override
    {
        std::cout << "vl530x setup completed." << std::endl;
        return 0;
    }

    std::int32_t
    loop(Sensors::Vl530x::Gpio& gpio) override
    {
        if(!node()->i2c.isAddressed()) return 0;

        std::uint16_t distance = m_distribution(m_rng);

        UByte high = static_cast<UByte>(getByte<1>(distance));
        UByte low  = static_cast<UByte>(getByte<0>(distance));
        Byte  checksum =
          static_cast<Byte>(high > low ? high - low : low - high);

        node()->i2c.write(static_cast<Byte>(high));
        node()->i2c.write(static_cast<Byte>(low));
        node()->i2c.write(checksum);

        return 0;
    }

private:
    boost::random::mt19937                                 m_rng;
    boost::random::uniform_int_distribution<std::uint16_t> m_distribution {0,
                                                                            4000};
};

#endif // SENSOR_H
