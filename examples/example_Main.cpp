#include "example_Main.hpp"

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

int main(int argc, char* argv[])
{
    std::sort(GetExamples().begin(), GetExamples().end(), [](const auto& l, const auto& r) {
        return std::lexicographical_compare(
            l.name, l.name + strlen(l.name),
            r.name, r.name + strlen(r.name));
    });

    if (argc > 1) {
        for (auto& example : GetExamples()) {
            if (!strcmp(example.name, argv[1])) {
                example.fn();
                return EXIT_SUCCESS;
            }
        }
    }

    NOVA_LOG("Examples:");
    for (auto& example : GetExamples())
        NOVA_LOG(" - {}", example.name);
}