> 使用FTXUI作为终端显示界面，通过TUI调整配置选项，生成合并图片文件为gif的命令并调用ffmpeg执行

> Using FTXUI as the terminal display interface, adjust configuration options through TUI, generate a command to merge image files into a gif, and call ffmpeg to execute.

#### 功能
- 自动通过文件名数字特征生成图片流顺序
- 通过终端图形界面对命令行参数进行有限自由度的配置
- 截取ffmpeg输出日志，并通过终端图形界面呈现平滑的执行进度条

> [!warning] 程序会尝试重命名待处理文件方便ffmpeg命令的生成，请勿中断程序执行命令，程序在执行完命令后才会恢复原文件名。若因意外导致程序关闭或退出，可以通过log文件获得新旧文件名的对照信息

---

#### Function
- Automatically generate the sequence of image streams based on numerical features in filenames.  
- Configure command-line parameters with limited flexibility through a terminal graphical interface.  
- Capture ffmpeg output logs and display a smooth execution progress bar via the terminal graphical interface.  

> [!warning] The program will attempt to rename the files to be processed for easier generation of ffmpeg commands. Do not interrupt the program during command execution, as the original filenames will only be restored after the command completes. If the program is unexpectedly closed or terminated, the mapping between old and new filenames can be retrieved from the log file.
