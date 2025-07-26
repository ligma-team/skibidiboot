// could someone please fix the eltorito iso shit

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

// more settings :3
const size_t ISO_SECTOR_SIZE = 2048;
const size_t ISO_BOOT_SECTOR = 11;
const size_t ISO_BOOT_CATALOG_SECTOR = 20;
const size_t ISO_BOOT_IMAGE_SECTOR = 21;

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

// fuck you iso
void writeElToritoISO(const std::vector<char>& image, const std::string& outPath) {
    size_t paddedSize = ((image.size() + 511) / 512) * 512;
    std::vector<char> floppy = image;
    floppy.resize(paddedSize, 0x00);
    
    if (floppy.size() >= 512) {
        floppy[510] = 0x55;
        floppy[511] = 0xAA;
    }

    size_t floppySectors = floppy.size() / 512;
    size_t totalSectors = ISO_BOOT_IMAGE_SECTOR + ((floppy.size() + ISO_SECTOR_SIZE - 1) / ISO_SECTOR_SIZE);

    std::vector<char> iso(totalSectors * ISO_SECTOR_SIZE, 0);

    char* pvd = &iso[ISO_SECTOR_SIZE * 16];
    pvd[0] = 1;
    memcpy(pvd + 1, "CD001", 5);
    pvd[6] = 1;
    memset(pvd + 8, ' ', 32);
    memcpy(pvd + 8, "LINUX", 5);
    memset(pvd + 40, ' ', 32);
    memcpy(pvd + 40, "SKIBIDIBOOT", 11);

    uint32_t volumeSize = totalSectors;
    for (int i = 0; i < 4; ++i) {
        pvd[80 + i] = (volumeSize >> (8 * i)) & 0xFF;
        pvd[84 + i] = (volumeSize >> (8 * (3 - i))) & 0xFF;
    }

    pvd[120] = 1; pvd[121] = 0; pvd[122] = 0; pvd[123] = 1;
    pvd[124] = 1; pvd[125] = 0; pvd[126] = 0; pvd[127] = 1;
    pvd[128] = 0x00; pvd[129] = 0x08; pvd[130] = 0x08; pvd[131] = 0x00;

    char* rootDir = pvd + 156;
    rootDir[0] = 34;
    rootDir[2] = ISO_BOOT_IMAGE_SECTOR & 0xFF;
    rootDir[3] = (ISO_BOOT_IMAGE_SECTOR >> 8) & 0xFF;
    rootDir[4] = (ISO_BOOT_IMAGE_SECTOR >> 16) & 0xFF;
    rootDir[5] = (ISO_BOOT_IMAGE_SECTOR >> 24) & 0xFF;
    rootDir[10] = 0; rootDir[11] = 0; rootDir[12] = 0; rootDir[13] = 8;
    rootDir[25] = 2;
    rootDir[32] = 1;
    rootDir[33] = 0;

    char* bootRec = &iso[ISO_SECTOR_SIZE * 17];
    bootRec[0] = 0;
    memcpy(bootRec + 1, "CD001", 5);
    bootRec[6] = 1;
    memcpy(bootRec + 7, "EL TORITO SPECIFICATION", 23);
    uint32_t catalogSector = ISO_BOOT_CATALOG_SECTOR;
    for (int i = 0; i < 4; ++i)
        bootRec[71 + i] = (catalogSector >> (8 * i)) & 0xFF;

    char* term = &iso[ISO_SECTOR_SIZE * 18];
    term[0] = 255;
    memcpy(term + 1, "CD001", 5);
    term[6] = 1;

    char* catalog = &iso[ISO_SECTOR_SIZE * ISO_BOOT_CATALOG_SECTOR];
    memset(catalog, 0, ISO_SECTOR_SIZE);
    catalog[0] = 0x01;
    catalog[30] = 0x55;
    catalog[31] = 0xAA;
    memcpy(catalog + 4, "SKIBIDIBOOT MAKER", 17);

    catalog[32] = 0x88;
    catalog[33] = 0x00;
    catalog[34] = 0x02;
    catalog[35] = 0x00;
    catalog[36] = 0x00;
    catalog[37] = 0x00;

    uint16_t sectorCount = 0x0960;
    catalog[38] = sectorCount & 0xFF;
    catalog[39] = (sectorCount >> 8) & 0xFF;

    uint32_t bootImageSector = ISO_BOOT_IMAGE_SECTOR;
    for (int i = 0; i < 4; ++i)
        catalog[40 + i] = (bootImageSector >> (8 * i)) & 0xFF;

    uint16_t* words = reinterpret_cast<uint16_t*>(catalog);
    uint16_t checksum = 0;
    for (int i = 0; i < 16; ++i) {
        if (i == 14) continue;
        checksum += words[i];
    }
    words[14] = -checksum;

    size_t bootImageOffset = ISO_SECTOR_SIZE * ISO_BOOT_IMAGE_SECTOR;
    memcpy(&iso[bootImageOffset], floppy.data(), floppy.size());

    std::ofstream out(outPath, std::ios::binary);
    if (!out) throw std::runtime_error("cannot open ISO output");
    out.write(iso.data(), iso.size());
    out.close();

    std::cout << "iso written to " << outPath << " (" << totalSectors << " sectors)\n";
    std::cout << "boot catalog @ sector " << ISO_BOOT_CATALOG_SECTOR
              << ", boot image @ sector " << ISO_BOOT_IMAGE_SECTOR << "\n";
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "skibidiboot_mkimage <kernel binary> <output image>\n";
        std::cerr << "skibidiboot_mkimage <bootloader> <kernel binary> <output image>\n";
        std::cerr << "skibidiboot_mkimage -c <config.skibidiboot> <output image>\n";
        std::cerr << "skibidiboot_mkimage <dir> <output image>\n";
        return 1;
    }

    try {
        std::string bootloaderPath;
        std::string stage2Path;
        std::string outputPath;

        if (argc == 3 && std::filesystem::is_directory(argv[1])) {
            std::string isoDir = argv[1];
            outputPath = argv[2];

            std::string configPath = isoDir + "/.skibidiboot";
            stage2Path = parseSkibidiBoot(configPath);
            bootloaderPath = findBootloader();

            if (!std::filesystem::exists(stage2Path)) {
                stage2Path = isoDir + "/" + stage2Path;
            }

        } else if (argc == 3) {
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

        auto ends_with = [](const std::string& str, const std::string& suffix) {
            return str.size() >= suffix.size() &&
                   str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
        };

        if (ends_with(outputPath, ".iso")) {
            writeElToritoISO(image, outputPath);
        } else {
            std::ofstream out(outputPath, std::ios::binary);
            if (!out) throw std::runtime_error("could not write to " + outputPath);
            out.write(image.data(), image.size());
            std::cout << "generated floppy image at " << outputPath << std::endl;
        }

        std::cout << "bootloader   " << boot.size() << " bytes\n";
        std::cout << "kernel       " << stage2.size() << " bytes\n";
        std::cout << "final size   " << image.size() << " bytes\n";

    } catch (const std::exception& e) {
        std::cerr << "uh oh " << e.what() << std::endl;
        return 1;
    }

    return 0;
}