#ifndef HARDDISK_H
#define HARDDISK_H

#include <CPS4042/Hardwares/Comm/Usb.h>
#include <CPS4042/Sketchs/AbstractSketch.h>

class HardDisk : public AbstractSketch<Sensors::Usb>
{
public:
    explicit HardDisk(Sensors::Usb* node) :
        AbstractSketch<Sensors::Usb> {node}
    {}

    std::int32_t
    setup(Sensors::Usb::Gpio& gpio) override
    {
        std::cout << "HardDisk setup completed." << std::endl;
        return 0;
    }

    std::int32_t
    loop(Sensors::Usb::Gpio& gpio) override
    {
        if(!node()->usart.isDataAvailable()) return 0;

        Byte address = node()->usart.read();
        Byte data    = dataAt(address);

        node()->usart.write(data);

        return 0;
    }

private:
    Byte
    dataAt(Byte address) const
    {
        auto value = static_cast<UByte>(address);
        return static_cast<Byte>(value * 2U);
    }
};

#endif    // HARDDISK_H
