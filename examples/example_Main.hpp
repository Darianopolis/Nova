#include <nova/core/nova_Core.hpp>

using namespace nova::types;

struct ExampleListing
{
    const char* name;
    void(*fn)();
};

std::vector<ExampleListing>& GetExamples();
std::monostate RegisterExample(const char* name, void(*fn)());

#define NOVA_EXAMPLE(name, strName) \
    void example_##name(); \
    static auto example_##name##_state = RegisterExample(strName, example_##name); \
    void example_##name()
