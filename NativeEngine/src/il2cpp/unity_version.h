#pragma once

namespace il2cppdumper {

inline const char* unityVersionRange(double metadataVersion) {
    if (metadataVersion >= 111) return "6000.7.0a6+";
    if (metadataVersion >= 108) return "6000.6.0f1+";
    if (metadataVersion >= 107) return "6000.5.5f1+";
    if (metadataVersion >= 106) return "6000.5.0f1 - 6000.5.4f1";
    if (metadataVersion >= 105) return "6000.5.0a5 - 6000.5.0b*";
    if (metadataVersion >= 104) return "6000.5.0a1 - 6000.5.0a4";
    if (metadataVersion >= 39) return "6000.3.0b1+";
    if (metadataVersion >= 38) return "6000.3.0a5 - 6000.3.0a6";
    if (metadataVersion >= 35) return "6000.3.0a2 - 6000.3.0a4";
    if (metadataVersion >= 33) return "6000.3 alpha";
    if (metadataVersion >= 31) return "2022.3.33 - 2023.x / 6000.0 - 6000.2";
    if (metadataVersion >= 29.1) return "2021.3.0 - 2022.3.32";
    if (metadataVersion >= 29) return "2021.2.0 - 2021.2.x";
    if (metadataVersion >= 27.2) return "2021.1.0 - 2021.1.x";
    if (metadataVersion >= 27.1) return "2020.2.4 - 2020.3.x";
    if (metadataVersion >= 27) return "2020.2.0 - 2020.2.3";
    if (metadataVersion >= 24.5) return "2020.1.11 - 2020.1.x";
    if (metadataVersion >= 24.4) return "2020.1.0 - 2020.1.10";
    if (metadataVersion >= 24.3) return "2019.4.21 - 2019.4.x";
    if (metadataVersion >= 24.2) return "2019.3.7 - 2019.4.20";
    if (metadataVersion >= 24.1) return "2018.4.x - 2019.3.6";
    if (metadataVersion >= 24) return "2017.1 - 2018.3";
    if (metadataVersion >= 23) return "5.6.x";
    if (metadataVersion >= 22) return "5.5.x";
    if (metadataVersion >= 21) return "5.3.5 - 5.4.x";
    if (metadataVersion >= 20) return "5.3.3 - 5.3.4";
    if (metadataVersion >= 19) return "5.3.2";
    if (metadataVersion >= 16) return "5.3.0 - 5.3.1";
    return "unknown";
}

}
