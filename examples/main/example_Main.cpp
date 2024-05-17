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

                    nova::Log("Example({}) exited successfully", example.name);
                } catch (std::exception& e) {
                    nova::Log("────────────────────────────────────────────────────────────────────────────────");
                    if (auto* ne = dynamic_cast<nova::Exception*>(&e)) {
                        nova::Log("Location: {}({})", ne->location().file_name(), ne->location().line());
                        nova::Log("────────────────────────────────────────────────────────────────────────────────");
                        nova::Log("{}", std::to_string(ne->stack()));
                        nova::Log("────────────────────────────────────────────────────────────────────────────────");
                    }
                    nova::Log("Error: {}", e.what());
                    nova::Log("────────────────────────────────────────────────────────────────────────────────");
                } catch (...) {
                    nova::Log("Unknown Error");
                }
                return EXIT_SUCCESS;
            }
        }
    }

    nova::Log("Examples:");
    for (auto& example : GetExamples())
        nova::Log(" - {}", example.name);
}
catch (const std::exception& e) {
    nova::Log("Error: {}", e.what());
}