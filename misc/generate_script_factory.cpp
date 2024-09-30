#include <cctype>
#include <fstream>
#include <filesystem>

std::string toTypename(const std::string& filename) {
    std::string res;

    bool up = true;

    for (size_t i = 0; i < filename.size(); i++) {
        if (filename[i] == '.')
            break;

        if (filename[i] == '_') {
            up = true;
        } else {
            res += up ? (char)toupper(filename[i]) : filename[i];

            if (up) {
                up = false;
            }
        }
    }

    return res;
}

int main() {
    std::ofstream file("src/scripts/script.cpp", std::ios::out | std::ios::trunc);

    std::string includes;
    std::string ifs;

    bool first = true;

    for (const auto& entry : std::filesystem::directory_iterator("src/scripts/")) {
        std::string scriptFilename = entry.path().filename().string();

        if (scriptFilename == "script.h" || scriptFilename == "script.cpp")
            continue;

        std::string scriptName = toTypename(scriptFilename);
        includes += "#include \"" + scriptFilename + "\"\n";

        if (first) {
            ifs += "\tif (name == \"" + scriptName + "\") {\n\t\tnewScript = std::make_unique<" + scriptName + ">(entity);\n\t}";
            first = false;
        } else {
            ifs += " else if (name == \"" + scriptName + "\") {\n\t\tnewScript = std::make_unique<" + scriptName + ">(entity);\n\t}";
        }
    }

    std::string content = "#include \"script.h\"\n\n" +
                          includes +
                          "\nstd::unique_ptr<Script> Script::createInstance(const std::string& name, Entity& entity) {\n\tstd::unique_ptr<Script> newScript = nullptr;\n\n" +
                          ifs +
                          "\n\n\treturn std::move(newScript);\n}\n";

    file << content;

    file.close();
}
