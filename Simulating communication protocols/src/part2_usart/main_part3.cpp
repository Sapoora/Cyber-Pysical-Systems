#include <CPS4042/Hardwares/Boards/Esp8266.h>
#include <CPS4042/Hardwares/Comm/I2CMultiplexer.h>
#include <CPS4042/Hardwares/Sensors/VL530X.h>
#include <CPS4042/Sketchs/I2CMuxSketch.h>
#include <CPS4042/Sketchs/MuxMicrocontroller.h>
#include <CPS4042/Sketchs/RangeSensor.h>
#include <CPS4042/Wires/Link.h>
#include <CPS4042/main.h>

std::int32_t
main()
{
    Boards::Esp8266          esp8266;
    Sensors::I2CMultiplexer i2cMux;
    Sensors::Vl530x         sensorA;
    Sensors::Vl530x         sensorB;

    auto linkMuxVdd = std::make_shared<Link>();
    auto linkMuxGnd = std::make_shared<Link>();
    auto linkTx     = std::make_shared<Link>();
    auto linkRx     = std::make_shared<Link>();

    auto linkA_Vdd = std::make_shared<Link>();
    auto linkA_Gnd = std::make_shared<Link>();
    auto linkA_Scl = std::make_shared<Link>();
    auto linkA_Sda = std::make_shared<Link>();

    auto linkB_Vdd = std::make_shared<Link>();
    auto linkB_Gnd = std::make_shared<Link>();
    auto linkB_Scl = std::make_shared<Link>();
    auto linkB_Sda = std::make_shared<Link>();

    CPS_SET_OBJECT_NAME(esp8266);
    CPS_SET_OBJECT_NAME(i2cMux);
    CPS_SET_OBJECT_NAME(sensorA);
    CPS_SET_OBJECT_NAME(sensorB);

    CPS_SET_OBJECT_NAME_PTR(linkMuxVdd);
    CPS_SET_OBJECT_NAME_PTR(linkMuxGnd);
    CPS_SET_OBJECT_NAME_PTR(linkTx);
    CPS_SET_OBJECT_NAME_PTR(linkRx);

    CPS_SET_OBJECT_NAME_PTR(linkA_Vdd);
    CPS_SET_OBJECT_NAME_PTR(linkA_Gnd);
    CPS_SET_OBJECT_NAME_PTR(linkA_Scl);
    CPS_SET_OBJECT_NAME_PTR(linkA_Sda);

    CPS_SET_OBJECT_NAME_PTR(linkB_Vdd);
    CPS_SET_OBJECT_NAME_PTR(linkB_Gnd);
    CPS_SET_OBJECT_NAME_PTR(linkB_Scl);
    CPS_SET_OBJECT_NAME_PTR(linkB_Sda);

    // Microcontroller <-> MUX over USART.
    esp8266.gpio().vdd2.attachLink(linkMuxVdd);
    esp8266.gpio().gnd2.attachLink(linkMuxGnd);
    esp8266.gpio().tx.attachLink(linkTx);
    esp8266.gpio().rx.attachLink(linkRx);

    i2cMux.gpio().vdd.attachLink(linkMuxVdd);
    i2cMux.gpio().gnd.attachLink(linkMuxGnd);
    i2cMux.gpio().rx.attachLink(linkTx);
    i2cMux.gpio().tx.attachLink(linkRx);

    // MUX C2I channel 0 <-> first VL530X sensor.
    i2cMux.gpio().vdd0.attachLink(linkA_Vdd);
    i2cMux.gpio().gnd0.attachLink(linkA_Gnd);
    i2cMux.gpio().scl0.attachLink(linkA_Scl);
    i2cMux.gpio().sda0.attachLink(linkA_Sda);

    sensorA.gpio().vdd.attachLink(linkA_Vdd);
    sensorA.gpio().gnd.attachLink(linkA_Gnd);
    sensorA.gpio().scl.attachLink(linkA_Scl);
    sensorA.gpio().sda.attachLink(linkA_Sda);

    // MUX C2I channel 1 <-> second VL530X sensor.
    i2cMux.gpio().vdd1.attachLink(linkB_Vdd);
    i2cMux.gpio().gnd1.attachLink(linkB_Gnd);
    i2cMux.gpio().scl1.attachLink(linkB_Scl);
    i2cMux.gpio().sda1.attachLink(linkB_Sda);

    sensorB.gpio().vdd.attachLink(linkB_Vdd);
    sensorB.gpio().gnd.attachLink(linkB_Gnd);
    sensorB.gpio().scl.attachLink(linkB_Scl);
    sensorB.gpio().sda.attachLink(linkB_Sda);

    MuxMicroController micro(&esp8266);
    I2CMuxSketch       muxSketch(&i2cMux);
    RangeSensor        sensorSketchA(&sensorA, 0, 20, "SensorA[0..20]");
    RangeSensor        sensorSketchB(&sensorB, 50, 100, "SensorB[50..100]");

    sensorSketchA.start();
    sensorSketchB.start();
    muxSketch.start();
    micro.start();

    return Application::exec();
}
