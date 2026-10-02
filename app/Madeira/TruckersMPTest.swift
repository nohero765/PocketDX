// SPDX-License-Identifier: GPL-3.0-or-later
import SwiftUI
import CryptoKit

enum TruckersMPTest {
    static let appID = 227300
    static let dll = "C:\\TruckersMP\\core_ets2mp.dll"

    static func configure() {
        setenv("MADEIRA_START_DLL_EXE", "eurotrucks2.exe", 1)
        setenv("MADEIRA_START_DLL", dll, 1)
        setenv("MADEIRA_TARGET_ARGS", "-rdevice dx11 -nointro -64bit", 1)
        setenv("WINEDLLOVERRIDES", "d3dcompiler_47=n,b", 1)
    }

    static func clear() {
        for key in ["MADEIRA_START_DLL_EXE", "MADEIRA_START_DLL", "MADEIRA_TARGET_ARGS", "WINEDLLOVERRIDES"] { unsetenv(key) }
    }
}

actor TruckersMPPayload {
    static let shared = TruckersMPPayload()
    private var busy = false

    struct Failure: LocalizedError {
        let errorDescription: String?
        init(_ text: String) { errorDescription = text }
    }

    func prepare(drive: URL) throws {
        guard !busy else { throw Failure("TruckersMP is being prepared.") }
        busy = true
        defer { busy = false }
        guard let source = Bundle.main.url(forResource: "TruckersMP", withExtension: nil),
              let manifest = try? Data(contentsOf: source.appendingPathComponent("payload.json")),
              FileManager.default.fileExists(atPath: source.appendingPathComponent("core_ets2mp.dll").path) else {
            throw Failure("TruckersMP files are missing from this IPA.")
        }
        let fm = FileManager.default
        let destination = drive.appendingPathComponent("TruckersMP", isDirectory: true)
        let stamp = Data(SHA256.hash(data: manifest).map { String(format: "%02x", $0) }.joined().utf8)
        if (try? Data(contentsOf: destination.appendingPathComponent(".payload-stamp"))) == stamp,
           fm.fileExists(atPath: destination.appendingPathComponent("core_ets2mp.dll").path) {
            try prepareCompiler(payload: destination, drive: drive)
            return
        }
        guard fm.fileExists(atPath: drive.path) else { throw Failure("Prepare Steam first.") }
        let values = try drive.resourceValues(forKeys: [.volumeAvailableCapacityForImportantUsageKey])
        if let free = values.volumeAvailableCapacityForImportantUsage, free < 1_100_000_000 {
            throw Failure("Free at least 1.1 GB to prepare TruckersMP.")
        }
        let stage = drive.appendingPathComponent(".truckersmp-stage-" + UUID().uuidString)
        let backup = drive.appendingPathComponent(".truckersmp-backup-" + UUID().uuidString)
        defer { try? fm.removeItem(at: stage) }
        try fm.copyItem(at: source, to: stage)
        try stamp.write(to: stage.appendingPathComponent(".payload-stamp"), options: .atomic)
        let existed = fm.fileExists(atPath: destination.path)
        if existed { try fm.moveItem(at: destination, to: backup) }
        do { try fm.moveItem(at: stage, to: destination) }
        catch {
            if existed { try? fm.moveItem(at: backup, to: destination) }
            throw error
        }
        if existed { try? fm.removeItem(at: backup) }
        try prepareCompiler(payload: destination, drive: drive)
    }

    private func prepareCompiler(payload: URL, drive: URL) throws {
        let fm = FileManager.default
        let source = payload.appendingPathComponent("runtime/d3dcompiler_47.dll")
        let target = drive.appendingPathComponent("windows/system32/d3dcompiler_47.dll")
        let temporary = target.deletingLastPathComponent().appendingPathComponent(".d3dcompiler_47-" + UUID().uuidString)
        try fm.copyItem(at: source, to: temporary)
        defer { try? fm.removeItem(at: temporary) }
        // Remove the prefix's link itself, never write through it into the bundle.
        if fm.fileExists(atPath: target.path) || (try? fm.destinationOfSymbolicLink(atPath: target.path)) != nil {
            try fm.removeItem(at: target)
        }
        try fm.moveItem(at: temporary, to: target)
    }
}

struct TruckersMPTestView: View {
    let launch: () -> Void
    let enableJIT: () -> Void
    let preparingPayload: Bool
    @ObservedObject private var dock = MadeiraDockModel.shared
    @ObservedObject private var steam = SteamOwnedLibrary.shared
    @ObservedObject private var library = LibraryModel.shared
    @State private var showSignIn = false
    @State private var jitReady = false

    private var installed: Bool { dock.games.contains { $0.id == TruckersMPTest.appID && $0.installed } }

    var body: some View {
        Form {
            Section {
                LabeledContent("JIT", value: jitReady ? "Ready" : "Required")
                if !jitReady { Button("Enable JIT", action: enableJIT) }
                LabeledContent("Steam", value: SteamSignIn.isSignedIn ? "Signed in" : "Sign in required")
                if !SteamSignIn.isSignedIn { Button("Sign in to Steam") { showSignIn = true } }
                if !dock.clientInstalled {
                    Button("Prepare Steam") { dock.prepareClient() }.disabled(dock.preparing)
                }
                if dock.preparing { ProgressView(dock.progress) }
                LabeledContent("ETS2", value: installed ? "Installed" : "Install required")
                Button(installed ? "Update ETS2" : "Install ETS2") { steam.install(TruckersMPTest.appID) }
                    .disabled(!steam.signedIn || !dock.clientInstalled || dock.preparing ||
                              steam.refreshing || steam.hasActiveDownload)
                if let download = steam.downloads[TruckersMPTest.appID] {
                    SteamDownloadStatus(download: download)
                    if case .paused = download.state {
                        Button("Resume ETS2 download") { steam.install(TruckersMPTest.appID) }
                    }
                    if case .failed = download.state {
                        Button("Retry ETS2 download") { steam.install(TruckersMPTest.appID) }
                    }
                }
            }
            Section {
                Button(action: launch) {
                    HStack {
                        if preparingPayload { ProgressView() }
                        Text(preparingPayload ? "Preparing TruckersMP…" : "Launch TruckersMP")
                    }.frame(maxWidth: .infinity)
                }
                .buttonStyle(.borderedProminent)
                .disabled(!jitReady || !installed || !SteamSignIn.isSignedIn || !dock.clientInstalled ||
                          preparingPayload || dock.preparing || steam.hasActiveDownload)
            }
            if let error = library.error ?? dock.error ?? steam.error {
                Section { Text(error).foregroundStyle(.red).textSelection(.enabled) }
            }
        }
        .sheet(isPresented: $showSignIn) { SteamSignInView() }
        .task {
            steam.start()
            dock.refresh()
            jitReady = jit_check_debugged()
        }
        .onReceive(NotificationCenter.default.publisher(for: UIApplication.didBecomeActiveNotification)) { _ in
            jitReady = jit_check_debugged()
            dock.refresh()
        }
        .onReceive(NotificationCenter.default.publisher(for: SteamSignIn.didChange)) { _ in dock.refresh() }
        .onChange(of: steam.downloads) { _, _ in dock.refresh() }
    }
}

struct TruckersMPTestHUD: View {
    @ObservedObject private var library = LibraryModel.shared
    @ObservedObject private var dock = MadeiraDockModel.shared

    var body: some View {
        VStack {
            HStack {
                Button("Keyboard", systemImage: "keyboard") { MetalBackedView.toggleKeyboard() }
                Spacer()
                Button("Close", systemImage: "xmark") { library.requestQuit() }
            }
            .labelStyle(.iconOnly).buttonStyle(.bordered).padding()
            if library.launching {
                ProgressView(dock.status ?? "Starting TruckersMP…")
                    .padding().background(.regularMaterial, in: .rect(cornerRadius: 12))
            }
            Spacer()
        }
    }
}
