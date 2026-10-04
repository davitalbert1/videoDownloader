#include "IniParser.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

IniParser::IniParser(const std::string& filepath) {
    loadFromFile(filepath);
}

std::string IniParser::trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

bool IniParser::loadFromFile(const std::string& filepath) {
    m_data.clear();
    m_urls.clear();

    std::ifstream file(filepath);
    if (!file.is_open()) return false;

    std::string currentSection = "default";
    std::string line;

    while (std::getline(file, line)) {
        std::string trimmed = trim(line);
        if (trimmed.empty()) continue;

        // Comentários
        if (trimmed[0] == ';' || trimmed[0] == '#') continue;

        // Seção [Nome]
        if (trimmed.front() == '[' && trimmed.back() == ']') {
            currentSection = trim(trimmed.substr(1, trimmed.length() - 2));
            continue;
        }

        // Chave=Valor
        size_t equalPos = trimmed.find('=');
        if (equalPos != std::string::npos) {
            std::string key = trim(trimmed.substr(0, equalPos));
            std::string val = trim(trimmed.substr(equalPos + 1));

            // Remover aspas caso existam em volta do valor
            if (val.size() >= 2 && ((val.front() == '"' && val.back() == '"') || 
                                   (val.front() == '\'' && val.back() == '\''))) {
                val = val.substr(1, val.size() - 2);
            }

            m_data[currentSection][key] = val;

            // Se a chave contiver url ou for video, ou o valor começar com http:// ou https://
            if (val.rfind("http://", 0) == 0 || val.rfind("https://", 0) == 0 ||
                key.find("url") != std::string::npos || key.find("video") != std::string::npos) {
                m_urls.push_back(val);
            }
        } else {
            // Linha sem '=', pode ser uma URL direta listada linha a linha
            if (trimmed.rfind("http://", 0) == 0 || trimmed.rfind("https://", 0) == 0) {
                m_urls.push_back(trimmed);
            }
        }
    }

    return true;
}

std::vector<std::string> IniParser::getUrls() const {
    return m_urls;
}

std::string IniParser::get(const std::string& section, const std::string& key, const std::string& defaultValue) const {
    auto secIt = m_data.find(section);
    if (secIt != m_data.end()) {
        auto keyIt = secIt->second.find(key);
        if (keyIt != secIt->second.end()) return keyIt->second;
    }
    return defaultValue;
}
