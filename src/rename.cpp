#include <iostream>
#include <filesystem>
#include <map>
#include <regex>
#include <string>
#include <algorithm>
#include <iomanip> // 用于 std::setw 和 std::setfill

namespace fs = std::filesystem;

// 从文件名中提取数字部分
std::string extractNumberFromFilename(const std::string &filename)
{
    std::string number;
    bool foundDigit = false;

    // 从文件名末尾开始遍历
    for (auto it = filename.rbegin(); it != filename.rend(); ++it)
    {
        if (std::isdigit(*it))
        {
            foundDigit = true;
            number.push_back(*it);
        }
        else if (foundDigit)
        {
            // 遇到第一个非数字字符时停止
            break;
        }
    }

    // 反转数字部分以恢复原始顺序
    std::reverse(number.begin(), number.end());
    return number;
}

// 主函数
int main()
{
    std::string directory = ".";                // 当前目录
    std::string extension = ".jpg";             // 文件后缀
    std::map<std::string, std::string> fileMap; // 存储文件名和数字部分的映射

    // 遍历目录中的文件
    for (const auto &entry : fs::directory_iterator(directory))
    {
        if (entry.is_regular_file() && entry.path().extension() == extension)
        {
            std::string filename = entry.path().filename().string();
            std::string number = extractNumberFromFilename(filename);

            if (!number.empty())
            {
                fileMap[number] = filename;
            }
        }
    }

    // 按照数字部分排序
    std::vector<std::pair<std::string, std::string>> sortedFiles(fileMap.begin(), fileMap.end());
    std::sort(sortedFiles.begin(), sortedFiles.end(), [](const auto &a, const auto &b)
              { return std::stoi(a.first) < std::stoi(b.first); });

    // 重命名文件
    int counter = 1;
    for (const auto &[number, filename] : sortedFiles)
    {
        // 使用 std::setw 和 std::setfill 补位
        std::ostringstream newFilenameStream;
        newFilenameStream << "image_" << std::setw(5) << std::setfill('0') << counter << extension;
        std::string newFilename = newFilenameStream.str();

        fs::rename(filename, newFilename);
        std::cout << "Renamed: " << filename << " -> " << newFilename << std::endl;
        counter++;
    }

    return 0;
}