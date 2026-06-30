#include <CPS4042/Hardwares/Comm/Usb.h>
#include <CPS4042/Sketchs/HardDisk.h>
#include <CPS4042/Sketchs/UsartMicrocontroller.h>
#include <CPS4042/Wires/Link.h>
#include <CPS4042/main.h>

std::int32_t
main()
{
    Boards::Esp8266 esp8266;
    Sensors::Usb    usb;

    auto            linkVdd = std::make_shared<Link>();
    auto            linkGnd = std::make_shared<Link>();
    auto            linkTx  = std::make_shared<Link>();
    auto            linkRx  = std::make_shared<Link>();

    CPS_SET_OBJECT_NAME(esp8266);
    CPS_SET_OBJECT_NAME(usb);

    CPS_SET_OBJECT_NAME_PTR(linkVdd);
    CPS_SET_OBJECT_NAME_PTR(linkGnd);
    CPS_SET_OBJECT_NAME_PTR(linkTx);
    CPS_SET_OBJECT_NAME_PTR(linkRx);

    esp8266.gpio().vdd2.attachLink(linkVdd);
    esp8266.gpio().gnd2.attachLink(linkGnd);
    esp8266.gpio().tx.attachLink(linkTx);
    esp8266.gpio().rx.attachLink(linkRx);

    usb.gpio().vdd.attachLink(linkVdd);
    usb.gpio().gnd.attachLink(linkGnd);
    usb.gpio().rx.attachLink(linkTx);
    usb.gpio().tx.attachLink(linkRx);

    UsartMicroController micro(&esp8266);
    HardDisk             disk(&usb);

    micro.start();
    disk.start();

    return Application::exec();
}
