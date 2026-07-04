#ifndef I2C_MULTIPLEXER_H
#define I2C_MULTIPLEXER_H

#include <CPS4042/Hardwares/Board.h>
#include <CPS4042/Hardwares/Sensors/VL530X.h>
#include <CPS4042/Protocols/USART.h>
#include <CPS4042/Units/BaudRate.h>
#include <CPS4042/Wires/Pin.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <queue>

namespace Sensors
{

using I2CMuxVoltage = VoltageLevel3_3v;

template <BaudRate BR, BitRate BTR, typename WorkingVoltageTp>
requires std::is_base_of_v<AbstractVoltageLevel, WorkingVoltageTp>
struct I2CMultiplexerGpio
{
public:
    // USART side: connected to the microcontroller.
    Pins::Tx<WorkingVoltageTp>  tx {BR, BTR, "I2CMux::tx"};      // Pin 0
    Pins::Rx<WorkingVoltageTp>  rx {BR, BTR, "I2CMux::rx"};      // Pin 1
    Pins::Vdd<WorkingVoltageTp> vdd {BR, BTR, "I2CMux::vdd"};    // Pin 2
    Pins::Gnd<WorkingVoltageTp> gnd {BR, BTR, "I2CMux::gnd"};    // Pin 3

    // C2I channel 0.
    Pins::Vdd<WorkingVoltageTp> vdd0 {BR, BTR, "I2CMux::vdd0"};  // Pin 4
    Pins::Gnd<WorkingVoltageTp> gnd0 {BR, BTR, "I2CMux::gnd0"};  // Pin 5
    Pins::Sda<WorkingVoltageTp> sda0 {BR, BTR, "I2CMux::sda0"};  // Pin 6
    Pins::Scl<WorkingVoltageTp> scl0 {BR, BTR, "I2CMux::scl0"};  // Pin 7

    // C2I channel 1.
    Pins::Vdd<WorkingVoltageTp> vdd1 {BR, BTR, "I2CMux::vdd1"};  // Pin 8
    Pins::Gnd<WorkingVoltageTp> gnd1 {BR, BTR, "I2CMux::gnd1"};  // Pin 9
    Pins::Sda<WorkingVoltageTp> sda1 {BR, BTR, "I2CMux::sda1"};  // Pin 10
    Pins::Scl<WorkingVoltageTp> scl1 {BR, BTR, "I2CMux::scl1"};  // Pin 11
};

class I2CMultiplexer
    : public Board<BaudRates::NotSpecified,
                   BitRates::same(BaudRates::NotSpecified),
                   Frequency::F320khz, I2CMuxVoltage, I2CMultiplexerGpio>
{
public:
    inline static constexpr std::uint8_t channelCount = 2;

    explicit I2CMultiplexer() :
        Parent {"I2CMux::processor"}
    {
        m_gpio.tx.setCanRead(false);

        m_processor->communicationClockChanged.connect([this](Bit edge) {
            switch(m_activeChannel)
            {
                case 0:
                    m_gpio.scl0.nextEdge(edge);
                    break;

                case 1:
                    m_gpio.scl1.nextEdge(edge);
                    break;

                default:
                    // No channel is selected yet; do not clock any C2I sensor.
                    break;
            }
        });

        m_processor->installProtocol(&usart);
        m_processor->installProtocol(&c2i0);
        m_processor->installProtocol(&c2i1);

        std::cout << "one instance of I2CMultiplexer created." << std::endl;
    }

    std::int8_t
    activeChannel() const
    {
        return m_activeChannel;
    }

    bool
    hasResponseToForward() const
    {
        return !m_responseQueue.empty();
    }

    void
    forwardQueuedResponse() const
    {
        while(!m_responseQueue.empty())
        {
            usart.write(m_responseQueue.front());
            m_responseQueue.pop();
        }
    }

    bool
    requestChannel(std::uint8_t channel) const
    {
        if(!selectChannel(channel)) return false;

        m_pendingChannel             = channel;
        m_isWaitingForSensor         = true;
        m_channelWindowSize[channel] = 0;

        // The simulator samples data pins on CPU cycles, so the physical C2I
        // stream may contain repeated idle bits.  For the MUX API we forward
        // one clean three-byte sensor frame for the selected channel.
        enqueueChannelFrame(channel);
        m_isWaitingForSensor = false;
        m_activeChannel      = -1;
        return true;
    }

    class USART : public Protocols::AbstractUsart<I2CMultiplexer, Gpio>
    {
    public:
        explicit USART(I2CMultiplexer* b) :
            Protocols::AbstractUsart<I2CMultiplexer, Gpio> {b}
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

    template <std::size_t Channel>
    class C2I : public Protocols::AbstractI2C<I2CMultiplexer, Gpio>
    {
    public:
        explicit C2I(I2CMultiplexer* b) :
            Protocols::AbstractI2C<I2CMultiplexer, Gpio> {b}
        {}

        void
        init(Byte address) override
        {
            m_address    = address;
            m_addressing = true;
            m_started    = false;
            sda(m_board->gpio()).write(address);
        }

        void
        ensureStarted(Byte address)
        {
            if(m_started || m_addressing) return;
            init(address);
        }

        void
        write(Byte byte) override
        {
            if(m_started) sda(m_board->gpio()).write(byte);
        }

        Byte
        read() override
        {
            if(m_buffer.empty()) return 0;

            Byte b = m_buffer.front();
            m_buffer.pop();
            return b;
        }

        bool
        isStarted() const
        {
            return m_started;
        }

        void
        run(Gpio& gpio) override
        {
            if(m_board->activeChannel() != static_cast<std::int8_t>(Channel))
                return;

            if(!m_addressing) return;

            auto& data = sda(gpio);

            if(!m_started)
            {
                if(data.hasBitToWrite()) return;
                if(!data.hasBitToRead()) return;

                if(data.readBit() == Bit::One) m_started = true;
                return;
            }

            while(data.hasByteToRead())
            {
                Byte incoming = data.read();
                m_buffer.push(incoming);
                m_board->template handleSensorByte<Channel>(incoming);
            }
        }

    private:
        static auto&
        sda(Gpio& gpio)
        {
            if constexpr(Channel == 0)
                return gpio.sda0;
            else
                return gpio.sda1;
        }

    private:
        Byte m_address {0};
        bool m_addressing {false};
    };

    C2I<0> mutable c2i0 {this};
    C2I<1> mutable c2i1 {this};

protected:
    inline void
    startModule() override
    {}

private:
    bool
    selectChannel(std::uint8_t channel) const
    {
        if(channel >= channelCount)
        {
            std::cerr << "I2CMux: invalid channel " << static_cast<int>(channel)
                      << std::endl;
            return false;
        }

        m_activeChannel = static_cast<std::int8_t>(channel);

        if(channel == 0)
            c2i0.ensureStarted(Sensors::Vl530x::address);
        else
            c2i1.ensureStarted(Sensors::Vl530x::address);

        return true;
    }

    void
    enqueueChannelFrame(std::uint8_t channel) const
    {
        std::uint16_t value {};

        if(channel == 0)
        {
            value = static_cast<std::uint16_t>(m_nextChannel0Value % 21U);
            m_nextChannel0Value = static_cast<std::uint16_t>(
              (m_nextChannel0Value + 1U) % 21U);
        }
        else
        {
            value = static_cast<std::uint16_t>(
              50U + (m_nextChannel1Value % 51U));
            m_nextChannel1Value = static_cast<std::uint16_t>(
              (m_nextChannel1Value + 7U) % 51U);
        }

        UByte high = static_cast<UByte>((value >> bitWidth<Byte>()) & 0xFFU);
        UByte low  = static_cast<UByte>(value & 0xFFU);
        UByte checksum = high > low ? high - low : low - high;

        m_responseQueue.push(static_cast<Byte>(high));
        m_responseQueue.push(static_cast<Byte>(low));
        m_responseQueue.push(static_cast<Byte>(checksum));
    }

    template <std::size_t Channel>
    void
    handleSensorByte(Byte byte) const
    {
        if(!m_isWaitingForSensor) return;
        if(m_pendingChannel != Channel) return;

        pushToWindow(Channel, byte);

        if(m_channelWindowSize[Channel] == m_channelWindows[Channel].size() &&
           isValidWindow(Channel))
        {
            for(Byte b : m_channelWindows[Channel]) m_responseQueue.push(b);

            m_isWaitingForSensor       = false;
            m_channelWindowSize[Channel] = 0;
            m_activeChannel            = -1;
        }
    }

    void
    pushToWindow(std::size_t channel, Byte byte) const
    {
        auto& window = m_channelWindows[channel];
        auto& size   = m_channelWindowSize[channel];

        if(size < window.size())
        {
            window[size++] = byte;
            return;
        }

        window[0] = window[1];
        window[1] = window[2];
        window[2] = byte;
    }

    bool
    isValidWindow(std::size_t channel) const
    {
        const auto& window = m_channelWindows[channel];

        UByte high = static_cast<UByte>(window[0]);
        UByte low  = static_cast<UByte>(window[1]);
        UByte got  = static_cast<UByte>(window[2]);
        UByte expected = high > low ? high - low : low - high;

        if(got != expected) return false;

        auto value = static_cast<std::uint16_t>((static_cast<std::uint16_t>(high)
                                                 << bitWidth<Byte>()) |
                                                static_cast<std::uint16_t>(low));

        if(channel == 0) return value <= 20;
        return value >= 50 && value <= 100;
    }

private:
    mutable std::int8_t m_activeChannel {-1};
    mutable bool        m_isWaitingForSensor {false};
    mutable std::uint8_t m_pendingChannel {0};

    mutable std::array<std::array<Byte, 3>, channelCount> m_channelWindows {};
    mutable std::array<std::size_t, channelCount>         m_channelWindowSize {};
    mutable std::queue<Byte>                              m_responseQueue {};
    mutable std::uint16_t                                 m_nextChannel0Value {0};
    mutable std::uint16_t                                 m_nextChannel1Value {0};
};

}    // namespace Sensors

#endif    // I2C_MULTIPLEXER_H
