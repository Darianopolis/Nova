#include "example_Main.hpp"

int main(int argc, char* argv[])
{
    bool failed = true;
    
    if (argc > 1) {
        for (auto& example : Examples) {
            if (!strcmp(example.name, argv[1])) {
                example.fn();
                failed = false;
                break;
            }
        }
    }

    if (failed) {
        NOVA_LOG("Examples:");
        for (auto& example : Examples)
            NOVA_LOG(" - {}", example.name);
    }
}