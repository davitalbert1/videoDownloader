#ifndef INI_PARSER_H
#define INI_PARSER_H

#include <string>
#include <vector>
#include <map>

struct IniEntry {
    std::string section;
    std::string key;
    std::string url;
    std::string customFolder; // Se fornecido na seção ou chave
};

class IniParser {
public:
    IniParser() = default;
    explicit IniParser(const std::string& filepath);

    bool loadFromFile(const std::string& filepath);
    std::vector<std::string> getUrls() const;
    std::vector<IniEntry> getEntries() const;
    std::string get(const std::string& section, const std::string& key, const std::string& defaultValue = "") const;

private:
    std::map<std::string, std::map<std::string, std::string>> m_data;
    std::vector<std::string> m_urls;
    std::vector<IniEntry> m_entries;

    static std::string trim(const std::string& str);
};
#endif // INI_PARSER_H
