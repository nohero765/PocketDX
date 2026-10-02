// SPDX-License-Identifier: GPL-3.0-or-later
import Foundation

/// Imports the user's Windows install and its real Steam install record.
/// Depot/build metadata is preserved so Steam does not treat it as a new download.
actor ETS2Import {
    static let shared = ETS2Import()
    private var busy = false

    struct Failure: LocalizedError {
        let errorDescription: String?
        init(_ text: String) { errorDescription = text }
    }

    func importFolder(_ selected: URL, drive: URL,
                      progress: @Sendable (Double) -> Void = { _ in }) throws {
        guard !busy else { throw Failure("ETS2 is already being imported.") }
        busy = true
        defer { busy = false }
        let scoped = selected.startAccessingSecurityScopedResource()
        defer { if scoped { selected.stopAccessingSecurityScopedResource() } }
        let fm = FileManager.default
        let manifest = selected.appendingPathComponent("appmanifest_227300.acf")
        guard let size = try? manifest.resourceValues(forKeys: [.fileSizeKey]).fileSize,
              size <= 1_048_576, let record = try? Data(contentsOf: manifest) else {
            throw Failure("Select Steam's steamapps folder, or include appmanifest_227300.acf inside your ETS2 folder.")
        }
        var parser = try SteamKeyValues(record)
        let state = try parser.read()["AppState"]
        guard state?["appid"]?.string == "227300",
              let flags = state?["StateFlags"]?.string.flatMap(UInt32.init), flags & 4 != 0,
              let build = state?["buildid"]?.string.flatMap(UInt64.init), build > 0,
              let depots = state?["InstalledDepots"]?.fields, !depots.isEmpty,
              depots.values.allSatisfy({ ($0["manifest"]?.string.flatMap(UInt64.init) ?? 0) > 0 }),
              let folder = state?["installdir"]?.string,
              !folder.isEmpty, folder != ".", folder != "..",
              !folder.contains("/"), !folder.contains("\\"), !folder.contains(":"),
              !folder.unicodeScalars.contains(where: { $0.value < 32 }) else {
            throw Failure("Use the appmanifest_227300.acf from a complete Steam ETS2 installation.")
        }
        let nested = selected.appendingPathComponent("common/" + folder, isDirectory: true)
        let source = (fm.fileExists(atPath: nested.path) ? nested : selected).standardizedFileURL
        try Self.validateGame(source)
        let steamApps = drive.appendingPathComponent("Program Files (x86)/Steam/steamapps", isDirectory: true)
        let common = steamApps.appendingPathComponent("common", isDirectory: true)
        let target = common.appendingPathComponent(folder, isDirectory: true)
        let sourcePath = source.resolvingSymlinksInPath().standardizedFileURL.path
        let targetPath = target.resolvingSymlinksInPath().standardizedFileURL.path
        guard sourcePath != targetPath, !targetPath.hasPrefix(sourcePath + "/") else {
            throw Failure("Select the original ETS2 folder outside this app's Wine installation.")
        }
        guard fm.fileExists(atPath: drive.path) else { throw Failure("Prepare Steam before importing ETS2.") }
        let keys: Set<URLResourceKey> = [.isDirectoryKey, .isRegularFileKey, .isSymbolicLinkKey, .fileSizeKey]
        var readError: Error?
        guard let walk = fm.enumerator(at: source, includingPropertiesForKeys: Array(keys),
                                      errorHandler: { _, error in readError = error; return false }) else {
            throw Failure("Cannot read the ETS2 folder.")
        }
        var entries: [(URL, String, Bool, Int64)] = []
        var bytes: Int64 = 0
        for case let url as URL in walk {
            let values = try url.resourceValues(forKeys: keys)
            guard values.isSymbolicLink != true else { throw Failure("Import a folder with actual game files, not symbolic links.") }
            guard values.isDirectory == true || values.isRegularFile == true else { continue }
            let relative = String(url.path.dropFirst(source.path.count + 1))
            let size = Int64(values.fileSize ?? 0)
            if values.isRegularFile == true { bytes += size }
            entries.append((url, relative, values.isDirectory == true, size))
        }
        if let readError { throw readError }
        if let free = try Self.availableBytes(on: drive), free < bytes + 1_100_000_000 {
            throw Failure("Free space is needed for the ETS2 copy and 1.1 GB of TruckersMP files.")
        }
        try fm.createDirectory(at: common, withIntermediateDirectories: true)
        let stage = common.appendingPathComponent(".ets2-import-" + UUID().uuidString)
        let backup = common.appendingPathComponent(".ets2-backup-" + UUID().uuidString)
        defer { try? fm.removeItem(at: stage) }
        try fm.createDirectory(at: stage, withIntermediateDirectories: false)
        var copied: Int64 = 0
        var lastUpdate = Date.distantPast
        progress(0)
        for (url, relative, directory, size) in entries {
            let destination = stage.appendingPathComponent(relative)
            if directory { try fm.createDirectory(at: destination, withIntermediateDirectories: true) }
            else {
                try fm.copyItem(at: url, to: destination)
                copied += size
                if Date().timeIntervalSince(lastUpdate) >= 0.2 {
                    progress(Double(copied) / Double(max(bytes, 1)))
                    lastUpdate = Date()
                }
            }
        }
        try Self.validateGame(stage)
        let existed = fm.fileExists(atPath: target.path)
        if existed { try fm.moveItem(at: target, to: backup) }
        do {
            try fm.moveItem(at: stage, to: target)
            // Write last: discovery must never advertise an incomplete copy.
            try record.write(to: steamApps.appendingPathComponent("appmanifest_227300.acf"), options: .atomic)
        } catch {
            try? fm.removeItem(at: target)
            if existed { try? fm.moveItem(at: backup, to: target) }
            throw error
        }
        if existed { try? fm.removeItem(at: backup) }
        progress(1)
    }

    static func availableBytes(on volume: URL) throws -> Int64? {
        let values = try volume.resourceValues(forKeys: [.volumeAvailableCapacityForImportantUsageKey, .volumeAvailableCapacityKey])
        if let important = values.volumeAvailableCapacityForImportantUsage, important > 0 { return important }
        // Some providers report zero for the important-usage estimate even
        // when the ordinary free-space value is available.
        return values.volumeAvailableCapacity.map(Int64.init)
    }

    static func validateGame(_ folder: URL) throws {
        for name in ["base.scs", "def.scs", "bin/win_x64/eurotrucks2.exe",
                     "bin/win_x64/fmod.dll", "bin/win_x64/fmodstudio.dll", "bin/win_x64/steam_api64.dll"] {
            let values = try? folder.appendingPathComponent(name).resourceValues(forKeys: [.isRegularFileKey, .isSymbolicLinkKey, .fileSizeKey])
            guard values?.isRegularFile == true, values?.isSymbolicLink != true, (values?.fileSize ?? 0) > 0 else {
                throw Failure("ETS2 is missing \(name). Import the complete Windows installation.")
            }
        }
        let file = try FileHandle(forReadingFrom: folder.appendingPathComponent("bin/win_x64/eurotrucks2.exe"))
        defer { try? file.close() }
        let dos = [UInt8](try file.read(upToCount: 64) ?? Data())
        guard dos.count == 64, dos[0] == 0x4d, dos[1] == 0x5a else { throw Failure("ETS2 must be the Windows 64-bit version.") }
        let offset = (0..<4).reduce(UInt64(0)) { $0 | UInt64(dos[60 + $1]) << ($1 * 8) }
        guard (64...1_048_576).contains(offset) else { throw Failure("ETS2 has an invalid Windows executable header.") }
        try file.seek(toOffset: offset)
        let pe = [UInt8](try file.read(upToCount: 26) ?? Data())
        guard pe.count == 26, pe.prefix(4).elementsEqual([0x50, 0x45, 0, 0]),
              pe[4] == 0x64, pe[5] == 0x86, pe[24] == 0x0b, pe[25] == 0x02 else {
            throw Failure("ETS2 must be the Windows 64-bit version.")
        }
    }
}
