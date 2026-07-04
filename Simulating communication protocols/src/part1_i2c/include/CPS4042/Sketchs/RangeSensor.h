#ifndef RANGE_SENSOR_H
#define RANGE_SENSOR_H

#include <CPS4042/Hardwares/Sensors/VL530X.h>
#include <CPS4042/Sketchs/AbstractSketch.h>

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>

#include <cstdint>
#include <iostream>
#include <string>

class RangeSensor : public AbstractSketch<Sensors::Vl530x>
{
public:
    RangeSensor(Sensors::Vl530x* node, std::uint16_t minValue,
                std::uint16_t maxValue, std::string label) :
        AbstractSketch<Sensors::Vl530x> {node},
        m_distribution {minValue, maxValue},
        m_label {std::move(label)}
    {}

    std::int32_t
    setup(Sensors::Vl530x::Gpio& gpio) override
    {
        std::cout << m_label << " setup completed." << std::endl;
        return 0;
    }

    std::int32_t
    loop(Sensors::Vl530x::Gpio& gpio) override
    {
        if(!node()->i2c.isAddressed()) return 0;

        std::uint16_t value = m_distribution(m_rng);
        UByte         high  = static_cast<UByte>(getByte<1>(value));
        UByte         low   = static_cast<UByte>(getByte<0>(value));
        Byte          checksum =
          static_cast<Byte>(high > low ? high - low : low - high);

        node()->i2c.write(static_cast<Byte>(high));
        node()->i2c.write(static_cast<Byte>(low));
        node()->i2c.write(checksum);

        return 0;
    }

private:
    boost::random::mt19937                                 m_rng;
    boost::random::uniform_int_distribution<std::uint16_t> m_distribution;
    std::string                                            m_label;
};

#endif    // RANGE_SENSOR_H
