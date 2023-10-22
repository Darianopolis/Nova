#include "nova_Window.hpp"

namespace nova
{
    EventSystem* EventSystem::Create()
    {
        return new EventSystem;
    }

    void EventSystem::Destroy(EventSystem* eventSystem)
    {
        delete eventSystem;
    }

    void EventSystem::WaitEvents(Span<Window*> windows)
    {
        (void)windows;

        glfwWaitEvents();
    }

    void EventSystem::PollEvents(Span<Window*> windows)
    {
        (void)windows;

        glfwPollEvents();
    }
}