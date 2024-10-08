#include <nova/core/Core.hpp>

using namespace nova::types;

using ExampleEntryFnPtr = void(*)(nova::Span<nova::StringView>);

struct ExampleListing
{
    nova::StringView name;
    ExampleEntryFnPtr  fn;
};

std::vector<ExampleListing>& GetExamples();
std::monostate RegisterExample(nova::StringView, ExampleEntryFnPtr fn);

#define NOVA_EXAMPLE(name, strName) \
    void example_##name(nova::Span<nova::StringView> args); \
    static auto NOVA_UNIQUE_VAR() = RegisterExample(strName, example_##name); \
    void example_##name([[maybe_unused]] nova::Span<nova::StringView> args)
