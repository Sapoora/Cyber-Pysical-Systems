#ifndef USB_H
#define USB_H

#include <CPS4042/Hardwares/Board.h>
#include <CPS4042/Protocols/USART.h>
#include <CPS4042/Units/BaudRate.h>
#include <CPS4042/Wires/Pin.h>
#include <boost/pfr.hpp>
#include <iostream>

namespace Sensors
{
using UsbVoltage = VoltageLevel3_3v;

template <BaudRate BR, BitRate BTR, typename WorkingVoltageTp>
requires std::is_base_of_v<AbstractVoltageLevel, WorkingVoltageTp>
struct UsbGpio
{
public:
    Pins::Tx<WorkingVoltageTp>  tx {BR, BTR, "Usb::tx"};      // Pin 0
    Pins::Rx<WorkingVoltageTp>  rx {BR, BTR, "Usb::rx"};      // Pin 1
    Pins::Vdd<WorkingVoltageTp> vdd {BR, BTR, "Usb::vdd"};    // Pin 2
    Pins::Gnd<WorkingVoltageTp> gnd {BR, BTR, "Usb::gnd"};    // Pin 3
};

class Usb : public Board<BaudRates::NotSpecified,
                         BitRates::same(BaudRates::NotSpecified),
                         Frequency::F320khz, UsbVoltage, UsbGpio>
{
public:
    explicit Usb() :
        Parent {"Usb::processor"}
    {
        m_processor->installProtocol(&usart);

        std::cout << "one instance of Usb" << " created." << std::endl;
    }

    class USART : public Protocols::AbstractUsart<Usb, Gpio>
    {
    public:
        explicit USART(Usb* b) :
            Protocols::AbstractUsart<Usb, Gpio> {b}
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

protected:
    inline void
    startModule() override
    {}
};

}    // namespace Sensors

#endif    // USB_H
