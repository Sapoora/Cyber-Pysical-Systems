#ifndef I2C_MULTIPLEXER_H
#define I2C_MULTIPLEXER_H

#include <CPS4042/Hardwares/Board.h>
#include <CPS4042/Protocols/Protocol.h>
#include <CPS4042/Protocols/USART.h>
#include <CPS4042/Units/BaudRate.h>
#include <CPS4042/Wires/Pin.h>
#include <boost/pfr.hpp>

#include <cstdint>
#include <iostream>
#include <type_traits>
#include <utility>

namespace Comm
{

using I2CMultiplexerVoltage = VoltageLevel3_3v;

template <BaudRate BR, BitRate BTR, typename WorkingVoltageTp>
requires std::is_base_of_v<AbstractVoltageLevel, WorkingVoltageTp>
struct I2CMultiplexerGpio
{
public:
    /*
     * USART side:
     * These pins connect the MUX to the microcontroller.
     *
     * ESP8266::tx  --->  I2CMux::rx
     * ESP8266::rx  <---  I2CMux::tx
     */
    Pins::Tx<WorkingVoltageTp>  tx  {BR, BTR, "I2CMux::tx"};     // Pin 0
    Pins::Rx<WorkingVoltageTp>  rx  {BR, BTR, "I2CMux::rx"};     // Pin 1
    Pins::Vdd<WorkingVoltageTp> vdd {BR, BTR, "I2CMux::vdd"};    // Pin 2
    Pins::Gnd<WorkingVoltageTp> gnd {BR, BTR, "I2CMux::gnd"};    // Pin 3

    /*
     * C2I / I2C channel 0:
     * This channel connects to sensor 0.
     */
    Pins::Vdd<WorkingVoltageTp> ch0_vdd {BR, BTR, "I2CMux::ch0_vdd"};    // Pin 4
    Pins::Gnd<WorkingVoltageTp> ch0_gnd {BR, BTR, "I2CMux::ch0_gnd"};    // Pin 5
    Pins::Sda<WorkingVoltageTp> ch0_sda {BR, BTR, "I2CMux::ch0_sda"};    // Pin 6
    Pins::Scl<WorkingVoltageTp> ch0_scl {BR, BTR, "I2CMux::ch0_scl"};    // Pin 7

    /*
     * C2I / I2C channel 1:
     * This channel connects to sensor 1.
     */
    Pins::Vdd<WorkingVoltageTp> ch1_vdd {BR, BTR, "I2CMux::ch1_vdd"};    // Pin 8
    Pins::Gnd<WorkingVoltageTp> ch1_gnd {BR, BTR, "I2CMux::ch1_gnd"};    // Pin 9
    Pins::Sda<WorkingVoltageTp> ch1_sda {BR, BTR, "I2CMux::ch1_sda"};    // Pin 10
    Pins::Scl<WorkingVoltageTp> ch1_scl {BR, BTR, "I2CMux::ch1_scl"};    // Pin 11
};

class I2CMultiplexer
    : public Board<BaudRates::NotSpecified,
                   BitRates::same(BaudRates::NotSpecified),
                   Frequency::F320khz,
                   I2CMultiplexerVoltage,
                   I2CMultiplexerGpio>
{
public:
    enum class Channel : std::uint8_t
    {
        Ch0 = 0,
        Ch1 = 1
    };

    inline static constexpr std::uint8_t channelCount = 2;

    explicit I2CMultiplexer() :
        Parent {"I2CMux::processor"}
    {
        /*
         * This board must write on tx and read from rx.
         * We disable reading from tx so that the MUX does not consume its own
         * transmitted USART bits.
         */
        m_gpio.tx.setCanRead(false);
        m_processor->communicationClockChanged.connect(
          [this](Bit edge) { m_gpio.ch0_scl.nextEdge(edge); });

        m_processor->communicationClockChanged.connect(
          [this](Bit edge) { m_gpio.ch1_scl.nextEdge(edge); });

        /*
         * The processor can manage multiple protocols at the same time:
         * - one USART protocol for the microcontroller side
         * - one C2I protocol for channel 0
         * - one C2I protocol for channel 1
         */
        m_processor->installProtocol(&usart);
        m_processor->installProtocol(&i2c0);
        m_processor->installProtocol(&i2c1);

        std::cout << "one instance of I2CMultiplexer created." << std::endl;
    }

    class USART : public Protocols::AbstractUsart<I2CMultiplexer, Gpio>
    {
    public:
        explicit USART(I2CMultiplexer* board) :
            Protocols::AbstractUsart<I2CMultiplexer, Gpio> {board}
        {}

        void
        write(Byte byte) override
        {
            Protocols::USART::writeFrame(m_board->gpio().tx, byte);
        }

        Byte
        read() override
        {
            return m_receiver.read();
        }

        bool
        isDataAvailable() const
        {
            return m_receiver.isDataAvailable();
        }

        void
        run(Gpio& gpio) override
        {
            while(gpio.rx.hasBitToRead())
            {
                m_receiver.push(gpio.rx.readBit());
            }
        }

    private:
        Protocols::USART::Receiver m_receiver;

    } mutable usart {this};

    class C2I : public Protocols::AbstractI2C<I2CMultiplexer, Gpio>
    {
    public:
        explicit C2I(I2CMultiplexer* board, Channel channel) :
            Protocols::AbstractI2C<I2CMultiplexer, Gpio> {board},
            m_channel {channel}
        {}

        void
        init(Byte address) override
        {
            m_address    = address;
            m_addressing = true;

            /*
             * The MUX behaves like a C2I master toward the selected sensor.
             * First, it writes the sensor address on that channel's SDA line.
             */
            sda().write(address);
        }

        void
        write(Byte byte) override
        {
            (void)byte;
        }

        Byte
        read() override
        {
            Byte byte = m_buffer.front();
            m_buffer.pop();
            return byte;
        }

        void
        run(Gpio& gpio) override
        {
            if(!m_addressing) return;

            auto& dataLine = sda(gpio);

            /*
             * Before ACK:
             * Wait until the address byte has been completely written,
             * then wait for the sensor ACK bit.
             */
            if(!m_started)
            {
                if(dataLine.hasBitToWrite()) return;
                if(!dataLine.hasBitToRead()) return;

                if(dataLine.readBit() != Bit::One) return;

                m_started = true;
                return;
            }

            /*
             * After ACK:
             * Every complete byte available on SDA is sensor data.
             */
            while(dataLine.hasByteToRead())
            {
                m_buffer.push(dataLine.read());
            }
        }

    private:
        using SdaPin =
          std::remove_reference_t<decltype(std::declval<Gpio&>().ch0_sda)>;

        SdaPin&
        sda()
        {
            return sda(m_board->gpio());
        }

        SdaPin&
        sda(Gpio& gpio)
        {
            if(m_channel == Channel::Ch0)
            {
                return gpio.ch0_sda;
            }

            return gpio.ch1_sda;
        }

    private:
        Channel m_channel;
        Byte    m_address {0};
        bool    m_addressing {false};

    } mutable i2c0 {this, Channel::Ch0},
              i2c1 {this, Channel::Ch1};

protected:
    inline void
    startModule() override
    {}
};

}    // namespace Comm

#endif    // I2C_MULTIPLEXER_H
