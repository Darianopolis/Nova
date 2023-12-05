#include <nova/core/nova_Core.hpp>
#include <nova/core/nova_Containers.hpp>

using namespace nova::types;

using ExampleEntryFnPtr = void(*)(nova::Span<std::string_view>);

struct ExampleListing
{
    const char* name;
    ExampleEntryFnPtr fn;
};

std::vector<ExampleListing>& GetExamples();
std::monostate RegisterExample(const char* name, ExampleEntryFnPtr fn);

#define NOVA_EXAMPLE(name, strName) \
    void example_##name(nova::Span<std::string_view> args); \
    static auto example_##name##_state = RegisterExample(strName, example_##name); \
    void example_##name(nova::Span<std::string_view> args)
