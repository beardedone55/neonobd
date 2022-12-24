#pragma once

namespace neon {
    enum InterfaceType {
        BLUETOOTH_IF = 0,
        SERIAL_IF = 1
    };

    enum ResponseType { 
        USER_YN,
        USER_STRING,
        USER_INT,
        USER_NONE
    };

};
