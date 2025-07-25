#include <iostream>
#include <fstream>
#include <vector>
#include <stdexcept>
#include <filesystem>
#include <sstream>
#include <string>
#ifdef _WIN32
#include <stdbool.h>
#include <windows.h>
#endif

const int BOOTLOADER_SIZE = 512;
const int FLOPPY_SIZE = 1474560;

void padTo(std::vector<char>& data, size_t size) {
    if (data.size() < size)
        data.resize(size, 0x00);
}

std::vector<char> readFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("cannot open " + path.string());
    return std::vector<char>((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());
}

std::string getExecutableDir() {
#ifdef _WIN32
    wchar_t wideBuffer[MAX_PATH];
    GetModuleFileNameW(NULL, wideBuffer, MAX_PATH);
    std::filesystem::path widePath(wideBuffer);
    return widePath.parent_path().string();
#else
    return ".";
#endif
}

std::string findBootloader() {
    std::string exeDir = getExecutableDir();
    std::vector<std::string> searchPaths = {
        exeDir + "/bootloader.bin",
        exeDir + "/build/bootloader.bin",
        exeDir + "/../bootloader.bin",
        exeDir + "/../build/bootloader.bin",
        "./bootloader.bin",
        "./build/bootloader.bin",
        "../bootloader.bin",
        "../build/bootloader.bin"
    };
    for (const auto& path : searchPaths) {
        if (std::filesystem::exists(path)) {
            std::cout << "found bootloader at " << path << std::endl;
            return path;
        }
    }
    throw std::runtime_error("bootloader.bin not found");
}

std::string parseSkibidiBoot(const std::string& configPath) {
    std::ifstream file(configPath);
    if (!file) throw std::runtime_error("failed to open config " + configPath);

    std::string line;
    bool insideOSBlock = false;
    while (std::getline(file, line)) {
        line.erase(0, line.find_first_not_of(" \t\n\r"));
        line.erase(line.find_last_not_of(" \t\n\r") + 1);

        if (line.empty() || line[0] == '#') continue;

        if (line.find('{') != std::string::npos) {
            insideOSBlock = true;
            continue;
        }

        if (line.find('}') != std::string::npos) {
            insideOSBlock = false;
            continue;
        }

        if (insideOSBlock && line.find("kernel") != std::string::npos) {
            size_t eq = line.find('=');
            if (eq != std::string::npos) {
                std::string value = line.substr(eq + 1);
                value.erase(0, value.find_first_not_of(" \t\""));
                value.erase(value.find_last_not_of(" \t\"") + 1);
                return value;
            }
        }
    }

    throw std::runtime_error("kernel not found in config");
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "skibidiboot_mkimage <kernel binary> <output image>\n";
        std::cerr << "skibidiboot_mkimage <bootloader> <kernel binary> <output image>\n";
        std::cerr << "skibidiboot_mkimage -c <config.skibidiboot> <output image>\n";
        return 1;
    }

    try {
        std::string bootloaderPath;
        std::string stage2Path;
        std::string outputPath;

        if (argc == 3) {
            bootloaderPath = findBootloader();
            stage2Path = argv[1];
            outputPath = argv[2];
        } else if (argc == 4 && std::string(argv[1]) != "-c") {
            bootloaderPath = argv[1];
            stage2Path = argv[2];
            outputPath = argv[3];
        } else if (argc == 4 && std::string(argv[1]) == "-c") {
            std::string configPath = argv[2];
            outputPath = argv[3];
            bootloaderPath = findBootloader();
            stage2Path = parseSkibidiBoot(configPath);
        } else {
            throw std::runtime_error("invalid number of arguments");
        }

        std::vector<char> boot = readFile(std::filesystem::path(bootloaderPath));
        std::vector<char> stage2 = readFile(stage2Path);

        padTo(boot, BOOTLOADER_SIZE);

        std::vector<char> image;
        image.insert(image.end(), boot.begin(), boot.end());
        image.insert(image.end(), stage2.begin(), stage2.end());
        padTo(image, FLOPPY_SIZE);

        std::ofstream out(outputPath, std::ios::binary);
        if (!out) throw std::runtime_error("could not write to " + outputPath);
        out.write(image.data(), image.size());

        std::cout << "generated at " << outputPath << std::endl;
        std::cout << "bootloader   " << boot.size() << " bytes\n";
        std::cout << "kernel       " << stage2.size() << " bytes\n";
        std::cout << "image        " << image.size() << " bytes\n";

    } catch (const std::exception& e) {
        std::cerr << "uh oh " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
