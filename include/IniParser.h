#define INI_PARSER_H
#include <string>
#include <vector>
#include <map>

class IniParser {
public:
    IniParser() = default;
    explicit IniParser(const std::string& filepath);

    bool loadFromFile(const std::string& filepath);
    std::vector<std::string> getUrls() const;
    std::string get(const std::string& section, const std::string& key, const std::string& defaultValue = "") const;

private:
    std::map<std::string, std::map<std::string, std::string>> m_data;
    std::vector<std::string> m_urls;

    static std::string trim(const std::string& str);
};
