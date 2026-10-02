// SPDX-License-Identifier: GPL-3.0-or-later
import Foundation

@main struct ImportChecks {
    static func main() async throws {
        let fm = FileManager.default
        let root = URL(fileURLWithPath: CommandLine.arguments[1]).appendingPathComponent("ets2-import-tests-" + UUID().uuidString)
        defer { try? fm.removeItem(at: root) }
        let source = root.appendingPathComponent("steamapps")
        let game = source.appendingPathComponent("common/Euro Truck Simulator 2")
        let drive = root.appendingPathComponent("drive_c")
        let installed = drive.appendingPathComponent("Program Files (x86)/Steam/steamapps/common/Euro Truck Simulator 2")
        try fm.createDirectory(at: game.appendingPathComponent("bin/win_x64"), withIntermediateDirectories: true)
        try fm.createDirectory(at: drive, withIntermediateDirectories: true)
        for name in ["base.scs", "def.scs"] { try Data("game data".utf8).write(to: game.appendingPathComponent(name)) }
        for name in ["fmod.dll", "fmodstudio.dll", "steam_api64.dll"] {
            try Data("game library".utf8).write(to: game.appendingPathComponent("bin/win_x64/" + name))
        }
        var exe = Data(repeating: 0, count: 90)
        exe[0] = 0x4d; exe[1] = 0x5a; exe[60] = 64
        exe[64] = 0x50; exe[65] = 0x45
        exe[68] = 0x64; exe[69] = 0x86; exe[88] = 0x0b; exe[89] = 0x02
        let executable = game.appendingPathComponent("bin/win_x64/eurotrucks2.exe")
        try exe.write(to: executable)
        let manifest = Data("""
        "AppState"
        {
            "appid" "227300"
            "StateFlags" "4"
            "installdir" "Euro Truck Simulator 2"
            "buildid" "12345"
            "InstalledDepots" { "227302" { "manifest" "123456789" } }
        }
        """.utf8)
        let recordURL = source.appendingPathComponent("appmanifest_227300.acf")
        try manifest.write(to: recordURL)
        try await ETS2Import.shared.importFolder(source, drive: drive)
        let importedRecord = drive.appendingPathComponent("Program Files (x86)/Steam/steamapps/appmanifest_227300.acf")
        precondition((try? Data(contentsOf: importedRecord)) == manifest)
        precondition((try? Data(contentsOf: installed.appendingPathComponent("bin/win_x64/eurotrucks2.exe"))) == exe)
        // A rejected replacement must leave the prior game and record intact.
        exe[68] = 0x4c; exe[69] = 0x01
        try exe.write(to: executable)
        do {
            try await ETS2Import.shared.importFolder(source, drive: drive)
            fatalError("accepted 32-bit ETS2")
        } catch is ETS2Import.Failure {}
        precondition((try? Data(contentsOf: importedRecord)) == manifest)
        let oldEXE = try Data(contentsOf: installed.appendingPathComponent("bin/win_x64/eurotrucks2.exe"))
        precondition(oldEXE[68] == 0x64)
        let steamDLL = game.appendingPathComponent("bin/win_x64/steam_api64.dll")
        try fm.removeItem(at: steamDLL)
        do {
            try await ETS2Import.shared.importFolder(source, drive: drive)
            fatalError("accepted missing Steam API library")
        } catch is ETS2Import.Failure {}
        precondition(fm.fileExists(atPath: installed.appendingPathComponent("bin/win_x64/steam_api64.dll").path))
        try Data("game library".utf8).write(to: steamDLL)
        exe[68] = 0x64; exe[69] = 0x86
        try exe.write(to: executable)
        try fm.createSymbolicLink(at: game.appendingPathComponent("linked-file"), withDestinationURL: recordURL)
        do {
            try await ETS2Import.shared.importFolder(source, drive: drive)
            fatalError("accepted a symbolic link")
        } catch is ETS2Import.Failure {}
        try fm.removeItem(at: game.appendingPathComponent("linked-file"))
        // A plain game folder also works when the real record is included.
        try manifest.write(to: game.appendingPathComponent("appmanifest_227300.acf"))
        try Data("replacement".utf8).write(to: game.appendingPathComponent("base.scs"))
        try await ETS2Import.shared.importFolder(game, drive: drive)
        precondition((try? Data(contentsOf: installed.appendingPathComponent("base.scs"))) == Data("replacement".utf8))
        try Data("\"AppState\" { \"appid\" \"270880\" }".utf8).write(to: recordURL)
        do {
            try await ETS2Import.shared.importFolder(source, drive: drive)
            fatalError("accepted ATS metadata")
        } catch is ETS2Import.Failure {}
        let leftovers = try fm.contentsOfDirectory(atPath: installed.deletingLastPathComponent().path)
        precondition(leftovers == ["Euro Truck Simulator 2"])
        print("ETS2 import checks passed")
    }
}
