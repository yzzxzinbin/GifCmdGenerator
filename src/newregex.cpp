#include <iostream>
#include <regex>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

int main() {
    // 指定要扫描的目录
    std::string directory = "./"; // 当前目录，可以更改为其他目录路径
    // 正则表达式模式，匹配以单个字母开头，后跟三位数字，并以.jpg结尾的文件名
    std::regex pattern("A\\d{3}\\.jpg");

    try {
        // 遍历指定目录
        for (const auto& entry : fs::directory_iterator(directory)) {
            // 检查是否是常规文件
            if (entry.is_regular_file()) {
                // 获取文件名
                std::string filename = entry.path().filename().string();
                // 进行正则表达式匹配
                if (std::regex_match(filename, pattern)) {
                    // 如果匹配，输出文件信息
                    std::cout << "Matched file: " << entry.path() << std::endl;
                }
            }
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Standard exception: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Unknown exception caught" << std::endl;
    }

    return 0;
}
