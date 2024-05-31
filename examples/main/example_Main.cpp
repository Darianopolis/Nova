#include "example_Main.hpp"

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

int main()
{
    std::sort(GetExamples().begin(), GetExamples().end(), [](const auto& l, const auto& r) {
        return std::lexicographical_compare(
            l.name.Data(), l.name.Data() + l.name.Size(),
            r.name.Data(), r.name.Data() + r.name.Size());
    });

    auto args = nova::env::ParseCmdLineArgs(nova::env::GetCmdLineArgs());
    args.erase(args.begin());

    for (;;) {
        for (auto& example : GetExamples()) {
            if (example.name == args[0]) {
                try {
                    std::vector<nova::StringView> arg_view(&args[1], &args[1] + std::max(0ull, args.size() - 1));
                    example.fn(arg_view);

                    nova::Log("\nExample({}) exited successfully", example.name);

                    return EXIT_SUCCESS;
                } catch (std::exception& e) {
                    // nova::Exception logs on creation, don't duplicate
                    if (!dynamic_cast<nova::Exception*>(&e)) {
                        nova::Log("\n────────────────────────────────────────────────────────────────────────────────");
                        nova::Log(  "Error: {}", e.what());
                        nova::Log(  "────────────────────────────────────────────────────────────────────────────────");
                    }
                } catch (...) {
                    nova::Log("\nUnknown Error");
                }

                return EXIT_FAILURE;
            }
        }

        nova::Log("Examples:");
        for (auto& example : GetExamples()) {
            nova::Log(" - {}", example.name);
        }

        std::cout << "> " << std::flush;

        std::string arg_str;
        std::getline(std::cin, arg_str);

        args = nova::env::ParseCmdLineArgs(arg_str);
    }

    return EXIT_FAILURE;
}