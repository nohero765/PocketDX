import SwiftUI

@main
struct MadeiraApp: App {
    init() {
        UserDefaults.standard.set("new", forKey: "madeiraFrontend")
        UserDefaults.standard.set(true, forKey: "MetalForceHudEnabled")
        setenv("MTL_HUD_ENABLED", "1", 1)
    }

    var body: some Scene {
        WindowGroup {
            ContentView()
                .modifier(ClaimGamepadEvents())
                .onAppear {
                    GamepadInput.shared.start()
                    HardwareInput.shared.start()
                }
        }
    }
}
