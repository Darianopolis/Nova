#include "example_Main.hpp"

int main(int argc, char* argv[])
{
    if (argc > 1) {
        for (auto& example : Examples) {
            if (!strcmp(example.name, argv[1])) {
                example.fn();
                return EXIT_SUCCESS;
            }
        }
    }

    NOVA_LOG("Examples:");
    for (auto& example : Examples)
        NOVA_LOG(" - {}", example.name);
}