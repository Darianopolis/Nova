#include <nova/core/nova_Core.hpp>

using namespace nova::types;

struct Example 
{ 
    const char* name; 
    void(*fn)(); 
};

void example_Compute();
void example_Draw();
void example_MultiTarget();
void example_RayTracing();
void example_Triangle();

static constexpr std::array Examples {
    Example{ "compute", example_Compute     },
    Example{ "draw",    example_Draw        },
    Example{ "multi",   example_MultiTarget },
    Example{ "rt",      example_RayTracing  },
    Example{ "tri",     example_Triangle    },
};