#include <cstdlib>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <regex>
#include <filesystem>
#include <chrono>
#include <fstream>
#include <ftxui/screen/screen.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

using namespace ftxui;
namespace fs = std::filesystem;

// 全局变量存储用户输入参数
std::string output_path = "output.gif";
std::string framerate = "10";
std::string width = "320";
std::string quality = "";            // 质量参数（默认留空）
std::string loop_count = "0";        // 循环次数（0=无限）
std::string extension = "jpg";       // 文件后缀名
std::string command_display;         // 实时显示生成的命令
std::string error_message;           // 错误提示信息
std::string result_message;          // 运行结果信息
std::atomic<float> progress{0};      // 进度条值（0.0 - 1.0）
std::atomic<bool> is_running{false}; // 是否正在运行

const std::string log_file_path = "ffmpeg.log"; // 日志文件路径
std::map<std::string, std::string> fileMap;     // 存储文件名和数字部分的映射

bool isValidNumber(const std::string &s, int &value);
std::string extractNumberFromFilename(const std::string &filename);
void RenameFiles();
void GenerateCommand();
void ExecuteCommand();

// ---------------------------------------------------
// 检查字符串是否为有效整数
bool isValidNumber(const std::string &s, int &value)
{
    try
    {
        value = std::stoi(s);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

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

// 重命名文件函数
void RenameFiles()
{

    // 遍历目录中的文件
    for (const auto &entry : fs::directory_iterator("."))
    {
        if (entry.is_regular_file() && entry.path().extension() == "." + extension)
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
        std::ostringstream newFilenameStream;
        newFilenameStream << "image_" << std::setw(3) << std::setfill('0') << counter << "." << extension;
        std::string newFilename = newFilenameStream.str();

        fs::rename(filename, newFilename);
        counter++;
    }
}

// 生成FFmpeg命令的函数
void GenerateCommand()
{
    // 清空错误信息
    error_message.clear();

    // 检查参数有效性
    std::vector<std::string> errors;
    int tmp;

    // 帧率检查
    if (!isValidNumber(framerate, tmp) || tmp <= 0)
    {
        errors.push_back("帧率必须为正整数");
    }

    // 宽度检查
    if (!isValidNumber(width, tmp) || tmp <= 0)
    {
        errors.push_back("宽度必须为正整数");
    }

    // 质量检查（可选参数）
    if (!quality.empty())
    {
        int q;
        if (!isValidNumber(quality, q) || q < 1 || q > 31)
        {
            errors.push_back("质量参数应为1-31（值越小质量越高）");
        }
    }

    // 循环次数检查
    if (!isValidNumber(loop_count, tmp) || tmp < 0)
    {
        errors.push_back("循环次数必须为非负整数（0=无限循环）");
    }

    // 合并错误信息
    if (!errors.empty())
    {
        error_message = "错误：";
        for (const auto &err : errors)
        {
            error_message += "\n  • " + err;
        }
    }

    // 生成命令（如果无错误）
    command_display.clear();
    if (error_message.empty())
    {
        command_display = "ffmpeg -hide_banner -loglevel info -framerate " + framerate +
                          " -i image_%03d." + extension +
                          " -vf \"scale=" + width + ":-1\"";

        // 添加质量参数
        if (!quality.empty())
        {
            command_display += " -q:v " + quality;
        }

        // 添加循环参数
        command_display += " -loop " + loop_count + " -y " + output_path; // 添加 -y 参数
    }
}

// 执行命令并捕获进度
void ExecuteCommand()
{
    char buffer[64];
    int last_frame = 0;                // 上一次的帧数
    int current_frame = 0;             // 当前的帧数
    int total_frames = fileMap.size(); // 总帧数
    const float smooth_step = 0.01f;   // 每次增加的进度步长

    if (!error_message.empty())
        return;

    // 重置进度和结果信息
    progress = 0;
    result_message.clear();
    is_running = true;

    // 打开日志文件
    std::ofstream log_file(log_file_path);
    if (!log_file.is_open())
    {
        result_message = "错误：无法创建日志文件";
        is_running = false;
        return;
    }

    // 启动ffmpeg进程
    FILE *pipe = popen((command_display + " 2>&1").c_str(), "r"); // 捕获标准错误
    if (!pipe)
    {
        result_message = "错误：无法启动ffmpeg进程";
        is_running = false;
        return;
    }

    // 正则表达式解析 frame
    std::regex frame_regex(R"(frame=\s*(\d+))");
    std::smatch matches;
    std::string line;

    // 读取ffmpeg输出
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
    {
        line = buffer;
        log_file << line; // 将输出写入日志文件

        // 解析 frame
        if (std::regex_search(line, matches, frame_regex))
        {
            current_frame = std::stoi(matches[1]); // 更新当前帧数
        }

        // 平滑插值更新进度
        float target_progress = static_cast<float>(current_frame) / total_frames;
        while (progress < target_progress)
        {
            progress = progress + smooth_step; // 逐步增加进度
            if (progress > target_progress)
            {
                progress = target_progress;
            }

            // 主动触发界面刷新
            ScreenInteractive::Active()->PostEvent(Event::Custom);

            // 稍微延迟一下，避免进度条更新过快
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        // 捕获错误信息
        if (line.find("Error") != std::string::npos || line.find("failed") != std::string::npos)
        {
            result_message += line;
        }
    }

    // 关闭管道和日志文件
    pclose(pipe);
    log_file.close();
    is_running = false;

    // 更新结果信息
    if (result_message.empty())
    {
        result_message = "成功：GIF已生成！";
    }
    else
    {
        result_message = "失败：\n" + result_message;
    }

    // 最后一次刷新界面
    ScreenInteractive::Active()->PostEvent(Event::Custom);
}
int main()
{
    // 定义输入组件
    Component output_path_input = Input(&output_path, "输出路径");
    Component framerate_input = Input(&framerate, "帧率（如10）");
    Component width_input = Input(&width, "宽度（如320）");
    Component quality_input = Input(&quality, "质量（1-31，可选）");
    Component loop_input = Input(&loop_count, "循环次数（0=无限）");
    Component extension_input = Input(&extension, "文件后缀名（如jpg）");

    // 定义按钮组件
    Component execute_button = Button("生成GIF", []
                                      {
        RenameFiles(); // 先重命名文件
        GenerateCommand();
        if (error_message.empty()) {
            std::thread(ExecuteCommand).detach(); // 异步执行
        } });
    Component quit_button = Button("退出", [&]
                                   { ScreenInteractive::Active()->Exit(); });

    // 组合所有组件
    auto layout = Container::Vertical({
        output_path_input,
        framerate_input,
        width_input,
        quality_input,
        loop_input,
        extension_input,
        Container::Horizontal({
            execute_button,
            quit_button,
        }),
    });

    // 渲染界面
    auto renderer = Renderer(layout, [&]
                             {
        GenerateCommand(); // 实时更新命令和错误信息

        Elements display_elements;
        display_elements.push_back(hbox(text(" 输出文件路径:  "), output_path_input->Render()));
        display_elements.push_back(hbox(text(" 帧率 (fps):    "), framerate_input->Render()));
        display_elements.push_back(hbox(text(" 宽度 (px):     "), width_input->Render()));
        display_elements.push_back(hbox(text(" 质量 (1-31):   "), quality_input->Render()));
        display_elements.push_back(hbox(text(" 循环次数:      "), loop_input->Render()));
        display_elements.push_back(hbox(text(" 文件后缀名:    "), extension_input->Render()));
        display_elements.push_back(separator());

        // 错误信息显示
        if (!error_message.empty()) {
            display_elements.push_back(text(error_message) | color(Color::Red) | bold);
            display_elements.push_back(separator());
        }

        // FFmpeg命令显示
        display_elements.push_back(text(" FFmpeg命令:"));
        display_elements.push_back(text(command_display) | border | flex);
        display_elements.push_back(separator());

        // 进度条显示
        if (is_running) {
            display_elements.push_back(hbox({
                text(" 进度: "),
                gauge(progress) | flex,
            }));
            display_elements.push_back(separator());
        }

        // 运行结果显示
        if (!result_message.empty()) {
            ftxui::Color result_color;
            if (is_running) {
                result_color = ftxui::Color::Default;
            } else {
                if (result_message.find("成功") != std::string::npos) {
                    result_color = ftxui::Color::Green;
                } else {
                    result_color = ftxui::Color::Red;
                }
            }
            display_elements.push_back(text(result_message) | color(result_color));
            display_elements.push_back(separator());
        }
        // 按钮布局
        auto buttons = hbox({
            execute_button->Render() | border | color(Color::Green),
            text(" "),
            quit_button->Render() | border | color(Color::Red),
        }) | center;

        display_elements.push_back(buttons);

        return vbox(display_elements) | border | flex | size(HEIGHT, LESS_THAN, 24); });

    // 启动交互式界面
    auto screen = ScreenInteractive::TerminalOutput();
    screen.Loop(renderer);
    return 0;
}