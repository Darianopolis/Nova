#include "example_Main.hpp"

#include <nova/core/nova_Debug.hpp>
#include <nova/core/nova_Strings.hpp>

std::vector<ExampleListing>& GetExamples()
{
    static std::vector<ExampleListing> examples;
    return examples;
}

std::monostate RegisterExample(nova::StringView name, ExampleEntryFnPtr fn)
{
    GetExamples().emplace_back(name, fn);
    return {};
}

int main(int argc, char* argv[]) try
{
    std::sort(GetExamples().begin(), GetExamples().end(), [](const auto& l, const auto& r) {
        return std::lexicographical_compare(
            l.name.Data(), l.name.Data() + l.name.Size(),
            r.name.Data(), r.name.Data() + r.name.Size());
    });

    if (argc > 1) {
        for (auto& example : GetExamples()) {
            if (example.name == argv[1]) {
                try {
                    std::vector<nova::StringView> args(&argv[2], &argv[2] + std::max(0, argc - 2));
                    example.fn(args);

                    NOVA_LOG("Example({}) exited successfully", example.name);
                } catch (std::exception& e) {
                    NOVA_LOG("────────────────────────────────────────────────────────────────────────────────");
                    if (auto* ne = dynamic_cast<nova::Exception*>(&e)) {
                        NOVA_LOG("Location: {}({})", ne->location().file_name(), ne->location().line());
                        NOVA_LOG("────────────────────────────────────────────────────────────────────────────────");
                        NOVA_LOG("{}", std::to_string(ne->stack()));
                        NOVA_LOG("────────────────────────────────────────────────────────────────────────────────");
                    }
                    NOVA_LOG("Error: {}", e.what());
                    NOVA_LOG("────────────────────────────────────────────────────────────────────────────────");
                } catch (...) {
                    NOVA_LOG("Unknown Error");
                }
                return EXIT_SUCCESS;
            }
        }
    }

    NOVA_LOG("Examples:");
    for (auto& example : GetExamples())
        NOVA_LOG(" - {}", example.name);
}
catch (const std::exception& e) {
    NOVA_LOG("Error: {}", e.what());
}