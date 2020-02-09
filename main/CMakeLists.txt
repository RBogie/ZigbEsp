set(COMPONENT_SRCS "main.cpp z-stack/Controller.cpp z-stack/Device.cpp z-stack/Endpoint.cpp z-stack/serial.cpp z-stack/zcl.cpp z-stack/ZclFrame.cpp z-stack/znp.cpp z-stack/ZnpFrame.cpp z-stack/ZStack.cpp http/firmware.cpp")
set(COMPONENT_ADD_INCLUDEDIRS "")

register_component()
component_compile_options(-std=c++14)