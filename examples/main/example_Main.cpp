#include "example_Main.hpp"

#include <nova/core/nova_Debug.hpp>

std::vector<ExampleListing>& GetExamples()
{
    static std::vector<ExampleListing> examples;
    return examples;
}

std::monostate RegisterExample(const char* name, void(*fn)())
{
    GetExamples().emplace_back(name, fn);
    return {};
}

int main(int argc, char* argv[]) try
{
    std::sort(GetExamples().begin(), GetExamples().end(), [](const auto& l, const auto& r) {
        return std::lexicographical_compare(
            l.name, l.name + strlen(l.name),
            r.name, r.name + strlen(r.name));
    });

    if (argc > 1) {
        for (auto& example : GetExamples()) {
            if (!strcmp(example.name, argv[1])) {
                try {
                    example.fn();
                } catch (std::exception& e) {
                    NOVA_LOG("--------------------------------------------------------------------------------");
                    if (auto* ne = dynamic_cast<nova::Exception*>(&e)) {
                        NOVA_LOG("Location: {}({})", ne->location().file_name(), ne->location().line());
                        NOVA_LOG("--------------------------------------------------------------------------------");
                        std::cout << ne->stack() << '\n';
                        NOVA_LOG("--------------------------------------------------------------------------------");
                    }
                    NOVA_LOG("Error: {}", e.what());
                    NOVA_LOG("--------------------------------------------------------------------------------");
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