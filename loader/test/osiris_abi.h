// The test's own statement of libOsiris's interface, as C++ declarations: the compiler
// mangles these, so a typo in the loader's asm() labels fails the smoke test. The
// struct layouts, however, are the same reading of the disassembly the loader uses —
// the test cannot catch an error there.
#pragma once
#include <cstdint>

class COsiSmartBuf;

class COsiArgumentDesc {
public:
    bool IsStringType() const;
    COsiArgumentDesc* next = nullptr;
    union {
        const char* string;
        int32_t int32;
        int64_t int64 = 0;
        float real;
    };
    uint16_t type = 0;
    bool flag = false;
};

struct MappingInfo {
    const char* name;
    uint32_t id;
    uint32_t num_params;
    uint8_t* param_types;
};

struct TOsirisInitFunction {
    uint32_t (*get_handle)(uint32_t, uint32_t, uint32_t, uint32_t);
    bool (*call)(uint32_t, COsiArgumentDesc*);
    bool (*query)(uint32_t, COsiArgumentDesc*);
    void (*error)(const char*);
    void (*assert_)(bool, const char*, bool);
};

class COsiris {
public:
    void RegisterDIVFunctions(TOsirisInitFunction*);
    void GetFunctionMappings(MappingInfo**, unsigned int*);
    bool InitGame();
    bool Load(COsiSmartBuf&);
    bool Merge(wchar_t const*);
    int Event(unsigned int, COsiArgumentDesc*);
};
