#include "nova_Win32Window.hpp"

#include <GameInput.h>
#include <imgui.h>
#include <nova/core/nova_Guards.hpp>

#include <initguid.h>
#include <cfgmgr32.h>
#include <devpkey.h>
#include <hidsdi.h>

#pragma comment(lib, "Cfgmgr32.lib")
#pragma comment(lib, "Hid.lib")

namespace nova
{
    static
    void ListHIDDevices()
    {
        u32 raw_device_count;
        GetRawInputDeviceList(nullptr, &raw_device_count, sizeof(RAWINPUTDEVICELIST));
        std::vector<RAWINPUTDEVICELIST> raw_devices(raw_device_count);
        GetRawInputDeviceList(raw_devices.data(), &raw_device_count, sizeof(RAWINPUTDEVICELIST));

        for (auto[id, raw_device] : raw_devices | Enumerate) {
            NOVA_STACK_POINT();
            NOVA_LOG("Raw device: {}", id);

            // Try and use the data in RID_DEVICE_INFO to associate `device` with a device from devices
            u32 rid_device_info_size = sizeof(RID_DEVICE_INFO);
            RID_DEVICE_INFO rid_device_info;
            if (auto res = GetRawInputDeviceInfo(raw_device.hDevice, RIDI_DEVICEINFO, &rid_device_info, &rid_device_info_size); res == ~0u || res == 0) {
                // NOVA_LOG("Skipping device because we failed to get the necessary info to associate it");
                continue;
            }
            NOVA_LOG("  product id: {}", rid_device_info.hid.dwProductId);
            NOVA_LOG("  vendor id: {}", rid_device_info.hid.dwVendorId);

            u32 device_path_size;
            GetRawInputDeviceInfo(raw_device.hDevice, RIDI_DEVICENAME, nullptr, &device_path_size);
            std::wstring device_path(device_path_size, 0);
            GetRawInputDeviceInfo(raw_device.hDevice, RIDI_DEVICENAME, device_path.data(), &device_path_size);

            NOVA_LOG("  device path: {}", NOVA_STACK_FROM_UTF16(device_path));

            {
                wchar_t product_str[1024] = {};
                wchar_t manufacturer_str[1024] = {};
                wchar_t serial_number[4093] = {};
                HANDLE h = CreateFile(device_path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                HidD_GetProductString(h, product_str, 1024);
                HidD_GetManufacturerString(h, manufacturer_str, 1024);
                HidD_GetSerialNumberString(h, serial_number, 4093);
                NOVA_LOG("  product: {}", NOVA_STACK_FROM_UTF16(product_str).data());
                NOVA_LOG("  manufacturer: {}", NOVA_STACK_FROM_UTF16(manufacturer_str).data());
                NOVA_LOG("  serial number: {}", NOVA_STACK_FROM_UTF16(serial_number).data());
                CloseHandle(h);
            }

            std::wstring friendly_name;
            std::wstring manufacturer_name;

            DEVPROPTYPE property_type;
            ULONG property_size = 0;
            CONFIGRET cr = CM_Get_Device_Interface_PropertyW(device_path.c_str(), &DEVPKEY_Device_InstanceId, &property_type, nullptr, &property_size, 0);

            std::wstring instance_id;
            instance_id.resize(property_size);
            cr = CM_Get_Device_Interface_PropertyW(device_path.c_str(), &DEVPKEY_Device_InstanceId, &property_type, reinterpret_cast<PBYTE>(instance_id.data()), &property_size, 0);
            NOVA_LOG("  instance id: {}", NOVA_STACK_FROM_UTF16(instance_id));

            DEVINST dev_inst;
            cr = CM_Locate_DevNodeW(&dev_inst, reinterpret_cast<DEVINSTID>(instance_id.data()), CM_LOCATE_DEVNODE_NORMAL);

            property_size = 0;

            cr = CM_Get_DevNode_PropertyW(dev_inst, &DEVPKEY_NAME, &property_type, nullptr, &property_size, 0);
            friendly_name.resize(property_size);
            cr = ::CM_Get_DevNode_PropertyW(dev_inst, &DEVPKEY_NAME, &property_type, reinterpret_cast<PBYTE>(friendly_name.data()), &property_size, 0);

            property_size = 0;

            cr = ::CM_Get_DevNode_PropertyW(dev_inst, &DEVPKEY_Device_ClassGuid, &property_type, nullptr, &property_size, 0);
            GUID* guid = (GUID*)NOVA_STACK_ALLOC(std::byte, property_size);
            cr = ::CM_Get_DevNode_PropertyW(dev_inst, &DEVPKEY_Device_ClassGuid, &property_type, (PBYTE)guid, &property_size, 0);

            OLECHAR* guid_str;
            StringFromCLSID(*guid, &guid_str);
            NOVA_LOG("  guid: {}", NOVA_STACK_FROM_UTF16(guid_str));
            ::CoTaskMemFree(guid_str);

            property_size = 0;

            cr = CM_Get_DevNode_PropertyW(dev_inst, &DEVPKEY_Device_Manufacturer, &property_type, nullptr, &property_size, 0);
            manufacturer_name.resize(property_size);
            cr = ::CM_Get_DevNode_PropertyW(dev_inst, &DEVPKEY_Device_Manufacturer, &property_type, reinterpret_cast<PBYTE>(manufacturer_name.data()), &property_size, 0);

            NOVA_LOG("  device name: {}", NOVA_STACK_FROM_UTF16(friendly_name));
            NOVA_LOG("  manufacturer name: {}", NOVA_STACK_FROM_UTF16(manufacturer_name));
        }
    }

    static
    void FetchDeviceName(Handle<Application>::Impl::GameInputState::GameInputDevice& device)
    {
        auto* info = device.handle->GetDeviceInfo();

        {
            NOVA_LOG("  product id: {}", info->productId);
            NOVA_LOG("  vendor id: {}", info->vendorId);

            u32 raw_device_count;
            GetRawInputDeviceList(nullptr, &raw_device_count, sizeof(RAWINPUTDEVICELIST));
            std::vector<RAWINPUTDEVICELIST> raw_devices(raw_device_count);
            GetRawInputDeviceList(raw_devices.data(), &raw_device_count, sizeof(RAWINPUTDEVICELIST));

            for (auto& raw_device : raw_devices) {
                // Try and use the data in RID_DEVICE_INFO to associate `device` with a device from devices
                u32 rid_device_info_size = sizeof(RID_DEVICE_INFO);
                RID_DEVICE_INFO rid_device_info;
                if (auto res = GetRawInputDeviceInfo(raw_device.hDevice, RIDI_DEVICEINFO, &rid_device_info, &rid_device_info_size); res == ~0u || res == 0) {
                    continue;
                }

                if (info->vendorId != rid_device_info.hid.dwVendorId || info->productId != rid_device_info.hid.dwProductId) {
                    continue;
                }

                u32 device_path_size;
                GetRawInputDeviceInfo(raw_device.hDevice, RIDI_DEVICENAME, nullptr, &device_path_size);
                std::wstring device_path(device_path_size, 0);
                GetRawInputDeviceInfo(raw_device.hDevice, RIDI_DEVICENAME, device_path.data(), &device_path_size);

                wchar_t product_string[1024] = {};
                wchar_t manufacturer_string[1024] = {};

                HANDLE h = CreateFile(device_path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                HidD_GetProductString(h, product_string, 1024);
                HidD_GetManufacturerString(h, manufacturer_string, 1024);
                device.name = NOVA_STACK_FROM_UTF16(product_string);
                device.manufacturer = NOVA_STACK_FROM_UTF16(manufacturer_string);
                CloseHandle(h);

                break;
            }
        }

        if (device.name.empty()) {
            if (info->deviceFamily == GameInputFamilyXboxOne) {
                device.name = "Xbox One Controller";
                device.manufacturer = "Microsoft";
            } else if (info->deviceFamily == GameInputFamilyXbox360) {
                device.name = "Xbox 360 Controller";
                device.manufacturer = "Microsoft";
            } else if (info->supportedInput == GameInputKindMouse) {
                device.name = "Generic Mouse";
                device.manufacturer = "Unknown Manufacturer";;
            } else if (info->supportedInput == GameInputKindKeyboard) {
                device.name = "Generic Keyboard";
                device.manufacturer = "Unknown Manufacturer";
            }
        }
    }

    static
    void CALLBACK DeviceCallback(
            _In_ GameInputCallbackToken /* callback_token */,
            _In_ void*                            context,
            _In_ IGameInputDevice*                 device,
            _In_ uint64_t                    /* timestamp */,
            _In_ GameInputDeviceStatus     current_status,
            _In_ GameInputDeviceStatus    previous_status)
    {

        auto app = Application(static_cast<Application::Impl*>(context));

        auto* info = device->GetDeviceInfo();

        NOVA_LOG("GameInput Device[{}]", static_cast<void*>(device));

        auto StatusString = [](GameInputDeviceStatus status) {
            std::string str;
            if (status & GameInputDeviceNoStatus) str += "NoStatus|";
            if (status & GameInputDeviceConnected) str += "Connected|";
            if (status & GameInputDeviceInputEnabled) str += "InputEnabled|";
            if (status & GameInputDeviceOutputEnabled) str += "OutputEnabled|";
            if (status & GameInputDeviceRawIoEnabled) str += "RawIoEnabled|";
            if (status & GameInputDeviceAudioCapture) str += "AudioCapture|";
            if (status & GameInputDeviceAudioRender) str += "AudioRender|";
            if (status & GameInputDeviceSynchronized) str += "Synchronized|";
            if (status & GameInputDeviceWireless) str += "Wireless|";
            if (status & GameInputDeviceUserIdle) str += "UserIdle|";
            if (!str.empty()) {
                str.pop_back();
            }

            return str;
        };

        NOVA_LOG("  status [{}] -> [{}]", StatusString(previous_status), StatusString(current_status));

        if (!(current_status & GameInputDeviceConnected)) {
            for (auto& reg : app->game_input.devices) {
                if (reg.handle == device) {
                    NOVA_LOG("  name = {}", reg.name);
                    break;
                }
            }
            std::erase_if(app->game_input.devices, [&](auto& registered) { return registered.handle == device; });
            return;
        }

        auto& registered_device = app->game_input.devices.emplace_back();
        registered_device.handle = device;
        registered_device.kinds = info->supportedInput;
        registered_device.key_states.resize(app->win32_input.to_win32_virtual_key.size());

        FetchDeviceName(registered_device);

        NOVA_LOG("  name = {}", registered_device.name);
        NOVA_LOG("  manufacturer = {}", registered_device.manufacturer);

        NOVA_LOG("  supported:");
        if (info->supportedInput & GameInputKindUnknown)          NOVA_LOG("    Unknown");
        if (info->supportedInput & GameInputKindRawDeviceReport)  NOVA_LOG("    RawDeviceReport");
        if (info->supportedInput & GameInputKindControllerAxis)   NOVA_LOG("    ControllerAxis");
        if (info->supportedInput & GameInputKindControllerButton) NOVA_LOG("    ControllerButton");
        if (info->supportedInput & GameInputKindControllerSwitch) NOVA_LOG("    ControllerSwitch");
        if (info->supportedInput & GameInputKindController)       NOVA_LOG("    Controller");
        if (info->supportedInput & GameInputKindKeyboard)         NOVA_LOG("    Keyboard");
        if (info->supportedInput & GameInputKindMouse)            NOVA_LOG("    Mouse");
        if (info->supportedInput & GameInputKindTouch)            NOVA_LOG("    Touch");
        if (info->supportedInput & GameInputKindMotion)           NOVA_LOG("    Motion");
        if (info->supportedInput & GameInputKindArcadeStick)      NOVA_LOG("    ArcadeStick");
        if (info->supportedInput & GameInputKindFlightStick)      NOVA_LOG("    FlightStick");
        if (info->supportedInput & GameInputKindGamepad)          NOVA_LOG("    Gamepad");
        if (info->supportedInput & GameInputKindRacingWheel)      NOVA_LOG("    RacingWheel");
        if (info->supportedInput & GameInputKindUiNavigation)     NOVA_LOG("    UiNavigation");
    }

    void Application::Impl::InitGameInput()
    {
        ListHIDDevices();

        NOVA_LOG("-----------------------------------");
        NOVA_LOG("-- Registering GameInput Devices --");
        NOVA_LOG("-----------------------------------");

        GameInputCreate(&game_input.handle);
        game_input.handle->RegisterDeviceCallback(
            nullptr,
            GameInputKindController
            | GameInputKindControllerAxis
            | GameInputKindControllerButton
            | GameInputKindControllerSwitch
            | GameInputKindKeyboard
            | GameInputKindMouse
            | GameInputKindGamepad
            | GameInputKindFlightStick
            | GameInputKindArcadeStick
            | GameInputKindRacingWheel,
            GameInputDeviceAnyStatus,
            GameInputBlockingEnumeration,
            this,
            DeviceCallback,
            &game_input.device_callback_token);
    }

    void Application::Impl::DestroyGameInput()
    {
        game_input.handle->UnregisterCallback(game_input.device_callback_token, UINT64_MAX);
        game_input.handle->Release();
    }

    void Application::DebugInputState() const
    {
        NOVA_STACK_POINT();

        auto& gi = impl->game_input;

        ImGui::Begin("Input Debug");
        NOVA_DEFER() { ImGui::End(); };

        {
            // Gamepad

            IGameInputReading* reading;
            if (SUCCEEDED(gi.handle->GetCurrentReading(GameInputKindGamepad, nullptr, &reading))) {
                GameInputGamepadState state;
                reading->GetGamepadState(&state);

                if (ImGui::CollapsingHeader("Gamepad##")) {
                    ImGui::Text("leftTrigger: %.4f", state.leftTrigger);
                    ImGui::Text("rightTrigger: %.4f", state.rightTrigger);
                    ImGui::Text("leftThumbstickX: %.4f", state.leftThumbstickX);
                    ImGui::Text("leftThumbstickY: %.4f", state.leftThumbstickY);
                    ImGui::Text("rightThumbstickX: %.4f", state.rightThumbstickX);
                    ImGui::Text("rightThumbstickY: %.4f", state.rightThumbstickY);

                    if (state.buttons & GameInputGamepadMenu) ImGui::Text("Menu");
                    if (state.buttons & GameInputGamepadView) ImGui::Text("View");
                    if (state.buttons & GameInputGamepadA) ImGui::Text("A");
                    if (state.buttons & GameInputGamepadB) ImGui::Text("B");
                    if (state.buttons & GameInputGamepadX) ImGui::Text("X");
                    if (state.buttons & GameInputGamepadY) ImGui::Text("Y");
                    if (state.buttons & GameInputGamepadDPadUp) ImGui::Text("DPadUp");
                    if (state.buttons & GameInputGamepadDPadDown) ImGui::Text("DPadDown");
                    if (state.buttons & GameInputGamepadDPadLeft) ImGui::Text("DPadLeft");
                    if (state.buttons & GameInputGamepadDPadRight) ImGui::Text("DPadRight");
                    if (state.buttons & GameInputGamepadLeftShoulder) ImGui::Text("LeftShoulder");
                    if (state.buttons & GameInputGamepadRightShoulder) ImGui::Text("RightShoulder");
                    if (state.buttons & GameInputGamepadLeftThumbstick) ImGui::Text("LeftThumbstick");
                    if (state.buttons & GameInputGamepadRightThumbstick) ImGui::Text("RightThumbstick");
                }

                reading->Release();
            }
        }

        u32 device_index = 0;
        for (auto& device : gi.devices) {
            IGameInputReading* reading;

            bool showing = ImGui::CollapsingHeader(NOVA_STACK_FORMAT("[{}]: {}", ++device_index, device.name).data());

            gi.handle->GetCurrentReading(device.kinds, device.handle, &reading);
            NOVA_DEFER(&) { reading->Release(); };

            // Mouse

            if (device.kinds & GameInputKindMouse) {
                auto& info = device.handle->GetDeviceInfo()->mouseInfo;

                GameInputMouseState state;
                reading->GetMouseState(&state);

                device.mouse_buttons[0] = state.buttons & info->supportedButtons & GameInputMouseLeftButton;
                device.mouse_buttons[1] = state.buttons & info->supportedButtons & GameInputMouseRightButton;
                device.mouse_buttons[2] = state.buttons & info->supportedButtons & GameInputMouseMiddleButton;
                device.mouse_buttons[3] = state.buttons & info->supportedButtons & GameInputMouseButton4;
                device.mouse_buttons[4] = state.buttons & info->supportedButtons & GameInputMouseButton5;
                device.mouse_buttons[5] = state.buttons & info->supportedButtons & GameInputMouseWheelTiltLeft;
                device.mouse_buttons[6] = state.buttons & info->supportedButtons & GameInputMouseWheelTiltRight;

                // Store fixed point?

                device.mouse_pos.x = f32(state.positionX);
                device.mouse_pos.y = f32(state.positionY);

                if (info->hasWheelX) device.mouse_wheel.x = f32(state.wheelX);
                if (info->hasWheelY) device.mouse_wheel.y = f32(state.wheelY);

                if (showing) {
                    ImGui::Checkbox("Mouse Left##", &device.mouse_buttons[0]);
                    ImGui::Checkbox("Mouse Right##", &device.mouse_buttons[1]);
                    ImGui::Checkbox("Mouse Middle##", &device.mouse_buttons[2]);
                    ImGui::Checkbox("Mouse Button 4##", &device.mouse_buttons[3]);
                    ImGui::Checkbox("Mouse Button 5##", &device.mouse_buttons[4]);
                    ImGui::Checkbox("Mouse Wheel Tilt Left##", &device.mouse_buttons[5]);
                    ImGui::Checkbox("Mouse Wheel Tilt Right##", &device.mouse_buttons[6]);

                    ImGui::Text("Mouse Pos:  (%.2f, %.2f)", device.mouse_pos.x, device.mouse_pos.y);
                    ImGui::Text("Mouse Scroll: (%.2f, %.2f)", device.mouse_wheel.x, device.mouse_wheel.y);
                }
            }

            // Keyboard

            if (device.kinds & GameInputKindKeyboard) {
                // Reset key states
                for (auto& state : device.key_states) state = false;

                // Query new set keys
                NOVA_STACK_POINT();
                auto key_states = NOVA_STACK_ALLOC(GameInputKeyState, reading->GetKeyCount());
                reading->GetKeyState(reading->GetKeyCount(), key_states);

                // Set pressed keys
                for (u32 i = 0; i < reading->GetKeyCount(); ++i) {
                    auto vk = impl->win32_input.from_win32_virtual_key[key_states[i].virtualKey];
                    device.key_states[vk] = true;
                }

                if (showing) {
                    for (auto key = 0; key < device.key_states.size(); ++key) {
                        if (device.key_states[key]) {
                            ImGui::Text("Key: %s", impl->win32_input.key_names[key].data());
                        }
                    }
                }
            }

            // Axis

            if (device.kinds & GameInputKindControllerAxis) {
                device.axis_states.resize(reading->GetControllerAxisCount());
                reading->GetControllerAxisState(reading->GetControllerAxisCount(), device.axis_states.data());

                if (showing) {
                    for (auto[id, axis] : device.axis_states | Enumerate) {
                        ImGui::Text("Axis[%i]: %.4f", id, axis);
                    }
                }
            }

            // Button

            if (device.kinds & GameInputKindControllerButton) {
                device.button_states.resize(reading->GetControllerButtonCount());
                reading->GetControllerButtonState(reading->GetControllerButtonCount(),
                    reinterpret_cast<bool*>(device.button_states.data()));

                if (showing) {
                    for (auto[id, state] : device.button_states | Enumerate) {
                        ImGui::Text("Button[%i]: %s", id, state ? "pressed" : "released");
                    }
                }
            }

            // Switch

            if (device.kinds & GameInputKindControllerSwitch) {
                device.switch_states.resize(reading->GetControllerSwitchCount());
                reading->GetControllerSwitchState(reading->GetControllerSwitchCount(), device.switch_states.data());

                if (showing) {
                    for (auto[id, state] : device.switch_states | Enumerate) {
                        const auto* name = "N/A";
                        switch (state) {
                            break;case GameInputSwitchCenter:    name = "Center";
                            break;case GameInputSwitchUp:        name = "Up";
                            break;case GameInputSwitchUpRight:   name = "UpRight";
                            break;case GameInputSwitchRight:     name = "Right";
                            break;case GameInputSwitchDownRight: name = "DownRight";
                            break;case GameInputSwitchDown:      name = "Down";
                            break;case GameInputSwitchDownLeft:  name = "DownLeft";
                            break;case GameInputSwitchLeft:      name = "Left";
                            break;case GameInputSwitchUpLeft:    name = "UpLeft";
                        }
                        ImGui::Text("Switch[%i]: %s", id, name);
                    }
                }
            }
        }
    }
}
