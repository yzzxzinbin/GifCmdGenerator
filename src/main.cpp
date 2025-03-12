#include <cstdlib>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <regex>
#include <ftxui/screen/screen.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

using namespace ftxui;

// 全局变量存储用户输入参数
std::string input_pattern = "input_%03d.png";
std::string output_path = "output.gif";
std::string framerate = "10";
std::string width = "320";
std::string quality = "";            // 质量参数（默认留空）
std::string loop_count = "0";        // 循环次数（0=无限）
std::string command_display;         // 实时显示生成的命令
std::string error_message;           // 错误提示信息
std::string result_message;          // 运行结果信息
std::atomic<float> progress{0};      // 进度条值（0.0 - 1.0）
std::atomic<bool> is_running{false}; // 是否正在运行

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
                          " -i " + input_pattern +
                          " -vf \"scale=" + width + ":-1\"";

        // 添加质量参数
        if (!quality.empty())
        {
            command_display += " -q:v " + quality;
        }

        // 添加循环参数
        command_display += " -loop " + loop_count + " " + output_path;
    }
}

// 执行命令并捕获进度
void ExecuteCommand()
{
    if (!error_message.empty())
        return;

    // 重置进度和结果信息
    progress = 0;
    result_message.clear();
    is_running = true;

    // 启动ffmpeg进程
    FILE *pipe = popen((command_display + " 2>&1").c_str(), "r"); // 捕获标准错误
    if (!pipe)
    {
        result_message = "错误：无法启动ffmpeg进程";
        is_running = false;
        return;
    }

    // 正则表达式解析时间
    std::regex time_regex(R"(time=(\d+):(\d+):(\d+\.\d+))");
    std::smatch matches;
    std::string line;

    // 读取ffmpeg输出
    char buffer[128];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
    {
        line = buffer;

        // 解析时间
        if (std::regex_search(line, matches, time_regex))
        {
            int hours = std::stoi(matches[1]);
            int minutes = std::stoi(matches[2]);
            float seconds = std::stof(matches[3]);

            // 计算总秒数
            float total_seconds = hours * 3600 + minutes * 60 + seconds;

            // 假设总时长为10分钟（可以根据实际情况调整）
            float total_duration = 600; // 10分钟
            progress = total_seconds / total_duration;
        }

        // 捕获错误信息
        if (line.find("Error") != std::string::npos || line.find("failed") != std::string::npos)
        {
            result_message += line;
        }
    }

    // 关闭管道
    pclose(pipe);
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
}

int main()
{
    // 定义输入组件
    Component input_pattern_input = Input(&input_pattern, "输入文件模式");
    Component output_path_input = Input(&output_path, "输出路径");
    Component framerate_input = Input(&framerate, "帧率（如10）");
    Component width_input = Input(&width, "宽度（如320）");
    Component quality_input = Input(&quality, "质量（1-31，可选）");
    Component loop_input = Input(&loop_count, "循环次数（0=无限）");

    // 定义按钮组件
    Component execute_button = Button("生成GIF", []
                                      {
        GenerateCommand();
        if (error_message.empty()) {
            std::thread(ExecuteCommand).detach(); // 异步执行
        } });
    Component quit_button = Button("退出", [&]
                                   { ScreenInteractive::Active()->Exit(); });

    // 组合所有组件
    auto layout = Container::Vertical({
        input_pattern_input,
        output_path_input,
        framerate_input,
        width_input,
        quality_input,
        loop_input,
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
        display_elements.push_back(hbox(text(" 输入文件模式: "), input_pattern_input->Render()));
        display_elements.push_back(hbox(text(" 输出文件路径: "), output_path_input->Render()));
        display_elements.push_back(hbox(text(" 帧率 (fps):    "), framerate_input->Render()));
        display_elements.push_back(hbox(text(" 宽度 (px):     "), width_input->Render()));
        display_elements.push_back(hbox(text(" 质量 (1-31):  "), quality_input->Render()));
        display_elements.push_back(hbox(text(" 循环次数:      "), loop_input->Render()));
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